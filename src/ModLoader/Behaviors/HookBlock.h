/**
 * @file HookBlock.h
 * @brief Header for HookBlock behavior
 */

#ifndef BML_MODLOADER_HOOKBLOCK_H
#define BML_MODLOADER_HOOKBLOCK_H

#include "CKAll.h"

namespace ModLoader {

/// Callback function type for HookBlock
typedef int (*CKBehaviorCallback)(const CKBehaviorContext *behcontext, void *arg);

/// Register HookBlock behavior with the engine
CKObjectDeclaration *FillBehaviorHookBlockDecl();

/// Create HookBlock prototype
CKERROR CreateHookBlockProto(CKBehaviorPrototype **pproto);

/// HookBlock execution function
int HookBlock(const CKBehaviorContext &behcontext);

} // namespace ModLoader

#endif // BML_MODLOADER_HOOKBLOCK_H
