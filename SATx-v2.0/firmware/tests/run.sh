#!/usr/bin/env bash
# Chay test logic tren may tinh — khong can board, khong can cam bien.
#
#   ./run.sh
#
# Chay nguyen SATx.ino voi HAL gia lap: EEPROM/CRC, JSON-lines va chu trinh
# do, chuyen kenh, dong ho, NVS. Kiem ca nhanh doc cam bien va nhanh mo phong.
set -euo pipefail
cd "$(dirname "$0")"

OUT="build"
mkdir -p "$OUT"

# ArduinoJson: tim trong thu muc thu vien cua Arduino IDE / arduino-cli.
JSON_INC=""
for candidate in \
    "$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
    "$HOME/Arduino/libraries/ArduinoJson/src"; do
  [[ -d "$candidate" ]] && JSON_INC="$candidate" && break
done
if [[ -z "$JSON_INC" ]]; then
  echo "Khong tim thay ArduinoJson. Cai bang:  arduino-cli lib install ArduinoJson"
  exit 1
fi

FLAGS=(-std=c++17 -Wall -Wextra -Wno-unused-parameter -I stub -I "$JSON_INC")

echo "--- Bien dich ---"
g++ "${FLAGS[@]}" -o "$OUT/test_recordstore" test_recordstore.cpp
g++ "${FLAGS[@]}" -o "$OUT/test_protocol" test_protocol.cpp
g++ "${FLAGS[@]}" -o "$OUT/test_workflow" test_workflow.cpp
g++ "${FLAGS[@]}" -DSATX_SIMULATE_SENSORS=1 -o "$OUT/test_workflow_simulated" test_workflow.cpp

echo
"$OUT/test_recordstore"
echo
"$OUT/test_protocol"
echo
"$OUT/test_workflow"
"$OUT/test_workflow_simulated"
