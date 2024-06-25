#include "StringUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace utils {
    std::vector<std::string> SplitString(const std::string &str, const std::string &delim) {
        size_t lpos, pos = 0;
        std::vector<std::string> ret;

        lpos = str.find_first_not_of(delim, pos);
        while (lpos != std::string::npos) {
            pos = str.find_first_of(delim, lpos);
            ret.push_back(str.substr(lpos, pos - lpos));
            if (pos == std::string::npos)
                break;
            lpos = str.find_first_not_of(delim, pos);
        }

        if (pos != std::string::npos)
            ret.emplace_back("");

        return ret;
    }

    std::vector<std::wstring> SplitString(const std::wstring &str, const std::wstring &delim) {
        size_t lpos, pos = 0;
        std::vector<std::wstring> ret;

        lpos = str.find_first_not_of(delim, pos);
        while (lpos != std::wstring::npos) {
            pos = str.find_first_of(delim, lpos);
            ret.push_back(str.substr(lpos, pos - lpos));
            if (pos == std::wstring::npos)
                break;
            lpos = str.find_first_not_of(delim, pos);
        }

        if (pos != std::wstring::npos)
            ret.emplace_back(L"");

        return ret;
    }

    void TrimString(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](char x) { return !std::isspace(x); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](char x) { return !std::isspace(x); }).base(), s.end());
    }

    void TrimString(std::wstring &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wchar_t x) { return !std::iswspace(x); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](wchar_t x) { return !std::iswspace(x); }).base(), s.end());
    }

    std::string JoinString(const std::vector<std::string> &str, const std::string &delim) {
        std::string ret;
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i].empty())
                continue;
            ret += str[i];
            if (i < str.size() - 1)
                ret += delim;
        }
        return ret;
    }

    std::wstring JoinString(const std::vector<std::wstring> &str, const std::wstring &delim) {
        std::wstring ret;
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i].empty())
                continue;
            ret += str[i];
            if (i < str.size() - 1)
                ret += delim;
        }
        return ret;
    }

    std::string JoinString(const std::vector<std::string> &str, char delim) {
        return JoinString(str, std::string(1, delim));
    }

    std::wstring JoinString(const std::vector<std::wstring> &str, wchar_t delim) {
        return JoinString(str, std::wstring(1, delim));
    }

    bool StringStartsWith(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(0, s2.size(), s2) == 0;
        }
        return false;
    }

    bool StringStartsWith(const std::wstring &s1, const std::wstring &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(0, s2.size(), s2) == 0;
        }
        return false;
    }

    bool StringStartsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return _strnicmp(s1.c_str(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }

    bool StringStartsWithCaseInsensitive(const std::wstring &s1, const std::wstring &s2) {
        if (s1.size() >= s2.size()) {
            return wcsncmp(s1.c_str(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }

    bool StringEndsWith(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(s1.size() - s2.size(), s2.size(), s2) == 0;
        }
        return false;
    }

    bool StringEndsWith(const std::wstring &s1, const std::wstring &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(s1.size() - s2.size(), s2.size(), s2) == 0;
        }
        return false;
    }

    bool StringEndsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return _strnicmp(s1.c_str() + s1.size() - s2.size(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }

    bool StringEndsWithCaseInsensitive(const std::wstring &s1, const std::wstring &s2) {
        if (s1.size() >= s2.size()) {
            return _wcsnicmp(s1.c_str() + s1.size() - s2.size(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }

    int AnsiToUtf16(const char *charStr, wchar_t *wcharStr, int size) {
        return ::MultiByteToWideChar(CP_ACP, 0, charStr, -1, wcharStr, size);
    }

    int Utf16ToAnsi(const wchar_t *wcharStr, char *charStr, int size) {
        return ::WideCharToMultiByte(CP_ACP, 0, wcharStr, -1, charStr, size, nullptr, nullptr);
    }

    int Utf16ToUtf8(const wchar_t *wcharStr, char *charStr, int size) {
        return ::WideCharToMultiByte(CP_UTF8, 0, wcharStr, -1, charStr, size, nullptr, nullptr);
    }

    int Utf8ToUtf16(const char *charStr, wchar_t *wcharStr, int size) {
        return ::MultiByteToWideChar(CP_UTF8, 0, charStr, -1, wcharStr, size);
    }

    std::wstring AnsiToUtf16(const char *str) {
        if (!str || str[0] == '\0')
            return L"";

        size_t len = strlen(str);
        int size = ::MultiByteToWideChar(CP_ACP, 0, str, (int) len, nullptr, 0);
        std::wstring wstr(size, 0);
        ::MultiByteToWideChar(CP_ACP, 0, str, (int) len, &wstr[0], size);
        return wstr;
    }

    std::string Utf16ToAnsi(const wchar_t *wstr) {
        if (!wstr || wstr[0] == '\0')
            return "";

        size_t len = wcslen(wstr);
        int size = ::WideCharToMultiByte(CP_ACP, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        ::WideCharToMultiByte(CP_ACP, 0, wstr, (int) len, &str[0], size, nullptr, nullptr);
        return str;
    }

    std::wstring Utf8ToUtf16(const char *str) {
        if (!str || str[0] == '\0')
            return L"";

        size_t len = strlen(str);
        int size = ::MultiByteToWideChar(CP_UTF8, 0, str, (int) len, nullptr, 0);
        std::wstring wstr(size, 0);
        ::MultiByteToWideChar(CP_UTF8, 0, str, (int) len, &wstr[0], size);
        return wstr;
    }

    std::string Utf16ToUtf8(const wchar_t *wstr) {
        if (!wstr || wstr[0] == '\0')
            return "";

        size_t len = wcslen(wstr);
        int size = ::WideCharToMultiByte(CP_UTF8, 0, wstr, (int) len, nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, wstr, (int) len, &str[0], size, nullptr, nullptr);
        return str;
    }

    std::wstring AnsiToUtf16(const std::string &str) {
        if (str.empty())
            return L"";

        int size = ::MultiByteToWideChar(CP_ACP, 0, &str[0], (int) str.size(), nullptr, 0);
        std::wstring wstr(size, 0);
        ::MultiByteToWideChar(CP_ACP, 0, &str[0], (int) str.size(), &wstr[0], size);
        return wstr;
    }

    std::string Utf16ToAnsi(const std::wstring &wstr) {
        if (wstr.empty())
            return "";

        int size = ::WideCharToMultiByte(CP_ACP, 0, &wstr[0], -1, nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        ::WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int) wstr.size(), &str[0], size, nullptr, nullptr);
        return str;
    }

    std::wstring Utf8ToUtf16(const std::string &str) {
        if (str.empty())
            return L"";

        int size = ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int) str.size(), nullptr, 0);
        std::wstring wstr(size, 0);
        ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int) str.size(), &wstr[0], size);
        return wstr;
    }

    std::string Utf16ToUtf8(const std::wstring &wstr) {
        if (wstr.empty())
            return "";

        int size = ::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), &str[0], size, nullptr, nullptr);
        return str;
    }
}
