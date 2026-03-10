#!/usr/bin/env bash
# Flash ESP32-Mumble firmware
# Usage: ./scripts/flash.sh [generic|box3] [--device /dev/ttyUSB0]

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
  box3)
    YAML="esp32-s3-box3.yaml"
    ;;
  *)
    echo "Usage: $0 [generic|box3] [--device /dev/ttyUSB0]"
    echo "  generic - Flash generic ESP32-S3 config (default)"
    echo "  box3    - Flash ESP32-S3 Box 3 config"
    exit 1
    ;;
esac

echo "Flashing $YAML..."
esphome run "$ESPHOME_DIR/$YAML" "$@"
