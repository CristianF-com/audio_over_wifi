#!/bin/bash
# update.sh — lo lanza systemd (sonos_wifi_update.service, como root) cuando se pulsa «Actualizar servidor» en la web.
# build.sh lo instala en /usr/local/lib/sonos_wifi/update.sh sustituyendo @REPO@ y @USER@.
# Hace: git pull --ff-only → compilar → instalar → reiniciar; si la versión nueva no arranca, vuelve a la anterior.
# Estado en /var/lib/sonos_wifi/update.state (running|ok|failed) y registro en update.log (la web los muestra).
set -u
REPO="@REPO@"; RUSER="@USER@"; STATE=/var/lib/sonos_wifi
mkdir -p "$STATE"; : > "$STATE/update.log"; echo running > "$STATE/update.state"
exec >> "$STATE/update.log" 2>&1
fail() { echo "ERROR: $1"; echo failed > "$STATE/update.state"; exit 1; }
as_user() { sudo -u "$RUSER" "$@"; }

echo "== Actualizando SonosWiFi ($(date '+%F %T')) =="
cd "$REPO" || fail "no existe la carpeta $REPO"
OLD="$(as_user git rev-parse --short HEAD)"
echo "Versión actual: $OLD"
as_user git pull --ff-only || fail "git pull falló (¿sin internet o con cambios locales en $REPO?)"
NEW="$(as_user git rev-parse --short HEAD)"
if [ "$OLD" = "$NEW" ] && [ -x /usr/local/bin/sonos_wifi ]; then
    echo "Ya estaba al día ($NEW). No se reinicia el servicio."; echo ok > "$STATE/update.state"; exit 0
fi
echo "Nueva versión: $NEW. Compilando (puede tardar unos minutos)…"
as_user bash -c 'mkdir -p rpi/build && cd rpi/build && cmake .. -DCMAKE_BUILD_TYPE=Release >/dev/null && make -j"$(nproc)"' \
    || fail "falló la compilación; se mantiene la versión instalada ($OLD)"
[ -x /usr/local/bin/sonos_wifi ] && cp -f /usr/local/bin/sonos_wifi /usr/local/bin/sonos_wifi.prev
install -m 755 rpi/build/sonos_wifi /usr/local/bin/sonos_wifi || fail "no se pudo instalar el binario"
install -d /usr/local/lib/sonos_wifi
install -m 755 scripts/sonos_ble_prov.py /usr/local/lib/sonos_wifi/sonos_ble_prov.py
# la propia actualización también se actualiza (el script en ejecución sigue usando su copia antigua)
sed "s|@REPO@|$REPO|; s|@USER@|$RUSER|" scripts/update.sh > "$STATE/update.sh.new" \
    && install -m 755 "$STATE/update.sh.new" /usr/local/lib/sonos_wifi/update.sh; rm -f "$STATE/update.sh.new"
echo "Reiniciando el servicio… (la web se corta unos segundos)"
systemctl restart sonos_wifi
sleep 8
if systemctl is-active --quiet sonos_wifi; then
    echo "Listo: versión $NEW en marcha."; echo ok > "$STATE/update.state"
else
    echo "El servicio no arrancó con la versión nueva: se vuelve a la anterior."
    if [ -x /usr/local/bin/sonos_wifi.prev ]; then
        install -m 755 /usr/local/bin/sonos_wifi.prev /usr/local/bin/sonos_wifi; systemctl restart sonos_wifi
    fi
    fail "la versión $NEW no arrancó; se restauró la anterior ($OLD). Revisa: journalctl -u sonos_wifi"
fi
