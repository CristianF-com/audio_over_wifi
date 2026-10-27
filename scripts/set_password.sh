#!/bin/bash
# set_password.sh — protege la web/API de la RPi con contraseña (HTTP Basic; el nombre de usuario es libre).
# Uso:  sudo ./scripts/set_password.sh "mi clave"      |      sudo ./scripts/set_password.sh --off
# Úsala con HTTPS (:8443); por HTTP (:8080) la clave viaja sin cifrar por tu LAN.
set -euo pipefail
[ $# -eq 1 ] || { sed -n '2,5p' "$0"; exit 2; }
sudo mkdir -p /etc/sonos_wifi
if [ "$1" = "--off" ]; then
    sudo rm -f /etc/sonos_wifi/password; echo "Contraseña eliminada."
else
    [ ${#1} -ge 6 ] || { echo "ERROR: mínimo 6 caracteres"; exit 2; }
    printf '%s\n' "$1" | sudo tee /etc/sonos_wifi/password >/dev/null
    sudo chmod 600 /etc/sonos_wifi/password
    echo "Contraseña guardada (el servicio corre como tu usuario: se lee con permisos del servicio)."
    # el servicio lee el fichero como su usuario
    U="$(systemctl show -p User --value sonos_wifi 2>/dev/null || true)"; [ -n "$U" ] && sudo chown "$U" /etc/sonos_wifi/password || true
fi
sudo systemctl restart sonos_wifi
echo "Servicio reiniciado."
