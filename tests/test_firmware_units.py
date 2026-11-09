#!/usr/bin/env python3
"""Pruebas de unidades del firmware que no necesitan ESP-IDF:
 - valid_ipv4() de ble_prov.c (se extrae y se compila con gcc)
 - los 7 UUID GATT de ble_prov.c coinciden con los de la página /ble (JS)"""
import re, subprocess, sys, os
here = os.path.dirname(os.path.abspath(__file__))
ok = True
def check(n, c, x=""):
    global ok; print(("PASS " if c else "FAIL ") + n + (f"  [{x}]" if x else "")); ok &= bool(c)

c = open(f"{here}/../esp32/main/ble_prov.c").read()
i = c.index("static bool valid_ipv4"); fn = c[i:c.index("\n}\n", i) + 3]
open("/tmp/ip_test.c", "w").write("#include <stdio.h>\n#include <stdbool.h>\n" + fn + r'''
int main(){ struct{const char*s;int ok;} t[]={{"",1},{"192.168.1.10",1},{"0.0.0.0",1},{"255.255.255.255",1},{"256.1.1.1",0},
 {"1.2.3",0},{"1.2.3.4.5",0},{"a.b.c.d",0},{"1..2.3",0},{"1.2.3.",0},{"1234.1.1.1",0},{" 1.2.3.4",0},{"1.2.3.4 ",0}};
 int bad=0; for(unsigned i=0;i<sizeof t/sizeof*t;i++) if(valid_ipv4(t[i].s)!=t[i].ok){printf("caso '%s'\n",t[i].s);bad++;} return bad; }''')
r = subprocess.run("gcc -Wall -o /tmp/ip_test /tmp/ip_test.c && /tmp/ip_test", shell=True, capture_output=True, text=True)
check("valid_ipv4(): 13 casos", r.returncode == 0, r.stdout.strip() + r.stderr.strip())

m = re.search(r"#define SONOS_UUID\(n\) BLE_UUID128_INIT\(([^)]*\)[^)]*)\)", c) or re.search(r"#define SONOS_UUID\(n\) BLE_UUID128_INIT\((.*)\)\s*$", c, re.M)
bytes_ = [int(x, 16) if x.strip().startswith("0x") else None for x in m.group(1).replace("(n)", "n").split(",")]
js = open(f"{here}/../rpi/include/web_pages.h", encoding="utf-8").read()
check("página /ble usa las 7 características", all(f"U({n})" in js or f"C_" in js for n in range(1, 8)) and "C_RPI=U(7)" in js)
for n in range(1, 8):
    be = bytes(reversed([b if b is not None else n for b in bytes_])).hex()
    uuid = f"{be[0:8]}-{be[8:12]}-{be[12:16]}-{be[16:20]}-{be[20:]}"
    check(f"UUID {n} C == JS (5c0a{n:04x}-…)", uuid == f"5c0a{n:04x}-8e3b-4d5e-9a41-534f4e4f5346", uuid)
for n in range(2, 8):
    check(f"característica {n} declarada en el servicio GATT", f"SONOS_UUID({n})" in c)

rm = open(f"{here}/../esp32/main/receiver_main.c").read()
kc = open(f"{here}/../esp32/main/Kconfig.projbuild").read()
m = re.search(r'config SONOS_WIFI_COUNTRY\s+string "[^"]*"\s+default "([A-Z]{2})"', kc)
check("país WiFi por defecto = AR (Kconfig)", m and m.group(1) == "AR", m.group(1) if m else "")
check("esp_wifi_set_country_code() con CONFIG_SONOS_WIFI_COUNTRY", "esp_wifi_set_country_code(CONFIG_SONOS_WIFI_COUNTRY, false)" in rm)
check("el país se fija entre esp_wifi_init() y esp_wifi_start()",
      rm.index("esp_wifi_init(&cfg)") < rm.index("apply_country();") < rm.index("esp_wifi_start()"))
sd = open(f"{here}/../esp32/sdkconfig.defaults").read()
check("sdkconfig.defaults: tabla de particiones propia con dos aplicaciones de 1,5 MB (OTA; el binario supera 1 MB)",
      "CONFIG_PARTITION_TABLE_CUSTOM=y" in sd and 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"' in sd and "SINGLE_APP_LARGE" not in sd.replace("# ", ""))
check("ble_store_config_init() declarada antes de usarla (ESP-IDF no la expone)",
      "void ble_store_config_init(void);" in c and c.index("void ble_store_config_init(void);") < c.index("    ble_store_config_init();"))


# ── ESP32-WROOM-32D (esp32_wroom): mismo código, otro chip ──
import re as _re
w = f"{here}/../esp32_wroom"
sd = open(f"{w}/sdkconfig.defaults").read()
check("wroom: target esp32 (clásico) y tabla de particiones OTA", 'CONFIG_IDF_TARGET="esp32"' in sd and "CONFIG_PARTITION_TABLE_CUSTOM=y" in sd and "esp32c5" not in sd.replace("# ", ""))
wk = open(f"{w}/main/Kconfig.projbuild").read()
pins = {n: int(_re.search(r"config I2S_%s_GPIO\n\s+int .*\n\s+default (\d+)" % n, wk).group(1)) for n in ("BCK", "WS", "DOUT")}
check("BLE: la característica ID acepta 255 (ID automático) además de 0..MAX_NODES-1 (si no, el móvil/RPi recibe ATT 0x13)", "buf[0] >= MAX_NODES && buf[0] != ESP_ID_AUTO" in c)
check("wroom: pines I2S por defecto 26/25/22", pins == {"BCK": 26, "WS": 25, "DOUT": 22}, pins)
check("wroom: sin selección de banda 5 GHz (solo 2.4)", "SONOS_WIFI_BAND" not in wk and 'default "AR"' in wk)
wc = open(f"{w}/main/CMakeLists.txt").read()
check("wroom: compila los mismos fuentes que el C5", "../../esp32/main/receiver_main.c" in wc and "../../esp32/main/ble_prov.c" in wc and "../../esp32/main/ota_update.c" in wc and "../../common" in wc)
check("wroom: README con pines y estado confirmado en placa (audio y OTA)", "GPIO26" in open(f"{w}/README.md").read() and "OTA" in open(f"{w}/README.md").read() and "confirmado por el usuario" in open(f"{w}/README.md").read())
src = open(f"{here}/../esp32/main/receiver_main.c").read()
check("C5 y WROOM: apply_band_mode protegido por el chip (C5 igual que antes)", "#if CONFIG_IDF_TARGET_ESP32C5" in src and src.count("static void apply_band_mode") == 2 and src.count("esp_wifi_set_band_mode") == 1)
def _balanced(t):
    d = 0
    for l in t.splitlines():
        l = l.strip()
        if _re.match(r"#\s*if(def|ndef)?\b", l): d += 1
        elif _re.match(r"#\s*endif\b", l): d -= 1
        if d < 0: return False
    return d == 0
check("receiver_main.c: #if/#endif balanceados", _balanced(src))

# ── OTA por WiFi, versión/chip en el HELLO y RSSI en el REPORT (NO compilado con ESP-IDF real: solo comprobaciones de coherencia) ──
ou = open(f"{here}/../esp32/main/ota_update.c").read()
fwv = int(_re.search(r"#define FW_VERSION\s+(\d+)", src).group(1))
check("FW_VERSION = 16 y cabe en 6 bits del HELLO (≤ 63)", fwv == 16 and fwv <= 63, fwv)
check("PKT_SETTINGS: el ESP la acepta solo de la RPi con id propio, guarda en NVS (led, ble_win, txpwr) y ejecuta la acción",
      all(t in src for t in ("case PKT_SETTINGS:", "s.esp_id != g_receiver_id", "cfg_settings_save(s.led, s.ble_window_s, s.tx_power_dbm)", 'nvs_set_u8(nh, "led"', 'nvs_set_u16(nh, "ble_win"', 'nvs_set_u8(nh, "txpwr"', "SET_ACT_BLE_OPEN", "SET_ACT_FORGET", "cfg_factory()", "SET_ACT_REBOOT")))
check("PKT_SETTINGS: rangos validados (ble 5..3600, potencia 2..20) y el LED apagado respeta «identificar»", all(t in src for t in ("ble_win >= 5 && ble_win <= 3600", "tx >= 2 && tx <= 20", "g_led_mode == SET_LED_OFF && now >= g_identify_until_us")))
check("PKT_SETTINGS: la ventana BLE sale de g_ble_window_s y «abrir BLE» la mantiene 5 min", "now - up_since > (int64_t)g_ble_window_s * 1000000" in src and "now > g_ble_open_until_us" in src and "300000000LL" in src)
for _k in ("esp32", "esp32_wroom"):
    _kc = open(f"{here}/../{_k}/main/Kconfig.projbuild", encoding="utf-8").read()
    check(f"{_k}: el BLE se apaga a los 15 s de tener WiFi (no 300)", re.search(r"config SONOS_BLE_WINDOW_S.*?default (\d+)", _kc, re.S).group(1) == "15")
check("PKT_WIFI: el ESP la acepta solo de la RPi con id propio, valida largos, guarda la red como PENDIENTE y reinicia", all(t in src for t in ("case PKT_WIFI:", "w.esp_id != g_receiver_id", "src.sin_addr.s_addr != g_rpi_addr.sin_addr.s_addr", "sl < 1 || sl > 32", "pl < 8 || pl > 64", "cfg_pending_save(w.ssid, w.pass)", "cfg_reboot()")))
check("PKT_WIFI: la red nueva se confirma al conectar y hablar con la RPi; a los 60 s sin conectar se descarta y vuelve a la anterior", all(t in src for t in ("g_wifi_trial", "g_wifi_up && g_welcomed", "cfg_pending_finish(true)", "cfg_pending_finish(false)", "> 60000000")))
check("PKT_WIFI: una configuración por BLE anula la prueba pendiente (cfg_save borra pend_ssid/pend_pass)", 'nvs_erase_key(nh, "pend_ssid"); nvs_erase_key(nh, "pend_pass");' in src)
check("LED de estado: una tarea con patrones (sin Wi-Fi, sin RPi, sin audio, audio, identificar, OTA) y desactivable con GPIO -1", all(t in src for t in ("led_task", "CONFIG_SONOS_LED_GPIO >= 0", "g_identify_until_us", "ota_update_active()", "!g_wifi_up", "!g_welcomed", "g_frames_played")))
for _k, _g in (("esp32", "27"), ("esp32_wroom", "-1")):
    _kc2 = open(f"{here}/../{_k}/main/Kconfig.projbuild", encoding="utf-8").read()
    check(f"{_k}: LED de estado en GPIO {_g} por defecto", re.search(r"config SONOS_LED_GPIO.*?default (-?\d+)", _kc2, re.S).group(1) == _g)
_pk = open(f"{here}/../common/protocol.h", encoding="utf-8").read()
check("PKT_SETTINGS = 0x0C y SettingsPacket de 12 bytes", "#define PKT_SETTINGS    0x0C" in _pk and "sizeof(SettingsPacket) == 12" in _pk)
check("PKT_WIFI = 0x0B y WifiPacket de 112 bytes con ssid[36] y pass[68]", "#define PKT_WIFI        0x0B" in _pk and 'sizeof(WifiPacket)    == 112' in _pk and "char     ssid[36]" in _pk and "char     pass[68]" in _pk)
_pr = {m.group(1): int(m.group(2)) for m in _re.finditer(r'xTaskCreate\((\w+),\s*"\w+",\s*\d+,\s*NULL,\s*(\d+),', src)}
check("prioridades: I2S (15) y recepción (14) bajo lwIP (18) y el WiFi (23), como cuando funcionaba mejor", _pr.get("i2s_task") == 15 and _pr.get("audio_rx_task") == 14 and _pr.get("control_task") == 12, _pr)
check("ID automático: HELLO con ESP_ID_AUTO, el WELCOME asignado se acepta solo sin ID y no se escribe en NVS", "g_receiver_id == ESP_ID_AUTO && (w.reserved & WELCOME_F_ASSIGNED)" in src and "hello.esp_id = g_receiver_id;" in src and "nvs_erase_key(nh, \"receiver_id\")" in src)
check("ID automático: Kconfig por defecto 255 en C5 y WROOM; DEF_L/DEF_R no se indexan con 255", all("default 255" in open(f).read() for f in (f"{here}/../esp32/main/Kconfig.projbuild", f"{here}/../esp32_wroom/main/Kconfig.projbuild")) and "g_receiver_id < MAX_NODES ? g_receiver_id : 0" in src)
check("HELLO: reserved = HELLO_PACK(FW_VERSION, chip) con el chip según el target", "hello.reserved = HELLO_PACK(FW_VERSION, FW_CHIP_ID)" in src and "#define FW_CHIP_ID          FW_CHIP_C5" in src and "#define FW_CHIP_ID          FW_CHIP_ESP32" in src)
check("REPORT: rssi_dbm sale de esp_wifi_sta_get_ap_info", "r.rssi_dbm = ap.rssi" in src)
check("PKT_OTA: solo de la RPi con la que se habla, con el id propio y tras el WELCOME", "case PKT_OTA:" in src and "o.esp_id != g_receiver_id" in src and "src.sin_addr.s_addr != g_rpi_addr.sin_addr.s_addr" in src and "!g_welcomed" in src)
check("durante la OTA el ESP calla (HELLO, SYNC y REPORT) para que la RPi sepa que sigue descargando", src.count("ota_update_active()") >= 3)
check("el firmware nuevo se confirma (rollback) solo tras conectar y sincronizar", "g_sync_valid && !g_fw_confirmed" in src and "ota_mark_valid()" in src)
check("ota_update.c: valida el tamaño recibido contra el anunciado y aborta si no coincide", "esp_https_ota_get_image_len_read" in ou and "esp_https_ota_abort" in ou and "esp_https_ota_finish" in ou)
check("ota_update.c: el token va en la URL con 16 dígitos hexadecimales (la RPi lo compara como número)", "?t=%016llx" in ou)
check("tras fallar, la OTA se libera (el ESP vuelve a anunciarse y la RPi la da por terminada)", "s_active = false;" in ou and "vTaskDelete" in ou)
for d, extra in (("esp32", ""), ("esp32_wroom", "")):
    t = open(f"{here}/../{d}/sdkconfig.defaults").read()
    check(f"{d}: OTA por HTTP permitido, rollback activado y flash de 4 MB", "CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=y" in t and "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y" in t and "CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y" in t)
    cm = open(f"{here}/../{d}/main/CMakeLists.txt").read()
    check(f"{d}: CMake incluye ota_update.c y los componentes app_update, esp_http_client y esp_https_ota", "ota_update.c" in cm and all(x in cm for x in ("app_update", "esp_http_client", "esp_https_ota")))
    rows = [[x.strip() for x in l.split(",")] for l in open(f"{here}/../{d}/partitions.csv") if l.strip() and not l.startswith("#")]
    part = {r[0]: (int(r[3], 16), int(r[4], 16), r[1], r[2]) for r in rows}
    check(f"{d}: partitions.csv tiene nvs, otadata, ota_0 y ota_1", {"nvs", "otadata", "ota_0", "ota_1"} <= set(part), list(part))
    check(f"{d}: ota_0 y ota_1 miden 1,5 MB, alineadas a 64 KB y sin solaparse", part["ota_0"][1] == part["ota_1"][1] == 0x180000 and part["ota_0"][0] % 0x10000 == 0 and part["ota_1"][0] % 0x10000 == 0 and part["ota_0"][0] + part["ota_0"][1] <= part["ota_1"][0])
    ends = sorted((o, o + z) for o, z, _t, _s in part.values())
    check(f"{d}: ninguna partición se solapa y todo cabe en 4 MB", all(a[1] <= b[0] for a, b in zip(ends, ends[1:])) and ends[-1][1] <= 0x400000, ends[-1])
    check(f"{d}: otadata de 8 KB (ESP-IDF lo exige)", part["otadata"][1] == 0x2000)
check("C5 y WROOM comparten ota_update.c (mismo código, sin #if por chip)", "CONFIG_IDF_TARGET" not in ou)
check("protocol.h: PKT_OTA = 0x0A y OtaPacket de 24 bytes", "PKT_OTA" in open(f"{here}/../common/protocol.h").read() and "== 24, \"OtaPacket\"" in open(f"{here}/../common/protocol.h").read())
sys.exit(0 if ok else 1)
