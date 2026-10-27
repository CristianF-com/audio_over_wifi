#pragma once
// Configuración de ESP32 por Bluetooth desde la propia RPi: guarda la WiFi de los altavoces (0600) y
// delega el Bluetooth en scripts/sonos_ble_prov.py (BlueZ/bleak) lanzado como subproceso.
#include <string>
#include <mutex>

class BleManager {
public:
    struct Settings { std::string ssid, pass, rpi_ip; };
    void configure(const std::string& helper, const std::string& settings_file);   // antes de usarlo
    bool available() const;                       // el ayudante existe y es legible
    Settings settings() const;
    // pass vacío = conservar la actual. false + err si no es válido.
    bool setSettings(const std::string& ssid, const std::string& pass, const std::string& rpi_ip, std::string& err);
    // Lanza «helper <cmd>» con stdin_json por stdin; devuelve la última línea JSON de su stdout (o {"error":...}).
    std::string run(const std::string& cmd, const std::string& stdin_json, int timeout_s);
    static bool validAddress(const std::string& a);
private:
    bool save();
    std::string helper_, file_;
    mutable std::mutex sm_;                       // protege settings_
    Settings settings_;
    std::mutex busy_;                             // una operación Bluetooth a la vez
};
