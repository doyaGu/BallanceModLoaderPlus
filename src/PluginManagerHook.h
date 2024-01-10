#ifndef BML_PLUGINMANAGERHOOK_H
#define BML_PLUGINMANAGERHOOK_H

#include <string>
#include <vector>

#include "CKPluginManager.h"

#include "Macros.h"

CP_HOOK_CLASS(CKPluginManager) {
public:
    CP_DECLARE_METHOD_HOOK(int, ParsePlugins, (CKSTRING Directory));
    CP_DECLARE_METHOD_HOOK(CKERROR, RegisterPlugin, (CKSTRING str));

    CP_DECLARE_METHOD_HOOK_PTRS(CKPluginManager, int, ParsePlugins, (CKSTRING Directory));
    CP_DECLARE_METHOD_HOOK_PTRS(CKPluginManager, CKERROR, RegisterPlugin, (CKSTRING str));

    static bool InitHooks();
    static void ShutdownHooks();
};

#endif // BML_PLUGINMANAGERHOOK_H
