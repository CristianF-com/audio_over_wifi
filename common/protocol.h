/**
 * protocol.h — Protocolo UDP SonosWiFi v5 (ÚNICA copia; la usan RPi y ESP32)
 * ==========================================================================
 * Puertos
 *   5005  control  : HELLO, WELCOME, CONFIG, REPORT, SYNC_REQ, SYNC_RESP
 *                    (la RPi escucha en 5005; cada ESP también escucha en 5005)
 *   5006  audio    : AUDIO unicast RPi -> ESP
 *
 * Reloj maestro = CLOCK_MONOTONIC_RAW de la RPi, en µs.
 * Cada AUDIO lleva el instante maestro de captura de su primer frame.
 * El ESP lo reproduce en  timestamp_us + PLAYOUT_DELAY_US  (hora maestra),
 * así todos los altavoces suenan alineados aunque lleguen en momentos distintos.
 *
 * v3: CONFIG lleva playout_delay_us (latencia ajustable en caliente, PLAYOUT_MIN_US..MAX_US).
 * v4: hasta 10 receptores (esp_id 0..9) y paquete PKT_AUDIO_MONO para el modo «1 ESP por canal»:
       mismo tamaño (500 B) pero con 240 frames MONO (5 ms) en vez de 120 estéreo, así 10 ESP mono
       cuestan los mismos paquetes/s que 5 ESP estéreo. El receptor lo duplica a L+R.
 * v5: recuperación de pérdidas. El ESP detecta huecos en seq y pide el reenvío con PKT_NACK (hasta 8 seq);
 *     la RPi guarda los últimos TX_HISTORY_PACKETS paquetes por nodo y reenvía el mismo paquete (mismo seq/ts).
 *     REPORT incluye recovered_packets. Con la red limpia no cuesta ni un byte extra.
 * Volumen, crossover y ecualización: se aplican SOLO en la RPi. El ESP no toca las muestras.
 * (antes:) Volumen: se aplica SOLO en la RPi. El ESP no escala muestras.
 * Todo en little-endian (RPi y ESP32-C5 lo son). Structs empaquetados.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
  #define PROTO_ASSERT(c, m) static_assert(c, m)
#else
  #define PROTO_ASSERT(c, m) _Static_assert(c, m)
#endif

#define PROTOCOL_VERSION    5
#define PROTOCOL_MAGIC      0x534E4657u   /* "SNFW" */

#define PORT_DISCOVERY      5005
#define PORT_AUDIO          5006

#define AUDIO_SAMPLE_RATE   48000
#define AUDIO_CHANNELS      10            /* canales ALSA de la Traktor */
#define MAX_NODES           10            /* 5 pares estéreo ó 10 ESP mono */

/* 120 frames = 2.5 ms; 120 divide exacto a 240/480 (períodos ALSA de 5/10 ms) */
#define FRAMES_PER_PACKET   120
#define AUDIO_PKT_SAMPLES   (FRAMES_PER_PACKET * 2)   /* L,R intercalados */

#define PLAYOUT_DELAY_US    80000         /* latencia por defecto captura -> altavoz   */
#define PLAYOUT_MIN_US      40000         /* rango ajustable en caliente (web UI)      */
#define PLAYOUT_MAX_US      200000
#define RX_QUEUE_PACKETS    96            /* cola ESP: 240 ms >= PLAYOUT_MAX_US        */

#define PKT_HELLO       0x01   /* ESP -> broadcast:5005                 */
#define PKT_WELCOME     0x02   /* RPi -> ESP:5005 (unicast)             */
#define PKT_AUDIO       0x03   /* RPi -> ESP:5006                       */
#define PKT_REPORT      0x04   /* ESP -> RPi:5005                       */
#define PKT_CONFIG      0x05   /* RPi -> ESP:5005                       */
#define PKT_SYNC_REQ    0x06   /* ESP -> RPi:5005                       */
#define PKT_SYNC_RESP   0x07   /* RPi -> ESP:5005                       */
#define PKT_AUDIO_MONO  0x08   /* RPi -> ESP:5006 (240 frames mono; num_samples = 240) */
#define PKT_NACK        0x09   /* ESP -> RPi:5005 (pide reenvío de paquetes de audio)  */

#define NACK_MAX_SEQS       8
#define TX_HISTORY_PACKETS  128   /* RPi: paquetes recordados por nodo (potencia de 2; 128 = 0.6 s mono / 0.3 s estéreo) */

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_HELLO */
    uint8_t  esp_id;       /* 0..9 */
    uint8_t  version;      /* PROTOCOL_VERSION */
    uint8_t  reserved;
    uint16_t audio_port;   /* host order */
    uint8_t  mac[6];
} HelloPacket;             /* 16 */

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_WELCOME */
    uint8_t  esp_id;
    uint8_t  ch_left;
    uint8_t  ch_right;
    uint8_t  volume_l;
    uint8_t  volume_r;
    uint8_t  enabled;
    uint8_t  reserved;
    uint32_t sample_rate;
    uint32_t playout_delay_us;
} WelcomePacket;           /* 20 */

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_CONFIG */
    uint8_t  esp_id;
    uint8_t  ch_left;
    uint8_t  ch_right;
    uint8_t  volume_l;
    uint8_t  volume_r;
    uint8_t  enabled;
    uint8_t  reserved;
    uint32_t playout_delay_us;   /* latencia global; 0 = sin cambio */
} ConfigPacket;            /* 16 */

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_AUDIO */
    uint8_t  esp_id;
    uint16_t num_samples;  /* = AUDIO_PKT_SAMPLES (L+R) */
    uint32_t seq;          /* monotónico por ESP */
    uint64_t timestamp_us; /* hora maestra del primer frame */
    int16_t  samples[AUDIO_PKT_SAMPLES];
} AudioPacket;             /* 20 + 480 = 500. Con pkt_type = PKT_AUDIO_MONO: samples[] son 240 frames mono */

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_REPORT */
    uint8_t  esp_id;
    uint8_t  reserved[2];
    int64_t  clock_offset_us;  /* maestro - local */
    int32_t  rtt_us;           /* mejor RTT de sync */
    int32_t  jitter_us;        /* estilo RFC3550 */
    int32_t  buffer_frames;    /* frames en cola */
    int32_t  sync_err_us;      /* error de alineación filtrado (+ = tarde) */
    uint32_t lost_packets;
    uint32_t late_packets;     /* descartados por llegar fuera de plazo */
    uint32_t underruns;
    float    drift_ppm;        /* corrección neta aplicada (+ = descarta) */
    uint32_t recovered_packets;/* recuperados por reenvío/reordenación (v5) */
} ReportPacket;            /* 52 */

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_NACK */
    uint8_t  esp_id;
    uint8_t  count;        /* 1..NACK_MAX_SEQS */
    uint8_t  reserved;
    uint32_t seq[NACK_MAX_SEQS];
} NackPacket;              /* 40 */

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_SYNC_REQ */
    uint8_t  esp_id;
    uint8_t  reserved[2];
    uint64_t t1_us;        /* reloj local ESP al enviar */
} SyncReqPacket;           /* 16 */

typedef struct {
    uint32_t magic;
    uint8_t  pkt_type;     /* PKT_SYNC_RESP */
    uint8_t  esp_id;
    uint8_t  reserved[2];
    uint64_t t1_us;        /* eco */
    uint64_t t2_us;        /* maestro: recepción */
    uint64_t t3_us;        /* maestro: envío */
} SyncRespPacket;          /* 32 */

#pragma pack(pop)

PROTO_ASSERT(sizeof(HelloPacket)    == 16, "HelloPacket");
PROTO_ASSERT(sizeof(WelcomePacket)  == 20, "WelcomePacket");
PROTO_ASSERT(sizeof(ConfigPacket)   == 16, "ConfigPacket");
PROTO_ASSERT(sizeof(AudioPacket)    == 500, "AudioPacket");
PROTO_ASSERT(sizeof(ReportPacket)   == 52, "ReportPacket");
PROTO_ASSERT(sizeof(NackPacket)     == 40, "NackPacket");
PROTO_ASSERT(sizeof(SyncReqPacket)  == 16, "SyncReqPacket");
PROTO_ASSERT(sizeof(SyncRespPacket) == 32, "SyncRespPacket");
