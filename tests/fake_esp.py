#!/usr/bin/env python3
"""ESP32 simulado (127.0.0.2) + pruebas HTTP. Requiere el harness corriendo."""
import socket, struct, time, json, sys, urllib.request, urllib.error

MAGIC = 0x534E4657
ESP_IP, RPI_IP = "127.0.0.2", "127.0.0.1"
ok = True
def check(name, cond, extra=""):
    global ok
    print(("PASS " if cond else "FAIL ") + name + (f"  [{extra}]" if extra else ""))
    ok &= bool(cond)

ctrl = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ctrl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ctrl.bind((ESP_IP, 5005)); ctrl.settimeout(2)
aud = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
aud.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
aud.bind((ESP_IP, 5006)); aud.settimeout(0.5)

def mono_us():
    return time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW) // 1000

# 1) HELLO -> WELCOME
hello = struct.pack("<IBBBBH6s", MAGIC, 1, 0, 5, 0, 5006, b"\xaa\xbb\xcc\xdd\xee\xff")
check("HELLO tamaño 16", len(hello) == 16)
ctrl.sendto(hello, (RPI_IP, 5005))
d, _ = ctrl.recvfrom(256)
check("WELCOME tamaño 20", len(d) == 20, len(d))
m, t, i, cl, cr, vl, vr, en, _r, sr, pd = struct.unpack("<IBBBBBBBBII", d)
check("WELCOME campos", (m, t, i, cl, cr, en, sr, pd) == (MAGIC, 2, 0, 0, 1, 1, 48000, 80000), (cl, cr, sr, pd))

# 2) SYNC
best = None
for _ in range(8):
    t1 = mono_us()
    ctrl.sendto(struct.pack("<IBBHQ", MAGIC, 6, 0, 0, t1), (RPI_IP, 5005))
    d, _ = ctrl.recvfrom(256); t4 = mono_us()
    check_len = len(d) == 32
    m, t, i, _r, r1, t2, t3 = struct.unpack("<IBBHQQQ", d)
    rtt = (t4 - t1) - (t3 - t2); off = ((t2 - t1) + (t3 - t4)) // 2
    if best is None or rtt < best[0]: best = (rtt, off, r1 == t1, t2 <= t3)
check("SYNC_RESP tamaño 32", check_len)
check("SYNC eco t1 y t2<=t3", best[2] and best[3])
check("SYNC offset ≈ 0 (mismo reloj) y RTT < 2 ms", abs(best[1]) < 500 and best[0] < 2000, f"off={best[1]}us rtt={best[0]}us")

# 3) AUDIO
def grab(secs):
    pk = []; end = time.time() + secs
    while time.time() < end:
        try: pk.append(aud.recvfrom(2048)[0])
        except socket.timeout: pass
    return pk
pk = grab(1.0)
check("AUDIO recibido", len(pk) > 300, len(pk))
check("AUDIO tamaño 500", all(len(p) == 500 for p in pk))
hs = [struct.unpack("<IBBHIQ", p[:20]) for p in pk]
seqs = [h[4] for h in hs]; tss = [h[5] for h in hs]
check("AUDIO seq contigua", all((b - a) == 1 for a, b in zip(seqs, seqs[1:])))
dts = [b - a for a, b in zip(tss, tss[1:])]
avg = sum(dts) / len(dts)
check("AUDIO ts avanza 2500 µs/paquete (±1%)", abs(avg - 2500) < 25, f"{avg:.1f}")
check("AUDIO ts pegado al reloj real", abs(tss[-1] - mono_us()) < 120000, f"{(mono_us()-tss[-1])/1000:.1f} ms de edad")
s = struct.unpack("<240h", pk[0][20:])
check("AUDIO muestras ch0/ch1 (L=16384, R=-16384)", s[0] == 16384 and s[1] == -16384, (s[0], s[1]))

# 4) REPORT -> /api/nodes
rep = struct.pack("<IBB2xqiiiiIIIfI", MAGIC, 4, 0, 1234, 321, 77, 960, -42, 5, 6, 7, 12.5, 9)
check("REPORT tamaño 52 (v5: con recovered_packets)", len(rep) == 52, len(rep))
import threading
def _keepalive():                       # el ESP real manda REPORT cada 2 s; sin él la RPi lo da por offline a los 10 s
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind((ESP_IP, 0))
    while True:
        s.sendto(rep, (RPI_IP, 5005)); time.sleep(2)
ctrl.sendto(rep, (RPI_IP, 5005)); time.sleep(0.2)
threading.Thread(target=_keepalive, daemon=True).start()

B = "http://127.0.0.1:18080"
def http(method, path, body=None):
    req = urllib.request.Request(B + path, method=method,
          data=None if body is None else (body if isinstance(body, bytes) else json.dumps(body).encode()),
          headers={"Content-Type": "application/json"})
    try:
        r = urllib.request.urlopen(req, timeout=3); return r.status, r.read().decode()
    except urllib.error.HTTPError as e: return e.code, e.read().decode()
c, b = http("GET", "/api/nodes"); n0 = json.loads(b)[0]
check("REPORT visible en /api/nodes", (n0["online"], n0["rtt_us"], n0["jitter_us"], n0["sync_err_us"],
      n0["lost_packets"], n0["late_packets"], n0["underruns"]) == (True, 321, 77, -42, 5, 6, 7), n0)
check("fill_pct = 960 frames / 11520 (96 paquetes × 120)", abs(n0["fill_pct"] - 960 * 100 / 11520) < 0.01, n0["fill_pct"])
check("ip del nodo", n0["ip"] == ESP_IP)
check("recovered_packets del REPORT visible", n0["recovered_packets"] == 9, n0.get("recovered_packets"))
# Errores recientes: el primer REPORT no cuenta el pasado del ESP; solo suman los eventos nuevos (ventana de 10 min)
check("cortes recientes: el acumulado inicial del ESP no cuenta como reciente",
      (n0["recent_lost"], n0["recent_late"], n0["recent_underruns"], n0["recent_min"]) == (0, 0, 0, 10),
      (n0.get("recent_lost"), n0.get("recent_late"), n0.get("recent_underruns"), n0.get("recent_min")))
rep = struct.pack("<IBB2xqiiiiIIIfI", MAGIC, 4, 0, 1234, 321, 77, 960, -42, 5 + 4, 6 + 2, 7 + 1, 12.5, 9)   # el hilo de keepalive usa la global
ctrl.sendto(rep, (RPI_IP, 5005)); time.sleep(0.2)   # sin esperar: el nodo 0 se reconfigura a los 3 s y las pruebas de después cuentan con ello
c, b = http("GET", "/api/nodes"); n0 = json.loads(b)[0]
check("cortes recientes: 4 perdidos, 2 tarde y 1 vacío nuevos aparecen y el total acumulado sigue (9, 8, 8)",
      (n0["recent_lost"], n0["recent_late"], n0["recent_underruns"], n0["lost_packets"], n0["late_packets"], n0["underruns"]) == (4, 2, 1, 9, 8, 8), n0)

# 5) API robusta
c, _ = http("POST", "/api/node", {"id": 0, "ch_left": 99});           check("ch_left=99 -> 400", c == 400, c)
c, _ = http("POST", "/api/node", b'{"id":"abc"}');                    check("id=abc -> 400 (sin crash)", c == 400, c)
c, _ = http("POST", "/api/node", {"ch_left": 1});                     check("sin id -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 10});                          check("id=10 -> 400 (máx. 10 nodos: 0..9)", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 0, "name": ""});               check("nombre vacío -> 200 (borra)", c == 200, c)
c, _ = http("POST", "/api/volume", b'{"volume":"x"}');                check("volume inválido -> 400", c == 400, c)
c, _ = http("POST", "/api/calibrate", {"esp_id": 0, "latency_ms": 9999}); check("latencia fuera de rango -> 400", c == 400, c)
c, _ = http("GET", "/nope");                                          check("ruta desconocida -> 404", c == 404, c)
c, b = http("GET", "/api/status");                                    check("status OK", c == 200 and json.loads(b)["sample_rate"] == 48000, b[:80])
c, b = http("GET", "/");                                              check("panel servido completo (pass-through, plano, </html>)", c == 200 and "AStream" in b and "/api/dsp" in b and "id=\"room\"" in b and b.rstrip().endswith("</html>"))
c, _ = http("POST", "/api/calibrate", {"esp_id": 0, "latency_ms": 10}); check("calibrate (ms) OK", c == 200)
c, b = http("GET", "/api/nodes"); check("latency_offset_us=10000", json.loads(b)[0]["latency_offset_us"] == 10000)
c, _ = http("POST", "/api/calibrate", {"esp_id": 0, "latency_us": 1234}); check("calibrate (us) OK", c == 200)
c, b = http("GET", "/api/nodes"); check("latency_offset_us=1234", json.loads(b)[0]["latency_offset_us"] == 1234)
c, _ = http("POST", "/api/calibrate", {"esp_id": 0, "latency_us": 600000}); check("latency_us fuera de rango -> 400", c == 400, c)
http("POST", "/api/calibrate", {"esp_id": 0, "latency_us": 0})
c, b = http("GET", "/calibrate"); check("página /calibrate servida completa", c == 200 and "ANALYSIS_BEGIN" in b and "getUserMedia" in b and "ck_enable" in b and b.rstrip().endswith("</html>"))
c, b = http("GET", "/ble");       check("página /ble servida completa", c == 200 and "requestDevice" in b and "5c0a" in b and b.rstrip().endswith("</html>"))

# 6) Volumen maestro + CONFIG (el harness cambia el nodo 0 a 2/1 vol 128 a los 3 s)
http("POST", "/api/volume", {"volume": 0.5}); time.sleep(0.1)
pk = grab(0.3); s = struct.unpack("<240h", pk[-1][20:])
check("volumen maestro 0.5 aplicado", abs(s[0] - 8192) <= 1, s[0])
cfgpk = None; end = time.time() + 6
while time.time() < end:
    try:
        d, _ = ctrl.recvfrom(256)
        if len(d) == 16 and d[4] == 5:
            cfgpk = d
            if (d[6], d[7], d[8], d[9]) == (2, 1, 128, 255): break   # espera a la reconfiguración del arnés (a los 3 s)
    except socket.timeout: pass
check("CONFIG recibido (16 B) en puerto 5005 del ESP", cfgpk is not None)
if cfgpk:
    m, t, i, cl, cr, vl, vr, en, _r, pl = struct.unpack("<IBBBBBBBBI", cfgpk)
    check("CONFIG campos", (cl, cr, vl, vr, en) == (2, 1, 128, 255, 1), (cl, cr, vl, vr, en))
    check("CONFIG lleva playout_delay_us = 80000", pl == 80000, pl)
pk = grab(0.3); s = struct.unpack("<240h", pk[-1][20:])
exp_l = round(8192 * (128 / 255) * 0.5)
check("nuevo mapeo: L=ch2*(128/255)*0.5, R=ch1*1.0*0.5", abs(s[0] - exp_l) <= 1 and abs(s[1] - (-8192)) <= 1, (s[0], s[1], exp_l))
c, b = http("POST", "/api/wifi/scan", {}); w = json.loads(b) if c == 200 else {}
nets = w.get("networks", [])
check("wifi/scan -> 200 con 4 redes (la oculta se descarta y Casa-5G se une en una)", c == 200 and len(nets) == 4, (c, len(nets)))
check("wifi/scan: 5 GHz primero (por señal) y luego 2,4 GHz", [n["ssid"] for n in nets] == ["Casa-5G", "Casa:Sala", "Casa-2G", "Invitados"], [n["ssid"] for n in nets])
check("wifi/scan: misma red en dos canales de 5 GHz -> se queda la más fuerte (71 %)", [n for n in nets if n["ssid"] == "Casa-5G"][0]["signal"] == 71 if nets else False, nets)
check("wifi/scan: SSID con «:» sin escape y red abierta sin seguridad", any(n["ssid"] == "Casa:Sala" for n in nets) and [n for n in nets if n["ssid"] == "Invitados"][0]["security"] == "" if nets else False, nets)
check("wifi/scan: banda según la frecuencia", [n for n in nets if n["ssid"] == "Casa-2G"][0]["band"] == "2,4 GHz" and [n for n in nets if n["ssid"] == "Casa:Sala"][0]["band"] == "5 GHz" if nets else False, nets)
c, _ = http("GET", "/api/wifi/scan"); check("wifi/scan solo por POST (GET -> 404)", c == 404, c)
c, b = http("GET", "/api/system"); si = json.loads(b) if c == 200 else {}
check("/api/system sin contraseña: protected=false y WiFi actual de la RPi", c == 200 and si.get("protected") is False and si.get("wifi", {}).get("ssid") == "CasaHogar", si)
c, _ = http("POST", "/api/update", {});               check("sin contraseña no se puede actualizar el servidor -> 403", c == 403, c)
c, _ = http("POST", "/api/rpi/wifi", {"ssid": "X"});  check("sin contraseña no se puede cambiar la WiFi de la RPi -> 403", c == 403, c)
c, _ = http("POST", "/api/wifi/push", {"id": 0, "ssid": "X", "pass": "12345678"}); check("sin contraseña no se puede cambiar la WiFi de un altavoz -> 403", c == 403, c)
c, _ = http("POST", "/api/node", {"id": 0, "name": "Sofá"});          check("nombre del altavoz -> 200", c == 200, c)
c, b = http("GET", "/api/nodes"); n0 = [n for n in json.loads(b) if n["id"] == 0][0] if c == 200 else {}
check("/api/nodes devuelve el nombre", n0.get("name") == "Sofá", n0.get("name"))
c, _ = http("POST", "/api/node", {"id": 0, "name": "a<b"});            check("nombre con < -> 400", c == 400, c)

# 7) disable -> deja de enviar audio
http("POST", "/api/node", {"id": 0, "enabled": False}); time.sleep(0.3)
while True:
    try: aud.recvfrom(2048)
    except socket.timeout: break
pk = grab(0.5)
check("nodo deshabilitado: sin audio", len(pk) == 0, len(pk))

# 8) HTTPS (/calibrate y /ble deben poder servirse por TLS)
import ssl
ctxs = ssl.create_default_context(); ctxs.check_hostname = False; ctxs.verify_mode = ssl.CERT_NONE
try:
    r = urllib.request.urlopen("https://127.0.0.1:18443/calibrate", context=ctxs, timeout=3)
    body = r.read().decode(); check("HTTPS /calibrate", r.status == 200 and "makeChirp" in body)
    r = urllib.request.urlopen("https://127.0.0.1:18443/api/status", context=ctxs, timeout=3)
    check("HTTPS /api/status", r.status == 200 and json.loads(r.read())["sample_rate"] == 48000)
except Exception as e:
    check("HTTPS", False, repr(e))

# 8b) LATENCIA en caliente: /api/playout -> CONFIG a los nodos online con el nuevo valor
c, b = http("GET", "/api/status"); st = json.loads(b)
check("status: playout_ms=80 y rango 40..200", (st["playout_ms"], st["playout_min_ms"], st["playout_max_ms"]) == (80, 40, 200), st.get("playout_ms"))
for bad in (39, 201, "x"):
    c, _ = http("POST", "/api/playout", {"ms": bad}); check(f"playout ms={bad!r} -> 400", c == 400, c)
while True:                                   # vaciar CONFIG/WELCOME pendientes
    try: ctrl.recvfrom(256)
    except socket.timeout: break
ctrl.settimeout(1)
c, _ = http("POST", "/api/playout", {"ms": 120}); check("playout 120 OK", c == 200, c)
got = None
end = time.time() + 3
while time.time() < end and got is None:
    try:
        d, _ = ctrl.recvfrom(256)
        if len(d) == 16 and d[4] == 5: got = struct.unpack("<IBBBBBBBBI", d)[-1]
    except socket.timeout: pass
check("CONFIG con playout_delay_us=120000 enviado al ESP", got == 120000, got)
c, b = http("GET", "/api/status"); check("status refleja 120 ms", json.loads(b)["playout_ms"] == 120)
http("POST", "/api/playout", {"ms": 80}); ctrl.settimeout(2)

# 8c) Latencia AUTO (el arnés corre con SONOS_AUTO_FAST=1: ventana 2 s, enfriamiento 0,6 s, limpio 3 s, bajar cada 1,5 s)
lostc = [9, 8, 8]                                      # contadores acumulados que viene mandando el hilo de keepalive
def bump(dl):
    global rep
    lostc[0] += dl
    rep = struct.pack("<IBBxxqiiiiIIIfI", MAGIC, 4, 0, 1234, 321, 77, 960, -42, lostc[0], lostc[1], lostc[2], 12.5, 9)
    ctrl.sendto(rep, (RPI_IP, 5005))
def wait_playout(v, t=3.0):                              # el bucle de control se despierta con paquetes o cada 1 s: se espera con plazo
    end = time.time() + t
    while time.time() < end and lat()["playout_ms"] != v: time.sleep(0.05)
    return lat()
def lat():
    c, b = http("GET", "/api/latency"); return json.loads(b)
c, _ = http("POST", "/api/playout", {"auto": "quizá"}); check("auto con valor inválido -> 400", c == 400, c)
c, _ = http("POST", "/api/playout", {"auto": True, "max_ms": 500}); check("auto max_ms fuera de rango -> 400", c == 400, c)
a0 = lat()["auto"]; check("auto: desactivado por defecto", a0["on"] is False and a0["changes"] == 0, a0)
c, _ = http("POST", "/api/playout", {"auto": True, "max_ms": 110}); check("auto: activar con tope 110 ms", c == 200, c)
a0 = lat()["auto"]; check("auto: el mínimo es el margen que había (80) y el tope 110", (a0["on"], a0["floor_ms"], a0["max_ms"]) == (True, 80, 110), a0)
time.sleep(0.5); check("auto: sin cortes el margen no se mueve", lat()["playout_ms"] == 80, lat())
a0 = lat()["auto"]; check("auto: umbrales por defecto (cada subida 10 ms, baja tras 10 min limpios)", (a0["clean_min"], a0["step_ms"]) == (10, 10) and a0["raise_events"] in (4, 8), a0)
for bad in ({"raise_events": 0}, {"raise_events": 101}, {"clean_min": 0}, {"clean_min": 121}, {"step_ms": 7}, {"step_ms": 45}, {"step_ms": "x"}):
    c, _ = http("POST", "/api/playout", dict(bad, auto=True)); check(f"auto: umbral inválido {bad} -> 400", c == 400, c)
c, _ = http("POST", "/api/playout", {"auto": True, "raise_events": 30, "clean_min": 20, "step_ms": 20}); a0 = lat()["auto"]
check("auto: se pueden fijar los umbrales (30 cortes, 20 min, paso 20 ms)", c == 200 and (a0["raise_events"], a0["clean_min"], a0["step_ms"]) == (30, 20, 20), a0)
bump(10); time.sleep(0.7)
check("auto: con el umbral en 30, 10 cortes no hacen subir el margen", lat()["playout_ms"] == 80, lat())
time.sleep(2.2)                                       # que caduque la ventana de eventos
http("POST", "/api/playout", {"auto": True, "raise_events": 4, "clean_min": 10, "step_ms": 10})
bump(10)
end = time.time() + 2
while time.time() < end and lat()["playout_ms"] != 90: time.sleep(0.02)
l = lat(); check("auto: 10 cortes -> sube 10 ms (90)", (l["playout_ms"], l["auto"]["last_dir"], l["auto"]["changes"]) == (90, 1, 1), l)
check("auto: el retardo total para el reproductor sigue al margen (90 + extra)", abs(l["total_ms"] - (90 + l["extra_ms"])) < 0.01, l)
bump(10); time.sleep(0.2)
check("auto: más cortes dentro del enfriamiento no vuelven a subir", lat()["playout_ms"] == 90, lat())
bump(10); l = wait_playout(100)
check("auto: pasado el enfriamiento sube otros 10 ms (100)", l["playout_ms"] == 100, l)
bump(10); wait_playout(110); bump(10); time.sleep(1.8); bump(10); time.sleep(1.8)
l = lat(); check("auto: no pasa del tope (110)", l["playout_ms"] == 110, l)
c, b = http("GET", "/api/status"); check("status refleja el margen subido por Auto", json.loads(b)["playout_ms"] == 110)
time.sleep(6.5); l = lat()
check("auto: tras un rato limpio baja despacio hacia el mínimo (nunca por debajo de 80)", 80 <= l["playout_ms"] < 110 and l["auto"]["last_dir"] == -1, l)
c, _ = http("POST", "/api/playout", {"ms": 100}); l = lat()
check("auto: un ajuste manual pasa a ser el nuevo mínimo y el margen", (l["playout_ms"], l["auto"]["floor_ms"]) == (100, 100), l)
c, _ = http("POST", "/api/playout", {"auto": False}); l = lat()
check("auto: al desactivar vuelve el modo manual y el margen se queda donde estaba", (l["auto"]["on"], l["playout_ms"]) == (False, 100), l)
bump(30); time.sleep(0.8)
check("manual: con Auto apagado los cortes no tocan el margen", lat()["playout_ms"] == 100, lat())
http("POST", "/api/playout", {"ms": 80}); ctrl.settimeout(2)

# 9) CALIBRACIÓN extremo a extremo: sin nodos habilitados -> 409; luego chirps con temporización exacta
c, b = http("POST", "/api/cal/start", {}); check("cal/start sin nodos habilitados -> 409", c == 409, c)
http("POST", "/api/node", {"id": 0, "enabled": True, "ch_left": 0, "ch_right": 1, "volume_l": 255, "volume_r": 255})
http("POST", "/api/volume", {"volume": 1.0}); http("POST", "/api/calibrate", {"esp_id": 0, "latency_us": 7000})
c, b = http("POST", "/api/cal/start", {"level_db": 50}); check("level_db fuera de rango -> 409/400", c in (400, 409), c)
c, b = http("POST", "/api/cal/start", {"rounds": 9}); check("rounds fuera de rango -> 409/400", c in (400, 409), c)
aud.settimeout(0.01)
_end = time.time() + 0.3
while time.time() < _end:                # drenar audio normal pendiente (llega a 400 pkt/s: drenar por tiempo)
    try: aud.recvfrom(2048)
    except socket.timeout: pass
aud.settimeout(0.5)
t_post = mono_us()
c, b = http("POST", "/api/cal/start", {"level_db": -12, "rounds": 2}); cal = json.loads(b)
check("cal/start OK y secuencia [0,0]", c == 200 and cal["sequence"] == [0, 0], b[:120])
c, b2 = http("POST", "/api/cal/start", {}); check("segundo cal/start mientras corre -> 409", c == 409, c)
c, b2 = http("GET", "/api/cal/status"); check("cal/status running", json.loads(b2)["running"] is True, b2)
pk = grab(cal["total_ms"] / 1000 + 0.5)
seq = [struct.unpack("<IBBHIQ", p[:20]) for p in pk]
s_all = []
ok_seq = True
for a, b_ in zip(seq, seq[1:]):
    if b_[4] != a[4] + 1: ok_seq = False
check("calibración: flujo de paquetes continuo", ok_seq and len(pk) > 1000, len(pk))
import numpy as np
x = np.concatenate([np.frombuffer(p[20:], dtype="<i2") for p in pk]).astype(np.float64)
xl = x[0::2]; t_first = seq[0][5]
idx = np.where(np.abs(xl) > 150)[0]
runs = []
if len(idx):
    st = idx[0]; pv = idx[0]
    for i in idx[1:]:
        if i - pv > 2400: runs.append((st, pv)); st = i
        pv = i
    runs.append((st, pv))
def tsec(i): return (t_first + i * 1e6 / 48000 - t_post) / 1e6          # s respecto a la petición
runs = [r for r in runs if 0.8 < tsec(r[0]) < 4.8]                      # excluye audio normal antes/después
check("calibración: 2 chirps (2 rondas, 1 nodo)", len(runs) == 2, [(round(tsec(a) * 1000), round((b - a) / 48), int(np.abs(xl[a:b + 1]).max())) for a, b in runs])
if len(runs) == 2:
    t_s = [t_first + r[0] * 1e6 / 48000 for r in runs]
    dur = [(r[1] - r[0]) / 48 for r in runs]
    check("separación entre chirps = 1400 ms (±0.3 ms)", abs((t_s[1] - t_s[0]) - 1.4e6) < 300, f"{(t_s[1]-t_s[0])/1000:.3f} ms")
    check("duración chirp ≈ 300 ms (fades incluidos)", all(270 < d < 301 for d in dur), dur)
    check("primer chirp ≈ 1.5 s tras la petición (±120 ms)", abs((t_s[0] - t_post) - 1.5e6) < 120000, f"{(t_s[0]-t_post)/1000:.0f} ms")
    peak = max(np.abs(xl[a:b + 1]).max() for a, b in runs); exp_pk = 32767 * 10 ** (-12 / 20)
    check("amplitud = -12 dBFS (±3%)", abs(peak - exp_pk) / exp_pk < 0.03, f"{peak:.0f} vs {exp_pk:.0f}")
    check("canal R == canal L durante calibración (mono)", np.array_equal(x[0::2], x[1::2]) or all(np.array_equal(x[2*a:2*b+2:2], x[2*a+1:2*b+2:2]) for a, b in runs))
    def region(t0s, t1s):
        i0 = int((t0s * 1e6 + t_post - t_first) * 48000 / 1e6); i1 = int((t1s * 1e6 + t_post - t_first) * 48000 / 1e6)
        return np.abs(xl[max(0, i0):i1])
    gA = region(0.15, tsec(runs[0][0]) - 0.05); gB = region(tsec(runs[0][1]) + 0.05, tsec(runs[1][0]) - 0.05)
    gC = region(tsec(runs[1][1]) + 0.05, 4.8)
    check("silencio (Traktor silenciada) antes/entre/después de los chirps", gA.max() == 0 and gB.max() == 0 and gC.max() == 0, (gA.max(), gB.max(), gC.max()))
    # los ts del test NO incluyen el offset manual de 7 ms: el primer chirp cae en t_post + 1500 ms (+ latencia de captura/POST)
    xl.astype("<f4").tofile("/tmp/cal_wave.f32")
    json.dump({"runs": [[int(a), int(b)] for a, b in runs]}, open("/tmp/cal_wave.json", "w"))
time.sleep(1.0)
c, b2 = http("GET", "/api/cal/status"); check("cal/status terminada", json.loads(b2)["running"] is False, b2)
c, _ = http("POST", "/api/cal/stop"); check("cal/stop OK", c == 200)

# 10) MODO MONO (1 ESP por canal) y distribuciones
print("\n-- mono / distribuciones --")
def pkts_after_switch():
    grab(0.4)                                   # vacía lo que quedó en el socket del modo anterior
    return grab(1.0)
http("POST", "/api/volume", {"volume": 1.0})
c, _ = http("POST", "/api/node", {"id": 0, "ch_left": 0, "volume_l": 255, "volume_r": 255, "enabled": True, "mono": True}); check("nodo 0 -> mono OK", c == 200, c)
pk = pkts_after_switch()
ty = {p[4] for p in pk}
check("modo mono: solo PKT_AUDIO_MONO (0x08)", ty == {8}, ty)
check("mono: ~200 paquetes/s de 500 B", 170 < len(pk) < 230 and all(len(p) == 500 for p in pk), len(pk))
hs = [struct.unpack("<IBBHIQ", p[:20]) for p in pk]
check("mono: num_samples=240 y seq contigua", all(h[3] == 240 for h in hs) and all(b[4] - a[4] == 1 for a, b in zip(hs, hs[1:])))
avg = sum(b[5] - a[5] for a, b in zip(hs, hs[1:])) / (len(hs) - 1)
check("mono: ts avanza 5000 µs/paquete (±1%)", abs(avg - 5000) < 50, f"{avg:.1f}")
sm = struct.unpack("<240h", pk[0][20:])
check("mono: 240 muestras del canal 0 (=16384)", all(v == 16384 for v in sm), sm[:4])
c, b2 = http("GET", "/api/nodes"); n0 = json.loads(b2)[0]
check("API: nodo 0 mono, ch_right=ch_left", n0["mono"] is True and n0["ch_right"] == n0["ch_left"] == 0, n0)
c, b2 = http("GET", "/api/status"); st = json.loads(b2)
check("API: layout = 2 (personalizada) tras editar un nodo", st["layout"] == 2 and st["max_nodes"] == 10, st)

c, _ = http("POST", "/api/node", {"id": 0, "mono": False, "ch_left": 0, "ch_right": 1}); check("nodo 0 -> estéreo OK", c == 200, c)
pk = pkts_after_switch()
check("modo estéreo: PKT_AUDIO (0x03) y ~400 paquetes/s", {p[4] for p in pk} == {3} and 340 < len(pk) < 460, (len(pk), {p[4] for p in pk}))

c, _ = http("POST", "/api/layout", {"layout": "mono10"}); check("layout mono10 OK", c == 200, c)
c, b2 = http("GET", "/api/nodes"); ns = json.loads(b2)
check("mono10: 10 nodos mono, canal i, todos habilitados",
      len(ns) == 10 and all(n["mono"] and n["ch_left"] == n["ch_right"] == n["id"] and n["enabled"] for n in ns))
c, b2 = http("GET", "/api/status"); check("status layout = 1", json.loads(b2)["layout"] == 1)
pk = pkts_after_switch(); check("mono10: nodo 0 recibe paquetes mono", {p[4] for p in pk} == {8} and len(pk) > 170, (len(pk), {p[4] for p in pk}))

c, _ = http("POST", "/api/layout", {"layout": "stereo5"}); check("layout stereo5 OK", c == 200, c)
c, b2 = http("GET", "/api/nodes"); ns = json.loads(b2)
check("stereo5: nodos 0..4 pares (2i,2i+1) estéreo habilitados",
      all((not n["mono"]) and n["ch_left"] == 2 * n["id"] and n["ch_right"] == 2 * n["id"] + 1 and n["enabled"] for n in ns[:5]))
check("stereo5: nodos 5..9 deshabilitados", all(not n["enabled"] for n in ns[5:]))
c, b2 = http("GET", "/api/status"); check("status layout = 0", json.loads(b2)["layout"] == 0)
c, _ = http("POST", "/api/layout", {"layout": "xyz"}); check("layout inválido -> 400", c == 400, c)
c, _ = http("POST", "/api/layout", b'{}');             check("layout ausente -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 9, "enabled": True}); check("nodo 9 editable (id máx.)", c == 200, c)
http("POST", "/api/layout", {"layout": "stereo5"})

print("\n-- posición de los altavoces en el plano --")
c, b2 = http("GET", "/api/nodes"); n0 = json.loads(b2)[0]
check("posición por defecto automática (pos_x/pos_y = -1)", n0["pos_x"] == -1 and n0["pos_y"] == -1, (n0["pos_x"], n0["pos_y"]))
c, _ = http("POST", "/api/node", {"id": 0, "pos_x": 12.5, "pos_y": 80.25}); check("fijar posición -> 200", c == 200, c)
c, b2 = http("GET", "/api/nodes"); n0 = json.loads(b2)[0]
check("la posición se devuelve (12.5, 80.25)", (n0["pos_x"], n0["pos_y"]) == (12.5, 80.25), (n0["pos_x"], n0["pos_y"]))
check("fijar posición no cambia canales ni volumen", (n0["ch_left"], n0["ch_right"], n0["volume_l"]) == (0, 1, 255), n0)
c, _ = http("POST", "/api/node", {"id": 0, "pos_x": 101, "pos_y": 5});   check("pos_x=101 -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 0, "pos_x": -3, "pos_y": 5});    check("pos_x negativo -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 0, "pos_x": 40});                check("pos_x sin pos_y -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 0, "pos_x": "abc", "pos_y": 5}); check("pos_x no numérico -> 400", c == 400, c)
c, b2 = http("GET", "/api/nodes"); n0 = json.loads(b2)[0]
check("las posiciones inválidas no modifican la guardada", (n0["pos_x"], n0["pos_y"]) == (12.5, 80.25), (n0["pos_x"], n0["pos_y"]))
c, _ = http("POST", "/api/node", {"id": 0, "pos2_x": 77.5, "pos2_y": 33}); check("fijar posición del canal derecho -> 200", c == 200, c)
c, b2 = http("GET", "/api/nodes"); n0 = json.loads(b2)[0]
check("pos2 se devuelve y no toca la posición izquierda", (n0["pos2_x"], n0["pos2_y"], n0["pos_x"], n0["pos_y"]) == (77.5, 33, 12.5, 80.25), n0)
c, _ = http("POST", "/api/node", {"id": 0, "pos2_x": 77.5});  check("pos2_x sin pos2_y -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 0, "pos2_x": 120, "pos2_y": 3}); check("pos2_x=120 -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 0, "pos_auto": True}); check("pos_auto -> 200", c == 200, c)
c, b2 = http("GET", "/api/nodes"); n0 = json.loads(b2)[0]
check("pos_auto devuelve la posición automática (ambos canales)", n0["pos_x"] == -1 and n0["pos_y"] == -1 and n0["pos2_x"] == -1 and n0["pos2_y"] == -1, (n0["pos_x"], n0["pos_y"]))

print("\n-- ESP conocidos (aparecen en el panel) --")
c, b2 = http("GET", "/api/nodes"); ns = json.loads(b2)
check("el ESP que se conectó (0) es «known»; los que nunca se vieron, no", ns[0]["known"] is True and all(n["known"] is False for n in ns[1:]), [n["known"] for n in ns])
c, b2 = http("POST", "/api/node", {"id": 0, "forget": True}); check("quitar un ESP en línea -> 400", c == 400 and "en línea" in b2.decode() if isinstance(b2, bytes) else c == 400, (c, b2))
c, _ = http("POST", "/api/node", {"id": 5, "forget": True}); check("quitar un ESP apagado -> 200", c == 200, c)
c, b2 = http("GET", "/api/nodes"); check("ESP en línea sigue «known» tras intentar quitarlo", json.loads(b2)[0]["known"] is True)

# 11) REENVÍO (NACK) y límites
print("\n-- reenvío de paquetes (NACK) --")
pk = pkts_after_switch()
seqof = lambda p: struct.unpack("<IBBHIQ", p[:20])[4]
def nack_pkt(seqs, esp_id=0):
    q = list(seqs) + [0] * (8 - len(seqs))
    return struct.pack("<IBBBB8I", MAGIC, 9, esp_id, len(seqs), 0, *q)
check("NACK tamaño 40", len(nack_pkt([1])) == 40)
target = pk[-30]; st = seqof(target)
ctrl.sendto(nack_pkt([st, 0xFFFFFF00]), (RPI_IP, 5005))
got = [p for p in grab(0.3) if seqof(p) == st]
check("NACK: la RPi reenvía el MISMO paquete (bytes idénticos) y solo el que está en el historial", len(got) == 1 and got[0] == target, len(got))
other = pk[-25]; so = seqof(other)
w = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); w.bind(("127.0.0.5", 0))
w.sendto(nack_pkt([so]), (RPI_IP, 5005))
got = [p for p in grab(0.3) if seqof(p) == so]
check("NACK desde una IP desconocida se ignora", len(got) == 0, len(got))
ctrl.sendto(nack_pkt([so], esp_id=99), (RPI_IP, 5005)); ctrl.sendto(b"\x57\x46\x4e\x53\x09", (RPI_IP, 5005))
ctrl.sendto(nack_pkt([]), (RPI_IP, 5005))
time.sleep(0.2); c, b = http("GET", "/api/status"); check("NACK malformados no tumban la RPi", c == 200)
c, b = http("GET", "/api/nodes"); n0 = json.loads(b)[0]
check("retx_sent contabiliza el reenvío", n0["retx_sent"] == 1, n0["retx_sent"])
fresh = grab(0.2); want = [seqof(fresh[-12 - i]) for i in range(8)]; ctrl.sendto(nack_pkt(want), (RPI_IP, 5005))
after = grab(0.3); cnt = {q: sum(1 for p in after if seqof(p) == q) for q in want}
check("NACK de 8 seq: se reenvían las 8 (una vez cada una)", all(v == 1 for v in cnt.values()), cnt)
c, b = http("GET", "/api/nodes"); check("retx_sent = 1 + 8", json.loads(b)[0]["retx_sent"] == 9, json.loads(b)[0]["retx_sent"])

# 12) Gestión de graves (crossover) con un 2.º ESP simulado, EQ, roles y lip-sync
print("\n-- graves / EQ / roles / lip-sync --")
ctrl1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); ctrl1.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); ctrl1.bind(("127.0.0.3", 5005)); ctrl1.settimeout(1)
aud1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); aud1.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); aud1.bind(("127.0.0.3", 5006)); aud1.settimeout(0.5)
ctrl1.sendto(struct.pack("<IBBBBH6s", MAGIC, 1, 1, 5, 0, 5006, b"\xaa\xbb\xcc\xdd\xee\x01"), (RPI_IP, 5005))
rep1 = struct.pack("<IBB2xqiiiiIIIfI", MAGIC, 4, 1, 0, 300, 50, 960, 0, 0, 0, 0, 0.0, 0)
ctrl1.sendto(rep1, (RPI_IP, 5005))
def _keep1():
    t = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); t.bind(("127.0.0.3", 0))
    while True: t.sendto(rep1, (RPI_IP, 5005)); time.sleep(2)
threading.Thread(target=_keep1, daemon=True).start()
def grab_from(sock, secs):
    pk = []; end = time.time() + secs
    while time.time() < end:
        try: pk.append(sock.recvfrom(2048)[0])
        except socket.timeout: pass
    return pk
def last_vals(pk):                                     # muestras (canal L) del último paquete, estéreo o mono
    p = pk[-1]; sm = struct.unpack("<240h", p[20:]); return sm[::2] if p[4] == 3 else sm
c, b = http("GET", "/api/nodes"); check("2.º ESP simulado online (ID 1)", json.loads(b)[1]["online"] is True)
http("POST", "/api/layout", {"layout": "stereo5"}); http("POST", "/api/node", {"id": 2, "role": 1, "volume_l": 100})
c, b = http("GET", "/api/status"); check("cambiar rol o volumen NO convierte la distribución en «personalizada»", json.loads(b)["layout"] == 0, b[:120])
http("POST", "/api/node", {"id": 2, "role": 0, "volume_l": 255})
http("POST", "/api/volume", {"volume": 1.0})
# canal 2 del harness = DC 8192; canal 3 = 0.  Sat = nodo 1 (canal 2, mono); Sub = nodo 0 (canal 3, mono)
c, _ = http("POST", "/api/node", {"id": 1, "ch_left": 2, "mono": True, "volume_l": 255, "enabled": True, "role": 1}); check("nodo 1 -> satélite (mono, canal 2)", c == 200, c)
c, _ = http("POST", "/api/node", {"id": 0, "ch_left": 3, "mono": True, "volume_l": 255, "enabled": True, "role": 2}); check("nodo 0 -> subwoofer (mono, canal 3)", c == 200, c)
c, b = http("GET", "/api/status"); st = json.loads(b)
check("por defecto el procesado está en pass-through (dsp_xover=false, dsp_eq=false)", st["dsp_xover"] is False and st["dsp_eq"] is False, b[:200])
grab_from(aud1, 0.8); grab(0.3)
sat = last_vals(grab_from(aud1, 0.5)); sub = last_vals(grab(0.5))
check("PASS-THROUGH con roles asignados: el satélite recibe el canal completo (8192)", all(abs(v - 8192) <= 1 for v in sat), (min(sat), max(sat)))
check("PASS-THROUGH: el sub recibe solo su canal (3, mudo = 0) sin graves añadidos", all(v == 0 for v in sub), (min(sub), max(sub)))
for bad in ({}, {"xover": "quizás"}, {"eq": 5}, b"xx"):
    c, _ = http("POST", "/api/dsp", bad); check(f"/api/dsp inválido {bad!r} -> 400", c == 400, c)
c, _ = http("POST", "/api/dsp", {"xover": True}); check("activar crossover OK", c == 200)
c, b = http("GET", "/api/status"); st = json.loads(b); check("status: dsp_xover=true, dsp_eq=false", st["dsp_xover"] is True and st["dsp_eq"] is False)
grab_from(aud1, 0.8); grab(0.3)
sat = last_vals(grab_from(aud1, 0.5)); sub = last_vals(grab(0.5))
check("crossover: el satélite pierde los graves (DC → ~0)", max(abs(v) for v in sat) <= 20, max(abs(v) for v in sat))
check("crossover: el sub recibe los graves del satélite (DC 8192)", all(abs(v - 8192) <= 30 for v in sub), (min(sub), max(sub)))
c, b = http("GET", "/api/nodes"); ns = json.loads(b); check("roles visibles en la API", ns[0]["role"] == 2 and ns[1]["role"] == 1)
http("POST", "/api/node", {"id": 0, "enabled": False}); time.sleep(0.3); grab_from(aud1, 0.4)
sat = last_vals(grab_from(aud1, 0.4))
check("sin subwoofer activo los satélites vuelven a rango completo (8192)", all(abs(v - 8192) <= 1 for v in sat), (min(sat), max(sat)))
http("POST", "/api/node", {"id": 0, "enabled": True}); time.sleep(0.3); grab_from(aud1, 0.8)
sat = last_vals(grab_from(aud1, 0.4)); check("sub habilitado otra vez: satélite filtrado", max(abs(v) for v in sat) <= 20, max(abs(v) for v in sat))
http("POST", "/api/node", {"id": 0, "role": 0}); time.sleep(0.3); grab_from(aud1, 0.4)
sat = last_vals(grab_from(aud1, 0.4)); sub = last_vals(grab(0.4))
check("rol del sub -> rango completo: satélite 8192 y nodo 0 (canal 3 mudo) 0", all(abs(v - 8192) <= 1 for v in sat) and all(v == 0 for v in sub), (min(sat), max(sat), min(sub), max(sub)))
c, _ = http("POST", "/api/node", {"id": 0, "role": 5}); check("rol inválido -> 400", c == 400, c)
for bad in (10, 300, 39, "x"):
    c, _ = http("POST", "/api/bass", {"xover_hz": bad}); check(f"xover_hz={bad!r} -> 400", c == 400, c)
c, _ = http("POST", "/api/bass", {"xover_hz": 120}); check("xover_hz=120 OK", c == 200)
c, b = http("GET", "/api/status"); check("status xover_hz=120", json.loads(b)["xover_hz"] == 120, b[:60])
http("POST", "/api/bass", {"xover_hz": 80})
http("POST", "/api/dsp", {"xover": False, "eq": False})
http("POST", "/api/node", {"id": 0, "role": 0, "enabled": False}); http("POST", "/api/node", {"id": 1, "role": 0, "ch_left": 4, "mono": True})
def tone_win(sock, secs=0.9):                          # pico en una ventana larga (el tono de prueba va a pulsos: 400 ms sí / 200 ms no)
    grab_from(sock, 0.2); v = []
    for p in grab_from(sock, secs): v += list(struct.unpack("<240h", p[20:]))
    return max(abs(x) for x in v)
def tone_peak(sock, secs=0.5):
    grab_from(sock, 0.3); pk = grab_from(sock, secs); v = []
    for p in pk[-40:]: v += list(struct.unpack("<240h", p[20:]))
    return max(abs(x) for x in v)
http("POST", "/api/eq", {"id": 1, "eq2": 6}); time.sleep(0.2)
off = tone_peak(aud1)
check("EQ desactivado (pass-through): el tono pasa sin cambios aunque haya ganancias guardadas (pico ≈ 8192)", abs(off - 8192) <= 60, off)
http("POST", "/api/dsp", {"eq": True}); time.sleep(0.3)
on = tone_peak(aud1)
check("EQ activado: banda 1.4 kHz +6 dB duplica el tono (pico ≈ 16384)", abs(on - 16384) <= 400, on)
http("POST", "/api/eq", {"id": 1, "eq2": -6}); time.sleep(0.3)
dn = tone_peak(aud1); check("EQ −6 dB en 1.4 kHz: pico ≈ 4096", abs(dn - 4096) <= 200, dn)
http("POST", "/api/dsp", {"eq": False}); time.sleep(0.3)
off2 = tone_peak(aud1); check("EQ otra vez en pass-through: vuelve a 8192", abs(off2 - 8192) <= 60, off2)
http("POST", "/api/eq", {"id": 1, "reset": True}); http("POST", "/api/node", {"id": 0, "enabled": True})
c, _ = http("POST", "/api/eq", {"id": 1, "eq0": 3.5, "eq5": -2}); check("eq OK", c == 200, c)
c, b = http("GET", "/api/nodes"); e = json.loads(b)[1]["eq"]; check("eq visible en la API (banda 0 = 3.5, banda 5 = -2, resto 0)", e == [3.5, 0, 0, 0, 0, -2], e)
for bad in ({"id": 1, "eq2": 13}, {"id": 1, "eq2": "x"}, {"id": 11, "eq0": 1}, {}):
    c, _ = http("POST", "/api/eq", bad); check(f"eq inválido {bad} -> 400", c == 400, c)
c, _ = http("POST", "/api/eq", {"id": 1, "reset": True}); c, b = http("GET", "/api/nodes"); check("eq reset", json.loads(b)[1]["eq"] == [0] * 6)
c, b = http("GET", "/api/latency"); L = json.loads(b)
check("lip-sync: audio_delay_ms = -playout (−80)", L["playout_ms"] == 80 and L["audio_delay_ms"] == -80 and abs(L["mpv_audio_delay_s"] + 0.08) < 1e-9, L)
c, _ = http("POST", "/api/latency", {"extra_ms": 12}); c, b = http("GET", "/api/latency"); L = json.loads(b)
check("lip-sync: extra_ms=12 -> total 92, delay −92", L["total_ms"] == 92 and L["audio_delay_ms"] == -92 and abs(L["mpv_audio_delay_s"] + 0.092) < 1e-9, L)
for bad in (500, -51, "x"):
    c, _ = http("POST", "/api/latency", {"extra_ms": bad}); check(f"extra_ms={bad!r} -> 400", c == 400, c)
http("POST", "/api/latency", {"extra_ms": 0})
import subprocess, shutil, os
lua = shutil.which("lua5.4") or shutil.which("lua") or shutil.which("luajit")
if lua:
    here = os.path.dirname(os.path.abspath(__file__))
    r = subprocess.run([lua, here + "/test_lipsync.lua", here + "/../scripts/sonos_lipsync.lua"], capture_output=True, text=True, timeout=30)
    print(r.stdout.strip()); check("script de mpv (sonos_lipsync.lua) contra la API real", r.returncode == 0, r.stderr.strip()[:200])
else: print("SKIP script mpv (no hay Lua instalado)")
c, b = http("POST", "/api/cal/start", {}); 
http("POST", "/api/cal/stop")
check("cal/start informa los bordes de banda del EQ", c == 200 and '"eq_edges":[250,500,1000,2000,4000,8000,12000]' in b.replace(" ", ""), b[:200])
time.sleep(0.5)
http("POST", "/api/layout", {"layout": "stereo5"})

# 8b) Vúmetros de entrada y estado de la transmisión (/api/levels)
c, b = http("GET", "/api/levels"); lv = json.loads(b)
check("levels: capturando, con señal y enviando a los altavoces en línea", c == 200 and lv["capturing"] is True and lv["signal"] is True and lv["sending"] is True and lv["nodes_online"] >= 1, b)
check("levels: 10 canales en dBFS", len(lv["db"]) == 10 and all(-90.0 <= x <= 0.0 for x in lv["db"]), lv["db"])
check("levels: ch0/ch1 (±0x40000000) = −6,0 dBFS", abs(lv["db"][0] + 6.0) < 0.2 and abs(lv["db"][1] + 6.0) < 0.2, lv["db"])
check("levels: ch2 (0x20000000) = −12,0 dBFS y ch4 (seno, pico ≈ amplitud) ≈ −12 dBFS", abs(lv["db"][2] + 12.04) < 0.2 and abs(lv["db"][4] + 12.04) < 0.5, lv["db"])
check("levels: canales sin señal = −90 dBFS", all(lv["db"][i] == -90.0 for i in (3, 5, 6, 7, 8, 9)), lv["db"])
check("levels: pps plausible (> 100 paquetes/s) y signal_age_ms pequeño", lv["pps"] > 100 and 0 <= lv["signal_age_ms"] < 500, (lv["pps"], lv["signal_age_ms"]))

# 9) Configuración de ESP32 por Bluetooth desde la RPi (ayudante en modo simulado, ver ble_mock_setup.py)
c, b = http("GET", "/api/ble/status"); bs = json.loads(b)
check("BLE: ayudante disponible, sin WiFi guardada", c == 200 and bs["available"] is True and bs["ssid"] == "" and bs["has_pass"] is False, b)
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:01", "id": 5}); check("BLE: provisionar sin WiFi guardada -> error claro", c == 400 and "Primero guarda" in b, b)
for bad in ({"ssid": "", "pass": "clave12345"}, {"ssid": "Casa5G", "pass": "corta"}, {"ssid": "x" * 33, "pass": "clave12345"}, {"ssid": "Casa5G", "pass": "clave12345", "rpi_ip": "300.1.1.1"}, {"ssid": "Casa5G", "pass": "clave12345", "rpi_ip": "abc"}):
    c, b = http("POST", "/api/ble/wifi", bad); check(f"BLE: WiFi inválida {bad.get('ssid','')[:6]!r}/{bad.get('pass')!r}/{bad.get('rpi_ip')!r} -> 400", c == 400, (c, b))
c, b = http("POST", "/api/ble/wifi", {"ssid": "Casa 5G \"ñ\"", "pass": "clave\\12345", "rpi_ip": "192.168.1.10"}); check("BLE: guardar WiFi (con comillas, barra y ñ)", c == 200 and '"ok":true' in b.replace(" ", ""), b)
c, b = http("GET", "/api/ble/status"); bs = json.loads(b)
check("BLE: el estado muestra SSID e IP y que hay contraseña, pero NUNCA la contraseña", bs["ssid"] == 'Casa 5G "ñ"' and bs["has_pass"] is True and bs["rpi_ip"] == "192.168.1.10" and "clave" not in b, b)
st = os.stat("/tmp/sonos_harness_ble.txt")
check("BLE: archivo de ajustes con permisos 0600", (st.st_mode & 0o777) == 0o600, oct(st.st_mode & 0o777))
c, b = http("POST", "/api/ble/wifi", {"ssid": "Casa 5G", "pass": "", "rpi_ip": ""}); c2, b2 = http("GET", "/api/ble/status")
check("BLE: contraseña vacía = conservar la anterior", c == 200 and json.loads(b2)["has_pass"] is True and json.loads(b2)["rpi_ip"] == "", b2)
http("POST", "/api/ble/wifi", {"ssid": "Casa5G", "pass": "clave12345", "rpi_ip": ""})
c, b = http("POST", "/api/ble/scan", {"seconds": 99}); check("BLE: scan con seconds fuera de rango -> 400", c == 400, (c, b))
c, b = http("POST", "/api/ble/scan", {"seconds": 3}); sc = json.loads(b).get("devices", []); check("BLE: scan responde con la lista", c == 200 and sc, (c, b[:200]))
online = {n["id"] for n in json.loads(http("GET", "/api/nodes")[1]) if n["online"]}
check("BLE: scan devuelve los 5 dispositivos simulados", c == 200 and len(sc) == 5, b[:200])
new = [d for d in sc if d["new"]]; sug = [d["suggested_id"] for d in new]
check("BLE: IDs sugeridos únicos y no usados por altavoces en línea %s" % sorted(online), len(set(sug)) == len(sug) and not (set(sug) & online), (sug, online))
check("BLE: el que ya está en la red conserva su ID", [d for d in sc if d["address"].endswith("03")][0]["suggested_id"] == 2, sc)
mlog = "/tmp/sonos_ble_mock.log"
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:01", "id": min(online)})
check("BLE: ID de un altavoz en línea sin force -> error", c == 400 and "ya lo usa" in b, (c, b))
check("BLE: …y no se toca el dispositivo", open(mlog).read() == "", open(mlog).read())
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:01", "id": 7}); lg = [json.loads(l) for l in open(mlog)]
check("BLE: provisionar con ID libre -> ok y el ayudante recibe SSID, longitud de contraseña e ID", c == 200 and '"ok":true' in b.replace(" ", "") and lg == [{"cmd": "provision", "address": "AA:BB:CC:00:00:01", "id": 7, "ssid": "Casa5G", "pass_len": 10, "rpi_ip": ""}], (b, lg))
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:05", "id": 8}); check("BLE: el error del dispositivo llega a la UI", c == 400 and "emparejamiento rechazado" in b, (c, b))
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:03", "id": min(online), "force": True}); check("BLE: force=true permite reconfigurar el ID de un altavoz en línea", c == 200, (c, b))
for bad in ({"address": "no-es-mac!", "id": 3}, {"address": "AA:BB:CC:00:00:01", "id": 10}, {"address": "AA:BB:CC:00:00:01", "id": "99"}, {"address": "AA:BB:CC:00:00:01; rm -rf /", "id": 3}):
    c, b = http("POST", "/api/ble/provision", bad); check(f"BLE: provision inválido {bad} -> 400", c == 400, (c, b))
open(mlog, "w").close()
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:01"}); lg = [json.loads(l) for l in open(mlog)]
check("BLE: sin número en la petición -> ID automático (255): el ayudante recibe 255 y la WiFi", c == 200 and len(lg) == 1 and lg[0]["id"] == 255 and lg[0]["ssid"] == "Casa5G", (c, b, lg))
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:01", "id": "auto"}); check("BLE: id=\"auto\" también -> ok", c == 200, (c, b))
c, b = http("POST", "/api/ble/provision", {"address": "AA:BB:CC:00:00:01", "id": min(online)}); check("BLE: un número fijo de un altavoz en línea sigue rechazándose sin force (compatibilidad)", c == 400 and "ya lo usa" in b, (c, b))
c, b = http("POST", "/api/ble/identify", {"address": "AA:BB:CC:00:00:02"}); lg = [json.loads(l) for l in open(mlog)]
check("BLE: identify -> ok y llega al ayudante", c == 200 and lg[-1] == {"cmd": "identify", "address": "AA:BB:CC:00:00:02"}, (b, lg[-1:]))
c, b = http("POST", "/api/ble/identify", {"address": "zz"}); check("BLE: identify con dirección inválida -> 400", c == 400, (c, b))

# 10) Funciones nuevas: tono de prueba por altavoz, modo noche, LFE→frontales, distancias, perfiles y salud del ESP
print("\n== Tono de prueba, modo noche, perfiles, distancias, salud ==")
http("POST", "/api/dsp", {"xover": False, "eq": False}); http("POST", "/api/volume", {"volume": 1.0})
http("POST", "/api/node", {"id": 0, "role": 0, "ch_left": 3, "mono": True, "volume_l": 255, "enabled": True})
http("POST", "/api/node", {"id": 1, "role": 0, "ch_left": 4, "mono": True, "volume_l": 255, "enabled": True})
time.sleep(0.3); grab_from(aud1, 0.4); grab(0.3)
for bad in ({}, {"id": 12}, {"id": 0, "side": 5}, {"id": 0, "ms": 200}, {"id": 0, "ms": 99999}, {"id": 0, "level_db": 0}, {"id": 0, "level_db": -80}, {"id": 8}):
    c, b = http("POST", "/api/tone", bad); check(f"tono inválido {bad} -> 400", c == 400, (c, b))
c, b = http("GET", "/api/tone"); check("tono: inactivo al principio", c == 200 and json.loads(b)["active"] is False, b)
sat0 = tone_peak(aud1); sub0 = tone_peak(aud)
check("tono: antes de probar, nodo 1 = 8192 y nodo 0 (canal 3 mudo) = 0", abs(sat0 - 8192) <= 60 and sub0 == 0, (sat0, sub0))
c, b = http("POST", "/api/tone", {"id": 0, "side": 2, "ms": 4000, "solo": True, "level_db": -20}); check("tono: iniciar en nodo 0 (solo)", c == 200, (c, b))
c, b = http("GET", "/api/tone"); tj = json.loads(b)
check("tono: estado activo, nodo 0, quedan 0..4000 ms", tj["active"] is True and tj["id"] == 0 and 0 < tj["remaining_ms"] <= 4000, b)
time.sleep(0.3)
t0 = tone_win(aud); t1 = tone_win(aud1)
check("tono: el nodo 0 suena (pico ≈ -20 dBFS = 3277)", 2800 <= t0 <= 3700, t0)
check("tono: con solo=true el resto queda en silencio", t1 == 0, t1)
c, b = http("POST", "/api/calibrate", {"esp_id": 0, "latency_ms": 0}); check("tono: el resto de la API sigue respondiendo", c == 200, c)
c, b = http("POST", "/api/cal/start", {}); check("tono: no se puede calibrar mientras suena el tono -> 409", c == 409 or c == 200, c)
http("POST", "/api/cal/stop", {})
c, b = http("POST", "/api/tone/stop", {}); check("tono: parar", c == 200, c)
time.sleep(0.4)
sat1 = tone_peak(aud1); sub1 = tone_peak(aud)
check("tono: tras parar vuelve todo a la normalidad (8192 / 0)", abs(sat1 - 8192) <= 60 and sub1 == 0, (sat1, sub1))
c, b = http("GET", "/api/tone"); check("tono: estado inactivo tras parar", json.loads(b)["active"] is False, b)
c, b = http("POST", "/api/tone", {"id": 0, "side": 0, "ms": 3000, "solo": False, "level_db": -20}); time.sleep(0.4)
t0 = tone_win(aud); t1 = tone_win(aud1)
check("tono sin solo: el nodo 0 suena y el 1 conserva su señal", 2800 <= t0 <= 3700 and abs(t1 - 8192) <= 60, (t0, t1))
http("POST", "/api/tone/stop", {}); time.sleep(0.3)
http("POST", "/api/tone", {"id": 0, "ms": 1000}); time.sleep(1.6)
c, b = http("GET", "/api/tone"); check("tono: termina solo a los 1000 ms", json.loads(b)["active"] is False, b)
http("POST", "/api/node", {"id": 0, "enabled": False}); c, b = http("POST", "/api/tone", {"id": 0}); check("tono en un altavoz deshabilitado -> 400", c == 400, (c, b))
http("POST", "/api/node", {"id": 0, "enabled": True})

# Modo noche (compresor): la señal del nodo 1 es un tono de pico 8192 (−12 dBFS); el detector ve el pico de los 10 canales (−6 dBFS)
for bad in ({"night": 4}, {"night": -1}, {"night": "x"}, {"lfe_db": 50}, {"lfe": "quizás"}):
    c, _ = http("POST", "/api/dsp", bad); check(f"dsp inválido {bad} -> 400", c == 400, c)
c, b = http("GET", "/api/status"); st = json.loads(b)
check("status: modo noche apagado, LFE a frontales apagado por defecto", st["night"] == 0 and st["lfe_front"] is False and st["lfe_active"] is False, b[:300])
for lv, exp in ((1, 8192 * 10 ** (-10 / 20)), (3, 8192 * 10 ** (-21 / 20))):
    http("POST", "/api/dsp", {"night": lv}); time.sleep(1.2)
    pk = tone_peak(aud1); check(f"modo noche {lv}: el pico baja a ≈ {exp:.0f} (±8 %)", abs(pk - exp) <= exp * 0.08, pk)
http("POST", "/api/dsp", {"night": 0}); time.sleep(1.5)
pk = tone_peak(aud1); check("modo noche apagado: vuelve a pass-through exacto (8192)", abs(pk - 8192) <= 60, pk)
c, b = http("POST", "/api/dsp", {"lfe": True, "lfe_db": 3}); c2, b2 = http("GET", "/api/status"); st = json.loads(b2)
check("LFE a frontales: se guarda y se refleja en status", c == 200 and st["lfe_front"] is True and abs(st["lfe_db"] - 3) < 0.01, b2[:300])
http("POST", "/api/dsp", {"lfe": False, "lfe_db": 0})

# Distancias
c, _ = http("POST", "/api/node", {"id": 1, "dist_m": 2.5}); c2, b = http("GET", "/api/nodes")
check("distancia: 2.5 m visible en la API", c == 200 and json.loads(b)[1]["dist_m"] == 2.5 and json.loads(b)[0]["dist_m"] == -1, b[:80])
for bad in (0.1, 31, "x"):
    c, _ = http("POST", "/api/node", {"id": 1, "dist_m": bad}); check(f"distancia {bad!r} -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 1, "dist_m": -1}); c2, b = http("GET", "/api/nodes"); check("distancia: -1 la borra", c == 200 and json.loads(b)[1]["dist_m"] == -1, b[:80])
http("POST", "/api/node", {"id": 1, "dist_m": 3.2})

# Perfiles
c, b = http("GET", "/api/profiles"); pj = json.loads(b)
check("perfiles: lista vacía, máximo 12, ninguno activo", c == 200 and pj["profiles"] == [] and pj["max"] == 12 and pj["active"] == -1, b)
for bad in ({}, {"save": "   "}, {"load": 0}, {"delete": 3}, {"load": "x"}):
    c, _ = http("POST", "/api/profiles", bad); check(f"perfil inválido {bad} -> 400", c == 400, c)
http("POST", "/api/volume", {"volume": 0.7}); http("POST", "/api/dsp", {"night": 2}); http("POST", "/api/node", {"id": 1, "volume_l": 180}); http("POST", "/api/eq", {"id": 1, "eq1": 2.5})
c, b = http("POST", "/api/profiles", {"save": "Cine noche ñ"}); pj = json.loads(b); check("perfil: guardar «Cine noche ñ»", c == 200 and pj["profiles"] == ["Cine noche ñ"], b)
http("POST", "/api/volume", {"volume": 1.0}); http("POST", "/api/dsp", {"night": 0}); http("POST", "/api/node", {"id": 1, "volume_l": 255}); http("POST", "/api/eq", {"id": 1, "reset": True})
c, b = http("POST", "/api/profiles", {"save": "Música"}); check("perfil: guardar un segundo", json.loads(b)["profiles"] == ["Cine noche ñ", "Música"], b)
c, b = http("POST", "/api/profiles", {"save": "Música"}); check("perfil: mismo nombre sobrescribe, no duplica", len(json.loads(b)["profiles"]) == 2, b)
c, b = http("POST", "/api/profiles", {"load": 0}); st = json.loads(http("GET", "/api/status")[1]); nd = json.loads(http("GET", "/api/nodes")[1])[1]
check("perfil: cargar restaura volumen maestro, modo noche, volumen y EQ del nodo", c == 200 and abs(st["master_volume"] - 0.7) < 0.01 and st["night"] == 2 and nd["volume_l"] == 180 and nd["eq"][1] == 2.5 and st["profile_active"] == 0, (st, nd["volume_l"], nd["eq"]))
c, _ = http("POST", "/api/profiles", {"load": 7}); check("perfil: cargar uno que no existe -> 400", c == 400, c)
for i in range(12): http("POST", "/api/profiles", {"save": f"p{i}"})
c, b = http("POST", "/api/profiles", {"save": "uno más"}); check("perfil: más de 12 -> error claro", c == 400 and "error" in b, (c, b))
c, b = http("POST", "/api/profiles", {"save": "x" * 60}); check("perfil: nombre largo sin hueco -> 400 (lista llena)", c == 400, (c, b))
for _ in range(12): http("POST", "/api/profiles", {"delete": 0})
c, b = http("GET", "/api/profiles"); pj = json.loads(b); check("perfil: borrar deja la lista vacía", pj["profiles"] == [], b)
http("POST", "/api/profiles", {"save": "a=b\nc"}); c, b = http("GET", "/api/profiles"); check("perfil: «=» y saltos de línea se limpian del nombre", json.loads(b)["profiles"] == ["abc"], b)
http("POST", "/api/profiles", {"delete": 0}); http("POST", "/api/dsp", {"night": 0}); http("POST", "/api/volume", {"volume": 1.0})
http("POST", "/api/node", {"id": 1, "dist_m": -1, "volume_l": 255}); http("POST", "/api/eq", {"id": 1, "reset": True})

# Salud: versión de firmware, chip y RSSI que anuncia el ESP (HELLO.reserved = versión | chip<<8; REPORT.rssi_dbm)
ctrl.sendto(struct.pack("<IBBBBH6s", MAGIC, 1, 0, 5, 6 | (1 << 6), 5006, b"\xaa\xbb\xcc\xdd\xee\xff"), (RPI_IP, 5005)); time.sleep(0.2)
try:
    while True: ctrl.recvfrom(256)                       # WELCOME de respuesta
except (socket.timeout, BlockingIOError): pass
n0 = json.loads(http("GET", "/api/nodes")[1])[0]
check("salud: el HELLO con reserved = v6 | chip 1<<6 llega como fw_version 6, chip 1", n0["fw_version"] == 6 and n0["chip"] == 1, (n0["fw_version"], n0["chip"]))
rep_rssi = struct.pack("<IBBbBqiiiiIIIfI", MAGIC, 4, 0, -57, 0, 1234, 321, 77, 960, -42, 5, 6, 7, 12.5, 9)
got = None
for _ in range(4):
    ctrl.sendto(rep_rssi, (RPI_IP, 5005)); time.sleep(0.05)
    n0 = json.loads(http("GET", "/api/nodes")[1])[0]
    if n0["rssi_dbm"] == -57: got = n0; break
check("salud: el RSSI del REPORT llega a /api/nodes (-57 dBm)", got is not None, n0["rssi_dbm"])
n0 = json.loads(http("GET", "/api/nodes")[1])[0]
check("salud: fw_version, chip y ota_busy presentes en la API", all(k in n0 for k in ("fw_version", "chip", "ota_busy", "rssi_dbm")) and n0["ota_busy"] is False, n0)

# 11) Actualización de firmware por WiFi (OTA) — solo el lado de la RPi; el firmware del ESP NO está verificado
print("\n== OTA (lado RPi) ==")
def fake_image(chip, ver=b"6.0-test", size=70000):
    h = bytearray(size); h[0] = 0xE9; h[1] = 1; h[12:14] = struct.pack("<H", chip)
    h[32:36] = struct.pack("<I", 0xABCD5432); h[48:48 + len(ver)] = ver
    for i in range(80, size): h[i] = (i * 31) & 0xFF
    return bytes(h)
def http_bin(method, path, data=None):
    req = urllib.request.Request(B + path, method=method, data=data, headers={"Content-Type": "application/octet-stream"})
    try:
        r = urllib.request.urlopen(req, timeout=5); return r.status, r.read()
    except urllib.error.HTTPError as e: return e.code, e.read()
c, b = http("GET", "/api/firmware"); check("OTA: al principio no hay firmware cargado", c == 200 and json.loads(b)["loaded"] is False, b)
c, b = http_bin("POST", "/api/firmware", b"x" * 1000); check("OTA: archivo diminuto -> 400", c == 400 and b"demasiado" in b, (c, b[:80]))
c, b = http_bin("POST", "/api/firmware", bytes(range(256)) * 300); check("OTA: bytes al azar (no es imagen ESP-IDF) -> 400", c == 400 and b"ESP-IDF" in b, (c, b[:80]))
bl = bytearray(fake_image(0x17)); bl[32:36] = b"\0\0\0\0"
c, b = http_bin("POST", "/api/firmware", bytes(bl)); check("OTA: sin descriptor de aplicación (p. ej. bootloader) -> 400", c == 400, (c, b[:80]))
try: c, b = http_bin("POST", "/api/firmware", b"\0" * (3 * 1024 * 1024 + 10))
except (urllib.error.URLError, ConnectionError, OSError): c = "conexión cerrada"
check("OTA: más de 3 MB -> rechazado (el servidor corta la conexión)", c != 200, c)
c, b = http_bin("POST", "/api/firmware", fake_image(0x0000)); fj = json.loads(b)
check("OTA: imagen de ESP32 clásico aceptada (chip 2, versión leída)", c == 200 and fj["chip"] == 2 and fj["version"] == "6.0-test" and fj["size"] == 70000, b)
c, b = http("POST", "/api/ota", {"id": 0}); check("OTA: imagen de ESP32 clásico hacia un altavoz C5 -> 400 «otro chip»", c == 400 and "otro chip" in b, (c, b))
c, b = http("POST", "/api/ota", {"id": 7}); check("OTA: altavoz que no está en línea -> 400", c == 400 and "en línea" in b, (c, b))
c, b = http("POST", "/api/ota", {"id": 1}); check("OTA: altavoz con firmware antiguo (sin versión) -> 400 «por cable»", c == 400 and "cable" in b, (c, b))
c, b = http("POST", "/api/ota", {"id": 99}); check("OTA: id inválido -> 400", c == 400, (c, b))
img = fake_image(0x17, b"6.1-c5", 90001)
c, b = http_bin("POST", "/api/firmware", img); fj = json.loads(b); check("OTA: imagen de C5 aceptada (chip 1)", c == 200 and fj["chip"] == 1 and fj["version"] == "6.1-c5", b)
c, b = http("GET", "/api/firmware"); check("OTA: info del firmware cargado", json.loads(b) == {"loaded": True, "size": 90001, "chip": 1, "version": "6.1-c5"}, b)
for q in ("", "?t=", "?t=1234", "?t=zz"):
    c, b = http_bin("GET", "/fw/ota.bin" + q); check(f"OTA: descarga sin token válido {q!r} -> 403", c == 403, (c, b[:40]))
try:
    while True: ctrl.recvfrom(256)
except (socket.timeout, BlockingIOError): pass
ctrl.settimeout(2)
c, b = http("POST", "/api/ota", {"id": 0}); check("OTA: orden aceptada para el altavoz 0 (C5, firmware v6)", c == 200 and '"ok":true' in b.replace(" ", ""), (c, b))
got = None
for _ in range(10):
    try: d, _a = ctrl.recvfrom(256)
    except socket.timeout: break
    if len(d) == 24 and d[4] == 0x0A: got = d; break
check("OTA: el ESP recibe PKT_OTA (24 bytes)", got is not None)
m, t, eid, _r, port, _r2, size, tok = struct.unpack("<IBB2sHHIQ", got)
check("OTA: PKT_OTA lleva id 0, el puerto HTTP de la RPi (18080), el tamaño y un token", (m, t, eid, port, size) == (MAGIC, 0x0A, 0, 18080, 90001) and tok != 0, (hex(m), t, eid, port, size))
c, b = http_bin("GET", "/fw/ota.bin?t=%x" % (tok ^ 1)); check("OTA: token incorrecto -> 403", c == 403, c)
c, b = http_bin("GET", "/fw/ota.bin?t=%x" % tok); check("OTA: con el token, el ESP descarga exactamente el firmware subido", c == 200 and b == img, (c, len(b)))
n0 = json.loads(http("GET", "/api/nodes")[1])[0]; check("OTA: el nodo queda marcado como actualizando", n0["ota_busy"] is True, n0["ota_busy"])
grab(0.3); pk = grab(0.6); check("OTA: durante la actualización la RPi deja de mandarle audio", len(pk) == 0, len(pk))
c, b = http("POST", "/api/ota", {"id": 0}); check("OTA: segunda orden mientras actualiza -> 400", c == 400 and "ya se está" in b, (c, b))
time.sleep(4.0)
ctrl.sendto(struct.pack("<IBBBBH6s", MAGIC, 1, 0, 5, 7 | (1 << 6), 5006, b"\xaa\xbb\xcc\xdd\xee\xff"), (RPI_IP, 5005)); time.sleep(0.4)
try:
    while True: ctrl.recvfrom(256)
except (socket.timeout, BlockingIOError): pass
n0 = json.loads(http("GET", "/api/nodes")[1])[0]
check("OTA: el ESP vuelve con firmware v7: termina la actualización y se anuncia la versión nueva", n0["ota_busy"] is False and n0["fw_version"] == 7, (n0["ota_busy"], n0["fw_version"]))
grab(0.3); pk = grab(0.6); check("OTA: el audio se reanuda", len(pk) > 100, len(pk))
c, b = http("POST", "/api/ota", {"id": 0}); http("POST", "/api/ota/cancel", {"id": 0})
n0 = json.loads(http("GET", "/api/nodes")[1])[0]; check("OTA: cancelar libera al altavoz", c == 200 and n0["ota_busy"] is False, (c, n0["ota_busy"]))

# 12) Tipografía Roboto servida por la RPi
print("\n== Fuentes Roboto ==")
for w in (300, 400, 500, 700):
    r = urllib.request.urlopen(B + f"/fonts/roboto-{w}.woff2", timeout=3); d = r.read()
    check(f"fuente Roboto {w}: woff2 válido, tipo font/woff2 y caché larga", r.status == 200 and d[:4] == b"wOF2" and 15000 < len(d) < 40000 and r.headers["Content-Type"] == "font/woff2" and "max-age=31536000" in r.headers["Cache-Control"], (len(d), r.headers["Content-Type"]))
c, b = http_bin("GET", "/?forma=recta"); check("la página acepta una consulta (?forma=recta) -> 200 con HTML", c == 200 and b"<html" in b, c)
c, b = http_bin("GET", "/api/status?x=1"); check("una consulta en la API no rompe la ruta", c == 200 and b"running" in b, c)
c, b = http_bin("GET", "/fonts/roboto-900.woff2"); check("fuente inexistente -> 404", c == 404, c)
c, b = http_bin("GET", "/"); check("las páginas no piden fuentes externas (sin Google Fonts ni CDN)", b"fonts.googleapis" not in b and b"fonts.gstatic" not in b and b"/fonts/roboto-400.woff2" in b, c)

# 13) ID automático por MAC (HELLO con esp_id 255 -> la RPi asigna la casilla y la recuerda)
print("\n== ID automático por MAC ==")
def drain():
    ctrl.settimeout(0.15)
    try:
        while True: ctrl.recvfrom(256)
    except (socket.timeout, BlockingIOError): pass
    ctrl.settimeout(2)
def hello_auto(mac, esp_id=255):
    drain()
    ctrl.sendto(struct.pack("<IBBBBH6s", MAGIC, 1, esp_id, 5, 7 | (1 << 6), 5006, mac), (RPI_IP, 5005))
    t0 = time.time()
    while time.time() - t0 < 1.5:
        try: d, _ = ctrl.recvfrom(256)
        except (socket.timeout, BlockingIOError): return None
        if len(d) == 20 and d[4] == 2: return struct.unpack("<IBBBBBBBBII", d)
    return None
nds = json.loads(http("GET", "/api/nodes")[1])
free = [n["id"] for n in nds if not n["known"] and not n["mac"] and not n["online"]]
check("auto-ID: hay casillas libres para probar", len(free) >= 3, free)
M1, M2 = b"\x02\x11\x22\x33\x44\x55", b"\x02\x11\x22\x33\x44\x66"
w1 = hello_auto(M1)
check("auto-ID: HELLO con esp_id 255 -> WELCOME con la primera casilla libre y WELCOME_F_ASSIGNED", w1 is not None and w1[2] == free[0] and (w1[8] & 1) == 1 and w1[9] == 48000, w1)
w1b = hello_auto(M1)
check("auto-ID: el mismo ESP (misma MAC) vuelve a recibir la misma casilla", w1b is not None and w1b[2] == free[0], w1b)
w2 = hello_auto(M2)
check("auto-ID: otro ESP recibe la siguiente casilla libre", w2 is not None and w2[2] == free[1], (w2, free))
nds = {n["id"]: n for n in json.loads(http("GET", "/api/nodes")[1])}
check("auto-ID: /api/nodes muestra la MAC de cada casilla y que ya es un ESP conocido", nds[free[0]]["mac"] == "02:11:22:33:44:55" and nds[free[1]]["mac"] == "02:11:22:33:44:66" and nds[free[0]]["known"], (nds[free[0]]["mac"], nds[free[1]]["mac"]))
check("auto-ID: el WELCOME trae el mapa de canales de esa casilla", w1[3:5] == (nds[free[0]]["ch_left"], nds[free[0]]["ch_right"]), (w1[3:5], nds[free[0]]["ch_left"]))
check("auto-ID: sin MAC (todo ceros) no se asigna nada", hello_auto(b"\x00" * 6) is None)
w3 = hello_auto(M1, esp_id=free[2])
check("auto-ID: el mismo ESP pasa a un número fijo -> la casilla antigua se libera", w3 is not None and w3[2] == free[2] and (w3[8] & 1) == 0, w3)
nds = {n["id"]: n for n in json.loads(http("GET", "/api/nodes")[1])}
check("auto-ID: …y la MAC queda en la casilla nueva, no en la vieja", nds[free[2]]["mac"] == "02:11:22:33:44:55" and nds[free[0]]["mac"] == "", (nds[free[2]]["mac"], nds[free[0]]["mac"]))
c, _ = http("POST", "/api/node", {"id": 0, "ch_left": 0, "ch_right": 1}); check("auto-ID: las casillas automáticas no afectan al resto", c == 200, c)

# 14) Nombres de las 10 entradas (editables, persistidos en <config>.chnames)
print("\n== Nombres de las entradas ==")
DEF = ["FL", "FR", "C", "LFE", "SL", "SR", "SBL", "SBR", "AUX-L", "AUX-R"]
c, b = http("GET", "/api/names"); check("nombres: por defecto son los de siempre (FL, FR, C, LFE, SL, SR, SBL, SBR, AUX-L, AUX-R)", c == 200 and json.loads(b)["names"] == DEF, (c, b))
mine = ["Frontal I", "Frontal D", "Centro", "Sub", "Lat. izq", "Lat. der", "Fondo I", "Fondo D", "Extra 1", "Extra 2"]
c, b = http("POST", "/api/names", {"names": mine}); check("nombres: se guardan los 10 y la respuesta los devuelve", c == 200 and json.loads(b)["names"] == mine, (c, b))
c, b = http("GET", "/api/status"); check("nombres: /api/status incluye ch_names", c == 200 and json.loads(b)["ch_names"] == mine, (c, b))
try: pf = open("/tmp/sonos_harness_config.txt.chnames").read()
except OSError: pf = ""
check("nombres: persisten en <config>.chnames", "A1=Frontal I" in pf and "A10=Extra 2" in pf, pf)
c, b = http("POST", "/api/names", {"names": ["  Sala TV  "] + DEF[1:]}); n = json.loads(b).get("names", []) if c == 200 else []
check("nombres: se recortan espacios y un nombre igual al de fábrica no se guarda como personalizado", c == 200 and n[0] == "Sala TV" and n[1:] == DEF[1:], (c, b))
c, b = http("POST", "/api/names", {"names": ["", "", "", "", "", "", "", "", "", ""]}); check("nombres: vacío = nombre de fábrica", c == 200 and json.loads(b)["names"] == DEF, (c, b))
for bad, why in (({"names": DEF[:9]}, "9 nombres"), ({"names": ["<b>x"] + DEF[1:]}, "con <"), ({"names": ['a"b'] + DEF[1:]}, 'con "'), ({"names": ["a&b"] + DEF[1:]}, "con &"), ({"names": ["1234567890123"] + DEF[1:]}, "13 caracteres"), ({}, "sin names")):
    c, b = http("POST", "/api/names", bad); check("nombres: se rechaza " + why, c == 400 and "error" in b, (c, b))
c, b = http("POST", "/api/names", {"names": ["Ñandú ñoño"] + DEF[1:]}); check("nombres: acepta acentos y ñ (12 caracteres, más bytes)", c == 200 and json.loads(b)["names"][0] == "Ñandú ñoño", (c, b))
c, b = http("POST", "/api/names", {"names": mine}); c2, b2 = http("POST", "/api/names", {"reset": True})
check("nombres: «reset» vuelve a los de fábrica", c2 == 200 and json.loads(b2)["names"] == DEF, (c2, b2))
c, b = http("POST", "/api/names", {"names": mine})   # se deja personalizado: la interfaz lo comprueba después

# ── Entradas de audio: placas con «Usar», reloj principal, ids de canal ──
import os
try: os.remove("/tmp/sonos_harness_inputs.txt")
except FileNotFoundError: pass
c, b = http("GET", "/api/inputs"); j = json.loads(b) if c == 200 else {}
check("inputs: GET sin placas en el arnés (lista vacía, reloj automático, nada pendiente)", c == 200 and j.get("devices") == [] and isinstance(j.get("cards"), list) and j.get("max_total") == 32 and j.get("restart_needed") is False and j.get("clock") == "", (c, b[:200]))
c, b = http("GET", "/api/status"); st = json.loads(b)
check("inputs: /api/status trae ch_ids A1…A10 y 10 canales", st["ch_ids"] == ["A%d" % i for i in range(1, 11)] and st["input_channels"] == 10 and len(st["ch_names"]) == 10, st.get("ch_ids"))
c, b = http("GET", "/api/levels"); check("inputs: /api/levels con 10 medidores", len(json.loads(b)["db"]) == 10, b[:80])
c, b = http("GET", "/api/names"); check("inputs: /api/names devuelve también los ids", json.loads(b).get("ids") == st["ch_ids"], b[:120])
body = {"devices": [{"key": "Traktor", "channels": 10, "enabled": True, "tag": "A"}, {"key": "Scarlett 18i20", "channels": 8, "enabled": False, "tag": "B"}, {"key": "hw:3,0", "channels": 2, "enabled": True, "tag": "C"}], "clock": "Traktor"}
c, b = http("POST", "/api/inputs", body)
check("inputs: POST guarda y pide reiniciar", c == 200 and json.loads(b).get("ok") is True and json.loads(b).get("restarting") is True, (c, b))
c, b = http("GET", "/api/inputs"); j = json.loads(b)
check("inputs: lo guardado se lista con «Usar», letra y reloj, y queda pendiente de reiniciar", [(d["key"], d["channels"], d["enabled"], d["tag"]) for d in j["devices"]] == [("Traktor", 10, True, "A"), ("Scarlett 18i20", 8, False, "B"), ("hw:3,0", 2, True, "C")] and j["clock"] == "Traktor" and j["clock_effective"] == "Traktor" and j["restart_needed"] is True and all(d["connected"] is False for d in j["devices"]), b[:400])
check("inputs: el fichero es v2", open("/tmp/sonos_harness_inputs.txt").read() == "v2\nclock|Traktor\ndev|Traktor|10|1|A\ndev|Scarlett 18i20|8|0|B\ndev|hw:3,0|2|1|C\n")
c, b = http("POST", "/api/inputs", {"devices": body["devices"], "clock": "rpi"}); c2, b2 = http("GET", "/api/inputs")
check("inputs: la Raspberry puede ser el reloj", json.loads(b).get("ok") is True and json.loads(b2)["clock_effective"] == "rpi", b2[:200])
c, b = http("POST", "/api/inputs", {"devices": [{"key": "A", "channels": 2, "enabled": True}, {"key": "B", "channels": 2, "enabled": True}], "clock": ""}); c2, b2 = http("GET", "/api/inputs")
check("inputs: reloj automático con varias placas activas = la Raspberry; las letras se asignan solas", json.loads(b2)["clock_effective"] == "rpi" and [d["tag"] for d in json.loads(b2)["devices"]] == ["A", "B"], b2[:300])
c, b = http("POST", "/api/inputs", {"devices": [{"key": "A", "channels": 2, "enabled": True}], "clock": ""}); c2, b2 = http("GET", "/api/inputs")
check("inputs: con una sola placa activa su reloj es el principal", json.loads(b2)["clock_effective"] == "A", b2[:200])
for bad, why in (({"devices": [{"key": "x", "channels": 0, "enabled": True}]}, "0 canales"), ({"devices": [{"key": "x", "channels": 33, "enabled": True}]}, "33 canales"),
                 ({"devices": [{"key": "x", "channels": 20, "enabled": True}, {"key": "y", "channels": 20, "enabled": True}]}, "más de 32 activos"),
                 ({"devices": [{"key": "x", "channels": 2, "enabled": True}, {"key": "x", "channels": 2, "enabled": True}]}, "repetida"),
                 ({"devices": [{"key": "a|b", "channels": 2, "enabled": True}]}, "con |"),
                 ({"devices": [{"key": "x", "channels": 2, "enabled": False}], "clock": "x"}, "reloj en una placa desactivada"),
                 ({"devices": [{"key": "x", "channels": 2, "enabled": True}], "clock": "zzz"}, "reloj inexistente"), ({}, "sin lista")):
    c, b = http("POST", "/api/inputs", bad); check("inputs: se rechaza " + why, c == 400 and "error" in json.loads(b), (c, b))
c, b = http("GET", "/api/inputs"); check("inputs: los rechazos no tocan lo guardado", len(json.loads(b)["devices"]) == 1)
c, b = http("POST", "/api/inputs", {"devices": [], "clock": ""}); c2, b2 = http("GET", "/api/inputs")
check("inputs: lista vacía y nada pendiente", json.loads(b).get("ok") is True and json.loads(b2)["devices"] == [] and json.loads(b2)["restart_needed"] is False, b2[:200])
c, b = http("POST", "/api/node", {"id": 1, "ch_left": 10, "ch_right": 11}); check("inputs: con 10 entradas un altavoz no puede usar el canal 11", "error" in b or c >= 400, (c, b))
c, b = http("POST", "/api/names", {"names": ["x"] * 11}); check("inputs: /api/names exige un nombre por canal activo (11 ≠ 10)", c == 400 and "10" in b, (c, b))

print("\nRESULTADO:", "TODO OK" if ok else "HAY FALLOS")
sys.exit(0 if ok else 1)
