#include "StringUtils.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <codecvt>
#endif

namespace utils {
#ifdef _WIN32
    // Windows implementation using Windows API
    std::wstring ToWString(const std::string &str, bool isUtf8) {
        if (str.empty()) return L"";

        int codePage = isUtf8 ? CP_UTF8 : CP_ACP;
        int size = MultiByteToWideChar(codePage, 0, str.c_str(),
                                       static_cast<int>(str.size()), nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(codePage, 0, str.c_str(),
                            static_cast<int>(str.size()), &result[0], size);
        return result;
    }

    std::string ToString(const std::wstring &wstr, bool toUtf8) {
        if (wstr.empty()) return "";

        int codePage = toUtf8 ? CP_UTF8 : CP_ACP;
        int size = WideCharToMultiByte(codePage, 0, wstr.c_str(),
                                       static_cast<int>(wstr.size()),
                                       nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(codePage, 0, wstr.c_str(),
                            static_cast<int>(wstr.size()),
                            &result[0], size, nullptr, nullptr);
        return result;
    }
#else
    // Cross-platform implementation using C++11 std::wstring_convert
    std::wstring ToWString(const std::string& str, bool isUtf8) {
        if (str.empty()) return L"";

        if (isUtf8) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.from_bytes(str);
        } else {
            // For non-UTF8, use locale-dependent conversion
            std::wstring result;
            result.reserve(str.size());
            std::locale loc("");
            for (char c : str) {
                result.push_back(std::use_facet<std::ctype<wchar_t>>(loc).widen(c));
            }
            return result;
        }
    }

    std::string ToString(const std::wstring& wstr, bool toUtf8) {
        if (wstr.empty()) return "";

        if (toUtf8) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.to_bytes(wstr);
        } else {
            // For non-UTF8, use locale-dependent conversion
            std::string result;
            result.reserve(wstr.size());
            std::locale loc("");
            for (wchar_t c : wstr) {
                result.push_back(std::use_facet<std::ctype<wchar_t>>(loc).narrow(c, '?'));
            }
            return result;
        }
    }
#endif
}
