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

# 5) API robusta
c, _ = http("POST", "/api/node", {"id": 0, "ch_left": 99});           check("ch_left=99 -> 400", c == 400, c)
c, _ = http("POST", "/api/node", b'{"id":"abc"}');                    check("id=abc -> 400 (sin crash)", c == 400, c)
c, _ = http("POST", "/api/node", {"ch_left": 1});                     check("sin id -> 400", c == 400, c)
c, _ = http("POST", "/api/node", {"id": 10});                          check("id=10 -> 400 (máx. 10 nodos: 0..9)", c == 400, c)
c, _ = http("POST", "/api/volume", b'{"volume":"x"}');                check("volume inválido -> 400", c == 400, c)
c, _ = http("POST", "/api/calibrate", {"esp_id": 0, "latency_ms": 9999}); check("latencia fuera de rango -> 400", c == 400, c)
c, _ = http("GET", "/nope");                                          check("ruta desconocida -> 404", c == 404, c)
c, b = http("GET", "/api/status");                                    check("status OK", c == 200 and json.loads(b)["sample_rate"] == 48000, b[:80])
c, b = http("GET", "/");                                              check("panel servido completo (pass-through, plano, </html>)", c == 200 and "SonosWiFi" in b and "/api/dsp" in b and "id=\"room\"" in b and b.rstrip().endswith("</html>"))
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
cfgpk = None; end = time.time() + 5
while time.time() < end and cfgpk is None:
    try:
        d, _ = ctrl.recvfrom(256)
        if len(d) == 16 and d[4] == 5: cfgpk = d
    except socket.timeout: pass
check("CONFIG recibido (16 B) en puerto 5005 del ESP", cfgpk is not None)
if cfgpk:
    m, t, i, cl, cr, vl, vr, en, _r, pl = struct.unpack("<IBBBBBBBBI", cfgpk)
    check("CONFIG campos", (cl, cr, vl, vr, en) == (2, 1, 128, 255, 1), (cl, cr, vl, vr, en))
    check("CONFIG lleva playout_delay_us = 80000", pl == 80000, pl)
pk = grab(0.3); s = struct.unpack("<240h", pk[-1][20:])
exp_l = round(8192 * (128 / 255) * 0.5)
check("nuevo mapeo: L=ch2*(128/255)*0.5, R=ch1*1.0*0.5", abs(s[0] - exp_l) <= 1 and abs(s[1] - (-8192)) <= 1, (s[0], s[1], exp_l))

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
for bad in ({"address": "no-es-mac!", "id": 3}, {"address": "AA:BB:CC:00:00:01", "id": 10}, {"address": "AA:BB:CC:00:00:01"}, {"address": "AA:BB:CC:00:00:01; rm -rf /", "id": 3}):
    c, b = http("POST", "/api/ble/provision", bad); check(f"BLE: provision inválido {bad} -> 400", c == 400, (c, b))
c, b = http("POST", "/api/ble/identify", {"address": "AA:BB:CC:00:00:02"}); lg = [json.loads(l) for l in open(mlog)]
check("BLE: identify -> ok y llega al ayudante", c == 200 and lg[-1] == {"cmd": "identify", "address": "AA:BB:CC:00:00:02"}, (b, lg[-1:]))
c, b = http("POST", "/api/ble/identify", {"address": "zz"}); check("BLE: identify con dirección inválida -> 400", c == 400, (c, b))

print("\nRESULTADO:", "TODO OK" if ok else "HAY FALLOS")
sys.exit(0 if ok else 1)
