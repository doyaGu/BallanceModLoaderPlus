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

    std::string JoinString(const std::vector<std::string> &str, char delim) {
        return JoinString(str, std::string(1, delim));
    }

    bool StringStartsWith(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(0, s2.size(), s2) == 0;
        }
        return false;
    }

    bool StringStartsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return strnicmp(s1.c_str(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }

    bool StringEndsWith(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(s1.size() - s2.size(), s2.size(), s2) == 0;
        }
        return false;
    }

    bool StringEndsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return strnicmp(s1.c_str() + s1.size() - s2.size(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }

    int AnsiToUtf16(const char *charStr, wchar_t *wcharStr, int size) {
        return ::MultiByteToWideChar(CP_ACP, 0, charStr, -1, wcharStr, size);
    }

    int Utf16ToAnsi(const wchar_t *wcharStr, char *charStr, int size) {
        return ::WideCharToMultiByte(CP_ACP, 0, wcharStr, -1, charStr, size, NULL, NULL);
    }
}
