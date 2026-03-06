#ifndef BML_CORE_PLATFORM_COMPAT_H
#define BML_CORE_PLATFORM_COMPAT_H

#include <cstdint>
#include <cstdio>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
using HMODULE = void *;
using DWORD = std::uint32_t;

inline void OutputDebugStringA(const char *message) {
    if (!message) {
        return;
    }
    std::fputs(message, stderr);
    std::fflush(stderr);
}
#endif

#endif // BML_CORE_PLATFORM_COMPAT_H
