#!/bin/bash
# provision_esp.sh — Escribe ID + WiFi en la partición NVS de UNA placa (sin recompilar).
# Uso:  ./scripts/provision_esp.sh <id 0-9> "<ssid>" "<password>" [puerto_serie]
# Requiere el entorno ESP-IDF activo (. ~/esp-idf/export.sh) y la tabla de particiones por
# defecto (nvs @ 0x9000, 0x6000 bytes).
set -euo pipefail
[ $# -ge 3 ] || { sed -n '2,6p' "$0"; exit 2; }
ID="$1"; SSID="$2"; PASS="$3"; PORT="${4:-/dev/ttyACM0}"
[[ "$ID" =~ ^[0-9]$ ]] || { echo "ERROR: id debe ser 0-9"; exit 2; }
[ -n "${IDF_PATH:-}" ] || { echo "ERROR: activa ESP-IDF primero (. ~/esp-idf/export.sh)"; exit 2; }
[ ${#SSID} -le 32 ] && [ ${#PASS} -le 64 ] || { echo "ERROR: SSID<=32, password<=64"; exit 2; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
SSID="$SSID" PASS="$PASS" ID="$ID" OUT="$TMP/nvs.csv" python3 - << 'PY'
import csv, os
with open(os.environ["OUT"], "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["key", "type", "encoding", "value"])
    w.writerow(["config", "namespace", "", ""])
    w.writerow(["wifi_ssid", "data", "string", os.environ["SSID"]])
    w.writerow(["wifi_pass", "data", "string", os.environ["PASS"]])
    w.writerow(["receiver_id", "data", "u8", os.environ["ID"]])
PY
python3 "$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py" \
    generate "$TMP/nvs.csv" "$TMP/nvs.bin" 0x6000
python3 -m esptool --chip esp32c5 -p "$PORT" write_flash 0x9000 "$TMP/nvs.bin"
echo "OK: placa en $PORT provisionada como ID $ID (red '$SSID')"
