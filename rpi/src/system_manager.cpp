#include "system_manager.h"
#include "ble_manager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <fstream>
#include <sstream>

#ifndef SONOS_GIT
#define SONOS_GIT "desconocida"
#endif

static bool hasCtl(const std::string& s) { for (unsigned char c : s) if (c < 0x20 || c == 0x7f) return true; return false; }
static std::string nmcliPath() { const char* e = getenv("SONOS_NMCLI"); return (e && *e) ? e : "nmcli"; }
static bool exitOk(int st) { return WIFEXITED(st) && WEXITSTATUS(st) == 0; }
static std::string lastLines(const std::string& t, int n, size_t cap) {
    size_t pos = t.size(); int c = 0;
    while (pos > 0 && c <= n) { pos = t.rfind('\n', pos - 1); if (pos == std::string::npos) { pos = 0; break; } c++; }
    std::string o = t.substr(pos == 0 ? 0 : pos + 1);
    if (o.size() > cap) o = o.substr(o.size() - cap);
    return o;
}

const char* SystemManager::version() { return SONOS_GIT; }
SystemManager::~SystemManager() { if (th_.joinable()) th_.join(); }
void SystemManager::configure(const std::string& state_dir) { dir_ = state_dir; last_.state = "idle"; }

// Interfaz WiFi y conexión activa en ella.
static bool wifiIface(std::string& iface, std::string& conn) {
    std::string so; int st = 0;
    iface.clear(); conn.clear();
    if (!execCapture({nmcliPath(), "-t", "-f", "DEVICE,TYPE", "dev"}, 10, so, st) || !exitOk(st)) return false;
    size_t pos = 0;
    while (pos < so.size()) {
        size_t nl = so.find('\n', pos); if (nl == std::string::npos) nl = so.size();
        auto f = splitNmcli(so.substr(pos, nl - pos)); pos = nl + 1;
        if (f.size() >= 2 && f[1] == "wifi" && !f[0].empty()) { iface = f[0]; break; }
    }
    if (iface.empty()) return false;
    so.clear();
    if (execCapture({nmcliPath(), "-t", "-f", "NAME,TYPE,DEVICE", "con", "show", "--active"}, 10, so, st) && exitOk(st)) {
        pos = 0;
        while (pos < so.size()) {
            size_t nl = so.find('\n', pos); if (nl == std::string::npos) nl = so.size();
            auto f = splitNmcli(so.substr(pos, nl - pos)); pos = nl + 1;
            if (f.size() >= 3 && f[2] == iface && f[1].find("wireless") != std::string::npos) { conn = f[0]; break; }
        }
    }
    return true;
}

bool SystemManager::wifiNow(Wifi& w) {
    { std::lock_guard<std::mutex> l(m_); w = last_; }
    std::string iface, conn;
    if (!wifiIface(iface, conn)) { w.iface.clear(); w.ssid.clear(); return false; }
    w.iface = iface; w.ssid = conn;
    return true;
}

bool SystemManager::wifiConnect(const std::string& ssid, const std::string& pass, std::string& err) {
    if (ssid.empty() || ssid.size() > 32 || hasCtl(ssid)) { err = "El nombre de la red debe tener entre 1 y 32 caracteres"; return false; }
    if (ssid[0] == '-') { err = "El nombre de la red no puede empezar por «-»"; return false; }
    if (!pass.empty() && (pass.size() < 8 || pass.size() > 63 || hasCtl(pass))) { err = "La contraseña debe tener entre 8 y 63 caracteres"; return false; }
    bool exp = false;
    if (!busy_.compare_exchange_strong(exp, true)) { err = "Ya hay un cambio de red en curso"; return false; }
    if (th_.joinable()) th_.join();
    { std::lock_guard<std::mutex> l(m_); last_.state = "connecting"; last_.target = ssid; last_.msg = "Conectando…"; }
    th_ = std::thread([this, ssid, pass] { wifiWorker(ssid, pass); });
    return true;
}

void SystemManager::wifiWorker(std::string ssid, std::string pass) {
    auto fin = [&](const char* state, const std::string& msg) {
        { std::lock_guard<std::mutex> l(m_); last_.state = state; last_.msg = msg; }
        busy_ = false;
    };
    std::string iface, prev, so; int st = 0;
    if (!wifiIface(iface, prev)) { fin("failed", "La Raspberry no tiene WiFi (o falta NetworkManager)"); return; }
    std::vector<std::string> a = {nmcliPath(), "dev", "wifi", "connect", ssid};
    if (!pass.empty()) { a.push_back("password"); a.push_back(pass); }
    a.push_back("ifname"); a.push_back(iface);
    bool ran = execCapture(a, 60, so, st);
    if (ran && exitOk(st)) { fin("ok", "Conectada a «" + ssid + "»"); return; }
    std::string back;
    if (!prev.empty()) {
        std::string s2; int st2 = 0;
        if (execCapture({nmcliPath(), "con", "up", "id", prev}, 40, s2, st2) && exitOk(st2)) back = " Se volvió a «" + prev + "».";
        else back = " No se pudo volver a «" + prev + "»: revisa la Raspberry.";
    }
    fin("failed", std::string(ran ? "No se pudo conectar a «" : "Tardó demasiado en conectar a «") + ssid + "» (¿contraseña incorrecta o fuera de alcance?)." + back);
}

SystemManager::Update SystemManager::updateStatus() {
    Update u; u.state = "idle";
    if (dir_.empty()) return u;
    std::ifstream f(dir_ + "/update.state");
    std::string line;
    if (f && std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line == "running" || line == "ok" || line == "failed") u.state = line;
    }
    std::ifstream lg(dir_ + "/update.log", std::ios::binary);
    if (lg) {
        std::ostringstream ss; ss << lg.rdbuf();
        std::string t = ss.str();
        if (t.size() > 65536) t = t.substr(t.size() - 65536);
        u.log = lastLines(t, 40, 6000);
        for (auto& c : u.log) if ((unsigned char)c < 0x20 && c != '\n' && c != '\t') c = ' ';
    }
    return u;
}

bool SystemManager::updateStart(std::string& err) {
    if (updateStatus().state == "running") { err = "Ya hay una actualización en curso"; return false; }
    // Solo se puede lanzar el servicio fijo de root «sonos_wifi_update»; no se ejecuta nada que venga de la web.
    const char* e = getenv("SONOS_UPDATE_CMD");
    std::vector<std::string> a;
    if (e && *e) a = {e};
    else a = {"sudo", "-n", "/usr/bin/systemctl", "start", "--no-block", "sonos_wifi_update.service"};
    std::string so; int st = 0;
    if (!execCapture(a, 20, so, st) || !exitOk(st)) {
        err = "No se pudo lanzar la actualización. Ejecuta una vez «sudo ./scripts/build.sh» en la Raspberry para instalarla.";
        return false;
    }
    return true;
}
