#include "udp_streamer.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <time.h>
#include <cerrno>

static constexpr uint64_t OTA_WAIT_US = 150ull * 1000000ull;   // tiempo máximo que se espera a que un ESP termine de actualizarse

static uint64_t mono_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

// Mapa de canales de un preajuste para el nodo i.
static void layoutDefaults(int layout, uint8_t i, uint8_t& l, uint8_t& r, bool& mono, bool& enabled) {
    if (layout == LAYOUT_MONO10) { l = r = i % AUDIO_CHANNELS; mono = true; enabled = true; }
    else if (i < AUDIO_CHANNELS / 2) { l = 2 * i; r = 2 * i + 1; mono = false; enabled = true; }
    else { l = r = i % AUDIO_CHANNELS; mono = true; enabled = false; }   // nodos 5..9 sin usar en estéreo
}

UdpStreamer::UdpStreamer() {
    for (uint8_t i = 0; i < MAX_NODES; i++) {
        nodes_[i].id       = i;
        layoutDefaults(LAYOUT_STEREO5, i, nodes_[i].ch_left, nodes_[i].ch_right, nodes_[i].mono, nodes_[i].enabled);
        tx_[i].pkt.magic    = PROTOCOL_MAGIC;
        tx_[i].pkt.pkt_type = PKT_AUDIO;
        tx_[i].pkt.esp_id   = i;
        tx_[i].pkt.num_samples = AUDIO_PKT_SAMPLES;
        hist_[i].resize(TX_HISTORY_PACKETS);
    }
}
UdpStreamer::~UdpStreamer() { shutdown(); }

bool UdpStreamer::init() {
    int yes = 1;
    ctrl_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctrl_sock_ < 0) { perror("ctrl socket"); return false; }
    setsockopt(ctrl_sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(ctrl_sock_, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    struct timeval tv{1, 0};
    setsockopt(ctrl_sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    sockaddr_in ba{};
    ba.sin_family = AF_INET; ba.sin_port = htons(PORT_DISCOVERY);
    ba.sin_addr.s_addr = INADDR_ANY;
    if (bind(ctrl_sock_, (sockaddr*)&ba, sizeof(ba)) < 0) { perror("ctrl bind"); return false; }

    audio_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (audio_sock_ < 0) { perror("audio socket"); return false; }
    int sndbuf = 1 << 20;
    setsockopt(audio_sock_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    int tos = 0xB8;                                   // DSCP EF: prioridad WMM si el AP la respeta
    setsockopt(audio_sock_, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));

    running_ = true;
    ctrl_thread_ = std::thread(&UdpStreamer::controlLoop, this);
    fprintf(stderr, "[UDP] Control :%d | Audio unicast :%d listo\n", PORT_DISCOVERY, PORT_AUDIO);
    return true;
}

void UdpStreamer::shutdown() {
    if (!running_.exchange(false) && !ctrl_thread_.joinable()) return;
    if (ctrl_thread_.joinable()) ctrl_thread_.join();   // sale por el timeout de 1 s
    if (dirty_.exchange(false) && !cfg_path_.empty()) saveConfig();
    if (ctrl_sock_  >= 0) { ::close(ctrl_sock_);  ctrl_sock_  = -1; }
    if (audio_sock_ >= 0) { ::close(audio_sock_); audio_sock_ = -1; }
}

void UdpStreamer::setMasterVolume(float v) {
    if (!(v >= 0.0f)) v = 0.0f;          // también filtra NaN
    if (v > 2.0f) v = 2.0f;
    master_vol_.store(v);
    dirty_.store(true);
}

void UdpStreamer::sendConfig(const ESPNode& n) {
    if (!n.online || ctrl_sock_ < 0) return;
    ConfigPacket cp{};
    cp.magic = PROTOCOL_MAGIC; cp.pkt_type = PKT_CONFIG; cp.esp_id = n.id;
    cp.ch_left = n.ch_left; cp.ch_right = n.ch_right;
    cp.volume_l = n.volume_l; cp.volume_r = n.volume_r;
    cp.enabled = n.enabled ? 1 : 0;
    cp.playout_delay_us = playout_us_.load();
    sendto(ctrl_sock_, &cp, sizeof(cp), 0, (const sockaddr*)&n.ctrl_addr, sizeof(n.ctrl_addr));
}

bool UdpStreamer::setPlayoutUs(uint32_t us) {
    if (us < PLAYOUT_MIN_US || us > PLAYOUT_MAX_US) return false;
    playout_us_.store(us);
    dirty_.store(true);
    std::array<ESPNode, MAX_NODES> snap = getNodes();
    for (const auto& n : snap) sendConfig(n);            // los ESP reajustan solos (breve hueco/salto al cambiar)
    fprintf(stderr, "[Control] Playout = %u ms\n", us / 1000);
    return true;
}

// ─── Control loop ─────────────────────────────────────────────────────────────
void UdpStreamer::controlLoop() {
    sched_param sp{}; sp.sched_priority = 60;          // mejor precisión de t2/t3 en sync
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);

    uint8_t buf[256];
    while (running_) {
        sockaddr_in src{}; socklen_t slen = sizeof(src);
        int n = recvfrom(ctrl_sock_, buf, sizeof(buf), 0, (sockaddr*)&src, &slen);
        uint64_t t2 = mono_us();                       // lo antes posible
        if (n >= 5) {
            uint32_t magic; memcpy(&magic, buf, 4);
            if (magic == PROTOCOL_MAGIC) {
                switch (buf[4]) {
                    case PKT_HELLO:    handleHello (buf, n, src);     break;
                    case PKT_REPORT:   handleReport(buf, n, src);     break;
                    case PKT_SYNC_REQ: handleSync  (buf, n, src, t2); break;
                    case PKT_NACK:     handleNack  (buf, n, src);     break;
                    default: break;
                }
            }
        }
        markOffline();                                 // SIEMPRE, también tras timeout
        if (dirty_.load() && !cfg_path_.empty() && mono_us() - last_save_us_ > 1000000ULL) {
            dirty_.store(false); last_save_us_ = mono_us(); saveConfig();
        }
    }
}

void UdpStreamer::markOffline() {
    uint64_t now = mono_us();
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (auto& n : nodes_) {
        if (n.ota_busy && now > n.ota_until_us) { n.ota_busy = false; fprintf(stderr, "[OTA] ESP%d: tiempo agotado\n", n.id); }
        if (n.ota_busy) continue;                       // se está actualizando: el ESP calla un rato
        if (n.online && (now - n.last_seen) > 10000000ULL) {
            n.online = false;
            fprintf(stderr, "[Control] ESP%d offline\n", n.id);
        }
    }
}

void UdpStreamer::handleHello(const uint8_t* b, int n, const sockaddr_in& src) {
    if (n < (int)sizeof(HelloPacket)) return;
    HelloPacket h; memcpy(&h, b, sizeof(h));
    if (h.esp_id >= MAX_NODES && h.esp_id != ESP_ID_AUTO) return;
    if (h.version != PROTOCOL_VERSION) {
        fprintf(stderr, "[Control] ESP%d con protocolo v%u (esperado v%u) — ignorado\n",
                h.esp_id, h.version, PROTOCOL_VERSION);
        return;
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));

    uint64_t mac = 0;
    for (int i = 0; i < 6; i++) mac = (mac << 8) | h.mac[i];
    uint8_t assigned = 0;
    WelcomePacket w{};
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        if (h.esp_id == ESP_ID_AUTO) {                 // sin número: la casilla se asigna por MAC (la misma de siempre, o la primera libre)
            int slot = -1;
            if (mac != 0) {
                for (int i = 0; i < MAX_NODES && slot < 0; i++) if (nodes_[i].mac == mac) slot = i;
                for (int i = 0; i < MAX_NODES && slot < 0; i++) if (!nodes_[i].known && !nodes_[i].online && nodes_[i].mac == 0) slot = i;
            }
            if (slot < 0) {
                fprintf(stderr, "[Control] ESP sin ID (%s): no hay casillas libres o no envía MAC — ignorado\n", ip);
                return;
            }
            h.esp_id = (uint8_t)slot; assigned = WELCOME_F_ASSIGNED;
            if (nodes_[slot].mac != mac)
                fprintf(stderr, "[Control] ESP %02X:%02X:%02X:%02X:%02X:%02X -> casilla %d (automática)\n",
                        h.mac[0], h.mac[1], h.mac[2], h.mac[3], h.mac[4], h.mac[5], slot);
        }
        if (mac != 0) {                                 // recuerda quién ocupa cada casilla (también con número fijo) y libera la anterior de ese ESP
            for (int i = 0; i < MAX_NODES; i++) if (i != h.esp_id && nodes_[i].mac == mac) { nodes_[i].mac = 0; dirty_.store(true); }
            if (nodes_[h.esp_id].mac != mac) { nodes_[h.esp_id].mac = mac; dirty_.store(true); }
        }
        ESPNode& node = nodes_[h.esp_id];
        if (node.online && node.ip != ip)
            fprintf(stderr, "[Control] AVISO: dos placas con ID %u (%s y %s)\n",
                    h.esp_id, node.ip.c_str(), ip);
        bool was_offline = !node.online;
        if (!node.known) { node.known = true; node.enabled = true; dirty_.store(true); }   // primera vez: aparece en el panel y suena
        node.fw_version = HELLO_FW_VERSION(h.reserved);
        node.chip       = HELLO_FW_CHIP(h.reserved);
        if (node.ota_busy && mono_us() > node.ota_until_us - OTA_WAIT_US + 4000000ull) {   // el ESP vuelve a anunciarse (reinicio o fallo): fin de la OTA
            node.ota_busy = false;
            fprintf(stderr, "[OTA] ESP%d de vuelta (firmware v%u)\n", h.esp_id, node.fw_version);
        }
        node.ip         = ip;
        node.audio_port = h.audio_port ? h.audio_port : PORT_AUDIO;
        node.online     = true;
        node.last_seen  = mono_us();
        node.audio_addr = {}; node.audio_addr.sin_family = AF_INET;
        node.audio_addr.sin_port = htons(node.audio_port);
        node.audio_addr.sin_addr = src.sin_addr;
        node.ctrl_addr  = node.audio_addr;
        node.ctrl_addr.sin_port = htons(PORT_DISCOVERY);
        if (was_offline)
            fprintf(stderr, "[Control] ESP%d online: %s ch%d/%d\n",
                    h.esp_id, ip, node.ch_left, node.ch_right);

        w.magic = PROTOCOL_MAGIC; w.pkt_type = PKT_WELCOME; w.esp_id = h.esp_id;
        w.ch_left = node.ch_left; w.ch_right = node.ch_right;
        w.volume_l = node.volume_l; w.volume_r = node.volume_r;
        w.enabled = node.enabled ? 1 : 0;
        w.reserved = assigned;
        w.sample_rate = AUDIO_SAMPLE_RATE;
        w.playout_delay_us = playout_us_.load();
    }
    sendto(ctrl_sock_, &w, sizeof(w), 0, (const sockaddr*)&src, sizeof(src));
}

void UdpStreamer::handleReport(const uint8_t* b, int n, const sockaddr_in& src) {
    if (n < (int)sizeof(ReportPacket)) return;
    ReportPacket r; memcpy(&r, b, sizeof(r));
    if (r.esp_id >= MAX_NODES) return;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    ESPNode& nd = nodes_[r.esp_id];
    if (nd.ip != ip) return;                           // solo reportes desde la IP conocida
    nd.clock_offset_us = r.clock_offset_us;
    nd.rtt_us        = r.rtt_us;
    nd.jitter_us     = r.jitter_us;
    nd.buffer_frames = r.buffer_frames;
    nd.sync_err_us   = r.sync_err_us;
    nd.lost_packets  = r.lost_packets;
    nd.late_packets  = r.late_packets;
    nd.underruns     = r.underruns;
    nd.drift_ppm     = r.drift_ppm;
    nd.recovered_packets = r.recovered_packets;
    nd.rssi_dbm      = r.rssi_dbm;
    nd.last_seen     = mono_us();
    nd.online        = true;
}

// El ESP detectó huecos en seq y pide reenvío. Se reenvía el MISMO paquete (mismo seq y timestamp) si aún está en el historial.
void UdpStreamer::handleNack(const uint8_t* b, int n, const sockaddr_in& src) {
    if (n < (int)sizeof(NackPacket)) return;
    NackPacket k; memcpy(&k, b, sizeof(k));
    if (k.esp_id >= MAX_NODES || k.count == 0 || k.count > NACK_MAX_SEQS) return;
    sockaddr_in to;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        const ESPNode& nd = nodes_[k.esp_id];
        if (!nd.online || src.sin_addr.s_addr != nd.audio_addr.sin_addr.s_addr) return;   // solo desde la IP conocida
        to = nd.audio_addr;
    }
    uint64_t now = mono_us();
    if (now - retx_win_start_[k.esp_id] > 1000000ULL) { retx_win_start_[k.esp_id] = now; retx_win_n_[k.esp_id] = 0; }
    for (int i = 0; i < k.count; i++) {
        if (retx_win_n_[k.esp_id] >= 300) break;                 // tope: 300 reenvíos/s por nodo (evita tormentas)
        AudioPacket pk; bool have = false;
        {
            std::lock_guard<std::mutex> lk(hist_mtx_[k.esp_id]);
            const HistSlot& h = hist_[k.esp_id][k.seq[i] & (TX_HISTORY_PACKETS - 1)];
            if (h.used && h.seq == k.seq[i]) { pk = h.pkt; have = true; }
        }
        if (!have) continue;                                     // ya fuera del historial
        if (sendto(audio_sock_, &pk, sizeof(pk), MSG_DONTWAIT, (const sockaddr*)&to, sizeof(to)) >= 0) {
            retx_[k.esp_id].fetch_add(1, std::memory_order_relaxed);
            retx_win_n_[k.esp_id]++;
        }
    }
}

void UdpStreamer::handleSync(const uint8_t* b, int n, const sockaddr_in& src, uint64_t t2) {
    if (n < (int)sizeof(SyncReqPacket)) return;
    SyncReqPacket q; memcpy(&q, b, sizeof(q));
    SyncRespPacket r{};
    r.magic = PROTOCOL_MAGIC; r.pkt_type = PKT_SYNC_RESP; r.esp_id = q.esp_id;
    r.t1_us = q.t1_us; r.t2_us = t2;
    r.t3_us = mono_us();                               // lo más tarde posible
    sendto(ctrl_sock_, &r, sizeof(r), 0, (const sockaddr*)&src, sizeof(src));
}

// ─── Hot path ─────────────────────────────────────────────────────────────────
static uint64_t rawNowUs() { timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts); return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull; }

float UdpStreamer::meterDb(int ch) const {
    if (ch < 0 || ch >= AUDIO_CHANNELS) return -90.0f;
    const uint32_t v = meter_[ch].load(std::memory_order_relaxed);
    if (v == 0) return -90.0f;
    const float db = 20.0f * log10f((float)v / 2147483648.0f);
    return db < -90.0f ? -90.0f : (db > 0.0f ? 0.0f : db);
}
int64_t UdpStreamer::signalAgeMs() const { const uint64_t t = last_signal_us_.load(std::memory_order_relaxed); return t ? (int64_t)((rawNowUs() - t) / 1000) : -1; }
int64_t UdpStreamer::captureAgeMs() const { const uint64_t t = last_block_us_.load(std::memory_order_relaxed); return t ? (int64_t)((rawNowUs() - t) / 1000) : -1; }

void UdpStreamer::pushFrames(const int32_t* s32, uint32_t frames, uint64_t capture_ts) {
    {   // Medidores de entrada: pico por canal en este bloque; caída ~300 ms (exp(-t/0.3 s)) para que la web vea los picos entre consultas
        static constexpr uint32_t SIGNAL_THRESH = 679000;   // ≈ -70 dBFS sobre 2^31
        uint32_t pk[AUDIO_CHANNELS] = {0};
        for (uint32_t f = 0; f < frames; f++) {
            const int32_t* r = s32 + (size_t)f * AUDIO_CHANNELS;
            for (int c = 0; c < AUDIO_CHANNELS; c++) {
                const int64_t v = r[c];
                const uint32_t a = (uint32_t)(v < 0 ? -v : v);
                if (a > pk[c]) pk[c] = a;
            }
        }
        const float decay = expf(-(float)frames / (float)AUDIO_SAMPLE_RATE / 0.3f);
        bool sig = false;
        for (int c = 0; c < AUDIO_CHANNELS; c++) {
            const uint32_t held = (uint32_t)((float)meter_[c].load(std::memory_order_relaxed) * decay);
            meter_[c].store(pk[c] > held ? pk[c] : held, std::memory_order_relaxed);
            if (pk[c] > SIGNAL_THRESH) sig = true;
        }
        const uint64_t now = rawNowUs();
        last_block_us_.store(now, std::memory_order_relaxed);
        if (sig) last_signal_us_.store(now, std::memory_order_relaxed);
        if (pps_t_ == 0) { pps_t_ = now; pps_n_ = pkts_sent_.load(std::memory_order_relaxed); }
        else if (now - pps_t_ >= 1000000ull) {           // paquetes/s con ventana de ~1 s
            const uint64_t n = pkts_sent_.load(std::memory_order_relaxed);
            pps_.store((uint32_t)((n - pps_n_) * 1000000ull / (now - pps_t_)), std::memory_order_relaxed);
            pps_t_ = now; pps_n_ = n;
        }
    }
    if (audio_sock_ < 0) return;
    const float mv = master_vol_.load(std::memory_order_relaxed);

    // Reloj de stream: sigue al reloj de pared suavemente (evita saltos de ±1 ms
    // por jitter de lectura) y se re-ancla si se desvía > 20 ms.
    double err = (double)capture_ts - stream_next_;
    if (!stream_init_ || std::fabs(err) > 20000.0) { stream_next_ = (double)capture_ts; stream_init_ = true; }
    else stream_next_ += err * 0.002;
    const double period_ts = stream_next_;
    stream_next_ += (double)frames * 1e6 / AUDIO_SAMPLE_RATE;

    // ── Calibración: arranque / parada / fin (hilo de captura) ──
    if (cal_stop_req_.exchange(false)) { cal_run_ = false; cal_req_.store(false); cal_state_.store(0); }
    if (cal_req_.exchange(false)) {
        std::lock_guard<std::mutex> lk(cal_mtx_);
        cal_len_ = cal_req_seq_.size();
        for (size_t i = 0; i < cal_len_; i++) cal_seq_[i] = cal_req_seq_[i];
        cal_amp_ = 32767.0f * powf(10.0f, cal_req_level_db_ / 20.0f);
        cal_t0_  = period_ts + (double)CAL_LEAD_US;
        cal_end_ts_ = cal_t0_ + (double)cal_len_ * (double)CAL_SLOT_US + (double)CAL_TAIL_US;
        cal_run_ = true;
        fprintf(stderr, "[Cal] Inicio: %zu chirps, %.0f dB, t0=+%.1fs\n",
                cal_len_, cal_req_level_db_, CAL_LEAD_US / 1e6);
    }
    if (cal_run_ && period_ts > cal_end_ts_) {
        cal_run_ = false; cal_state_.store(0);
        fprintf(stderr, "[Cal] Fin\n");
    }
    if (cal_run_) {                                      // una vez por período, no por nodo
        if (cal_wave_.size() < frames) { cal_wave_.resize(frames); cal_who_.resize(frames); }
        const double r = log(CAL_F1_HZ / CAL_F0_HZ), T = CAL_CHIRP_US / 1e6;
        const double K = 2.0 * M_PI * CAL_F0_HZ * T / r;
        for (uint32_t f = 0; f < frames; f++) {
            double fts = period_ts + (double)f * 1e6 / AUDIO_SAMPLE_RATE - cal_t0_;
            cal_who_[f] = 255; cal_wave_[f] = 0.0f;
            if (fts < 0) continue;
            size_t k = (size_t)(fts / (double)CAL_SLOT_US);
            if (k >= cal_len_) continue;
            double tau_us = fts - (double)k * (double)CAL_SLOT_US;
            if (tau_us >= (double)CAL_CHIRP_US) continue;
            double t = tau_us / 1e6;
            double w = 1.0;
            if (tau_us < CAL_FADE_US)                    w = 0.5 - 0.5 * cos(M_PI * tau_us / CAL_FADE_US);
            else if (tau_us > CAL_CHIRP_US - CAL_FADE_US) w = 0.5 - 0.5 * cos(M_PI * ((double)CAL_CHIRP_US - tau_us) / CAL_FADE_US);
            cal_who_[f]  = cal_seq_[k];
            cal_wave_[f] = (float)(sin(K * (exp(t * r / T) - 1.0)) * w) * cal_amp_;
        }
    }
    const bool cal = cal_run_;

    // ── Tono de prueba (diagnóstico): arranque / parada / fin (hilo de captura) ──
    if (tone_stop_req_.exchange(false)) { tone_run_ = false; tone_req_.store(false); tone_active_.store(false); }
    if (tone_req_.exchange(false)) {
        tone_run_id_ = tone_id_.load(); tone_run_side_ = tone_side_.load(); tone_run_solo_ = tone_solo_.load();
        tone_amp_ = 32767.0f * powf(10.0f, tone_level_db_.load() / 20.0f);
        tone_t0_ = period_ts; tone_end_ts_ = period_ts + (double)tone_dur_ms_.load() * 1000.0;
        tone_run_ = true;
    }
    if (tone_run_ && (period_ts > tone_end_ts_ || cal)) { tone_run_ = false; tone_active_.store(false); }
    const bool tone = tone_run_ && !cal;
    if (tone) {                                          // 1 kHz a pulsos (400 ms sí / 200 ms no) con fundidos de 8 ms
        if (tone_wave_.size() < frames) tone_wave_.resize(frames);
        for (uint32_t f = 0; f < frames; f++) {
            const double t_us = period_ts + (double)f * 1e6 / AUDIO_SAMPLE_RATE - tone_t0_;
            const double ph = fmod(t_us, 600000.0);
            double w = 0.0;
            if (ph < 400000.0) {
                w = 1.0;
                if (ph < 8000.0) w = 0.5 - 0.5 * cos(M_PI * ph / 8000.0);
                else if (ph > 392000.0) w = 0.5 - 0.5 * cos(M_PI * (400000.0 - ph) / 8000.0);
            }
            tone_wave_[f] = (float)(sin(2.0 * M_PI * 1000.0 * t_us / 1e6) * w) * tone_amp_;
        }
    }

    struct Route { bool active, mono; uint8_t role; sockaddr_in addr; uint8_t chl, chr; float gl, gr; int64_t lat_us; float eq[EQ_BANDS]; };
    Route rt[MAX_NODES];
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        for (int i = 0; i < MAX_NODES; i++) {
            const ESPNode& n = nodes_[i];
            rt[i].active = n.online && n.enabled && !n.ota_busy;
            rt[i].mono   = n.mono;
            rt[i].role   = n.role;
            rt[i].addr   = n.audio_addr;
            rt[i].chl    = n.ch_left;  rt[i].chr = n.ch_right;
            rt[i].gl     = n.volume_l / 255.0f * mv;
            rt[i].gr     = n.volume_r / 255.0f * mv;
            rt[i].lat_us = (int64_t)n.latency_offset_us;
            memcpy(rt[i].eq, n.eq_db, sizeof(rt[i].eq));
        }
    }

    // ── DSP: gestión de graves (crossover LR4) y EQ por altavoz. Se omite durante la calibración. ──
    // Con un subwoofer online: los satélites van con pasa-altos y sus graves se suman al sub (pasa-bajos).
    // Sin sub online los satélites reciben el canal completo (no se pierden los graves si el sub falla).
    const double FS = (double)AUDIO_SAMPLE_RATE;
    bool use_xo = false; uint32_t satMask = 0, lpMask = 0;
    if (!cal) {
        if (xover_on_.load(std::memory_order_relaxed))
            for (int i = 0; i < MAX_NODES; i++) if (rt[i].active && rt[i].role == ROLE_SUB) use_xo = true;
        if (use_xo) {
            for (int i = 0; i < MAX_NODES; i++) {
                if (!rt[i].active) continue;
                uint32_t m = (1u << rt[i].chl) | (rt[i].mono ? 0u : (1u << rt[i].chr));
                if (rt[i].role == ROLE_SAT) satMask |= m;
                else if (rt[i].role == ROLE_SUB) lpMask |= m;
            }
            lpMask |= satMask;
            const float fc = xover_hz_.load(std::memory_order_relaxed);
            if (fc != (float)dsp_.xo_fc) {
                for (int c = 0; c < AUDIO_CHANNELS; c++) { dsp_.hp[c].setHighpass(FS, fc); dsp_.lp[c].setLowpass(FS, fc); }
                dsp_.xo_fc = fc;
            }
            for (int c = 0; c < AUDIO_CHANNELS; c++) {                 // canal que entra en uso: estado limpio (sin transitorio)
                if (((satMask & ~dsp_.prev_hp) >> c) & 1u) dsp_.hp[c].reset();
                if (((lpMask  & ~dsp_.prev_lp) >> c) & 1u) dsp_.lp[c].reset();
            }
            dsp_.prev_hp = satMask; dsp_.prev_lp = lpMask;
            const size_t need = (size_t)frames * AUDIO_CHANNELS;
            if (dsp_hp_.size() < need) { dsp_hp_.resize(need); dsp_lp_.resize(need); }
            if (dsp_bass_.size() < frames) dsp_bass_.resize(frames);
            for (uint32_t f = 0; f < frames; f++) {
                const int32_t* fr = s32 + (size_t)f * AUDIO_CHANNELS;
                double bass = 0;
                for (int c = 0; c < AUDIO_CHANNELS; c++) {
                    if (!((lpMask >> c) & 1u)) continue;
                    double x = (double)(fr[c] >> 16);                 // Traktor: 24 bits en [31:8]
                    double l = dsp_.lp[c].run(x);
                    dsp_lp_[(size_t)f * AUDIO_CHANNELS + c] = (float)l;
                    if ((satMask >> c) & 1u) { bass += l; dsp_hp_[(size_t)f * AUDIO_CHANNELS + c] = (float)dsp_.hp[c].run(x); }
                }
                dsp_bass_[f] = (float)bass;
            }
        } else { dsp_.prev_hp = dsp_.prev_lp = 0; }
        static const float ZERO_EQ[EQ_BANDS] = {0, 0, 0, 0, 0, 0};          // EQ desactivado = filtros planos (identidad exacta)
        const bool eq_on = eq_on_.load(std::memory_order_relaxed);
        for (int i = 0; i < MAX_NODES; i++)
            if (rt[i].active) { const float* g = eq_on ? rt[i].eq : ZERO_EQ; dsp_.eq[i][0].set(g, FS); dsp_.eq[i][1].set(g, FS); }
    }
    // Graves (LFE) a los frontales: solo si nadie reproduce el canal LFE (3)
    bool lfe_mix = false; double lfe_g = 1.0;
    if (!cal && lfe_front_.load(std::memory_order_relaxed)) {
        bool covered = false;
        for (int i = 0; i < MAX_NODES; i++)
            if (rt[i].active && (rt[i].chl == 3 || (!rt[i].mono && rt[i].chr == 3))) covered = true;
        lfe_mix = !covered;
    }
    lfe_active_.store(lfe_mix, std::memory_order_relaxed);
    if (lfe_mix) {
        lfe_g = pow(10.0, (double)lfe_db_.load(std::memory_order_relaxed) / 20.0);
        if (!dsp_.lfe_prev) { dsp_.lfe_lp.reset(); dsp_.lfe_lp.setLowpass(FS, 120.0); }
        if (dsp_lfe_.size() < frames) dsp_lfe_.resize(frames);
        for (uint32_t f = 0; f < frames; f++) dsp_lfe_[f] = (float)dsp_.lfe_lp.run((double)(s32[(size_t)f * AUDIO_CHANNELS + 3] >> 16));
    }
    dsp_.lfe_prev = lfe_mix;
    // Modo noche: compresor enlazado sobre la salida (detector = pico de los 10 canales de entrada)
    const int night = night_.load(std::memory_order_relaxed);
    const bool night_on = !cal && !tone && night > 0;
    if (night_on) {
        static const double NT[4] = {0, -26, -30, -34}, NR[4] = {1, 2, 3, 4};
        const double T = NT[night & 3], R = NR[night & 3];
        const double aa = 1.0 - exp(-1.0 / (0.005 * FS)), ar = 1.0 - exp(-1.0 / (0.4 * FS));
        if (dsp_night_.size() < frames) dsp_night_.resize(frames);
        double env = dsp_.night_env_db;
        for (uint32_t f = 0; f < frames; f++) {
            const int32_t* fr = s32 + (size_t)f * AUDIO_CHANNELS;
            int32_t pk = 0;
            for (int c = 0; c < AUDIO_CHANNELS; c++) { int32_t v = fr[c] >> 16; if (v < 0) v = -v; if (v > pk) pk = v; }
            const double db = 20.0 * log10((pk < 1 ? 1.0 : (double)pk) / 32768.0);
            const double target = db > T ? (db - T) * (1.0 - 1.0 / R) : 0.0;
            env += (target - env) * (target > env ? aa : ar);
            dsp_night_[f] = (float)exp(-env * 0.11512925464970229);        // 10^(-env/20)
        }
        dsp_.night_env_db = env;
    } else dsp_.night_env_db = 0;

    auto clip16 = [](double v) -> int16_t {
        if (v >  32767.0) v =  32767.0;
        if (v < -32768.0) v = -32768.0;
        return (int16_t)lrint(v);
    };
    // Muestra de entrada (antes del EQ y del volumen) para el canal `ch` de la ruta `r` en el frame `f`.
    auto srcv = [&](const Route& r, uint32_t f, const int32_t* fr, uint8_t ch) -> double {
        if (use_xo) {
            if (r.role == ROLE_SAT) return (double)dsp_hp_[(size_t)f * AUDIO_CHANNELS + ch];
            if (r.role == ROLE_SUB) {
                double b = (double)dsp_bass_[f];
                return ((satMask >> ch) & 1u) ? b : b + (double)dsp_lp_[(size_t)f * AUDIO_CHANNELS + ch];
            }
        }
        double v = (double)(fr[ch] >> 16);
        if (lfe_mix && r.role == ROLE_FULL && ch < 2) v += (double)dsp_lfe_[f] * lfe_g;
        return v;
    };

    for (int i = 0; i < MAX_NODES; i++) {
        TxState& tx = tx_[i];
        if (!rt[i].active) { tx.n = 0; continue; }
        if (tx.mono != rt[i].mono) {                     // cambio de modo en caliente: descarta el paquete a medias
            tx.mono = rt[i].mono; tx.n = 0;
            tx.pkt.pkt_type = tx.mono ? PKT_AUDIO_MONO : PKT_AUDIO;
        }
        const uint32_t per_pkt = tx.mono ? AUDIO_PKT_SAMPLES : FRAMES_PER_PACKET;   // mono: 240 frames, estéreo: 120
        for (uint32_t f = 0; f < frames; f++) {
            if (tx.n == 0) {
                double ts = period_ts + (double)f * 1e6 / AUDIO_SAMPLE_RATE + (cal ? 0.0 : (double)rt[i].lat_us);
                tx.pkt.timestamp_us = ts > 0 ? (uint64_t)ts : 0;
            }
            if (tone && (i == tone_run_id_ || tone_run_solo_)) {   // tono de prueba: solo en ese altavoz (y silencio en el resto si «solo»)
                const bool me = (i == tone_run_id_);
                const double w = me ? (double)tone_wave_[f] : 0.0;
                if (tx.mono) tx.pkt.samples[tx.n] = clip16(w * rt[i].gl);
                else {
                    tx.pkt.samples[tx.n * 2]     = clip16(tone_run_side_ != 1 ? w * rt[i].gl : 0.0);
                    tx.pkt.samples[tx.n * 2 + 1] = clip16(tone_run_side_ != 0 ? w * rt[i].gr : 0.0);
                }
            } else if (cal) {                            // test: mono, sin volumen ni offsets
                int16_t v = (cal_who_[f] == i) ? (int16_t)lrintf(cal_wave_[f]) : 0;
                if (tx.mono) tx.pkt.samples[tx.n] = v;
                else         tx.pkt.samples[tx.n * 2] = tx.pkt.samples[tx.n * 2 + 1] = v;
            } else {
                const int32_t* fr = s32 + (size_t)f * AUDIO_CHANNELS;
                const double ng = night_on ? (double)dsp_night_[f] : 1.0;
                double l = dsp_.eq[i][0].run(srcv(rt[i], f, fr, rt[i].chl)) * rt[i].gl * ng;
                if (tx.mono) {
                    tx.pkt.samples[tx.n] = clip16(l);
                } else {
                    double r = dsp_.eq[i][1].run(srcv(rt[i], f, fr, rt[i].chr)) * rt[i].gr * ng;
                    tx.pkt.samples[tx.n * 2]     = clip16(l);
                    tx.pkt.samples[tx.n * 2 + 1] = clip16(r);
                }
            }
            if (++tx.n == per_pkt) {
                tx.pkt.seq = tx.seq++;
                {                                        // historial para atender los NACK del ESP
                    std::lock_guard<std::mutex> lk(hist_mtx_[i]);
                    HistSlot& h = hist_[i][tx.pkt.seq & (TX_HISTORY_PACKETS - 1)];
                    h.seq = tx.pkt.seq; h.used = true; h.pkt = tx.pkt;
                }
                if (sendto(audio_sock_, &tx.pkt, sizeof(tx.pkt), MSG_DONTWAIT,
                           (const sockaddr*)&rt[i].addr, sizeof(rt[i].addr)) < 0)
                    send_errors_.fetch_add(1, std::memory_order_relaxed);
                else
                    pkts_sent_.fetch_add(1, std::memory_order_relaxed);
                tx.n = 0;
            }
        }
    }
}

// ─── Config ───────────────────────────────────────────────────────────────────
std::array<ESPNode, MAX_NODES> UdpStreamer::getNodes() const {
    std::array<ESPNode, MAX_NODES> out;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        out = nodes_;
    }
    for (int i = 0; i < MAX_NODES; i++) out[i].retx_sent = retx_[i].load(std::memory_order_relaxed);
    return out;
}

bool UdpStreamer::setNodeConfig(uint8_t id, uint8_t ch_l, uint8_t ch_r,
                                uint8_t vol_l, uint8_t vol_r, bool enabled, bool mono) {
    if (id >= MAX_NODES || ch_l >= AUDIO_CHANNELS || ch_r >= AUDIO_CHANNELS) return false;
    if (mono) { ch_r = ch_l; vol_r = vol_l; }            // mono: un solo canal y un solo volumen
    ESPNode snap; bool structural = false;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        ESPNode& n = nodes_[id];
        structural = (n.ch_left != ch_l || n.ch_right != ch_r || n.enabled != enabled || n.mono != mono);
        n.ch_left = ch_l; n.ch_right = ch_r;
        n.volume_l = vol_l; n.volume_r = vol_r; n.enabled = enabled; n.mono = mono;
        snap = n;
    }
    if (structural) layout_.store(LAYOUT_CUSTOM);        // el mapa de canales ya no coincide con un preajuste (volúmenes no cuentan)
    sendConfig(snap);
    dirty_.store(true);
    return true;
}

bool UdpStreamer::setLayout(int layout) {
    if (layout != LAYOUT_STEREO5 && layout != LAYOUT_MONO10) return false;
    std::array<ESPNode, MAX_NODES> snap;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        for (uint8_t i = 0; i < MAX_NODES; i++) {
            ESPNode& n = nodes_[i];
            layoutDefaults(layout, i, n.ch_left, n.ch_right, n.mono, n.enabled);
            if (n.mono) n.volume_r = n.volume_l;         // volúmenes y latencia se conservan
        }
        snap = nodes_;
    }
    layout_.store(layout);
    for (const auto& n : snap) sendConfig(n);
    dirty_.store(true);
    fprintf(stderr, "[Control] Distribución: %s\n", layout == LAYOUT_MONO10 ? "10 ESP mono (1 por canal)" : "5 ESP estéreo");
    return true;
}

bool UdpStreamer::setLatencyOffset(uint8_t id, int32_t us) {
    if (id >= MAX_NODES) return false;
    us = std::max(-500000, std::min(500000, us));
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    nodes_[id].latency_offset_us = us;
    dirty_.store(true);
    return true;
}

bool UdpStreamer::forgetNode(uint8_t id) {
    if (id >= MAX_NODES) return false;
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    if (nodes_[id].online) return false;
    nodes_[id].known = false;
    nodes_[id].mac = 0;                  // la casilla queda libre para otro ESP (o para el mismo, si vuelve)
    dirty_.store(true);
    return true;
}
bool UdpStreamer::setNodeRole(uint8_t id, uint8_t role) {
    if (id >= MAX_NODES || role > ROLE_SUB) return false;
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    nodes_[id].role = role;
    dirty_.store(true);
    return true;
}
bool UdpStreamer::setNodePos(uint8_t id, float x, float y, int side) {
    if (id >= MAX_NODES || side < 0 || side > 1 || !std::isfinite(x) || !std::isfinite(y)) return false;
    const bool autopos = (x < 0 && y < 0);
    if (!autopos && (x < 0 || x > 100 || y < 0 || y > 100)) return false;
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    (side ? nodes_[id].pos2_x : nodes_[id].pos_x) = autopos ? -1.0f : x;
    (side ? nodes_[id].pos2_y : nodes_[id].pos_y) = autopos ? -1.0f : y;
    dirty_.store(true);
    return true;
}
bool UdpStreamer::setNodeEq(uint8_t id, const float g[EQ_BANDS]) {
    if (id >= MAX_NODES) return false;
    for (int b = 0; b < EQ_BANDS; b++) if (!std::isfinite(g[b]) || std::fabs(g[b]) > EQ_MAX_DB) return false;
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (int b = 0; b < EQ_BANDS; b++) nodes_[id].eq_db[b] = g[b];
    dirty_.store(true);
    return true;
}
bool UdpStreamer::setXoverHz(float hz) {
    if (!(hz >= XOVER_MIN_HZ && hz <= XOVER_MAX_HZ)) return false;
    xover_hz_.store(hz); dirty_.store(true);
    return true;
}
bool UdpStreamer::setLfeDb(float db) {
    if (!(db >= -12.0f && db <= 10.0f)) return false;
    lfe_db_.store(db); dirty_.store(true);
    return true;
}
bool UdpStreamer::setNightLevel(int lv) {
    if (lv < 0 || lv > 3) return false;
    night_.store(lv); dirty_.store(true);
    return true;
}
bool UdpStreamer::setNodeDist(uint8_t id, float m) {
    if (id >= MAX_NODES || !std::isfinite(m)) return false;
    if (m >= 0 && (m < 0.2f || m > 30.0f)) return false;
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    nodes_[id].dist_m = m < 0 ? -1.0f : m;
    dirty_.store(true);
    return true;
}
bool UdpStreamer::setLipsyncExtraMs(int ms) {
    if (ms < -50 || ms > 200) return false;
    lipsync_extra_ms_.store(ms); dirty_.store(true);
    return true;
}

// ─── Tono de prueba (API para el hilo web) ────────────────────────────────────
bool UdpStreamer::toneStart(uint8_t id, int side, int ms, bool solo, float level_db) {
    if (id >= MAX_NODES || side < 0 || side > 2 || ms < 1000 || ms > 10000 || !(level_db >= -40.0f && level_db <= -6.0f)) return false;
    if (cal_state_.load() != 0) return false;            // no durante la calibración
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        if (!nodes_[id].online || !nodes_[id].enabled) return false;
    }
    tone_id_.store(id); tone_side_.store(side); tone_solo_.store(solo); tone_dur_ms_.store(ms); tone_level_db_.store(level_db);
    tone_end_wall_us_.store(mono_us() + (uint64_t)ms * 1000ull);
    tone_active_.store(true);
    tone_req_.store(true);
    return true;
}
void UdpStreamer::toneStop() { tone_stop_req_.store(true); tone_active_.store(false); }
int64_t UdpStreamer::toneRemainingMs() const {
    if (!tone_active_.load()) return 0;
    const int64_t r = (int64_t)tone_end_wall_us_.load() - (int64_t)mono_us();
    return r > 0 ? r / 1000 : 0;
}

// ─── OTA ──────────────────────────────────────────────────────────────────────
bool UdpStreamer::sendOta(uint8_t id, uint16_t http_port, uint32_t size, uint64_t token) {
    if (id >= MAX_NODES || ctrl_sock_ < 0) return false;
    sockaddr_in to;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        ESPNode& n = nodes_[id];
        if (!n.online || n.ota_busy) return false;
        n.ota_busy = true; n.ota_until_us = mono_us() + OTA_WAIT_US;
        to = n.ctrl_addr;
    }
    OtaPacket o{};
    o.magic = PROTOCOL_MAGIC; o.pkt_type = PKT_OTA; o.esp_id = id;
    o.http_port = http_port; o.size = size; o.token = token;
    for (int k = 0; k < 3; k++) sendto(ctrl_sock_, &o, sizeof(o), 0, (const sockaddr*)&to, sizeof(to));   // UDP: se repite por si se pierde
    fprintf(stderr, "[OTA] ESP%d: orden de actualización enviada (%u bytes)\n", id, size);
    return true;
}
void UdpStreamer::cancelOta(uint8_t id) {
    if (id >= MAX_NODES) return;
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    nodes_[id].ota_busy = false;
}

// ─── Calibración (API para el hilo web) ───────────────────────────────────────
bool UdpStreamer::calStart(const std::vector<uint8_t>& seq, float level_db) {
    if (seq.empty() || seq.size() > CAL_MAX_SLOTS || cal_state_.load() != 0) return false;
    for (uint8_t id : seq) if (id >= MAX_NODES) return false;
    if (!(level_db >= -40.0f && level_db <= -3.0f)) return false;
    {
        std::lock_guard<std::mutex> lk(cal_mtx_);
        cal_req_seq_ = seq;
        cal_req_level_db_ = level_db;
    }
    cal_state_.store(1);                                 // ocupado desde ya
    int64_t total = CAL_LEAD_US + (int64_t)seq.size() * CAL_SLOT_US + CAL_TAIL_US;
    cal_end_wall_us_.store(mono_us() + (uint64_t)total);
    cal_req_.store(true);
    return true;
}
void UdpStreamer::calStop() { cal_stop_req_.store(true); }
int64_t UdpStreamer::calRemainingMs() const {
    if (!cal_state_.load()) return 0;
    int64_t r = (int64_t)cal_end_wall_us_.load() - (int64_t)mono_us();
    return r > 0 ? r / 1000 : 0;
}

// ─── Persistencia ─────────────────────────────────────────────────────────────
// Fichero de texto key=value (sin dependencias). Claves desconocidas o valores inválidos se ignoran.
bool UdpStreamer::saveConfig() {
    if (cfg_path_.empty()) return false;
    auto nodes = getNodes();
    std::string tmp = cfg_path_ + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) { fprintf(stderr, "[Config] No se puede escribir %s: %s\n", tmp.c_str(), strerror(errno)); return false; }
    fprintf(f, "version=4\nmaster_volume=%.4f\nplayout_us=%u\nlayout=%d\nxover_hz=%.1f\nlipsync_extra_ms=%d\ndsp_xover=%d\ndsp_eq=%d\n",
            master_vol_.load(), playout_us_.load(), layout_.load(), xover_hz_.load(), lipsync_extra_ms_.load(),
            xover_on_.load() ? 1 : 0, eq_on_.load() ? 1 : 0);
    fprintf(f, "dsp_night=%d\ndsp_lfe=%d\nlfe_db=%.1f\n", night_.load(), lfe_front_.load() ? 1 : 0, lfe_db_.load());
    for (const auto& n : nodes)
        fprintf(f, "node%d.ch_left=%d\nnode%d.ch_right=%d\nnode%d.volume_l=%d\nnode%d.volume_r=%d\n"
                   "node%d.enabled=%d\nnode%d.mono=%d\nnode%d.latency_offset_us=%d\n",
                n.id, n.ch_left, n.id, n.ch_right, n.id, n.volume_l, n.id, n.volume_r,
                n.id, n.enabled ? 1 : 0, n.id, n.mono ? 1 : 0, n.id, n.latency_offset_us);
    for (const auto& n : nodes) {
        fprintf(f, "node%d.role=%d\nnode%d.known=%d\nnode%d.pos_x=%.2f\nnode%d.pos_y=%.2f\nnode%d.pos2_x=%.2f\nnode%d.pos2_y=%.2f\n", n.id, n.role, n.id, n.known ? 1 : 0, n.id, n.pos_x, n.id, n.pos_y, n.id, n.pos2_x, n.id, n.pos2_y);
        fprintf(f, "node%d.dist_m=%.2f\n", n.id, n.dist_m);
        if (n.mac) fprintf(f, "node%d.mac=%llu\n", n.id, (unsigned long long)n.mac);
        for (int b = 0; b < EQ_BANDS; b++) fprintf(f, "node%d.eq%d=%.2f\n", n.id, b, n.eq_db[b]);
    }
    bool ok = (fflush(f) == 0) & (ferror(f) == 0);
    fclose(f);
    if (!ok || rename(tmp.c_str(), cfg_path_.c_str()) != 0) {      // atómico: nunca queda un fichero a medias
        fprintf(stderr, "[Config] Error guardando %s\n", cfg_path_.c_str()); return false;
    }
    return true;
}

bool UdpStreamer::loadConfig() {
    if (cfg_path_.empty()) return false;
    FILE* f = fopen(cfg_path_.c_str(), "r");
    if (!f) return false;                                          // primera ejecución: normal
    char line[160]; int applied = 0;
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; const char* key = line; char* val = eq + 1;
        char* nl = strpbrk(val, "\r\n"); if (nl) *nl = 0;
        char* end = nullptr;
        double dv = strtod(val, &end);
        if (end == val || *end != 0 || !std::isfinite(dv)) continue;
        if (!strcmp(key, "master_volume")) { if (dv >= 0 && dv <= 2) { master_vol_.store((float)dv); applied++; } continue; }
        if (!strcmp(key, "layout"))        { if (dv >= 0 && dv <= 2 && dv == (int)dv) { layout_.store((int)dv); applied++; } continue; }
        if (!strcmp(key, "dsp_xover"))     { xover_on_.store(dv != 0); applied++; continue; }
        if (!strcmp(key, "dsp_eq"))        { eq_on_.store(dv != 0); applied++; continue; }
        if (!strcmp(key, "dsp_night"))     { if (dv >= 0 && dv <= 3 && dv == (int)dv) { night_.store((int)dv); applied++; } continue; }
        if (!strcmp(key, "dsp_lfe"))       { lfe_front_.store(dv != 0); applied++; continue; }
        if (!strcmp(key, "lfe_db"))        { if (dv >= -12 && dv <= 10) { lfe_db_.store((float)dv); applied++; } continue; }
        if (!strcmp(key, "xover_hz"))      { if (dv >= XOVER_MIN_HZ && dv <= XOVER_MAX_HZ) { xover_hz_.store((float)dv); applied++; } continue; }
        if (!strcmp(key, "lipsync_extra_ms")) { if (dv >= -50 && dv <= 200 && dv == (int)dv) { lipsync_extra_ms_.store((int)dv); applied++; } continue; }
        if (!strcmp(key, "playout_us"))    { if (dv >= PLAYOUT_MIN_US && dv <= PLAYOUT_MAX_US) { playout_us_.store((uint32_t)dv); applied++; } continue; }
        int id = -1; char field[32] = {0};
        if (sscanf(key, "node%d.%31s", &id, field) != 2 || id < 0 || id >= MAX_NODES) continue;
        ESPNode& n = nodes_[id];
        long v = (long)dv;
        if      (!strcmp(field, "ch_left")  && v >= 0 && v < AUDIO_CHANNELS) { n.ch_left  = (uint8_t)v; applied++; }
        else if (!strcmp(field, "ch_right") && v >= 0 && v < AUDIO_CHANNELS) { n.ch_right = (uint8_t)v; applied++; }
        else if (!strcmp(field, "volume_l") && v >= 0 && v <= 255) { n.volume_l = (uint8_t)v; applied++; }
        else if (!strcmp(field, "volume_r") && v >= 0 && v <= 255) { n.volume_r = (uint8_t)v; applied++; }
        else if (!strcmp(field, "known"))    { n.known = (v != 0); applied++; }
        else if (!strcmp(field, "mac") && dv >= 0 && dv < 281474976710656.0) { n.mac = (uint64_t)dv; applied++; }
        else if (!strcmp(field, "dist_m") && (dv < 0 || (dv >= 0.2 && dv <= 30))) { n.dist_m = dv < 0 ? -1.0f : (float)dv; applied++; }
        else if (!strcmp(field, "pos_x") && dv >= -1 && dv <= 100) { n.pos_x = (float)dv; applied++; }
        else if (!strcmp(field, "pos_y") && dv >= -1 && dv <= 100) { n.pos_y = (float)dv; applied++; }
        else if (!strcmp(field, "pos2_x") && dv >= -1 && dv <= 100) { n.pos2_x = (float)dv; applied++; }
        else if (!strcmp(field, "pos2_y") && dv >= -1 && dv <= 100) { n.pos2_y = (float)dv; applied++; }
        else if (!strcmp(field, "role") && v >= 0 && v <= ROLE_SUB) { n.role = (uint8_t)v; applied++; }
        else if (!strncmp(field, "eq", 2) && field[2] >= '0' && field[2] < '0' + EQ_BANDS && field[3] == 0 && std::fabs(dv) <= EQ_MAX_DB)
                                             { n.eq_db[field[2] - '0'] = (float)dv; applied++; }
        else if (!strcmp(field, "enabled"))  { n.enabled = (v != 0); applied++; }
        else if (!strcmp(field, "mono"))     { n.mono = (v != 0); if (n.mono) { n.ch_right = n.ch_left; n.volume_r = n.volume_l; } applied++; }
        else if (!strcmp(field, "latency_offset_us") && v >= -500000 && v <= 500000) { n.latency_offset_us = (int32_t)v; applied++; }
    }
    fclose(f);
    fprintf(stderr, "[Config] %s cargado (%d valores)\n", cfg_path_.c_str(), applied);
    loadProfiles();
    return true;
}

// ─── Perfiles guardados ───────────────────────────────────────────────────────
// Fichero aparte (<config>.profiles, key=value): pN.name=..., pN.master=..., pN.nI.vl=... Se escribe al guardar/borrar un perfil.
static std::string cleanName(const std::string& in) {
    std::string o;
    for (unsigned char c : in) { if (c >= 0x20 && c != '=' && c != 0x7f) o += (char)c; }
    size_t a = o.find_first_not_of(' '), b = o.find_last_not_of(' ');
    if (a == std::string::npos) return "";
    o = o.substr(a, b - a + 1);
    // limita a 24 caracteres UTF-8 sin cortar a la mitad
    size_t n = 0, chars = 0;
    while (n < o.size() && chars < 24) { n++; while (n < o.size() && ((unsigned char)o[n] & 0xC0) == 0x80) n++; chars++; }
    return o.substr(0, n);
}
std::vector<std::string> UdpStreamer::profileNames() const {
    std::lock_guard<std::mutex> lk(prof_mtx_);
    std::vector<std::string> r;
    for (const auto& p : profiles_) r.push_back(p.name);
    return r;
}
bool UdpStreamer::profileSave(const std::string& name_in, std::string& err) {
    const std::string name = cleanName(name_in);
    if (name.empty()) { err = "pon un nombre al perfil"; return false; }
    Profile pr;
    pr.name = name; pr.master = master_vol_.load(); pr.xover_hz = xover_hz_.load();
    pr.xover_on = xover_on_.load(); pr.eq_on = eq_on_.load(); pr.lfe_front = lfe_front_.load(); pr.lfe_db = lfe_db_.load(); pr.night = night_.load();
    const auto nodes = getNodes();
    for (int i = 0; i < MAX_NODES; i++) {
        pr.n[i].vl = nodes[i].volume_l; pr.n[i].vr = nodes[i].volume_r; pr.n[i].role = nodes[i].role; pr.n[i].lat = nodes[i].latency_offset_us;
        for (int b = 0; b < EQ_BANDS; b++) pr.n[i].eq[b] = nodes[i].eq_db[b];
    }
    int idx = -1;
    {
        std::lock_guard<std::mutex> lk(prof_mtx_);
        for (size_t k = 0; k < profiles_.size(); k++) if (profiles_[k].name == name) idx = (int)k;
        if (idx < 0) {
            if ((int)profiles_.size() >= MAX_PROFILES) { err = "máximo " + std::to_string(MAX_PROFILES) + " perfiles: borra alguno"; return false; }
            profiles_.push_back(pr); idx = (int)profiles_.size() - 1;
        } else profiles_[idx] = pr;
    }
    profile_active_.store(idx);
    saveProfiles();
    return true;
}
bool UdpStreamer::profileLoad(int idx) {
    Profile pr;
    {
        std::lock_guard<std::mutex> lk(prof_mtx_);
        if (idx < 0 || idx >= (int)profiles_.size()) return false;
        pr = profiles_[idx];
    }
    std::array<ESPNode, MAX_NODES> snap;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        for (int i = 0; i < MAX_NODES; i++) {
            ESPNode& n = nodes_[i];
            n.volume_l = pr.n[i].vl; n.volume_r = n.mono ? pr.n[i].vl : pr.n[i].vr;
            n.latency_offset_us = pr.n[i].lat; n.role = pr.n[i].role;
            for (int b = 0; b < EQ_BANDS; b++) n.eq_db[b] = pr.n[i].eq[b];
        }
        snap = nodes_;
    }
    master_vol_.store(pr.master); xover_hz_.store(pr.xover_hz);
    xover_on_.store(pr.xover_on); eq_on_.store(pr.eq_on); lfe_front_.store(pr.lfe_front); lfe_db_.store(pr.lfe_db); night_.store(pr.night);
    for (const auto& n : snap) sendConfig(n);
    profile_active_.store(idx);
    dirty_.store(true);
    fprintf(stderr, "[Perfil] Cargado «%s»\n", pr.name.c_str());
    return true;
}
bool UdpStreamer::profileDelete(int idx) {
    {
        std::lock_guard<std::mutex> lk(prof_mtx_);
        if (idx < 0 || idx >= (int)profiles_.size()) return false;
        profiles_.erase(profiles_.begin() + idx);
    }
    const int a = profile_active_.load();
    if (a == idx) profile_active_.store(-1); else if (a > idx) profile_active_.store(a - 1);
    saveProfiles();
    return true;
}
bool UdpStreamer::saveProfiles() {
    if (cfg_path_.empty()) return false;
    const std::string path = cfg_path_ + ".profiles", tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return false;
    {
        std::lock_guard<std::mutex> lk(prof_mtx_);
        fprintf(f, "version=1\ncount=%zu\n", profiles_.size());
        for (size_t k = 0; k < profiles_.size(); k++) {
            const Profile& p = profiles_[k];
            fprintf(f, "p%zu.name=%s\np%zu.master=%.4f\np%zu.xover_hz=%.1f\np%zu.xover_on=%d\np%zu.eq_on=%d\np%zu.lfe_front=%d\np%zu.lfe_db=%.1f\np%zu.night=%d\n",
                    k, p.name.c_str(), k, p.master, k, p.xover_hz, k, p.xover_on ? 1 : 0, k, p.eq_on ? 1 : 0, k, p.lfe_front ? 1 : 0, k, p.lfe_db, k, p.night);
            for (int i = 0; i < MAX_NODES; i++) {
                fprintf(f, "p%zu.n%d.vl=%d\np%zu.n%d.vr=%d\np%zu.n%d.role=%d\np%zu.n%d.lat=%d\n", k, i, p.n[i].vl, k, i, p.n[i].vr, k, i, p.n[i].role, k, i, p.n[i].lat);
                for (int b = 0; b < EQ_BANDS; b++) fprintf(f, "p%zu.n%d.eq%d=%.2f\n", k, i, b, p.n[i].eq[b]);
            }
        }
    }
    const bool ok = (fflush(f) == 0) & (ferror(f) == 0);
    fclose(f);
    if (!ok || rename(tmp.c_str(), path.c_str()) != 0) { fprintf(stderr, "[Perfil] Error guardando %s\n", path.c_str()); return false; }
    return true;
}
bool UdpStreamer::loadProfiles() {
    if (cfg_path_.empty()) return false;
    FILE* f = fopen((cfg_path_ + ".profiles").c_str(), "r");
    if (!f) return false;
    std::vector<Profile> v;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; const char* key = line; char* val = eq + 1;
        char* nl = strpbrk(val, "\r\n"); if (nl) *nl = 0;
        unsigned k = 0; char rest[48] = {0};
        if (sscanf(key, "p%u.%47s", &k, rest) != 2 || k >= (unsigned)MAX_PROFILES) continue;
        if (v.size() <= k) v.resize(k + 1);
        Profile& p = v[k];
        if (!strcmp(rest, "name")) { p.name = cleanName(val); continue; }
        char* end = nullptr; const double dv = strtod(val, &end);
        if (end == val || *end != 0 || !std::isfinite(dv)) continue;
        if      (!strcmp(rest, "master")   && dv >= 0 && dv <= 2) p.master = (float)dv;
        else if (!strcmp(rest, "xover_hz") && dv >= XOVER_MIN_HZ && dv <= XOVER_MAX_HZ) p.xover_hz = (float)dv;
        else if (!strcmp(rest, "xover_on")) p.xover_on = dv != 0;
        else if (!strcmp(rest, "eq_on"))    p.eq_on = dv != 0;
        else if (!strcmp(rest, "lfe_front")) p.lfe_front = dv != 0;
        else if (!strcmp(rest, "lfe_db")   && dv >= -12 && dv <= 10) p.lfe_db = (float)dv;
        else if (!strcmp(rest, "night")    && dv >= 0 && dv <= 3) p.night = (int)dv;
        else {
            int i = -1; char fld[16] = {0};
            if (sscanf(rest, "n%d.%15s", &i, fld) != 2 || i < 0 || i >= MAX_NODES) continue;
            if      (!strcmp(fld, "vl")   && dv >= 0 && dv <= 255) p.n[i].vl = (uint8_t)dv;
            else if (!strcmp(fld, "vr")   && dv >= 0 && dv <= 255) p.n[i].vr = (uint8_t)dv;
            else if (!strcmp(fld, "role") && dv >= 0 && dv <= ROLE_SUB) p.n[i].role = (uint8_t)dv;
            else if (!strcmp(fld, "lat")  && dv >= -500000 && dv <= 500000) p.n[i].lat = (int32_t)dv;
            else if (!strncmp(fld, "eq", 2) && fld[2] >= '0' && fld[2] < '0' + EQ_BANDS && fld[3] == 0 && std::fabs(dv) <= EQ_MAX_DB) p.n[i].eq[fld[2] - '0'] = (float)dv;
        }
    }
    fclose(f);
    v.erase(std::remove_if(v.begin(), v.end(), [](const Profile& p) { return p.name.empty(); }), v.end());
    std::lock_guard<std::mutex> lk(prof_mtx_);
    profiles_ = v;
    return true;
}
