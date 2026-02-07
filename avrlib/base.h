// Replacement for avrlib/base.h for non-AVR platforms
// This provides basic types and macros used by the Grids pattern generator

#ifndef AVRLIB_BASE_H_
#define AVRLIB_BASE_H_

#include <stdint.h>

namespace avrlib {

// Disable copy constructor and assignment operator
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete

}  // namespace avrlib

#endif  // AVRLIB_BASE_H_
