# esphome-sensmos

Publish any ESPHome sensor to the **[Sensmos](https://sensmos.com)** live map — no wallet, no signup, no firmware flashing. Add a few lines, pick a passkey, done.

Your device shows up as a **software node** (purple) on [sensmos.com/map](https://sensmos.com/map/): values, history, charts and an optional heatmap layer.

## How it works

You add a `sensmos:` block with a **passkey** and a list of `sensor id → entity name` mappings. Every `update_interval` the component reads those sensors and POSTs them to the Sensmos ingest endpoint over HTTPS. The first push auto-registers your node (identity = `sha256(key)`), later pushes overwrite. Location comes from the optional `lat`/`lon` or is resolved from your IP (GeoIP).

It never touches the GALU economy — software nodes are data + map only.

## Install

```yaml
external_components:
  - source: github://Galusz/esphome-sensmos@main
```

## Configure

```yaml
sensmos:
  key: !secret sensmos_key      # passkey, min 32 chars — this IS your node identity
  update_interval: 60s          # min 20s (the server rate-limits faster pushes)
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

- **`pub.<native>`** — a name from the Sensmos native list → categorized (PWR/ENV/LIF) and eligible for the heatmap.
  PWR: `batt_soc batt_v batt_power load_power pv_power pv_daily grid_v grid_freq grid_import grid_export ev_charge`
  ENV: `temp_out humidity_out pressure co2 pm25 pm10 noise_db uv_index rain_mm`
  LIF: `temp_in humidity_in motion presence door window steps sleep_score`
- **`own.<anything>`** — your own value → shown as the node's data (no heatmap), e.g. `own.cell_delta`.

## Full example — JK-BMS over BLE

Drop-in on top of [syssi/esphome-jk-bms](https://github.com/syssi/esphome-jk-bms): just add `id:` to the sensors you want on the map and the `sensmos:` block.

```yaml
external_components:
  - source: github://syssi/esphome-jk-bms@main
  - source: github://Galusz/esphome-sensmos@main

sensor:
  - platform: jk_bms_ble
    state_of_charge:
      name: "${name} state of charge"
      id: jk_soc
    total_voltage:
      name: "${name} total voltage"
      id: jk_voltage
    power:
      name: "${name} power"
      id: jk_power
    # ... your other sensors unchanged ...

sensmos:
  key: !secret sensmos_key
  update_interval: 60s
  sensors:
    - id: jk_soc
      entity: pub.batt_soc
    - id: jk_voltage
      entity: pub.batt_v
    - id: jk_power
      entity: pub.batt_power
```

And in `secrets.yaml`:

```yaml
sensmos_key: "paste-your-48-char-passkey-here-xxxxxxxxxxxx"
```

## Notes

- **Frameworks:** works on both `esp-idf` (uses the cert bundle) and `arduino`.
- **Timeout / pruning:** a software node goes offline after 48 h without a push and reappears automatically on the next one. Online/offline status is shown on the map.
- **Up to 50 entities** per node.
- The push is a short blocking HTTPS call on the update interval — keep `update_interval` ≥ 60 s if your device also does time-sensitive work (e.g. BLE).

---

Part of the **Sensmos** project · [sensmos.com](https://sensmos.com) · [map](https://sensmos.com/map/) · [whitepaper](https://github.com/Galusz/sensmos-protocol/blob/main/WHITEPAPER.md)
