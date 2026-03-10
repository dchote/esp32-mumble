"""ESPHome Mumble client component for ESP32-S3."""

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import number, text
from esphome.const import CONF_ID, CONF_PORT, CONF_PASSWORD, CONF_CHANNEL

CODEOWNERS = ["@esphome"]
DEPENDENCIES = ["network"]
AUTO_LOAD = []

CONF_SERVER = "server"
CONF_USERNAME = "username"
CONF_MODE = "mode"
CONF_PTT_PIN = "ptt_pin"
CONF_MUTE_PIN = "mute_pin"
CONF_CRYPTO = "crypto"
CONF_SERVER_TEXT = "server_text_id"
CONF_PORT_NUMBER = "port_number_id"
CONF_USERNAME_TEXT = "username_text_id"
CONF_PASSWORD_TEXT = "password_text_id"
CONF_CHANNEL_TEXT = "channel_text_id"

CONF_ALWAYS_ON = "always_on"
CONF_PUSH_TO_TALK = "push_to_talk"
CONF_LITE = "lite"
CONF_LEGACY = "legacy"

mumble_ns = cg.esphome_ns.namespace("mumble")
MumbleComponent = mumble_ns.class_("MumbleComponent", cg.Component)
MumblePttPressAction = mumble_ns.class_(
    "MumblePttPressAction", automation.Action
)

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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MumbleComponent),
            cv.Optional(CONF_SERVER, default=""): cv.string,
            cv.Optional(CONF_PORT, default=64738): cv.port,
            cv.Optional(CONF_USERNAME, default=""): cv.string,
            cv.Optional(CONF_PASSWORD, default=""): cv.string,
            cv.Optional(CONF_CHANNEL, default=""): cv.string,
            cv.Optional(CONF_SERVER_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_PORT_NUMBER): cv.use_id(number.Number),
            cv.Optional(CONF_USERNAME_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_PASSWORD_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_CHANNEL_TEXT): cv.use_id(text.Text),
            cv.Optional(CONF_MODE, default=CONF_ALWAYS_ON): cv.enum(
                MUMBLE_MODE, lower=True
            ),
            cv.Optional(CONF_PTT_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_MUTE_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_CRYPTO, default=CONF_LITE): cv.enum(
                MUMBLE_CRYPTO, lower=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_connection_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_mode(MUMBLE_MODE[config[CONF_MODE]]))
    cg.add(var.set_crypto(MUMBLE_CRYPTO[config[CONF_CRYPTO]]))

    if CONF_SERVER_TEXT in config:
        server_text = await cg.get_variable(config[CONF_SERVER_TEXT])
        cg.add(var.set_server_text(server_text))
    if CONF_PORT_NUMBER in config:
        port_number = await cg.get_variable(config[CONF_PORT_NUMBER])
        cg.add(var.set_port_number(port_number))
    if CONF_USERNAME_TEXT in config:
        username_text = await cg.get_variable(config[CONF_USERNAME_TEXT])
        cg.add(var.set_username_text(username_text))
    if CONF_PASSWORD_TEXT in config:
        password_text = await cg.get_variable(config[CONF_PASSWORD_TEXT])
        cg.add(var.set_password_text(password_text))
    if CONF_CHANNEL_TEXT in config:
        channel_text = await cg.get_variable(config[CONF_CHANNEL_TEXT])
        cg.add(var.set_channel_text(channel_text))

    if CONF_PTT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_PTT_PIN])
        cg.add(var.set_ptt_pin(pin))
    if CONF_MUTE_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_MUTE_PIN])
        cg.add(var.set_mute_pin(pin))


MUMBLE_PTT_PRESS_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MumbleComponent),
    }
)


@automation.register_action(
    "mumble.ptt_press",
    MumblePttPressAction,
    MUMBLE_PTT_PRESS_ACTION_SCHEMA,
    synchronous=True,
)
async def mumble_ptt_press_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, var)
