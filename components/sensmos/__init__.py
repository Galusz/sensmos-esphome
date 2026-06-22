import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@Galusz"]
DEPENDENCIES = ["network"]

sensmos_ns = cg.esphome_ns.namespace("sensmos")
SensmosComponent = sensmos_ns.class_("SensmosComponent", cg.PollingComponent)

CONF_KEY = "key"
CONF_ENTITY = "entity"
CONF_LAT = "lat"
CONF_LON = "lon"
CONF_LABEL = "label"
CONF_SENSORS = "sensors"

SENSOR_MAP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
        cv.Required(CONF_ENTITY): cv.string,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SensmosComponent),
        # passkey = tożsamość noda (device_id = sha256(key)). Min 32 znaki.
        cv.Required(CONF_KEY): cv.All(cv.string, cv.Length(min=32)),
        cv.Optional(CONF_LAT): cv.float_,
        cv.Optional(CONF_LON): cv.float_,
        cv.Optional(CONF_LABEL): cv.string,
        cv.Required(CONF_SENSORS): cv.All(
            cv.ensure_list(SENSOR_MAP_SCHEMA), cv.Length(min=1, max=50)
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_key(config[CONF_KEY]))
    if CONF_LAT in config and CONF_LON in config:
        cg.add(var.set_location(config[CONF_LAT], config[CONF_LON]))
    if CONF_LABEL in config:
        cg.add(var.set_label(config[CONF_LABEL]))

    for item in config[CONF_SENSORS]:
        s = await cg.get_variable(item[CONF_ID])
        cg.add(var.add_sensor(s, item[CONF_ENTITY]))
