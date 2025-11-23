#include "HookUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Psapi.h>
#include <strsafe.h>

namespace utils {
    void OutputDebugA(const char *format, ...) {
        char buf[4096] = {0};

        va_list args;
        va_start(args, format);
        ::StringCchVPrintfA(buf, ARRAYSIZE(buf) - 1, format, args);
        va_end(args);

        buf[ARRAYSIZE(buf) - 1] = '\0';
        ::OutputDebugStringA(buf);
    }

    void OutputDebugW(const wchar_t *format, ...) {
        wchar_t buf[4096] = {0};

        va_list args;
        va_start(args, format);
        ::StringCchVPrintfW(buf, ARRAYSIZE(buf) - 1, format, args);
        va_end(args);

        buf[ARRAYSIZE(buf) - 1] = L'\0';
        ::OutputDebugStringW(buf);
    }

    void *GetSelfModuleHandle() {
        MEMORY_BASIC_INFORMATION mbi;
        return ((::VirtualQuery((LPVOID) &GetSelfModuleHandle, &mbi, sizeof(mbi)) != 0)
                ? (HMODULE) mbi.AllocationBase : nullptr);
    }

    void *GetModuleBaseAddress(void *hModule) {
        if (!hModule)
            return nullptr;

        MODULEINFO moduleInfo;
        ::GetModuleInformation(::GetCurrentProcess(), (HMODULE)hModule, &moduleInfo, sizeof(moduleInfo));

        return moduleInfo.lpBaseOfDll;
    }

    void *GetModuleBaseAddress(const char *modulePath) {
        if (!modulePath)
            return nullptr;

        int size = ::MultiByteToWideChar(CP_UTF8, 0, modulePath, -1, nullptr, 0);
        if (size == 0)
            return nullptr;

        auto ws = new wchar_t[size];
        ::MultiByteToWideChar(CP_UTF8, 0, modulePath, -1, ws, size);

        HMODULE hModule = ::GetModuleHandleW(ws);
        delete[] ws;
        if (!hModule)
            return nullptr;

        MODULEINFO moduleInfo;
        ::GetModuleInformation(::GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));

        return moduleInfo.lpBaseOfDll;
    }

    uint32_t ProtectRegion(void *region, size_t size, uint32_t protection) {
        DWORD oldProtect;
        VirtualProtect(region, size, protection, &oldProtect);
        return oldProtect;
    }

    uint32_t UnprotectRegion(void *region, size_t size) {
        DWORD oldProtect;
        VirtualProtect(region, size, PAGE_EXECUTE_READWRITE, &oldProtect);
        return oldProtect;
    }

    void *HookVirtualMethod(void *instance, void *hook, size_t offset) {
        uintptr_t vtable = *((uintptr_t *) instance);
        uintptr_t entry = vtable + offset * sizeof(uintptr_t);
        uintptr_t original = *((uintptr_t *) entry);

        uint32_t originalProtection = UnprotectRegion((void *) entry, sizeof(void *));
        *((uintptr_t *) entry) = (uintptr_t) hook;
        ProtectRegion((void *) entry, sizeof(void *), originalProtection);

        return (void *) original;
    }
}
