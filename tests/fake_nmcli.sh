#!/bin/sh
# nmcli de pruebas. Imita «dev wifi list», «dev», «con show --active», «dev wifi connect» y «con up».
LOG=/tmp/fake_nmcli.log
case "$*" in
  *"-f DEVICE,TYPE dev"*) printf 'eth0:ethernet\nwlan0:wifi\nlo:loopback\n'; exit 0;;
  *"con show --active"*)  printf 'Cable:802-3-ethernet:eth0\nCasaHogar:802-11-wireless:wlan0\n'; exit 0;;
  *"dev wifi connect"*)
    echo "connect $*" >> "$LOG"
    case "$*" in *"connect Mala "*) echo "Error: Connection activation failed" >&2; exit 4;; esac
    exit 0;;
  *"con up"*) echo "up $*" >> "$LOG"; exit 0;;
esac
cat <<'EOT'
Casa-2G:82:2437 MHz:WPA2
Casa-5G:64:5180 MHz:WPA2 WPA3
Casa-5G:71:5745 MHz:WPA2 WPA3
Casa\:Sala:55:5200 MHz:WPA2
:40:2412 MHz:WPA2
Invitados:30:2462 MHz:--
EOT
