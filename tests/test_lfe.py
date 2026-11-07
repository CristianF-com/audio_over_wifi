#!/usr/bin/env python3
"""LFE -> frontales (sin subwoofer): mezcla real en el camino de audio de la RPi. Arnés con SONOS_LFE_DC=1 (canal 3 = DC 8192)
y un único ESP simulado (127.0.0.2, canales 0/1 = +16384/-16384)."""
import socket, struct, time, json, sys, urllib.request, urllib.error
MAGIC = 0x534E4657
ok = True
def check(n, c, x=""):
    global ok; print(("PASS " if c else "FAIL ") + n + (f"  [{x}]" if x else "")); ok &= bool(c)
B = "http://127.0.0.1:18080"
def http(method, path, body=None):
    req = urllib.request.Request(B + path, method=method, data=None if body is None else json.dumps(body).encode(), headers={"Content-Type": "application/json"})
    try:
        r = urllib.request.urlopen(req, timeout=3); return r.status, r.read().decode()
    except urllib.error.HTTPError as e: return e.code, e.read().decode()
ctrl = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); ctrl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); ctrl.bind(("127.0.0.2", 5005)); ctrl.settimeout(1)
aud = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); aud.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); aud.bind(("127.0.0.2", 5006)); aud.settimeout(0.3)
ctrl.sendto(struct.pack("<IBBBBH6s", MAGIC, 1, 0, 5, 0, 5006, b"\xaa" * 6), ("127.0.0.1", 5005))
rep = struct.pack("<IBB2xqiiiiIIIfI", MAGIC, 4, 0, 0, 300, 50, 960, 0, 0, 0, 0, 0.0, 0)
def grab(secs):
    pk = []; end = time.time() + secs
    while time.time() < end:
        try: pk.append(aud.recvfrom(2048)[0])
        except socket.timeout: pass
    return pk
def lr(pk):                                   # (L, R) del último paquete estéreo: lo normal es que sean constantes
    sm = struct.unpack("<240h", pk[-1][20:]); return sm[0::2], sm[1::2]
def settle():
    ctrl.sendto(rep, ("127.0.0.1", 5005)); time.sleep(0.6); grab(0.3); ctrl.sendto(rep, ("127.0.0.1", 5005))
time.sleep(0.3); ctrl.sendto(rep, ("127.0.0.1", 5005)); time.sleep(3.4)   # el arnés reconfigura el nodo 0 a los 3 s: esperar a que pase
ctrl.sendto(rep, ("127.0.0.1", 5005))
http("POST", "/api/volume", {"volume": 1.0}); http("POST", "/api/dsp", {"xover": False, "eq": False, "lfe": False, "lfe_db": 0, "night": 0})
http("POST", "/api/node", {"id": 0, "ch_left": 0, "ch_right": 1, "mono": False, "volume_l": 255, "volume_r": 255, "role": 0, "enabled": True})
settle(); L, R = lr(grab(0.6))
check("LFE apagado: pass-through exacto (L=16384, R=-16384)", all(v == 16384 for v in L) and all(v == -16384 for v in R), (min(L), max(L), min(R), max(R)))
c, b = http("GET", "/api/status"); check("status: lfe_active=false", json.loads(b)["lfe_active"] is False, b[:200])
http("POST", "/api/dsp", {"lfe": True, "lfe_db": 0}); settle(); L, R = lr(grab(0.6))
check("LFE a frontales 0 dB: L = 16384 + 8192 y R = -16384 + 8192 (±40, filtro asentado)", all(abs(v - 24576) <= 40 for v in L) and all(abs(v - (-8192)) <= 40 for v in R), (min(L), max(L), min(R), max(R)))
c, b = http("GET", "/api/status"); check("status: lfe_active=true mientras se mezcla", json.loads(b)["lfe_active"] is True, b[:200])
http("POST", "/api/dsp", {"lfe_db": -6}); settle(); L, R = lr(grab(0.6)); exp = 8192 * 10 ** (-6 / 20)
check("LFE -6 dB: se suma 8192·0,501 = %.0f" % exp, all(abs(v - (16384 + exp)) <= 40 for v in L), (min(L), max(L)))
http("POST", "/api/node", {"id": 0, "ch_left": 0, "ch_right": 3}); settle(); L, R = lr(grab(0.6))
check("si un altavoz ya reproduce el canal LFE (3), no se mezcla en los frontales (L=16384, R=8192)", all(v == 16384 for v in L) and all(v == 8192 for v in R), (min(L), max(L), min(R), max(R)))
c, b = http("GET", "/api/status"); check("status: lfe_active=false cuando el LFE ya suena", json.loads(b)["lfe_active"] is False, b[:200])
http("POST", "/api/node", {"id": 0, "ch_left": 0, "ch_right": 1}); http("POST", "/api/dsp", {"lfe": False}); settle(); L, R = lr(grab(0.6))
check("al apagarlo vuelve a pass-through exacto", all(v == 16384 for v in L) and all(v == -16384 for v in R), (min(L), max(L), min(R), max(R)))
print("RESULTADO LFE:", "TODO OK" if ok else "HAY FALLOS"); sys.exit(0 if ok else 1)
