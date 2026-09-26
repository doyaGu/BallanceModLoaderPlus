#ifndef BML_BEHAVIOR_BLOCKS_HOOKBLOCK_H
#define BML_BEHAVIOR_BLOCKS_HOOKBLOCK_H

#include "Behavior/Blocks/Hook.h"
#include "Behavior/Block.h"

namespace BML::Behavior::Internal::HookBlock {

// A Block with autoActivateOutputs false leaves its Outs to the callback.
BlockSpec Make(std::shared_ptr<Binding> binding,
          int inputCount = 1, int outputCount = 1,
          bool autoActivateOutputs = true);

BlockSpec Make(Callback callback, void *argument = nullptr,
          int inputCount = 1, int outputCount = 1);

void Register(XObjectDeclarationArray *registry);

} // namespace BML::Behavior::Internal::HookBlock

#endif // BML_BEHAVIOR_BLOCKS_HOOKBLOCK_H
