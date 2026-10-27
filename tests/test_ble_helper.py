#!/usr/bin/env python3
"""Prueba scripts/sonos_ble_prov.py con un módulo «bleak» falso: qué se escribe en cada característica y en qué orden."""
import json, os, subprocess, sys, tempfile, textwrap
HELPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "scripts", "sonos_ble_prov.py")
fails = 0
def check(name, cond, extra=""):
    global fails
    print(("PASS " if cond else "FAIL ") + "BLE-helper " + name + ("" if cond else "  " + str(extra)))
    if not cond: fails += 1

tmp = tempfile.mkdtemp(prefix="fakebleak_")
os.makedirs(os.path.join(tmp, "bleak"))
open(os.path.join(tmp, "bleak", "__init__.py"), "w").write(textwrap.dedent('''
    import json, os
    SVC = "5c0a0001-8e3b-4d5e-9a41-534f4e4f5346"
    CFG = json.load(open(os.environ["FAKE_BLEAK_CFG"]))
    def log(e):
        with open(os.environ["FAKE_BLEAK_LOG"], "a") as f: f.write(json.dumps(e) + "\\n")
    class Dev:
        def __init__(self, a, n): self.address = a; self.name = n
    class Adv:
        def __init__(self, uu, rssi, name): self.service_uuids = uu; self.rssi = rssi; self.local_name = name
    class BleakScanner:
        @staticmethod
        async def discover(timeout=5, return_adv=False):
            return {d["address"]: (Dev(d["address"], d["name"]), Adv(d.get("uuids", [SVC]), d["rssi"], d["name"])) for d in CFG["scan"]}
    class BleakClient:
        def __init__(self, address, timeout=10): self.a = address
        async def __aenter__(self):
            if self.a in CFG.get("unreachable", []): raise RuntimeError("timeout de conexión")
            return self
        async def __aexit__(self, *x): return False
        async def pair(self): log(["pair", self.a])
        async def read_gatt_char(self, u):
            d = CFG["state"][self.a]
            n = u[4:8]
            if n == "0004": return bytearray([d["id"]])
            if n == "0002": return bytearray(d["ssid"].encode())
            if n == "0005": return bytearray(json.dumps(d["status"]).encode())
            raise KeyError(u)
        async def write_gatt_char(self, u, data, response=False):
            log(["write", self.a, u[4:8], bytes(data).decode("latin1") if u[4:8] != "0004" else list(data), response])
            if u[4:8] == "0006" and bytes(data) == b"apply" and CFG.get("apply_raises"): raise RuntimeError("Device disconnected")
'''))
def mkcfg(**kw):
    cfg = {"scan": [
        {"address": "AA:00:00:00:00:01", "name": "SonosWiFi-0001", "rssi": -70},
        {"address": "AA:00:00:00:00:02", "name": "SonosWiFi-0002", "rssi": -45},
        {"address": "AA:00:00:00:00:03", "name": "SonosWiFi-0003", "rssi": -60},
        {"address": "AA:00:00:00:00:09", "name": "Altavoz JBL", "rssi": -30, "uuids": []},
        {"address": "AA:00:00:00:00:0A", "name": "Otro", "rssi": -35, "uuids": ["5c0a0001-8e3b-4d5e-9a41-534f4e4f5346"]},   # sin nombre Sonos pero con el servicio
    ], "state": {
        "AA:00:00:00:00:01": {"id": 0, "ssid": "", "status": {"fw": 5, "id": 0, "wifi": 0}},
        "AA:00:00:00:00:02": {"id": 0, "ssid": "", "status": {"fw": 5, "id": 0, "wifi": 0}},
        "AA:00:00:00:00:03": {"id": 1, "ssid": "Casa5G", "status": {"fw": 5, "id": 1, "wifi": 1, "ip": "192.168.1.51"}},
        "AA:00:00:00:00:0A": {"id": 0, "ssid": "", "status": {"fw": 5, "id": 0, "wifi": 0}},
    }}
    cfg.update(kw); p = os.path.join(tmp, "cfg.json"); json.dump(cfg, open(p, "w")); return p
def run(cmd, req, cfgpath, log):
    env = dict(os.environ, PYTHONPATH=tmp, FAKE_BLEAK_CFG=cfgpath, FAKE_BLEAK_LOG=log)
    env.pop("SONOS_BLE_MOCK", None)
    r = subprocess.run([sys.executable, HELPER, cmd], input=json.dumps(req).encode(), capture_output=True, env=env, timeout=30)
    try: return json.loads(r.stdout.decode().strip().splitlines()[-1]), r
    except Exception: return {"_raw": r.stdout.decode(), "_err": r.stderr.decode()}, r
def writes(log):
    return [json.loads(l) for l in open(log)] if os.path.exists(log) else []

log = os.path.join(tmp, "log1.jsonl")
res, _ = run("scan", {"seconds": 3, "used": [0]}, mkcfg(), log)
d = res.get("devices", [])
check("scan: solo dispositivos SonosWiFi (por servicio o por nombre), el JBL se ignora", sorted(x["address"][-2:] for x in d) == ["01", "02", "03", "0A"], res)
check("scan: ordenados por señal (más fuerte primero)", [x["rssi"] for x in d] == sorted([x["rssi"] for x in d], reverse=True), d)
by = {x["address"][-2:]: x for x in d}
check("scan: lee ID, SSID y estado", by["03"]["id"] == 1 and by["03"]["ssid"] == "Casa5G" and by["03"]["status"]["ip"] == "192.168.1.51", by["03"])
check("scan: «new» = sin WiFi", by["01"]["new"] and by["02"]["new"] and not by["03"]["new"], d)
check("scan: el que ya está en la red conserva su ID (1)", by["03"]["suggested_id"] == 1, by["03"])
check("scan: IDs nuevos = menores libres (0 usado, 1 reservado) -> 2, 3, 4 sin repetir",
      sorted(by[k]["suggested_id"] for k in ("01", "02", "0A")) == [2, 3, 4], {k: by[k]["suggested_id"] for k in by})
check("scan: el más cercano recibe el menor ID libre (0A -35 dBm -> 2, 02 -45 dBm -> 3)", by["0A"]["suggested_id"] == 2 and by["02"]["suggested_id"] == 3, {k: by[k]["suggested_id"] for k in by})
check("scan: no escribe nada (solo lee)", not any(w[0] == "write" for w in writes(log)))

cfgp = mkcfg(unreachable=["AA:00:00:00:00:01"])
res, _ = run("scan", {"seconds": 3, "used": []}, cfgp, os.path.join(tmp, "log2.jsonl"))
u = [x for x in res["devices"] if x["address"].endswith("01")][0]
check("scan: un dispositivo inaccesible sale con error pero no rompe el resto", "No se pudo conectar" in (u["error"] or "") and len(res["devices"]) == 4, u)

log = os.path.join(tmp, "log3.jsonl")
req = {"address": "AA:00:00:00:00:02", "id": 4, "ssid": "Mi Red ñ5G", "pass": "secreta123", "rpi_ip": "192.168.1.10"}
res, _ = run("provision", req, mkcfg(), log)
w = [x for x in writes(log) if x[0] == "write"]
check("provision: devuelve ok", res == {"ok": True}, res)
check("provision: orden id, ssid, rpi, pass, apply", [x[2] for x in w] == ["0004", "0002", "0007", "0003", "0006"], w)
check("provision: ID=4 (1 byte), SSID UTF-8, contraseña y «apply» exactos",
      w[0][3] == [4] and w[1][3].encode("latin1").decode("utf-8") == "Mi Red ñ5G" and w[3][3] == "secreta123" and w[4][3] == "apply", w)
check("provision: escribe con respuesta (response=True)", all(x[4] is True for x in w), w)
check("provision: intenta emparejar antes de escribir", writes(log)[0][0] == "pair", writes(log)[:1])

log = os.path.join(tmp, "log4.jsonl")
res, _ = run("provision", {"address": "AA:00:00:00:00:02", "id": 5, "ssid": "Casa", "pass": "", "rpi_ip": ""}, mkcfg(), log)
w = [x[2] for x in writes(log) if x[0] == "write"]
check("provision sin contraseña ni IP: no escribe esas características (se conservan)", res == {"ok": True} and w == ["0004", "0002", "0006"], (res, w))

log = os.path.join(tmp, "log5.jsonl")
res, _ = run("provision", req, mkcfg(apply_raises=True), log)
check("provision: el ESP corta la conexión al reiniciar tras «apply» -> sigue siendo ok", res == {"ok": True}, res)
res, _ = run("provision", dict(req, address="AA:00:00:00:00:01"), mkcfg(unreachable=["AA:00:00:00:00:01"]), os.path.join(tmp, "log6.jsonl"))
check("provision: dispositivo inaccesible -> error legible", "No se pudo configurar" in res.get("error", ""), res)
for name, bad in (("ID 10", dict(req, id=10)), ("ID texto", dict(req, id="3")), ("SSID vacío", dict(req, ssid="")), ("contraseña corta", dict(req, **{"pass": "corta"})), ("SSID largo", dict(req, ssid="x" * 33))):
    lg = os.path.join(tmp, "logv.jsonl")
    if os.path.exists(lg): os.remove(lg)
    res, _ = run("provision", bad, mkcfg(), lg)
    check("provision rechaza " + name + " sin tocar el dispositivo", "error" in res and not writes(lg), (res, writes(lg)))

log = os.path.join(tmp, "log7.jsonl")
res, _ = run("identify", {"address": "AA:00:00:00:00:03"}, mkcfg(), log)
w = [x for x in writes(log) if x[0] == "write"]
check("identify: escribe «identify» en el comando", res == {"ok": True} and len(w) == 1 and w[0][2] == "0006" and w[0][3] == "identify", (res, w))

# sin bleak
env = dict(os.environ); env.pop("PYTHONPATH", None); env.pop("SONOS_BLE_MOCK", None)
r = subprocess.run([sys.executable, "-S", HELPER, "scan"], input=b"{}", capture_output=True, env=env, timeout=30)
out = r.stdout.decode().strip()
check("sin bleak instalado: error claro con la orden de instalación", "bleak" in out and "apt install" in out and r.returncode != 0, (out, r.returncode))

# modo simulado (el que usan las pruebas del servidor)
mock = os.path.join(tmp, "mock.json"); mlog = os.path.join(tmp, "mock.log")
json.dump({"devices": [{"address": "AA:00:00:00:00:01", "name": "SonosWiFi-X", "rssi": -50, "id": 0, "ssid": "", "status": {"wifi": 0}}], "log": mlog}, open(mock, "w"))
env = dict(os.environ, SONOS_BLE_MOCK=mock)
r = subprocess.run([sys.executable, HELPER, "scan"], input=b'{"used":[0,1]}', capture_output=True, env=env, timeout=30)
m = json.loads(r.stdout.decode())["devices"][0]
check("modo simulado: scan anota IDs igual que el real", m["suggested_id"] == 2 and m["new"], m)
r = subprocess.run([sys.executable, HELPER, "check"], capture_output=True, env=env, timeout=30)
check("modo simulado: check -> backend mock", json.loads(r.stdout.decode()).get("backend") == "mock")
r = subprocess.run([sys.executable, HELPER, "scan"], input=b"no es json", capture_output=True, env=env, timeout=30)
check("JSON de entrada inválido -> error y código 2", r.returncode == 2 and "error" in r.stdout.decode(), r.stdout)
print("RESULTADO BLE-helper: " + ("TODO OK" if not fails else "%d FALLOS" % fails))
sys.exit(1 if fails else 0)
