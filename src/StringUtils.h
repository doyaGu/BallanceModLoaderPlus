#ifndef BML_STRINGUTILS_H
#define BML_STRINGUTILS_H

#include <string>
#include <vector>

namespace utils {
    std::vector<std::string> SplitString(const std::string &str, const std::string &delim);

    std::string JoinString(const std::vector<std::string> &str, const std::string &delim);
    std::string JoinString(const std::vector<std::string> &str, char delim);

    bool StringStartsWith(const std::string &s1, const std::string &s2);
    bool StringStartsWithCaseInsensitive(const std::string &s1, const std::string &s2);

    bool StringEndsWith(const std::string &s1, const std::string &s2);
    bool StringEndsWithCaseInsensitive(const std::string &s1, const std::string &s2);

    int AnsiToUtf16(const char *charStr, wchar_t *wcharStr, int size);
    int Utf16ToAnsi(const wchar_t *wcharStr, char *charStr, int size);

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
}

#endif // BML_STRINGUTILS_H
