#include "Behavior/Engine/Access.h"

namespace BML::Behavior::Internal::Engine {
namespace {


} // namespace


void SetParentId(CKBehavior *behavior, CKBehavior *parent) noexcept {
    if (behavior)
        behavior->SetParent(parent);
}


bool IsDelayed(CKBehaviorLink *link) noexcept {
    return link && link->IsActivationPending();
}

} // namespace BML::Behavior::Internal::Engine
