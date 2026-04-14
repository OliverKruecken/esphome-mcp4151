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

void MCP4151Output::write_bit_(bool bit, bool release_after) {
  this->sdio_pin_->digital_write(bit);
  delayMicroseconds(1);
  this->sck_pin_->digital_write(true);   // rising edge -- MCP4151 latches on this edge
  delayMicroseconds(1);
  this->sck_pin_->digital_write(false);  // falling edge -- MCP4151 starts driving CMDERR here
  if (release_after) {
    // Release SDIO AFTER the falling edge of C0: the chip begins driving SDIO (CMDERR, open-drain)
    // on that edge (datasheet §6.1.3). Releasing before would leave the line floating at the
    // moment the chip takes over; releasing after ensures a clean handoff.
    delayMicroseconds(1);
    this->sdio_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }
}

bool MCP4151Output::read_cmderr_bit_() {
  // The chip has been driving SDIO since the falling edge of C0 — line is already settled.
  // No setup delay needed before the rising edge.
  this->sck_pin_->digital_write(true);          // rising edge -- sample CMDERR
  bool ok = this->sdio_pin_->digital_read();    // 1 = valid command, 0 = error
  delayMicroseconds(1);
  this->sck_pin_->digital_write(false);         // falling edge -- MCP4151 releases SDIO here
  delayMicroseconds(1);
  // Reclaim SDIO as output AFTER the falling edge, once the chip has released the line.
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  return ok;
}

void MCP4151Output::set_wiper_(uint16_t value) {
  // 16-bit SPI write command (MSB first, per MCP4151 datasheet §7.5 / Figure 7-1):
  //   bits[15:12] = address  (0b0000 = volatile wiper 0)
  //   bits[11:10] = opcode   (0b00   = write)
  //   bit[9]      = CMDERR   (MCP4151 drives this bit open-drain; host releases SDIO)
  //   bits[8:0]   = 9-bit wiper data (D8:D0; 0x000 = wiper at B, 0x100 = wiper at A)
  uint16_t command = value & 0x01FFU;
  ESP_LOGD(TAG, "set_wiper: value=%u command=0x%04X", value, command);

  this->cs_pin_->digital_write(false);  // assert CS (active low)

  // Bits 15-11: address[3:0] + opcode[1] -- host drives, normal write
  for (int i = 15; i >= 11; i--) {
    this->write_bit_((command >> i) & 1U);
  }

  // Bit 10: opcode[0] (C0) -- after the falling edge the chip takes over SDIO for CMDERR
  this->write_bit_((command >> 10) & 1U, /*release_after=*/true);

  // Bit 9: CMDERR -- MCP4151 drives SDIO open-drain; read, then reclaim after falling edge
  bool ok = this->read_cmderr_bit_();
  ESP_LOGD(TAG, "CMDERR=%s", ok ? "1 (valid)" : "0 (error)");
  if (!ok) {
    ESP_LOGW(TAG, "MCP4151 CMDERR=0: write command rejected by device");
  }

  // Bits 8-0: 9-bit wiper data (D8 first) -- host drives again
  for (int i = 8; i >= 0; i--) {
    this->write_bit_((command >> i) & 1U);
  }

  this->cs_pin_->digital_write(true);   // deassert CS
}

}  // namespace mcp4151
}  // namespace esphome
