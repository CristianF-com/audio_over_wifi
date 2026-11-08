#include "ble_manager.h"
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <algorithm>

static std::string jerr(const std::string& m) {
    std::string o = "{\"error\":\"";
    for (unsigned char c : m) { if (c == '"' || c == '\\') { o += '\\'; o += (char)c; } else if (c < 0x20) o += ' '; else o += (char)c; }
    return o + "\"}";
}
static bool hasCtl(const std::string& s) { for (unsigned char c : s) if (c < 0x20 || c == 0x7f) return true; return false; }
static bool validIpv4(const std::string& s) {
    int parts = 0, val = 0, dig = 0;
    for (size_t i = 0; i <= s.size(); i++) {
        char c = i < s.size() ? s[i] : '.';
        if (c >= '0' && c <= '9') { val = val * 10 + (c - '0'); if (++dig > 3 || val > 255) return false; }
        else if (c == '.') { if (!dig) return false; parts++; val = 0; dig = 0; }
        else return false;
    }
    return parts == 4;
}

void BleManager::configure(const std::string& helper, const std::string& settings_file) {
    helper_ = helper; file_ = settings_file;
    FILE* f = file_.empty() ? nullptr : fopen(file_.c_str(), "r");
    if (!f) return;
    char line[256];
    std::lock_guard<std::mutex> l(sm_);
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        size_t e = s.find('='); if (e == std::string::npos) continue;
        std::string k = s.substr(0, e), v = s.substr(e + 1);
        if (k == "ssid") settings_.ssid = v; else if (k == "pass") settings_.pass = v; else if (k == "rpi_ip") settings_.rpi_ip = v;
    }
    fclose(f);
}
bool BleManager::available() const { return !helper_.empty() && access(helper_.c_str(), R_OK) == 0; }
BleManager::Settings BleManager::settings() const { std::lock_guard<std::mutex> l(sm_); return settings_; }

bool BleManager::setSettings(const std::string& ssid, const std::string& pass, const std::string& rpi_ip, std::string& err) {
    if (ssid.empty() || ssid.size() > 32 || hasCtl(ssid)) { err = "El nombre de la WiFi debe tener entre 1 y 32 caracteres"; return false; }
    if (!pass.empty() && (pass.size() < 8 || pass.size() > 63 || hasCtl(pass))) { err = "La contraseña debe tener entre 8 y 63 caracteres"; return false; }
    if (!rpi_ip.empty() && !validIpv4(rpi_ip)) { err = "La IP de la Raspberry no es válida"; return false; }
    {
        std::lock_guard<std::mutex> l(sm_);
        settings_.ssid = ssid; settings_.rpi_ip = rpi_ip;
        if (!pass.empty()) settings_.pass = pass;
    }
    if (!save()) { err = "No se pudo guardar el archivo de ajustes"; return false; }
    return true;
}
bool BleManager::save() {
    if (file_.empty()) return true;
    Settings s = settings();
    std::string tmp = file_ + ".tmp";
    int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return false;
    fchmod(fd, 0600);                              // la contraseña WiFi solo la lee el usuario del servicio
    std::string d = "ssid=" + s.ssid + "\npass=" + s.pass + "\nrpi_ip=" + s.rpi_ip + "\n";
    bool ok = write(fd, d.data(), d.size()) == (ssize_t)d.size();
    ok = (fsync(fd) == 0) && ok;
    close(fd);
    return ok && rename(tmp.c_str(), file_.c_str()) == 0;
}
bool BleManager::validAddress(const std::string& a) {
    if (a.size() < 17 || a.size() > 36) return false;
    for (char c : a) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == ':' || c == '-')) return false;
    return true;
}

static double monoS() { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }

std::string BleManager::run(const std::string& cmd, const std::string& stdin_json, int timeout_s) {
    if (!available()) return jerr("Falta el ayudante Bluetooth (sonos_ble_prov.py). Usa la página /ble desde el móvil.");
    std::unique_lock<std::mutex> lk(busy_, std::try_to_lock);
    if (!lk.owns_lock()) return jerr("Hay otra búsqueda o configuración Bluetooth en curso");
    int in[2], out[2], er[2];
    if (pipe(in) || pipe(out) || pipe(er)) return jerr("No se pudo crear el subproceso");
    pid_t pid = fork();
    if (pid < 0) { return jerr("No se pudo lanzar el ayudante"); }
    if (pid == 0) {
        dup2(in[0], 0); dup2(out[1], 1); dup2(er[1], 2);
        for (int fd : {in[0], in[1], out[0], out[1], er[0], er[1]}) close(fd);
        execlp("python3", "python3", "-u", helper_.c_str(), cmd.c_str(), (char*)nullptr);
        _exit(127);
    }
    close(in[0]); close(out[1]); close(er[1]);
    // stdin: se escribe y se cierra (el JSON es corto, cabe en el pipe)
    { size_t off = 0; while (off < stdin_json.size()) { ssize_t w = write(in[1], stdin_json.data() + off, stdin_json.size() - off); if (w <= 0) break; off += (size_t)w; } }
    close(in[1]);
    std::string so, se; bool timed_out = false;
    double t0 = monoS();
    struct pollfd p[2] = {{out[0], POLLIN, 0}, {er[0], POLLIN, 0}};
    int open_n = 2;
    while (open_n > 0) {
        double left = timeout_s - (monoS() - t0);
        if (left <= 0) { timed_out = true; break; }
        int r = poll(p, 2, (int)(left * 1000) + 1);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) continue;
        for (int i = 0; i < 2; i++) {
            if (p[i].fd < 0 || !(p[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char buf[4096]; ssize_t n = read(p[i].fd, buf, sizeof(buf));
            if (n > 0) { (i == 0 ? so : se).append(buf, (size_t)n); if (so.size() > (1u << 20) || se.size() > (1u << 20)) { timed_out = true; open_n = 0; } }
            else { p[i].fd = -1; open_n--; }
        }
    }
    if (timed_out) kill(pid, SIGKILL);
    close(out[0]); close(er[0]);
    int st = 0; waitpid(pid, &st, 0);
    if (timed_out) return jerr("El Bluetooth tardó demasiado en responder");
    // última línea no vacía que parezca JSON
    size_t e = so.find_last_not_of("\r\n \t");
    if (e != std::string::npos) {
        size_t b = so.rfind('\n', e); b = (b == std::string::npos) ? 0 : b + 1;
        std::string last = so.substr(b, e - b + 1);
        if (!last.empty() && last[0] == '{') return last;
    }
    if (!se.empty()) { fprintf(stderr, "[BLE] ayudante: %s\n", se.substr(0, 400).c_str()); }
    std::string tail = se; size_t le = tail.find_last_not_of("\r\n \t");
    if (le != std::string::npos) { size_t lb = tail.rfind('\n', le); tail = tail.substr(lb == std::string::npos ? 0 : lb + 1, le - (lb == std::string::npos ? 0 : lb + 1) + 1); }
    return jerr(tail.empty() ? "El ayudante Bluetooth no respondió" : "El ayudante Bluetooth falló: " + tail.substr(0, 200));
}

// ─── Redes WiFi visibles desde la RPi ─────────────────────────────────────────────
// Ejecuta un programa sin shell y devuelve su stdout. false si no se pudo lanzar o venció el tiempo.
bool execCapture(const std::vector<std::string>& argv, int timeout_s, std::string& so, int& status) {
    int out[2], er[2];
    if (pipe(out)) return false;
    if (pipe(er)) { close(out[0]); close(out[1]); return false; }
    pid_t pid = fork();
    if (pid < 0) { for (int fd : {out[0], out[1], er[0], er[1]}) close(fd); return false; }
    if (pid == 0) {
        int dn = open("/dev/null", O_RDONLY); if (dn >= 0) dup2(dn, 0);
        dup2(out[1], 1); dup2(er[1], 2);
        for (int fd : {out[0], out[1], er[0], er[1]}) close(fd);
        std::vector<char*> a; for (const auto& x : argv) a.push_back(const_cast<char*>(x.c_str())); a.push_back(nullptr);
        execvp(a[0], a.data());
        _exit(127);
    }
    close(out[1]); close(er[1]);
    bool timed_out = false; double t0 = monoS();
    struct pollfd p[2] = {{out[0], POLLIN, 0}, {er[0], POLLIN, 0}};
    int open_n = 2;
    while (open_n > 0) {
        double left = timeout_s - (monoS() - t0);
        if (left <= 0) { timed_out = true; break; }
        int r = poll(p, 2, (int)(left * 1000) + 1);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) continue;
        for (int i = 0; i < 2; i++) {
            if (p[i].fd < 0 || !(p[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char buf[4096]; ssize_t n = read(p[i].fd, buf, sizeof(buf));
            if (n > 0) { if (i == 0) { so.append(buf, (size_t)n); if (so.size() > (1u << 20)) { timed_out = true; open_n = 0; } } }
            else { p[i].fd = -1; open_n--; }
        }
    }
    if (timed_out) kill(pid, SIGKILL);
    close(out[0]); close(er[0]);
    waitpid(pid, &status, 0);
    return !timed_out;
}

void BleManager::parseNmcli(const std::string& text, std::vector<WifiNet>& out) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos); if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos); pos = nl + 1;
        std::vector<std::string> f = splitNmcli(line);
        if (f.size() < 4 || f[0].empty() || f[0].size() > 32 || hasCtl(f[0])) continue;   // ocultas o inválidas
        WifiNet n; n.ssid = f[0]; n.signal = atoi(f[1].c_str()); n.freq = atoi(f[2].c_str());
        n.security = (f[3] == "--") ? "" : f[3].substr(0, 40);
        bool dup = false;
        for (auto& e : out) if (e.ssid == n.ssid && (e.freq >= 4900) == (n.freq >= 4900)) { if (n.signal > e.signal) e = n; dup = true; break; }
        if (!dup) out.push_back(n);
    }
    // 5 GHz primero (mejor para el audio), luego por señal
    std::stable_sort(out.begin(), out.end(), [](const WifiNet& a, const WifiNet& b) {
        bool a5 = a.freq >= 4900, b5 = b.freq >= 4900;
        if (a5 != b5) return a5;
        return a.signal > b.signal;
    });
    if (out.size() > 40) out.resize(40);
}

bool BleManager::scanWifi(std::vector<WifiNet>& out, std::string& err) {
    std::unique_lock<std::mutex> lk(wifi_busy_, std::try_to_lock);
    if (!lk.owns_lock()) { err = "Ya hay una búsqueda de redes en curso"; return false; }
    const char* e = getenv("SONOS_NMCLI");
    std::string prog = (e && *e) ? e : "nmcli";
    std::string so; int st = 0;
    // primero obliga a re-escanear; si NetworkManager lo rechaza (escaneo reciente) se usa la lista que ya tiene
    for (const char* rescan : {"yes", "no"}) {
        so.clear(); st = 0;
        if (!execCapture({prog, "-t", "-f", "SSID,SIGNAL,FREQ,SECURITY", "dev", "wifi", "list", "--rescan", rescan}, 20, so, st)) {
            err = "La búsqueda de redes tardó demasiado"; return false;
        }
        if (WIFEXITED(st) && WEXITSTATUS(st) == 127) { err = "La Raspberry no tiene NetworkManager (nmcli). Escribe el nombre de la red a mano."; return false; }
        if (WIFEXITED(st) && WEXITSTATUS(st) == 0) break;
    }
    if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0)) { err = "No se pudo buscar redes: comprueba que la WiFi de la Raspberry está encendida"; return false; }
    out.clear(); parseNmcli(so, out);
    return true;
}

std::vector<std::string> splitNmcli(const std::string& line) {
    std::vector<std::string> f(1);
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '\\' && i + 1 < line.size()) f.back() += line[++i];
        else if (line[i] == ':') f.emplace_back();
        else f.back() += line[i];
    }
    return f;
}
