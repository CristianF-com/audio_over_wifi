// Respuesta en frecuencia de los filtros de rpi/include/dsp.h, medida con tonos (estado estacionario).
#include "dsp.h"
#include <cstdio>
#include <vector>
static int fails = 0;
#define CHECK(n, c) do { bool _c = (c); printf("%s %s\n", _c ? "PASS" : "FAIL", n); if (!_c) fails++; } while (0)
static const double FS = 48000;
template <class F> static double gain_db(double f, F proc) {                  // ganancia a f Hz
    double sx = 0, sy = 0; const int N = 48000 * 2, skip = 48000;
    for (int i = 0; i < N; i++) { double x = std::sin(2 * M_PI * f * i / FS) * 10000, y = proc(x);
        if (i >= skip) { sx += x * x; sy += y * y; } }
    return 10 * std::log10(sy / sx);
}
int main() {
    for (double fc : {60.0, 80.0, 120.0}) {
        Lr4 hp, lp; hp.setHighpass(FS, fc); lp.setLowpass(FS, fc);
        char n[96];
        double a = gain_db(fc, [&](double x) { return hp.run(x); }); hp.reset();
        snprintf(n, sizeof n, "LR4 fc=%.0f: pasa-altos −6 dB en fc (%.2f dB)", fc, a); CHECK(n, std::fabs(a + 6.02) < 0.15);
        double b = gain_db(fc, [&](double x) { return lp.run(x); }); lp.reset();
        snprintf(n, sizeof n, "LR4 fc=%.0f: pasa-bajos −6 dB en fc (%.2f dB)", fc, b); CHECK(n, std::fabs(b + 6.02) < 0.15);
        double c = gain_db(fc / 2, [&](double x) { return hp.run(x); }); hp.reset();
        snprintf(n, sizeof n, "LR4 fc=%.0f: pasa-altos a fc/2 ≤ −20 dB (%.1f dB)", fc, c); CHECK(n, c < -20);
        double d = gain_db(fc * 2, [&](double x) { return lp.run(x); }); lp.reset();
        snprintf(n, sizeof n, "LR4 fc=%.0f: pasa-bajos a 2·fc ≤ −20 dB (%.1f dB)", fc, d); CHECK(n, d < -20);
        double worst = 0;                                                        // LP + HP = plano (módulo 1)
        for (double f : {20.0, 40.0, fc * 0.7, fc, fc * 1.4, 300.0, 1000.0, 5000.0}) {
            Lr4 h2, l2; h2.setHighpass(FS, fc); l2.setLowpass(FS, fc);
            double e = gain_db(f, [&](double x) { return h2.run(x) + l2.run(x); });
            if (std::fabs(e) > worst) worst = std::fabs(e);
        }
        snprintf(n, sizeof n, "LR4 fc=%.0f: HP+LP suman plano (error máx. %.3f dB)", fc, worst); CHECK(n, worst < 0.1);
    }
    for (int bnd = 0; bnd < EQ_BANDS; bnd++) {
        EqChain eq; float g[EQ_BANDS] = {0}; g[bnd] = 6.0f; eq.set(g, FS);
        double at = gain_db(eqCenterHz(bnd), [&](double x) { return eq.run(x); });
        char n[96]; snprintf(n, sizeof n, "EQ banda %d (%.0f Hz) +6 dB: ganancia %.2f dB", bnd, eqCenterHz(bnd), at);
        CHECK(n, std::fabs(at - 6.0) < 0.25);
    }
    { EqChain eq; float g[EQ_BANDS] = {0}; g[2] = -6; eq.set(g, FS);
      double far = gain_db(100, [&](double x) { return eq.run(x); });
      CHECK("EQ banda 1.4 kHz −6 dB: a 100 Hz casi sin efecto (< 0.3 dB)", std::fabs(far) < 0.3); }
    { EqChain eq; float g[EQ_BANDS] = {0}; eq.set(g, FS);
      CHECK("EQ plano: identidad exacta", eq.run(1234.5) == 1234.5 && !eq.active);
      g[0] = 0.02f; eq.set(g, FS); CHECK("EQ: < 0.05 dB se ignora", !eq.active);
      g[3] = 3; eq.set(g, FS); CHECK("EQ: banda activa", eq.active);
      g[3] = 0; eq.set(g, FS); CHECK("EQ: volver a 0 desactiva", !eq.active && eq.run(777.0) == 777.0); }
    printf("RESULTADO DSP: %s\n", fails ? "HAY FALLOS" : "TODO OK");
    return fails ? 1 : 0;
}
