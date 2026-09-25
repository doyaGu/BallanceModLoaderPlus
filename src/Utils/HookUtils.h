#ifndef BML_HOOKUTILS_H
#define BML_HOOKUTILS_H

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

    inline void **GetVTable(void *instance) {
        if (instance) {
            return *static_cast<void ***>(instance);
        } else {
            return nullptr;
        }
    }

}

#endif // BML_HOOKUTILS_H
