#pragma once

#include <cstdint>

namespace esphome {
namespace mumble {

// Mumble permission bitmask (matches go-mumble-server pkg/mumble/permission.go)
constexpr uint32_t PERMISSION_NONE = 0;
constexpr uint32_t PERMISSION_WRITE = 0x01;
constexpr uint32_t PERMISSION_TRAVERSE = 0x02;
constexpr uint32_t PERMISSION_ENTER = 0x04;
constexpr uint32_t PERMISSION_SPEAK = 0x08;
constexpr uint32_t PERMISSION_MUTE_DEAFEN = 0x10;
constexpr uint32_t PERMISSION_MOVE = 0x20;
constexpr uint32_t PERMISSION_MAKE_CHANNEL = 0x40;
constexpr uint32_t PERMISSION_LINK_CHANNEL = 0x80;
constexpr uint32_t PERMISSION_WHISPER = 0x100;
constexpr uint32_t PERMISSION_TEXT_MESSAGE = 0x200;
constexpr uint32_t PERMISSION_MAKE_TEMP_CHANNEL = 0x400;
constexpr uint32_t PERMISSION_LISTEN = 0x800;
constexpr uint32_t PERMISSION_KICK = 0x10000;
constexpr uint32_t PERMISSION_BAN = 0x20000;
constexpr uint32_t PERMISSION_REGISTER = 0x40000;
constexpr uint32_t PERMISSION_SELF_REGISTER = 0x80000;
constexpr uint32_t PERMISSION_RESET_USER = 0x100000;
constexpr uint32_t PERMISSION_CACHED = 0x8000000;
constexpr uint32_t PERMISSION_ALL = 0xF07FF;

}  // namespace mumble
}  // namespace esphome
