#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace mcp4151 {

class MCP4151Output : public output::FloatOutput, public Component {
 public:
  void set_cs_pin(GPIOPin *pin) { this->cs_pin_ = pin; }
  void set_sck_pin(GPIOPin *pin) { this->sck_pin_ = pin; }
  void set_sdio_pin(GPIOPin *pin) { this->sdio_pin_ = pin; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

 private:
  void set_wiper_(uint16_t value);
  void write_bit_(bool bit, bool switch_to_input_after = false);
  bool read_status_bit_();

  GPIOPin *cs_pin_{nullptr};
  GPIOPin *sck_pin_{nullptr};
  GPIOPin *sdio_pin_{nullptr};
};

}  // namespace mcp4151
}  // namespace esphome
