#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace mcp4151 {

class MCP4151Output : public output::FloatOutput, public PollingComponent {
 public:
  void set_cs_pin(GPIOPin *pin) { this->cs_pin_ = pin; }
  void set_sck_pin(GPIOPin *pin) { this->sck_pin_ = pin; }
  void set_sdio_pin(GPIOPin *pin) { this->sdio_pin_ = pin; }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_raw_values(bool v) { this->raw_values_ = v; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

 private:
  void set_wiper_(uint16_t value);
  uint16_t read_wiper_();
  void write_bit_(bool bit, bool release_after = false);
  bool read_cmderr_bit_();
  bool read_bit_();

  GPIOPin *cs_pin_{nullptr};
  GPIOPin *sck_pin_{nullptr};
  GPIOPin *sdio_pin_{nullptr};
  sensor::Sensor *sensor_{nullptr};
  bool raw_values_{false};
};

}  // namespace mcp4151
}  // namespace esphome