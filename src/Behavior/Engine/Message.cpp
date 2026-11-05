#include "Behavior/Engine/Message.h"

#include "Behavior/Engine/Scope.h"

namespace BML::Behavior::Internal::Engine {

int Send(CKContext *context, CKBehavior *behavior, CKDWORD message,
         const CKBehaviorContext *frame) {
    if (!context || !behavior)
        return CKERR_INVALIDOBJECT;
    Scope scope(context, behavior, frame);
    return behavior->CallCallbackFunction(message);
}

} // namespace BML::Behavior::Internal::Engine
