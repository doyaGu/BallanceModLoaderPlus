#include "Engine/Text.h"

#include <climits>
#include "StringUtils.h"

#if !BML_RUNTIME_SDL
#include <windows.h>
#endif

namespace BML::CKText {

#if !BML_RUNTIME_SDL
static bool IsAscii(std::string_view text) noexcept {
    for (const char byte : text) {
        if (static_cast<unsigned char>(byte) >= 0x80u)
            return false;
    }
    return true;
}

static bool Widen(std::string_view text, unsigned codePage, DWORD flags,
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
static bool Narrow(const std::wstring &wide, unsigned codePage, bool exact,
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

#endif

#if BML_RUNTIME_SDL
bool EncodeNativeText(std::string_view utf8, std::string &native, unsigned codePage) {
    (void) codePage;
    if (!utils::IsValidUtf8(utf8))
        return false;
    native.assign(utf8);
    return true;
}

std::string_view Utf8Text(std::string_view native, std::string &storage,
                          unsigned codePage) {
    (void) codePage;
    if (utils::IsValidUtf8(native)) return native;
    storage.clear();
    return storage;
}

#else
bool EncodeNativeText(std::string_view utf8, std::string &native, unsigned codePage) {
    const unsigned page = codePage == ActiveCodePage ? GetACP() : codePage;
    if (IsAscii(utf8) || page == CP_UTF8) {
        if (!utils::IsValidUtf8(utf8))
            return false;
        native.assign(utf8);
        return true;
    }
    if (utf8.size() > static_cast<std::size_t>(INT_MAX))
        return false;
    std::wstring wide;
    std::string converted;
    if (!Widen(utf8, CP_UTF8, MB_ERR_INVALID_CHARS, wide) ||
        !Narrow(wide, page, true, converted))
        return false;
    native.swap(converted);
    return true;
}

std::string_view Utf8Text(std::string_view native, std::string &storage,
                          unsigned codePage) {
    if (IsAscii(native))
        return native;
    const unsigned page = codePage == ActiveCodePage ? GetACP() : codePage;
    if (page == CP_UTF8 && utils::IsValidUtf8(native))
        return native;
    std::wstring wide;
    if (native.size() > static_cast<std::size_t>(INT_MAX) ||
        !Widen(native, page, MB_ERR_INVALID_CHARS, wide) ||
        !Narrow(wide, CP_UTF8, false, storage)) {
        storage.clear();
    }
    return storage;
}

#endif

} // namespace BML::CKText
