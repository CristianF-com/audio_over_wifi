#include "ble_prov.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"
#include "protocol.h"          /* MAX_NODES */

#ifndef CONFIG_SONOS_BLE_REQUIRE_ENC
  #define CONFIG_SONOS_BLE_REQUIRE_ENC 1
#endif

static const char* TAG = "BLE";

/* UUID little-endian de 5c0a00NN-8e3b-4d5e-9a41-534f4e4f5346 */
#define SONOS_UUID(n) BLE_UUID128_INIT(0x46,0x53,0x4f,0x4e,0x4f,0x53,0x41,0x9a,0x5e,0x4d,0x3b,0x8e,(n),0x00,0x0a,0x5c)
static const ble_uuid128_t U_SVC  = SONOS_UUID(1);
static const ble_uuid128_t U_SSID = SONOS_UUID(2);
static const ble_uuid128_t U_PASS = SONOS_UUID(3);
static const ble_uuid128_t U_ID   = SONOS_UUID(4);
static const ble_uuid128_t U_STAT = SONOS_UUID(5);
static const ble_uuid128_t U_CMD  = SONOS_UUID(6);
static const ble_uuid128_t U_RPI  = SONOS_UUID(7);

enum { CH_SSID = 2, CH_PASS, CH_ID, CH_STATUS, CH_CMD, CH_RPI };

#if CONFIG_SONOS_BLE_REQUIRE_ENC
  #define WR_FLAGS (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC)
#else
  #define WR_FLAGS (BLE_GATT_CHR_F_WRITE)
#endif

static const ble_prov_ops_t* s_ops;
static char     s_ssid[33], s_pass[64], s_name[20], s_rpi[16];
static bool     s_has_pass;
static uint8_t  s_id;
static uint8_t  s_own_addr_type;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_status_handle;
static bool     s_notify, s_active;

static void adv_start(void);

/* "a.b.c.d" con cada octeto 0..255 (o cadena vacía) */
static bool valid_ipv4(const char* s)
{
    if (!*s) return true;
    int parts = 0, val = 0, digits = 0;
    for (const char* p = s;; p++) {
        if (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); if (++digits > 3 || val > 255) return false; }
        else if (*p == '.' || *p == 0) {
            if (!digits) return false;
            parts++; val = 0; digits = 0;
            if (*p == 0) break;
        } else return false;
    }
    return parts == 4;
}

/* ─── GATT ─────────────────────────────────────────────────────────────────── */
static int chr_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    const int which = (int)(intptr_t)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        static char js[224];                                 /* solo la usa la tarea host */
        const void* p = NULL; size_t n = 0;
        switch (which) {
        case CH_SSID:   p = s_ssid; n = strlen(s_ssid); break;
        case CH_ID:     p = &s_id;  n = 1; break;
        case CH_RPI:    p = s_rpi;  n = strlen(s_rpi); break;
        case CH_STATUS: n = s_ops->status_json(js, sizeof(js)); p = js; break;
        default:        return BLE_ATT_ERR_READ_NOT_PERMITTED;
        }
        return os_mbuf_append(ctxt->om, p, (uint16_t)n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t buf[72]; uint16_t len = 0;
        if (OS_MBUF_PKTLEN(ctxt->om) > sizeof(buf) - 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len) != 0) return BLE_ATT_ERR_UNLIKELY;
        buf[len] = 0;

        switch (which) {
        case CH_SSID:
            if (len < 1 || len > 32) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            memcpy(s_ssid, buf, len); s_ssid[len] = 0;
            return 0;
        case CH_PASS:
            if (len == 0) { s_has_pass = false; return 0; }                 /* mantener */
            if (len < 8 || len > 63) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            memcpy(s_pass, buf, len); s_pass[len] = 0; s_has_pass = true;
            return 0;
        case CH_RPI:
            if (len > 15 || !valid_ipv4((char*)buf)) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            memcpy(s_rpi, buf, len); s_rpi[len] = 0;
            return 0;
        case CH_ID:
            if (len != 1 || buf[0] >= MAX_NODES) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            s_id = buf[0];
            return 0;
        case CH_CMD:
            if (!strcmp((char*)buf, "apply")) {
                if (s_ssid[0] == 0) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
                if (s_ops->save(s_ssid, s_has_pass ? s_pass : NULL, s_id, s_rpi) != 0) return BLE_ATT_ERR_UNLIKELY;
                ESP_LOGI(TAG, "Config guardada (ID=%u, SSID='%s'); reiniciando", s_id, s_ssid);
                s_ops->reboot();
                return 0;
            }
            if (!strcmp((char*)buf, "identify")) { s_ops->identify(); return 0; }
            if (!strcmp((char*)buf, "reboot"))   { s_ops->reboot();   return 0; }
            if (!strcmp((char*)buf, "factory"))  {
                if (s_ops->factory() != 0) return BLE_ATT_ERR_UNLIKELY;
                s_ops->reboot();
                return 0;
            }
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        default: break;
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &U_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &U_SSID.u, .access_cb = chr_access, .arg = (void*)(intptr_t)CH_SSID,
              .flags = BLE_GATT_CHR_F_READ | WR_FLAGS },
            { .uuid = &U_PASS.u, .access_cb = chr_access, .arg = (void*)(intptr_t)CH_PASS,
              .flags = WR_FLAGS },
            { .uuid = &U_ID.u,   .access_cb = chr_access, .arg = (void*)(intptr_t)CH_ID,
              .flags = BLE_GATT_CHR_F_READ | WR_FLAGS },
            { .uuid = &U_STAT.u, .access_cb = chr_access, .arg = (void*)(intptr_t)CH_STATUS,
              .val_handle = &s_status_handle, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY },
            { .uuid = &U_CMD.u,  .access_cb = chr_access, .arg = (void*)(intptr_t)CH_CMD,
              .flags = WR_FLAGS },
            { .uuid = &U_RPI.u,  .access_cb = chr_access, .arg = (void*)(intptr_t)CH_RPI,
              .flags = BLE_GATT_CHR_F_READ | WR_FLAGS },
            { 0 }
        },
    },
    { 0 }
};

/* ─── GAP ──────────────────────────────────────────────────────────────────── */
static int gap_event(struct ble_gap_event* ev, void* arg)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "conexión %s (status %d)", ev->connect.status == 0 ? "OK" : "fallida", ev->connect.status);
        if (ev->connect.status == 0) s_conn = ev->connect.conn_handle;
        else { s_conn = BLE_HS_CONN_HANDLE_NONE; adv_start(); }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "desconectado (motivo %d)", ev->disconnect.reason);
        s_conn = BLE_HS_CONN_HANDLE_NONE; s_notify = false;
        memset(s_pass, 0, sizeof(s_pass)); s_has_pass = false;        /* no dejar la clave en RAM */
        adv_start();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        adv_start();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (ev->subscribe.attr_handle == s_status_handle) s_notify = ev->subscribe.cur_notify != 0;
        break;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU=%u", ev->mtu.value);
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "cifrado %s (status %d)", ev->enc_change.status == 0 ? "activo" : "FALLÓ", ev->enc_change.status);
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc d;
        if (ble_gap_conn_find(ev->repeat_pairing.conn_handle, &d) == 0) ble_store_util_delete_peer(&d.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    default: break;
    }
    return 0;
}

static void adv_start(void)
{
    if (!s_active) return;
    struct ble_gap_adv_params ap;
    memset(&ap, 0, sizeof(ap));
    ap.conn_mode = BLE_GAP_CONN_MODE_UND;
    ap.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ap.itvl_min  = BLE_GAP_ADV_ITVL_MS(100);
    ap.itvl_max  = BLE_GAP_ADV_ITVL_MS(150);
    int rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &ap, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) ESP_LOGW(TAG, "adv_start rc=%d", rc);
}

static void on_sync(void)
{
    if (ble_hs_util_ensure_addr(0) != 0 || ble_hs_id_infer_auto(0, &s_own_addr_type) != 0) {
        ESP_LOGE(TAG, "sin dirección BLE"); return;
    }
    struct ble_hs_adv_fields f;
    memset(&f, 0, sizeof(f));
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.uuids128 = &U_SVC;                       /* el filtro de Web Bluetooth busca este UUID */
    f.num_uuids128 = 1;
    f.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&f);

    struct ble_hs_adv_fields r;
    memset(&r, 0, sizeof(r));
    r.name = (uint8_t*)s_name; r.name_len = (uint8_t)strlen(s_name); r.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&r);

    ESP_LOGI(TAG, "anunciando como '%s'", s_name);
    adv_start();
}

static void on_reset(int reason) { ESP_LOGW(TAG, "host reset: %d", reason); }

static void host_task(void* p)
{
    nimble_port_run();                          /* no vuelve hasta nimble_port_stop() */
    nimble_port_freertos_deinit();
}

/* ─── API ──────────────────────────────────────────────────────────────────── */
void ble_prov_init(const ble_prov_ops_t* ops, const char* cur_ssid, uint8_t cur_id, const char* cur_rpi_ip)
{
    s_ops = ops; s_id = cur_id;
    strlcpy(s_rpi, cur_rpi_ip ? cur_rpi_ip : "", sizeof(s_rpi));
    strlcpy(s_ssid, cur_ssid ? cur_ssid : "", sizeof(s_ssid));

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_name, sizeof(s_name), "SonosWiFi-%02X%02X", mac[4], mac[5]);   /* único aunque todos tengan ID 0 */

    if (nimble_port_init() != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init falló"); return; }

    ble_hs_cfg.reset_cb        = on_reset;
    ble_hs_cfg.sync_cb         = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap  = BLE_HS_IO_NO_INPUT_OUTPUT;   /* "Just Works": cifra el enlace; sin protección MITM activa */
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm    = 0;
    ble_hs_cfg.sm_sc      = 1;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(s_svcs);
    if (rc == 0) rc = ble_gatts_add_svcs(s_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "GATT rc=%d", rc); return; }
    ble_svc_gap_device_name_set(s_name);
    ble_store_config_init();

    s_active = true;
    nimble_port_freertos_init(host_task);
}

void ble_prov_stop(void)
{
    s_active = false;
    ble_gap_adv_stop();
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    ESP_LOGI(TAG, "BLE en silencio");
}
void ble_prov_resume(void) { s_active = true; adv_start(); ESP_LOGI(TAG, "BLE reactivado"); }
void ble_prov_notify(void) { if (s_notify && s_conn != BLE_HS_CONN_HANDLE_NONE) ble_gatts_chr_updated(s_status_handle); }
bool ble_prov_connected(void) { return s_conn != BLE_HS_CONN_HANDLE_NONE; }
