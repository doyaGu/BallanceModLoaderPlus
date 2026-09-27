#include "Api/Behavior/Text.h"

#include <climits>
#include <cstdint>

#include <windows.h>

namespace BML::Api::Behavior {
namespace {

bool IsAscii(std::string_view text) noexcept {
    for (const char byte : text) {
        if (static_cast<unsigned char>(byte) >= 0x80u)
            return false;
    }
    return true;
}

bool Widen(std::string_view text, unsigned codePage, DWORD flags,
           std::wstring &out) {
    const int size = static_cast<int>(text.size());
    const int wideSize = MultiByteToWideChar(
        codePage, flags, text.data(), size, nullptr, 0);
    if (wideSize <= 0)
        return false;
    out.assign(static_cast<std::size_t>(wideSize), L'\0');
    return MultiByteToWideChar(codePage, flags, text.data(), size,
                               out.data(), wideSize) == wideSize;
}

// An exact conversion fails instead of substituting a best-fit or default
// character. UTF-8 accepts neither check, and needs neither.
bool Narrow(const std::wstring &wide, unsigned codePage, bool exact,
            std::string &out) {
    const int wideSize = static_cast<int>(wide.size());
    const DWORD flags = exact ? WC_NO_BEST_FIT_CHARS : 0;
    BOOL usedDefault = FALSE;
    BOOL *defaulted = exact ? &usedDefault : nullptr;
    const int size = WideCharToMultiByte(
        codePage, flags, wide.data(), wideSize, nullptr, 0, nullptr, defaulted);
    if (size <= 0 || usedDefault)
        return false;
    out.assign(static_cast<std::size_t>(size), '\0');
    return WideCharToMultiByte(codePage, flags, wide.data(), wideSize,
                               out.data(), size, nullptr, defaulted) == size &&
        !usedDefault;
}

} // namespace

bool IsUtf8(const char *data, std::size_t size) noexcept {
    if (!data && size)
        return false;
    for (std::size_t index = 0; index < size;) {
        const unsigned char first = static_cast<unsigned char>(data[index++]);
        if (first < 0x80)
            continue;
        unsigned continuation = 0;
        std::uint32_t codepoint = 0;
        if ((first & 0xe0u) == 0xc0u) {
            continuation = 1;
            codepoint = first & 0x1fu;
            if (codepoint < 2)
                return false;
        } else if ((first & 0xf0u) == 0xe0u) {
            continuation = 2;
            codepoint = first & 0x0fu;
        } else if ((first & 0xf8u) == 0xf0u) {
            continuation = 3;
            codepoint = first & 0x07u;
            if (codepoint > 4)
                return false;
        } else {
            return false;
        }
        if (continuation > size - index)
            return false;
        for (unsigned part = 0; part < continuation; ++part) {
            const unsigned char byte = static_cast<unsigned char>(data[index++]);
            if ((byte & 0xc0u) != 0x80u)
                return false;
            codepoint = (codepoint << 6) | (byte & 0x3fu);
        }
        if ((continuation == 2 && codepoint < 0x800u) ||
            (continuation == 3 && codepoint < 0x10000u) ||
            codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu))
            return false;
    }
    return true;
}

std::string NativeText(std::string_view utf8, unsigned codePage) {
    if (IsAscii(utf8) || utf8.size() > static_cast<std::size_t>(INT_MAX))
        return std::string(utf8);
    const unsigned page = codePage == ActiveCodePage ? GetACP() : codePage;
    std::wstring wide;
    std::string native;
    if (page == CP_UTF8 ||
        !Widen(utf8, CP_UTF8, MB_ERR_INVALID_CHARS, wide) ||
        !Narrow(wide, page, true, native))
        return std::string(utf8);
    return native;
}

std::string_view Utf8Text(std::string_view native, std::string &storage,
                          unsigned codePage) {
    if (IsUtf8(native.data(), native.size()))
        return native;
    std::wstring wide;
    if (native.size() > static_cast<std::size_t>(INT_MAX) ||
        !Widen(native, codePage == ActiveCodePage ? CP_ACP : codePage, 0,
               wide) ||
        !Narrow(wide, CP_UTF8, false, storage)) {
        storage.clear();
    }
    return storage;
}

} // namespace BML::Api::Behavior
