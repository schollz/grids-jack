// Replacement for avrlib/op.h for non-AVR platforms
// Provides math utility functions used by the Grids pattern generator

#ifndef AVRLIB_OP_H_
#define AVRLIB_OP_H_

#include <stdint.h>

namespace avrlib {

// 8-bit * 8-bit unsigned multiply, return high byte (shifted right 8 bits)
inline uint8_t U8U8MulShift8(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((static_cast<uint16_t>(a) * b) >> 8);
}

// 8-bit * 8-bit unsigned multiply, return full 16-bit result
inline uint16_t U8U8Mul(uint8_t a, uint8_t b) {
  return static_cast<uint16_t>(a) * b;
}

// Linear interpolation (mix) between two 8-bit values
// Returns: a + (b - a) * (t / 256)
inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t t) {
  return a + ((static_cast<uint16_t>(b) - a) * t >> 8);
}

}  // namespace avrlib

#endif  // AVRLIB_OP_H_
