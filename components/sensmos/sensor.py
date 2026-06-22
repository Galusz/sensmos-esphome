import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32, sensor
from esphome.core import CORE

from . import sensmos_ns

CONF_NODE = "node"
CONF_ENTITY = "entity"

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json"]

SensmosGetSensor = sensmos_ns.class_(
    "SensmosGetSensor", sensor.Sensor, cg.PollingComponent
)

_HEX64 = re.compile(r"^[0-9a-f]{64}$")


def _node_id(value):
    value = cv.string_strict(value).strip().lower()
    if not _HEX64.match(value):
        raise cv.Invalid(
            "node must be a 64 hex-char device id (copy it from the node popup on the map)"
        )
    return value


CONFIG_SCHEMA = (
    sensor.sensor_schema(SensmosGetSensor)
    .extend(
        {
            cv.Required(CONF_NODE): _node_id,
            cv.Required(CONF_ENTITY): cv.string,
        }
    )
    .extend(cv.polling_component_schema("5min"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_target(config[CONF_NODE], config[CONF_ENTITY]))

    if CORE.is_esp32:
        esp32.include_builtin_idf_component("esp_http_client")
        esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE", True)
        esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_DYNAMIC_BUFFER", True)
