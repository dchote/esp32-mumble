"""ESPHome Mumble client component for ESP32-S3.

Configuration: server, port, username, password, channel, mode, crypto (inline).
Optional HA-editable entities: server_text_id, port_text_id, username_text_id,
password_text_id, channel_text_id, mode_select_id, crypto_select_id (values persisted to NVS).
Optional hardware: ptt_pin (press-and-hold PTT), mute_pin.
"""

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome import pins
from esphome.components import microphone, select, speaker, text
from esphome.const import CONF_ID, CONF_PORT, CONF_PASSWORD, CONF_CHANNEL

CODEOWNERS = ["@danielhoward314"]
DEPENDENCIES = ["network"]
AUTO_LOAD = []

CONF_SERVER = "server"
CONF_USERNAME = "username"
CONF_MODE = "mode"
CONF_PTT_PIN = "ptt_pin"
CONF_MUTE_PIN = "mute_pin"
CONF_CRYPTO = "crypto"
CONF_CA_CERT = "ca_cert"
CONF_SERVER_TEXT = "server_text_id"
CONF_PORT_TEXT = "port_text_id"
CONF_USERNAME_TEXT = "username_text_id"
CONF_PASSWORD_TEXT = "password_text_id"
CONF_CHANNEL_TEXT = "channel_text_id"
CONF_MODE_SELECT = "mode_select_id"
CONF_CRYPTO_SELECT = "crypto_select_id"
CONF_MICROPHONE = "microphone_id"
CONF_SPEAKER = "speaker_id"

CONF_ALWAYS_ON = "always_on"
CONF_PUSH_TO_TALK = "push_to_talk"
CONF_LITE = "lite"
CONF_LEGACY = "legacy"

mumble_ns = cg.esphome_ns.namespace("mumble")
MumbleComponent = mumble_ns.class_("MumbleComponent", cg.Component)
MumbleChannelSelect = mumble_ns.class_("MumbleChannelSelect", select.Select, cg.Component)

MUMBLE_MODE = {
    CONF_ALWAYS_ON: 0,
    CONF_PUSH_TO_TALK: 1,
}

MUMBLE_CRYPTO = {
    CONF_LITE: 0,
    CONF_LEGACY: 1,
}

def _validate_connection_config(config):
    has_server = (config.get(CONF_SERVER, "") or "").strip() or CONF_SERVER_TEXT in config
    has_username = (config.get(CONF_USERNAME, "") or "").strip() or CONF_USERNAME_TEXT in config
    if not has_server:
        raise cv.Invalid("At least one of 'server' or 'server_text_id' is required")
    if not has_username:
        raise cv.Invalid("At least one of 'username' or 'username_text_id' is required")
    return config


CONF_CHANNEL_SELECT_ID = "channel_select_id"
CONF_CHANNEL_SELECT = "channel_select"

CHANNEL_SELECT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CHANNEL_SELECT_ID): cv.declare_id(MumbleChannelSelect),
        cv.Optional("name", default="5. Channel"): cv.string,
        cv.Optional("icon", default="mdi:forum"): cv.icon,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MumbleComponent),
            cv.Optional(CONF_CHANNEL_SELECT): CHANNEL_SELECT_SCHEMA,
            cv.Optional(CONF_SERVER, default=""): cv.string,
            cv.Optional(CONF_PORT, default=64738): cv.port,
            cv.Optional(CONF_USERNAME, default=""): cv.string,
            cv.Optional(CONF_PASSWORD, default=""): cv.string,
            cv.Optional(CONF_CHANNEL, default=""): cv.string,
            cv.Optional(CONF_SERVER_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_PORT_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_USERNAME_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_PASSWORD_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_CHANNEL_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_MODE_SELECT): cv.use_id(select.Select),
            cv.Optional(CONF_CRYPTO_SELECT): cv.use_id(select.Select),
            cv.Optional(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
            cv.Optional(CONF_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(CONF_MODE, default=CONF_ALWAYS_ON): cv.enum(
                MUMBLE_MODE, lower=True
            ),
            cv.Optional(CONF_PTT_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_MUTE_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_CRYPTO, default=CONF_LEGACY): cv.enum(
                MUMBLE_CRYPTO, lower=True
            ),
            cv.Optional(CONF_CA_CERT, default=""): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_connection_config,
)


async def to_code(config):
    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("WiFi", None)
        cg.add_library("NetworkClientSecure", None)
    if CORE.is_esp32:
        import os
        lib_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "lib", "micro-opus"))
        cg.add_library("micro-opus", None, "symlink://" + lib_path)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_mode(MUMBLE_MODE[config[CONF_MODE]]))
    cg.add(var.set_crypto(MUMBLE_CRYPTO[config[CONF_CRYPTO]]))
    cg.add(var.set_ca_cert(config.get(CONF_CA_CERT, "")))

    if CONF_SERVER_TEXT in config:
        server_text = await cg.get_variable(config[CONF_SERVER_TEXT])
        cg.add(var.set_server_text(server_text))
    if CONF_PORT_TEXT in config:
        port_text = await cg.get_variable(config[CONF_PORT_TEXT])
        cg.add(var.set_port_text(port_text))
    if CONF_USERNAME_TEXT in config:
        username_text = await cg.get_variable(config[CONF_USERNAME_TEXT])
        cg.add(var.set_username_text(username_text))
    if CONF_PASSWORD_TEXT in config:
        password_text = await cg.get_variable(config[CONF_PASSWORD_TEXT])
        cg.add(var.set_password_text(password_text))
    if CONF_CHANNEL_TEXT in config:
        channel_text = await cg.get_variable(config[CONF_CHANNEL_TEXT])
        cg.add(var.set_channel_text(channel_text))

    if CONF_CHANNEL_SELECT in config:
        chan_conf = config[CONF_CHANNEL_SELECT]
        chan_sel = cg.new_Pvariable(chan_conf[CONF_CHANNEL_SELECT_ID])
        await cg.register_component(chan_sel, config)
        cg.add(cg.App.register_select(chan_sel))
        cg.add(chan_sel.set_parent(var))
        cg.add(chan_sel.set_name(chan_conf.get("name", "5. Channel")))
        cg.add(chan_sel.set_entity_category(cg.EntityCategory.ENTITY_CATEGORY_CONFIG))
        cg.add(chan_sel.set_icon(chan_conf.get("icon", "mdi:chat")))
        cg.add(var.set_channel_select(chan_sel))
    if CONF_MODE_SELECT in config:
        mode_select = await cg.get_variable(config[CONF_MODE_SELECT])
        cg.add(var.set_mode_select(mode_select))
    if CONF_CRYPTO_SELECT in config:
        crypto_select = await cg.get_variable(config[CONF_CRYPTO_SELECT])
        cg.add(var.set_crypto_select(crypto_select))
    if CONF_MICROPHONE in config:
        mic_var = await cg.get_variable(config[CONF_MICROPHONE])
        cg.add(var.set_microphone(mic_var))
    if CONF_SPEAKER in config:
        speaker_var = await cg.get_variable(config[CONF_SPEAKER])
        cg.add(var.set_speaker(speaker_var))

    if CONF_PTT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_PTT_PIN])
        cg.add(var.set_ptt_pin(pin))
    if CONF_MUTE_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_MUTE_PIN])
        cg.add(var.set_mute_pin(pin))


MUMBLE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MumbleComponent),
    }
)

MumbleMicrophoneEnableAction = mumble_ns.class_(
    "MumbleMicrophoneEnableAction", automation.Action
)
MumbleMicrophoneDisableAction = mumble_ns.class_(
    "MumbleMicrophoneDisableAction", automation.Action
)
MumblePttPressAction = mumble_ns.class_(
    "MumblePttPressAction", automation.Action
)
MumbleResetConfigAction = mumble_ns.class_(
    "MumbleResetConfigAction", automation.Action
)


@automation.register_action(
    "mumble.microphone_enable",
    MumbleMicrophoneEnableAction,
    MUMBLE_ACTION_SCHEMA,
)
async def mumble_microphone_enable_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, var)


@automation.register_action(
    "mumble.microphone_disable",
    MumbleMicrophoneDisableAction,
    MUMBLE_ACTION_SCHEMA,
)
async def mumble_microphone_disable_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, var)


@automation.register_action(
    "mumble.ptt_press",
    MumblePttPressAction,
    MUMBLE_ACTION_SCHEMA,
)
async def mumble_ptt_press_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, var)


@automation.register_action(
    "mumble.reset_config",
    MumbleResetConfigAction,
    MUMBLE_ACTION_SCHEMA,
)
async def mumble_reset_config_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, var)
