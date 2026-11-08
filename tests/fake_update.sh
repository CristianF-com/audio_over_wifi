#!/bin/sh
# Servicio de actualización de pruebas: deja update.state/update.log como lo haría update.sh y vuelve enseguida.
D="${SONOS_UPDATE_DIR:-/tmp/sonos_upd}"; mkdir -p "$D"
echo running > "$D/update.state"; echo "Descargando cambios…" > "$D/update.log"
( sleep 1; echo "Compilando…" >> "$D/update.log"; echo "Listo: versión de prueba" >> "$D/update.log"; echo ok > "$D/update.state" ) >/dev/null 2>&1 &
exit 0
