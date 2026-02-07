// Minimal replacement for avrlib/resources_manager.h
// Not needed for pattern generation, but included for compatibility

#ifndef AVRLIB_RESOURCES_MANAGER_H_
#define AVRLIB_RESOURCES_MANAGER_H_

namespace avrlib {

// Dummy resources manager template - not used in pattern generation
template<typename ResourceId, typename Tables>
class ResourcesManager {
 public:
  ResourcesManager() { }
};

// Dummy resources tables template
template<typename StringTable, typename LookupTable>
struct ResourcesTables {
  ResourcesTables() { }
};

}  // namespace avrlib

#endif  // AVRLIB_RESOURCES_MANAGER_H_
