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
  LOG_UPDATE_INTERVAL(this);
  if (this->sensor_ != nullptr)
    LOG_SENSOR("  ", "Wiper Sensor", this->sensor_);
}

void MCP4151Output::update() {
  if (this->sensor_ == nullptr)
    return;
  uint16_t wiper = this->read_wiper_();
  this->sensor_->publish_state(wiper / 256.0f);
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
  if (release_after) {
    // Release SDIO (input + pull-up) while SCK is still HIGH, before the falling edge.
    // Per the datasheet, MCP4151 starts driving SDIO (CMDERR) on the falling edge of C0.
    // Being in input mode before that edge prevents a drive conflict.
    this->sdio_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }
  this->sck_pin_->digital_write(false);  // falling edge
}

bool MCP4151Output::read_cmderr_bit_() {
  delayMicroseconds(1);
  this->sck_pin_->digital_write(true);          // rising edge
  bool ok = this->sdio_pin_->digital_read();    // 1 = valid command, 0 = error
  delayMicroseconds(1);
  this->sck_pin_->digital_write(false);         // falling edge -- MCP4151 stops driving SDIO here
  // Reclaim SDIO as output only AFTER the falling edge, once the chip has released the line.
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  return ok;
}

bool MCP4151Output::read_bit_() {
  // Clock one bit in from the chip (chip drives SDIO, host only toggles SCK).
  delayMicroseconds(1);
  this->sck_pin_->digital_write(true);          // rising edge -- sample SDIO here
  bool bit = this->sdio_pin_->digital_read();
  delayMicroseconds(1);
  this->sck_pin_->digital_write(false);         // falling edge
  return bit;
}

uint16_t MCP4151Output::read_wiper_() {
  // Read command (MCP4151 datasheet §7.6 / Figure 7-3):
  //   bits[15:12] = 0b0000 (address: volatile wiper 0)
  //   bits[11:10] = 0b11   (opcode: read)
  //   bit[9]      = CMDERR  ← chip drives (1 = valid, 0 = error)
  //   bits[8:0]   = 9-bit wiper value ← chip drives all 9 bits
  static const uint16_t kReadCmd = 0x0C00U;  // 0b0000_1100_0000_0000

  this->cs_pin_->digital_write(false);  // assert CS

  // Send the 6-bit command header (bits 15..10).
  // Bit 10 uses release_after=true: SDIO switches to INPUT+PULLUP while SCK is
  // still HIGH, so the pin is already released when the falling edge hands the
  // bus to the chip (same handoff timing as the write CMDERR sequence).
  for (int i = 15; i >= 11; i--)
    this->write_bit_((kReadCmd >> i) & 1U);
  this->write_bit_((kReadCmd >> 10) & 1U, /*release_after=*/true);

  // Read 10 bits MSB-first: bit 9 = CMDERR, bits 8:0 = wiper value.
  // The chip drives SDIO for all 10 bits; host only clocks SCK.
  uint16_t result = 0;
  for (int i = 9; i >= 0; i--) {
    if (this->read_bit_())
      result |= (1U << i);
  }

  // Reclaim SDIO as output (idle low) once the chip has released the line.
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->sdio_pin_->digital_write(false);

  this->cs_pin_->digital_write(true);   // deassert CS

  bool cmderr = (result >> 9) & 1U;
  uint16_t wiper = result & 0x01FFU;
  ESP_LOGD(TAG, "read_wiper: CMDERR=%u value=%u", cmderr, wiper);
  if (!cmderr)
    ESP_LOGW(TAG, "MCP4151 read: CMDERR=0 (command not accepted)");

  return wiper;
}

void MCP4151Output::set_wiper_(uint16_t value) {
  // 16-bit SPI write command (MSB first, per MCP4151 datasheet section 7.5):
  //   bits[15:12] = address  (0b0000  = volatile wiper 0)
  //   bits[11:10] = opcode   (0b00    = write)
  //   bit[9]      = CMDERR   (driven by MCP4151 on SDIO during this clock; host releases)
  //   bits[8:0]   = 9-bit wiper data (0 = wiper at B, 256 = wiper at A)
  // For address=0 and opcode=write=0 the command word equals the wiper value.
  uint16_t command = value & 0x01FFU;

  this->cs_pin_->digital_write(false);  // assert CS (active low)

  // Bits 15-11: address[3:0] + opcode[1] -- host drives, normal write
  for (int i = 15; i >= 11; i--) {
    this->write_bit_((command >> i) & 1U);
  }

  // Bit 10: opcode[0] (C0) -- write, then release SDIO so MCP4151 can drive CMDERR
  // The chip switches SDIO to output on the falling edge of this clock (datasheet §6.1.3).
  this->write_bit_((command >> 10) & 1U, true);

  // Bit 9: CMDERR -- MCP4151 drives SDIO; read it, then reclaim line after falling edge
  bool ok = this->read_cmderr_bit_();
  if (!ok) {
    ESP_LOGW(TAG, "MCP4151 reported a write error (CMDERR low) -- command ignored by device");
  }

  // Bits 8-0: 9-bit wiper data -- host drives again
  for (int i = 8; i >= 0; i--) {
    this->write_bit_((command >> i) & 1U);
  }

  this->cs_pin_->digital_write(true);   // deassert CS
}

}  // namespace mcp4151
}  // namespace esphome