#ifndef BML_BEHAVIOR_OBJECTREF_H
#define BML_BEHAVIOR_OBJECTREF_H

#include <cstdint>

namespace BML::Behavior::Internal {

struct ObjectRef {
    std::uint32_t Domain = 0;
    std::uint32_t Slot = 0;
    std::uint32_t Generation = 0;

    [[nodiscard]] bool IsNull() const noexcept { return Domain == 0; }

    friend bool operator==(const ObjectRef &, const ObjectRef &) = default;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_OBJECTREF_H
