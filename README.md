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

The MCP4151 uses a 3-wire SPI interface (CS, SCK, SDIO). For write operations the SDIO line is driven entirely by the host — no direction switching is needed. A **330Ω resistor in series** on the SDIO line is optional but provides a safety margin against accidental shorts or ESD.

| MCP4151 Pin | Signal | Connect to                        |
|-------------|--------|-----------------------------------|
| 1           | CS     | GPIO (`cs_pin`)                   |
| 2           | SCK    | GPIO (`sck_pin`)                  |
| 3           | SDIO   | GPIO (`sdio_pin`) *(330Ω optional)* |
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
    sdio_pin: GPIO0    # 330Ω series resistor optional but recommended
```

### Options

| Parameter  | Required | Description                  |
|------------|----------|------------------------------|
| `cs_pin`   | yes      | Chip select pin (active low) |
| `sck_pin`  | yes      | Serial clock pin             |
| `sdio_pin` | yes      | Serial data pin              |

All standard ESPHome `output` options (`min_power`, `max_power`, `id`, etc.) are also supported.

## Example

Expose the potentiometer as a dimmable light in Home Assistant:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/OliverKruecken/esphome-mcp4151
      ref: main
    components: [mcp4151]

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

## License

MIT
