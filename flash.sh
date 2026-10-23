#!/bin/zsh
set -e
setopt null_glob

source /Users/pes/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1

PORT="$1"
if [ -z "$PORT" ]; then
  ports=(/dev/cu.usbmodem* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*)
  PORT="${ports[1]}"
fi

if [ -z "$PORT" ]; then
  echo "ESP32 не найден. Подключи плату по USB и повтори." >&2
  exit 1
fi

echo "Порт: $PORT"
idf.py -p "$PORT" build flash monitor
