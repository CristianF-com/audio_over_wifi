/**
 * plc.h — ocultación de paquetes perdidos (PLC). C puro, sin ESP-IDF: tests/test_plc.c lo ejecuta en el PC.
 *
 * Cuando falta audio se rellena el hueco con el último trozo reproducido, alternando al revés / al derecho
 * (así la muestra es continua en cada unión: no hay clic) y bajando el volumen poco a poco; pasados PLC_STEPS
 * trozos (10 ms) queda en silencio. El primer paquete que llega después entra mezclado con la continuación
 * del relleno (fundido cruzado), sin escalón. Antes se hacía fundido a cero + silencio, que se oía como microcorte.
 */
#pragma once
#include <stdint.h>
#include <string.h>
#include "protocol.h"

#define PLC_N      FRAMES_PER_PACKET
#define PLC_STEPS  4
static const int16_t PLC_G[PLC_STEPS + 1] = { 256, 180, 100, 40, 0 };   /* ganancia Q8 al empezar cada trozo */

/* Trozo n.º idx del relleno (0 = el primero tras el último audio bueno). */
static inline void plc_fill(int16_t* out, const int16_t* last, int idx)
{
    if (idx < 0 || idx >= PLC_STEPS) { memset(out, 0, (size_t)PLC_N * 2 * sizeof(int16_t)); return; }
    const int32_t g0 = PLC_G[idx], g1 = PLC_G[idx + 1];
    for (int f = 0; f < PLC_N; f++) {
        const int s = (idx & 1) ? f : PLC_N - 1 - f;              /* par: al revés; impar: al derecho */
        const int32_t g = g0 + ((g1 - g0) * f) / PLC_N;
        out[2 * f]     = (int16_t)((last[2 * s]     * g) / 256);
        out[2 * f + 1] = (int16_t)((last[2 * s + 1] * g) / 256);
    }
}

/* Entrada tras un hueco de k trozos: work (paquete nuevo) sube 0→1 mientras la continuación del relleno baja 1→0. */
static inline void plc_blend_in(int16_t* work, const int16_t* last, int k)
{
    static int16_t tail[PLC_N * 2];
    plc_fill(tail, last, k);
    for (int f = 0; f < PLC_N; f++) {
        work[2 * f]     = (int16_t)((work[2 * f]     * f + tail[2 * f]     * (PLC_N - f)) / PLC_N);
        work[2 * f + 1] = (int16_t)((work[2 * f + 1] * f + tail[2 * f + 1] * (PLC_N - f)) / PLC_N);
    }
}
