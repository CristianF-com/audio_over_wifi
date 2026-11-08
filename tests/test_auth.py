#!/usr/bin/env python3
"""Autenticación HTTP Basic de la web (harness en modo 'auth', puerto 18081)."""
import urllib.request, urllib.error, base64, sys
ok = True
def check(n, c, x=""):
    global ok; print(("PASS " if c else "FAIL ") + n + (f"  [{x}]" if x else "")); ok &= bool(c)
def get(path, user=None, pw=None):
    req = urllib.request.Request("http://127.0.0.1:18081" + path)
    if user is not None: req.add_header("Authorization", "Basic " + base64.b64encode(f"{user}:{pw}".encode()).decode())
    try:
        r = urllib.request.urlopen(req, timeout=3); return r.status, dict(r.headers)
    except urllib.error.HTTPError as e: return e.code, dict(e.headers)
c, h = get("/api/status");                           check("sin credenciales -> 401", c == 401, c)
check("401 incluye WWW-Authenticate Basic", "Basic" in h.get("WWW-Authenticate", ""), h.get("WWW-Authenticate"))
c, _ = get("/");                                     check("la SPA también exige contraseña", c == 401, c)
c, _ = get("/api/status", "x", "mal");               check("contraseña incorrecta -> 401", c == 401, c)
c, _ = get("/api/status", "sonos", "s3cret");        check("prefijo de la contraseña -> 401", c == 401, c)
c, _ = get("/api/status", "sonos", "s3cret:clave");  check("contraseña correcta (con ':' dentro) -> 200", c == 200, c)
c, _ = get("/api/status", "otro", "s3cret:clave");   check("el nombre de usuario es libre -> 200", c == 200, c)
c, _ = get("/calibrate", "sonos", "s3cret:clave");   check("/calibrate con credenciales -> 200", c == 200, c)
# PWA: manifiesto, service worker e iconos son públicos (el navegador los pide sin credenciales); lo demás sigue protegido
for path, ct in (("/manifest.webmanifest", "application/manifest+json"), ("/sw.js", "text/javascript"), ("/icon.svg", "image/svg+xml"),
                 ("/icons/icon-192.png", "image/png"), ("/icons/icon-512.png", "image/png"), ("/icons/icon-maskable-512.png", "image/png"), ("/apple-touch-icon.png", "image/png")):
    c, h = get(path); check(f"PWA {path} sin contraseña -> 200 ({ct})", c == 200 and h.get("Content-Type", "").startswith(ct), (c, h.get("Content-Type")))
c, h = get("/sw.js"); check("sw.js con Service-Worker-Allowed: /", h.get("Service-Worker-Allowed") == "/", h.get("Service-Worker-Allowed"))
c, _ = get("/api/nodes"); check("con los recursos PWA públicos, /api/nodes sigue exigiendo contraseña", c == 401, c)
c, _ = get("/fonts/roboto-400.woff2"); check("fuentes: públicas sin contraseña (las pide el CSS sin credenciales)", c == 200, c)
# OTA: solo el binario con token es público (lo pide el ESP); subir firmware y dar la orden exigen contraseña
c, _ = get("/fw/ota.bin?t=1");  check("OTA: /fw/ota.bin sin contraseña llega a la comprobación del token -> 403 (no 401)", c == 403, c)
for path in ("/api/firmware", "/api/tone", "/api/profiles"):
    c, _ = get(path); check(f"OTA/nuevas: {path} sin contraseña -> 401", c == 401, c)
def post(path, data=b"{}", user=None, pw=None):
    req = urllib.request.Request("http://127.0.0.1:18081" + path, data=data, method="POST")
    if user is not None: req.add_header("Authorization", "Basic " + base64.b64encode(f"{user}:{pw}".encode()).decode())
    try: return urllib.request.urlopen(req, timeout=3).status
    except urllib.error.HTTPError as e: return e.code
check("OTA: POST /api/firmware sin contraseña -> 401", post("/api/firmware", b"x" * 70000) == 401)
check("OTA: POST /api/ota sin contraseña -> 401", post("/api/ota", b'{"id":0}') == 401)
print("RESULTADO AUTH:", "TODO OK" if ok else "HAY FALLOS"); # ── Red de la RPi y actualización del servidor (solo con contraseña) ───────────────
import json, time
def post(path, body, user="sonos", pw="s3cret:clave"):
    req = urllib.request.Request("http://127.0.0.1:18081" + path, data=json.dumps(body).encode(), method="POST", headers={"Content-Type": "application/json"})
    if user is not None: req.add_header("Authorization", "Basic " + base64.b64encode(f"{user}:{pw}".encode()).decode())
    try:
        r = urllib.request.urlopen(req, timeout=5); return r.status, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        try: return e.code, json.loads(e.read() or b"{}")
        except Exception: return e.code, {}
def sysinfo():
    req = urllib.request.Request("http://127.0.0.1:18081/api/system", headers={"Authorization": "Basic " + base64.b64encode(b"sonos:s3cret:clave").decode()})
    return json.loads(urllib.request.urlopen(req, timeout=5).read())
def esperar(f, t=6):
    end = time.time() + t
    while time.time() < end:
        v = f()
        if v: return v
        time.sleep(0.2)
    return None
c, _ = post("/api/update", {}, None);        check("actualizar sin credenciales -> 401", c == 401, c)
c, _ = post("/api/rpi/wifi", {"ssid": "X"}, None); check("cambiar la WiFi de la RPi sin credenciales -> 401", c == 401, c)
si = sysinfo()
check("/api/system: versión, protegido y WiFi actual de la RPi (CasaHogar en wlan0)", si["protected"] is True and si["version"] and si["wifi"]["ssid"] == "CasaHogar" and si["wifi"]["iface"] == "wlan0", si)
c, r = post("/api/rpi/wifi", {"ssid": "Casa", "pass": "corta"}); check("WiFi de la RPi: contraseña corta -> 400", c == 400, (c, r))
c, r = post("/api/rpi/wifi", {"ssid": "-malo", "pass": "clave12345"}); check("WiFi de la RPi: nombre que empieza por «-» -> 400", c == 400, (c, r))
c, r = post("/api/rpi/wifi", {"ssid": "Mala", "pass": "clave12345"}); check("WiFi de la RPi: cambio aceptado (asíncrono) -> 200", c == 200, (c, r))
w = esperar(lambda: (lambda x: x if x["wifi"]["state"] in ("ok", "failed") else None)(sysinfo()))
check("WiFi de la RPi: si falla, vuelve a la red anterior y lo dice", w and w["wifi"]["state"] == "failed" and "Se volvió a «CasaHogar»" in w["wifi"]["msg"], w and w["wifi"])
lg = open("/tmp/fake_nmcli.log").read()
check("WiFi de la RPi: usó nmcli con ssid, contraseña e interfaz, y luego «con up id CasaHogar»", "connect dev wifi connect Mala password clave12345 ifname wlan0" in lg and "up con up id CasaHogar" in lg, lg)
c, r = post("/api/rpi/wifi", {"ssid": "Casa-5G", "pass": "clave12345"}); check("WiFi de la RPi: nueva red válida -> 200", c == 200, (c, r))
w = esperar(lambda: (lambda x: x if x["wifi"]["state"] in ("ok", "failed") else None)(sysinfo()))
check("WiFi de la RPi: conectada a «Casa-5G»", w and w["wifi"]["state"] == "ok" and "Casa-5G" in w["wifi"]["msg"], w and w["wifi"])
check("WiFi de la RPi: la contraseña no sale nunca por la API", "clave12345" not in json.dumps(sysinfo()))
c, r = post("/api/update", {}); check("actualizar el servidor -> 200", c == 200, (c, r))
u = esperar(lambda: (lambda x: x if x["update"]["state"] == "ok" else None)(sysinfo()))
check("actualizar: estado «ok» y registro visible", u and "Listo" in u["update"]["log"], u and u["update"])

sys.exit(0 if ok else 1)
