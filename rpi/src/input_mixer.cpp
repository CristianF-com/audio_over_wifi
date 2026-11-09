#include "input_mixer.h"
#include <alsa/asoundlib.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <sstream>
#include <time.h>
#include <pthread.h>

static uint64_t nowUs() { timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts); return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull; }
static std::string jesc2(const std::string& s) {
    std::string o;
    for (unsigned char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += (char)c; } else if (c >= 0x20) o += (char)c; }
    return o;
}
static bool ieq(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return false;
    auto low = [](std::string x) { for (auto& c : x) c = (char)tolower((unsigned char)c); return x; };
    return low(hay).find(low(needle)) != std::string::npos;
}

InputMixer::InputMixer(UdpStreamer& s, unsigned block_frames) : s_(s), block_(block_frames ? block_frames : 240) {
    zero_.assign((size_t)block_ * 2, 0);
}
InputMixer::~InputMixer() { stop(); }

std::vector<ExtraSpec> InputMixer::loadFile(const std::string& path) {
    std::vector<ExtraSpec> v;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return v;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        std::string l(line);
        if (!l.empty() && l[0] == '@') continue;                 // @traktor|pos|master
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        const size_t bar = l.rfind('|');
        if (bar == std::string::npos || bar == 0) continue;
        ExtraSpec e; e.device = l.substr(0, bar);
        e.channels = (unsigned)strtoul(l.c_str() + bar + 1, nullptr, 10);
        if (e.channels >= 1 && e.channels <= (unsigned)MAX_INPUTS) v.push_back(e);
    }
    fclose(f);
    return v;
}
void InputMixer::loadTraktor(const std::string& path, int& pos, bool& master) {
    pos = 0; master = true;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int p = 0, m = 1;
        if (sscanf(line, "@traktor|%d|%d", &p, &m) == 2) { pos = p < 0 ? 0 : (p > 8 ? 8 : p); master = m != 0; }
    }
    fclose(f);
}
void InputMixer::parseTraktor(const std::string& b, int& pos, bool& master) {
    const size_t k = b.find("\"traktor\"");
    if (k == std::string::npos) return;
    const size_t ob = b.find('{', k), cb = ob == std::string::npos ? ob : b.find('}', ob);
    if (cb == std::string::npos) return;
    const std::string o = b.substr(ob, cb - ob + 1);
    size_t q = o.find("\"pos\"");
    if (q != std::string::npos) { const long v = strtol(o.c_str() + o.find(':', q) + 1, nullptr, 10); pos = v < 0 ? 0 : (v > 8 ? 8 : (int)v); }
    q = o.find("\"master\"");
    if (q != std::string::npos) master = o.find("true", q) != std::string::npos;
}
bool InputMixer::saveFile(const std::string& path, const std::vector<ExtraSpec>& v, int traktor_pos, bool traktor_master) {
    const std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return false;
    if (traktor_pos != 0 || !traktor_master) fprintf(f, "@traktor|%d|%d\n", traktor_pos, traktor_master ? 1 : 0);
    for (const auto& e : v) fprintf(f, "%s|%u\n", e.device.c_str(), e.channels);
    const bool ok = (fflush(f) == 0) & (ferror(f) == 0);
    fclose(f);
    return ok && rename(tmp.c_str(), path.c_str()) == 0;
}
bool InputMixer::validate(std::vector<ExtraSpec>& v, std::string& err, bool traktor_present) {
    unsigned total = 0;
    const unsigned maxTotal = traktor_present ? (unsigned)(MAX_INPUTS - AUDIO_CHANNELS) : (unsigned)MAX_INPUTS;   // sin Traktor las extras ocupan desde la entrada 1
    const std::string trk = AudioCapture::findCard("traktor");
    for (size_t i = 0; i < v.size(); i++) {
        auto& e = v[i];
        for (unsigned char c : e.device) if (c < 0x20 || c == '|' || c == '"' || c == '\\' || c == '<' || c == '>') { err = "nombre de dispositivo no válido"; return false; }
        if (e.device.empty() || e.device.size() > 64) { err = "falta el dispositivo"; return false; }
        if (e.channels < 1 || e.channels > (unsigned)MAX_INPUTS) { err = "los canales de cada interfaz van de 1 a " + std::to_string(MAX_INPUTS); return false; }
        if (ieq(e.device, "traktor") || (!trk.empty() && e.device == trk)) { err = "la Traktor ya es la entrada principal"; return false; }
        for (size_t j = 0; j < i; j++) if (v[j].device == e.device) { err = "interfaz repetida: " + e.device; return false; }
        total += e.channels;
    }
    if (total > maxTotal) { err = "máximo " + std::to_string(maxTotal) + " canales extra en total" + (traktor_present ? " (con la Traktor)" : ""); return false; }
    return true;
}
bool InputMixer::parseJson(const std::string& b, std::vector<ExtraSpec>& out, std::string& err, bool traktor_present) {
    out.clear();
    const size_t k = b.find("\"extras\"");
    const size_t lb = k == std::string::npos ? k : b.find('[', k);
    if (lb == std::string::npos) { err = "falta extras (lista de interfaces)"; return false; }
    size_t pos = lb + 1;
    while (true) {
        const size_t ob = b.find('{', pos);
        const size_t rb = b.find(']', pos);
        if (ob == std::string::npos || (rb != std::string::npos && rb < ob)) break;
        const size_t cb = b.find('}', ob);
        if (cb == std::string::npos) { err = "JSON incompleto"; return false; }
        const std::string o = b.substr(ob, cb - ob + 1);
        ExtraSpec e;
        size_t d = o.find("\"device\"");
        if (d != std::string::npos) { d = o.find('"', o.find(':', d)); size_t q = d == std::string::npos ? d : o.find('"', d + 1); if (q != std::string::npos) e.device = o.substr(d + 1, q - d - 1); }
        size_t c = o.find("\"channels\"");
        e.channels = c == std::string::npos ? 0 : (unsigned)strtoul(o.c_str() + o.find(':', c) + 1, nullptr, 10);
        out.push_back(e);
        pos = cb + 1;
        if (out.size() > 8) { err = "demasiadas interfaces"; return false; }
    }
    return validate(out, err, traktor_present);
}

std::vector<CardInfo> InputMixer::listCards() {
    std::vector<CardInfo> v;
    int card = -1;
    while (snd_card_next(&card) >= 0 && card >= 0) {
        char* nm = nullptr;
        CardInfo c; c.index = card; c.id = "hw:" + std::to_string(card) + ",0";
        if (snd_card_get_name(card, &nm) == 0 && nm) { c.name = nm; free(nm); }
        c.traktor = ieq(c.name, "traktor");
        snd_pcm_t* p = nullptr;
        const int r = snd_pcm_open(&p, c.id.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
        if (r == 0) {
            snd_pcm_hw_params_t* hw; snd_pcm_hw_params_alloca(&hw);
            if (snd_pcm_hw_params_any(p, hw) >= 0) { unsigned mx = 0; if (snd_pcm_hw_params_get_channels_max(hw, &mx) >= 0) c.max_channels = (int)mx; }
            snd_pcm_close(p);
        } else if (r == -EBUSY) c.busy = true;
        else continue;                                    // tarjeta sin entrada de captura
        v.push_back(c);
    }
    return v;
}

void InputMixer::setExtras(const std::vector<ExtraSpec>& v) {
    ex_.clear(); trk_.reset();
    // Orden de entradas: las extras en el orden de la lista y la Traktor insertada en su posición (si está conectada).
    // Sin Traktor, las extras toman las entradas 1, 2, 3…
    std::vector<int> order;                                       // índice en v, o -1 = Traktor
    for (size_t i = 0; i < v.size(); i++) order.push_back((int)i);
    if (traktor_) order.insert(order.begin() + std::min<size_t>((size_t)trk_pos_, order.size()), -1);
    int first = 0;
    std::vector<std::unique_ptr<Extra>> byspec(v.size());
    std::vector<std::string> glob;                                // nombre de cada entrada global (las 10 primeras usan los editables)
    for (int o : order) {
        const int n = o < 0 ? AUDIO_CHANNELS : (int)v[(size_t)o].channels;
        if (first + n > MAX_INPUTS) continue;
        if (o < 0) {
            trk_first_ = first;
            for (int c = 0; c < n; c++) glob.push_back("TK" + std::to_string(c + 1));
            if (!trk_master_) {
                trk_ = std::make_unique<Extra>();
                trk_->spec.device = "@traktor"; trk_->spec.channels = AUDIO_CHANNELS; trk_->first = first; trk_->test = true;
                trk_->ring.assign((size_t)48000 / 5 * AUDIO_CHANNELS, 0);
                trk_->online = true;
            }
        } else {
            auto e = std::make_unique<Extra>();
            e->spec = v[(size_t)o]; e->first = first;
            e->ring.assign((size_t)48000 / 5 * e->spec.channels, 0);       // 200 ms
            for (int c = 0; c < n; c++) glob.push_back("USB" + std::to_string(o + 1) + "-" + std::to_string(c + 1));
            byspec[(size_t)o] = std::move(e);
        }
        first += n;
    }
    for (auto& e : byspec) if (e) ex_.push_back(std::move(e));
    if (!traktor_) trk_first_ = 0;
    total_ = first < AUDIO_CHANNELS ? AUDIO_CHANNELS : first;    // mínimo 10 entradas (las que sobren, en silencio)
    std::vector<std::string> names;                                // nombres de las entradas ≥ 10
    for (size_t g = AUDIO_CHANNELS; g < glob.size(); g++) names.push_back(glob[g]);
    s_.setInputChannels(total_, names);
    out_.assign((size_t)block_ * total_, 0);
}

void InputMixer::setErr(Extra& e, const std::string& s) { std::lock_guard<std::mutex> lk(e.mtx); e.error = s; }
bool InputMixer::openExtra(Extra& e) {
    std::string dev = e.spec.device;
    if (dev.compare(0, 3, "hw:") != 0 && dev.compare(0, 7, "plughw:") != 0) {
        dev = AudioCapture::findCard(dev.c_str());
        if (dev.empty()) { setErr(e, "no se encuentra la tarjeta"); return false; }
    }
    const std::string trk = AudioCapture::findCard("traktor");
    if (!trk.empty() && dev == trk) { setErr(e, "es la Traktor"); return false; }
    AudioConfig c; c.device = dev; c.channels = e.spec.channels; c.period_size = block_;
    auto cap = std::make_unique<AudioCapture>(c);
    if (!cap->open()) { setErr(e, "no se pudo abrir a 48 kHz con " + std::to_string(e.spec.channels) + " canales (¿ocupada o con otros canales?)"); return false; }
    Extra* ep = &e;
    cap->setCallback([this, ep](const int32_t* d, uint32_t n, uint64_t) { push(*ep, d, n); });
    if (!cap->start()) { setErr(e, "no arranca"); return false; }
    { std::lock_guard<std::mutex> lk(e.mtx); e.resolved = dev; e.error.clear(); }
    e.cap = std::move(cap);
    { std::lock_guard<std::mutex> lk(e.mtx); e.fill = 0; e.primed = false; }
    e.online = true;
    fprintf(stderr, "[Entradas] interfaz extra %s: %u canales → entradas %d..%d\n", dev.c_str(), e.spec.channels, e.first + 1, e.first + (int)e.spec.channels);
    return true;
}

void InputMixer::push(Extra& e, const int32_t* d, uint32_t frames) {
    const unsigned ch = e.spec.channels;
    std::lock_guard<std::mutex> lk(e.mtx);
    const size_t cap = e.ring.size() / ch;
    if (e.fill + frames > cap) {                           // desborda: tira lo más viejo
        const size_t drop = e.fill + frames - cap;
        memmove(e.ring.data(), e.ring.data() + drop * ch, (e.fill - std::min(drop, e.fill)) * ch * sizeof(int32_t));
        e.fill -= std::min(drop, e.fill); e.dropped += drop;
    }
    memcpy(e.ring.data() + e.fill * ch, d, (size_t)frames * ch * sizeof(int32_t));
    e.fill += frames;
}
void InputMixer::injectForTest(size_t idx, const int32_t* data, uint32_t frames) {
    if (idx >= ex_.size()) return;
    ex_[idx]->test = true; ex_[idx]->online = true;
    push(*ex_[idx], data, frames);
}

void InputMixer::pull(Extra& e, uint32_t frames, int T) {
    const unsigned ch = e.spec.channels;
    std::lock_guard<std::mutex> lk(e.mtx);
    // Objetivo de colchón: 2 bloques. Si hay de más (su reloj va más rápido) se recorta hasta el objetivo;
    // si falta, se rellena con silencio y se vuelve a llenar el colchón antes de seguir.
    const size_t target = (size_t)frames * 2, high = (size_t)frames * 6;
    if (!e.primed && e.fill >= target) e.primed = true;
    if (e.primed && e.fill > high) { const size_t drop = e.fill - target; memmove(e.ring.data(), e.ring.data() + drop * ch, (e.fill - drop) * ch * sizeof(int32_t)); e.fill -= drop; e.dropped += drop; }
    size_t take = 0;
    if (e.primed) {
        take = std::min<size_t>(frames, e.fill);
        if (take < frames) { e.underruns++; e.primed = false; }
    }
    for (uint32_t f = 0; f < frames; f++) {
        int32_t* o = &out_[(size_t)f * T + e.first];
        if (f < take) memcpy(o, e.ring.data() + (size_t)f * ch, ch * sizeof(int32_t));
        else memset(o, 0, ch * sizeof(int32_t));
    }
    if (take) { memmove(e.ring.data(), e.ring.data() + take * ch, (e.fill - take) * ch * sizeof(int32_t)); e.fill -= take; e.blocks++; }
}
void InputMixer::mix(const int32_t* trk, uint32_t frames, uint64_t ts) {
    std::lock_guard<std::mutex> mk(mix_mtx_);
    if (out_.size() < (size_t)frames * total_) out_.assign((size_t)frames * total_, 0);
    const int T = total_;
    for (uint32_t f = 0; f < frames; f++) {
        int32_t* o = &out_[(size_t)f * T];
        memset(o, 0, (size_t)T * sizeof(int32_t));
        if (trk) memcpy(o + trk_first_, trk + (size_t)f * AUDIO_CHANNELS, AUDIO_CHANNELS * sizeof(int32_t));
    }
    if (trk_) pull(*trk_, frames, T);
    for (auto& ep : ex_) pull(*ep, frames, T);
    s_.pushFrames(out_.data(), frames, ts);
}

void InputMixer::onTraktor(const int32_t* data, uint32_t frames, uint64_t ts) {
    last_traktor_us_.store(nowUs(), std::memory_order_relaxed);
    if (trk_) { push(*trk_, data, frames); return; }               // Traktor sin ser el reloj: entra por su colchón
    mix(data, frames, ts);
}

bool InputMixer::start() {
    if (running_) return false;
    running_ = true;
    for (auto& e : ex_) openExtra(*e);                     // las que fallen se reintentan en el hilo de vigilancia
    watch_ = std::thread(&InputMixer::watchLoop, this);
    if (!traktor_ || trk_) {                               // sin Traktor, o Traktor que no manda el reloj: reloj propio cada bloque
        clock_ = std::thread([this] {
            sched_param sp{}; sp.sched_priority = 70; pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
            uint64_t next = nowUs();
            while (running_) {
                const uint64_t n = nowUs();
                if (next > n) { timespec ts{0, (long)((next - n) * 1000)}; nanosleep(&ts, nullptr); }
                if (!running_) break;
                if (trk_ || last_traktor_us_.load() == 0 || nowUs() - last_traktor_us_.load() > 100000) mix(nullptr, block_, next);
                next += (uint64_t)block_ * 1000000ull / AUDIO_SAMPLE_RATE;
                if (nowUs() > next + 100000) next = nowUs();
            }
        });
    }
    return true;
}
void InputMixer::stop() {
    if (!running_.exchange(false)) { if (watch_.joinable()) watch_.join(); if (clock_.joinable()) clock_.join(); return; }
    if (clock_.joinable()) clock_.join();
    if (watch_.joinable()) watch_.join();
    for (auto& e : ex_) if (e->cap) { e->cap->stop(); e->cap->close(); }
}
void InputMixer::watchLoop() {
    while (running_) {
        for (int i = 0; i < 20 && running_; i++) { timespec ts{0, 100000000}; nanosleep(&ts, nullptr); }
        for (auto& ep : ex_) {
            Extra& e = *ep;
            if (e.test) continue;
            if (e.cap && !e.cap->isRunning()) {            // desenchufada o error: cerrar y reintentar
                e.online = false; setErr(e, "se desconectó");
                e.cap->stop(); e.cap->close(); e.cap.reset();
                fprintf(stderr, "[Entradas] %s se desconectó\n", e.spec.device.c_str());
            }
            if (!e.cap && nowUs() >= e.next_try_us) { e.next_try_us = nowUs() + 3000000ull; openExtra(e); }
        }
    }
}

std::string InputMixer::statusJson() const {
    std::ostringstream o;
    o << "{\"extras\":[";
    for (size_t i = 0; i < ex_.size(); i++) {
        const Extra& e = *ex_[i];
        std::lock_guard<std::mutex> lk(e.mtx);
        o << (i ? "," : "") << "{\"device\":\"" << jesc2(e.spec.device) << "\",\"channels\":" << e.spec.channels
          << ",\"first\":" << e.first << ",\"online\":" << (e.online.load() ? "true" : "false")
          << ",\"resolved\":\"" << jesc2(e.resolved) << "\",\"error\":\"" << jesc2(e.error) << "\""
          << ",\"fill_ms\":" << (e.fill * 1000.0 / AUDIO_SAMPLE_RATE) << ",\"underruns\":" << e.underruns << ",\"dropped_ms\":" << (e.dropped * 1000.0 / AUDIO_SAMPLE_RATE) << "}";
    }
    o << "],\"traktor\":{\"first\":" << trk_first_ << ",\"master\":" << (trk_master_ ? "true" : "false");
    if (trk_) { std::lock_guard<std::mutex> lk(trk_->mtx); o << ",\"underruns\":" << trk_->underruns << ",\"dropped_ms\":" << (trk_->dropped * 1000.0 / AUDIO_SAMPLE_RATE); }
    o << "},\"total\":" << total_ << "}";
    return o.str();
}
