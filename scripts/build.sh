#!/bin/bash
# build.sh — Compilar e instalar SonosWiFi en la Raspberry Pi (servicio systemd)
# Uso:  sudo ./scripts/build.sh      (o ./scripts/build.sh con sudo configurado)
set -euo pipefail

REAL_USER="${SUDO_USER:-$(whoami)}"
id "$REAL_USER" &>/dev/null || { echo "ERROR: usuario '$REAL_USER' no encontrado"; exit 1; }
echo "=== SonosWiFi build — usuario del servicio: $REAL_USER ==="

echo "Instalando dependencias..."
sudo apt-get update -qq
sudo apt-get install -y libasound2-dev libssl-dev openssl cmake g++ pkg-config python3 bluez
# Bluetooth desde la RPi (opcional): python3-bleak si existe en el repositorio; si no, pip
sudo apt-get install -y python3-bleak 2>/dev/null || sudo pip3 install --break-system-packages bleak || echo "AVISO: no se pudo instalar bleak; la configuración por Bluetooth desde la RPi no estará disponible (sigue la de /ble)"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../rpi"
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"

echo "Instalando binario..."
sudo install -m 755 sonos_wifi /usr/local/bin/sonos_wifi
sudo install -d /usr/local/lib/sonos_wifi
sudo install -m 755 "$SCRIPT_DIR/sonos_ble_prov.py" /usr/local/lib/sonos_wifi/sonos_ble_prov.py
sudo usermod -aG bluetooth "$REAL_USER" 2>/dev/null || true

if arecord -l 2>/dev/null | grep -qi traktor; then
    echo "Traktor detectada:"; arecord -l | grep -i traktor
else
    echo "ADVERTENCIA: no se ve la Traktor en 'arecord -l'. El servicio reintentará cada 3 s."
fi

# Certificado HTTPS autofirmado (el navegador del móvil exige HTTPS para micrófono y Web Bluetooth)
sudo mkdir -p /etc/sonos_wifi
if [ ! -f /etc/sonos_wifi/cert.pem ]; then
    HN="$(hostname)"; IP="$(hostname -I | awk '{print $1}')"
    SAN="DNS:${HN},DNS:${HN}.local"; [ -n "$IP" ] && SAN="${SAN},IP:${IP}"
    sudo openssl req -x509 -newkey rsa:2048 -nodes -days 3650 -subj "/CN=${HN}" \
        -addext "subjectAltName=${SAN}" \
        -keyout /etc/sonos_wifi/key.pem -out /etc/sonos_wifi/cert.pem 2>/dev/null
    sudo chown root:"$REAL_USER" /etc/sonos_wifi/key.pem && sudo chmod 640 /etc/sonos_wifi/key.pem
    echo "Certificado HTTPS creado para: ${SAN}"
fi

# '-d auto' busca la tarjeta cuyo nombre contiene "traktor" al arrancar
# (el índice hw:N cambia entre reinicios). Para fijar una: -d hw:2,0
sudo tee /etc/systemd/system/sonos_wifi.service > /dev/null << UNIT
[Unit]
Description=SonosWiFi UDP Audio Streamer
After=network-online.target sound.target
Wants=network-online.target
StartLimitIntervalSec=60
StartLimitBurst=10

[Service]
Type=simple
ExecStart=/usr/local/bin/sonos_wifi -d auto -p 8080 -S 8443 -c /var/lib/sonos_wifi/config.txt
Restart=always
RestartSec=3
User=${REAL_USER}
SupplementaryGroups=audio bluetooth
# El propio programa pone SCHED_FIFO solo en los hilos de captura (80) y control (60).
AmbientCapabilities=CAP_SYS_NICE
LimitRTPRIO=95
LimitMEMLOCK=infinity
# Estado persistente (volúmenes, canales, latencias, playout): /var/lib/sonos_wifi/config.txt
StateDirectory=sonos_wifi
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
UNIT

id -nG "$REAL_USER" | grep -qw audio || { sudo usermod -aG audio "$REAL_USER"; echo "Añadido $REAL_USER al grupo audio (reinicia sesión)"; }

sudo systemctl daemon-reload
sudo systemctl enable sonos_wifi
sudo systemctl restart sonos_wifi

echo
echo "OK. Estado: sudo systemctl status sonos_wifi | Logs: journalctl -u sonos_wifi -f"
IPA="$(hostname -I | awk '{print $1}')"
echo "Web UI:        http://${IPA}:8080"
echo "Calibrar (móvil): https://${IPA}:8443/calibrate   (acepta el aviso del certificado)"
echo "Config. BLE:      https://${IPA}:8443/ble"
echo "ESP32: ver README.md (idf.py flash + configurar por Bluetooth en /ble, o scripts/provision_esp.sh)"
