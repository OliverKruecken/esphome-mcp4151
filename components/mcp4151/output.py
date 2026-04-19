import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, sensor
import esphome.pins as pins
from esphome.const import CONF_ID, CONF_SENSOR

CONF_CS_PIN = "cs_pin"
CONF_SCK_PIN = "sck_pin"
CONF_SDIO_PIN = "sdio_pin"

mcp4151_ns = cg.esphome_ns.namespace("mcp4151")
MCP4151Output = mcp4151_ns.class_("MCP4151Output", output.FloatOutput, cg.PollingComponent)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP4151Output),
            cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_SCK_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_SDIO_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SENSOR): sensor.sensor_schema(
                unit_of_measurement="",
                accuracy_decimals=3,
                state_class=sensor.STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(output.FLOAT_OUTPUT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs_pin))

    sck_pin = await cg.gpio_pin_expression(config[CONF_SCK_PIN])
    cg.add(var.set_sck_pin(sck_pin))

    sdio_pin = await cg.gpio_pin_expression(config[CONF_SDIO_PIN])
    cg.add(var.set_sdio_pin(sdio_pin))

    if CONF_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))