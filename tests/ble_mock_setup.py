#!/usr/bin/env python3
"""Prepara el Bluetooth simulado que usan run_tests.sh y run_ui_tests.sh (SONOS_BLE_MOCK)."""
import json, os
devs = [
  {"address": "AA:BB:CC:00:00:01", "name": "SonosWiFi-A101", "rssi": -48, "id": 0, "ssid": "", "status": {"fw": 5, "id": 0, "wifi": 0, "ip": "", "rssi": 0, "rpi": 0, "sync": 0}},
  {"address": "AA:BB:CC:00:00:02", "name": "SonosWiFi-B202", "rssi": -63, "id": 0, "ssid": "", "status": {"fw": 5, "id": 0, "wifi": 0, "ip": "", "rssi": 0, "rpi": 0, "sync": 0}},
  {"address": "AA:BB:CC:00:00:03", "name": "SonosWiFi-C303", "rssi": -71, "id": 2, "ssid": "Casa5G", "status": {"fw": 5, "id": 2, "wifi": 1, "ip": "192.168.1.52", "rssi": -55, "rpi": 1, "sync": 1}},
  {"address": "AA:BB:CC:00:00:04", "name": "SonosWiFi-D404", "rssi": -80, "id": None, "ssid": None, "status": None, "error": "No se pudo conectar: timeout"},
  {"address": "AA:BB:CC:00:00:05", "name": "SonosWiFi-E505", "rssi": -66, "id": 0, "ssid": "", "status": {"fw": 5, "id": 0, "wifi": 0}, "fail": "No se pudo configurar: emparejamiento rechazado"},
]
log = "/tmp/sonos_ble_mock.log"
open(log, "w").close()
json.dump({"devices": devs, "log": log}, open("/tmp/sonos_ble_mock.json", "w"))
for f in ("/tmp/sonos_harness_ble.txt",):
    try: os.remove(f)
    except OSError: pass
