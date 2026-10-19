#!/usr/bin/env bash
# =============================================================================
#  serve.sh — Phuc vu giao dien tai http://localhost:8080
#
#  VI SAO PHAI CHAY QUA SERVER, KHONG MO THANG FILE .html?
#
#  Hai chuc nang cua trang doi hoi "ngu canh bao mat" (secure context):
#     · Web Serial  — mo cong USB toi thiet bi
#     · getUserMedia — bat camera de quet QR CCCD
#  Chrome CHAN camera tren file:// va cham module ES cung bi chan boi CORS.
#  http://localhost duoc coi la bao mat, nen chi can mot server tinh la du.
#
#  Day KHONG phai backend: khong co logic nao chay o day, chi la phat tep tinh.
#  Toan bo giao dien van chay trong trinh duyet.
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"

PORT="${1:-8080}"
URL="http://localhost:${PORT}"

echo "SATx — giao dien cuc bo"
echo "  ${URL}"
echo
echo "  Dung Chrome hoac Edge (Web Serial khong co tren Safari/Firefox)."
echo "  Chua co mach? Mo Cai dat -> Nguon du lieu -> Mo phong."
echo
echo "  Ctrl+C de dung."
echo

# Mo trinh duyet neu co the, roi moi chay server o tien canh.
if command -v open >/dev/null 2>&1; then (sleep 1 && open "$URL") & fi

if command -v python3 >/dev/null 2>&1; then
  exec python3 -m http.server "$PORT" --bind 127.0.0.1
elif command -v npx >/dev/null 2>&1; then
  exec npx --yes serve -l "$PORT" .
else
  echo "Can python3 hoac Node.js de chay server tinh." >&2
  exit 1
fi
