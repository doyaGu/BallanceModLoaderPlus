#ifndef BML_STRINGUTILS_H
#define BML_STRINGUTILS_H

#include <string>
#include <vector>

namespace utils {
    std::vector<std::string> SplitString(const std::string &str, const std::string &delim);
    std::vector<std::wstring> SplitString(const std::wstring &str, const std::wstring &delim);

    void TrimString(std::string &s);
    void TrimString(std::wstring &s);

    std::string JoinString(const std::vector<std::string> &str, const std::string &delim);
    std::wstring JoinString(const std::vector<std::wstring> &str, const std::wstring &delim);
    std::string JoinString(const std::vector<std::string> &str, char delim);
    std::wstring JoinString(const std::vector<std::wstring> &str, wchar_t delim);

    bool StringStartsWith(const std::string &s1, const std::string &s2);
    bool StringStartsWith(const std::wstring &s1, const std::wstring &s2);
    bool StringStartsWithCaseInsensitive(const std::string &s1, const std::string &s2);
    bool StringStartsWithCaseInsensitive(const std::wstring &s1, const std::wstring &s2);

    bool StringEndsWith(const std::string &s1, const std::string &s2);
    bool StringEndsWith(const std::wstring &s1, const std::wstring &s2);
    bool StringEndsWithCaseInsensitive(const std::string &s1, const std::string &s2);
    bool StringEndsWithCaseInsensitive(const std::wstring &s1, const std::wstring &s2);

    int AnsiToUtf16(const char *charStr, wchar_t *wcharStr, int size);
    int Utf16ToAnsi(const wchar_t *wcharStr, char *charStr, int size);
    int Utf16ToUtf8(const wchar_t *wcharStr, char *charStr, int size);
    int Utf8ToUtf16(const char *charStr, wchar_t *wcharStr, int size);

    std::wstring AnsiToUtf16(const char *str);
    std::string Utf16ToAnsi(const wchar_t *wstr);
    std::wstring Utf8ToUtf16(const char *str);
    std::string Utf16ToUtf8(const wchar_t *wstr);

    std::wstring AnsiToUtf16(const std::string &str);
    std::string Utf16ToAnsi(const std::wstring &wstr);
    std::wstring Utf8ToUtf16(const std::string &str);
    std::string Utf16ToUtf8(const std::wstring &wstr);

    // BKDR hash
    inline size_t HashString(const char *str) {
        size_t hash = 0;
        if (str) {
            while (char ch = *str++) {
                hash = hash * 131 + ch;
            }
        }
        return hash;
    }

    inline size_t HashString(const wchar_t *str) {
        size_t hash = 0;
        if (str) {
            while (wchar_t ch = *str++) {
                hash = hash * 131 + ch;
            }
        }
        return hash;
    }
}

#endif // BML_STRINGUTILS_H
