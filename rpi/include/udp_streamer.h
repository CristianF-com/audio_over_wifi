#pragma once
#include <cstdint>
#include <string>
#include <array>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <netinet/in.h>
#include "protocol.h"
#include "dsp.h"

// Papel de cada altavoz en la gestión de graves (crossover Linkwitz-Riley 4.º orden en la RPi)
enum NodeRole : uint8_t {
    ROLE_FULL = 0,   // rango completo: recibe el canal tal cual
    ROLE_SAT  = 1,   // satélite: pasa-altos; sus graves se envían al subwoofer (si hay un sub online)
    ROLE_SUB  = 2    // subwoofer: su canal (LFE) + los graves de todos los satélites, con pasa-bajos
};

// Estado de cada ESP32 (visible en la web UI)
struct ESPNode {
    uint8_t            id        = 0;
    std::string        ip;
    uint16_t           audio_port= PORT_AUDIO;
    uint8_t            ch_left   = 0;
    uint8_t            ch_right  = 1;
    uint8_t            volume_l  = 255;
    uint8_t            volume_r  = 255;
    bool               mono      = false;   // «1 ESP por canal»: ch_right = ch_left, vol_r = vol_l, paquetes PKT_AUDIO_MONO
    bool               enabled   = true;
    uint8_t            role      = ROLE_FULL;
    float              eq_db[EQ_BANDS] = {0, 0, 0, 0, 0, 0};   // ecualizador por altavoz (dB por banda de 1 octava)
    bool               online    = false;
    uint64_t           last_seen = 0;       // µs (mono)
    sockaddr_in        audio_addr{};        // ip:audio_port
    sockaddr_in        ctrl_addr{};         // ip:5005

    // Métricas del último REPORT
    int64_t  clock_offset_us = 0;
    int32_t  rtt_us          = 0;
    int32_t  jitter_us       = 0;
    int32_t  buffer_frames   = 0;
    int32_t  sync_err_us     = 0;
    uint32_t lost_packets    = 0;
    uint32_t late_packets    = 0;
    uint32_t underruns       = 0;
    float    drift_ppm       = 0.0f;
    uint32_t recovered_packets = 0;      // recuperados por reenvío (REPORT del ESP)
    uint32_t retx_sent       = 0;        // reenvíos servidos por la RPi

    // Ajuste de alineación (distancia/latencia acústica) en µs. + = retrasa este nodo.
    int32_t  latency_offset_us = 0;
};

// ── Calibración acústica ────────────────────────────────────────────────────
// La RPi emite, nodo por nodo, un chirp logarítmico (silenciando el resto del audio).
// El móvil graba y /calibrate calcula retardos y niveles. Estas constantes se envían a
// la página en /api/cal/start, así que hay una única fuente de verdad.
constexpr int64_t CAL_SLOT_US  = 1400000;   // separación entre chirps
constexpr int64_t CAL_CHIRP_US = 300000;    // duración del chirp
constexpr int64_t CAL_LEAD_US  = 1500000;   // desde la petición hasta el 1er chirp
constexpr int64_t CAL_TAIL_US  = 600000;    // margen final
constexpr double  CAL_F0_HZ    = 250.0;
constexpr double  CAL_F1_HZ    = 12000.0;
constexpr double  CAL_FADE_US  = 10000.0;   // fade-in/out coseno
constexpr size_t  CAL_MAX_SLOTS = MAX_NODES * 3;

// Distribución de canales: preajustes que configuran todos los nodos de una vez.
enum Layout : int {
    LAYOUT_STEREO5 = 0,   // 5 ESP estéreo: nodo i = canales (2i, 2i+1); nodos 5..9 deshabilitados
    LAYOUT_MONO10  = 1,   // 10 ESP mono:   nodo i = canal i (se oye por L y R); todos habilitados
    LAYOUT_CUSTOM  = 2    // mapa editado a mano
};

class UdpStreamer {
public:
    UdpStreamer();
    ~UdpStreamer();

    bool init();
    void shutdown();

    // Llamado desde el hilo de captura (hot path)
    void pushFrames(const int32_t* s32_data, uint32_t frames, uint64_t timestamp_us);

    std::array<ESPNode, MAX_NODES> getNodes() const;
    bool setNodeConfig(uint8_t id, uint8_t ch_l, uint8_t ch_r,
                       uint8_t vol_l, uint8_t vol_r, bool enabled, bool mono = false);
    // Aplica un preajuste a TODOS los nodos (conserva volúmenes y ajustes de latencia).
    bool setLayout(int layout);
    int  layout() const { return layout_.load(); }
    bool setLatencyOffset(uint8_t id, int32_t us);   // ±500000 µs
    bool setNodeRole(uint8_t id, uint8_t role);                      // NodeRole
    bool setNodeEq(uint8_t id, const float gains_db[EQ_BANDS]);      // ±EQ_MAX_DB por banda
    float xoverHz() const { return xover_hz_.load(); }               // frecuencia de corte graves (40..200 Hz)
    bool  setXoverHz(float hz);
    // Procesado activable: con ambos en false la señal pasa SIN filtros (pass-through) —solo volumen y retardo por altavoz—.
    // Úsalo cuando los canales ya llegan separados (5.1/7.1 decodificado). Activa el crossover/EQ si llega una señal
    // estéreo/mezclada que hay que separar o ecualizar. Valor por defecto: ambos desactivados.
    bool xoverOn() const { return xover_on_.load(); }
    bool eqOn()    const { return eq_on_.load(); }
    void setXoverOn(bool on) { xover_on_.store(on); dirty_.store(true); }
    void setEqOn(bool on)    { eq_on_.store(on);    dirty_.store(true); }
    // Ajuste manual extra (ms) que se suma a la latencia recomendada para el reproductor (p. ej. retardo de entrada USB)
    int  lipsyncExtraMs() const { return lipsync_extra_ms_.load(); }
    bool setLipsyncExtraMs(int ms);                                  // -50..200

    // Calibración: sequence = ids de nodo en orden de emisión (se repite por rondas).
    bool    calStart(const std::vector<uint8_t>& sequence, float level_db);
    void    calStop();
    bool    calRunning() const { return cal_state_.load() != 0; }
    int64_t calRemainingMs() const;

    float masterVolume() const { return master_vol_.load(); }
    void  setMasterVolume(float v);

    // Latencia global captura->altavoz (PLAYOUT_MIN_US..MAX_US). Se envía en caliente a los nodos online.
    uint32_t playoutUs() const { return playout_us_.load(); }
    bool     setPlayoutUs(uint32_t us);

    // Persistencia (fichero key=value). loadConfig() al arrancar; saveConfig() es automático (~1 s tras cada cambio).
    void setConfigPath(const std::string& p) { cfg_path_ = p; }
    bool loadConfig();
    bool saveConfig();

    uint64_t sendErrors() const { return send_errors_.load(); }

private:
    void controlLoop();                       // HELLO / REPORT / SYNC_REQ
    void markOffline();
    void handleHello(const uint8_t* b, int n, const sockaddr_in& src);
    void handleNack(const uint8_t* b, int n, const sockaddr_in& src);
    void handleReport(const uint8_t* b, int n, const sockaddr_in& src);
    void handleSync(const uint8_t* b, int n, const sockaddr_in& src, uint64_t t2);

    struct TxState {                          // solo lo toca el hilo de captura
        AudioPacket pkt;
        uint32_t    n   = 0;                  // frames acumulados en pkt
        uint32_t    seq = 0;
        bool        mono = false;             // modo del paquete en construcción
    };

    int               ctrl_sock_  = -1;
    int               audio_sock_ = -1;
    std::thread       ctrl_thread_;
    std::atomic<bool> running_{false};
    std::atomic<float> master_vol_{1.0f};
    std::atomic<uint32_t> playout_us_{PLAYOUT_DELAY_US};
    std::string           cfg_path_;
    std::atomic<bool>     dirty_{false};
    std::atomic<int>      layout_{LAYOUT_STEREO5};
    std::atomic<float>    xover_hz_{(float)XOVER_DEFAULT_HZ};
    std::atomic<bool>     xover_on_{false}, eq_on_{false};
    std::atomic<int>      lipsync_extra_ms_{0};

    // Historial de los últimos TX_HISTORY_PACKETS paquetes por nodo, para atender los NACK
    struct HistSlot { uint32_t seq = 0; bool used = false; AudioPacket pkt; };
    std::vector<HistSlot>   hist_[MAX_NODES];
    std::mutex              hist_mtx_[MAX_NODES];
    std::atomic<uint32_t>   retx_[MAX_NODES] = {};
    uint64_t                retx_win_start_[MAX_NODES] = {};      // limitador: solo hilo de control
    uint32_t                retx_win_n_[MAX_NODES] = {};

    // DSP (solo hilo de captura)
    struct Dsp {
        double   xo_fc = 0;
        Lr4      hp[AUDIO_CHANNELS], lp[AUDIO_CHANNELS];
        EqChain  eq[MAX_NODES][2];
        uint32_t prev_hp = 0, prev_lp = 0;
    } dsp_;
    std::vector<float> dsp_hp_, dsp_lp_, dsp_bass_;
    uint64_t              last_save_us_ = 0;
    void sendConfig(const ESPNode& n);
    std::atomic<uint64_t> send_errors_{0};

    mutable std::mutex              nodes_mtx_;
    std::array<ESPNode, MAX_NODES>  nodes_;
    std::array<TxState, MAX_NODES>  tx_;

    // Calibración: petición (hilo web) -> estado (hilo de captura)
    std::mutex            cal_mtx_;
    std::vector<uint8_t>  cal_req_seq_;
    float                 cal_req_level_db_ = -12.0f;
    std::atomic<bool>     cal_req_{false};
    std::atomic<bool>     cal_stop_req_{false};
    std::atomic<int>      cal_state_{0};         // 0 idle, 1 en curso
    std::atomic<uint64_t> cal_end_wall_us_{0};   // para /api/cal/status
    bool                  cal_run_ = false;      // --- solo hilo de captura ---
    double                cal_t0_ = 0, cal_end_ts_ = 0;
    float                 cal_amp_ = 0;
    uint8_t               cal_seq_[CAL_MAX_SLOTS] = {};
    size_t                cal_len_ = 0;
    std::vector<float>    cal_wave_;
    std::vector<uint8_t>  cal_who_;

    // Reloj de stream (solo hilo de captura)
    bool    stream_init_ = false;
    double  stream_next_ = 0.0;               // µs maestros del próximo frame
};
