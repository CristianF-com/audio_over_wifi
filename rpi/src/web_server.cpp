#include "web_server.h"
#include "web_pages.h"
#include <poll.h>
#include <cerrno>
#include <strings.h>
#include <vector>
#include <fcntl.h>
#ifdef WITH_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <random>

// ─── JSON helpers ──────────────────────────────────────────────────────────────
static std::string jesc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) o += ' ';
        else o += (char)c;
    }
    return o;
}
static std::string jstr(const std::string& k, const std::string& v) {
    return "\"" + k + "\":\"" + jesc(v) + "\"";
}
static std::string jnum(const std::string& k, double v) {
    if (!std::isfinite(v)) v = 0;
    char b[48];
    if (v == std::floor(v) && std::fabs(v) < 1e15) snprintf(b, sizeof(b), "%lld", (long long)v);
    else snprintf(b, sizeof(b), "%.3f", v);
    return "\"" + k + "\":" + b;
}
static std::string jbool(const std::string& k, bool v) {
    return "\"" + k + "\":" + (v ? "true" : "false");
}
// Extrae el valor (string sin comillas o literal) de una clave; "" si no existe.
static std::string jsonGet(const std::string& j, const std::string& k) {
    std::string needle = "\"" + k + "\"";
    size_t p = j.find(needle); if (p == std::string::npos) return "";
    p = j.find(':', p + needle.size()); if (p == std::string::npos) return "";
    p++;
    while (p < j.size() && (j[p]==' '||j[p]=='\t'||j[p]=='\r'||j[p]=='\n')) p++;
    if (p >= j.size()) return "";
    if (j[p] == '"') {
        p++; std::string r;
        while (p < j.size()) {
            if (j[p]=='\\' && p+1 < j.size()) {
                p++;
                switch (j[p]) {
                case 'n': r += '\n'; break; case 't': r += '\t'; break; case 'r': r += '\r'; break;
                case 'b': r += '\b'; break; case 'f': r += '\f'; break;
                case 'u': {                                     // \uXXXX (con pares sustitutos) -> UTF-8
                    auto hex4 = [&](size_t q, unsigned& v) { if (q + 4 > j.size()) return false; v = 0;
                        for (size_t k = 0; k < 4; k++) { char h = j[q + k]; v <<= 4;
                            if (h >= '0' && h <= '9') v |= h - '0'; else if (h >= 'a' && h <= 'f') v |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') v |= h - 'A' + 10; else return false; } return true; };
                    unsigned cp = 0;
                    if (!hex4(p + 1, cp)) { r += 'u'; break; }
                    p += 4;
                    unsigned lo = 0;
                    if (cp >= 0xD800 && cp < 0xDC00 && p + 6 < j.size() && j[p+1] == '\\' && j[p+2] == 'u' && hex4(p + 3, lo) && lo >= 0xDC00 && lo < 0xE000) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6;
                    }
                    if (cp < 0x80) r += (char)cp;
                    else if (cp < 0x800) { r += (char)(0xC0 | (cp >> 6)); r += (char)(0x80 | (cp & 0x3F)); }
                    else if (cp < 0x10000) { r += (char)(0xE0 | (cp >> 12)); r += (char)(0x80 | ((cp >> 6) & 0x3F)); r += (char)(0x80 | (cp & 0x3F)); }
                    else { r += (char)(0xF0 | (cp >> 18)); r += (char)(0x80 | ((cp >> 12) & 0x3F)); r += (char)(0x80 | ((cp >> 6) & 0x3F)); r += (char)(0x80 | (cp & 0x3F)); }
                    break; }
                default: r += j[p];
                }
            }
            else if (j[p]=='"') break;
            else r += j[p];
            p++;
        }
        return r;
    }
    size_t e = j.find_first_of(",}]\r\n", p);
    std::string v = j.substr(p, e == std::string::npos ? std::string::npos : e - p);
    while (!v.empty() && (v.back()==' '||v.back()=='\t')) v.pop_back();
    return v;
}
// Parseo numérico SIN excepciones. Devuelve false si no es un número válido.
static bool parseInt(const std::string& s, long lo, long hi, long& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0' || v < lo || v > hi) return false;
    out = v; return true;
}
static bool parseFloat(const std::string& s, double lo, double hi, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v) || v < lo || v > hi) return false;
    out = v; return true;
}

static std::string b64decode(const std::string& in) {
    static const std::string T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; int val = 0, bits = -8;
    for (unsigned char ch : in) {
        if (ch == '=' || ch == '\r' || ch == '\n' || ch == ' ') continue;
        size_t pos = T.find((char)ch); if (pos == std::string::npos) return "";
        val = (val << 6) + (int)pos; bits += 6;
        if (bits >= 0) { out.push_back((char)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}
static bool constEq(const std::string& a, const std::string& b) {   // tiempo constante respecto al contenido
    unsigned diff = (unsigned)(a.size() ^ b.size());
    for (size_t i = 0; i < std::max(a.size(), b.size()); i++)
        diff |= (unsigned)((i < a.size() ? a[i] : 0) ^ (i < b.size() ? b[i] : 0));
    return diff == 0;
}

struct Conn {
    int   fd  = -1;
    void* ssl = nullptr;                       // SSL* si es HTTPS
    ssize_t rd(void* b, size_t n) {
#ifdef WITH_TLS
        if (ssl) { int r = SSL_read((SSL*)ssl, b, (int)n); return r > 0 ? r : -1; }
#endif
        return ::read(fd, b, n);
    }
    ssize_t wr(const void* b, size_t n) {
#ifdef WITH_TLS
        if (ssl) { int r = SSL_write((SSL*)ssl, b, (int)n); return r > 0 ? r : -1; }
#endif
        return ::write(fd, b, n);
    }
};

static constexpr long MAX_BODY = 16384, MAX_FW_BODY = 3 * 1024 * 1024;   // el firmware (.bin) es lo único que pasa de 16 KB
static bool readHTTP(Conn& c, std::string& method, std::string& path, std::string& body, std::string& auth, std::string& cookie, bool& xhr) {
    const int fd = c.fd;
    std::string raw; raw.reserve(4096); char tmp[16384];
    struct timeval tv{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    while (true) {
        int n = (int)c.rd(tmp, 1024); if (n <= 0) return false;
        raw.append(tmp, n);
        if (raw.find("\r\n\r\n") != std::string::npos) break;
        if (raw.size() > 16384) return false;
    }
    size_t le = raw.find("\r\n"); if (le == std::string::npos) return false;
    std::istringstream fs(raw.substr(0, le)); std::string ver;
    fs >> method >> path >> ver;
    size_t he = raw.find("\r\n\r\n");
    std::string hdrs = raw.substr(0, he);
    for (auto& ch : hdrs) ch = (char)tolower((unsigned char)ch);
    size_t ap = hdrs.find("\r\nauthorization:");
    if (ap != std::string::npos) {                         // valor con su capitalización original
        ap += 16;
        size_t ae = raw.find("\r\n", ap);
        auth = raw.substr(ap, ae == std::string::npos ? std::string::npos : ae - ap);
        while (!auth.empty() && (auth.front() == ' ' || auth.front() == '\t')) auth.erase(auth.begin());
    }
    size_t cp = hdrs.find("\r\ncookie:");
    if (cp != std::string::npos) {                         // valor con su capitalización original
        cp += 9;
        size_t ce = raw.find("\r\n", cp);
        cookie = raw.substr(cp, ce == std::string::npos ? std::string::npos : ce - cp);
        while (!cookie.empty() && (cookie.front() == ' ' || cookie.front() == '\t')) cookie.erase(cookie.begin());
    }
    xhr = hdrs.find("\r\nx-requested-with: sonos") != std::string::npos;   // lo añade la propia web a sus fetch: no se le manda el diálogo Basic del navegador
    long clen = 0;
    size_t pp = hdrs.find("content-length:");
    if (pp != std::string::npos) {
        pp += 15;
        size_t ee = hdrs.find("\r\n", pp);
        std::string v = hdrs.substr(pp, ee == std::string::npos ? std::string::npos : ee - pp);
        while (!v.empty() && v.front() == ' ') v.erase(v.begin());
        if (!parseInt(v, 0, path == "/api/firmware" ? MAX_FW_BODY : MAX_BODY, clen)) return false;
    }
    body = raw.substr(he + 4);
    while ((long)body.size() < clen) {
        long r = clen - (long)body.size();
        int ch = r < (long)sizeof(tmp) ? (int)r : (int)sizeof(tmp);
        int n = (int)c.rd(tmp, (size_t)ch); if (n <= 0) break;
        body.append(tmp, n);
    }
    return true;
}



// ─── WebServer impl ────────────────────────────────────────────────────────────
// Sin cabeceras CORS a propósito: la API no tiene autenticación y solo debe usarse
// desde la propia UI (mismo origen). Úsala solo en una LAN de confianza.
WebServer::WebServer(UdpStreamer& s, AudioCapture& c, uint16_t p)
    : streamer_(s), capture_(c), port_(p) {}
WebServer::~WebServer() { stop(); }

static int listenOn(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_port = htons(port);
    a.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (sockaddr*)&a, sizeof(a)) < 0 || listen(fd, 10) < 0) {
        fprintf(stderr, "[Web] No se puede abrir :%u: %s\n", port, strerror(errno));
        ::close(fd); return -1;
    }
    return fd;
}

void WebServer::setTLS(uint16_t port, const std::string& cert, const std::string& key) {
    tls_port_ = port; cert_ = cert; key_ = key;
}

bool WebServer::start() {
    sfd_ = listenOn(port_);
    if (sfd_ < 0) return false;
    fprintf(stderr, "[Web] HTTP  :%u\n", port_);
    if (tls_port_) {
#ifdef WITH_TLS
        SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        if (SSL_CTX_use_certificate_chain_file(ctx, cert_.c_str()) == 1 &&
            SSL_CTX_use_PrivateKey_file(ctx, key_.c_str(), SSL_FILETYPE_PEM) == 1 &&
            SSL_CTX_check_private_key(ctx) == 1) {
            ssl_ctx_ = ctx;
            tfd_ = listenOn(tls_port_);
            if (tfd_ >= 0) fprintf(stderr, "[Web] HTTPS :%u (cert %s)\n", tls_port_, cert_.c_str());
        } else {
            SSL_CTX_free(ctx);
            fprintf(stderr, "[Web] HTTPS desactivado: no se pudo cargar %s / %s (ver build.sh)\n",
                    cert_.c_str(), key_.c_str());
        }
#else
        fprintf(stderr, "[Web] Compilado SIN OpenSSL: /calibrate y /ble no funcionarán (instala libssl-dev)\n");
#endif
    }
    running_ = true;
    thread_  = std::thread(&WebServer::serverLoop, this);
    return true;
}
void WebServer::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();            // sale por el timeout de poll()
    if (sfd_ >= 0) { ::close(sfd_); sfd_ = -1; }
    if (tfd_ >= 0) { ::close(tfd_); tfd_ = -1; }
    for (int i = 0; i < 100 && active_.load() > 0; i++) usleep(50000);   // espera clientes
#ifdef WITH_TLS
    if (ssl_ctx_) { SSL_CTX_free((SSL_CTX*)ssl_ctx_); ssl_ctx_ = nullptr; }
#endif
}
void WebServer::serverLoop() {
    while (running_) {
        struct pollfd pf[2]; int np = 0, which[2];
        pf[np] = {sfd_, POLLIN, 0}; which[np++] = 0;
        if (tfd_ >= 0) { pf[np] = {tfd_, POLLIN, 0}; which[np++] = 1; }
        if (poll(pf, np, 300) <= 0) continue;
        for (int i = 0; i < np; i++) {
            if (!(pf[i].revents & POLLIN)) continue;
            int cfd = accept(pf[i].fd, nullptr, nullptr);
            if (cfd < 0) continue;
            if (active_.load() >= 16) { ::close(cfd); continue; }   // tope de conexiones
            bool tls = (which[i] == 1);
            active_++;
            std::thread([this, cfd, tls] { handleClient(cfd, tls); active_--; }).detach();
        }
    }
}
// ── Sesiones de la web: cookie «sonos_sid» (token aleatorio de 128 bits, en memoria, 30 días, máx. 32) ──
static std::string cookieSid(const std::string& cookie) {
    size_t i = cookie.find("sonos_sid=");
    if (i == std::string::npos || (i > 0 && cookie[i - 1] != ' ' && cookie[i - 1] != ';')) return std::string();
    i += 10; size_t e = cookie.find(';', i);
    return cookie.substr(i, e == std::string::npos ? std::string::npos : e - i);
}
std::string WebServer::sessionCreate() {
    unsigned char rnd[16] = {0};
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f || fread(rnd, 1, sizeof(rnd), f) != sizeof(rnd)) { if (f) fclose(f); for (auto& x : rnd) x = (unsigned char)rand(); } else fclose(f);
    char hex[33]; for (int i = 0; i < 16; i++) snprintf(hex + 2 * i, 3, "%02x", rnd[i]);
    const std::string tok = hex; const time_t now = time(nullptr);
    std::lock_guard<std::mutex> lk(sess_mtx_);
    for (auto it = sess_.begin(); it != sess_.end();) it = it->second < now ? sess_.erase(it) : std::next(it);
    while (sess_.size() >= 32) sess_.erase(sess_.begin());
    sess_[tok] = now + 2592000;
    return tok;
}
bool WebServer::sessionValid(const std::string& cookie) {
    const std::string tok = cookieSid(cookie);
    if (tok.size() != 32) return false;
    std::lock_guard<std::mutex> lk(sess_mtx_);
    auto it = sess_.find(tok);
    return it != sess_.end() && it->second >= time(nullptr);
}
void WebServer::sessionDrop(const std::string& cookie) {
    const std::string tok = cookieSid(cookie);
    std::lock_guard<std::mutex> lk(sess_mtx_);
    sess_.erase(tok);
}
void WebServer::handleClient(int cfd, bool tls) {
    Conn c; c.fd = cfd;
    struct timeval tv0{5, 0};
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv0, sizeof(tv0));
    setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &tv0, sizeof(tv0));
#ifdef WITH_TLS
    SSL* ssl = nullptr;
    if (tls) {
        ssl = SSL_new((SSL_CTX*)ssl_ctx_);
        SSL_set_fd(ssl, cfd);
        if (SSL_accept(ssl) <= 0) { SSL_free(ssl); ::close(cfd); return; }   // típico: el navegador rechaza el cert
        c.ssl = ssl;
    }
#else
    (void)tls;
#endif
    std::string m, p, b, auth, cookie; bool xhr = false;
    if (!readHTTP(c, m, p, b, auth, cookie, xhr)) {
#ifdef WITH_TLS
        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
#endif
        ::close(cfd); return;
    }
    std::string resp, ct = "application/json", extra;
    int code = 200; const char* cache = "no-store";
    if (m == "GET" && p.rfind("/fw/ota.bin", 0) != 0) { const size_t qm = p.find('?'); if (qm != std::string::npos) p.resize(qm); }   // las páginas aceptan ?forma=recta (lo lee theme.js); solo el token de la OTA usa la consulta
    // Recursos de la PWA (manifiesto, service worker, iconos): públicos, sin datos; el navegador los pide sin credenciales.
    const char* pwaTxt = nullptr; const char* pwaCt = nullptr;
    const unsigned char* pwaBin = nullptr; size_t pwaBinLen = 0;
    if (m == "GET") {
        if      (p == "/manifest.webmanifest") { pwaTxt = PWA_MANIFEST;  pwaCt = "application/manifest+json; charset=utf-8"; }
        else if (p == "/sw.js")                { pwaTxt = PWA_SW;        pwaCt = "text/javascript; charset=utf-8"; extra = "Service-Worker-Allowed: /\r\n"; }
        else if (p == "/icon.svg")             { pwaTxt = PWA_ICON_SVG;  pwaCt = "image/svg+xml"; }
        else if (p == "/icons/icon-180.png" || p == "/apple-touch-icon.png") { pwaBin = PWA_ICON_180; pwaBinLen = sizeof(PWA_ICON_180); }
        else if (p == "/icons/icon-192.png")   { pwaBin = PWA_ICON_192;  pwaBinLen = sizeof(PWA_ICON_192); }
        else if (p == "/icons/icon-512.png")   { pwaBin = PWA_ICON_512;  pwaBinLen = sizeof(PWA_ICON_512); }
        else if (p == "/fonts/roboto-300.woff2") { pwaBin = PWA_FONT_ROBOTO_300; pwaBinLen = sizeof(PWA_FONT_ROBOTO_300); }
        else if (p == "/fonts/roboto-400.woff2") { pwaBin = PWA_FONT_ROBOTO_400; pwaBinLen = sizeof(PWA_FONT_ROBOTO_400); }
        else if (p == "/fonts/roboto-500.woff2") { pwaBin = PWA_FONT_ROBOTO_500; pwaBinLen = sizeof(PWA_FONT_ROBOTO_500); }
        else if (p == "/fonts/roboto-700.woff2") { pwaBin = PWA_FONT_ROBOTO_700; pwaBinLen = sizeof(PWA_FONT_ROBOTO_700); }
        else if (p == "/icons/icon-maskable-512.png") { pwaBin = PWA_ICON_MASKABLE_512; pwaBinLen = sizeof(PWA_ICON_MASKABLE_512); }
    }
    bool fwget = false;                                     // GET /fw/ota.bin?t=<token>: lo pide el ESP, que no puede enviar contraseña; lo protege un token de un solo uso por orden
    if (m == "GET" && p.rfind("/fw/ota.bin", 0) == 0) fwget = true;
    // Las páginas (HTML) son públicas: no llevan datos y así la app instalada las abre sin diálogo del navegador; la API sí exige clave,
    // y la propia web pide usuario y clave con su pantalla de entrada (POST /api/login → cookie de sesión).
    const bool page = m == "GET" && (p == "/" || p == "/index.html" || p == "/calibrate" || p == "/ble");
    const bool pwa = pwaTxt || pwaBin || fwget || page;
    bool authed = password_.empty();
    if (!authed && auth.size() > 6 && strncasecmp(auth.c_str(), "Basic ", 6) == 0) {
        std::string up = b64decode(auth.substr(6));
        size_t colon = up.find(':');
        authed = colon != std::string::npos && constEq(up.substr(colon + 1), password_);   // el usuario es libre
    }
    if (!authed) authed = sessionValid(cookie);
    if (m == "POST" && p == "/api/login") {                 // entrada desde la web: la clave correcta crea una sesión (cookie HttpOnly)
        if (password_.empty()) { resp = "{\"ok\":true}"; }
        else if (constEq(jsonGet(b, "pass"), password_)) {
            resp = "{\"ok\":true}";
            extra = "Set-Cookie: sonos_sid=" + sessionCreate() + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=2592000\r\n";
        } else {
            usleep(700000);                                  // frena la adivinanza de claves
            code = 401; resp = "{\"error\":\"Clave incorrecta\"}";
        }
    }
    else if (m == "POST" && p == "/api/logout") {
        sessionDrop(cookie);
        extra = "Set-Cookie: sonos_sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0\r\n";
        resp = "{\"ok\":true}";
    }
    else if (!authed && !pwa) {
        code = 401; resp = "{\"error\":\"unauthorized\"}";
        if (!xhr) extra = "WWW-Authenticate: Basic realm=\"AStream\", charset=\"UTF-8\"\r\n";   // a la web no: pide la clave con su pantalla
    }
    else if (fwget) { resp = serveFirmware(p, code); ct = code == 200 ? "application/octet-stream" : "application/json"; }
    else if (pwaTxt) { resp = pwaTxt; ct = pwaCt; }
    else if (pwaBin) { resp.assign(reinterpret_cast<const char*>(pwaBin), pwaBinLen); ct = p.rfind("/fonts/", 0) == 0 ? "font/woff2" : "image/png"; if (p.rfind("/fonts/", 0) == 0) cache = "public, max-age=31536000, immutable"; }
    else if ((p == "/" || p == "/index.html") && m == "GET") { resp = loadPage("panel.html", PAGE_PANEL); ct = "text/html; charset=utf-8"; }
    else if (p == "/calibrate" && m == "GET") { resp = loadPage("calibrate.html", PAGE_CAL); ct = "text/html; charset=utf-8"; }
    else if (p == "/ble"       && m == "GET") { resp = loadPage("ble.html", PAGE_BLE); ct = "text/html; charset=utf-8"; }
    else if (m == "GET")  resp = handleGET(p, code);
    else if (m == "POST") resp = handlePOST(p, b, code);
    else { code = 405; resp = "{\"error\":\"method not allowed\"}"; }
    const char* st = code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 401 ? "Unauthorized" :
                     code == 403 ? "Forbidden" : code == 404 ? "Not Found" : code == 409 ? "Conflict" : "Error";
    std::ostringstream r;
    r << "HTTP/1.1 " << code << " " << st << "\r\nContent-Type: " << ct << "\r\n"
      << extra << "Cache-Control: " << cache << "\r\n"
      << "Content-Length: " << resp.size() << "\r\nConnection: close\r\n\r\n" << resp;
    std::string rs = r.str();
    size_t off = 0;
    while (off < rs.size()) {
        ssize_t w = c.wr(rs.data() + off, rs.size() - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
#ifdef WITH_TLS
    if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
#endif
    ::close(cfd);
}
// Con -w <carpeta> las páginas se leen del disco en cada petición y se montan igual que gen_web_pages.py
// (solo nombres fijos: no hay forma de pedir otro fichero). Si algo falla, se sirve la copia incrustada.
std::string WebServer::loadPage(const char* file, const char* embedded) {
    if (web_dir_.empty()) return embedded;
    auto rd = [&](const std::string& f, std::string& out) {
        FILE* fp = fopen((web_dir_ + "/" + f).c_str(), "rb");
        if (!fp) return false;
        char buf[8192]; size_t n; out.clear();
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) out.append(buf, n);
        fclose(fp); return true;
    };
    auto trim = [](std::string& t) { while (!t.empty() && isspace((unsigned char)t.back())) t.pop_back(); size_t i = 0; while (i < t.size() && isspace((unsigned char)t[i])) i++; t.erase(0, i); };
    std::string h, css, ana, theme;
    if (!rd(file, h) || !rd("shared.css", css) || !rd("analysis.js", ana) || !rd("theme.js", theme)) return embedded;
    trim(css); trim(ana); trim(theme); trim(h);
    auto rep = [&](const char* tag, const std::string& v) {
        const std::string t = tag; size_t pos = 0;
        while ((pos = h.find(t, pos)) != std::string::npos) { h.replace(pos, t.size(), v); pos += v.size(); }
    };
    rep("/*@SHARED_CSS@*/", css); rep("/*@ANALYSIS@*/", ana); rep("/*@THEME_JS@*/", theme);
    return h;
}
std::string WebServer::handleGET(const std::string& p, int& code) {
    if (p == "/api/status") return getStatus();
    if (p == "/api/levels") return getLevels();
    if (p == "/api/cal/status") {
        std::ostringstream o;
        o << "{" << jbool("running", streamer_.calRunning()) << ","
          << jnum("remaining_ms", (double)streamer_.calRemainingMs()) << "}";
        return o.str();
    }
    if (p == "/api/nodes")  return getNodes();
    if (p == "/api/latency") return getLatency();
    if (p == "/api/ble/status") return bleStatus();
    if (p == "/api/system") return systemInfo();
    if (p == "/api/tone")     return toneStatus();
    if (p == "/api/firmware") return firmwareInfo();
    if (p == "/api/profiles") return profilesList();
    if (p == "/api/names") return namesList();
    if (p == "/api/inputs") return inputsGet();
    code = 404; return "{\"error\":\"not found\"}";
}
std::string WebServer::handlePOST(const std::string& p, const std::string& b, int& code) {
    std::string r;
    if      (p == "/api/node")      r = setNode(b);
    else if (p == "/api/volume")    r = setVolume(b);
    else if (p == "/api/playout")   r = jsonGet(b, "auto").empty() ? setPlayout(b) : setPlayoutAuto(b);
    else if (p == "/api/layout")    r = setLayout(b);
    else if (p == "/api/bass")      r = setBass(b);
    else if (p == "/api/dsp")       r = setDsp(b);
    else if (p == "/api/eq")        r = setEq(b);
    else if (p == "/api/latency")   r = setLatency(b);
    else if (p == "/api/calibrate") r = setCalibration(b);
    else if (p == "/api/tone")      r = toneStart(b);
    else if (p == "/api/tone/stop") { streamer_.toneStop(); r = "{\"ok\":true}"; }
    else if (p == "/api/profiles")  r = profilesPost(b);
    else if (p == "/api/names")     r = namesPost(b);
    else if (p == "/api/inputs")    r = inputsPost(b);
    else if (p == "/api/firmware")  r = firmwareUpload(b);
    else if (p == "/api/ota")       r = otaStart(b);
    else if (p == "/api/esp/settings") {
        // olvidar la WiFi deja el altavoz sin red hasta reconfigurarlo por Bluetooth: solo con contraseña configurada
        if (jsonGet(b, "action") == "forget" && password_.empty()) { code = 403; return "{\"error\":\"Configura una contraseña para usar esto: sudo ./scripts/set_password.sh \\\"tu clave\\\"\"}"; }
        r = espSettings(b);
    }
    else if (p == "/api/wifi/push") {
        // cambia la red WiFi de un altavoz: manda la clave por la red local, solo con contraseña configurada
        if (password_.empty()) { code = 403; return "{\"error\":\"Configura una contraseña para usar esto: sudo ./scripts/set_password.sh \\\"tu clave\\\"\"}"; }
        r = wifiPush(b);
    }
    else if (p == "/api/ota/cancel") r = otaCancel(b);
    else if (p == "/api/ble/wifi")      r = bleWifi(b);
    else if (p == "/api/ble/scan")      r = bleScan(b);
    else if (p == "/api/wifi/scan")     r = wifiScan();
    else if (p == "/api/rpi/wifi" || p == "/api/update") {
        // cambian la red de la RPi o ejecutan una actualización: solo con contraseña configurada
        if (password_.empty()) { code = 403; return "{\"error\":\"Configura una contraseña para usar esto: sudo ./scripts/set_password.sh \\\"tu clave\\\"\"}"; }
        r = (p == "/api/update") ? updatePost() : rpiWifiPost(b);
    }
    else if (p == "/api/ble/provision") r = bleProvision(b);
    else if (p == "/api/ble/identify")  r = bleIdentify(b);
    else if (p == "/api/cal/start") { r = calStart(b); if (r.find("\"error\"") != std::string::npos) { code = 409; return r; } }
    else if (p == "/api/cal/stop")  { streamer_.calStop(); r = "{\"ok\":true}"; }
    else { code = 404; return "{\"error\":\"not found\"}"; }
    // /api/ble/*: la respuesta del ayudante puede traer «"error": null» dentro de la lista de dispositivos; solo cuenta si empieza por {"error"
    const bool is_err = (p.rfind("/api/ble/", 0) == 0 || p.rfind("/api/wifi/", 0) == 0 || p.rfind("/api/rpi/", 0) == 0 || p == "/api/update") ? r.rfind("{\"error\"", 0) == 0 : r.find("\"error\"") != std::string::npos;
    if (is_err) code = 400;
    return r;
}
// ─── Configuración de ESP32 por Bluetooth desde la RPi ────────────────────────────
std::string WebServer::bleStatus() {
    auto st = ble_.settings();
    std::ostringstream o;
    o << "{" << jbool("available", ble_.available()) << "," << jstr("ssid", st.ssid) << ","
      << jbool("has_pass", !st.pass.empty()) << "," << jstr("rpi_ip", st.rpi_ip) << "}";   // la contraseña NO sale nunca por la API
    return o.str();
}
std::string WebServer::bleWifi(const std::string& b) {
    std::string err;
    if (!ble_.setSettings(jsonGet(b, "ssid"), jsonGet(b, "pass"), jsonGet(b, "rpi_ip"), err)) return "{\"error\":\"" + jesc(err) + "\"}";
    return "{\"ok\":true}";
}
std::string WebServer::systemInfo() {
    SystemManager::Wifi w; const bool wok = sys_.wifiNow(w);
    auto u = sys_.updateStatus();
    std::ostringstream o;
    o << "{" << jstr("version", SystemManager::version()) << "," << jbool("protected", !password_.empty()) << ","
      << "\"wifi\":{" << jbool("available", wok) << "," << jstr("ssid", w.ssid) << "," << jstr("iface", w.iface) << ","
      << jstr("state", w.state.empty() ? "idle" : w.state) << "," << jstr("msg", w.msg) << "," << jstr("target", w.target) << "},"
      << "\"update\":{" << jstr("state", u.state) << "," << jstr("log", u.log) << "}}";
    return o.str();
}
std::string WebServer::rpiWifiPost(const std::string& b) {
    std::string err;
    if (!sys_.wifiConnect(jsonGet(b, "ssid"), jsonGet(b, "pass"), err)) return "{\"error\":\"" + jesc(err) + "\"}";
    return "{\"ok\":true}";
}
std::string WebServer::updatePost() {
    std::string err;
    if (!sys_.updateStart(err)) return "{\"error\":\"" + jesc(err) + "\"}";
    return "{\"ok\":true}";
}
std::string WebServer::wifiScan() {
    std::vector<BleManager::WifiNet> nets; std::string err;
    if (!ble_.scanWifi(nets, err)) return "{\"error\":\"" + jesc(err) + "\"}";
    std::ostringstream o; o << "{\"networks\":[";
    for (size_t i = 0; i < nets.size(); i++) {
        const auto& n = nets[i];
        o << (i ? "," : "") << "{" << jstr("ssid", n.ssid) << "," << jnum("signal", n.signal) << "," << jnum("freq", n.freq) << ","
          << jstr("band", n.freq >= 5925 ? "6 GHz" : n.freq >= 4900 ? "5 GHz" : "2,4 GHz") << "," << jstr("security", n.security) << "}";
    }
    o << "]}";
    return o.str();
}
std::string WebServer::bleScan(const std::string& b) {
    long secs = 6; std::string s = jsonGet(b, "seconds");
    if (!s.empty() && !parseInt(s, 3, 15, secs)) return "{\"error\":\"seconds fuera de rango (3-15)\"}";
    std::ostringstream in; in << "{\"seconds\":" << secs << ",\"used\":[";
    bool first = true;
    for (const auto& n : streamer_.getNodes()) if (n.online) { in << (first ? "" : ",") << (int)n.id; first = false; }
    in << "]}";
    return ble_.run("scan", in.str(), (int)secs + 12 * MAX_NODES + 15);
}
std::string WebServer::bleProvision(const std::string& b) {
    std::string addr = jsonGet(b, "address");
    long id;
    if (!BleManager::validAddress(addr)) return "{\"error\":\"dirección Bluetooth no válida\"}";
    const std::string ids = jsonGet(b, "id");
    if (ids.empty() || ids == "auto" || ids == "255") id = ESP_ID_AUTO;      // por defecto el ESP recibe su casilla de la RPi por MAC
    else if (!parseInt(ids, 0, MAX_NODES - 1, id)) return "{\"error\":\"id fuera de rango (0-9)\"}";
    auto st = ble_.settings();
    if (st.ssid.empty()) return "{\"error\":\"Primero guarda el nombre y la contraseña de la WiFi\"}";
    if (id != ESP_ID_AUTO && jsonGet(b, "force") != "true")
        for (const auto& n : streamer_.getNodes())
            if (n.id == (int)id && n.online)
                return "{\"error\":\"El ID " + std::to_string(id) + " ya lo usa un altavoz en línea. Elige otro número.\"}";
    std::string in = "{" + jstr("address", addr) + ",\"id\":" + std::to_string(id) + "," + jstr("ssid", st.ssid) + ","
                   + jstr("pass", st.pass) + "," + jstr("rpi_ip", st.rpi_ip) + "}";
    return ble_.run("provision", in, 60);
}
std::string WebServer::bleIdentify(const std::string& b) {
    std::string addr = jsonGet(b, "address");
    if (!BleManager::validAddress(addr)) return "{\"error\":\"dirección Bluetooth no válida\"}";
    return ble_.run("identify", "{" + jstr("address", addr) + "}", 30);
}
// Niveles de entrada por canal y estado de la transmisión (ligero: la UI lo consulta ~8 veces por segundo)
std::string WebServer::getLevels() {
    const int64_t sig = streamer_.signalAgeMs(), blk = streamer_.captureAgeMs();
    const bool capturing = blk >= 0 && blk < 1000;                             // están llegando bloques de la tarjeta
    const bool signal = capturing && sig >= 0 && sig < 2000;                     // algún canal > -70 dBFS en los últimos 2 s
    int online = 0;
    for (const auto& n : streamer_.getNodes()) if (n.enabled && n.online) online++;
    const uint32_t pps = streamer_.packetsPerSec();
    std::ostringstream o;
    o << "{" << jbool("capturing", capturing) << "," << jbool("signal", signal) << ","
      << jnum("signal_age_ms", (double)sig) << "," << jnum("pps", pps) << ","
      << jbool("sending", capturing && online > 0 && pps > 0) << "," << jnum("nodes_online", online) << ",\"db\":[";
    char b[16];
    for (int c = 0; c < streamer_.inputChannels(); c++) { snprintf(b, sizeof(b), "%s%.1f", c ? "," : "", streamer_.meterDb(c)); o << b; }
    o << "]}";
    return o.str();
}
std::string WebServer::getStatus() {
    const auto& c = capture_.config();
    std::ostringstream o;
    o << "{" << jstr("device", c.device) << ","
      << jnum("sample_rate", c.sample_rate) << ","
      << "\"ch_names\":" << [&] { std::string r = "["; auto v = streamer_.channelNames(); for (size_t i = 0; i < v.size(); i++) r += (i ? ",\"" : "\"") + jesc(v[i]) + "\""; return r + "]"; }() << ",\"ch_ids\":" << [&] { std::string r = "["; auto v = streamer_.channelIds(); for (size_t i = 0; i < v.size(); i++) r += (i ? ",\"" : "\"") + jesc(v[i]) + "\""; return r + "]"; }() << ","
      << jnum("channels", c.channels) << ","
      << jnum("period_frames", c.period_size) << ","
      << jnum("playout_ms", streamer_.playoutUs() / 1000.0) << ","
      << jnum("playout_min_ms", PLAYOUT_MIN_US / 1000.0) << "," << jnum("playout_max_ms", PLAYOUT_MAX_US / 1000.0) << ","
      << jnum("send_errors", (double)streamer_.sendErrors()) << ","
      << jnum("max_nodes", MAX_NODES) << "," << jnum("layout", streamer_.layout()) << ","
      << jnum("xover_hz", streamer_.xoverHz()) << ","
      << jbool("dsp_xover", streamer_.xoverOn()) << "," << jbool("dsp_eq", streamer_.eqOn()) << "," << jnum("lipsync_extra_ms", streamer_.lipsyncExtraMs()) << ","
      << jbool("running", capture_.isRunning() || (mixer_ && mixer_->active())) << "," << jnum("input_channels", streamer_.inputChannels()) << ","
      << jbool("lfe_front", streamer_.lfeFront()) << "," << jnum("lfe_db", streamer_.lfeDb()) << "," << jbool("lfe_active", streamer_.lfeActive()) << ","
      << jnum("night", streamer_.nightLevel()) << "," << jnum("profile_active", streamer_.profileActive()) << ","
      << jnum("master_volume", streamer_.masterVolume()) << "}";
    return o.str();
}
static std::string macStr(uint64_t m) {                   // 0 = casilla libre -> ""
    if (!m) return "";
    char b[18]; snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned)(m >> 40) & 255, (unsigned)(m >> 32) & 255,
                         (unsigned)(m >> 24) & 255, (unsigned)(m >> 16) & 255, (unsigned)(m >> 8) & 255, (unsigned)m & 255);
    return b;
}
std::string WebServer::getNodes() {
    auto nodes = streamer_.getNodes();
    std::ostringstream o;
    o << "[";
    bool first = true;
    for (const auto& n : nodes) {
        if (!first) o << ",";
        first = false;
        double fill = n.buffer_frames > 0
            ? std::min(100.0, n.buffer_frames * 100.0 / (RX_QUEUE_PACKETS * FRAMES_PER_PACKET)) : 0.0;
        o << "{" << jnum("id", n.id) << ","
          << jstr("ip", n.ip) << ","
          << jnum("ch_left",  n.ch_left)  << ","
          << jnum("ch_right", n.ch_right) << ","
          << jnum("volume_l", n.volume_l) << ","
          << jnum("volume_r", n.volume_r) << ","
          << jbool("enabled", n.enabled)  << ","
          << jbool("mono",    n.mono)     << ","
          << jnum("role",     n.role)     << ","
          << jbool("known",   n.known)    << ","
          << jstr("mac",     macStr(n.mac)) << ","
          << jstr("name",    streamer_.nodeName(n.id)) << ","
          << jnum("pos_x",    std::round(n.pos_x * 100.0) / 100.0) << "," << jnum("pos_y", std::round(n.pos_y * 100.0) / 100.0) << ","
          << jnum("pos2_x", std::round(n.pos2_x * 100.0) / 100.0) << "," << jnum("pos2_y", std::round(n.pos2_y * 100.0) / 100.0) << ","
          << "\"eq\":[" ;
        for (int b = 0; b < EQ_BANDS; b++) o << (b ? "," : "") << std::round(n.eq_db[b] * 100.0) / 100.0;
        o << "],"
          << jnum("recovered_packets", n.recovered_packets) << "," << jnum("retx_sent", n.retx_sent) << ","
          << jbool("online",  n.online)   << ","
          << jnum("jitter_us",    n.jitter_us)    << ","
          << jnum("rtt_us",       n.rtt_us)       << ","
          << jnum("sync_err_us",  n.sync_err_us)  << ","
          << jnum("clock_offset_us", (double)n.clock_offset_us) << ","
          << jnum("lost_packets", n.lost_packets) << ","
          << jnum("late_packets", n.late_packets) << ","
          << jnum("underruns",    n.underruns)    << ","
          << jnum("recent_lost", n.recent_lost) << "," << jnum("recent_late", n.recent_late) << "," << jnum("recent_underruns", n.recent_under) << "," << jnum("recent_min", ESPNode::RECENT_MIN) << ","
          << jnum("drift_ppm",    n.drift_ppm)    << ","
          << jnum("buffer_frames",n.buffer_frames) << ","
          << jnum("latency_offset_us", n.latency_offset_us) << ","
          << jnum("fill_pct", fill) << ","
          << jnum("dist_m", n.dist_m < 0 ? -1.0 : std::round(n.dist_m * 100.0) / 100.0) << ","
          << jnum("fw_version", n.fw_version) << "," << jnum("chip", n.chip) << "," << jnum("rssi_dbm", n.rssi_dbm) << ","
          << jbool("ota_busy", n.ota_busy)
          << "}";
    }
    o << "]";
    return o.str();
}
std::string WebServer::setNode(const std::string& b) {
    static const char* BAD = "{\"error\":\"invalid parameters\"}";
    long id = 0;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return BAD;
    auto nodes = streamer_.getNodes();
    const ESPNode& cur = nodes[id];
    long chl = cur.ch_left, chr = cur.ch_right, vl = cur.volume_l, vr = cur.volume_r;
    bool enabled = cur.enabled, mono = cur.mono;
    std::string s;
    if (!(s = jsonGet(b, "ch_left")).empty()  && !parseInt(s, 0, streamer_.inputChannels() - 1, chl)) return BAD;
    if (!(s = jsonGet(b, "ch_right")).empty() && !parseInt(s, 0, streamer_.inputChannels() - 1, chr)) return BAD;
    if (!(s = jsonGet(b, "volume_l")).empty() && !parseInt(s, 0, 255, vl)) return BAD;
    if (!(s = jsonGet(b, "volume_r")).empty() && !parseInt(s, 0, 255, vr)) return BAD;
    if (!(s = jsonGet(b, "enabled")).empty()) enabled = (s != "false" && s != "0");
    if (!(s = jsonGet(b, "mono")).empty())    mono    = (s != "false" && s != "0");
    {   // «forget»: quitar del panel un ESP apagado (su configuración se conserva por si vuelve)
        std::string fg = jsonGet(b, "forget");
        if (fg == "true" || fg == "1") {
            if (!streamer_.forgetNode((uint8_t)id)) return "{\"error\":\"ese ESP está en línea: apágalo antes de quitarlo\"}";
            return "{\"ok\":true}";
        }
    }
    if (b.find("\"name\"") != std::string::npos) {                        // nombre del altavoz (≤ 12 caracteres)
        std::string err;
        if (!streamer_.setNodeName((int)id, jsonGet(b, "name"), err)) return "{\"error\":\"" + jesc(err) + "\"}";
    }
    long role = cur.role;
    if (!(s = jsonGet(b, "role")).empty() && !parseInt(s, 0, 2, role)) return BAD;
    if (!streamer_.setNodeConfig((uint8_t)id, (uint8_t)chl, (uint8_t)chr,
                                 (uint8_t)vl, (uint8_t)vr, enabled, mono)) return BAD;
    if (!streamer_.setNodeRole((uint8_t)id, (uint8_t)role)) return BAD;
    // Posición en el plano: «pos_x» y «pos_y» (0..100, % del plano) siempre juntos; «pos_auto»:true la devuelve a la posición según el canal.
    std::string sx = jsonGet(b, "pos_x"), sy = jsonGet(b, "pos_y"), sa = jsonGet(b, "pos_auto");
    if (sa == "true" || sa == "1") { if (!streamer_.setNodePos((uint8_t)id, -1, -1, 0) || !streamer_.setNodePos((uint8_t)id, -1, -1, 1)) return BAD; }
    else if (!sx.empty() || !sy.empty()) {
        double px, py;
        if (!parseFloat(sx, 0, 100, px) || !parseFloat(sy, 0, 100, py)) return BAD;
        if (!streamer_.setNodePos((uint8_t)id, (float)px, (float)py, 0)) return BAD;
    }
    if (!(s = jsonGet(b, "dist_m")).empty()) {                               // distancia a quien escucha (m); "-1" la borra
        double d;
        if (!parseFloat(s, -1.0, 30.0, d) || !streamer_.setNodeDist((uint8_t)id, (float)d)) return "{\"error\":\"dist_m fuera de rango (0.2..30, o -1 para borrar)\"}";
    }
    std::string tx = jsonGet(b, "pos2_x"), ty = jsonGet(b, "pos2_y");      // canal derecho (altavoz estéreo)
    if (!tx.empty() || !ty.empty()) {
        double qx, qy;
        if (!parseFloat(tx, 0, 100, qx) || !parseFloat(ty, 0, 100, qy)) return BAD;
        if (!streamer_.setNodePos((uint8_t)id, (float)qx, (float)qy, 1)) return BAD;
    }
    return "{\"ok\":true}";
}
std::string WebServer::setLayout(const std::string& b) {
    std::string l = jsonGet(b, "layout");
    int v = (l == "stereo5") ? LAYOUT_STEREO5 : (l == "mono10") ? LAYOUT_MONO10 : -1;
    if (v < 0 || !streamer_.setLayout(v)) return "{\"error\":\"layout debe ser stereo5 o mono10\"}";
    return "{\"ok\":true}";
}
// {"xover":bool,"eq":bool,"lfe":bool,"lfe_db":-12..10,"night":0..3}  (cada clave es opcional). xover y eq en false = pass-through.
std::string WebServer::setDsp(const std::string& b) {
    static const char* BAD = "{\"error\":\"usa xover, eq, lfe (true|false), lfe_db (-12..10) y/o night (0..3)\"}";
    auto flag = [&](const char* k, int& out) -> bool {          // out: -1 sin cambio, 0/1
        std::string v = jsonGet(b, k); out = -1;
        if (v.empty()) return true;
        if (v == "true" || v == "1") { out = 1; return true; }
        if (v == "false" || v == "0") { out = 0; return true; }
        return false;
    };
    int xo, eq, lf;
    if (!flag("xover", xo) || !flag("eq", eq) || !flag("lfe", lf)) return BAD;
    double ldb = 0; long nt = 0;
    const std::string sl = jsonGet(b, "lfe_db"), sn = jsonGet(b, "night");
    if (!sl.empty() && !parseFloat(sl, -12.0, 10.0, ldb)) return BAD;
    if (!sn.empty() && !parseInt(sn, 0, 3, nt)) return BAD;
    if (xo < 0 && eq < 0 && lf < 0 && sl.empty() && sn.empty()) return BAD;
    if (xo >= 0) streamer_.setXoverOn(xo == 1);
    if (eq >= 0) streamer_.setEqOn(eq == 1);
    if (lf >= 0) streamer_.setLfeFront(lf == 1);
    if (!sl.empty()) streamer_.setLfeDb((float)ldb);
    if (!sn.empty()) streamer_.setNightLevel((int)nt);
    return "{\"ok\":true}";
}
// Tono de prueba por altavoz: {"id":N,"side":0|1|2,"ms":1000..10000,"solo":bool,"level_db":-40..-6}
std::string WebServer::toneStart(const std::string& b) {
    long id, side = 2, ms = 3000; double lvl = -20; std::string s;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return "{\"error\":\"id fuera de rango (0-9)\"}";
    if (!(s = jsonGet(b, "side")).empty() && !parseInt(s, 0, 2, side)) return "{\"error\":\"side debe ser 0, 1 o 2\"}";
    if (!(s = jsonGet(b, "ms")).empty() && !parseInt(s, 1000, 10000, ms)) return "{\"error\":\"ms fuera de rango (1000..10000)\"}";
    if (!(s = jsonGet(b, "level_db")).empty() && !parseFloat(s, -40.0, -6.0, lvl)) return "{\"error\":\"level_db fuera de rango (-40..-6)\"}";
    const bool solo = jsonGet(b, "solo") != "false" && jsonGet(b, "solo") != "0";   // por defecto calla el resto
    if (!streamer_.toneStart((uint8_t)id, (int)side, (int)ms, solo, (float)lvl))
        return "{\"error\":\"no se pudo: el altavoz debe estar en línea y habilitado, y sin calibración en curso\"}";
    return "{\"ok\":true}";
}
std::string WebServer::toneStatus() {
    std::ostringstream o;
    o << "{" << jbool("active", streamer_.toneActive()) << "," << jnum("id", streamer_.toneNode()) << ","
      << jnum("side", streamer_.toneSide()) << "," << jnum("remaining_ms", (double)streamer_.toneRemainingMs()) << "}";
    return o.str();
}
// ─── Actualización de firmware por WiFi (OTA) ─────────────────────────────────────
// Flujo: 1) el usuario sube el .bin (POST /api/firmware, cuerpo binario) y se guarda en memoria;
//        2) POST /api/ota {"id":N}: se crea un token y se manda al ESP la orden PKT_OTA por UDP (puerto HTTP, tamaño y token);
//        3) el ESP descarga GET /fw/ota.bin?t=<token> por HTTP y se reinicia; al volver a anunciarse (HELLO) termina la OTA.
// SIN VERIFICAR EN HARDWARE: el lado del ESP (esp_https_ota, particiones, rollback) no se ha compilado con ESP-IDF real.
static uint64_t nowUs() { return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
// Imagen de aplicación de ESP-IDF: byte 0 = 0xE9; chip id en 12..13; esp_app_desc_t con magic 0xABCD5432 en el byte 32 (versión en 48..79).
// Devuelve FW_CHIP_* (1 C5, 2 ESP32), 0 si el chip no se reconoce, -1 si no es una imagen de aplicación.
static int imageChip(const std::string& d, std::string& ver) {
    ver.clear();
    if (d.size() < 80 || (unsigned char)d[0] != 0xE9 || d[1] < 1 || d[1] > 16) return -1;
    const unsigned magic = (unsigned char)d[32] | ((unsigned char)d[33] << 8) | ((unsigned char)d[34] << 16) | ((unsigned)(unsigned char)d[35] << 24);
    if (magic != 0xABCD5432u) return -1;
    for (int i = 48; i < 80 && d[i]; i++) if ((unsigned char)d[i] >= 0x20 && (unsigned char)d[i] < 0x7f) ver += d[i];
    const unsigned chip = (unsigned char)d[12] | ((unsigned char)d[13] << 8);
    return chip == 0x0000 ? FW_CHIP_ESP32 : chip == 0x0017 ? FW_CHIP_C5 : FW_CHIP_UNKNOWN;
}
std::string WebServer::firmwareUpload(const std::string& b) {
    if (b.size() < 65536) return "{\"error\":\"El archivo es demasiado pequeño para ser un firmware (mínimo 64 KB)\"}";
    std::string ver; const int chip = imageChip(b, ver);
    if (chip < 0) return "{\"error\":\"No es un firmware de aplicación de ESP-IDF (usa build/receiver.bin, no el bootloader ni la tabla de particiones)\"}";
    std::lock_guard<std::mutex> lk(fw_mtx_);
    fw_data_ = b; fw_chip_ = chip; fw_ver_ = ver; fw_token_ = 0;
    std::ostringstream o;
    o << "{\"ok\":true," << jnum("size", (double)b.size()) << "," << jnum("chip", chip) << "," << jstr("version", ver) << "}";
    return o.str();
}
std::string WebServer::firmwareInfo() {
    std::lock_guard<std::mutex> lk(fw_mtx_);
    std::ostringstream o;
    o << "{" << jbool("loaded", !fw_data_.empty()) << "," << jnum("size", (double)fw_data_.size()) << "," << jnum("chip", fw_chip_) << "," << jstr("version", fw_ver_) << "}";
    return o.str();
}
std::string WebServer::wifiPush(const std::string& b) {
    long id;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return "{\"error\":\"id fuera de rango (0-9)\"}";
    const std::string ssid = jsonGet(b, "ssid"), pass = jsonGet(b, "pass");
    if (ssid.empty() || ssid.size() > 32) return "{\"error\":\"El nombre de la red debe tener entre 1 y 32 caracteres\"}";
    if (!pass.empty() && (pass.size() < 8 || pass.size() > 64)) return "{\"error\":\"La clave debe tener entre 8 y 64 caracteres (o estar vacía si la red es abierta)\"}";
    for (char c : ssid + pass) if ((unsigned char)c < 0x20 || c == 0x7f) return "{\"error\":\"El nombre y la clave no pueden llevar caracteres de control\"}";
    const auto nodes = streamer_.getNodes();
    if (!nodes[id].online) return "{\"error\":\"El altavoz no está en línea\"}";
    if (nodes[id].fw_version < 14) return "{\"error\":\"Este altavoz tiene un firmware sin cambio de red por WiFi (hace falta la versión 14): actualízalo primero\"}";
    if (!streamer_.sendWifi((uint8_t)id, ssid, pass)) return "{\"error\":\"No se pudo enviar la orden (el altavoz se está actualizando o no está en línea)\"}";
    return "{\"ok\":true}";
}
std::string WebServer::espSettings(const std::string& b) {
    long id;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return "{\"error\":\"id fuera de rango (0-9)\"}";
    uint8_t act = SET_ACT_NONE, led = SET_LED_KEEP, tx = 0; uint16_t win = 0; long v;
    const std::string a = jsonGet(b, "action");
    if (a == "reboot") act = SET_ACT_REBOOT; else if (a == "ble_open") act = SET_ACT_BLE_OPEN; else if (a == "forget") act = SET_ACT_FORGET;
    else if (!a.empty()) return "{\"error\":\"Acción desconocida\"}";
    std::string s;
    if (!(s = jsonGet(b, "led")).empty()) { if (s != "off" && s != "status") return "{\"error\":\"led: off o status\"}"; led = s == "off" ? SET_LED_OFF : SET_LED_STATUS; }
    if (!(s = jsonGet(b, "ble_window")).empty()) { if (!parseInt(s, 5, 3600, v)) return "{\"error\":\"La ventana del BLE debe estar entre 5 y 3600 s\"}"; win = (uint16_t)v; }
    if (!(s = jsonGet(b, "tx_power")).empty()) { if (!parseInt(s, 2, 20, v)) return "{\"error\":\"La potencia debe estar entre 2 y 20 dBm\"}"; tx = (uint8_t)v; }
    if (act == SET_ACT_NONE && led == SET_LED_KEEP && !win && !tx) return "{\"error\":\"No hay nada que cambiar\"}";
    const auto nodes = streamer_.getNodes();
    if (!nodes[id].online) return "{\"error\":\"El altavoz no está en línea\"}";
    if (nodes[id].fw_version < 16) return "{\"error\":\"Este altavoz tiene un firmware sin ajustes por WiFi (hace falta la versión 16): actualízalo primero\"}";
    if (!streamer_.sendSettings((uint8_t)id, act, led, win, tx)) return "{\"error\":\"No se pudo enviar (el altavoz se está actualizando o no está en línea)\"}";
    return "{\"ok\":true}";
}
std::string WebServer::otaStart(const std::string& b) {
    long id;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return "{\"error\":\"id fuera de rango (0-9)\"}";
    const auto nodes = streamer_.getNodes();
    const ESPNode& n = nodes[id];
    if (!n.online) return "{\"error\":\"El altavoz no está en línea\"}";
    if (n.ota_busy) return "{\"error\":\"Ese altavoz ya se está actualizando\"}";
    if (n.fw_version == 0) return "{\"error\":\"Este altavoz tiene un firmware antiguo sin actualización por WiFi: la primera vez hay que flashearlo por cable\"}";
    uint32_t size; uint64_t tok;
    {
        std::lock_guard<std::mutex> lk(fw_mtx_);
        if (fw_data_.empty()) return "{\"error\":\"Primero sube el archivo de firmware\"}";
        if (fw_chip_ > 0 && n.chip > 0 && fw_chip_ != n.chip)
            return "{\"error\":\"Este firmware es para otro chip (" + std::string(fw_chip_ == FW_CHIP_C5 ? "ESP32-C5" : "ESP32 clásico") + "), no para este altavoz\"}";
        std::random_device rd;
        tok = ((uint64_t)rd() << 32) ^ rd(); if (!tok) tok = 1;
        fw_token_ = tok; fw_token_until_us_ = nowUs() + 300ull * 1000000ull;   // el ESP tiene 5 min para empezar la descarga
        size = (uint32_t)fw_data_.size();
    }
    if (!streamer_.sendOta((uint8_t)id, port_, size, tok)) return "{\"error\":\"No se pudo enviar la orden de actualización\"}";
    return "{\"ok\":true}";
}
std::string WebServer::otaCancel(const std::string& b) {
    long id;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return "{\"error\":\"id fuera de rango (0-9)\"}";
    streamer_.cancelOta((uint8_t)id);
    return "{\"ok\":true}";
}
// GET /fw/ota.bin?t=<token hex>: devuelve el firmware solo con el token vigente. Va por HTTP sin cifrar (LAN de confianza).
std::string WebServer::serveFirmware(const std::string& path, int& code) {
    const size_t q = path.find("?t=");
    const std::string t = q == std::string::npos ? "" : path.substr(q + 3);
    char* end = nullptr; const unsigned long long tok = t.empty() ? 0 : strtoull(t.c_str(), &end, 16);
    std::lock_guard<std::mutex> lk(fw_mtx_);
    if (!tok || !end || *end || tok != fw_token_ || nowUs() > fw_token_until_us_ || fw_data_.empty()) { code = 403; return "{\"error\":\"token no válido\"}"; }
    return fw_data_;
}
// Entradas de audio: GET lista las placas (configuradas y detectadas) con su estado; POST {"devices":[…],"clock":"…"} guarda y reinicia el servicio
std::string WebServer::inputsGet() {
    const auto cards = InputMixer::listCards();
    InputConfig saved;
    if (inputs_path_.empty() || !InputMixer::load(inputs_path_, saved)) saved = InputMixer::defaultConfig(cards);
    { std::string e; InputMixer::validate(saved, e); }
    std::vector<bool> used(cards.size(), false);
    std::ostringstream o;
    o << "{\"devices\":[";
    for (size_t i = 0; i < saved.devs.size(); i++) {
        const auto& d = saved.devs[i];
        const int ci = InputMixer::matchCard(d.key, cards);
        if (ci >= 0) used[(size_t)ci] = true;
        o << (i ? "," : "") << "{" << jstr("key", d.key) << "," << jstr("name", ci >= 0 && !cards[(size_t)ci].name.empty() ? cards[(size_t)ci].name : d.key) << "," << jnum("channels", d.channels) << ","
          << jbool("enabled", d.enabled) << "," << jstr("tag", d.tag) << "," << jbool("connected", ci >= 0) << "," << jstr("card", ci >= 0 ? cards[(size_t)ci].id : "") << ","
          << jnum("max_channels", ci >= 0 ? cards[(size_t)ci].max_channels : 0) << "," << jbool("busy", ci >= 0 && cards[(size_t)ci].busy) << "}";
    }
    o << "],\"cards\":[";
    bool first = true;
    for (size_t i = 0; i < cards.size(); i++) {
        if (used[i]) continue;
        int same = 0; for (const auto& c : cards) if (c.name == cards[i].name) same++;
        o << (first ? "" : ",") << "{" << jstr("key", same == 1 && !cards[i].name.empty() ? cards[i].name : cards[i].id) << "," << jstr("name", cards[i].name) << "," << jstr("id", cards[i].id) << ","
          << jnum("max_channels", cards[i].max_channels) << "," << jbool("busy", cards[i].busy) << "}";
        first = false;
    }
    std::string running;
    if (mixer_ && !direct_) running = mixer_->statusJson();
    else {                                                         // captura directa: una sola placa que manda su reloj
        std::ostringstream r; r << "{\"devices\":[";
        bool f = true;
        for (const auto& d : running_cfg_.devs) if (d.enabled) { r << (f ? "" : ",") << "{" << jstr("key", d.key) << "," << jbool("online", capture_.isRunning()) << ",\"first\":0,\"error\":\"\",\"master\":true,\"underruns\":0}"; f = false; }
        r << "],\"clock\":\"" << jesc(InputMixer::effectiveClock(running_cfg_)) << "\",\"total\":" << streamer_.inputChannels() << "}";
        running = r.str();
    }
    o << "]," << jstr("clock", saved.clock) << "," << jstr("clock_effective", InputMixer::effectiveClock(saved)) << ",\"max_total\":" << MAX_INPUTS << ","
      << jbool("direct", direct_) << ",\"running\":" << running << "," << jbool("restart_needed", !(saved == running_cfg_)) << "}";
    return o.str();
}
std::string WebServer::inputsPost(const std::string& b) {
    if (inputs_path_.empty()) return "{\"error\":\"esta Raspberry no guarda configuración\"}";
    InputConfig c; std::string err;
    if (!InputMixer::parseJson(b, c, err)) return "{" + jstr("error", err) + "}";
    if (!InputMixer::save(inputs_path_, c)) return "{\"error\":\"no se pudo guardar en la Raspberry\"}";
    const bool restart = (jsonGet(b, "restart") != "false");
    if (restart && restart_cb_) restart_cb_();
    return std::string("{\"ok\":true,") + jbool("restarting", restart && (bool)restart_cb_) + "}";
}
// Nombres de las 10 entradas: GET {"names":[10 nombres]}; POST {"names":[...10]} o {"reset":true}
std::string WebServer::namesList() {
    auto v = streamer_.channelNames();
    std::ostringstream o;
    o << "{\"names\":[";
    for (size_t i = 0; i < v.size(); i++) o << (i ? "," : "") << "\"" << jesc(v[i]) << "\"";
    o << "],\"ids\":[";
    const auto ids = streamer_.channelIds();
    for (size_t i = 0; i < ids.size(); i++) o << (i ? "," : "") << "\"" << jesc(ids[i]) << "\"";
    o << "]}";
    return o.str();
}
std::string WebServer::namesPost(const std::string& b) {
    std::string err;
    std::vector<std::string> v;
    if (jsonGet(b, "reset") == "true") { for (const auto& id : streamer_.channelIds()) v.push_back(UdpStreamer::defaultNameForId(id)); }
    else {
        const size_t k = b.find("\"names\"");
        const size_t lb = k == std::string::npos ? k : b.find('[', k), rb = lb == std::string::npos ? lb : b.find(']', lb);
        if (rb == std::string::npos) return "{\"error\":\"falta names (un nombre por entrada activa)\"}";
        size_t pos = lb + 1;
        while (pos < rb) {                                             // cadenas JSON con \" y \\ escapados
            size_t q = b.find('"', pos); if (q == std::string::npos || q >= rb) break;
            std::string s; size_t i = q + 1;
            for (; i < b.size() && b[i] != '"'; i++) {
                if (b[i] == '\\' && i + 1 < b.size()) {
                    i++; char c = b[i];
                    if (c == 'u' && i + 4 < b.size()) {                  // \uXXXX (algunos clientes escapan los acentos) -> UTF-8
                        auto hex4 = [&](size_t at) { return (unsigned)strtoul(b.substr(at, 4).c_str(), nullptr, 16); };
                        unsigned cp = hex4(i + 1); i += 4;
                        if (cp >= 0xD800 && cp < 0xDC00 && i + 6 < b.size() && b[i + 1] == '\\' && b[i + 2] == 'u') {
                            const unsigned lo = hex4(i + 3);
                            if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 6; }
                        }
                        if (cp < 0x80) s += (char)cp;
                        else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 63)); }
                        else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
                        else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 63)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
                    } else s += c == 'n' ? ' ' : c == 't' ? ' ' : c;
                }
                else s += b[i];
            }
            v.push_back(s); pos = i + 1;
        }
    }
    if (!streamer_.setChannelNames(v, err)) return "{\"error\":\"" + jesc(err) + "\"}";
    return namesList();
}
// Perfiles guardados: GET lista; POST {"save":"nombre"} | {"load":idx} | {"delete":idx}
std::string WebServer::profilesList() {
    auto names = streamer_.profileNames();
    std::ostringstream o;
    o << "{" << jnum("active", streamer_.profileActive()) << "," << jnum("max", UdpStreamer::MAX_PROFILES) << ",\"profiles\":[";
    for (size_t i = 0; i < names.size(); i++) o << (i ? "," : "") << "\"" << jesc(names[i]) << "\"";
    o << "]}";
    return o.str();
}
std::string WebServer::profilesPost(const std::string& b) {
    static const char* BAD = "{\"error\":\"usa save (nombre), load (índice) o delete (índice)\"}";
    long idx; std::string nm = jsonGet(b, "save"), ld = jsonGet(b, "load"), dl = jsonGet(b, "delete");
    if (!nm.empty()) {
        std::string err;
        if (!streamer_.profileSave(nm, err)) return "{\"error\":\"" + jesc(err) + "\"}";
        return profilesList();
    }
    if (!ld.empty()) {
        if (!parseInt(ld, 0, UdpStreamer::MAX_PROFILES - 1, idx) || !streamer_.profileLoad((int)idx)) return "{\"error\":\"perfil inexistente\"}";
        return profilesList();
    }
    if (!dl.empty()) {
        if (!parseInt(dl, 0, UdpStreamer::MAX_PROFILES - 1, idx) || !streamer_.profileDelete((int)idx)) return "{\"error\":\"perfil inexistente\"}";
        return profilesList();
    }
    return BAD;
}
std::string WebServer::setBass(const std::string& b) {
    double hz;
    if (!parseFloat(jsonGet(b, "xover_hz"), XOVER_MIN_HZ, XOVER_MAX_HZ, hz) || !streamer_.setXoverHz((float)hz))
        return "{\"error\":\"xover_hz fuera de rango (" + std::to_string((int)XOVER_MIN_HZ) + ".." + std::to_string((int)XOVER_MAX_HZ) + ")\"}";
    return "{\"ok\":true}";
}
// {"id":N,"eq0":dB,...,"eq5":dB}  (las bandas ausentes conservan su valor)  |  {"id":N,"reset":true}
std::string WebServer::setEq(const std::string& b) {
    static const char* BAD = "{\"error\":\"invalid parameters\"}";
    long id;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return BAD;
    float g[EQ_BANDS];
    const ESPNode cur = streamer_.getNodes()[id];
    for (int i = 0; i < EQ_BANDS; i++) g[i] = cur.eq_db[i];
    std::string r = jsonGet(b, "reset");
    if (r == "true" || r == "1") for (int i = 0; i < EQ_BANDS; i++) g[i] = 0.0f;
    for (int i = 0; i < EQ_BANDS; i++) {
        std::string v = jsonGet(b, "eq" + std::to_string(i));
        double d;
        if (v.empty()) continue;
        if (!parseFloat(v, -EQ_MAX_DB, EQ_MAX_DB, d)) return "{\"error\":\"eq fuera de rango (±" + std::to_string((int)EQ_MAX_DB) + " dB)\"}";
        g[i] = (float)d;
    }
    if (!streamer_.setNodeEq((uint8_t)id, g)) return BAD;
    return "{\"ok\":true}";
}
// Para el reproductor de vídeo: cuánto hay que ADELANTAR el audio. mpv: --audio-delay=<mpv_audio_delay_s> (negativo).
std::string WebServer::getLatency() {
    auto ap = streamer_.autoPlayout();
    double playout = streamer_.playoutUs() / 1000.0, extra = streamer_.lipsyncExtraMs();
    double period = capture_.config().period_size * 1000.0 / AUDIO_SAMPLE_RATE;
    double total = playout + extra;
    std::ostringstream o;
    o << "{" << jnum("playout_ms", playout) << "," << jnum("extra_ms", extra) << "," << jnum("period_ms", period) << ","
      << jnum("total_ms", total) << "," << jnum("audio_delay_ms", -total) << "," << jnum("mpv_audio_delay_s", -total / 1000.0) << ","
      << "\"auto\":{" << jbool("on", ap.on) << "," << jnum("floor_ms", ap.floor_us / 1000.0) << "," << jnum("max_ms", ap.max_us / 1000.0) << ","
      << jnum("changes", ap.changes) << "," << jnum("last_change_ago_s", ap.last_change_ago_ms < 0 ? -1.0 : ap.last_change_ago_ms / 1000.0) << ","
      << jnum("last_dir", ap.last_dir) << "," << jnum("recent_events", ap.recent_events) << ","
      << jnum("raise_events", ap.raise_events) << "," << jnum("clean_min", ap.clean_min) << "," << jnum("step_ms", ap.step_us / 1000.0) << "}}";
    return o.str();
}
std::string WebServer::setLatency(const std::string& b) {
    long ms;
    if (!parseInt(jsonGet(b, "extra_ms"), -50, 200, ms) || !streamer_.setLipsyncExtraMs((int)ms))
        return "{\"error\":\"extra_ms fuera de rango (-50..200)\"}";
    return "{\"ok\":true}";
}
std::string WebServer::setPlayout(const std::string& b) {
    long ms;
    if (!parseInt(jsonGet(b, "ms"), PLAYOUT_MIN_US / 1000, PLAYOUT_MAX_US / 1000, ms))
        return "{\"error\":\"ms fuera de rango (" + std::to_string(PLAYOUT_MIN_US / 1000) + ".." + std::to_string(PLAYOUT_MAX_US / 1000) + ")\"}";
    if (!streamer_.setPlayoutManual((uint32_t)ms * 1000)) return "{\"error\":\"no se pudo aplicar\"}";
    return "{\"ok\":true}";
}
// Latencia AUTO: {"auto":true|false,"max_ms":N}. El mínimo es el margen que había al activarlo (o el último que se fijó a mano).
std::string WebServer::setPlayoutAuto(const std::string& b) {
    std::string a = jsonGet(b, "auto");
    if (a != "true" && a != "false") return "{\"error\":\"auto debe ser true o false\"}";
    long mx = -1;
    std::string m = jsonGet(b, "max_ms");
    if (!m.empty() && !parseInt(m, PLAYOUT_MIN_US / 1000, PLAYOUT_MAX_US / 1000, mx))
        return "{\"error\":\"max_ms fuera de rango (" + std::to_string(PLAYOUT_MIN_US / 1000) + ".." + std::to_string(PLAYOUT_MAX_US / 1000) + ")\"}";
    long re = -1, cm = -1, st = -1;
    if (!jsonGet(b, "raise_events").empty() && !parseInt(jsonGet(b, "raise_events"), 1, 100, re)) return "{\"error\":\"raise_events fuera de rango (1..100)\"}";
    if (!jsonGet(b, "clean_min").empty() && !parseInt(jsonGet(b, "clean_min"), 1, 120, cm)) return "{\"error\":\"clean_min fuera de rango (1..120)\"}";
    if (!jsonGet(b, "step_ms").empty() && (!parseInt(jsonGet(b, "step_ms"), 5, 40, st) || st % 5)) return "{\"error\":\"step_ms fuera de rango (5..40, de 5 en 5)\"}";
    if (!streamer_.setAutoThresholds((int)re, (int)cm, (int)st)) return "{\"error\":\"umbrales no válidos\"}";
    if (!streamer_.setAutoPlayout(a == "true", mx < 0 ? -1 : mx * 1000)) return "{\"error\":\"no se pudo aplicar\"}";
    return "{\"ok\":true}";
}
std::string WebServer::setVolume(const std::string& b) {
    double v;
    if (!parseFloat(jsonGet(b, "volume"), 0.0, 2.0, v)) return "{\"error\":\"invalid volume\"}";
    streamer_.setMasterVolume((float)v);
    return "{\"ok\":true}";
}
std::string WebServer::calStart(const std::string& b) {
    double lvl = -12.0, rounds = 2;
    std::string s;
    if (!(s = jsonGet(b, "level_db")).empty() && !parseFloat(s, -40.0, -3.0, lvl))  return "{\"error\":\"level_db fuera de rango (-40..-3)\"}";
    if (!(s = jsonGet(b, "rounds")).empty()   && !parseFloat(s, 1.0, 3.0, rounds)) return "{\"error\":\"rounds fuera de rango (1..3)\"}";
    if (streamer_.calRunning()) return "{\"error\":\"calibración en curso\"}";
    std::vector<uint8_t> ids, seq;
    for (const auto& n : streamer_.getNodes()) if (n.online && n.enabled) ids.push_back(n.id);
    if (ids.empty()) return "{\"error\":\"ningún nodo online y habilitado\"}";
    for (int r = 0; r < (int)rounds; r++) for (uint8_t id : ids) seq.push_back(id);
    if (!streamer_.calStart(seq, (float)lvl)) return "{\"error\":\"no se pudo iniciar\"}";
    std::ostringstream o;
    o << "{\"ok\":true,\"sequence\":[";
    for (size_t i = 0; i < seq.size(); i++) o << (i ? "," : "") << (int)seq[i];
    int64_t total = CAL_LEAD_US + (int64_t)seq.size() * CAL_SLOT_US + CAL_TAIL_US;
    o << "]," << jnum("slot_ms", CAL_SLOT_US / 1000.0) << "," << jnum("chirp_ms", CAL_CHIRP_US / 1000.0) << ","
      << jnum("lead_ms", CAL_LEAD_US / 1000.0) << "," << jnum("fade_ms", CAL_FADE_US / 1000.0) << ","
      << jnum("f0", CAL_F0_HZ) << "," << jnum("f1", CAL_F1_HZ) << ",\"eq_edges\":[";
    for (int i = 0; i <= EQ_BANDS; i++) o << (i ? "," : "") << EQ_EDGES_HZ[i];
    o << "],"
      << jnum("total_ms", total / 1000.0) << "}";
    return o.str();
}
std::string WebServer::setCalibration(const std::string& b) {
    long id, lat_us;
    if (!parseInt(jsonGet(b, "esp_id"), 0, MAX_NODES - 1, id)) return "{\"error\":\"invalid parameters\"}";
    std::string sus = jsonGet(b, "latency_us");
    if (!sus.empty()) {
        if (!parseInt(sus, -500000, 500000, lat_us)) return "{\"error\":\"invalid latency_us\"}";
    } else {
        double ms;
        if (!parseFloat(jsonGet(b, "latency_ms"), -500.0, 500.0, ms)) return "{\"error\":\"invalid latency\"}";
        lat_us = lrint(ms * 1000.0);
    }
    streamer_.setLatencyOffset((uint8_t)id, (int32_t)lat_us);
    return "{\"ok\":true}";
}
