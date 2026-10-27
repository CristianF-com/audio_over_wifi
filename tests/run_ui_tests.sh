#!/bin/bash
# Pruebas de interfaz (Playwright). Necesita /tmp/sonos_harness compilado (lo hace run_tests.sh) y playwright global.
cd "$(dirname "$0")"
pkill -x sonos_harness 2>/dev/null; rm -f /tmp/sonos_harness_config.txt
python3 ble_mock_setup.py
SONOS_BLE_HELPER="$PWD/../scripts/sonos_ble_prov.py" SONOS_BLE_MOCK=/tmp/sonos_ble_mock.json /tmp/sonos_harness & HP=$!
python3 feed_nodes.py 5 40 & FP=$!
sleep 1.5
NODE_PATH=$(npm root -g) node test_ui.js "$1" && RC=0 || RC=$?
kill $FP $HP 2>/dev/null; wait $FP $HP 2>/dev/null
exit $RC
