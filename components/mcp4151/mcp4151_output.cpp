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

  this->cs_pin_->digital_write(true);    // CS idle high (active low)
  this->sck_pin_->digital_write(false);  // SCK idle low (SPI mode 0,0)
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

void MCP4151Output::send_byte_(uint8_t byte) {
  // Send 8 bits MSB first. SDIO stays OUTPUT throughout — no pin_mode switches.
  // The MCP4151 CMDERR output (bit 9) is open-drain: holding SDIO low during
  // that clock cannot damage the device and avoids all direction-switch timing gaps.
  for (int i = 7; i >= 0; i--) {
    this->sdio_pin_->digital_write((byte >> i) & 1U);
    delayMicroseconds(1);
    this->sck_pin_->digital_write(true);   // rising edge — MCP4151 latches here
    delayMicroseconds(1);
    this->sck_pin_->digital_write(false);  // falling edge
    delayMicroseconds(1);
  }
}

void MCP4151Output::set_wiper_(uint16_t value) {
  // 16-bit write command (MCP4151 datasheet §7.5, Figure 7-1):
  //   bits[15:12] = address 0b0000 (volatile wiper 0)
  //   bits[11:10] = opcode  0b00   (write)
  //   bit[9]      = 0 (CMDERR driven open-drain by chip; ESP holds low — safe)
  //   bits[8:0]   = 9-bit wiper value (0x000 = wiper at B, 0x100 = wiper at A)
  //
  // With address=0 and opcode=write=00, the upper 7 bits are all zero, so:
  //   byte1 = (value >> 8) & 0x01  — carries only D8
  //   byte2 =  value       & 0xFF  — carries D7..D0
  uint8_t byte1 = (value >> 8) & 0x01U;
  uint8_t byte2 =  value       & 0xFFU;
  ESP_LOGD(TAG, "set_wiper: value=%u byte1=0x%02X byte2=0x%02X", value, byte1, byte2);

  this->cs_pin_->digital_write(false);  // assert CS (active low)
  delayMicroseconds(1);
  this->send_byte_(byte1);
  this->send_byte_(byte2);
  delayMicroseconds(1);
  this->cs_pin_->digital_write(true);   // deassert CS
}

}  // namespace mcp4151
}  // namespace esphome
