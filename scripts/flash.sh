#!/usr/bin/env bash
# Flash ESP32-Mumble firmware
# Usage: ./scripts/flash.sh [generic|box|box3|echo|voice-pe|p4] [--device /dev/ttyUSB0]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ESPHOME_DIR="$PROJECT_ROOT/esphome"

CONFIG="${1:-generic}"
shift || true

case "$CONFIG" in
  generic)
    YAML="generic-esp32s3.yaml"
    ;;
  box)
    YAML="esp32-s3-box.yaml"
    ;;
  box3)
    YAML="esp32-s3-box3.yaml"
    ;;
  echo)
    YAML="m5stack-atom-echo.yaml"
    ;;
  voice-pe)
    YAML="home-assistant-voice-pe.yaml"
    ;;
  p4)
    YAML="waveshare-esp32-p4-poe.yaml"
    ;;
  *)
    echo "Usage: $0 [generic|box|box3|echo|voice-pe|p4] [--device /dev/ttyUSB0]"
    echo "  generic  - Flash generic ESP32-S3 config (default)"
    echo "  box      - Flash ESP32-S3 Box config (ESP-IDF)"
    echo "  box3     - Flash ESP32-S3 Box 3 config (ESP-IDF)"
    echo "  echo     - Flash M5Stack Atom Echo config (Arduino)"
    echo "  voice-pe - Flash Home Assistant Voice PE config (ESP-IDF)"
    echo "  p4       - Flash Waveshare ESP32-P4 config (native esp-idf, Ethernet)"
    exit 1
    ;;
esac

echo "Flashing $YAML..."
esphome run "$ESPHOME_DIR/$YAML" "$@"
