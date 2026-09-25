#ifndef BML_HOOKS_HOOK_LIFECYCLE_H
#define BML_HOOKS_HOOK_LIFECYCLE_H

bool HookObjectLoad();
bool UnhookObjectLoad();

bool HookPhysicalize();
bool UnhookPhysicalize();
void RunPhysicsPostProcess();

bool IsInputHookInstalled();
bool IsObjectLoadHookInstalled();
bool IsPhysicsPostProcessHookInstalled();
bool IsPhysicalizeHookInstalled();

#endif // BML_HOOKS_HOOK_LIFECYCLE_H
