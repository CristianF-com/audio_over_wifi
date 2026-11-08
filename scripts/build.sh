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
# Prioridad de tiempo real también desde systemd para todo el proceso (FIFO 50), como en el puente anterior que iba bien;
# además el programa sube a FIFO 80 el hilo de captura y a 60 el de control.
Nice=-10
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=50
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

# ── Cambiar la WiFi de la RPi y actualizar el servidor desde la web ──────────────────────────────
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
sed "s|@REPO@|$REPO_DIR|; s|@USER@|$REAL_USER|" "$SCRIPT_DIR/update.sh" | sudo tee /usr/local/lib/sonos_wifi/update.sh > /dev/null
sudo chmod 755 /usr/local/lib/sonos_wifi/update.sh
sudo tee /etc/systemd/system/sonos_wifi_update.service > /dev/null << UNIT2
[Unit]
Description=Actualizar SonosWiFi (git pull + compilar + reiniciar)
[Service]
Type=oneshot
User=root
ExecStart=/usr/local/lib/sonos_wifi/update.sh
TimeoutStartSec=1800
UNIT2
# El servicio (usuario normal) solo puede lanzar ESTE servicio concreto, nada más
echo "${REAL_USER} ALL=(root) NOPASSWD: /usr/bin/systemctl start --no-block sonos_wifi_update.service" | sudo tee /etc/sudoers.d/sonos_wifi > /dev/null
sudo chmod 440 /etc/sudoers.d/sonos_wifi
sudo visudo -cf /etc/sudoers.d/sonos_wifi > /dev/null || { echo "AVISO: sudoers inválido, lo quito"; sudo rm -f /etc/sudoers.d/sonos_wifi; }
# NetworkManager: permite a este usuario cambiar la WiFi aunque el servicio no tenga sesión gráfica
if [ -d /etc/polkit-1/rules.d ]; then
    sudo tee /etc/polkit-1/rules.d/50-sonos-wifi.rules > /dev/null << RULE
polkit.addRule(function(action, subject) {
    if (action.id.indexOf("org.freedesktop.NetworkManager.") == 0 && subject.user == "${REAL_USER}") { return polkit.Result.YES; }
});
RULE
fi
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
