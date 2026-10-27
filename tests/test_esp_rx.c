/* Prueba la tarea REAL de recepción del firmware (audio_rx_task + rx_deliver + rx_nack, extraídas de
 * receiver_main.c por run_tests.sh) en el PC con sockets UDP en loopback y un «RPi» simulado que descarta
 * paquetes y atiende los NACK. Las funciones de FreeRTOS/ESP se sustituyen por stubs mínimos. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocol.h"
#include "reorder.h"

typedef struct { uint64_t ts; int16_t s[AUDIO_PKT_SAMPLES]; } qitem_t;
typedef int portMUX_TYPE;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x)  ((void)(x))
#define pdTRUE 1
static int g_q, g_mux;
static uint8_t g_receiver_id = 3;
static volatile bool g_welcomed = true, g_sync_valid = true, g_enabled = true;
static volatile uint32_t g_late = 0, g_rx = 0, g_lost = 0, g_playout_us = 80000;
static volatile int32_t g_jitter_us = 0;
static int64_t g_offset_us = 0;
static volatile bool g_ro_reset = false;
static reorder_t g_ro;
static int g_ctrl_sock = -1, g_audio_sock = -1;
static struct sockaddr_in g_rpi_addr;

static qitem_t items[4096]; static int nitems;
static int xQueueSend(int q, const void* it, int t) { (void)q; (void)t; items[nitems++] = *(const qitem_t*)it; return pdTRUE; }
static int64_t esp_timer_get_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000; }

#include "/tmp/esp_rx_section.inc"

static int fails = 0;
#define CHECK(n, c) do { int _c = (c); printf("%s %s\n", _c ? "PASS" : "FAIL", n); if (!_c) fails++; } while (0)

static int tx_sock, ctrl_rx; static struct sockaddr_in esp_audio;
static int nack_pkts, retx_sent, never_retx = 90;
static AudioPacket store[400];
static void* nack_server(void* a) {
    (void)a;
    for (;;) {
        NackPacket k; int n = recv(ctrl_rx, &k, sizeof k, 0);
        if (n != (int)sizeof(NackPacket) || k.magic != PROTOCOL_MAGIC || k.pkt_type != PKT_NACK || k.esp_id != 3) continue;
        nack_pkts++;
        for (int i = 0; i < k.count; i++) if ((int)k.seq[i] != never_retx) {
            sendto(tx_sock, &store[k.seq[i]], sizeof(AudioPacket), 0, (struct sockaddr*)&esp_audio, sizeof esp_audio); retx_sent++; }
    }
    return NULL;
}
static void mk(uint32_t seq, bool mono) {
    AudioPacket* p = &store[seq]; memset(p, 0, sizeof *p);
    p->magic = PROTOCOL_MAGIC; p->pkt_type = mono ? PKT_AUDIO_MONO : PKT_AUDIO; p->esp_id = 3;
    p->num_samples = AUDIO_PKT_SAMPLES; p->seq = seq; p->timestamp_us = 1000000 + (uint64_t)seq * (mono ? 5000 : 2500);
    for (int i = 0; i < AUDIO_PKT_SAMPLES; i++) p->samples[i] = (int16_t)(seq * 10 + (mono ? i : i / 2) % 7);
}
static void sendpkt(uint32_t seq) { sendto(tx_sock, &store[seq], sizeof(AudioPacket), 0, (struct sockaddr*)&esp_audio, sizeof esp_audio); }

int main(void) {
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_addr.s_addr = inet_addr("127.0.0.1");
    g_audio_sock = socket(AF_INET, SOCK_DGRAM, 0); a.sin_port = 0; bind(g_audio_sock, (struct sockaddr*)&a, sizeof a);
    socklen_t l = sizeof a; getsockname(g_audio_sock, (struct sockaddr*)&a, &l); esp_audio = a;
    struct timeval tv = {0, 10000}; setsockopt(g_audio_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    g_ctrl_sock = socket(AF_INET, SOCK_DGRAM, 0);
    ctrl_rx = socket(AF_INET, SOCK_DGRAM, 0); struct sockaddr_in c = {0}; c.sin_family = AF_INET; c.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(ctrl_rx, (struct sockaddr*)&c, sizeof c); l = sizeof c; getsockname(ctrl_rx, (struct sockaddr*)&c, &l); g_rpi_addr = c;
    tx_sock = socket(AF_INET, SOCK_DGRAM, 0);
    pthread_t t1, t2; pthread_create(&t1, NULL, nack_server, NULL); pthread_create(&t2, NULL, (void*(*)(void*))audio_rx_task, NULL);

    /* A) estéreo: 120 paquetes, faltan 50, 60-62 (se recuperan) y 90 (el «RPi» nunca lo reenvía) */
    for (uint32_t s = 0; s < 120; s++) mk(s, false);
    for (uint32_t s = 0; s < 120; s++) {
        if (s == 50 || s == 60 || s == 61 || s == 62 || s == 90) { continue; }
        sendpkt(s); usleep(2500);
    }
    usleep(120000);
    int gaps = 0, inorder = 1;
    for (int i = 1; i < nitems; i++) { int64_t d = (int64_t)items[i].ts - (int64_t)items[i-1].ts; if (d == 5000) gaps++; else if (d != 2500) inorder = 0; }
    CHECK("estéreo: todo llega en orden de timestamp", inorder && nitems > 100);
    CHECK("estéreo: solo queda el hueco del 90 (4 de 5 pérdidas recuperadas)", nitems == 119 && gaps == 1);
    CHECK("estéreo: contadores recuperados=4, perdidos=1", g_ro.recovered == 4 && g_lost == 1);
    CHECK("el ESP envió NACK y la RPi reenvió", nack_pkts >= 2 && retx_sent >= 4);
    { int ok = 0; for (int i = 0; i < nitems; i++) if (items[i].ts == 1000000 + 61 * 2500ULL) ok = (items[i].s[0] == 610 && items[i].s[239] == 610 + (119 % 7));
      CHECK("muestras de un paquete recuperado (seq 61) intactas", ok); }

    /* B) modo mono: 20 paquetes PKT_AUDIO_MONO (240 frames) -> 40 trozos estéreo L=R, ts cada 2500 µs; con un reenvío */
    int base = nitems;
    for (uint32_t s = 120; s < 140; s++) mk(s, true);
    for (uint32_t s = 120; s < 140; s++) { if (s == 130) continue; sendpkt(s); usleep(5000); }
    usleep(120000);
    int lr = 1, sp = 1;
    for (int i = base; i < nitems; i++) { for (int f = 0; f < FRAMES_PER_PACKET; f++) if (items[i].s[2*f] != items[i].s[2*f+1]) lr = 0;
        if (i > base && (int64_t)items[i].ts - (int64_t)items[i-1].ts != 2500) sp = 0; }
    CHECK("mono: 20 paquetes -> 40 trozos (con el 130 recuperado)", nitems - base == 40);
    CHECK("mono: L == R en todas las muestras", lr);
    CHECK("mono: trozos consecutivos separados 2500 µs (sin huecos)", sp);
    CHECK("mono: primera muestra = primera del paquete; la 120ª = segunda mitad",
          items[base].s[0] == 1200 + 0 && items[base + 1].s[0] == (int16_t)(1200 + 120 % 7));

    /* C) deshabilitado: no entra nada en la cola */
    g_enabled = false; int b2 = nitems;
    for (uint32_t s = 140; s < 150; s++) { mk(s, false); sendpkt(s); usleep(2500); }
    usleep(50000); g_enabled = true;
    CHECK("nodo deshabilitado: no se encola audio", nitems == b2);
    printf("RESULTADO ESP-RX: %s\n", fails ? "HAY FALLOS" : "TODO OK");
    return fails ? 1 : 0;
}
