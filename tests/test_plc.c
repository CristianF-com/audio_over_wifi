/* Pruebas de la ocultación de pérdidas del firmware (esp32/main/plc.h), en el PC. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../esp32/main/plc.h"
static int fails = 0;
#define CHECK(n, c) do { int _c = (c); printf("%s %s\n", _c ? "PASS" : "FAIL", n); if (!_c) fails++; } while (0)
#define N PLC_N
static double sig(int n, int ch) { return 12000.0 * sin(2 * M_PI * (ch ? 310.0 : 440.0) * n / 48000.0); }
static void mk(int16_t* o, int n0) { for (int f = 0; f < N; f++) { o[2*f] = (int16_t)sig(n0 + f, 0); o[2*f+1] = (int16_t)sig(n0 + f, 1); } }
static int maxstep(const int16_t* s, int frames) { int m = 0; for (int f = 1; f < frames; f++) for (int c = 0; c < 2; c++) { int d = abs(s[2*f+c] - s[2*(f-1)+c]); if (d > m) m = d; } return m; }
static long energy(const int16_t* s) { long e = 0; for (int i = 0; i < N * 2; i++) e += labs(s[i]); return e; }

int main(void) {
    static int16_t last[N*2], out[N*2], line[N*2*8], nxt[N*2];
    mk(last, 0);
    const int natural = maxstep(last, N);                       /* mayor salto entre muestras de la señal sana */
    for (int k = 1; k <= 6; k++) {                              /* hueco de k trozos: último bueno + relleno + paquete nuevo mezclado */
        int w = 0; memcpy(line, last, sizeof last); w += N;
        for (int i = 0; i < k; i++) { plc_fill(line + 2*w, last, i); w += N; }
        mk(nxt, N * (k + 1)); plc_blend_in(nxt, last, k); memcpy(line + 2*w, nxt, sizeof nxt); w += N;
        char name[96]; snprintf(name, sizeof name, "hueco de %d: sin clic en ninguna unión (salto ≤ 2× el natural)", k);
        int ms = maxstep(line, w); CHECK(name, ms <= natural * 2 + 8);
        if (ms > natural * 2 + 8) printf("   salto=%d natural=%d\n", ms, natural);
    }
    plc_fill(out, last, 0);
    CHECK("primer trozo: empieza igual que terminó el audio (continuidad exacta)", out[0] == last[2*(N-1)] && out[1] == last[2*(N-1)+1]);
    CHECK("primer trozo: conserva más de la mitad de la energía (antes: fundido a cero)", energy(out) * 2 > energy(last));
    long e0 = energy(out); plc_fill(out, last, 1); long e1 = energy(out); plc_fill(out, last, 2); long e2 = energy(out); plc_fill(out, last, 3); long e3 = energy(out);
    CHECK("el relleno baja de volumen trozo a trozo", e0 > e1 && e1 > e2 && e2 > e3 && e3 > 0);
    plc_fill(out, last, PLC_STEPS); CHECK("a partir de 10 ms: silencio", energy(out) == 0);
    plc_fill(out, last, 7);         CHECK("hueco largo: silencio", energy(out) == 0);
    mk(nxt, 5 * N); static int16_t ref[N*2]; memcpy(ref, nxt, sizeof ref); plc_blend_in(nxt, last, PLC_STEPS + 2);
    int same_end = abs(nxt[2*(N-1)] - ref[2*(N-1)]) <= abs(ref[2*(N-1)]) / 50 + 2;
    CHECK("entrada tras hueco largo: sube desde cero y termina en el audio real", nxt[0] == 0 && nxt[1] == 0 && same_end);
    { int16_t mx[N*2]; for (int i = 0; i < N*2; i++) mx[i] = (i & 2) ? 32767 : -32768; int16_t o2[N*2]; int okr = 1;
      for (int i = 0; i < PLC_STEPS; i++) { plc_fill(o2, mx, i); for (int j = 0; j < N*2; j++) if (abs(o2[j]) > 32768) okr = 0; }
      int16_t w2[N*2]; memcpy(w2, mx, sizeof w2); plc_blend_in(w2, mx, 0);
      CHECK("fondo de escala: no desborda", okr && w2[N*2-1] != 0); }
    printf("\nRESULTADO PLC: %s\n", fails ? "HAY FALLOS" : "TODO OK");
    return fails ? 1 : 0;
}
