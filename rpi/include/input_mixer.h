#pragma once
// Interfaces USB de audio EXTRA además de la Traktor (opcional).
// Las entradas 0..9 son siempre las de la Traktor (si no está, silencio); cada interfaz extra
// añade sus canales a continuación (10, 11, …). Sin interfaces extra este módulo no se usa y el
// camino de la Traktor es exactamente el de siempre (captura → UdpStreamer::pushFrames).
// Cada interfaz tiene su propio reloj: se absorbe la deriva con un buffer elástico por interfaz.
#include "audio_capture.h"
#include "udp_streamer.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

struct ExtraSpec { std::string device; unsigned channels = 2; };    // device: «hw:N,0» o parte del nombre de la tarjeta

struct CardInfo { int index = 0; std::string id, name; int max_channels = 0; bool traktor = false, busy = false; };

class InputMixer {
public:
    InputMixer(UdpStreamer& s, unsigned block_frames);
    ~InputMixer();
    // Ficheros de configuración: una línea «dispositivo|canales» por interfaz
    static std::vector<ExtraSpec> loadFile(const std::string& path);
    static bool saveFile(const std::string& path, const std::vector<ExtraSpec>& v, int traktor_pos = 0, bool traktor_master = true);
    // Posición de la Traktor entre las interfaces (0 = primera) y si es el reloj principal; línea «@traktor|pos|master» del mismo fichero
    static void loadTraktor(const std::string& path, int& pos, bool& master);
    static void parseTraktor(const std::string& body, int& pos, bool& master);   // {"traktor":{"pos":N,"master":true|false}}; si falta, no toca los valores
    // Valida la lista (canales 1..32, total ≤ MAX_INPUTS-10, sin la Traktor ni repetidas); err en español
    static bool validate(std::vector<ExtraSpec>& v, std::string& err, bool traktor_present = true);
    // Extrae la lista de {"extras":[{"device":"…","channels":N},…]}
    static bool parseJson(const std::string& body, std::vector<ExtraSpec>& out, std::string& err, bool traktor_present = true);
    static std::vector<CardInfo> listCards();

    // Posición de la Traktor en el orden de entradas y reloj: master=true → la Traktor manda el reloj (camino de siempre);
    // master=false → la Traktor entra por un colchón como las demás y el mezclador usa su propio reloj. Antes de setExtras().
    void setTraktorLayout(int pos, bool master) { trk_pos_ = pos < 0 ? 0 : pos; trk_master_ = master; }
    int  traktorFirst() const { return trk_first_; }
    void tickForTest(uint32_t frames, uint64_t ts) { mix(nullptr, frames, ts); }
    void setTraktorPresent(bool p) { traktor_ = p; }           // ANTES de setExtras(): sin Traktor las extras empiezan en la entrada 1
    void setExtras(const std::vector<ExtraSpec>& v);          // antes de start()
    int  totalChannels() const { return total_; }
    bool start();                                              // abre las extras que pueda y arranca el hilo de vigilancia
    void stop();
    // Bloque de la Traktor (hilo de captura de la Traktor): une y envía al streamer
    void onTraktor(const int32_t* data, uint32_t frames, uint64_t ts_us);
    // Sin Traktor: reloj propio (bloques de silencio + extras)
    bool active() const { return running_.load(); }
    std::string statusJson() const;
    // Pruebas: alimenta una interfaz como si fuera su captura (sin ALSA)
    void injectForTest(size_t idx, const int32_t* data, uint32_t frames);

private:
    struct Extra {
        ExtraSpec spec;
        int       first = 0;                      // primer canal de entrada global
        std::string resolved, error;
        std::unique_ptr<AudioCapture> cap;
        mutable std::mutex mtx;
        std::vector<int32_t> ring;                // frames*channels, FIFO simple
        size_t    fill = 0;                       // frames en el buffer
        bool      primed = false;
        uint64_t  underruns = 0, dropped = 0, blocks = 0;
        std::atomic<bool> online{false};
        uint64_t  next_try_us = 0;
        bool      test = false;                   // alimentada a mano (pruebas)
    };
    void push(Extra& e, const int32_t* d, uint32_t frames);
    void pull(Extra& e, uint32_t frames, int T);
    void mix(const int32_t* traktor, uint32_t frames, uint64_t ts);
    bool openExtra(Extra& e);
    void setErr(Extra& e, const std::string& s);
    void watchLoop();
    UdpStreamer&  s_;
    unsigned      block_;
    bool          traktor_ = true;
    int           total_ = AUDIO_CHANNELS;
    std::vector<std::unique_ptr<Extra>> ex_;
    std::unique_ptr<Extra> trk_;                 // Traktor con colchón (solo si no es el reloj principal)
    int  trk_pos_ = 0, trk_first_ = 0;
    bool trk_master_ = true;
    std::vector<int32_t> out_, zero_;
    std::atomic<bool> running_{false};
    std::thread   watch_, clock_;
    std::atomic<uint64_t> last_traktor_us_{0};
    std::mutex    mix_mtx_;                       // serializa mix() (Traktor y reloj propio)
};
