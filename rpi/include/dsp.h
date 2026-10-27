// dsp.h — procesado de audio en la RPi (los ESP32 no tocan las muestras).
//   * Crossover Linkwitz-Riley de 4.º orden (24 dB/oct): pasa-altos para los «satélites», pasa-bajos para el sub.
//     LP + HP suman plano en módulo y en fase, así que no hay hueco ni pico en la frecuencia de corte.
//   * Ecualizador paramétrico de EQ_BANDS bandas de 1 octava por altavoz (filtros «peaking», fórmulas RBJ).
// Funciones sin dependencias: tests/test_dsp.cpp las comprueba midiendo respuestas en frecuencia.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

constexpr int    EQ_BANDS = 6;
// Bordes de las bandas = rango del barrido de calibración (250..12000 Hz), una octava cada una (la última, 8..12 kHz).
constexpr double EQ_EDGES_HZ[EQ_BANDS + 1] = {250, 500, 1000, 2000, 4000, 8000, 12000};
constexpr double EQ_Q      = 1.4;       // ≈ 1 octava
constexpr float  EQ_MAX_DB = 12.0f;     // tope por banda en la API; la calibración propone como mucho ±6
constexpr double XOVER_MIN_HZ = 40.0, XOVER_MAX_HZ = 200.0, XOVER_DEFAULT_HZ = 80.0;

inline double eqCenterHz(int b) { return std::sqrt(EQ_EDGES_HZ[b] * EQ_EDGES_HZ[b + 1]); }

struct Biquad {                        // forma directa II transpuesta, en double (graves a 48 kHz)
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double run(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void setCoef(double B0, double B1, double B2, double A0, double A1, double A2) {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;     // conserva z1/z2: cambios sin clic
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void setLowpass(double fs, double fc, double Q) {
        double w = 2 * M_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * Q);
        setCoef((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setHighpass(double fs, double fc, double Q) {
        double w = 2 * M_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * Q);
        setCoef((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setPeaking(double fs, double fc, double Q, double gain_db) {
        double A = std::pow(10.0, gain_db / 40.0), w = 2 * M_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * Q);
        setCoef(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
};

struct Lr4 {                            // 2 × Butterworth (Q = 1/√2) en cascada = Linkwitz-Riley 4.º orden
    Biquad s[2];
    void setLowpass (double fs, double fc) { for (auto& b : s) b.setLowpass (fs, fc, M_SQRT1_2); }
    void setHighpass(double fs, double fc) { for (auto& b : s) b.setHighpass(fs, fc, M_SQRT1_2); }
    inline double run(double x) { return s[1].run(s[0].run(x)); }
    void reset() { s[0].reset(); s[1].reset(); }
};

struct EqChain {                        // ecualizador de un canal de salida
    Biquad b[EQ_BANDS];
    float  applied[EQ_BANDS] = {0};
    bool   on[EQ_BANDS]      = {false};
    bool   active            = false;
    // Idempotente y barato si no hay cambios (se llama en cada bloque).
    void set(const float g[EQ_BANDS], double fs) {
        bool any = false;
        for (int i = 0; i < EQ_BANDS; i++) {
            if (g[i] == applied[i]) { any |= on[i]; continue; }
            applied[i] = g[i];
            on[i] = std::fabs(g[i]) >= 0.05f;
            if (on[i]) b[i].setPeaking(fs, eqCenterHz(i), EQ_Q, g[i]);
            else { b[i].setIdentity(); b[i].reset(); }
            any |= on[i];
        }
        active = any;
    }
    inline double run(double x) {
        if (!active) return x;
        for (int i = 0; i < EQ_BANDS; i++) if (on[i]) x = b[i].run(x);
        return x;
    }
};
