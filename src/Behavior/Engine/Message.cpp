#include "Behavior/Engine/Message.h"

#include "Behavior/Engine/Access.h"
#include "Behavior/Engine/Scope.h"

namespace BML::Behavior::Internal::Engine {

int Send(CKContext *context, CKBehavior *behavior, CKDWORD message,
         const CKBehaviorContext *frame) {
    if (!context || !behavior)
        return CKERR_INVALIDOBJECT;
    if (message != CKM_BEHAVIORCREATE) {
        Scope scope(context, behavior, frame);
        return behavior->CallCallbackFunction(message);
    }

    // Ballance's retail CK2.dll maps CKM_BEHAVIORCREATE to a zero callback
    // mask inside CallCallbackFunction. Dispatch CREATE from the live block
    // data so the provider's current callback, mask, and argument are kept.
    BehaviorBlockData *block = BlockData(behavior);
    if (!block || !block->m_Callback ||
        (block->m_CallbackMask & CKCB_BEHAVIORCREATE) == 0)
        return CK_OK;
    Scope scope(context, behavior, frame);
    context->m_BehaviorContext.CallbackMessage = message;
    context->m_BehaviorContext.CallbackArg = block->m_CallbackArg;
    return block->m_Callback(context->m_BehaviorContext);
}

} // namespace BML::Behavior::Internal::Engine
