// Replacement for avrlib/random.h for non-AVR platforms
// Simple random number generator for pattern perturbation

#ifndef AVRLIB_RANDOM_H_
#define AVRLIB_RANDOM_H_

#include <stdint.h>

namespace avrlib {

class Random {
 public:
  static inline void Update() {
    // Simple LCG (Linear Congruential Generator)
    rng_state_ = rng_state_ * 1664525L + 1013904223L;
  }
  
  static inline uint8_t GetByte() {
    Update();
    return static_cast<uint8_t>(rng_state_ >> 24);
  }
  
  static inline uint32_t state() {
    return rng_state_;
  }
  
  static inline void Seed(uint32_t seed) {
    rng_state_ = seed;
  }
  
 private:
  static uint32_t rng_state_;
};

}  // namespace avrlib

#endif  // AVRLIB_RANDOM_H_
