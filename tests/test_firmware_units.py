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
check("sdkconfig.defaults: partición de aplicación grande (el binario supera 1 MB)",
      "CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y" in sd)
check("ble_store_config_init() declarada antes de usarla (ESP-IDF no la expone)",
      "void ble_store_config_init(void);" in c and c.index("void ble_store_config_init(void);") < c.index("    ble_store_config_init();"))


# ── ESP32-WROOM-32D (esp32_wroom): mismo código, otro chip ──
import re as _re
w = f"{here}/../esp32_wroom"
sd = open(f"{w}/sdkconfig.defaults").read()
check("wroom: target esp32 (clásico) y partición grande", 'CONFIG_IDF_TARGET="esp32"' in sd and "SINGLE_APP_LARGE=y" in sd and "esp32c5" not in sd.replace("# ", ""))
wk = open(f"{w}/main/Kconfig.projbuild").read()
pins = {n: int(_re.search(r"config I2S_%s_GPIO\n\s+int .*\n\s+default (\d+)" % n, wk).group(1)) for n in ("BCK", "WS", "DOUT")}
check("wroom: pines I2S por defecto 26/25/22", pins == {"BCK": 26, "WS": 25, "DOUT": 22}, pins)
check("wroom: sin selección de banda 5 GHz (solo 2.4)", "SONOS_WIFI_BAND" not in wk and 'default "AR"' in wk)
wc = open(f"{w}/main/CMakeLists.txt").read()
check("wroom: compila los mismos fuentes que el C5", "../../esp32/main/receiver_main.c" in wc and "../../esp32/main/ble_prov.c" in wc and "../../common" in wc)
check("wroom: README con pines y aviso de no probado", "GPIO26" in open(f"{w}/README.md").read() and "No probado" in open(f"{w}/README.md").read())
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

sys.exit(0 if ok else 1)
