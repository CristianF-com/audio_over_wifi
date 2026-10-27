#!/bin/bash
# Prueba de integración del lado RPi (necesita g++ y libasound2-dev; no necesita hardware)
set -e
cd "$(dirname "$0")"
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj "/CN=test" \
    -keyout /tmp/sonos_test_key.pem -out /tmp/sonos_test_cert.pem 2>/dev/null
g++ -std=c++17 -O1 -Wall -Wextra -DWITH_TLS -I../rpi/include -I../common harness.cpp \
    ../rpi/src/udp_streamer.cpp ../rpi/src/web_server.cpp ../rpi/src/ble_manager.cpp ../rpi/src/audio_capture.cpp \
    -lasound -lpthread -lssl -lcrypto -o /tmp/sonos_harness
g++ -std=c++17 -O1 -Wall -Wextra -I../rpi/include -I../common test_config.cpp ../rpi/src/udp_streamer.cpp -lpthread -o /tmp/sonos_test_config
echo "== Firmware (unidades sin ESP-IDF) =="; python3 test_firmware_units.py || exit 1
echo; echo "== Firmware: reordenación + reenvío (reorder.h) =="
gcc -std=c11 -Wall -Wextra -I../common test_reorder.c -o /tmp/sonos_test_reorder && /tmp/sonos_test_reorder || exit 1
echo; echo "== Firmware: tarea RX real del ESP en el PC (loopback, pérdidas, mono) =="
awk '/\/\* ─── Tarea RX audio/{f=1} /\/\* ─── Tarea I2S/{f=0} f' ../esp32/main/receiver_main.c > /tmp/esp_rx_section.inc
gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -I../common -I../esp32/main test_esp_rx.c -lpthread -o /tmp/sonos_test_esp_rx && timeout 30 /tmp/sonos_test_esp_rx || exit 1
echo; echo "== DSP (crossover LR4 + EQ) =="
g++ -std=c++17 -O2 -Wall -Wextra -I../rpi/include test_dsp.cpp -o /tmp/sonos_test_dsp && /tmp/sonos_test_dsp || exit 1
echo; echo "== Persistencia de configuración =="; /tmp/sonos_test_config || exit 1
echo; echo "== Ayudante Bluetooth (bleak simulado) =="; python3 test_ble_helper.py || exit 1
echo; echo "== Autenticación web =="
/tmp/sonos_harness auth & AP=$!
sleep 0.6; python3 test_auth.py || { kill $AP 2>/dev/null; exit 1; }
kill $AP 2>/dev/null || true; wait $AP 2>/dev/null || true
echo; echo "== Protocolo, web, HTTPS, calibración =="
rm -f /tmp/sonos_harness_config.txt
python3 ble_mock_setup.py
SONOS_BLE_HELPER="$PWD/../scripts/sonos_ble_prov.py" SONOS_BLE_MOCK=/tmp/sonos_ble_mock.json /tmp/sonos_harness & HP=$!
sleep 0.5
python3 fake_esp.py && RC=0 || RC=$?
kill $HP 2>/dev/null || true; wait $HP 2>/dev/null || true
if [ $RC -eq 0 ] && command -v node >/dev/null; then
  echo; echo "== Análisis del navegador (Node) =="; node test_analysis.js || RC=1
  echo; echo "== Páginas web: generador + sintaxis JS =="; bash check_pages_js.sh || RC=1
fi
exit $RC
