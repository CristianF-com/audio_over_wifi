#pragma once
// Entradas de audio: cualquier número de interfaces USB (la Traktor es una más), cada una con «Usar» sí/no,
// y un único reloj principal: una de las placas o la propia Raspberry. Las entradas se numeran en el orden de la lista;
// cada canal tiene un id fijo (letra de la placa + nº de canal en ella: A1, A2… B1…).
// Con UNA sola placa activa y siendo ella el reloj, main.cpp captura directo y llama a pushFrames (camino de siempre);
// en cualquier otro caso se usa el InputMixer: cada placa que no manda el reloj entra por un colchón elástico.
#include "audio_capture.h"
#include "udp_streamer.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct InputDev { std::string key; unsigned channels = 2; bool enabled = false; std::string tag; };   // key: parte del nombre de la tarjeta o «hw:N,0»
struct InputConfig {
    std::vector<InputDev> devs;                 // el orden es el de las entradas
    std::string clock;                          // "" = automático (la única placa activa); "rpi" = la Raspberry; o la key de una placa activa
    bool operator==(const InputConfig& o) const;
};
struct CardInfo { int index = 0; std::string id, name; int max_channels = 0; bool busy = false; };

class InputMixer {
public:
    InputMixer(UdpStreamer& s, unsigned block_frames);
    ~InputMixer();
    // ── Configuración (fichero <config>.inputs, formato «v2»; los de versiones anteriores se convierten al leerlos) ──
    static bool load(const std::string& path, InputConfig& c);                    // false si no existe
    static bool save(const std::string& path, const InputConfig& c);
    static std::string serialize(const InputConfig& c);
    static bool parseJson(const std::string& body, InputConfig& out, std::string& err);   // {"devices":[{"key","channels","enabled","tag"}],"clock":"…"}
    static bool validate(InputConfig& c, std::string& err);                      // asigna letras; max. 32 canales activos
    static std::vector<CardInfo> listCards();
    static int  matchCard(const std::string& key, const std::vector<CardInfo>& cards);   // índice en cards o -1
    static InputConfig defaultConfig(const std::vector<CardInfo>& cards);        // sin fichero: la Traktor, o la única tarjeta de captura
    static std::string effectiveClock(const InputConfig& c);                     // "rpi" o la key de la placa que manda
    static std::vector<std::string> idsFor(const std::string& tag, unsigned channels);
    static std::string resolve(const std::string& key);                          // «hw:N,0» o ""

    // ── Mezclador (varias placas o placa que no manda el reloj) ──
    void configure(const InputConfig& c);                                          // antes de start()
    bool start();
    void stop();
    bool active() const { return running_.load(); }
    int  totalChannels() const { return total_; }
    int  firstOf(size_t enabled_idx) const;
    std::string statusJson() const;
    // Pruebas (sin ALSA): alimenta la placa n-ésima de las activas
    void injectForTest(size_t idx, const int32_t* data, uint32_t frames, bool as_master = false, uint64_t ts = 0);
    void tickForTest(uint32_t frames, uint64_t ts) { mix(nullptr, nullptr, frames, ts); }

private:
    struct Dev {
        InputDev spec;
        int       first = 0;
        bool      master = false;
        std::string resolved, error;
        std::unique_ptr<AudioCapture> cap;
        mutable std::mutex mtx;
        std::vector<int32_t> ring;
        size_t    fill = 0;
        bool      primed = false;
        uint64_t  underruns = 0, dropped = 0, blocks = 0;
        std::atomic<bool> online{false};
        uint64_t  next_try_us = 0;
        bool      test = false;
    };
    void push(Dev& d, const int32_t* data, uint32_t frames);
    void pull(Dev& d, uint32_t frames);
    void mix(Dev* master, const int32_t* mdata, uint32_t frames, uint64_t ts);
    void onBlock(Dev& d, const int32_t* data, uint32_t frames, uint64_t ts);
    bool openDev(Dev& d);
    void setErr(Dev& d, const std::string& s);
    void watchLoop();
    UdpStreamer&  s_;
    unsigned      block_;
    int           total_ = AUDIO_CHANNELS;
    std::string   clock_;
    std::vector<std::unique_ptr<Dev>> dev_;
    std::vector<int32_t> out_;
    std::atomic<bool> running_{false};
    std::thread   watch_, clock_th_;
    std::atomic<uint64_t> last_master_us_{0};
    std::mutex    mix_mtx_;
};
