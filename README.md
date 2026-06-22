<img src="logo.png" alt="Sensmos" height="80">

# Sensmos — ESPHome

**Publish any ESPHome sensor to the [Sensmos](https://sensmos.com) live map — no wallet, no signup, no firmware to write. Add a few lines, pick a passkey, done.**

> Your device shows up as a **software node** (purple) on [sensmos.com/map](https://sensmos.com/map/): live values, history, charts and an optional heatmap layer. It never touches the GALU economy — software nodes are data + map only.

## How it works

You add a `sensmos:` block with a **passkey** and a list of `sensor id → entity name` mappings. Every `update_interval` the component reads those sensors and POSTs them to the Sensmos ingest endpoint over HTTPS. The first push auto-registers your node (identity = `sha256(key)`), later pushes overwrite. Location comes from the optional `lat`/`lon`, or is resolved from your IP (GeoIP).

## Install

```yaml
external_components:
  - source: github://Galusz/sensmos-esphome@main
```

## Configure

```yaml
sensmos:
  key: !secret sensmos_key      # passkey, min 32 chars — this IS your node identity
  update_interval: 60s          # min 20s (the server rate-limits faster pushes)
  # insecure: true              # plain HTTP, no TLS — for low-RAM nodes (e.g. heavy BLE) where HTTPS fails
  # lat/lon optional — without them the server uses GeoIP
  # lat: 52.2297
  # lon: 21.0122
  # label: "My battery"
  sensors:
    - id: my_soc_sensor         # an existing ESPHome sensor's id
      entity: pub.batt_soc      # the Sensmos entity name
    - id: my_voltage_sensor
      entity: pub.batt_v
```

Generate a passkey (treat it like a password):

```bash
openssl rand -hex 24      # 48 hex chars
```

### Entity names

Pick **`pub.<native>`** to get a map category + heatmap, or **`own.<anything>`** for free-form data (shown on the node, no heatmap, e.g. `own.boiler_temp`).

Full native list:

| Category | Entities |
|----------|----------|
| ⚡ **PWR** (power) | `batt_soc` `batt_v` `batt_power` `load_power` `pv_power` `pv_daily` `grid_v` `grid_freq` `grid_import` `grid_export` `ev_charge` |
| 🌿 **ENV** (environment) | `temp_out` `humidity_out` `pressure` `co2` `pm25` `pm10` `noise_db` `uv_index` `rain_mm` |
| 🏠 **LIF** (home) | `temp_in` `humidity_in` `motion` `presence` `door` `window` `steps` `sleep_score` `uptime_s` `wifi_nets` `wifi_rssi` |

## Examples

### Air quality (smog) — PMS5003

Maps to native `pub.pm25` / `pub.pm10`, so it joins the air-quality heatmap on the map.

```yaml
external_components:
  - source: github://Galusz/sensmos-esphome@main

uart:
  rx_pin: GPIO16
  tx_pin: GPIO17
  baud_rate: 9600

sensor:
  - platform: pmsx003
    type: PMSX003
    pm_2_5:
      name: "PM2.5"
      id: pm25
    pm_10_0:
      name: "PM10"
      id: pm10

sensmos:
  key: !secret sensmos_key
  sensors:
    - id: pm25
      entity: pub.pm25
    - id: pm10
      entity: pub.pm10
```

### Minimal — no extra hardware

Just the ESP's own uptime + WiFi signal, to see a node appear on the map in a minute.

```yaml
external_components:
  - source: github://Galusz/sensmos-esphome@main

sensor:
  - platform: uptime
    name: "Uptime"
    id: dev_uptime
  - platform: wifi_signal
    name: "WiFi signal"
    id: dev_rssi

sensmos:
  key: !secret sensmos_key
  label: "My ESP"
  sensors:
    - id: dev_uptime
      entity: pub.uptime_s
    - id: dev_rssi
      entity: pub.wifi_rssi
```

And in `secrets.yaml`:

```yaml
sensmos_key: "paste-your-48-char-passkey-here-xxxxxxxxxxxx"
```

> Got a battery/BMS, solar inverter, weather station…? Same pattern — give the sensors an `id:` and map them to `pub.batt_soc`, `pub.pv_power`, `pub.temp_out`, etc.

## Read another node's data (preview)

The other direction: pull a value another node publishes and expose it as a normal ESPHome sensor — show a neighbour's air quality on a local display, drive an automation, whatever. It's a **preview, not realtime**: it polls the public map data on a long interval (default 5 min). No wallet, no subscription.

```yaml
external_components:
  - source: github://Galusz/sensmos-esphome@main

sensor:
  - platform: sensmos
    name: "Neighbour PM2.5"
    node: "0123…64-hex…cdef"   # the target node's device id (map → node popup → copy)
    entity: pub.pm25            # which published entity to read
    update_interval: 5min       # how often to poll (keep it long — it's a preview)
```

Each `- platform: sensmos` entry = one remote entity → one ESPHome sensor. Add more entries (same or different `node`) for more values. Works for any node, real or software. The fetch runs off the main loop, so it won't stall BLE.

## Notes

- **Frameworks:** works on both `esp-idf` (uses the cert bundle) and `arduino`.
- **Timeout / pruning:** a software node goes offline after 48 h without a push and reappears automatically on the next one. Online/offline status is shown on the map.
- **Up to 50 entities** per node.
- **Non-blocking:** both publish (POST) and read (GET) run the HTTPS call in a separate FreeRTOS task, so they don't stall the main loop or BLE. Still, keep intervals sane (publish ≥ 60 s, read ≥ a few min).
- **Low-RAM nodes:** on a memory-tight node (e.g. a big BLE stack like a BMS), the HTTPS handshake can fail with mbedTLS alloc errors (it must buffer the server cert). Add **`insecure: true`** to the `sensmos:` block and/or to a `- platform: sensmos` reader → it uses plain **HTTP** (no TLS, almost no RAM). It's telemetry and software nodes have no economy, so the risk is low.

## Part of the Sensmos project

| | |
|---|---|
| 🌐 Website | https://sensmos.com |
| 🗺️ Live map | https://sensmos.com/map/ |
| 📱 App | https://github.com/Galusz/sensmos-app |
| 🔌 Firmware | https://github.com/Galusz/sensmos-firmware |
| 🏠 Home Assistant | https://github.com/Galusz/sensmos-homeassistant |
| 📜 Protocol | https://github.com/Galusz/sensmos-protocol |
| 💬 Discord | https://discord.gg/ukea386Kqx |

GALU runs on Polygon. © 2026 Sensmos.
