#!/usr/bin/env python3
"""Generate the ESP Web Tools installer index.html for GitHub Pages."""

from __future__ import annotations

import argparse
from pathlib import Path

BOARDS = [
    (
        "ESP32-S3-BOX",
        "esp32-s3-box",
        "Original Box (ESP-IDF). First flash after a framework switch must be via USB.",
    ),
    (
        "ESP32-S3-BOX-3",
        "esp32-s3-box3",
        "Box-3 (ESP-IDF). First flash after a framework switch must be via USB.",
    ),
    (
        "Home Assistant Voice PE",
        "home-assistant-voice-pe",
        "Voice Preview Edition (ESP-IDF) with LED ring, mute slider, and jog dial.",
    ),
    (
        "M5Stack Atom Echo",
        "m5stack-atom-echo",
        "Compact Atom Echo (Arduino framework).",
    ),
    (
        "Generic ESP32-S3",
        "generic-esp32s3",
        "Placeholder pinout for custom boards (Arduino framework). Review pins before flashing.",
    ),
]


def render(version: str, release_url: str) -> str:
    cards = []
    for title, slug, desc in BOARDS:
        cards.append(
            f"""    <div class="card">
      <h2>{title}</h2>
      <p>{desc}</p>
      <esp-web-install-button manifest="{slug}/manifest.json"></esp-web-install-button>
    </div>"""
        )
    cards_html = "\n".join(cards)
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32-Mumble Firmware Installer</title>
  <style>
    :root {{ color-scheme: light dark; }}
    body {{
      font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
      max-width: 720px;
      margin: 2rem auto;
      padding: 0 1rem;
      line-height: 1.5;
    }}
    h1 {{ margin-bottom: 0.25rem; }}
    .muted {{ opacity: 0.75; margin-top: 0; }}
    .card {{
      border: 1px solid color-mix(in srgb, currentColor 25%, transparent);
      border-radius: 12px;
      padding: 1rem 1.25rem;
      margin: 1rem 0;
    }}
    .card h2 {{ margin: 0 0 0.5rem; font-size: 1.1rem; }}
    .card p {{ margin: 0.35rem 0 0.75rem; opacity: 0.85; font-size: 0.95rem; }}
    code {{ font-size: 0.9em; }}
  </style>
</head>
<body>
  <h1>ESP32-Mumble</h1>
  <p class="muted">Install or update firmware over USB with ESP Web Tools. Device OTA updates use the same manifests from Home Assistant.</p>
  <p class="muted">Release: <strong>{version}</strong> · <a href="{release_url}">GitHub Release</a></p>
{cards_html}

  <p class="muted">Manifests and OTA binaries are published under <code>https://dchote.github.io/esp32-mumble/&lt;board&gt;/</code>.</p>
  <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
</body>
</html>
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--release-url", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.version, args.release_url), encoding="utf-8")
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
