#ifndef BML_STRINGUTILS_H
#define BML_STRINGUTILS_H

#include <string>
#include <vector>

namespace utils {
    inline std::vector<std::string> Split(const std::string &str, const std::string &delim) {
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

    inline std::string Join(const std::vector<std::string> &str, const std::string &delim) {
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

    inline std::string Join(const std::vector<std::string> &str, char delim) {
        return Join(str, std::string(1, delim));
    }

    inline bool StartsWith(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(0, s2.size(), s2) == 0;
        }
        return false;
    }

    inline bool StartsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return strnicmp(s1.c_str(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }

    inline bool EndsWith(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return s1.compare(s1.size() - s2.size(), s2.size(), s2) == 0;
        }
        return false;
    }

    inline bool EndsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        if (s1.size() >= s2.size()) {
            return strnicmp(s1.c_str() + s1.size() - s2.size(), s2.c_str(), s2.size()) == 0;
        }
        return false;
    }
}

#endif // BML_STRINGUTILS_H
