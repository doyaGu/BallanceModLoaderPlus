#include "HookUtils.h"
#include "StringUtils.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace utils {
void OutputDebugA(const char *format, ...) {
    if (!format) return;
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}
void OutputDebugW(const wchar_t *format, ...) {
    if (!format) return;
    std::vector<wchar_t> buffer(512);
    for (;;) {
        va_list args;
        va_start(args, format);
        const int length = std::vswprintf(buffer.data(), buffer.size(), format, args);
        va_end(args);
        if (length >= 0 && static_cast<size_t>(length) < buffer.size()) {
            std::fputs(Utf16ToUtf8(std::wstring(buffer.data(), length)).c_str(), stderr);
            return;
        }
        if (buffer.size() >= 65536) return;
        buffer.resize(buffer.size() * 2);
    }
}
}
