#include "SdkUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "PathUtils.h"

namespace utils {

BMLVersion ParseVersion(const char *value) {
    if (!value || !*value)
        return BMLVersion(0, 0, 0);

    int parts[3] = {0, 0, 0};
    int n = 0;
    const char *p = value;

    // Scan digits, allow arbitrary separators/suffix after a part.
    while (*p && n < 3) {
        // Skip non-digits until we find a digit
        while (*p && (*p < '0' || *p > '9')) ++p;
        if (!*p)
            break;

        // Accumulate one integer part
        int val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            ++p;
        }
        parts[n++] = val;

        // Skip until next digit or end (tolerate '.', '-', '+', etc.)
        while (*p && (*p < '0' || *p > '9')) ++p;
    }

    return BMLVersion(parts[0], parts[1], parts[2]);
}

void *GetModuleFromAddress(const void *address) {
    if (!address)
        return nullptr;

    HMODULE module = nullptr;
    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(address),
                              &module)) {
        return nullptr;
    }

    return module;
}

std::wstring GetModuleDirectory(void *module) {
    if (!module)
        return {};

    std::wstring path(260, L'\0');
    for (;;) {
        const DWORD written = ::GetModuleFileNameW(static_cast<HMODULE>(module), path.data(),
                                                   static_cast<DWORD>(path.size()));
        if (written == 0)
            return {};
        if (written < path.size()) {
            path.resize(written);
            break;
        }
        if (path.size() >= 32768)
            return {};
        path.resize(path.size() * 2);
    }

    return GetParentDirectoryW(path);
}

std::wstring GetModuleDirectoryFromAddress(const void *address) {
    return GetModuleDirectory(GetModuleFromAddress(address));
}

} // namespace utils
