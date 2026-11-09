#pragma once
#include <cstdint>
#include <string>
#include <array>
#include <deque>
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
    uint64_t           mac       = 0;       // MAC (48 bits) del ESP que ocupa esta casilla; 0 = libre. Se persiste: asignación automática del ID
    bool               known     = false;   // este ESP ya se conectó alguna vez: aparece en el panel aunque esté apagado (se persiste)
    float              pos_x     = -1;      // posición en el plano de la sala, % del ancho (0..100); <0 = automática (según el canal)
    float              pos_y     = -1;      // % del alto (0..100)
    float              pos2_x    = -1;      // idem para el canal derecho (los nodos estéreo son dos altavoces en el plano)
    float              pos2_y    = -1;
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

    // Cortes recientes: los contadores del ESP son acumulados desde que arrancó; aquí se guardan por minuto (últimos RECENT_MIN)
    // para que un altavoz que lleva un rato bien deje de figurar con errores.
    static constexpr int RECENT_MIN = 10;
    bool     cnt_init        = false;    // ya hay contadores de referencia (el primer REPORT no cuenta el pasado)
    uint32_t cnt_lost = 0, cnt_late = 0, cnt_under = 0;
    uint32_t auto_events = 0;            // eventos (perdidos+tarde+vacíos) desde la última vuelta del modo AUTO de latencia
    uint64_t bk_min[RECENT_MIN]   = {};  // minuto (mono) de cada cubeta, 0 = vacía
    uint32_t bk_lost[RECENT_MIN]  = {}, bk_late[RECENT_MIN] = {}, bk_under[RECENT_MIN] = {};
    uint32_t recent_lost = 0, recent_late = 0, recent_under = 0;   // suma de los últimos RECENT_MIN minutos (la rellena getNodes)

    // Ajuste de alineación (distancia/latencia acústica) en µs. + = retrasa este nodo.
    int32_t  latency_offset_us = 0;

    float    dist_m          = -1;       // distancia del altavoz a quien escucha (m); <0 = sin definir. Solo informativa: sirve para calcular retardo y nivel
    uint8_t  fw_version      = 0;        // versión de firmware que anuncia el ESP en el HELLO (0 = firmware antiguo)
    uint8_t  chip            = FW_CHIP_UNKNOWN;
    int8_t   rssi_dbm        = 0;        // señal WiFi del último REPORT (0 = desconocida)
    bool     ota_busy        = false;    // actualizando firmware: no se le envía audio
    uint64_t ota_until_us    = 0;        // mono_us() hasta el que se espera a que termine
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
    bool forgetNode(uint8_t id);                                     // quita un ESP apagado del panel (false si está en línea); su configuración se conserva
    bool setNodeRole(uint8_t id, uint8_t role);                      // NodeRole
    bool setNodePos(uint8_t id, float x, float y, int side = 0);  // side 0 = canal izquierdo, 1 = derecho;                   // plano de la sala: x,y en 0..100 (% del plano); x<0 e y<0 = automática
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

    // Latencia AUTO: sube el margen de red (de 10 en 10 ms, hasta un tope) si hay cortes y lo baja despacio cuando lleva un rato limpio
    // (nunca por debajo del mínimo que eligió el usuario). Cada cambio reenvía CONFIG (breve salto en el audio y desplaza el sync con la imagen).
    struct AutoPlayout { bool on; uint32_t floor_us, max_us, changes; int64_t last_change_ago_ms; int last_dir; uint32_t recent_events, raise_events, clean_min, step_us; };
    AutoPlayout autoPlayout() const;
    bool        setAutoPlayout(bool on, int64_t max_us = -1);
    // Umbrales: cortes por minuto que hacen subir (1..100), minutos sin cortes para bajar (1..120) y paso de subida (5..40 ms, múltiplo de 5). -1 = no cambia.
    bool        setAutoThresholds(int raise_events, int clean_min, int step_ms);   // max_us < 0: no cambia el tope. Al activar, el mínimo pasa a ser el margen actual
    bool        setPlayoutManual(uint32_t us);                     // ajuste manual: con AUTO activo pasa a ser el nuevo mínimo
    void        autoTick();                                       // lo llama el bucle de control (internamente va cada AUTO_TICK)

    // Persistencia (fichero key=value). loadConfig() al arrancar; saveConfig() es automático (~1 s tras cada cambio).
    void setConfigPath(const std::string& p) { cfg_path_ = p; }
    bool loadConfig();
    bool saveConfig();

    uint64_t sendErrors() const { return send_errors_.load(); }

    // Medidores de la ENTRADA (antes de volumen/DSP). Los escribe el hilo de captura; la web los lee sin bloquear.
    float    meterDb(int ch) const;               // pico por canal en dBFS (-90 = silencio), ataque instantáneo y caída ~300 ms
    int64_t  signalAgeMs() const;                 // ms desde que algún canal superó -70 dBFS; -1 si nunca
    int64_t  captureAgeMs() const;                // ms desde el último bloque de audio recibido de la tarjeta; -1 si nunca
    uint32_t packetsPerSec() const { return pps_.load(std::memory_order_relaxed); }   // paquetes de audio enviados por segundo (todos los nodos)

    // ── Tono de prueba por altavoz (diagnóstico) ────────────────────────────────
    // Un tono de 1 kHz a pulsos suena SOLO en ese altavoz (side 0 = izquierdo, 1 = derecho, 2 = ambos).
    // solo = true silencia el resto de altavoces mientras dura. Respeta el volumen del altavoz y el maestro.
    bool toneStart(uint8_t id, int side, int ms, bool solo, float level_db);   // 1000..10000 ms, -40..-6 dBFS
    void toneStop();
    bool toneActive() const { return tone_active_.load(); }
    int  toneNode() const { return tone_id_.load(); }
    int  toneSide() const { return tone_side_.load(); }
    int64_t toneRemainingMs() const;

    // ── Opciones de procesado nuevas (desactivadas por defecto: pass-through intacto) ──
    // Graves (LFE) a los frontales: si NINGÚN altavoz activo reproduce el canal LFE (3), su contenido (pasa-bajos 120 Hz)
    // se suma a los canales 0 y 1 de los altavoces de rango completo. Con un subwoofer sonando el LFE no tiene efecto.
    bool  lfeFront() const { return lfe_front_.load(); }
    void  setLfeFront(bool on) { lfe_front_.store(on); dirty_.store(true); }
    float lfeDb() const { return lfe_db_.load(); }
    bool  setLfeDb(float db);                                    // -12..+10 dB
    bool  lfeActive() const { return lfe_active_.load(); }       // ahora mismo se está mezclando el LFE en los frontales
    // Modo noche: compresor de rango dinámico sobre la salida (0 = desactivado, 1 suave, 2 medio, 3 fuerte).
    int   nightLevel() const { return night_.load(); }
    bool  setNightLevel(int lv);

    bool  setNodeDist(uint8_t id, float m);                      // 0.2..30 m, o <0 para borrarla

    // ── Perfiles guardados (volúmenes, retardos, EQ, roles y opciones de procesado de todos los altavoces) ──
    static constexpr int MAX_PROFILES = 12;
    std::vector<std::string> profileNames() const;
    bool profileSave(const std::string& name, std::string& err); // mismo nombre = sobrescribe
    bool profileLoad(int idx);
    bool profileDelete(int idx);
    int  profileActive() const { return profile_active_.load(); }// último cargado en esta sesión; -1 si ninguno
    bool loadProfiles();

    // ── Nombres de las 10 entradas de audio (FL, FR, C…), editables; fichero aparte <config>.chnames ──
    static const char* defaultChannelName(int ch);
    std::vector<std::string> channelNames() const;       // solo las 10 editables de la Traktor
    std::vector<std::string> allChannelNames() const;    // + las de las interfaces USB extra
    // Total de canales de entrada que recibe pushFrames (10 por defecto; Traktor + interfaces extra) y nombres de los extra.
    void setInputChannels(int n, const std::vector<std::string>& extra_names);
    int  inputChannels() const { return in_ch_.load(); }
    bool setChannelNames(const std::vector<std::string>& names, std::string& err);   // vacío = nombre por defecto
    bool loadChannelNames();
    // ── Nombre de cada altavoz (p. ej. «Frontal»); fichero aparte <config>.spknames; vacío = sin nombre ──
    std::string nodeName(int id) const;
    bool setNodeName(int id, const std::string& name, std::string& err);
    bool loadSpkNames();

    // ── Actualización de firmware por WiFi (OTA): envía la orden al ESP y deja de mandarle audio ──
    bool sendOta(uint8_t id, uint16_t http_port, uint32_t size, uint64_t token);
    void cancelOta(uint8_t id);

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
    std::atomic<bool>     auto_on_{false};
    std::atomic<uint32_t> auto_floor_us_{PLAYOUT_DELAY_US}, auto_max_us_{160000}, auto_changes_{0};
    std::atomic<int>      auto_last_dir_{0};
    std::atomic<uint64_t> auto_last_change_us_{0};
    std::atomic<uint32_t> auto_recent_{0};
    uint64_t auto_tick_us_ = 5000000, auto_window_us_ = 60000000, auto_cool_us_ = 30000000;
    bool     auto_fast_ = false;                                 // solo pruebas (SONOS_AUTO_FAST=1): tiempos en décimas de segundo
    std::atomic<uint32_t> auto_raise_events_{8}, auto_clean_min_{10}, auto_step_us_{10000};
    uint64_t autoCleanUs() const { return auto_fast_ ? 3000000ULL : (uint64_t)auto_clean_min_.load() * 60000000ULL; }
    uint64_t auto_last_tick_ = 0, auto_last_event_us_ = 0;
    std::mutex auto_mtx_;
    std::deque<std::pair<uint64_t, uint32_t>> auto_win_;       // (hora, eventos) dentro de la ventana
    std::string           cfg_path_;
    std::atomic<bool>     dirty_{false};
    std::atomic<int>      layout_{LAYOUT_STEREO5};
    std::atomic<float>    xover_hz_{(float)XOVER_DEFAULT_HZ};
    std::atomic<bool>     xover_on_{false}, eq_on_{false};
    std::atomic<int>      lipsync_extra_ms_{0};
    std::atomic<bool>     lfe_front_{false}, lfe_active_{false};
    std::atomic<float>    lfe_db_{0.0f};
    std::atomic<int>      night_{0};
    std::atomic<int>      profile_active_{-1};

    // Tono de prueba: petición (hilo web) -> estado (hilo de captura)
    std::atomic<bool>     tone_active_{false}, tone_req_{false}, tone_stop_req_{false}, tone_solo_{false};
    std::atomic<int>      tone_id_{-1}, tone_side_{2}, tone_dur_ms_{3000};
    std::atomic<float>    tone_level_db_{-20.0f};
    std::atomic<uint64_t> tone_end_wall_us_{0};
    bool                  tone_run_ = false;                       // --- solo hilo de captura ---
    double                tone_t0_ = 0, tone_end_ts_ = 0;
    float                 tone_amp_ = 0;
    int                   tone_run_id_ = -1, tone_run_side_ = 2;
    bool                  tone_run_solo_ = false;
    std::vector<float>    tone_wave_;

    // Perfiles
    struct Profile {
        std::string name;
        float master = 1.0f, xover_hz = (float)XOVER_DEFAULT_HZ, lfe_db = 0;
        bool  xover_on = false, eq_on = false, lfe_front = false;
        int   night = 0;
        struct N { uint8_t vl = 255, vr = 255, role = ROLE_FULL; int32_t lat = 0; float eq[EQ_BANDS] = {0, 0, 0, 0, 0, 0}; } n[MAX_NODES];
    };
    mutable std::mutex    names_mtx_;
    std::vector<std::string> extra_names_;                 // nombres de las entradas extra (interfaces USB)
    std::atomic<int>      in_ch_{AUDIO_CHANNELS};
    std::string           ch_names_[MAX_NODES];            // nombre elegido por el usuario; vacío = el de fábrica
    bool saveChannelNames();
    std::string           spk_names_[MAX_NODES];
    bool saveSpkNames();
    mutable std::mutex    prof_mtx_;
    std::vector<Profile>  profiles_;
    bool saveProfiles();

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
        Lr4      hp[MAX_INPUTS], lp[MAX_INPUTS];
        EqChain  eq[MAX_NODES][2];
        uint32_t prev_hp = 0, prev_lp = 0;
        Lr4      lfe_lp;                     // pasa-bajos de 120 Hz del canal LFE (graves a los frontales)
        bool     lfe_prev = false;
        double   night_env_db = 0;           // reducción de ganancia del compresor (dB, suavizada)
    } dsp_;
    std::vector<float> dsp_hp_, dsp_lp_, dsp_bass_, dsp_lfe_, dsp_night_;
    uint64_t              last_save_us_ = 0;
    void sendConfig(const ESPNode& n);
    std::atomic<uint64_t> send_errors_{0};
    std::array<std::atomic<uint32_t>, MAX_INPUTS> meter_{};   // pico con caída, escala S32 (0..2^31)
    std::atomic<uint64_t> last_signal_us_{0}, last_block_us_{0}, pkts_sent_{0};
    std::atomic<uint32_t> pps_{0};
    uint64_t              pps_t_ = 0, pps_n_ = 0;                // solo hilo de captura

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
