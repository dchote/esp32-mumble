#!/usr/bin/env bash
# Build ESP32-Mumble firmware
# Usage: ./scripts/build.sh [generic|box3|all]
# Default: generic

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ESPHOME_DIR="$PROJECT_ROOT/esphome"

CONFIG="${1:-generic}"

build_one() {
  local name="$1"
  local yaml="$2"
  echo "Building $name..."
  esphome compile "$ESPHOME_DIR/$yaml"
}

case "$CONFIG" in
  generic)
    build_one "generic-esp32s3" "generic-esp32s3.yaml"
    ;;
  box3)
    build_one "esp32-s3-box3" "esp32-s3-box3.yaml"
    ;;
  all)
    build_one "generic-esp32s3" "generic-esp32s3.yaml"
    build_one "esp32-s3-box3" "esp32-s3-box3.yaml"
    ;;
  *)
    echo "Usage: $0 [generic|box3|all]"
    echo "  generic - Build generic ESP32-S3 config (default)"
    echo "  box3    - Build ESP32-S3 Box 3 config"
    echo "  all     - Build all configs"
    exit 1
    ;;
esac

echo "Build complete."
