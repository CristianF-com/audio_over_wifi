// Interfaces USB extra: configuración, validación, unión de canales y colchón elástico (sin ALSA ni hardware).
#include "input_mixer.h"
#include "udp_streamer.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
static int fails = 0;
#define CHECK(n, c) do { bool _c = (c); printf("%s %s\n", _c ? "PASS" : "FAIL", n); if (!_c) fails++; } while (0)

int main() {
    // ── validación y JSON ──
    std::vector<ExtraSpec> v; std::string err;
    CHECK("JSON válido (2 interfaces)", InputMixer::parseJson("{\"extras\":[{\"device\":\"hw:2,0\",\"channels\":8},{\"device\":\"Scarlett\",\"channels\":2}]}", v, err) && v.size() == 2 && v[0].device == "hw:2,0" && v[0].channels == 8 && v[1].device == "Scarlett");
    CHECK("lista vacía válida", InputMixer::parseJson("{\"extras\":[]}", v, err) && v.empty());
    CHECK("sin extras → error", !InputMixer::parseJson("{}", v, err) && !err.empty());
    CHECK("canales 0 rechazado", !InputMixer::parseJson("{\"extras\":[{\"device\":\"hw:2,0\",\"channels\":0}]}", v, err));
    CHECK("más de 22 canales rechazado", !InputMixer::parseJson("{\"extras\":[{\"device\":\"hw:2,0\",\"channels\":23}]}", v, err));
    CHECK("total > 22 rechazado", !InputMixer::parseJson("{\"extras\":[{\"device\":\"hw:2,0\",\"channels\":12},{\"device\":\"hw:3,0\",\"channels\":12}]}", v, err));
    CHECK("repetida rechazada", !InputMixer::parseJson("{\"extras\":[{\"device\":\"hw:2,0\",\"channels\":2},{\"device\":\"hw:2,0\",\"channels\":2}]}", v, err));
    CHECK("la Traktor no puede ser extra", !InputMixer::parseJson("{\"extras\":[{\"device\":\"Traktor Audio 10\",\"channels\":2}]}", v, err));
    CHECK("dispositivo con | o comillas rechazado", !InputMixer::parseJson("{\"extras\":[{\"device\":\"a|b\",\"channels\":2}]}", v, err));
    // ── fichero ──
    const char* path = "/tmp/sonos_inputs_test.inputs";
    remove(path);
    CHECK("sin fichero → lista vacía", InputMixer::loadFile(path).empty());
    std::vector<ExtraSpec> a = {{"hw:2,0", 8}, {"UMC1820", 6}};
    CHECK("guardar y recargar", InputMixer::saveFile(path, a) && InputMixer::loadFile(path).size() == 2 && InputMixer::loadFile(path)[1].device == "UMC1820" && InputMixer::loadFile(path)[1].channels == 6);

    // ── Solo Traktor: nada cambia ──
    {
        UdpStreamer s;
        CHECK("por defecto 10 entradas", s.inputChannels() == 10 && s.allChannelNames().size() == 10);
        CHECK("canal 10 rechazado sin interfaces extra", !s.setNodeConfig(0, 10, 11, 255, 255, true));
        CHECK("canales 0..9 aceptados", s.setNodeConfig(0, 8, 9, 255, 255, true));
    }
    // ── Con interfaces extra ──
    UdpStreamer s;
    InputMixer m(s, 240);
    m.setExtras({{"hw:2,0", 4}, {"hw:3,0", 2}});
    CHECK("total = 10 + 4 + 2", m.totalChannels() == 16 && s.inputChannels() == 16);
    { auto n = s.allChannelNames(); CHECK("nombres: 10 + USB1-1..4 + USB2-1..2", n.size() == 16 && n[3] == "LFE" && n[10] == "USB1-1" && n[13] == "USB1-4" && n[14] == "USB2-1" && n[15] == "USB2-2"); }
    CHECK("el nombre editable sigue siendo de 10 entradas", s.channelNames().size() == 10);
    CHECK("un altavoz puede usar un canal extra (15)", s.setNodeConfig(1, 10, 15, 255, 255, true));
    CHECK("canal 16 rechazado", !s.setNodeConfig(1, 10, 16, 255, 255, true));

    const uint32_t P = 240;
    std::vector<int32_t> trk((size_t)P * 10, 0), x1((size_t)P * 4, 0), x2((size_t)P * 2, 0);
    for (uint32_t f = 0; f < P; f++) { trk[(size_t)f * 10 + 0] = 0x40000000; x1[(size_t)f * 4 + 1] = 0x40000000; x2[(size_t)f * 2 + 1] = 0x10000000; }
    // 3 bloques de colchón y luego en régimen
    for (int i = 0; i < 3; i++) { m.injectForTest(0, x1.data(), P); m.injectForTest(1, x2.data(), P); }
    for (int i = 0; i < 20; i++) { m.injectForTest(0, x1.data(), P); m.injectForTest(1, x2.data(), P); m.onTraktor(trk.data(), P, (uint64_t)i * 5000); }
    CHECK("canal 0 (Traktor) con señal", s.meterDb(0) > -7 && s.meterDb(0) < -5);
    CHECK("canal 1 (Traktor) en silencio", s.meterDb(1) <= -89);
    CHECK("canal 11 = 2.º canal de USB1 con señal", s.meterDb(11) > -7 && s.meterDb(11) < -5);
    CHECK("canal 10 (1.º de USB1) en silencio", s.meterDb(10) <= -89);
    CHECK("canal 15 = 2.º canal de USB2 con señal (−18 dB)", s.meterDb(15) > -19 && s.meterDb(15) < -17);
    CHECK("estado: sin subdesbordes en régimen", m.statusJson().find("\"underruns\":0") != std::string::npos);

    // ── Deriva: una interfaz que va más rápida no acumula retraso (se recorta) ──
    for (int i = 0; i < 40; i++) { m.injectForTest(0, x1.data(), P); m.injectForTest(0, x1.data(), P); m.onTraktor(trk.data(), P, 100000 + (uint64_t)i * 5000); }
    { std::string j = m.statusJson(); size_t k = j.find("\"fill_ms\":"); double fill = atof(j.c_str() + k + 10);
      CHECK("deriva: el colchón no crece sin límite (≤ 40 ms)", fill <= 40.0);
      CHECK("deriva: se contabilizan los recortes", atof(j.c_str() + j.find("\"dropped_ms\":") + 13) > 0.0); }
    // ── Una interfaz que deja de entregar: silencio en sus canales, la Traktor sigue ──
    for (int i = 0; i < 250; i++) m.onTraktor(trk.data(), P, 400000 + (uint64_t)i * 5000);
    CHECK("USB1 sin datos → silencio (cae el medidor)", s.meterDb(11) < -30);
    CHECK("la Traktor sigue sonando", s.meterDb(0) > -7);
    { std::string j = m.statusJson(); CHECK("subdesborde contabilizado", atoi(j.c_str() + j.find("\"underruns\":") + 12) > 0); }
    // ── Sin Traktor: las extras toman las entradas desde la 1 ──
    {
        UdpStreamer s2; InputMixer m2(s2, 240);
        m2.setTraktorPresent(false);
        m2.setExtras({{"hw:2,0", 8}, {"hw:3,0", 4}});
        CHECK("sin Traktor: 12 canales extra → 12 entradas", m2.totalChannels() == 12 && s2.inputChannels() == 12);
        { auto n = s2.allChannelNames(); CHECK("sin Traktor: las 10 primeras conservan los nombres editables y las demás son USB", n.size() == 12 && n[0] == "FL" && n[9] == "AUX-R" && n[10] == "USB2-3" && n[11] == "USB2-4"); }
        std::vector<int32_t> x((size_t)P * 8, 0), y((size_t)P * 4, 0);
        for (uint32_t f = 0; f < P; f++) { x[(size_t)f * 8 + 0] = 0x40000000; y[(size_t)f * 4 + 3] = 0x40000000; }
        for (int i = 0; i < 3; i++) { m2.injectForTest(0, x.data(), P); m2.injectForTest(1, y.data(), P); }
        for (int i = 0; i < 20; i++) { m2.injectForTest(0, x.data(), P); m2.injectForTest(1, y.data(), P); m2.onTraktor(nullptr, P, (uint64_t)i * 5000); }
        CHECK("sin Traktor: el canal 1 de la 1.ª interfaz es la entrada 1", s2.meterDb(0) > -7 && s2.meterDb(0) < -5 && s2.meterDb(1) <= -89);
        CHECK("sin Traktor: el canal 4 de la 2.ª interfaz es la entrada 12", s2.meterDb(11) > -7 && s2.meterDb(11) < -5 && s2.meterDb(10) <= -89);
        CHECK("sin Traktor: un altavoz puede usar la entrada 1 y la 12", s2.setNodeConfig(0, 0, 11, 255, 255, true) && !s2.setNodeConfig(0, 0, 12, 255, 255, true));
        std::vector<ExtraSpec> w; std::string e;
        CHECK("sin Traktor: se admiten hasta 32 canales (con Traktor, 22)", InputMixer::parseJson("{\"extras\":[{\"device\":\"hw:2,0\",\"channels\":30}]}", w, e, false) && !InputMixer::parseJson("{\"extras\":[{\"device\":\"hw:2,0\",\"channels\":30}]}", w, e, true));
        UdpStreamer s3; InputMixer m3(s3, 240); m3.setTraktorPresent(false); m3.setExtras({{"hw:2,0", 2}});
        CHECK("sin Traktor y pocas extras: mínimo 10 entradas (resto en silencio)", m3.totalChannels() == 10 && s3.inputChannels() == 10);
    }
    // ── Traktor editable: posición y reloj ──
    {
        const char* pth = "/tmp/sonos_inputs_trk.inputs"; remove(pth);
        int tp = 5; bool tm = false;
        InputMixer::loadTraktor(pth, tp, tm);
        CHECK("sin @traktor: posición 0 y reloj principal por defecto", tp == 0 && tm);
        CHECK("guardar con posición y reloj", InputMixer::saveFile(pth, {{"hw:2,0", 2}}, 1, false));
        InputMixer::loadTraktor(pth, tp, tm);
        CHECK("recargar posición 1 y reloj secundario; la línea @traktor no se lee como interfaz", tp == 1 && !tm && InputMixer::loadFile(pth).size() == 1);
        InputMixer::saveFile(pth, {{"hw:2,0", 2}});
        { std::string txt; FILE* f = fopen(pth, "r"); char b[128]; while (fgets(b, sizeof b, f)) txt += b; fclose(f); CHECK("valores por defecto: el fichero no lleva @traktor", txt == "hw:2,0|2\n"); }
        int p2 = 0; bool m2 = true; InputMixer::parseTraktor("{\"extras\":[],\"traktor\":{\"pos\":2,\"master\":false}}", p2, m2);
        CHECK("JSON traktor: pos 2, master false", p2 == 2 && !m2);
        p2 = 3; m2 = false; InputMixer::parseTraktor("{\"extras\":[]}", p2, m2);
        CHECK("JSON sin traktor no toca los valores", p2 == 3 && !m2);

        // Traktor en el medio (pos 1), sigue siendo el reloj: A(2) | Traktor(10) | B(2)
        UdpStreamer s4; InputMixer m4(s4, 240);
        m4.setTraktorLayout(1, true); m4.setExtras({{"hw:2,0", 2}, {"hw:3,0", 2}});
        CHECK("Traktor en el medio: 14 entradas y sus canales empiezan en la 3", m4.totalChannels() == 14 && m4.traktorFirst() == 2);
        { auto n = s4.allChannelNames(); CHECK("nombres: TK en las entradas ≥ 10 de la Traktor, USB en las otras", n.size() == 14 && n[10] == "TK9" && n[11] == "TK10" && n[12] == "USB2-1" && n[13] == "USB2-2"); }
        std::vector<int32_t> a((size_t)P * 2, 0), b((size_t)P * 2, 0), k((size_t)P * 10, 0);
        for (uint32_t f = 0; f < P; f++) { a[(size_t)f * 2] = 0x40000000; b[(size_t)f * 2 + 1] = 0x40000000; k[(size_t)f * 10] = 0x20000000; }
        for (int i = 0; i < 3; i++) { m4.injectForTest(0, a.data(), P); m4.injectForTest(1, b.data(), P); }
        for (int i = 0; i < 20; i++) { m4.injectForTest(0, a.data(), P); m4.injectForTest(1, b.data(), P); m4.onTraktor(k.data(), P, (uint64_t)i * 5000); }
        CHECK("Traktor en el medio: A.1 = entrada 1 (−6 dB)", s4.meterDb(0) > -7 && s4.meterDb(0) < -5);
        CHECK("Traktor en el medio: Traktor FL = entrada 3 (−12 dB)", s4.meterDb(2) > -13 && s4.meterDb(2) < -11 && s4.meterDb(3) <= -89);
        CHECK("Traktor en el medio: B.2 = entrada 14 (−6 dB)", s4.meterDb(13) > -7 && s4.meterDb(13) < -5 && s4.meterDb(12) <= -89);

        // Traktor sin ser el reloj: entra por colchón y manda el reloj del mezclador (tickForTest)
        UdpStreamer s5; InputMixer m5(s5, 240);
        m5.setTraktorLayout(0, false); m5.setExtras({{"hw:2,0", 2}});
        CHECK("Traktor secundaria al principio: 12 entradas, Traktor en la 1", m5.totalChannels() == 12 && m5.traktorFirst() == 0);
        for (int i = 0; i < 3; i++) { m5.injectForTest(0, a.data(), P); m5.onTraktor(k.data(), P, 0); }
        for (int i = 0; i < 20; i++) { m5.injectForTest(0, a.data(), P); m5.onTraktor(k.data(), P, 0); m5.tickForTest(P, (uint64_t)i * 5000); }
        CHECK("Traktor secundaria: la Traktor suena por su colchón (entrada 1, −12 dB)", s5.meterDb(0) > -13 && s5.meterDb(0) < -11);
        CHECK("Traktor secundaria: la extra queda en las entradas 11–12 (A.1 = 11, −6 dB)", s5.meterDb(10) > -7 && s5.meterDb(10) < -5);
        CHECK("Traktor secundaria: el estado informa underruns de la Traktor", m5.statusJson().find("\"master\":false") != std::string::npos && m5.statusJson().find("\"traktor\":{") != std::string::npos);
        for (int i = 0; i < 250; i++) m5.tickForTest(P, 200000 + (uint64_t)i * 5000);
        CHECK("Traktor secundaria: si deja de entregar, silencio y la extra también sigue su colchón", s5.meterDb(0) < -30);
        // Traktor ausente: su hueco no existe aunque tenga posición
        UdpStreamer s6; InputMixer m6(s6, 240);
        m6.setTraktorPresent(false); m6.setTraktorLayout(1, true); m6.setExtras({{"hw:2,0", 2}, {"hw:3,0", 2}});
        CHECK("Traktor ausente: las extras ocupan desde la entrada 1 sin hueco (10 entradas mínimo)", m6.totalChannels() == 10);
    }
    printf("%s\n", fails ? "RESULTADO: FALLOS" : "RESULTADO: TODO OK");
    return fails ? 1 : 0;
}
