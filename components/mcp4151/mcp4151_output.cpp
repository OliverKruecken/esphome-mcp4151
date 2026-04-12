#include "mcp4151_output.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mcp4151 {

static const char *const TAG = "mcp4151.output";

void MCP4151Output::setup() {
  this->cs_pin_->setup();
  this->sck_pin_->setup();
  this->sdio_pin_->setup();

  this->cs_pin_->digital_write(true);           // CS idle high (active low)
  this->sck_pin_->digital_write(false);         // SCK idle low (SPI mode 0,0)
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT); // SDIO starts as output
  this->sdio_pin_->digital_write(false);
}

void MCP4151Output::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP4151 Digital Potentiometer:");
  LOG_PIN("  CS Pin:   ", this->cs_pin_);
  LOG_PIN("  SCK Pin:  ", this->sck_pin_);
  LOG_PIN("  SDIO Pin: ", this->sdio_pin_);
}

void MCP4151Output::write_state(float state) {
  // Map [0.0, 1.0] to [0, 256] (257 discrete wiper positions)
  auto value = static_cast<uint16_t>(state * 256.0f + 0.5f);
  if (value > 256)
    value = 256;
  ESP_LOGD(TAG, "Setting wiper to %u (state=%.3f)", value, state);
  this->set_wiper_(value);
}

void MCP4151Output::write_bit_(bool bit, bool switch_to_input_after) {
  this->sdio_pin_->digital_write(bit);
  delayMicroseconds(1);
  this->sck_pin_->digital_write(true);  // rising edge -- MCP4151 latches data here
  delayMicroseconds(1);
  if (switch_to_input_after) {
    // Switch to input with pull-up before the falling edge so the MCP4151 can
    // drive SDIO for the status bit on the next clock cycle.
    this->sdio_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }
  this->sck_pin_->digital_write(false); // falling edge -- MCP4151 drives status bit
}

bool MCP4151Output::read_status_bit_() {
  delayMicroseconds(1);
  this->sck_pin_->digital_write(true);            // rising edge
  bool bit = this->sdio_pin_->digital_read();     // sample MCP4151's status output
  delayMicroseconds(1);
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);   // back to output before falling edge
  this->sck_pin_->digital_write(false);           // falling edge
  return bit;
}

void MCP4151Output::set_wiper_(uint16_t value) {
  // 16-bit command (MSB first):
  //   bits[15:12] = address (0b0000 = wiper register)
  //   bits[11:10] = opcode  (0b00   = write)
  //   bit[9]      = status  (driven by MCP4151: 1=OK, 0=error)
  //   bits[8:0]   = 9-bit wiper value
  // For address=0 and opcode=write=0, the command word equals the value.
  uint16_t command = value & 0x01FFU;

  this->cs_pin_->digital_write(false);  // assert CS (active low)

  // Bits 15-11: address[3:0] + opcode[1] — write normally
  for (int i = 15; i >= 11; i--) {
    this->write_bit_((command >> i) & 1U);
  }

  // Bit 10: opcode[0] — write, then hand the line to the MCP4151
  this->write_bit_((command >> 10) & 1U, true);

  // Bit 9: read status from MCP4151 (high = OK, low = error), then reclaim line
  bool ok = this->read_status_bit_();
  if (!ok) {
    ESP_LOGW(TAG, "MCP4151 reported a write error (status bit low)");
  }

  // Bits 8-0: 9-bit wiper data — write normally
  for (int i = 8; i >= 0; i--) {
    this->write_bit_((command >> i) & 1U);
  }

  this->cs_pin_->digital_write(true);   // deassert CS
}

}  // namespace mcp4151
}  // namespace esphome
