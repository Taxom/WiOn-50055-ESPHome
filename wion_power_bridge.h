#pragma once

#include <Arduino.h>

// Woods WiOn 50055 / KAB power-monitor bridge diagnostic reader.
// Stock hardware connection:
//   GPIO0  -> clock/output to the intermediate MCU
//   GPIO12 <- data/input from the intermediate MCU
//
// The bridge returns four 32-bit words:
//   0x48 + 24-bit H
//   0x49 + 24-bit I
//   0x57 + 24-bit W
//   0x56 + 24-bit V

struct WionPowerFrame {
  uint32_t words[4] = {0, 0, 0, 0};
  uint32_t h = 0;
  uint32_t i = 0;
  uint32_t w = 0;
  uint32_t v = 0;
  bool valid = false;
};

static inline void wion_bus_setup() {
  pinMode(12, INPUT);

  // Preload GPIO0 HIGH before changing it to OUTPUT to avoid an unnecessary
  // LOW glitch on the clock line.
  digitalWrite(0, HIGH);
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
}

static inline WionPowerFrame wion_read_power_frame() {
  WionPowerFrame frame;

  // Keep the clock idle HIGH, matching the stock bus behavior and the
  // previously demonstrated proof-of-concept reader.
  digitalWrite(0, HIGH);
  delayMicroseconds(50);

  // A complete frame is 128 bits. 35 us half-period is deliberately used
  // here: it is slightly slower than the ~32 us seen in the stock firmware
  // and has already been demonstrated on this hardware family.
  noInterrupts();
  for (uint8_t block = 0; block < 4; block++) {
    uint32_t value = 0;
    for (uint8_t bit = 0; bit < 32; bit++) {
      digitalWrite(0, LOW);
      delayMicroseconds(35);

      value <<= 1;
      if (digitalRead(12))
        value |= 1U;

      digitalWrite(0, HIGH);
      delayMicroseconds(35);
    }
    frame.words[block] = value;
  }
  interrupts();

  digitalWrite(0, HIGH);

  const uint8_t tag_h = static_cast<uint8_t>(frame.words[0] >> 24);
  const uint8_t tag_i = static_cast<uint8_t>(frame.words[1] >> 24);
  const uint8_t tag_w = static_cast<uint8_t>(frame.words[2] >> 24);
  const uint8_t tag_v = static_cast<uint8_t>(frame.words[3] >> 24);

  frame.valid = (tag_h == 0x48 && tag_i == 0x49 &&
                 tag_w == 0x57 && tag_v == 0x56);

  frame.h = frame.words[0] & 0x00FFFFFFUL;
  frame.i = frame.words[1] & 0x00FFFFFFUL;
  frame.w = frame.words[2] & 0x00FFFFFFUL;
  frame.v = frame.words[3] & 0x00FFFFFFUL;

  return frame;
}
