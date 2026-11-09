#include "input_mixer.h"
#include <alsa/asoundlib.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <time.h>
#include <pthread.h>

static uint64_t nowUs() { timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts); return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull; }
static std::string jesc2(const std::string& s) {
    std::string o;
    for (unsigned char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += (char)c; } else if (c >= 0x20) o += (char)c; }
    return o;
}
static std::string low(std::string x) { for (auto& c : x) c = (char)tolower((unsigned char)c); return x; }
static bool contains(const std::string& hay, const std::string& needle) { return !needle.empty() && low(hay).find(low(needle)) != std::string::npos; }

bool InputConfig::operator==(const InputConfig& o) const { return InputMixer::serialize(*this) == InputMixer::serialize(o); }

// ── Configuración ──
std::string InputMixer::serialize(const InputConfig& c) {
    std::string s = "v2\nclock|" + c.clock + "\n";
    for (const auto& d : c.devs) s += "dev|" + d.key + "|" + std::to_string(d.channels) + "|" + (d.enabled ? "1" : "0") + "|" + d.tag + "\n";
    return s;
}
bool InputMixer::save(const std::string& path, const InputConfig& c) {
    const std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return false;
    const std::string s = serialize(c);
    fwrite(s.data(), 1, s.size(), f);
    const bool ok = (fflush(f) == 0) & (ferror(f) == 0);
    fclose(f);
    return ok && rename(tmp.c_str(), path.c_str()) == 0;
}
static std::vector<std::string> splitBar(const std::string& l) {
    std::vector<std::string> v; size_t a = 0;
    while (true) { size_t b = l.find('|', a); if (b == std::string::npos) { v.push_back(l.substr(a)); break; } v.push_back(l.substr(a, b - a)); a = b + 1; }
    return v;
}
bool InputMixer::load(const std::string& path, InputConfig& c) {
    c = InputConfig();
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    std::vector<std::string> lines; char line[300];
    while (fgets(line, sizeof(line), f)) { std::string l(line); while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back(); lines.push_back(l); }
    fclose(f);
    if (!lines.empty() && lines[0] == "v2") {
        for (const auto& l : lines) {
            const auto p = splitBar(l);
            if (p.size() == 2 && p[0] == "clock") c.clock = p[1];
            else if (p.size() == 5 && p[0] == "dev") {
                InputDev d; d.key = p[1]; d.channels = (unsigned)strtoul(p[2].c_str(), nullptr, 10); d.enabled = p[3] == "1"; d.tag = p[4];
                if (d.channels >= 1 && d.channels <= (unsigned)MAX_INPUTS) c.devs.push_back(d);
            }
        }
        return true;
    }
    // Versión anterior: «dispositivo|canales» por interfaz extra + «@traktor|pos|master»; la Traktor pasa a ser una placa más
    int pos = 0, master = 1;
    std::vector<InputDev> ex;
    for (const auto& l : lines) {
        if (!l.empty() && l[0] == '@') { int p = 0, m = 1; if (sscanf(l.c_str(), "@traktor|%d|%d", &p, &m) == 2) { pos = std::max(0, p); master = m; } continue; }
        const size_t bar = l.rfind('|');
        if (bar == std::string::npos || bar == 0) continue;
        InputDev d; d.key = l.substr(0, bar); d.channels = (unsigned)strtoul(l.c_str() + bar + 1, nullptr, 10); d.enabled = true;
        if (d.channels >= 1 && d.channels <= (unsigned)MAX_INPUTS) ex.push_back(d);
    }
    InputDev t; t.key = "Traktor"; t.channels = AUDIO_CHANNELS; t.enabled = true;
    ex.insert(ex.begin() + std::min<size_t>((size_t)pos, ex.size()), t);
    c.devs = ex; c.clock = master ? "Traktor" : "rpi";
    return true;
}
bool InputMixer::validate(InputConfig& c, std::string& err) {
    if (c.devs.size() > 12) { err = "demasiadas placas"; return false; }
    unsigned total = 0;
    for (size_t i = 0; i < c.devs.size(); i++) {
        auto& d = c.devs[i];
        if (d.key.empty() || d.key.size() > 64) { err = "falta el nombre de una placa"; return false; }
        for (unsigned char ch : d.key) if (ch < 0x20 || ch == '|' || ch == '"' || ch == '\\' || ch == '<' || ch == '>') { err = "nombre de placa no válido"; return false; }
        if (d.channels < 1 || d.channels > (unsigned)MAX_INPUTS) { err = "los canales de cada placa van de 1 a " + std::to_string(MAX_INPUTS); return false; }
        for (size_t j = 0; j < i; j++) if (c.devs[j].key == d.key) { err = "placa repetida: " + d.key; return false; }
        if (d.enabled) total += d.channels;
    }
    if (total > (unsigned)MAX_INPUTS) { err = "máximo " + std::to_string(MAX_INPUTS) + " canales activos en total"; return false; }
    for (auto& d : c.devs) {                                          // letras únicas A..Z; las que falten se asignan
        const bool ok = d.tag.size() == 1 && d.tag[0] >= 'A' && d.tag[0] <= 'Z';
        bool dup = false;
        if (ok) for (const auto& o : c.devs) if (&o != &d && o.tag == d.tag && &o < &d) dup = true;
        if (!ok || dup) d.tag.clear();
    }
    for (auto& d : c.devs) if (d.tag.empty()) {
        for (char l = 'A'; l <= 'Z'; l++) { bool used = false; for (const auto& o : c.devs) if (o.tag == std::string(1, l)) used = true; if (!used) { d.tag = std::string(1, l); break; } }
    }
    if (!c.clock.empty() && c.clock != "rpi") {
        bool ok = false; for (const auto& d : c.devs) if (d.enabled && d.key == c.clock) ok = true;
        if (!ok) { err = "el reloj principal debe ser una placa activa o la Raspberry"; return false; }
    }
    return true;
}
static std::string jfield(const std::string& o, const char* k) {      // valor (cadena, número o bool) de un campo simple
    const std::string q = std::string("\"") + k + "\"";
    size_t p = o.find(q);
    if (p == std::string::npos) return "";
    p = o.find(':', p); if (p == std::string::npos) return "";
    p++; while (p < o.size() && o[p] == ' ') p++;
    if (p < o.size() && o[p] == '"') { size_t e = o.find('"', p + 1); return e == std::string::npos ? "" : o.substr(p + 1, e - p - 1); }
    size_t e = p; while (e < o.size() && o[e] != ',' && o[e] != '}' && o[e] != ' ') e++;
    return o.substr(p, e - p);
}
bool InputMixer::parseJson(const std::string& b, InputConfig& out, std::string& err) {
    out = InputConfig();
    const size_t k = b.find("\"devices\"");
    const size_t lb = k == std::string::npos ? k : b.find('[', k);
    if (lb == std::string::npos) { err = "falta devices (lista de placas)"; return false; }
    size_t pos = lb + 1;
    while (true) {
        const size_t ob = b.find('{', pos), rb = b.find(']', pos);
        if (ob == std::string::npos || (rb != std::string::npos && rb < ob)) break;
        const size_t cb = b.find('}', ob);
        if (cb == std::string::npos) { err = "JSON incompleto"; return false; }
        const std::string o = b.substr(ob, cb - ob + 1);
        InputDev d; d.key = jfield(o, "key"); d.channels = (unsigned)strtoul(jfield(o, "channels").c_str(), nullptr, 10);
        d.enabled = jfield(o, "enabled") == "true"; d.tag = jfield(o, "tag");
        out.devs.push_back(d);
        pos = cb + 1;
        if (out.devs.size() > 12) { err = "demasiadas placas"; return false; }
    }
    // «clock» va después de la lista (o antes): se busca fuera de los objetos de placa
    std::string rest = b;
    for (size_t a = rest.find('['); a != std::string::npos; a = rest.find('[')) { const size_t e = rest.find(']', a); if (e == std::string::npos) break; rest.erase(a, e - a + 1); }
    out.clock = jfield(rest, "clock");
    return validate(out, err);
}

// ── Tarjetas ──
std::vector<CardInfo> InputMixer::listCards() {
    std::vector<CardInfo> v;
    int card = -1;
    while (snd_card_next(&card) >= 0 && card >= 0) {
        char* nm = nullptr;
        CardInfo c; c.index = card; c.id = "hw:" + std::to_string(card) + ",0";
        if (snd_card_get_name(card, &nm) == 0 && nm) { c.name = nm; free(nm); }
        snd_pcm_t* p = nullptr;
        const int r = snd_pcm_open(&p, c.id.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
        if (r == 0) {
            snd_pcm_hw_params_t* hw; snd_pcm_hw_params_alloca(&hw);
            if (snd_pcm_hw_params_any(p, hw) >= 0) { unsigned mx = 0; if (snd_pcm_hw_params_get_channels_max(hw, &mx) >= 0) c.max_channels = (int)mx; }
            snd_pcm_close(p);
        } else if (r == -EBUSY) c.busy = true;
        else continue;                                    // sin entrada de captura
        v.push_back(c);
    }
    return v;
}
int InputMixer::matchCard(const std::string& key, const std::vector<CardInfo>& cards) {
    for (size_t i = 0; i < cards.size(); i++) if (cards[i].id == key || cards[i].name == key) return (int)i;
    for (size_t i = 0; i < cards.size(); i++) if (key.compare(0, 3, "hw:") != 0 && contains(cards[i].name, key)) return (int)i;
    return -1;
}
InputConfig InputMixer::defaultConfig(const std::vector<CardInfo>& cards) {
    InputConfig c;
    int t = matchCard("Traktor", cards);
    if (t >= 0) { InputDev d; d.key = "Traktor"; d.channels = AUDIO_CHANNELS; d.enabled = true; c.devs.push_back(d); }
    else if (cards.size() == 1) {                          // una sola placa conectada: se usa esa (y su reloj)
        InputDev d; d.key = cards[0].name.empty() ? cards[0].id : cards[0].name;
        d.channels = (unsigned)std::max(1, std::min(cards[0].max_channels ? cards[0].max_channels : 2, (int)MAX_INPUTS)); d.enabled = true; c.devs.push_back(d);
    }
    std::string e; validate(c, e);
    return c;
}
std::string InputMixer::effectiveClock(const InputConfig& c) {
    if (c.clock == "rpi") return "rpi";
    if (!c.clock.empty()) return c.clock;
    const InputDev* one = nullptr; int n = 0;
    for (const auto& d : c.devs) if (d.enabled) { one = &d; n++; }
    return n == 1 ? one->key : "rpi";
}
std::vector<std::string> InputMixer::idsFor(const std::string& tag, unsigned channels) {
    std::vector<std::string> v;
    for (unsigned i = 0; i < channels; i++) v.push_back(tag + std::to_string(i + 1));
    return v;
}
std::string InputMixer::resolve(const std::string& key) {
    if (key.compare(0, 3, "hw:") == 0 || key.compare(0, 7, "plughw:") == 0) return key;
    const auto cards = listCards();
    const int i = matchCard(key, cards);
    return i >= 0 ? cards[(size_t)i].id : std::string();
}

// ── Mezclador ──
InputMixer::InputMixer(UdpStreamer& s, unsigned block_frames) : s_(s), block_(block_frames ? block_frames : 240) {}
InputMixer::~InputMixer() { stop(); }

void InputMixer::configure(const InputConfig& c) {
    dev_.clear();
    clock_ = effectiveClock(c);
    int first = 0;
    std::vector<std::string> ids;
    for (const auto& s : c.devs) {
        if (!s.enabled) continue;
        if (first + (int)s.channels > MAX_INPUTS) continue;
        auto d = std::make_unique<Dev>();
        d->spec = s; d->first = first; d->master = (s.key == clock_);
        d->ring.assign((size_t)48000 / 5 * s.channels, 0);       // 200 ms
        for (const auto& id : idsFor(s.tag, s.channels)) ids.push_back(id);
        first += (int)s.channels;
        dev_.push_back(std::move(d));
    }
    total_ = first < AUDIO_CHANNELS ? AUDIO_CHANNELS : first;    // mínimo 10 entradas (las que sobren, en silencio)
    s_.setInputChannels(total_, ids);
    out_.assign((size_t)block_ * total_, 0);
}
int InputMixer::firstOf(size_t i) const { return i < dev_.size() ? dev_[i]->first : -1; }

void InputMixer::setErr(Dev& d, const std::string& s) { std::lock_guard<std::mutex> lk(d.mtx); d.error = s; }
bool InputMixer::openDev(Dev& d) {
    const std::string dev = resolve(d.spec.key);
    if (dev.empty()) { setErr(d, "no está conectada"); return false; }
    AudioConfig c; c.device = dev; c.channels = d.spec.channels; c.period_size = block_;
    auto cap = std::make_unique<AudioCapture>(c);
    if (!cap->open()) { setErr(d, "no se pudo abrir a 48 kHz con " + std::to_string(d.spec.channels) + " canales (¿ocupada o con otros canales?)"); return false; }
    Dev* dp = &d;
    cap->setCallback([this, dp](const int32_t* data, uint32_t n, uint64_t ts) { onBlock(*dp, data, n, ts); });
    if (!cap->start()) { setErr(d, "no arranca"); return false; }
    { std::lock_guard<std::mutex> lk(d.mtx); d.resolved = dev; d.error.clear(); d.fill = 0; d.primed = false; }
    d.cap = std::move(cap);
    d.online = true;
    fprintf(stderr, "[Entradas] %s (%s): %u canales → entradas %d..%d%s\n", d.spec.key.c_str(), dev.c_str(), d.spec.channels, d.first + 1, d.first + (int)d.spec.channels, d.master ? " [reloj principal]" : "");
    return true;
}
void InputMixer::onBlock(Dev& d, const int32_t* data, uint32_t frames, uint64_t ts) {
    if (d.master) { last_master_us_.store(nowUs(), std::memory_order_relaxed); mix(&d, data, frames, ts); }
    else push(d, data, frames);
}
void InputMixer::push(Dev& d, const int32_t* data, uint32_t frames) {
    const unsigned ch = d.spec.channels;
    std::lock_guard<std::mutex> lk(d.mtx);
    const size_t cap = d.ring.size() / ch;
    if (d.fill + frames > cap) {                           // desborda: tira lo más viejo
        const size_t drop = std::min<size_t>(d.fill + frames - cap, d.fill);
        memmove(d.ring.data(), d.ring.data() + drop * ch, (d.fill - drop) * ch * sizeof(int32_t));
        d.fill -= drop; d.dropped += drop;
    }
    memcpy(d.ring.data() + d.fill * ch, data, (size_t)frames * ch * sizeof(int32_t));
    d.fill += frames;
}
void InputMixer::injectForTest(size_t idx, const int32_t* data, uint32_t frames, bool as_master, uint64_t ts) {
    if (idx >= dev_.size()) return;
    Dev& d = *dev_[idx];
    d.test = true; d.online = true;
    if (as_master && d.master) onBlock(d, data, frames, ts); else push(d, data, frames);
}
void InputMixer::pull(Dev& d, uint32_t frames) {
    const unsigned ch = d.spec.channels; const int T = total_;
    std::lock_guard<std::mutex> lk(d.mtx);
    // Colchón objetivo: 2 bloques. Si hay de más (su reloj va más rápido) se recorta hasta el objetivo;
    // si falta, se rellena con silencio y se vuelve a llenar el colchón antes de seguir.
    const size_t target = (size_t)frames * 2, high = (size_t)frames * 6;
    if (!d.primed && d.fill >= target) d.primed = true;
    if (d.primed && d.fill > high) { const size_t drop = d.fill - target; memmove(d.ring.data(), d.ring.data() + drop * ch, (d.fill - drop) * ch * sizeof(int32_t)); d.fill -= drop; d.dropped += drop; }
    size_t take = 0;
    if (d.primed) { take = std::min<size_t>(frames, d.fill); if (take < frames) { d.underruns++; d.primed = false; } }
    for (uint32_t f = 0; f < frames; f++) {
        int32_t* o = &out_[(size_t)f * T + d.first];
        if (f < take) memcpy(o, d.ring.data() + (size_t)f * ch, ch * sizeof(int32_t));
        else memset(o, 0, ch * sizeof(int32_t));
    }
    if (take) { memmove(d.ring.data(), d.ring.data() + take * ch, (d.fill - take) * ch * sizeof(int32_t)); d.fill -= take; d.blocks++; }
}
void InputMixer::mix(Dev* master, const int32_t* mdata, uint32_t frames, uint64_t ts) {
    std::lock_guard<std::mutex> mk(mix_mtx_);
    if (out_.size() < (size_t)frames * total_) out_.assign((size_t)frames * total_, 0);
    const int T = total_;
    for (uint32_t f = 0; f < frames; f++) {
        int32_t* o = &out_[(size_t)f * T];
        memset(o, 0, (size_t)T * sizeof(int32_t));
        if (master) memcpy(o + master->first, mdata + (size_t)f * master->spec.channels, master->spec.channels * sizeof(int32_t));
    }
    for (auto& d : dev_) if (d.get() != master && !d->master) pull(*d, frames);
    s_.pushFrames(out_.data(), frames, ts);
}

bool InputMixer::start() {
    if (running_) return false;
    running_ = true;
    for (auto& d : dev_) openDev(*d);                      // las que fallen se reintentan en el hilo de vigilancia
    watch_ = std::thread(&InputMixer::watchLoop, this);
    clock_th_ = std::thread([this] {                       // reloj propio: siempre (clock=rpi) o cuando la placa que manda no entrega
        sched_param sp{}; sp.sched_priority = 70; pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
        uint64_t next = nowUs();
        while (running_) {
            const uint64_t n = nowUs();
            if (next > n) { timespec ts{0, (long)((next - n) * 1000)}; nanosleep(&ts, nullptr); }
            if (!running_) break;
            const bool master_ok = clock_ != "rpi" && last_master_us_.load() != 0 && nowUs() - last_master_us_.load() < 100000;
            if (!master_ok) mix(nullptr, nullptr, block_, next);
            next += (uint64_t)block_ * 1000000ull / AUDIO_SAMPLE_RATE;
            if (nowUs() > next + 100000) next = nowUs();
        }
    });
    return true;
}
void InputMixer::stop() {
    const bool was = running_.exchange(false);
    if (clock_th_.joinable()) clock_th_.join();
    if (watch_.joinable()) watch_.join();
    if (was) for (auto& d : dev_) if (d->cap) { d->cap->stop(); d->cap->close(); }
}
void InputMixer::watchLoop() {
    while (running_) {
        for (int i = 0; i < 20 && running_; i++) { timespec ts{0, 100000000}; nanosleep(&ts, nullptr); }
        for (auto& dp : dev_) {
            Dev& d = *dp;
            if (d.test) continue;
            if (d.cap && !d.cap->isRunning()) {            // desenchufada o error: cerrar y reintentar
                d.online = false; setErr(d, "se desconectó");
                d.cap->stop(); d.cap->close(); d.cap.reset();
                fprintf(stderr, "[Entradas] %s se desconectó\n", d.spec.key.c_str());
            }
            if (!d.cap && nowUs() >= d.next_try_us) { d.next_try_us = nowUs() + 3000000ull; openDev(d); }
        }
    }
}

std::string InputMixer::statusJson() const {
    std::ostringstream o;
    o << "{\"devices\":[";
    for (size_t i = 0; i < dev_.size(); i++) {
        const Dev& d = *dev_[i];
        std::lock_guard<std::mutex> lk(d.mtx);
        o << (i ? "," : "") << "{\"key\":\"" << jesc2(d.spec.key) << "\",\"online\":" << (d.online.load() ? "true" : "false")
          << ",\"first\":" << d.first << ",\"resolved\":\"" << jesc2(d.resolved) << "\",\"error\":\"" << jesc2(d.error) << "\",\"master\":" << (d.master ? "true" : "false")
          << ",\"fill_ms\":" << (d.fill * 1000.0 / AUDIO_SAMPLE_RATE) << ",\"underruns\":" << d.underruns << ",\"dropped_ms\":" << (d.dropped * 1000.0 / AUDIO_SAMPLE_RATE) << "}";
    }
    o << "],\"clock\":\"" << jesc2(clock_) << "\",\"total\":" << total_ << "}";
    return o.str();
}
