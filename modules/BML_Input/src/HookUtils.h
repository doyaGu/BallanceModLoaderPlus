/**
 * @file HookUtils.h
 * @brief Memory protection and VTable utilities for BML_Input module
 * 
 * Self-contained hook utilities without BMLPlus dependencies.
 */

#ifndef BML_INPUT_HOOKUTILS_H
#define BML_INPUT_HOOKUTILS_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace utils {

inline void *GetModuleBaseAddress(const char *moduleName) {
    HMODULE hModule = GetModuleHandleA(moduleName);
    return hModule ? reinterpret_cast<void *>(hModule) : nullptr;
}

inline uint32_t ProtectRegion(void *region, size_t size, uint32_t protection) {
    DWORD oldProtect = 0;
    VirtualProtect(region, size, protection, &oldProtect);
    return oldProtect;
}

inline uint32_t UnprotectRegion(void *region, size_t size) {
    return ProtectRegion(region, size, PAGE_EXECUTE_READWRITE);
}

template<typename T>
inline void *TypeErase(T target) {
    return *reinterpret_cast<void **>(&target);
}

template<typename T>
inline T ForceReinterpretCast(void *addr) {
    return *reinterpret_cast<T *>(&addr);
}

template<typename T>
inline T ForceReinterpretCast(void *base, size_t offset) {
    void *p = static_cast<char *>(base) + offset;
    return *reinterpret_cast<T *>(&p);
}

inline void **GetVTable(void *instance) {
    if (instance) {
        return *static_cast<void ***>(instance);
    }
    return nullptr;
}

template<typename T>
inline void LoadVTable(void *instance, T &table) {
    if (instance) {
        void **src = static_cast<void **>(*static_cast<void **>(instance));
        void **dest = reinterpret_cast<void **>(&table);
        for (size_t i = 0; i < sizeof(T) / sizeof(void *); ++i) {
            dest[i] = src[i];
        }
    }
}

template<typename T>
inline void SaveVTable(void *instance, T &table) {
    if (instance) {
        void **src = reinterpret_cast<void **>(&table);
        void **dest = static_cast<void **>(*static_cast<void **>(instance));
        uint32_t originalProtection = UnprotectRegion(dest, sizeof(T));
        for (size_t i = 0; i < sizeof(T) / sizeof(void *); ++i) {
            dest[i] = src[i];
        }
        ProtectRegion(dest, sizeof(T), originalProtection);
    }
}

inline void *HookVirtualMethod(void *instance, void *hook, size_t offset) {
    if (!instance) return nullptr;
    
    void **vtable = *static_cast<void ***>(instance);
    void *original = vtable[offset];
    
    uint32_t oldProtect = UnprotectRegion(&vtable[offset], sizeof(void *));
    vtable[offset] = hook;
    ProtectRegion(&vtable[offset], sizeof(void *), oldProtect);
    
    return original;
}

template<typename T>
inline void *HookVirtualMethod(void *instance, T hook, size_t offset) {
    return HookVirtualMethod(instance, TypeErase<T>(hook), offset);
}

} // namespace utils

#endif // BML_INPUT_HOOKUTILS_H
