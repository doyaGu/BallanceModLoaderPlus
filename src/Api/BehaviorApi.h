#ifndef BML_API_BEHAVIORAPI_H
#define BML_API_BEHAVIORAPI_H

#include <string_view>

#include "BML/Behavior.h"

namespace BML::Api {

[[nodiscard]] const BML_BehaviorInterface &BehaviorInterface() noexcept;

// Loader-only. Opens a Session for a registered owner without identifying
// the caller: Script Mods run inside BMLPlus.dll, so the caller check in
// OpenSession cannot tell them apart. The owner must be active.
[[nodiscard]] int OpenBehaviorSessionFor(std::string_view ownerId,
                                         BML_BehaviorSession *outSession,
                                         BML_BehaviorStatus *status) noexcept;

} // namespace BML::Api

#endif // BML_API_BEHAVIORAPI_H
