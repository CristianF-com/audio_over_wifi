#!/usr/bin/env python3
"""sonos_ble_prov.py — descubre y configura por Bluetooth LE los ESP32 de SonosWiFi desde la Raspberry.

Lo invoca el servidor web (rpi/src/ble_manager.cpp). Nunca recibe la contraseña por argv: las órdenes van por stdin (JSON).
  scan       {"seconds": 6, "used": [0,1]}                                 -> {"devices":[{address,name,rssi,id,ssid,status,error}]}
  provision  {"address","id","ssid","pass","rpi_ip"}         -> {"ok":true} | {"error":"..."}
  identify   {"address"}                                     -> {"ok":true} | {"error":"..."}
  check                                                       -> {"ok":true,"backend":"bleak"|"mock"}
Modo simulado (pruebas, sin Bluetooth): SONOS_BLE_MOCK=/ruta/mock.json  {"devices":[...], "log":"/ruta/log.jsonl"}
Dependencia real: python3-bleak (BlueZ).
"""
import asyncio, json, os, sys

SVC = "5c0a0001-8e3b-4d5e-9a41-534f4e4f5346"
def U(n): return "5c0a%04x-8e3b-4d5e-9a41-534f4e4f5346" % n
C_SSID, C_PASS, C_ID, C_ST, C_CMD, C_RPI = U(2), U(3), U(4), U(5), U(6), U(7)
NAME_PREFIX = "SonosWiFi-"
CONNECT_TIMEOUT = 12.0

def annotate(devs, used):
    """suggested_id: el propio ID si ya está en la red; si no, el menor libre (no usado por un altavoz en línea ni ya sugerido). new = sin WiFi."""
    taken = set(int(u) for u in (used or []))
    for d in devs:
        st = d.get("status") or {}
        d["new"] = not (st.get("wifi") == 1)
    for d in devs:
        if not d["new"] and d.get("id") is not None:
            d["suggested_id"] = d["id"]; taken.add(d["id"])
    for d in devs:
        if d["new"] and d.get("status") is not None:      # sin estado legible (no se pudo conectar) no se reserva ningún ID
            free = [i for i in range(10) if i not in taken]
            d["suggested_id"] = free[0] if free else None
            if free: taken.add(free[0])
        elif d["new"]:
            d["suggested_id"] = None
    return devs

def out(obj):
    sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()

def fail(msg, code=1):
    out({"error": str(msg)}); sys.exit(code)

# ─── modo simulado ───────────────────────────────────────────────────────────────
def mock_load():
    with open(os.environ["SONOS_BLE_MOCK"], encoding="utf-8") as f: return json.load(f)

def mock_log(m, entry):
    p = m.get("log")
    if p:
        with open(p, "a", encoding="utf-8") as f: f.write(json.dumps(entry) + "\n")

def mock_run(cmd, req):
    m = mock_load()
    devs = {d["address"]: d for d in m.get("devices", [])}
    if cmd == "scan":
        return {"devices": annotate([{k: d.get(k) for k in ("address", "name", "rssi", "id", "ssid", "status", "error")} for d in devs.values()], req.get("used"))}
    d = devs.get(req.get("address"))
    if d is None: return {"error": "No se encuentra el dispositivo (¿apagado o fuera de alcance?)"}
    if d.get("fail"): return {"error": d["fail"]}
    if cmd == "identify":
        mock_log(m, {"cmd": "identify", "address": d["address"]}); return {"ok": True}
    mock_log(m, {"cmd": "provision", "address": d["address"], "id": req.get("id"), "ssid": req.get("ssid"),
                 "pass_len": len(req.get("pass") or ""), "rpi_ip": req.get("rpi_ip") or ""})
    return {"ok": True}

# ─── bleak (real) ─────────────────────────────────────────────────────────────
def import_bleak():
    try:
        import bleak
        return bleak
    except ImportError:
        fail("Falta la librería bleak. Instálala con: sudo apt install python3-bleak")

def dec(b): return bytes(b).decode("utf-8", "replace")

async def inspect(BleakClient, address):
    info = {"id": None, "ssid": None, "status": None, "error": None}
    try:
        async with BleakClient(address, timeout=CONNECT_TIMEOUT) as c:
            try:
                info["id"] = bytes(await c.read_gatt_char(C_ID))[0]
                info["ssid"] = dec(await c.read_gatt_char(C_SSID))
                st = dec(await c.read_gatt_char(C_ST))
                try: info["status"] = json.loads(st)
                except ValueError: info["status"] = None
            except Exception as e:           # lectura que exige emparejar, etc.
                info["error"] = "No se pudo leer: %s" % (e,)
    except Exception as e:
        info["error"] = "No se pudo conectar: %s" % (e,)
    return info

async def scan(req):
    bleak = import_bleak()
    secs = max(3.0, min(15.0, float(req.get("seconds", 6))))
    try:
        found = await bleak.BleakScanner.discover(timeout=secs, return_adv=True)
    except Exception as e:
        fail("Bluetooth no disponible: %s (¿bluetoothd activo? ¿el usuario está en el grupo «bluetooth»?)" % (e,))
    devs = []
    for d, adv in found.values():
        uu = [u.lower() for u in (getattr(adv, "service_uuids", None) or [])]
        name = (getattr(adv, "local_name", None) or getattr(d, "name", None) or "")
        if SVC in uu or name.startswith(NAME_PREFIX):
            devs.append({"address": d.address, "name": name or d.address, "rssi": getattr(adv, "rssi", None)})
    devs.sort(key=lambda x: -(x["rssi"] if x["rssi"] is not None else -999))
    for d in devs:                                    # de uno en uno: BlueZ no aguanta bien varias conexiones a la vez
        d.update(await inspect(bleak.BleakClient, d["address"]))
    return {"devices": annotate(devs, req.get("used"))}

async def write(c, uuid, data):
    await c.write_gatt_char(uuid, data, response=True)

async def provision(req):
    bleak = import_bleak()
    addr = req.get("address"); ident = req.get("id"); ssid = req.get("ssid") or ""
    pw = req.get("pass") or ""; rpi = (req.get("rpi_ip") or "").strip()
    if not addr: return {"error": "Falta la dirección del dispositivo"}
    if not isinstance(ident, int) or not 0 <= ident <= 9: return {"error": "ID fuera de rango (0-9)"}
    if not 1 <= len(ssid.encode()) <= 32: return {"error": "SSID vacío o de más de 32 bytes"}
    if pw and not 8 <= len(pw.encode()) <= 63: return {"error": "La contraseña debe tener entre 8 y 63 caracteres"}
    try:
        async with bleak.BleakClient(addr, timeout=CONNECT_TIMEOUT) as c:
            try: await c.pair()                       # las escrituras exigen enlace cifrado (Just Works); si ya está emparejado no pasa nada
            except Exception: pass
            await write(c, C_ID, bytes([ident]))
            await write(c, C_SSID, ssid.encode())
            if rpi: await write(c, C_RPI, rpi.encode())
            if pw: await write(c, C_PASS, pw.encode())
            try:
                await write(c, C_CMD, b"apply")       # el ESP se reinicia y puede cortar la conexión antes de responder
            except Exception as e:
                if "disconnect" not in str(e).lower() and "not connected" not in str(e).lower(): raise
    except Exception as e:
        return {"error": "No se pudo configurar: %s" % (e,)}
    return {"ok": True}

async def identify(req):
    bleak = import_bleak()
    try:
        async with bleak.BleakClient(req.get("address"), timeout=CONNECT_TIMEOUT) as c:
            try: await c.pair()
            except Exception: pass
            await write(c, C_CMD, b"identify")
    except Exception as e:
        return {"error": "No se pudo conectar: %s" % (e,)}
    return {"ok": True}

def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("scan", "provision", "identify", "check"):
        fail("uso: sonos_ble_prov.py scan|provision|identify|check (JSON por stdin)", 2)
    cmd = sys.argv[1]
    if cmd == "check":
        if os.environ.get("SONOS_BLE_MOCK"): out({"ok": True, "backend": "mock"}); return
        import_bleak(); out({"ok": True, "backend": "bleak"}); return
    try:
        raw = sys.stdin.buffer.read().decode("utf-8")
        req = json.loads(raw) if raw.strip() else {}
    except ValueError:
        fail("JSON de entrada no válido", 2)
    if os.environ.get("SONOS_BLE_MOCK"):
        out(mock_run(cmd, req)); return
    res = asyncio.run({"scan": scan, "provision": provision, "identify": identify}[cmd](req))
    out(res)

if __name__ == "__main__":
    main()
