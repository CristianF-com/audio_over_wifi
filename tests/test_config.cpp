// Prueba de persistencia: guardar -> recargar en otra instancia -> mismos valores; ficheros corruptos no rompen nada.
#include "udp_streamer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static int fails = 0;
#define CHECK(n, c) do { bool _c = (c); printf("%s %s\n", _c ? "PASS" : "FAIL", n); if (!_c) fails++; } while (0)
int main() {
    const char* path = "/tmp/sonos_cfg_test.txt";
    remove(path);
    {
        UdpStreamer a; a.setConfigPath(path);
        CHECK("sin fichero: loadConfig() = false (primera ejecución)", !a.loadConfig());
        a.setNodeConfig(2, 5, 4, 100, 150, false);
        a.setLatencyOffset(2, -1234); a.setLatencyOffset(0, 777);
        a.setMasterVolume(0.65f); a.setPlayoutUs(120000);
        CHECK("por defecto: pass-through (crossover y EQ desactivados)", !a.xoverOn() && !a.eqOn());
        a.setXoverOn(true); a.setEqOn(true);
        a.setNodeRole(2, ROLE_SUB); a.setNodeRole(4, ROLE_SAT); a.setXoverHz(110); a.setLipsyncExtraMs(-7);
        { float g[EQ_BANDS] = {1.5f, -2.25f, 0, 0, 6, -12}; CHECK("setNodeEq", a.setNodeEq(3, g)); }
        { float g[EQ_BANDS] = {13, 0, 0, 0, 0, 0}; CHECK("setNodeEq: 13 dB rechazado", !a.setNodeEq(3, g)); }
        CHECK("setNodeRole(7) rechazado", !a.setNodeRole(1, 3));
        CHECK("setXoverHz(30/250) rechazado", !a.setXoverHz(30) && !a.setXoverHz(250));
        CHECK("setLipsyncExtraMs(201) rechazado", !a.setLipsyncExtraMs(201));
        a.setNodeConfig(7, 3, 9, 90, 200, true, true);          // mono: ch_right=ch_left, vol_r=vol_l
        CHECK("setNodeConfig mono fuerza ch_right/vol_r", a.getNodes()[7].ch_right == 3 && a.getNodes()[7].volume_r == 90);
        CHECK("editar un nodo => layout personalizada", a.layout() == LAYOUT_CUSTOM);
        CHECK("setLayout(99) rechazado", !a.setLayout(99));
        CHECK("saveConfig OK", a.saveConfig());
    }
    {
        UdpStreamer b; b.setConfigPath(path);
        CHECK("loadConfig OK", b.loadConfig());
        auto n = b.getNodes();
        CHECK("nodo 2: canales 5/4", n[2].ch_left == 5 && n[2].ch_right == 4);
        CHECK("nodo 2: volúmenes 100/150 y deshabilitado", n[2].volume_l == 100 && n[2].volume_r == 150 && !n[2].enabled);
        CHECK("latencias -1234 / 777 µs", n[2].latency_offset_us == -1234 && n[0].latency_offset_us == 777);
        CHECK("volumen maestro 0.65", b.masterVolume() > 0.649f && b.masterVolume() < 0.651f);
        CHECK("playout 120000 µs", b.playoutUs() == 120000);
        CHECK("nodo 1 intacto (valores por defecto)", n[1].ch_left == 2 && n[1].ch_right == 3 && n[1].enabled && !n[1].mono);
        CHECK("nodo 7 mono persistido (canal 3, vol 90, habilitado)", n[7].mono && n[7].ch_left == 3 && n[7].ch_right == 3 && n[7].volume_l == 90 && n[7].volume_r == 90 && n[7].enabled);
        CHECK("layout personalizada persistida", b.layout() == LAYOUT_CUSTOM);
        CHECK("roles persistidos (nodo 2 sub, nodo 4 satélite, resto completo)", n[2].role == ROLE_SUB && n[4].role == ROLE_SAT && n[0].role == ROLE_FULL);
        CHECK("EQ persistido (nodo 3)", n[3].eq_db[0] == 1.5f && n[3].eq_db[1] == -2.25f && n[3].eq_db[4] == 6.0f && n[3].eq_db[5] == -12.0f && n[0].eq_db[0] == 0);
        CHECK("flags de procesado persistidos (crossover y EQ activos)", b.xoverOn() && b.eqOn());
        CHECK("crossover persistido (110 Hz) y extra lip-sync (−7 ms)", b.xoverHz() == 110.0f && b.lipsyncExtraMs() == -7);
        CHECK("por defecto nodos 5..9 deshabilitados", !n[5].enabled && !n[9].enabled);
    }
    {   // fichero con basura, valores fuera de rango y claves desconocidas
        FILE* f = fopen(path, "w");
        fputs("version=1\nmaster_volume=99\nplayout_us=5\nnode9.ch_left=1\nnode1.ch_left=77\nnode1.volume_l=abc\n"
              "node2.role=9\nnode2.eq0=50\nnode2.eq9=1\nxover_hz=5\nlipsync_extra_ms=9999\nnodeX.foo=1\nbasura sin igual\nnode3.volume_l=42\nnode3.latency_offset_us=999999999\n", f);
        fclose(f);
        UdpStreamer c; c.setConfigPath(path);
        CHECK("fichero corrupto: se carga sin crash", c.loadConfig());
        auto n = c.getNodes();
        CHECK("master_volume=99 ignorado", c.masterVolume() == 1.0f);
        CHECK("playout_us=5 ignorado", c.playoutUs() == PLAYOUT_DELAY_US);
        CHECK("canal 77 ignorado, volumen 'abc' ignorado", n[1].ch_left == 2 && n[1].volume_l == 255);
        CHECK("valor válido sí se aplica (node3.volume_l=42)", n[3].volume_l == 42);
        CHECK("latencia fuera de rango ignorada", n[3].latency_offset_us == 0);
        CHECK("fichero corrupto: procesado sigue en pass-through", !c.xoverOn() && !c.eqOn());
        CHECK("rol 9, eq 50 dB, eq9, xover 5 Hz y lip-sync 9999 ignorados", n[2].role == ROLE_FULL && n[2].eq_db[0] == 0 && c.xoverHz() == 80.0f && c.lipsyncExtraMs() == 0);
    }
    {   // distribuciones
        const char* lp = "/tmp/sonos_cfg_layout.txt"; remove(lp);
        { UdpStreamer e; e.setConfigPath(lp);
          e.setNodeConfig(1, 4, 4, 50, 50, true, true);
          CHECK("setLayout(mono10)", e.setLayout(LAYOUT_MONO10) && e.layout() == LAYOUT_MONO10);
          auto n = e.getNodes(); bool ok = true;
          for (int i = 0; i < MAX_NODES; i++) ok &= n[i].mono && n[i].enabled && n[i].ch_left == i && n[i].ch_right == i;
          CHECK("mono10: nodo i = canal i, mono, habilitado", ok);
          CHECK("mono10 conserva volumen del nodo", n[1].volume_l == 50 && n[1].volume_r == 50);
          e.saveConfig(); }
        { UdpStreamer g; g.setConfigPath(lp); g.loadConfig();
          CHECK("mono10 persistido", g.layout() == LAYOUT_MONO10 && g.getNodes()[9].mono);
          CHECK("setLayout(stereo5)", g.setLayout(LAYOUT_STEREO5));
          auto n = g.getNodes(); bool ok = true;
          for (int i = 0; i < 5; i++) ok &= !n[i].mono && n[i].enabled && n[i].ch_left == 2 * i && n[i].ch_right == 2 * i + 1;
          CHECK("stereo5: pares (2i,2i+1) estéreo", ok);
          CHECK("stereo5: nodos 5..9 deshabilitados", !n[5].enabled && !n[9].enabled); }
        remove(lp);
    }
    {   // rangos de setPlayoutUs
        UdpStreamer d;
        CHECK("setPlayoutUs(39 ms) rechazado", !d.setPlayoutUs(39000));
        CHECK("setPlayoutUs(201 ms) rechazado", !d.setPlayoutUs(201000));
        CHECK("setPlayoutUs(40 ms) y (200 ms) aceptados", d.setPlayoutUs(40000) && d.setPlayoutUs(200000));
    }
    remove(path);
    printf("RESULTADO CONFIG: %s\n", fails ? "HAY FALLOS" : "TODO OK");
    return fails ? 1 : 0;
}
