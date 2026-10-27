#pragma once
#include "udp_streamer.h"
#include "audio_capture.h"
#include "ble_manager.h"
#include <thread>
#include <atomic>
#include <string>

class WebServer {
public:
    WebServer(UdpStreamer& s, AudioCapture& c, uint16_t port = 8080);
    ~WebServer();
    void setTLS(uint16_t port, const std::string& cert, const std::string& key);   // antes de start()
    void setBle(const std::string& helper, const std::string& settings_file) { ble_.configure(helper, settings_file); }
    void setPassword(const std::string& pw) { password_ = pw; }   // vacío = sin autenticación
    bool start();
    void stop();
private:
    void serverLoop();
    void handleClient(int cfd, bool tls);
    std::string handleGET(const std::string& path, int& code);
    std::string handlePOST(const std::string& path, const std::string& body, int& code);
    std::string getStatus();
    std::string getNodes();
    std::string setNode(const std::string& body);
    std::string setVolume(const std::string& body);
    std::string setCalibration(const std::string& body);
    std::string setPlayout(const std::string& body);
    std::string setLayout(const std::string& body);
    std::string setBass(const std::string& body);
    std::string setDsp(const std::string& body);
    std::string setEq(const std::string& body);
    std::string getLatency();
    std::string setLatency(const std::string& body);
    std::string calStart(const std::string& body);
    std::string bleStatus();
    std::string bleWifi(const std::string& body);
    std::string bleScan(const std::string& body);
    std::string bleProvision(const std::string& body);
    std::string bleIdentify(const std::string& body);

    BleManager        ble_;
    UdpStreamer&      streamer_;
    AudioCapture&     capture_;
    uint16_t          port_;
    int               sfd_ = -1;
    int               tfd_ = -1;
    uint16_t          tls_port_ = 0;
    std::string       cert_, key_, password_;
    void*             ssl_ctx_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<int>  active_{0};
    std::thread       thread_;
};
