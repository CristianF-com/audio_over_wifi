/* Pruebas del módulo de reordenación/NACK del firmware (esp32/main/reorder.h), en el PC. */
#include <stdio.h>
#include <stdlib.h>
#include "../esp32/main/reorder.h"
static int fails = 0;
#define CHECK(n, c) do { int _c = (c); printf("%s %s\n", _c ? "PASS" : "FAIL", n); if (!_c) fails++; } while (0)

static uint32_t out[4096]; static int nout;
static uint32_t nk[256]; static int nnk, nk_calls, nk_max;
static void deliver(void* c, const AudioPacket* p) { (void)c; out[nout++] = p->seq; }
static void nack(void* c, const uint32_t* s, int n) { (void)c; nk_calls++; if (n > nk_max) nk_max = n; for (int i = 0; i < n; i++) nk[nnk++] = s[i]; }
static reorder_t R;
static void setup(void) { nout = nnk = nk_calls = nk_max = 0; ro_init(&R, deliver, nack, NULL, 20000); }
static void push(uint32_t seq, int64_t t) { AudioPacket p; memset(&p, 0, sizeof p); p.seq = seq; ro_push(&R, &p, t); }
static int ordered(void) { for (int i = 1; i < nout; i++) if ((int32_t)(out[i] - out[i-1]) != 1) return 0; return 1; }

int main(void) {
    { setup(); for (uint32_t s = 100; s < 200; s++) push(s, s * 2500);
      CHECK("en orden: 100 entregados, sin NACK, sin pérdidas", nout == 100 && ordered() && nk_calls == 0 && R.lost == 0 && R.recovered == 0); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 50; s++) push(s, t += 2500);
      for (uint32_t s = 51; s < 54; s++) push(s, t += 2500);                  /* falta el 50 */
      CHECK("pérdida simple: NACK de [50] una vez", nnk == 1 && nk[0] == 50 && nk_calls == 1);
      CHECK("pérdida simple: 51..53 retenidos hasta el reenvío", nout == 50);
      push(50, t += 2500);
      CHECK("reenvío: todo en orden (54 entregados), recuperado=1, perdidos=0", nout == 54 && ordered() && R.recovered == 1 && R.lost == 0);
      for (uint32_t s = 54; s < 60; s++) push(s, t += 2500);
      CHECK("sigue entregando en orden tras recuperar", nout == 60 && ordered()); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 60; s++) push(s, t += 2500);
      for (uint32_t s = 63; s < 66; s++) push(s, t += 2500);                  /* faltan 60,61,62 */
      CHECK("ráfaga de 3: NACK con 60,61,62", nnk == 3 && nk[0] == 60 && nk[1] == 61 && nk[2] == 62);
      push(62, t += 1000); push(60, t += 1000); push(61, t += 1000);          /* llegan desordenados */
      CHECK("ráfaga recuperada fuera de orden: 66 entregados en orden", nout == 66 && ordered() && R.recovered == 3 && R.lost == 0); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 70; s++) push(s, t += 2500);
      for (uint32_t s = 71; s < 75; s++) push(s, t += 2500);                  /* el 70 nunca llega */
      CHECK("sin reenvío: retenidos mientras hay plazo", nout == 70);
      ro_poll(&R, t + 5000);  CHECK("antes del plazo no se entrega", nout == 70);
      CHECK("reintento de NACK a mitad de espera", nk_calls >= 2 && nk[nnk - 1] == 70);
      ro_poll(&R, t + 25000);
      CHECK("tras el plazo: se entrega 71..74 y el 70 cuenta como perdido", nout == 74 && R.lost == 1 && out[70] == 71);
      push(70, t + 30000);
      CHECK("reenvío fuera de plazo: descartado (duplicado tardío)", nout == 74 && R.dups == 1);
      push(75, t + 31000); CHECK("el flujo continúa", nout == 75 && out[74] == 75); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 80; s++) push(s, t += 2500);
      push(81, t += 2500); push(80, t += 200);
      CHECK("simple intercambio 81,80: entrega 80,81 en orden", nout == 82 && ordered()); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 20; s++) push(s, t += 2500);
      push(20, t); push(20, t);
      CHECK("duplicado: se ignora", nout == 21 && R.dups == 1); }
    { setup(); int64_t t = 0; for (uint32_t s = 5000; s < 5010; s++) push(s, t += 2500);
      for (uint32_t s = 0; s < 5; s++) push(s, t += 2500);
      CHECK("la RPi se reinicia (seq vuelve a 0): resincroniza y entrega", nout == 15 && out[10] == 0 && out[14] == 4 && R.resyncs == 1); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 10; s++) push(s, t += 2500);
      push(40, t += 2500);
      CHECK("corte largo (>16 paquetes): resincroniza sin colgarse", nout == 11 && out[10] == 40 && R.resyncs == 1);
      push(41, t += 2500); CHECK("…y continúa", nout == 12 && out[11] == 41); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 10; s++) push(s, t += 2500);
      push(20, t += 2500);                                                    /* faltan 10..19 (10 huecos) */
      CHECK("nunca se piden más de NACK_MAX_SEQS por paquete", nk_max <= NACK_MAX_SEQS && nnk == NACK_MAX_SEQS); }
    { setup(); int64_t t = 0; for (uint32_t s = 0xFFFFFFF0u; s != 0x10u; s++) push(s, t += 2500);
      CHECK("desbordamiento de seq (32 bits) sin problemas", nout == 32 && ordered()); }
    { setup(); int64_t t = 0; for (uint32_t s = 0; s < 5; s++) push(s, t += 2500);
      ro_reset(&R); for (uint32_t s = 900; s < 905; s++) push(s, t += 2500);
      CHECK("ro_reset: nueva secuencia aceptada sin resync", nout == 10 && R.resyncs == 0); }
    printf("RESULTADO REORDER: %s\n", fails ? "HAY FALLOS" : "TODO OK");
    return fails ? 1 : 0;
}
