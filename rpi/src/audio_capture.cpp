#include "audio_capture.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <pthread.h>
#include <time.h>
#include <errno.h>

static uint64_t now_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

static bool icontains(const char* hay, const char* needle) {
    size_t n = strlen(needle);
    for (; *hay; ++hay) {
        size_t i = 0;
        while (i < n && hay[i] &&
               tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i])) ++i;
        if (i == n) return true;
    }
    return false;
}

std::string AudioCapture::findCard(const char* needle) {
    int card = -1;
    while (snd_card_next(&card) >= 0 && card >= 0) {
        char* name = nullptr;
        if (snd_card_get_name(card, &name) == 0 && name) {
            bool hit = icontains(name, needle);
            free(name);
            if (hit) return "hw:" + std::to_string(card) + ",0";
        }
    }
    return "";
}

AudioCapture::AudioCapture(const AudioConfig& cfg) : cfg_(cfg) {}
AudioCapture::~AudioCapture() { stop(); close(); }

bool AudioCapture::open() {
    int err;
    if (cfg_.device == "auto") {
        std::string d = findCard("traktor");
        if (d.empty()) {
            fprintf(stderr, "[ALSA] No se encontró ninguna tarjeta 'Traktor' (arecord -l)\n");
            return false;
        }
        cfg_.device = d;
    }
    snd_pcm_hw_params_t* hw; snd_pcm_sw_params_t* sw;

    if ((err = snd_pcm_open(&pcm_, cfg_.device.c_str(),
                             SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "[ALSA] Cannot open %s: %s (¿PipeWire/PulseAudio ocupa la tarjeta?)\n",
                cfg_.device.c_str(), snd_strerror(err));
        pcm_ = nullptr;
        return false;
    }
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm_, hw);
    snd_pcm_hw_params_set_access(pcm_, hw, SND_PCM_ACCESS_RW_INTERLEAVED);

    if ((err = snd_pcm_hw_params_set_format(pcm_, hw, SND_PCM_FORMAT_S32_LE)) < 0) {
        fprintf(stderr, "[ALSA] S32_LE not supported: %s\n", snd_strerror(err));
        return false;
    }
    if ((err = snd_pcm_hw_params_set_channels(pcm_, hw, cfg_.channels)) < 0) {
        fprintf(stderr, "[ALSA] Cannot set %uch: %s\n", cfg_.channels, snd_strerror(err));
        return false;
    }
    unsigned int rate = cfg_.sample_rate;
    snd_pcm_hw_params_set_rate_near(pcm_, hw, &rate, nullptr);
    if (rate != cfg_.sample_rate) {
        fprintf(stderr, "[ALSA] Rate pedido %u, obtenido %u — el protocolo asume %u\n",
                cfg_.sample_rate, rate, cfg_.sample_rate);
        return false;
    }

    snd_pcm_uframes_t period = cfg_.period_size ? cfg_.period_size : cfg_.sample_rate / 200;
    snd_pcm_uframes_t buf    = cfg_.buffer_size ? cfg_.buffer_size : period * 8;
    int dir = 0;
    snd_pcm_hw_params_set_period_size_near(pcm_, hw, &period, &dir);
    snd_pcm_hw_params_set_buffer_size_near(pcm_, hw, &buf);

    if ((err = snd_pcm_hw_params(pcm_, hw)) < 0) {
        fprintf(stderr, "[ALSA] hw_params: %s\n", snd_strerror(err));
        return false;
    }
    snd_pcm_hw_params_get_period_size(hw, &period, &dir);
    snd_pcm_hw_params_get_buffer_size(hw, &buf);
    cfg_.period_size = (unsigned)period;
    cfg_.buffer_size = (unsigned)buf;

    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(pcm_, sw);
    snd_pcm_sw_params_set_start_threshold(pcm_, sw, 1);
    snd_pcm_sw_params(pcm_, sw);

    buf_.resize((size_t)cfg_.period_size * cfg_.channels);
    fprintf(stderr, "[ALSA] %s %uHz %uch S32_LE period=%u(%.1fms) buffer=%u\n",
            cfg_.device.c_str(), cfg_.sample_rate, cfg_.channels,
            cfg_.period_size, cfg_.period_size * 1000.0 / cfg_.sample_rate,
            cfg_.buffer_size);
    return true;
}

void AudioCapture::close() {
    if (pcm_) { snd_pcm_close(pcm_); pcm_ = nullptr; }
}

bool AudioCapture::start() {
    if (!pcm_ || running_) return false;
    running_ = true;
    thread_ = std::thread(&AudioCapture::captureLoop, this);
    return true;
}

void AudioCapture::stop() {
    running_ = false;
    if (pcm_) snd_pcm_drop(pcm_);             // desbloquea readi()
    if (thread_.joinable()) thread_.join();   // y SOLO después cerramos (close())
}

void AudioCapture::captureLoop() {
    // Prioridad RT aplicada desde el propio hilo (sin carrera)
    sched_param sp{}; sp.sched_priority = 80;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp))
        fprintf(stderr, "[ALSA] Sin prioridad RT — falta CAP_SYS_NICE / LimitRTPRIO\n");
    else
        fprintf(stderr, "[ALSA] RT SCHED_FIFO prio=80\n");

    snd_pcm_prepare(pcm_);
    snd_pcm_start(pcm_);
    uint32_t overruns = 0;
    while (running_) {
        snd_pcm_sframes_t n = snd_pcm_readi(pcm_, buf_.data(), cfg_.period_size);
        if (n < 0) {
            if (!running_) break;
            if (n == -ENODEV) { fprintf(stderr, "[ALSA] Dispositivo desconectado\n"); break; }
            if (n == -EPIPE && ++overruns % 20 == 1)
                fprintf(stderr, "[ALSA] Overrun #%u\n", overruns);
            if (snd_pcm_recover(pcm_, (int)n, 1) < 0) break;
            snd_pcm_start(pcm_);
            continue;
        }
        if (n == 0) continue;
        uint64_t ts = now_us() - (uint64_t)n * 1000000ULL / cfg_.sample_rate;
        if (callback_) callback_(buf_.data(), (uint32_t)n, ts);
    }
    running_ = false;     // si salimos por error, isRunning() pasa a false (heartbeat lo avisa)
    fprintf(stderr, "[ALSA] Capture stopped. Overruns: %u\n", overruns);
}
