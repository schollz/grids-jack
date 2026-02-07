// Implementation of Random class for non-AVR platforms

#include "avrlib/random.h"
#include <time.h>

namespace avrlib {

// Initialize with a time-based seed
uint32_t Random::rng_state_ = static_cast<uint32_t>(time(nullptr));

}  // namespace avrlib
