/**
 * reorder.h — reordenación de paquetes de audio + petición de reenvío (NACK).
 * C puro, sin dependencias de ESP-IDF: tests/test_reorder.c lo ejecuta en el PC.
 *
 * Idea: los paquetes se entregan SIEMPRE en orden de seq. Si llega uno con hueco, se guarda en una ventana de
 * RO_WIN paquetes, se pide el reenvío de los que faltan (NACK; se repite a mitad de espera) y se espera como
 * máximo wait_us. Si el reenvío llega, todo sale en orden sin pérdida. Si no, se da por perdido y el PLC del
 * reproductor rellena el hueco (la línea de tiempo se conserva porque cada paquete lleva su timestamp).
 * La espera se paga solo cuando hay pérdidas y cabe en el colchón de reproducción (playout).
 */
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "protocol.h"

#define RO_WIN 16          /* potencia de 2 */

typedef void (*ro_deliver_fn)(void* ctx, const AudioPacket* p);
typedef void (*ro_nack_fn)(void* ctx, const uint32_t* seqs, int n);

typedef struct {
    AudioPacket   slot[RO_WIN];
    uint32_t      valid, asked;          /* bit i: slot[i] ocupado / ya pedido */
    uint32_t      next, hi;              /* próximo seq a entregar / mayor seq visto */
    bool          have;
    int64_t       deadline_us, nack2_us; /* 0 = sin huecos pendientes */
    int64_t       wait_us;
    uint32_t      lost, recovered, dups, resyncs;
    ro_deliver_fn deliver;
    ro_nack_fn    nack;
    void*         ctx;
} reorder_t;

typedef enum { RO_NEW = 0, RO_RECOVERED = 1, RO_DROPPED = 2 } ro_res_t;

static inline void ro_init(reorder_t* r, ro_deliver_fn d, ro_nack_fn n, void* ctx, int64_t wait_us)
{
    memset(r, 0, sizeof(*r));
    r->deliver = d; r->nack = n; r->ctx = ctx; r->wait_us = wait_us;
}
/* Descarta el estado de la secuencia (nueva RPi / nuevo WELCOME); conserva los contadores. */
static inline void ro_reset(reorder_t* r)
{
    r->valid = r->asked = 0; r->have = false; r->deadline_us = r->nack2_us = 0;
}
static inline void ro_set_wait(reorder_t* r, int64_t wait_us) { r->wait_us = wait_us; }

static inline void ro__drain(reorder_t* r)
{
    while (r->valid & (1u << (r->next & (RO_WIN - 1)))) {
        unsigned i = r->next & (RO_WIN - 1);
        r->deliver(r->ctx, &r->slot[i]);
        r->valid &= ~(1u << i); r->asked &= ~(1u << i);
        r->next++;
    }
    if (!r->valid) r->deadline_us = r->nack2_us = 0;
}

/* Se acabó la espera: entrega lo guardado en orden y da por perdidos los huecos. */
static inline void ro__expire(reorder_t* r)
{
    while (r->valid) {
        unsigned i = r->next & (RO_WIN - 1);
        if (r->valid & (1u << i)) { r->deliver(r->ctx, &r->slot[i]); r->valid &= ~(1u << i); }
        else r->lost++;
        r->asked &= ~(1u << i); r->next++;
    }
    r->deadline_us = r->nack2_us = 0;
}

static inline void ro__request(reorder_t* r, bool again)
{
    uint32_t seqs[NACK_MAX_SEQS]; int n = 0;
    for (uint32_t s = r->next; (int32_t)(r->hi - s) > 0 && n < NACK_MAX_SEQS; s++) {
        unsigned i = s & (RO_WIN - 1);
        if (r->valid & (1u << i)) continue;
        if (!again && (r->asked & (1u << i))) continue;
        r->asked |= 1u << i; seqs[n++] = s;
    }
    if (n && r->nack) r->nack(r->ctx, seqs, n);
}

static inline void ro__resync(reorder_t* r, uint32_t seq)
{
    while (r->valid) {                                   /* entrega lo que hubiera, sin contar huecos */
        unsigned i = r->next & (RO_WIN - 1);
        if (r->valid & (1u << i)) { r->deliver(r->ctx, &r->slot[i]); r->valid &= ~(1u << i); }
        r->next++;
    }
    r->asked = 0; r->deadline_us = r->nack2_us = 0; r->next = r->hi = seq; r->resyncs++;
}

static inline void ro_poll(reorder_t* r, int64_t now_us)
{
    if (!r->deadline_us) return;
    if (now_us >= r->deadline_us) { ro__expire(r); return; }
    if (r->nack2_us && now_us >= r->nack2_us) { r->nack2_us = 0; ro__request(r, true); }
}

static inline ro_res_t ro_push(reorder_t* r, const AudioPacket* p, int64_t now_us)
{
    const uint32_t seq = p->seq;
    if (!r->have) { r->have = true; r->next = r->hi = seq; }
    int32_t d = (int32_t)(seq - r->next);
    if (d < 0) {
        if (d >= -64) { r->dups++; return RO_DROPPED; }  /* reenvío tardío o duplicado */
        ro__resync(r, seq); d = 0;                       /* salto atrás grande: la RPi se reinició */
    } else if (d >= RO_WIN) {
        ro__resync(r, seq); d = 0;                       /* salto adelante grande: corte largo */
    }
    if (d > 0 && (r->valid & (1u << (seq & (RO_WIN - 1))))) { r->dups++; return RO_DROPPED; }

    ro_res_t res = RO_NEW;
    if ((int32_t)(seq - r->hi) < 0) { res = RO_RECOVERED; r->recovered++; }
    else r->hi = seq;

    if (d == 0) {
        r->deliver(r->ctx, p); r->next++; ro__drain(r);
    } else {
        unsigned i = seq & (RO_WIN - 1);
        r->slot[i] = *p; r->valid |= 1u << i; r->asked &= ~(1u << i);
        if (!r->deadline_us) { r->deadline_us = now_us + r->wait_us; r->nack2_us = now_us + r->wait_us / 2; }
        ro__request(r, false);
    }
    ro_poll(r, now_us);
    return res;
}
