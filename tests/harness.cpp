// Harness de pruebas (sin ALSA ni hardware): alimenta UdpStreamer con audio sintético
// y levanta la web UI. Lo usa tests/fake_esp.py.   Build: ver tests/run_tests.sh
#include "udp_streamer.h"
#include "audio_capture.h"
#include "web_server.h"
#include <thread>
#include <chrono>
#include <vector>
#include <time.h>
#include <cstdio>
#include <string>
#include <cmath>
#include <cstdlib>

static uint64_t now_us() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

int main(int argc, char** argv) {
    // Modo "auth": solo la web con contraseña (no hay audio) — lo usa test_auth.py
    if (argc > 1 && std::string(argv[1]) == "auth") {
        UdpStreamer s; AudioConfig c; AudioCapture cap(c);
        WebServer w(s, cap, 18081); w.setPassword("s3cret:clave");
        if (!w.start()) return 1;
        std::this_thread::sleep_for(std::chrono::seconds(6));
        w.stop(); return 0;
    }
    UdpStreamer s;
    s.setConfigPath("/tmp/sonos_harness_config.txt");
    AudioConfig cfg;
    AudioCapture cap(cfg);                // NO se abre: solo para config() en /api/status
    WebServer web(s, cap, 18080);
    if (const char* h = getenv("SONOS_BLE_HELPER")) web.setBle(h, "/tmp/sonos_harness_ble.txt");   // Bluetooth simulado (SONOS_BLE_MOCK)
    web.setTLS(18443, "/tmp/sonos_test_cert.pem", "/tmp/sonos_test_key.pem");
    if (!s.init() || !web.start()) return 1;

    const uint32_t P = 240;
    std::vector<int32_t> buf((size_t)P * AUDIO_CHANNELS);
    for (uint32_t f = 0; f < P; f++) {
        int32_t* fr = &buf[(size_t)f * AUDIO_CHANNELS];
        for (int c = 0; c < AUDIO_CHANNELS; c++) fr[c] = 0;
        fr[0] =  0x40000000;              // -> s16  16384
        fr[1] = -0x40000000;              // -> s16 -16384
        fr[2] =  0x20000000;              // -> s16   8192
    }
    uint64_t tone_n = 0;                  // canal 4: seno de 1414 Hz, amplitud 8192 (s16), fase continua entre bloques
    uint64_t t0 = now_us();
    uint64_t next = t0;
    bool cfg_done = false;
    while (now_us() - t0 < 150000000ULL) {
        next += 5000;
        for (uint32_t f = 0; f < P; f++, tone_n++)
            buf[(size_t)f * AUDIO_CHANNELS + 4] = (int32_t)(std::sin(2.0 * M_PI * 1414.0 * (double)tone_n / 48000.0) * 8192.0) << 16;
        s.pushFrames(buf.data(), P, next - 5000);   // ts = inicio del bloque
        if (!cfg_done && now_us() - t0 > 3000000ULL) {
            s.setNodeConfig(0, 2, 1, 128, 255, true);
            cfg_done = true;
        }
        uint64_t n = now_us();
        if (next > n) std::this_thread::sleep_for(std::chrono::microseconds(next - n));
    }
    web.stop(); s.shutdown();
    return 0;
}
