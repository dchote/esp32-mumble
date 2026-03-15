# 0011 – Display Layout Update with Chat Messages

## Summary

Redesigns the ESP32-S3-BOX and ESP32-S3-BOX-3 display layout to show a compact header (title, server address with right-aligned ping, and mic/RX/TX on one line) plus a scrolling chat message area. The Mumble component gains a chat history buffer and HTML stripping so received text messages can be shown on the display as plaintext.

## Implemented

### Display layout (esp32-s3-box.yaml, esp32-s3-box3.yaml)

- **Title**: "ESP32-Mumble" at top (font_title, y=10).
- **Server + Ping row**: Server address left-aligned at x=14; ping right-aligned at x=w-14 on the same line (y=30), using `TextAlign::TOP_RIGHT` for ping.
- **Separator**: Horizontal line at y=48.
- **Status row** (y=54, one line): Mic status left (Mic: MUTED/UNMUTED), RX status centered (`TextAlign::TOP_CENTER` at w/2), TX status right (`TextAlign::TOP_RIGHT` at w-14). Colors unchanged (red when muted, green for RX, blue for TX).
- **Separator**: Horizontal line at y=72.
- **Chat area**: Starts at y=78; shows up to ~9 lines (line_height=16). Each line is `sender: message`; long lines truncated to 34 characters with "...". Only the most recent N messages that fit are shown (oldest of those at top, newest at bottom).

### Component: Chat history and HTML stripping

- **ChatMessage struct** in `mumble_component.h`: `sender` (std::string), `message` (std::string).
- **Chat history**: `std::deque<ChatMessage> chat_history_` with `MAX_CHAT_MESSAGES = 10`. New messages are appended; front is popped when over limit.
- **Getter**: `get_chat_messages() const` returns `const std::deque<ChatMessage> &` for display lambdas.
- **strip_html()** in `mumble_component.cpp`: Removes HTML tags (`<...>`) and decodes `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`.
- **Text message callback**: On each received TextMessage, the component still sets `last_text_message_`, `last_text_sender_name_`, etc., then pushes `ChatMessage{sender_name, strip_html(message)}` onto `chat_history_` and trims the deque. Existing `on_text_message` trigger and getters unchanged.

## Files changed

- `components/mumble/mumble_component.h`: ChatMessage struct, `#include <deque>`, `MAX_CHAT_MESSAGES`, `chat_history_`, `get_chat_messages()`.
- `components/mumble/mumble_component.cpp`: `strip_html()`, callback update to fill and trim `chat_history_`.
- `esphome/esp32-s3-box.yaml`: Display lambda replaced with new layout and chat loop.
- `esphome/esp32-s3-box3.yaml`: Same display lambda and comment.
