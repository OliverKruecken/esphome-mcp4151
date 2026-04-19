# esphome-mcp4151

ESPHome external component for the MCP4151 digital potentiometer.

The MCP4151 is a single-channel, 256-step SPI digital potentiometer by Microchip. This component exposes it as a standard ESPHome `output` (0.0–1.0 → wiper position 0–256), making it usable with any ESPHome feature that drives an output: lights, fans, automations, scripts, etc.

## Installation

Add this to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/OliverKruecken/esphome-mcp4151
      ref: main
    components: [mcp4151]
```

## Wiring

The MCP4151 uses a 3-wire SPI interface (CS, SCK, SDIO). During each transaction the SDIO line briefly changes direction: the chip drives it for one clock cycle (the CMDERR status bit) while the host releases it. A **330Ω series resistor** on the SDIO line is strongly recommended to protect against any residual drive overlap during these transitions.

| MCP4151 Pin | Signal | Connect to                        |
|-------------|--------|-----------------------------------|
| 1           | CS     | GPIO (`cs_pin`)                   |
| 2           | SCK    | GPIO (`sck_pin`)                  |
| 3           | SDIO   | GPIO (`sdio_pin`) **via 330Ω**    |
| 4           | VSS    | GND                               |
| 5           | P0A    | One end of resistance element     |
| 6           | P0W    | Wiper output                      |
| 7           | P0B    | Other end of resistance element   |
| 8           | VDD    | 3.3V                              |

## Configuration

```yaml
output:
  - platform: mcp4151
    id: pot_output
    cs_pin: GPIO5
    sck_pin: GPIO4
    sdio_pin: GPIO0         # via 330Ω series resistor
    update_interval: 30s   # optional: enables periodic wiper readback
    raw_values: false       # optional: use 0..256 steps instead of 0..1 (default: false)
    sensor:                 # optional: publish wiper position to Home Assistant
      name: "Wiper Position"
```

### Options

| Parameter         | Required | Default | Description                                                             |
|-------------------|----------|---------|-------------------------------------------------------------------------|
| `cs_pin`          | yes      | —       | Chip select pin (active low)                                            |
| `sck_pin`         | yes      | —       | Serial clock pin                                                        |
| `sdio_pin`        | yes      | —       | Bidirectional serial data pin (connect via 330Ω)                        |
| `update_interval` | no       | 60s     | How often to read back the wiper position from the chip (requires `sensor`) |
| `raw_values`      | no       | `false` | When `true`, the output and sensor use raw wiper steps (0–256) instead of the normalized 0.0–1.0 range |
| `sensor`          | no       | —       | Sub-component that publishes the wiper position read back from hardware |

All standard ESPHome `output` options (`min_power`, `max_power`, `id`, etc.) are also supported.

### `raw_values`

By default the component uses the standard ESPHome float output range:

- **Write:** 0.0 → wiper step 0, 1.0 → wiper step 256
- **Sensor:** publishes 0.0–1.0

With `raw_values: true` the scaling is bypassed:

- **Write:** the float value is rounded directly to a wiper step (0–256)
- **Sensor:** publishes the raw step count as a float (0–256)

This is useful when you want to drive the potentiometer with a `number` entity or automation that works in natural resistance steps.

### Wiper readback sensor

When a `sensor` sub-component is configured the component periodically reads the wiper register back from the chip and publishes it to Home Assistant. The `update_interval` controls the polling period (minimum 1s, default 60s).

```yaml
output:
  - platform: mcp4151
    id: pot_output
    cs_pin: GPIO5
    sck_pin: GPIO4
    sdio_pin: GPIO0
    update_interval: 30s
    sensor:
      name: "Wiper Position"
      id: wiper_sensor
```

The published value matches the write interface: 0.0–1.0 by default, or 0–256 with `raw_values: true`.

## Examples

### Dimmable light

```yaml
output:
  - platform: mcp4151
    id: pot_output
    cs_pin: GPIO5
    sck_pin: GPIO4
    sdio_pin: GPIO0

light:
  - platform: monochromatic
    name: "Potentiometer Level"
    output: pot_output
    default_transition_length: 0s
```

### Dimmable light with wiper readback

```yaml
output:
  - platform: mcp4151
    id: pot_output
    cs_pin: GPIO5
    sck_pin: GPIO4
    sdio_pin: GPIO0
    update_interval: 30s
    sensor:
      name: "Wiper Position"

light:
  - platform: monochromatic
    name: "Potentiometer Level"
    output: pot_output
    default_transition_length: 0s
```

### Raw step control via a number entity

```yaml
output:
  - platform: mcp4151
    id: pot_output
    cs_pin: GPIO5
    sck_pin: GPIO4
    sdio_pin: GPIO0
    raw_values: true
    update_interval: 30s
    sensor:
      name: "Wiper Steps"   # publishes 0..256

number:
  - platform: template
    name: "Wiper Step"
    min_value: 0
    max_value: 256
    step: 1
    set_action:
      - output.set_level:
          id: pot_output
          level: !lambda "return x;"
```

## License

MIT
