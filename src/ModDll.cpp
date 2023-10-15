#include "ModDll.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

bool ModDll::Load() {
    if (!LoadDll()) return false;
    entry = GetFunction<GetBMLEntryFunction>("BMLEntry");
    if (!entry) return false;
    exit = GetFunction<GetBMLExitFunction>("BMLExit");
    return true;
}

INSTANCE_HANDLE ModDll::LoadDll() {
    dllInstance = LoadLibraryEx(dllFileName.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    return dllInstance;
}

void *ModDll::GetFunctionPtr(const char *func) const {
    return reinterpret_cast<void *>(::GetProcAddress((HMODULE)dllInstance, func));
}