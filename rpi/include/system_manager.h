#pragma once
// Gestión del propio sistema de la RPi desde la web: red WiFi de la Raspberry (con vuelta atrás si falla) y
// actualización del servidor (git + compilar + reiniciar) delegada en un servicio systemd de root (sonos_wifi_update).
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

class SystemManager {
public:
    ~SystemManager();
    void configure(const std::string& state_dir);      // carpeta con update.state / update.log (p. ej. /var/lib/sonos_wifi)
    struct Wifi { std::string ssid, iface, state, msg, target; };   // state: idle | connecting | ok | failed
    bool wifiNow(Wifi& w);                              // red actual (nmcli) + estado del último cambio
    bool wifiConnect(const std::string& ssid, const std::string& pass, std::string& err);   // asíncrono; vuelve a la red anterior si falla
    struct Update { std::string state, log; };          // state: idle | running | ok | failed
    Update updateStatus();
    bool updateStart(std::string& err);
    static const char* version();
private:
    void wifiWorker(std::string ssid, std::string pass);
    std::string dir_;
    std::mutex m_;
    Wifi last_;
    std::thread th_;
    std::atomic<bool> busy_{false};
};
