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
