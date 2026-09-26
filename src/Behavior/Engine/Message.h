#ifndef BML_BEHAVIOR_ENGINE_MESSAGE_H
#define BML_BEHAVIOR_ENGINE_MESSAGE_H

#include "CKAll.h"

namespace BML::Behavior::Internal::Engine {

// Sends one lifecycle message to a Block's callback inside a Scope and
// returns the callback result. CK2 sends none of these messages when a Block
// is created or edited programmatically, so the Runtime and graph edits
// emulate them through here. The caller checks that the Block survived.
int Send(CKContext *context, CKBehavior *behavior, CKDWORD message,
         const CKBehaviorContext *frame = nullptr);

} // namespace BML::Behavior::Internal::Engine

#endif // BML_BEHAVIOR_ENGINE_MESSAGE_H
