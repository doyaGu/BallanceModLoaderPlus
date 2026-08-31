#ifndef BML_API_BEHAVIORAPI_H
#define BML_API_BEHAVIORAPI_H

#include "BML/Behavior.h"

namespace BML::Api {

[[nodiscard]] const BML_BehaviorInterface &BehaviorInterface() noexcept;

} // namespace BML::Api

#endif // BML_API_BEHAVIORAPI_H
