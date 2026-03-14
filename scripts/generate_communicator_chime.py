"""Extract PCM from communicator WAV and generate C header.

Run manually: python scripts/generate_communicator_chime.py
Output: components/mumble/communicator_chime_data.h (commit to repo)

The header defines communicator_chime_data[] and COMMUNICATOR_CHIME_SIZE for the
Mumble component's bus-aware chime player. Expects 16-bit mono WAV.
"""
import os
import struct

# Project root = parent of scripts/
PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
# WAV relative to project root (esphome/sounds/communicator.wav)
WAV_PATH = os.path.join(PROJECT_DIR, "esphome", "sounds", "communicator.wav")
OUTPUT_PATH = os.path.join(PROJECT_DIR, "components", "mumble", "communicator_chime_data.h")


def parse_wav(path):
    """Extract raw PCM (16-bit mono) from WAV. Returns (bytes, None) or (None, err)."""
    if not os.path.exists(path):
        return None, f"File not found: {path}"
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 44:
        return None, "WAV too small"
    # RIFF header
    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        return None, "Invalid WAV (not RIFF WAVE)"
    # Find data chunk (skip fmt and other chunks)
    pos = 12
    pcm = None
    while pos + 8 <= len(data):
        chunk_id = data[pos : pos + 4]
        chunk_size = struct.unpack_from("<I", data, pos + 4)[0]
        pos += 8
        if pos + chunk_size > len(data):
            break
        if chunk_id == b"data":
            pcm = data[pos : pos + chunk_size]
            break
        pos += chunk_size
    if pcm is None:
        return None, "No data chunk in WAV"
    if len(pcm) % 2 != 0:
        return None, "Odd data length"
    return pcm, None


def main():
    pcm, err = parse_wav(WAV_PATH)
    if err:
        # Generate empty stub so build succeeds for configs without the file
        content = """// Auto-generated (WAV not found or invalid)
#define COMMUNICATOR_CHIME_SIZE 0
static const uint8_t communicator_chime_data[] = { 0 };
"""
        with open(OUTPUT_PATH, "w") as f:
            f.write(content)
        print(f"[generate_communicator_chime] WAV error: {err} -> empty stub")
        return
    # Generate C header (const in flash, no PROGMEM needed on ESP32)
    lines = [
        "// Auto-generated from esphome/sounds/communicator.wav (16-bit mono PCM)",
        f"#define COMMUNICATOR_CHIME_SIZE {len(pcm)}",
        "static const uint8_t communicator_chime_data[] = {",
    ]
    for i in range(0, len(pcm), 16):
        chunk = pcm[i : i + 16]
        hexs = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append("  " + hexs + ",")
    lines.append("};")
    with open(OUTPUT_PATH, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"[generate_communicator_chime] OK: {len(pcm)} bytes -> {OUTPUT_PATH}")


main()
