/**
 * receiver_main.c — SonosWiFi receptor para XIAO ESP32-C5 (ESP-IDF >= 5.5, C)
 * ===========================================================================
 * Diferencias clave frente a la versión para ESP32 clásico:
 *  - ESP32-C5 = UN solo núcleo RISC-V  -> xTaskCreate (sin pin a core 1).
 *  - No hay APLL                       -> I2S_CLK_SRC_DEFAULT; el desvío de reloj
 *                                         se corrige por software (ver i2s_task).
 *  - Pines I2S configurables (menuconfig), por defecto D8/D9/D10 de la XIAO.
 *
 * Sincronización entre altavoces
 *  1. control_task hace ping/pong NTP-like con la RPi (SYNC_REQ/SYNC_RESP) y
 *     mantiene g_offset_us = reloj_maestro - reloj_local (mejor RTT de 8 muestras).
 *  2. Cada AUDIO trae el instante maestro de captura (timestamp_us). Debe sonar
 *     en  timestamp_us + playout_delay  (hora maestra).
 *  3. i2s_task compara ese objetivo con la hora estimada de salida del chunk:
 *        err = (ahora_maestro + DMA_LAT) - (ts + playout)       (+ = tarde)
 *     |err| > 5 ms : descarta paquete (tarde) / escribe silencio (pronto)
 *     |err| ≤ 5 ms : corrige con 1 frame descartado/repetido cada SLEW_EVERY chunks.
 *  Todos los nodos usan la misma regla -> error mutuo típico < ~200 µs, a falta de
 *  calibrar DMA_LAT (es modo común, no afecta a la sincronía entre nodos).
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/i2s_std.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "sdkconfig.h"

#include <math.h>
#include "protocol.h"
#include "reorder.h"
#include "ble_prov.h"

static const char* TAG = "RECV";

/* ─── Configuración (Kconfig; ver main/Kconfig.projbuild) ──────────────────── */
#ifndef CONFIG_RECEIVER_ID
  #define CONFIG_RECEIVER_ID 0
#endif
#ifndef CONFIG_WIFI_SSID_DEFAULT
  #define CONFIG_WIFI_SSID_DEFAULT "TuRedWiFi"
#endif
#ifndef CONFIG_WIFI_PASS_DEFAULT
  #define CONFIG_WIFI_PASS_DEFAULT "TuPassword"
#endif
#ifndef CONFIG_SONOS_RPI_IP
  #define CONFIG_SONOS_RPI_IP ""      /* vacío = descubrimiento solo por broadcast */
#endif
#ifndef CONFIG_I2S_BCK_GPIO
  #define CONFIG_I2S_BCK_GPIO 8      /* XIAO D8  = GPIO8  */
#endif
#ifndef CONFIG_I2S_WS_GPIO
  #define CONFIG_I2S_WS_GPIO 9       /* XIAO D9  = GPIO9  */
#endif
#ifndef CONFIG_I2S_DOUT_GPIO
  #define CONFIG_I2S_DOUT_GPIO 10    /* XIAO D10 = GPIO10 */
#endif

#ifndef CONFIG_SONOS_BLE_ENABLE
  #define CONFIG_SONOS_BLE_ENABLE 1
#endif
#ifndef CONFIG_SONOS_BLE_WINDOW_S
  #define CONFIG_SONOS_BLE_WINDOW_S 300
#endif
#define FW_VERSION          5

#define I2S_DMA_DESC        6
#define I2S_DMA_FRAMES      FRAMES_PER_PACKET
/* Latencia estimada write()->altavoz en régimen (cola DMA casi llena). Modo común. */
#define DMA_LAT_US  ((int64_t)(I2S_DMA_DESC * I2S_DMA_FRAMES - I2S_DMA_FRAMES / 2) \
                     * 1000000 / AUDIO_SAMPLE_RATE)

#define CHUNK_US            ((int64_t)FRAMES_PER_PACKET * 1000000 / AUDIO_SAMPLE_RATE)   /* 2500 µs */
#define PLC_MAX_K           8        /* oculta hasta 8 paquetes perdidos seguidos (20 ms) */
#define ERR_SKIP_US         5000     /* fuera de esto: descartar / silencio     */
#define SLEW_DEADBAND_US    100      /* dentro de esto no se corrige            */
#define SLEW_EVERY          4        /* 1 frame cada 4 chunks = ~2083 ppm máx   */
#define FRAME_US            21       /* 1 frame @48 kHz ≈ 20.8 µs               */
#define SYNC_WIN            8
#define WELCOME_TIMEOUT_US  10000000LL

/* ─── Estado ───────────────────────────────────────────────────────────────── */
typedef struct { uint64_t ts; int16_t s[AUDIO_PKT_SAMPLES]; } qitem_t;

static uint8_t  g_receiver_id = CONFIG_RECEIVER_ID;
static char     g_ssid[33], g_pass[65], g_rpi_ip_cfg[16];

static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t  g_ch_left, g_ch_right, g_vol_l = 255, g_vol_r = 255;   /* informativos */
static volatile bool     g_enabled  = true;
static volatile bool     g_welcomed = false;
static volatile bool     g_flush    = false;
static volatile uint32_t g_playout_us = PLAYOUT_DELAY_US;

/* Reloj */
static int64_t           g_offset_us  = 0;       /* maestro - local (protegido por g_mux) */
static volatile int32_t  g_rtt_us     = 0;
static volatile bool     g_sync_valid = false;

/* Métricas */
static volatile uint32_t g_rx = 0, g_lost = 0, g_late = 0, g_underruns = 0;
static volatile bool     g_ro_reset = false;        /* lo atiende audio_rx_task (nueva secuencia) */
static reorder_t         g_ro;                      /* reordenación + NACK (solo lo toca audio_rx_task) */
static volatile int32_t  g_jitter_us = 0, g_sync_err_us = 0;
static volatile uint32_t g_corr_drop = 0, g_corr_ins = 0;
static volatile uint32_t g_frames_played = 0;
static volatile uint32_t g_concealed = 0;          /* chunks rellenados por PLC */
static volatile float    g_drift_ppm = 0.0f;

static volatile bool     g_wifi_up = false;
static char              g_ip_str[16] = "";
static volatile int64_t  g_identify_until_us = 0;   /* pitido de identificación */

static struct sockaddr_in g_rpi_addr;            /* RPi:5005 */
static int g_ctrl_sock = -1, g_audio_sock = -1;
static QueueHandle_t     g_q   = NULL;
static i2s_chan_handle_t g_i2s = NULL;

static inline int64_t master_now_us(void)
{
    portENTER_CRITICAL(&g_mux);
    int64_t off = g_offset_us;
    portEXIT_CRITICAL(&g_mux);
    return esp_timer_get_time() + off;
}

/* ─── Sync de reloj ────────────────────────────────────────────────────────── */
static struct { int32_t rtt; int64_t off; } s_win[SYNC_WIN];
static int s_cnt = 0, s_idx = 0;

static void sync_reset(void)
{
    s_cnt = s_idx = 0;
    g_sync_valid = false;
}

static void sync_add(int64_t t1, int64_t t2, int64_t t3, int64_t t4)
{
    int64_t rtt = (t4 - t1) - (t3 - t2);
    if (rtt < 0 || rtt > 200000) return;                 /* muestra inútil */
    int64_t off = ((t2 - t1) + (t3 - t4)) / 2;

    int64_t cur;
    portENTER_CRITICAL(&g_mux); cur = g_offset_us; portEXIT_CRITICAL(&g_mux);
    if (g_sync_valid && llabs(off - cur) > 100000) {     /* la RPi se reinició */
        ESP_LOGW(TAG, "Salto de reloj maestro (%lld ms) — resincronizando",
                 (long long)((off - cur) / 1000));
        sync_reset();
        g_flush = true;
    }
    s_win[s_idx].rtt = (int32_t)rtt;
    s_win[s_idx].off = off;
    s_idx = (s_idx + 1) % SYNC_WIN;
    if (s_cnt < SYNC_WIN) s_cnt++;

    int best = 0;                                        /* menor RTT = menos asimetría */
    for (int i = 1; i < s_cnt; i++)
        if (s_win[i].rtt < s_win[best].rtt) best = i;

    portENTER_CRITICAL(&g_mux);
    g_offset_us = s_win[best].off;
    portEXIT_CRITICAL(&g_mux);
    g_rtt_us = s_win[best].rtt;
    if (s_cnt >= 3) g_sync_valid = true;
}

/* ─── WiFi ─────────────────────────────────────────────────────────────────── */
static EventGroupHandle_t g_wifi_evt;
#define WIFI_CONNECTED_BIT BIT0
static esp_timer_handle_t g_wifi_retry_timer;
static int s_retry = 0;

static void wifi_retry_cb(void* arg) { esp_wifi_connect(); }

/* Banda del C5 (no puede usar 2.4 y 5 GHz a la vez). Por defecto SOLO 5 GHz: con un SSID compartido
 * entre bandas, el escaneo "por señal" suele elegir 2.4 GHz (más señal a distancia) y perderías la ventaja. */
static void apply_band_mode(void)
{
#if defined(CONFIG_SONOS_WIFI_BAND_2G)
    const wifi_band_mode_t m = WIFI_BAND_MODE_2G_ONLY;  const char* n = "2.4 GHz";
#elif defined(CONFIG_SONOS_WIFI_BAND_AUTO)
    const wifi_band_mode_t m = WIFI_BAND_MODE_AUTO;     const char* n = "auto";
#else
    const wifi_band_mode_t m = WIFI_BAND_MODE_5G_ONLY;  const char* n = "5 GHz";
#endif
    esp_err_t e = esp_wifi_set_band_mode(m);
    if (e != ESP_OK) ESP_LOGW(TAG, "set_band_mode(%s) falló: %d", n, (int)e);
    else ESP_LOGI(TAG, "Banda WiFi: solo %s", n);
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        apply_band_mode();                               /* antes del primer connect() */
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(g_wifi_evt, WIFI_CONNECTED_BIT);
        g_wifi_up = false;
        wifi_event_sta_disconnected_t* d = (wifi_event_sta_disconnected_t*)data;
        int64_t delay_us = (s_retry < 10 ? (int64_t)(s_retry + 1) * 1000000 : 10000000);   /* 1 s … 10 s, sin límite */
        s_retry++;
        ESP_LOGW(TAG, "WiFi desconectado (motivo %d) — reintento en %d s", d ? d->reason : -1, (int)(delay_us / 1000000));
        esp_timer_start_once(g_wifi_retry_timer, delay_us);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* e = (ip_event_got_ip_t*)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
        snprintf(g_ip_str, sizeof(g_ip_str), IPSTR, IP2STR(&e->ip_info.ip));
        s_retry = 0; g_wifi_up = true;
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
            ESP_LOGI(TAG, "AP canal %d (%s GHz) RSSI %d", ap.primary, ap.primary >= 36 ? "5" : "2.4", ap.rssi);
        xEventGroupSetBits(g_wifi_evt, WIFI_CONNECTED_BIT);
    }
}

static void load_config(void)
{
    strlcpy(g_ssid, CONFIG_WIFI_SSID_DEFAULT, sizeof(g_ssid));
    strlcpy(g_pass, CONFIG_WIFI_PASS_DEFAULT, sizeof(g_pass));
    strlcpy(g_rpi_ip_cfg, CONFIG_SONOS_RPI_IP, sizeof(g_rpi_ip_cfg));
    nvs_handle_t nh;
    if (nvs_open("config", NVS_READONLY, &nh) == ESP_OK) {
        uint8_t id = g_receiver_id;
        size_t sl = sizeof(g_ssid), pl = sizeof(g_pass);
        if (nvs_get_u8(nh, "receiver_id", &id) == ESP_OK && id < MAX_NODES) g_receiver_id = id;
        nvs_get_str(nh, "wifi_ssid", g_ssid, &sl);
        nvs_get_str(nh, "wifi_pass", g_pass, &pl);
        size_t rl = sizeof(g_rpi_ip_cfg);
        nvs_get_str(nh, "rpi_ip", g_rpi_ip_cfg, &rl);
        nvs_close(nh);
    }
}

static void wifi_start(void)
{
    g_wifi_evt = xEventGroupCreate();
    const esp_timer_create_args_t ta = { .callback = wifi_retry_cb, .name = "wifi_retry" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &g_wifi_retry_timer));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    wifi_config_t wc;
    memset(&wc, 0, sizeof(wc));
    strlcpy((char*)wc.sta.ssid,     g_ssid, sizeof(wc.sta.ssid));
    strlcpy((char*)wc.sta.password, g_pass, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;      /* mínimo WPA2; acepta también WPA3 */
    wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;              /* routers en modo solo-WPA3 */
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;          /* C5: escanea 2.4 y 5 GHz */
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wc.sta.pmf_cfg.capable  = true;
    wc.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));      /* imprescindible para baja jitter */

    ESP_LOGI(TAG, "Conectando a WiFi '%s'...", g_ssid);
}
static void wifi_wait(void)
{
    xEventGroupWaitBits(g_wifi_evt, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}

/* ─── Configuración persistente + callbacks del BLE ────────────────────────── */
static int cfg_save(const char* ssid, const char* pass, uint8_t id, const char* rpi_ip)
{
    nvs_handle_t nh;
    if (id >= MAX_NODES || nvs_open("config", NVS_READWRITE, &nh) != ESP_OK) return -1;
    esp_err_t e = nvs_set_str(nh, "wifi_ssid", ssid);
    if (e == ESP_OK && pass) e = nvs_set_str(nh, "wifi_pass", pass);
    if (e == ESP_OK) e = nvs_set_u8(nh, "receiver_id", id);
    if (e == ESP_OK) e = nvs_set_str(nh, "rpi_ip", rpi_ip ? rpi_ip : "");
    if (e == ESP_OK) e = nvs_commit(nh);
    nvs_close(nh);
    return e == ESP_OK ? 0 : -1;
}
static int cfg_factory(void)
{
    nvs_handle_t nh;
    if (nvs_open("config", NVS_READWRITE, &nh) != ESP_OK) return -1;
    nvs_erase_all(nh); nvs_commit(nh); nvs_close(nh);
    return 0;
}
static void restart_cb(void* arg) { esp_restart(); }
static void cfg_reboot(void)                              /* diferido: deja salir el ACK de la escritura BLE */
{
    static esp_timer_handle_t t;
    const esp_timer_create_args_t ta = { .callback = restart_cb, .name = "restart" };
    if (!t && esp_timer_create(&ta, &t) != ESP_OK) { esp_restart(); return; }
    esp_timer_start_once(t, 800000);
}
static void cfg_identify(void) { g_identify_until_us = esp_timer_get_time() + 1500000; }
static size_t status_json(char* buf, size_t len)
{
    wifi_ap_record_t ap; int rssi = 0, chan = 0;
    if (g_wifi_up && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) { rssi = ap.rssi; chan = ap.primary; }
    int n = snprintf(buf, len,
        "{\"fw\":%d,\"id\":%d,\"wifi\":%d,\"ip\":\"%s\",\"ch\":%d,\"rssi\":%d,\"rpi\":%d,\"sync\":%d,"
        "\"rtt\":%d,\"late\":%u,\"under\":%u,\"err\":%d}",
        FW_VERSION, g_receiver_id, g_wifi_up ? 1 : 0, g_ip_str, chan, rssi, g_welcomed ? 1 : 0, g_sync_valid ? 1 : 0,
        (int)g_rtt_us, (unsigned)g_late, (unsigned)g_underruns, (int)g_sync_err_us);
    return (n < 0) ? 0 : ((size_t)n >= len ? len - 1 : (size_t)n);
}
static const ble_prov_ops_t g_ble_ops = {
    .save = cfg_save, .identify = cfg_identify, .reboot = cfg_reboot,
    .factory = cfg_factory, .status_json = status_json,
};

/* ─── Sockets ──────────────────────────────────────────────────────────────── */
static bool sockets_init(void)
{
    int yes = 1;
    g_ctrl_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_ctrl_sock < 0) return false;
    setsockopt(g_ctrl_sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    setsockopt(g_ctrl_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct timeval tv = {0, 100000};                     /* 100 ms */
    setsockopt(g_ctrl_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(PORT_DISCOVERY),
                             .sin_addr = { .s_addr = htonl(INADDR_ANY) } };
    if (bind(g_ctrl_sock, (struct sockaddr*)&a, sizeof(a)) < 0) {
        ESP_LOGE(TAG, "ctrl bind failed: %d", errno); return false;
    }

    g_audio_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_audio_sock < 0) return false;
    struct timeval tv2 = {0, 10000};                      /* 10 ms: el temporizador de reenvíos corre aunque no lleguen paquetes */
    setsockopt(g_audio_sock, SOL_SOCKET, SO_RCVTIMEO, &tv2, sizeof(tv2));
    a.sin_port = htons(PORT_AUDIO);
    if (bind(g_audio_sock, (struct sockaddr*)&a, sizeof(a)) < 0) {
        ESP_LOGE(TAG, "audio bind failed: %d", errno); return false;
    }
    ESP_LOGI(TAG, "Sockets OK | control :%d | audio :%d", PORT_DISCOVERY, PORT_AUDIO);
    return true;
}

/* ─── I2S ──────────────────────────────────────────────────────────────────── */
static void i2s_init(void)
{
    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch.dma_desc_num  = I2S_DMA_DESC;
    ch.dma_frame_num = I2S_DMA_FRAMES;
    ch.auto_clear_after_cb = true;       /* si el compilador no conoce el campo (IDF<5.4): auto_clear */
    ESP_ERROR_CHECK(i2s_new_channel(&ch, &g_i2s, NULL));

    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),   /* clk_src DEFAULT: el C5 no tiene APLL */
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_I2S_BCK_GPIO,
            .ws   = CONFIG_I2S_WS_GPIO,
            .dout = CONFIG_I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0 },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(g_i2s, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(g_i2s));
    ESP_LOGI(TAG, "I2S OK BCK=%d WS=%d DOUT=%d %dHz 16bit (DMA %dx%d, lat≈%lldus)",
             CONFIG_I2S_BCK_GPIO, CONFIG_I2S_WS_GPIO, CONFIG_I2S_DOUT_GPIO,
             AUDIO_SAMPLE_RATE, I2S_DMA_DESC, I2S_DMA_FRAMES, (long long)DMA_LAT_US);
}

/* ─── Tarea de control: HELLO, WELCOME, CONFIG, SYNC ───────────────────────── */
static void control_task(void* pv)
{
    uint8_t buf[128] __attribute__((aligned(4)));
    struct sockaddr_in src;
    socklen_t slen;

    struct sockaddr_in bcast = { .sin_family = AF_INET, .sin_port = htons(PORT_DISCOVERY),
                                 .sin_addr = { .s_addr = htonl(INADDR_BROADCAST) } };
    HelloPacket hello;
    memset(&hello, 0, sizeof(hello));
    hello.magic = PROTOCOL_MAGIC; hello.pkt_type = PKT_HELLO;
    hello.esp_id = g_receiver_id; hello.version = PROTOCOL_VERSION;
    hello.audio_port = PORT_AUDIO;
    esp_wifi_get_mac(WIFI_IF_STA, hello.mac);

    int64_t last_hello = -10000000, last_sync = 0, last_welcome = 0;
    int sync_sent = 0;
    ESP_LOGI(TAG, "Control | ID=%d", g_receiver_id);

    while (1) {
        int64_t now = esp_timer_get_time();

        if (now - last_hello >= 2000000) {
            last_hello = now;
            sendto(g_ctrl_sock, &hello, sizeof(hello), 0, (struct sockaddr*)&bcast, sizeof(bcast));
            if (g_rpi_ip_cfg[0]) {      /* IP fija: sirve aunque el router filtre el broadcast (aislamiento de clientes, etc.) */
                struct sockaddr_in uni = { .sin_family = AF_INET, .sin_port = htons(PORT_DISCOVERY) };
                if (inet_aton(g_rpi_ip_cfg, &uni.sin_addr))
                    sendto(g_ctrl_sock, &hello, sizeof(hello), 0, (struct sockaddr*)&uni, sizeof(uni));
            }
        }
        if (g_welcomed) {
            if (now - last_welcome > WELCOME_TIMEOUT_US) {
                ESP_LOGW(TAG, "RPi no responde — parando audio, rediscovery");
                g_welcomed = false; sync_reset(); g_flush = true;
            } else {
                int64_t every = (sync_sent < 12) ? 150000 : 1000000;   /* ráfaga inicial */
                if (now - last_sync >= every) {
                    SyncReqPacket q;
                    memset(&q, 0, sizeof(q));
                    q.magic = PROTOCOL_MAGIC; q.pkt_type = PKT_SYNC_REQ; q.esp_id = g_receiver_id;
                    last_sync = now; sync_sent++;
                    q.t1_us = (uint64_t)esp_timer_get_time();
                    sendto(g_ctrl_sock, &q, sizeof(q), 0,
                           (struct sockaddr*)&g_rpi_addr, sizeof(g_rpi_addr));
                }
            }
        }

        slen = sizeof(src);
        int n = recvfrom(g_ctrl_sock, buf, sizeof(buf), 0, (struct sockaddr*)&src, &slen);
        int64_t t4 = esp_timer_get_time();
        if (n < 5) continue;
        uint32_t magic; memcpy(&magic, buf, 4);
        if (magic != PROTOCOL_MAGIC) continue;

        switch (buf[4]) {
        case PKT_WELCOME: {
            if (n < (int)sizeof(WelcomePacket)) break;
            WelcomePacket w; memcpy(&w, buf, sizeof(w));
            if (w.esp_id != g_receiver_id || w.sample_rate != AUDIO_SAMPLE_RATE) break;
            bool new_rpi = !g_welcomed || src.sin_addr.s_addr != g_rpi_addr.sin_addr.s_addr;
            portENTER_CRITICAL(&g_mux);
            g_ch_left = w.ch_left; g_ch_right = w.ch_right;
            g_vol_l = w.volume_l;  g_vol_r = w.volume_r;
            g_enabled = (w.enabled != 0);
            g_playout_us = (w.playout_delay_us >= PLAYOUT_MIN_US && w.playout_delay_us <= PLAYOUT_MAX_US)
                           ? w.playout_delay_us : PLAYOUT_DELAY_US;
            g_rpi_addr = src;
            g_rpi_addr.sin_port = htons(PORT_DISCOVERY);          /* siempre actualizado */
            portEXIT_CRITICAL(&g_mux);
            last_welcome = t4;
            if (new_rpi) {
                sync_reset(); sync_sent = 0; last_sync = 0; g_ro_reset = true; g_flush = true;
                g_welcomed = true;
                ESP_LOGI(TAG, "WELCOME: ch=%d/%d vol=%d/%d en=%d playout=%ums | RPi %s",
                         w.ch_left, w.ch_right, w.volume_l, w.volume_r, w.enabled,
                         (unsigned)g_playout_us, inet_ntoa(src.sin_addr));
            }
            break;
        }
        case PKT_CONFIG: {
            if (n < (int)sizeof(ConfigPacket)) break;
            ConfigPacket c; memcpy(&c, buf, sizeof(c));
            if (c.esp_id != g_receiver_id) break;
            portENTER_CRITICAL(&g_mux);
            g_ch_left = c.ch_left; g_ch_right = c.ch_right;
            g_vol_l = c.volume_l;  g_vol_r = c.volume_r;
            g_enabled = (c.enabled != 0);
            portEXIT_CRITICAL(&g_mux);
            if (!g_enabled) g_flush = true;
            if (c.playout_delay_us >= PLAYOUT_MIN_US && c.playout_delay_us <= PLAYOUT_MAX_US &&
                c.playout_delay_us != g_playout_us) {
                g_playout_us = c.playout_delay_us;                /* i2s_task se reajusta solo (descarta/rellena) */
                ESP_LOGI(TAG, "Playout -> %u ms", (unsigned)(g_playout_us / 1000));
            }
            ESP_LOGI(TAG, "CONFIG: ch=%d/%d vol=%d/%d en=%d", c.ch_left, c.ch_right,
                     c.volume_l, c.volume_r, c.enabled);
            break;
        }
        case PKT_SYNC_RESP: {
            if (n < (int)sizeof(SyncRespPacket)) break;
            SyncRespPacket r; memcpy(&r, buf, sizeof(r));
            if (r.esp_id != g_receiver_id) break;
            sync_add((int64_t)r.t1_us, (int64_t)r.t2_us, (int64_t)r.t3_us, t4);
            break;
        }
        default: break;
        }
    }
}

/* ─── Tarea RX audio ───────────────────────────────────────────────────────── */
static qitem_t g_item;                                   /* solo audio_rx_task */

/* Entrega UN paquete (ya en orden de seq) a la cola de reproducción. */
static void rx_deliver(void* ctx, const AudioPacket* p)
{
    (void)ctx;
    if (!g_welcomed || !g_sync_valid || !g_enabled) return;
    if (p->pkt_type == PKT_AUDIO) {
        g_item.ts = p->timestamp_us;
        memcpy(g_item.s, p->samples, sizeof(g_item.s));
        if (xQueueSend(g_q, &g_item, 0) != pdTRUE) g_late++;      /* cola llena: descartar */
    } else {
        /* Modo «1 ESP por canal»: 240 frames mono = 2 trozos estéreo de 120 frames (L = R).
         * El resto (cola, PLC, alineación) trabaja igual que en estéreo. */
        for (int h = 0; h < 2; h++) {
            const int16_t* m = p->samples + h * FRAMES_PER_PACKET;
            for (int i = 0; i < FRAMES_PER_PACKET; i++) g_item.s[2 * i] = g_item.s[2 * i + 1] = m[i];
            g_item.ts = p->timestamp_us + (uint64_t)h * (FRAMES_PER_PACKET * 1000000ULL / AUDIO_SAMPLE_RATE);
            if (xQueueSend(g_q, &g_item, 0) != pdTRUE) g_late++;
        }
    }
}

/* Pide a la RPi que reenvíe paquetes perdidos. */
static void rx_nack(void* ctx, const uint32_t* seqs, int n)
{
    (void)ctx;
    if (!g_welcomed || !g_enabled || n <= 0 || g_ctrl_sock < 0) return;
    NackPacket k;
    memset(&k, 0, sizeof(k));
    k.magic = PROTOCOL_MAGIC; k.pkt_type = PKT_NACK; k.esp_id = g_receiver_id;
    k.count = (uint8_t)(n > NACK_MAX_SEQS ? NACK_MAX_SEQS : n);
    for (int i = 0; i < k.count; i++) k.seq[i] = seqs[i];
    struct sockaddr_in rpi;
    portENTER_CRITICAL(&g_mux); rpi = g_rpi_addr; portEXIT_CRITICAL(&g_mux);
    sendto(g_ctrl_sock, &k, sizeof(k), 0, (struct sockaddr*)&rpi, sizeof(rpi));
}

static void audio_rx_task(void* pv)
{
    static uint8_t rx[sizeof(AudioPacket) + 32] __attribute__((aligned(4)));
    int64_t prev_transit = 0; bool have_prev = false; float jit = 0;
    ro_init(&g_ro, rx_deliver, rx_nack, NULL, 20000);

    while (1) {
        if (g_ro_reset) { g_ro_reset = false; ro_reset(&g_ro); have_prev = false; }
        /* Espera máx. de reenvío: 1/4 del colchón (80 ms -> 20 ms; 40 ms -> 10 ms). */
        int64_t w = (int64_t)g_playout_us / 4; if (w < 8000) w = 8000; if (w > 20000) w = 20000;
        ro_set_wait(&g_ro, w);

        int n = recvfrom(g_audio_sock, rx, sizeof(rx), 0, NULL, NULL);
        int64_t t_arr = esp_timer_get_time();
        ro_poll(&g_ro, t_arr);                                /* vence plazos aunque no llegue nada */
        if (n < (int)sizeof(AudioPacket)) continue;           /* timeout o paquete corto */

        AudioPacket* p = (AudioPacket*)rx;
        if (p->magic != PROTOCOL_MAGIC) continue;
        if (p->pkt_type != PKT_AUDIO && p->pkt_type != PKT_AUDIO_MONO) continue;
        if (p->esp_id != g_receiver_id || p->num_samples != AUDIO_PKT_SAMPLES) continue;
        g_rx++;

        ro_res_t res = ro_push(&g_ro, p, t_arr);              /* entrega en orden; pide reenvíos */
        g_lost = g_ro.lost;

        if (res == RO_NEW && g_welcomed && g_sync_valid && g_enabled) {   /* jitter solo con llegadas "normales" */
            int64_t transit = t_arr - (int64_t)p->timestamp_us;
            { portENTER_CRITICAL(&g_mux); int64_t off = g_offset_us; portEXIT_CRITICAL(&g_mux); transit += off; }
            if (have_prev) {
                float d = (float)llabs(transit - prev_transit);
                jit += (d - jit) / 16.0f;
                g_jitter_us = (int32_t)jit;
            }
            prev_transit = transit; have_prev = true;
        }
    }
}

/* ─── Tarea I2S: reproducción alineada + corrección de desvío ──────────────── */
static inline void i2s_put(const int16_t* d, size_t frames)
{
    size_t w = 0;
    i2s_channel_write(g_i2s, d, frames * 2 * sizeof(int16_t), &w, 100);
}

static void i2s_task(void* pv)
{
    static int16_t silence[FRAMES_PER_PACKET * 2];            /* ceros */
    static int16_t work[(FRAMES_PER_PACKET + 1) * 2];
    qitem_t cur;
    bool have = false, was_playing = false, err_valid = false;
    int64_t err_f = 0;
    uint32_t chunk_n = 0;
    float beep_ph = 0.0f;
    /* Ocultación de pérdidas (PLC): si falta(n) paquete(s) en la secuencia de timestamps, se rellena el hueco
     * (1.º chunk = último audio con fade-out, resto silencio) y el siguiente paquete entra con fade-in.
     * Así se conserva la línea de tiempo (no se pierde sincronía con los demás altavoces) y no hay clic. */
    static int16_t last_chunk[FRAMES_PER_PACKET * 2];
    uint64_t last_ts = 0; bool have_last = false, fade_in = false;

    while (1) {
        if (esp_timer_get_time() < g_identify_until_us) {      /* 440 Hz, -20 dBFS, ambos canales */
            static int16_t beep[FRAMES_PER_PACKET * 2];
            for (int i = 0; i < FRAMES_PER_PACKET; i++) {
                int16_t v = (int16_t)(3276.0f * sinf(beep_ph));
                beep[2 * i] = beep[2 * i + 1] = v;
                beep_ph += 2.0f * (float)M_PI * 440.0f / AUDIO_SAMPLE_RATE;
                if (beep_ph > 2.0f * (float)M_PI) beep_ph -= 2.0f * (float)M_PI;
            }
            i2s_put(beep, FRAMES_PER_PACKET);
            have = false; was_playing = false; err_valid = false; have_last = false;
            continue;
        }
        if (g_flush) {
            g_flush = false; xQueueReset(g_q);
            have = false; was_playing = false; err_valid = false; have_last = false;
        }
        if (!g_enabled || !g_welcomed || !g_sync_valid) {
            xQueueReset(g_q);
            have = false; was_playing = false; err_valid = false; have_last = false;
            vTaskDelay(pdMS_TO_TICKS(20));                    /* auto_clear emite silencio */
            continue;
        }

        if (!have) {
            if (xQueueReceive(g_q, &cur, 0) != pdTRUE) {
                if (was_playing) { g_underruns++; was_playing = false; err_valid = false; }
                i2s_put(silence, FRAMES_PER_PACKET);          /* bloquea ~2.5 ms: marca el ritmo */
                continue;
            }
            have = true;
            if (was_playing && have_last) {
                int64_t gap = (int64_t)cur.ts - (int64_t)last_ts - CHUNK_US;     /* >0: faltan paquetes */
                if (gap > CHUNK_US / 2 && gap <= PLC_MAX_K * CHUNK_US + CHUNK_US / 2) {
                    int k = (int)((gap + CHUNK_US / 2) / CHUNK_US);
                    for (int i = 0; i < k; i++) {
                        if (i == 0) {
                            for (int f = 0; f < FRAMES_PER_PACKET; f++) {
                                int32_t g = FRAMES_PER_PACKET - f;                /* rampa 1 -> 0 */
                                work[2 * f]     = (int16_t)((last_chunk[2 * f]     * g) / FRAMES_PER_PACKET);
                                work[2 * f + 1] = (int16_t)((last_chunk[2 * f + 1] * g) / FRAMES_PER_PACKET);
                            }
                            i2s_put(work, FRAMES_PER_PACKET);
                        } else {
                            i2s_put(silence, FRAMES_PER_PACKET);
                        }
                        g_concealed++;
                    }
                    last_ts += (uint64_t)k * (uint64_t)CHUNK_US;
                    fade_in = true;
                }
            }
        }

        int64_t err = master_now_us() + DMA_LAT_US - ((int64_t)cur.ts + (int64_t)g_playout_us);

        if (err > ERR_SKIP_US) {                              /* paquete caducado */
            g_late++; have = false; err_valid = false; have_last = false;
            continue;
        }
        if (err < -ERR_SKIP_US) {                             /* todavía no toca */
            was_playing = false; err_valid = false; have_last = false;
            i2s_put(silence, FRAMES_PER_PACKET);
            continue;
        }

        if (!err_valid) { err_f = err; err_valid = true; chunk_n = 0; }
        else            { err_f += (err - err_f) / 8; }

        size_t frames = FRAMES_PER_PACKET;
        memcpy(work, cur.s, sizeof(cur.s));
        memcpy(last_chunk, cur.s, sizeof(cur.s));                 /* sin fade: base del próximo PLC */
        last_ts = cur.ts; have_last = true;
        if (fade_in) {                                            /* tras un hueco: entrada suave */
            for (int f = 0; f < FRAMES_PER_PACKET; f++) {
                work[2 * f]     = (int16_t)((work[2 * f]     * f) / FRAMES_PER_PACKET);
                work[2 * f + 1] = (int16_t)((work[2 * f + 1] * f) / FRAMES_PER_PACKET);
            }
            fade_in = false;
        }

        if (++chunk_n % SLEW_EVERY == 0) {
            if (err_f > SLEW_DEADBAND_US) {                   /* vamos tarde -> saltar 1 frame */
                frames = FRAMES_PER_PACKET - 1;
                err_f -= FRAME_US; g_corr_drop++;
            } else if (err_f < -SLEW_DEADBAND_US) {           /* vamos pronto -> repetir 1 frame */
                work[frames * 2]     = work[frames * 2 - 2];
                work[frames * 2 + 1] = work[frames * 2 - 1];
                frames++;
                err_f += FRAME_US; g_corr_ins++;
            }
        }
        i2s_put(work, frames);
        g_frames_played += (uint32_t)frames;
        g_sync_err_us = (int32_t)err_f;
        have = false; was_playing = true;
    }
}

/* ─── Tarea de reportes ────────────────────────────────────────────────────── */
static void report_task(void* pv)
{
    uint32_t prev_drop = 0, prev_ins = 0, prev_frames = 0;
#if CONFIG_SONOS_BLE_ENABLE
    bool ble_off = false; int64_t down_since = 0;
#endif
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
#if CONFIG_SONOS_BLE_ENABLE
        {   /* BLE solo durante la ventana inicial (o si el WiFi no está disponible): cero impacto en el audio después */
            int64_t now = esp_timer_get_time();
            if (!ble_off) {
                if (g_wifi_up && !ble_prov_connected() && now > (int64_t)CONFIG_SONOS_BLE_WINDOW_S * 1000000) {
                    ble_prov_stop(); ble_off = true;
                }
            } else if (!g_wifi_up) {
                if (!down_since) down_since = now;
                else if (now - down_since > 60000000) { ble_prov_resume(); ble_off = false; down_since = 0; }
            } else down_since = 0;
            if (!ble_off) ble_prov_notify();
        }
#endif
        if (!g_welcomed) continue;

        uint32_t d = g_corr_drop, i = g_corr_ins, f = g_frames_played;
        uint32_t df = f - prev_frames;
        if (df > 0)
            g_drift_ppm = ((float)((int32_t)(d - prev_drop) - (int32_t)(i - prev_ins)) * 1e6f) / (float)df;
        prev_drop = d; prev_ins = i; prev_frames = f;

        int64_t off; portENTER_CRITICAL(&g_mux); off = g_offset_us; portEXIT_CRITICAL(&g_mux);
        int32_t qframes = (int32_t)uxQueueMessagesWaiting(g_q) * FRAMES_PER_PACKET;

        ReportPacket r;
        memset(&r, 0, sizeof(r));
        r.magic = PROTOCOL_MAGIC; r.pkt_type = PKT_REPORT; r.esp_id = g_receiver_id;
        r.clock_offset_us = off;
        r.rtt_us = g_rtt_us; r.jitter_us = g_jitter_us;
        r.buffer_frames = qframes; r.sync_err_us = g_sync_err_us;
        r.lost_packets = g_lost; r.late_packets = g_late; r.underruns = g_underruns;
        r.recovered_packets = g_ro.recovered;
        r.drift_ppm = g_drift_ppm;
        struct sockaddr_in rpi;
        portENTER_CRITICAL(&g_mux); rpi = g_rpi_addr; portEXIT_CRITICAL(&g_mux);
        sendto(g_ctrl_sock, &r, sizeof(r), 0, (struct sockaddr*)&rpi, sizeof(rpi));

        ESP_LOGI(TAG, "ID=%d ch=%d/%d q=%d%% rx=%u lost=%u late=%u under=%u jit=%dus "
                      "err=%dus rtt=%dus drift=%.0fppm plc=%u rec=%u sync=%d heap=%u",
                 g_receiver_id, g_ch_left, g_ch_right,
                 (int)(qframes * 100 / (RX_QUEUE_PACKETS * FRAMES_PER_PACKET)),
                 (unsigned)g_rx, (unsigned)g_lost, (unsigned)g_late, (unsigned)g_underruns,
                 (int)g_jitter_us, (int)g_sync_err_us, (int)g_rtt_us, g_drift_ppm, (unsigned)g_concealed,
                 (unsigned)g_ro.recovered, g_sync_valid, (unsigned)esp_get_free_heap_size());
    }
}

/* ─── app_main ─────────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "=== SonosWiFi Receiver (protocolo v%d) ===", PROTOCOL_VERSION);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    load_config();

    /* Solo informativo hasta el primer WELCOME (la RPi decide el mapa y si es mono o estéreo). */
    static const uint8_t DEF_L[MAX_NODES] = {0, 2, 4, 6, 8, 5, 6, 7, 8, 9};
    static const uint8_t DEF_R[MAX_NODES] = {1, 3, 5, 7, 9, 5, 6, 7, 8, 9};
    g_ch_left  = DEF_L[g_receiver_id];
    g_ch_right = DEF_R[g_receiver_id];
    ESP_LOGI(TAG, "ID=%d ch=%d/%d", g_receiver_id, g_ch_left, g_ch_right);

    g_q = xQueueCreate(RX_QUEUE_PACKETS, sizeof(qitem_t));
    if (!g_q) { ESP_LOGE(TAG, "FATAL: cola"); esp_restart(); }

    i2s_init();
    /* UN solo núcleo: prioridades por debajo de WiFi (23) y tcpip (18). */
    xTaskCreate(i2s_task, "i2s", 4096, NULL, 15, NULL);    /* ya activa: permite "identificar" sin WiFi */
    xTaskCreate(report_task, "rpt", 4096, NULL, 5, NULL);  /* gestiona la ventana BLE aunque aún no haya WiFi */

#if CONFIG_SONOS_BLE_ENABLE
    ble_prov_init(&g_ble_ops, g_ssid, g_receiver_id, g_rpi_ip_cfg);      /* ANTES de esperar al WiFi: configurable sin red */
#endif
    wifi_start();
    wifi_wait();
    if (!sockets_init()) { vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart(); }

    xTaskCreate(control_task,  "ctrl", 4096, NULL, 12, NULL);
    xTaskCreate(audio_rx_task, "arx",  4096, NULL, 14, NULL);

    ESP_LOGI(TAG, "Listo. Esperando WELCOME de la RPi...");
}
