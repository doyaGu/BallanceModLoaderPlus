#ifndef BML_BEHAVIOR_HOOKBLOCK_H
#define BML_BEHAVIOR_HOOKBLOCK_H

#include "Behavior/Runtime.h"

namespace BML::Behavior::HookBlock {

using Callback = int (*)(const CKBehaviorContext *context, void *argument);

Spec Make(Callback callback, void *argument = nullptr,
          int inputCount = 1, int outputCount = 1);

void Register(XObjectDeclarationArray *registry);

} // namespace BML::Behavior::HookBlock

#endif // BML_BEHAVIOR_HOOKBLOCK_H
