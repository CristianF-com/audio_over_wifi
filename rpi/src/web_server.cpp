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
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <sstream>
#include <cstdio>
#include <algorithm>

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

static bool readHTTP(Conn& c, std::string& method, std::string& path, std::string& body, std::string& auth) {
    const int fd = c.fd;
    std::string raw; raw.reserve(4096); char tmp[1024];
    struct timeval tv{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    while (true) {
        int n = (int)c.rd(tmp, sizeof(tmp)); if (n <= 0) return false;
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
    long clen = 0;
    size_t pp = hdrs.find("content-length:");
    if (pp != std::string::npos) {
        pp += 15;
        size_t ee = hdrs.find("\r\n", pp);
        std::string v = hdrs.substr(pp, ee == std::string::npos ? std::string::npos : ee - pp);
        while (!v.empty() && v.front() == ' ') v.erase(v.begin());
        if (!parseInt(v, 0, 16384, clen)) return false;
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
    std::string m, p, b, auth;
    if (!readHTTP(c, m, p, b, auth)) {
#ifdef WITH_TLS
        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
#endif
        ::close(cfd); return;
    }
    std::string resp, ct = "application/json", extra;
    int code = 200;
    bool authed = password_.empty();
    if (!authed && auth.size() > 6 && strncasecmp(auth.c_str(), "Basic ", 6) == 0) {
        std::string up = b64decode(auth.substr(6));
        size_t colon = up.find(':');
        authed = colon != std::string::npos && constEq(up.substr(colon + 1), password_);   // el usuario es libre
    }
    if (!authed) {
        code = 401; resp = "{\"error\":\"unauthorized\"}";
        extra = "WWW-Authenticate: Basic realm=\"SonosWiFi\", charset=\"UTF-8\"\r\n";
    }
    else if ((p == "/" || p == "/index.html") && m == "GET") { resp = PAGE_PANEL; ct = "text/html; charset=utf-8"; }
    else if (p == "/calibrate" && m == "GET") { resp = PAGE_CAL; ct = "text/html; charset=utf-8"; }
    else if (p == "/ble"       && m == "GET") { resp = PAGE_BLE; ct = "text/html; charset=utf-8"; }
    else if (m == "GET")  resp = handleGET(p, code);
    else if (m == "POST") resp = handlePOST(p, b, code);
    else { code = 405; resp = "{\"error\":\"method not allowed\"}"; }
    const char* st = code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 401 ? "Unauthorized" :
                     code == 404 ? "Not Found" : code == 409 ? "Conflict" : "Error";
    std::ostringstream r;
    r << "HTTP/1.1 " << code << " " << st << "\r\nContent-Type: " << ct << "\r\n"
      << extra << "Cache-Control: no-store\r\n"
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
std::string WebServer::handleGET(const std::string& p, int& code) {
    if (p == "/api/status") return getStatus();
    if (p == "/api/cal/status") {
        std::ostringstream o;
        o << "{" << jbool("running", streamer_.calRunning()) << ","
          << jnum("remaining_ms", (double)streamer_.calRemainingMs()) << "}";
        return o.str();
    }
    if (p == "/api/nodes")  return getNodes();
    if (p == "/api/latency") return getLatency();
    if (p == "/api/ble/status") return bleStatus();
    code = 404; return "{\"error\":\"not found\"}";
}
std::string WebServer::handlePOST(const std::string& p, const std::string& b, int& code) {
    std::string r;
    if      (p == "/api/node")      r = setNode(b);
    else if (p == "/api/volume")    r = setVolume(b);
    else if (p == "/api/playout")   r = setPlayout(b);
    else if (p == "/api/layout")    r = setLayout(b);
    else if (p == "/api/bass")      r = setBass(b);
    else if (p == "/api/dsp")       r = setDsp(b);
    else if (p == "/api/eq")        r = setEq(b);
    else if (p == "/api/latency")   r = setLatency(b);
    else if (p == "/api/calibrate") r = setCalibration(b);
    else if (p == "/api/ble/wifi")      r = bleWifi(b);
    else if (p == "/api/ble/scan")      r = bleScan(b);
    else if (p == "/api/ble/provision") r = bleProvision(b);
    else if (p == "/api/ble/identify")  r = bleIdentify(b);
    else if (p == "/api/cal/start") { r = calStart(b); if (r.find("\"error\"") != std::string::npos) { code = 409; return r; } }
    else if (p == "/api/cal/stop")  { streamer_.calStop(); r = "{\"ok\":true}"; }
    else { code = 404; return "{\"error\":\"not found\"}"; }
    // /api/ble/*: la respuesta del ayudante puede traer «"error": null» dentro de la lista de dispositivos; solo cuenta si empieza por {"error"
    const bool is_err = p.rfind("/api/ble/", 0) == 0 ? r.rfind("{\"error\"", 0) == 0 : r.find("\"error\"") != std::string::npos;
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
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return "{\"error\":\"id fuera de rango (0-9)\"}";
    auto st = ble_.settings();
    if (st.ssid.empty()) return "{\"error\":\"Primero guarda el nombre y la contraseña de la WiFi\"}";
    if (jsonGet(b, "force") != "true")
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
std::string WebServer::getStatus() {
    const auto& c = capture_.config();
    std::ostringstream o;
    o << "{" << jstr("device", c.device) << ","
      << jnum("sample_rate", c.sample_rate) << ","
      << jnum("channels", c.channels) << ","
      << jnum("period_frames", c.period_size) << ","
      << jnum("playout_ms", streamer_.playoutUs() / 1000.0) << ","
      << jnum("playout_min_ms", PLAYOUT_MIN_US / 1000.0) << "," << jnum("playout_max_ms", PLAYOUT_MAX_US / 1000.0) << ","
      << jnum("send_errors", (double)streamer_.sendErrors()) << ","
      << jnum("max_nodes", MAX_NODES) << "," << jnum("layout", streamer_.layout()) << ","
      << jnum("xover_hz", streamer_.xoverHz()) << ","
      << jbool("dsp_xover", streamer_.xoverOn()) << "," << jbool("dsp_eq", streamer_.eqOn()) << "," << jnum("lipsync_extra_ms", streamer_.lipsyncExtraMs()) << ","
      << jbool("running", capture_.isRunning()) << ","
      << jnum("master_volume", streamer_.masterVolume()) << "}";
    return o.str();
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
          << jnum("drift_ppm",    n.drift_ppm)    << ","
          << jnum("buffer_frames",n.buffer_frames) << ","
          << jnum("latency_offset_us", n.latency_offset_us) << ","
          << jnum("fill_pct", fill)
          << "}";
    }
    o << "]";
    return o.str();
}
std::string WebServer::setNode(const std::string& b) {
    static const char* BAD = "{\"error\":\"invalid parameters\"}";
    long id;
    if (!parseInt(jsonGet(b, "id"), 0, MAX_NODES - 1, id)) return BAD;
    auto nodes = streamer_.getNodes();
    const ESPNode& cur = nodes[id];
    long chl = cur.ch_left, chr = cur.ch_right, vl = cur.volume_l, vr = cur.volume_r;
    bool enabled = cur.enabled, mono = cur.mono;
    std::string s;
    if (!(s = jsonGet(b, "ch_left")).empty()  && !parseInt(s, 0, AUDIO_CHANNELS - 1, chl)) return BAD;
    if (!(s = jsonGet(b, "ch_right")).empty() && !parseInt(s, 0, AUDIO_CHANNELS - 1, chr)) return BAD;
    if (!(s = jsonGet(b, "volume_l")).empty() && !parseInt(s, 0, 255, vl)) return BAD;
    if (!(s = jsonGet(b, "volume_r")).empty() && !parseInt(s, 0, 255, vr)) return BAD;
    if (!(s = jsonGet(b, "enabled")).empty()) enabled = (s != "false" && s != "0");
    if (!(s = jsonGet(b, "mono")).empty())    mono    = (s != "false" && s != "0");
    long role = cur.role;
    if (!(s = jsonGet(b, "role")).empty() && !parseInt(s, 0, 2, role)) return BAD;
    if (!streamer_.setNodeConfig((uint8_t)id, (uint8_t)chl, (uint8_t)chr,
                                 (uint8_t)vl, (uint8_t)vr, enabled, mono)) return BAD;
    if (!streamer_.setNodeRole((uint8_t)id, (uint8_t)role)) return BAD;
    return "{\"ok\":true}";
}
std::string WebServer::setLayout(const std::string& b) {
    std::string l = jsonGet(b, "layout");
    int v = (l == "stereo5") ? LAYOUT_STEREO5 : (l == "mono10") ? LAYOUT_MONO10 : -1;
    if (v < 0 || !streamer_.setLayout(v)) return "{\"error\":\"layout debe ser stereo5 o mono10\"}";
    return "{\"ok\":true}";
}
// {"xover":true|false,"eq":true|false}  (cada clave es opcional). Ambos false = pass-through.
std::string WebServer::setDsp(const std::string& b) {
    auto flag = [&](const char* k, int& out) -> bool {          // out: -1 sin cambio, 0/1
        std::string v = jsonGet(b, k); out = -1;
        if (v.empty()) return true;
        if (v == "true" || v == "1") { out = 1; return true; }
        if (v == "false" || v == "0") { out = 0; return true; }
        return false;
    };
    int xo, eq;
    if (!flag("xover", xo) || !flag("eq", eq) || (xo < 0 && eq < 0)) return "{\"error\":\"usa xover y/o eq = true|false\"}";
    if (xo >= 0) streamer_.setXoverOn(xo == 1);
    if (eq >= 0) streamer_.setEqOn(eq == 1);
    return "{\"ok\":true}";
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
    double playout = streamer_.playoutUs() / 1000.0, extra = streamer_.lipsyncExtraMs();
    double period = capture_.config().period_size * 1000.0 / AUDIO_SAMPLE_RATE;
    double total = playout + extra;
    std::ostringstream o;
    o << "{" << jnum("playout_ms", playout) << "," << jnum("extra_ms", extra) << "," << jnum("period_ms", period) << ","
      << jnum("total_ms", total) << "," << jnum("audio_delay_ms", -total) << "," << jnum("mpv_audio_delay_s", -total / 1000.0) << "}";
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
    if (!streamer_.setPlayoutUs((uint32_t)ms * 1000)) return "{\"error\":\"no se pudo aplicar\"}";
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
