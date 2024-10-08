#ifndef BML_HOOKUTILS_H
#define BML_HOOKUTILS_H

#include <cstdint>

namespace utils {
    void OutputDebugA(const char *format, ...);
    void OutputDebugW(const wchar_t *format, ...);

    template<typename T>
    void *TypeErase(T target) {
        return *reinterpret_cast<void **>(&target);
    }

    template<typename T>
    T ForceReinterpretCast(void *addr) {
        return *reinterpret_cast<T *>(&addr);
    }

    template<typename T>
    T ForceReinterpretCast(void *base, size_t offset) {
        void *p = static_cast<char *>(base) + offset;
        return *reinterpret_cast<T *>(&p);
    }

    void *GetSelfModuleHandle();

    void *GetModuleBaseAddress(void *hModule);
    void *GetModuleBaseAddress(const char *modulePath);

    uint32_t ProtectRegion(void *region, size_t size, uint32_t protection);
    uint32_t UnprotectRegion(void *region, size_t size);

    inline void **GetVTable(void *instance) {
        if (instance) {
            return *static_cast<void ***>(instance);
        } else {
            return nullptr;
        }
    }

    template<typename T>
    void LoadVTable(void *instance, T &table) {
        if (instance) {
            void **src = static_cast<void**>(*static_cast<void**>(instance));
            void **dest = reinterpret_cast<void**>(&table);
            for (size_t i = 0; i < sizeof(T) / sizeof(void *); ++i) {
                dest[i] = src[i];
            }
        }
    }

    template<typename T>
    void SaveVTable(void *instance, T &table) {
        if (instance) {
            void **src = reinterpret_cast<void**>(&table);
            void **dest = static_cast<void**>(*static_cast<void**>(instance));
            uint32_t originalProtection = UnprotectRegion(dest, sizeof(T));
            for (size_t i = 0; i < sizeof(T) / sizeof(void *); ++i) {
                dest[i] = src[i];
            }
            ProtectRegion(dest, sizeof(T), originalProtection);
        }
    }

    void *HookVirtualMethod(void *instance, void *hook, size_t offset);

    template<typename T>
    void *HookVirtualMethod(void *instance, T hook, size_t offset) {
        return HookVirtualMethod(instance, TypeErase<T>(hook), offset);
    }
}

#endif // BML_HOOKUTILS_H
