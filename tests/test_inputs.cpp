// Entradas de audio: configuración, ids/nombres de canal, reloj principal y mezcla de varias placas (sin ALSA ni hardware).
#include "input_mixer.h"
#include "udp_streamer.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
static int fails = 0;
#define CHECK(n, c) do { bool _c = (c); printf("%s %s\n", _c ? "PASS" : "FAIL", n); if (!_c) fails++; } while (0)
static const uint32_t P = 240;
static InputDev dev(const char* k, unsigned ch, bool en, const char* tag = "") { InputDev d; d.key = k; d.channels = ch; d.enabled = en; d.tag = tag; return d; }
static std::vector<int32_t> blk(unsigned ch, int which, int32_t amp) {      // bloque con señal solo en el canal «which»
    std::vector<int32_t> v((size_t)P * ch, 0);
    for (uint32_t f = 0; f < P; f++) v[(size_t)f * ch + (size_t)which] = amp;
    return v;
}

int main() {
    // ── JSON y validación ──
    InputConfig c; std::string err;
    CHECK("JSON válido: 2 placas, reloj en la primera", InputMixer::parseJson("{\"devices\":[{\"key\":\"Traktor\",\"channels\":10,\"enabled\":true,\"tag\":\"A\"},{\"key\":\"Scarlett\",\"channels\":8,\"enabled\":false,\"tag\":\"B\"}],\"clock\":\"Traktor\"}", c, err)
          && c.devs.size() == 2 && c.devs[0].key == "Traktor" && c.devs[0].enabled && !c.devs[1].enabled && c.devs[1].channels == 8 && c.clock == "Traktor");
    CHECK("lista vacía válida", InputMixer::parseJson("{\"devices\":[],\"clock\":\"\"}", c, err) && c.devs.empty());
    CHECK("sin devices → error", !InputMixer::parseJson("{}", c, err));
    CHECK("las letras que faltan se asignan (A, B) y no se repiten", InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":2,\"enabled\":true},{\"key\":\"y\",\"channels\":2,\"enabled\":true,\"tag\":\"A\"},{\"key\":\"z\",\"channels\":2,\"enabled\":true,\"tag\":\"A\"}],\"clock\":\"\"}", c, err)
          && c.devs[0].tag != c.devs[1].tag && c.devs[1].tag == "A" && c.devs[2].tag != "A" && c.devs[0].tag != c.devs[2].tag);
    CHECK("canales 0 rechazado", !InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":0,\"enabled\":true}],\"clock\":\"\"}", c, err));
    CHECK("33 canales rechazado", !InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":33,\"enabled\":true}],\"clock\":\"\"}", c, err));
    CHECK("más de 32 canales ACTIVOS rechazado; con una desactivada se admite", !InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":20,\"enabled\":true},{\"key\":\"y\",\"channels\":20,\"enabled\":true}],\"clock\":\"\"}", c, err)
          && InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":20,\"enabled\":true},{\"key\":\"y\",\"channels\":20,\"enabled\":false}],\"clock\":\"\"}", c, err));
    CHECK("placa repetida rechazada", !InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":2,\"enabled\":true},{\"key\":\"x\",\"channels\":2,\"enabled\":true}],\"clock\":\"\"}", c, err));
    CHECK("nombre con | o comillas rechazado", !InputMixer::parseJson("{\"devices\":[{\"key\":\"a|b\",\"channels\":2,\"enabled\":true}],\"clock\":\"\"}", c, err));
    CHECK("reloj: una placa desactivada no puede ser el reloj principal", !InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":2,\"enabled\":false}],\"clock\":\"x\"}", c, err));
    CHECK("reloj «rpi» válido", InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":2,\"enabled\":true}],\"clock\":\"rpi\"}", c, err) && c.clock == "rpi");
    CHECK("reloj que no existe rechazado", !InputMixer::parseJson("{\"devices\":[{\"key\":\"x\",\"channels\":2,\"enabled\":true}],\"clock\":\"zzz\"}", c, err));

    // ── Fichero ──
    const char* path = "/tmp/sonos_inputs_test.inputs"; remove(path);
    InputConfig r;
    CHECK("sin fichero: load devuelve false", !InputMixer::load(path, r));
    InputConfig a; a.devs = {dev("Traktor", 10, true, "A"), dev("UMC1820", 6, false, "B")}; a.clock = "rpi";
    CHECK("guardar y recargar (incluye «Usar» y reloj)", InputMixer::save(path, a) && InputMixer::load(path, r) && r == a && !r.devs[1].enabled && r.clock == "rpi" && r.devs[1].channels == 6);
    { FILE* f = fopen(path, "w"); fputs("hw:2,0|8\nScarlett|2\n@traktor|1|0\n", f); fclose(f); }
    CHECK("fichero de la versión anterior: la Traktor pasa a ser una placa más (en su posición) y rpi si no era el reloj", InputMixer::load(path, r) && r.devs.size() == 3 && r.devs[0].key == "hw:2,0" && r.devs[1].key == "Traktor" && r.devs[1].channels == 10 && r.devs[2].key == "Scarlett" && r.clock == "rpi" && r.devs[0].enabled);
    { FILE* f = fopen(path, "w"); fputs("hw:2,0|2\n", f); fclose(f); }
    CHECK("fichero antiguo sin @traktor: la Traktor primera y reloj principal", InputMixer::load(path, r) && r.devs[0].key == "Traktor" && r.clock == "Traktor" && r.devs.size() == 2);

    // ── Tarjetas, valores de fábrica y reloj automático ──
    std::vector<CardInfo> trk(1), two(2), one(1);
    trk[0].name = "Traktor Audio 10"; trk[0].id = "hw:1,0"; trk[0].max_channels = 10;
    two[0].name = "Scarlett 18i20"; two[0].id = "hw:2,0"; two[0].max_channels = 18; two[1].name = "USB Audio Device"; two[1].id = "hw:3,0"; two[1].max_channels = 2;
    one[0] = two[1];
    CHECK("tarjeta «Traktor» se reconoce por parte del nombre", InputMixer::matchCard("Traktor", trk) == 0 && InputMixer::matchCard("hw:1,0", trk) == 0 && InputMixer::matchCard("Scarlett", trk) < 0);
    { InputConfig d = InputMixer::defaultConfig(trk); CHECK("de fábrica con la Traktor: solo ella, activa, 10 canales y su reloj", d.devs.size() == 1 && d.devs[0].key == "Traktor" && d.devs[0].enabled && d.devs[0].channels == 10 && d.devs[0].tag == "A" && InputMixer::effectiveClock(d) == "Traktor"); }
    { InputConfig d = InputMixer::defaultConfig(one); CHECK("de fábrica sin Traktor y con UNA placa: se usa esa y su reloj", d.devs.size() == 1 && d.devs[0].key == "USB Audio Device" && d.devs[0].channels == 2 && d.devs[0].enabled && InputMixer::effectiveClock(d) == "USB Audio Device"); }
    CHECK("de fábrica con varias placas y sin Traktor: ninguna (hay que elegir)", InputMixer::defaultConfig(two).devs.empty());
    { InputConfig d; d.devs = {dev("a", 2, true, "A"), dev("b", 2, false, "B")}; CHECK("reloj automático con una sola placa ACTIVA = esa", InputMixer::effectiveClock(d) == "a");
      d.devs[1].enabled = true; CHECK("reloj automático con varias activas = la Raspberry", InputMixer::effectiveClock(d) == "rpi");
      d.clock = "b"; CHECK("reloj elegido", InputMixer::effectiveClock(d) == "b"); }

    // ── Ids y nombres de canal ──
    {
        UdpStreamer s;
        CHECK("por defecto: ids A1…A10 y nombres de siempre", s.channelIds().size() == 10 && s.channelIds()[0] == "A1" && s.channelIds()[9] == "A10" && s.channelNames()[0] == "FL" && s.channelNames()[9] == "AUX-R");
        s.setInputChannels(12, {"A1","A2","A3","A4","A5","A6","A7","A8","A9","A10","B1","B2"});
        CHECK("ids con 2 placas: A1…A10, B1, B2", s.channelIds().size() == 12 && s.channelIds()[10] == "B1" && s.channelIds()[11] == "B2");
        CHECK("nombres de fábrica por canal local: B1 = FL, B2 = FR", s.channelNames()[10] == "FL" && s.channelNames()[11] == "FR");
        std::vector<std::string> n = s.channelNames(); n[0] = "Sala"; n[11] = "Micro";
        std::string e;
        CHECK("se renombran TODOS los canales activos (no solo 10)", s.setChannelNames(n, e) && s.channelNames()[0] == "Sala" && s.channelNames()[11] == "Micro");
        std::vector<std::string> bad(10, "x");
        CHECK("hacen falta tantos nombres como canales activos", !s.setChannelNames(bad, e) && e.find("12") != std::string::npos);
        s.setInputChannels(14, {"B1","B2","A1","A2","A3","A4","A5","A6","A7","A8","A9","A10","C1","C2"});   // se reordenan las placas
        CHECK("los nombres siguen al id de la placa, no a la posición", s.channelNames()[1] == "Micro" && s.channelNames()[2] == "Sala");
        n = s.channelNames(); for (auto& x : n) x = ""; 
        CHECK("nombre vacío = el de fábrica", s.setChannelNames(n, e) && s.channelNames()[2] == "FL" && s.channelNames()[1] == "FR");
        s.setInputChannels(12, {"A1","A2","A3","A4","A5","A6","A7","A8","A9","A10","A11","A12"});
        CHECK("del canal 11 en adelante el nombre de fábrica es «Ch11»", s.channelNames()[10] == "Ch11" && s.channelNames()[11] == "Ch12");
    }
    // ── Persistencia de nombres (por id; el formato antiguo chN= se lee como A1…A10) ──
    {
        const char* base = "/tmp/sonos_names_test.txt"; remove("/tmp/sonos_names_test.txt.chnames");
        UdpStreamer s; s.setConfigPath(base);
        s.setInputChannels(12, {"A1","A2","A3","A4","A5","A6","A7","A8","A9","A10","B1","B2"});
        std::vector<std::string> n = s.channelNames(); n[10] = "Guitarra"; n[0] = "Izq"; std::string e;
        CHECK("guardar nombres", s.setChannelNames(n, e));
        UdpStreamer s2; s2.setConfigPath(base); s2.loadChannelNames(); s2.setInputChannels(12, {"A1","A2","A3","A4","A5","A6","A7","A8","A9","A10","B1","B2"});
        CHECK("nombres persisten por id", s2.channelNames()[10] == "Guitarra" && s2.channelNames()[0] == "Izq" && s2.channelNames()[11] == "FR");
        { FILE* f = fopen("/tmp/sonos_names_test.txt.chnames", "w"); fputs("ch0=Viejo\nch9=Fin\n", f); fclose(f); }
        UdpStreamer s3; s3.setConfigPath(base); s3.loadChannelNames();
        CHECK("formato antiguo: ch0 → A1, ch9 → A10", s3.channelNames()[0] == "Viejo" && s3.channelNames()[9] == "Fin" && s3.channelNames()[1] == "FR");
    }

    // ── Mezcla: placa A (4 canales) reloj principal + B (2 canales) por colchón ──
    {
        UdpStreamer s; InputMixer m(s, P);
        InputConfig cfg; cfg.devs = {dev("A", 4, true, "A"), dev("B", 2, true, "B")}; cfg.clock = "A";
        m.configure(cfg);
        CHECK("2 placas (4+2 canales): mínimo 10 entradas; B empieza en la 5", m.totalChannels() == 10 && m.firstOf(0) == 0 && m.firstOf(1) == 4);
        CHECK("ids: A1…A4, B1, B2", s.channelIds()[3] == "A4" && s.channelIds()[4] == "B1" && s.channelIds()[5] == "B2");
        auto a1 = blk(4, 1, 0x40000000), b1 = blk(2, 1, 0x10000000);
        for (int i = 0; i < 3; i++) m.injectForTest(1, b1.data(), P);
        for (int i = 0; i < 20; i++) { m.injectForTest(1, b1.data(), P); m.injectForTest(0, a1.data(), P, true, (uint64_t)i * 5000); }
        CHECK("la placa que manda el reloj suena (A2 = entrada 2, −6 dB)", s.meterDb(1) > -7 && s.meterDb(1) < -5 && s.meterDb(0) <= -89);
        CHECK("la otra entra por colchón (B2 = entrada 6, −18 dB)", s.meterDb(5) > -19 && s.meterDb(5) < -17 && s.meterDb(4) <= -89);
        for (int i = 0; i < 250; i++) { m.injectForTest(0, a1.data(), P, true, 100000 + (uint64_t)i * 5000); }
        CHECK("si la otra deja de entregar: silencio en sus canales y A sigue sonando", s.meterDb(5) < -30 && s.meterDb(1) > -7);
        { std::string j = m.statusJson(); size_t k = j.find("\"key\":\"B\""); CHECK("estado: B informa cortes", k != std::string::npos && atoi(j.c_str() + j.find("\"underruns\":", k) + 12) > 0); }
    }
    // ── Reloj = Raspberry: todas por colchón y reloj propio; una placa desactivada no ocupa entradas ──
    {
        UdpStreamer s; InputMixer m(s, P);
        InputConfig cfg; cfg.devs = {dev("A", 2, false, "A"), dev("B", 2, true, "B"), dev("C", 2, true, "C")}; cfg.clock = "rpi";
        m.configure(cfg);
        CHECK("la desactivada no ocupa entradas: B = 1–2, C = 3–4", m.firstOf(0) == 0 && m.firstOf(1) == 2 && s.channelIds()[0] == "B1" && s.channelIds()[2] == "C1");
        auto x = blk(2, 0, 0x40000000), y = blk(2, 1, 0x40000000);
        for (int i = 0; i < 3; i++) { m.injectForTest(0, x.data(), P); m.injectForTest(1, y.data(), P); }
        for (int i = 0; i < 20; i++) { m.injectForTest(0, x.data(), P); m.injectForTest(1, y.data(), P); m.tickForTest(P, (uint64_t)i * 5000); }
        CHECK("clock=rpi: B1 (entrada 1) y C2 (entrada 4) suenan", s.meterDb(0) > -7 && s.meterDb(0) < -5 && s.meterDb(3) > -7 && s.meterDb(3) < -5 && s.meterDb(1) <= -89 && s.meterDb(2) <= -89);
        // un inyectado «como reloj» no mezcla si nadie manda
        m.injectForTest(0, x.data(), P, true, 0);
        CHECK("clock=rpi: el estado indica reloj rpi", m.statusJson().find("\"clock\":\"rpi\"") != std::string::npos);
    }
    // ── Traktor (10 canales) + otra placa ──
    {
        UdpStreamer s; InputMixer m(s, P);
        InputConfig cfg; cfg.devs = {dev("Traktor", 10, true, "A"), dev("Scarlett", 8, true, "B")}; cfg.clock = "Traktor";
        m.configure(cfg);
        CHECK("Traktor + Scarlett: 18 entradas, B desde la 11", m.totalChannels() == 18 && m.firstOf(1) == 10 && s.channelIds()[10] == "B1" && s.channelIds()[17] == "B8");
        CHECK("un altavoz puede usar un canal de la segunda placa (18) y no el 19", s.setNodeConfig(0, 0, 17, 255, 255, true) && !s.setNodeConfig(0, 0, 18, 255, 255, true));
        auto k = blk(10, 0, 0x40000000), b = blk(8, 7, 0x40000000);
        for (int i = 0; i < 3; i++) m.injectForTest(1, b.data(), P);
        for (int i = 0; i < 20; i++) { m.injectForTest(1, b.data(), P); m.injectForTest(0, k.data(), P, true, (uint64_t)i * 5000); }
        CHECK("Traktor FL = entrada 1 y B8 = entrada 18", s.meterDb(0) > -7 && s.meterDb(0) < -5 && s.meterDb(17) > -7 && s.meterDb(17) < -5);
    }
    // ── Deriva: una placa más rápida no acumula retraso (se recorta) ──
    {
        UdpStreamer s; InputMixer m(s, P);
        InputConfig cfg; cfg.devs = {dev("A", 10, true, "A"), dev("B", 2, true, "B")}; cfg.clock = "A"; m.configure(cfg);
        auto a = blk(10, 0, 0x40000000), b = blk(2, 0, 0x40000000);
        for (int i = 0; i < 60; i++) { m.injectForTest(1, b.data(), P); m.injectForTest(1, b.data(), P); m.injectForTest(0, a.data(), P, true, (uint64_t)i * 5000); }
        std::string j = m.statusJson(); size_t k = j.find("\"key\":\"B\"");
        double fill = atof(j.c_str() + j.find("\"fill_ms\":", k) + 10), dropped = atof(j.c_str() + j.find("\"dropped_ms\":", k) + 13);
        CHECK("deriva: el colchón no crece sin límite (≤ 40 ms)", fill <= 40.0);
        CHECK("deriva: se contabilizan los recortes", dropped > 0.0);
    }
    // ── Sin placas activas: silencio, 10 entradas ──
    {
        UdpStreamer s; InputMixer m(s, P);
        InputConfig cfg; cfg.devs = {dev("A", 2, false, "A")}; m.configure(cfg);
        for (int i = 0; i < 5; i++) m.tickForTest(P, (uint64_t)i * 5000);
        CHECK("ninguna activa: 10 entradas en silencio", m.totalChannels() == 10 && s.inputChannels() == 10 && s.meterDb(0) <= -89);
    }
    printf("%s\n", fails ? "RESULTADO: FALLOS" : "RESULTADO: TODO OK");
    return fails ? 1 : 0;
}
