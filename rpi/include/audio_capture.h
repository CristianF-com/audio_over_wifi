#pragma once
#include <alsa/asoundlib.h>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <cstdint>

struct AudioConfig {
    std::string      device      = "auto";   // "auto" = buscar tarjeta Traktor
    unsigned int     sample_rate = 48000;
    unsigned int     channels    = 10;
    unsigned int     period_size = 240;      // frames (5 ms); 0 = por defecto
    unsigned int     buffer_size = 0;        // 0 = period*8
};

class AudioCapture {
public:
    explicit AudioCapture(const AudioConfig& cfg);
    ~AudioCapture();
    bool open();
    void close();
    bool start();
    void stop();

    // S32_LE interleaved N canales. timestamp = instante (CLOCK_MONOTONIC_RAW, µs)
    // del primer frame del bloque.
    using FrameCallback = std::function<void(const int32_t* data,
                                              uint32_t frames,
                                              uint64_t timestamp_us)>;
    void setCallback(FrameCallback cb) { callback_ = std::move(cb); }
    const AudioConfig& config() const { return cfg_; }
    bool isRunning() const { return running_.load(); }

    // Devuelve "hw:N,0" de la primera tarjeta cuyo nombre contiene `needle`
    // (sin distinguir mayúsculas), o "" si no hay.
    static std::string findCard(const char* needle);

private:
    void captureLoop();
    AudioConfig          cfg_;
    snd_pcm_t*           pcm_  = nullptr;
    std::atomic<bool>    running_{false};
    std::thread          thread_;
    FrameCallback        callback_;
    std::vector<int32_t> buf_;
};
