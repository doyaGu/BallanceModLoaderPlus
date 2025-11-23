#ifndef BML_STRINGUTILS_H
#define BML_STRINGUTILS_H

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <locale>
#include <cctype>

namespace utils {
    // String splitting functions with multiple overloads
    template <typename StringT>
    std::vector<StringT> SplitString(const StringT &str, const StringT &delim) {
        std::vector<StringT> result;

        // Special case: empty input string should return empty vector
        if (str.empty()) {
            return result;
        }

        // Special case: empty delimiter
        if (delim.empty()) {
            result.push_back(str);
            return result;
        }

        typename StringT::size_type start = 0;
        typename StringT::size_type end;

        // Find each occurrence of delimiter and split
        while ((end = str.find(delim, start)) != StringT::npos) {
            // Add segment (even if empty)
            result.push_back(str.substr(start, end - start));
            start = end + delim.length();
        }

        // Add the last segment
        result.push_back(str.substr(start));

        return result;
    }

    // Overload for C-string delimiter
    template <typename StringT>
    std::vector<StringT> SplitString(const StringT &str, const typename StringT::value_type *delim) {
        return SplitString(str, StringT(delim));
    }

    // Overload for single character delimiter
    template <typename StringT>
    std::vector<StringT> SplitString(const StringT &str, typename StringT::value_type delim) {
        return SplitString(str, StringT(1, delim));
    }

    // String trimming functions
    template <typename StringT>
    void TrimString(StringT &s) {
        using CharT = typename StringT::value_type;
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](CharT c) { return !std::isspace(c, std::locale()); }));
        s.erase(std::find_if(s.rbegin(), s.rend(),
                             [](CharT c) { return !std::isspace(c, std::locale()); }).base(), s.end());
    }

    template <typename StringT>
    StringT TrimStringCopy(StringT s) {
        TrimString(s);
        return s;
    }

    // String joining functions with multiple overloads
    template <typename StringT>
    StringT JoinString(const std::vector<StringT> &strings, const StringT &delim) {
        if (strings.empty()) return StringT();

        StringT result = strings[0];
        for (size_t i = 1; i < strings.size(); ++i) {
            result += delim + strings[i];
        }
        return result;
    }

    // Overload for C-string delimiter
    template <typename StringT>
    StringT JoinString(const std::vector<StringT> &strings,
                       const typename StringT::value_type *delim) {
        return JoinString(strings, StringT(delim));
    }

    // Overload for single character delimiter
    template <typename StringT>
    StringT JoinString(const std::vector<StringT> &strings,
                       typename StringT::value_type delim) {
        return JoinString(strings, StringT(1, delim));
    }

    // Case conversion
    template <typename StringT>
    StringT ToLower(const StringT &s) {
        StringT result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](typename StringT::value_type c) {
                           return std::tolower(c, std::locale());
                       });
        return result;
    }

    template <typename StringT>
    StringT ToUpper(const StringT &s) {
        StringT result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](typename StringT::value_type c) {
                           return std::toupper(c, std::locale());
                       });
        return result;
    }

    // String comparison functions with multiple overloads
    template <typename StringT>
    bool StartsWith(const StringT &str, const StringT &prefix, bool caseSensitive = true) {
        if (str.size() < prefix.size()) return false;

        if (caseSensitive)
            return str.compare(0, prefix.size(), prefix) == 0;
        else
            return ToLower(str.substr(0, prefix.size())) == ToLower(prefix);
    }

    // Overload for C-string prefix
    template <typename StringT>
    bool StartsWith(const StringT &str, const typename StringT::value_type *prefix,
                    bool caseSensitive = true) {
        return StartsWith(str, StringT(prefix), caseSensitive);
    }

    template <typename StringT>
    bool EndsWith(const StringT &str, const StringT &suffix, bool caseSensitive = true) {
        if (str.size() < suffix.size()) return false;

        if (caseSensitive)
            return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
        else
            return ToLower(str.substr(str.size() - suffix.size())) == ToLower(suffix);
    }

    // Overload for C-string suffix
    template <typename StringT>
    bool EndsWith(const StringT &str, const typename StringT::value_type *suffix,
                  bool caseSensitive = true) {
        return EndsWith(str, StringT(suffix), caseSensitive);
    }

    template <typename StringT>
    bool Contains(const StringT &str, const StringT &substr, bool caseSensitive = true) {
        if (caseSensitive)
            return str.find(substr) != StringT::npos;
        else
            return ToLower(str).find(ToLower(substr)) != StringT::npos;
    }

    // Overload for C-string substring
    template <typename StringT>
    bool Contains(const StringT &str, const typename StringT::value_type *substr,
                  bool caseSensitive = true) {
        return Contains(str, StringT(substr), caseSensitive);
    }

    // String conversion declarations (implemented in .cpp file)
    std::wstring ToWString(const std::string &str, bool isUtf8 = true);
    std::string ToString(const std::wstring &wstr, bool toUtf8 = true);

    // Hash function
    template <typename CharT>
    size_t HashString(const CharT *str) {
        if (!str) return 0;

        // Start with a non-zero initial value (DJB2 hash)
        size_t hash = 5381;
        while (CharT ch = *str++) {
            hash = ((hash << 5) + hash) + ch; // hash * 33 + ch
        }
        return hash;
    }

    template <typename StringT>
    size_t HashString(const StringT &str) {
        // Start with a non-zero initial value (DJB2 hash)
        size_t hash = 5381;
        for (auto ch : str) {
            hash = ((hash << 5) + hash) + ch; // hash * 33 + ch
        }
        return hash;
    }

    std::string UnescapeString(const char *str);
    std::string EscapeString(const char *str);

    std::string StripAnsiCodes(const char *str);

    // Legacy compatibility functions
    inline bool StringStartsWith(const std::string &s1, const std::string &s2) {
        return StartsWith(s1, s2);
    }

    inline bool StringStartsWith(const std::string &s1, const char *s2) {
        return StartsWith(s1, s2);
    }

    inline bool StringEndsWith(const std::string &s1, const std::string &s2) {
        return EndsWith(s1, s2);
    }

    inline bool StringEndsWith(const std::string &s1, const char *s2) {
        return EndsWith(s1, s2);
    }

    inline bool StringStartsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        return StartsWith(s1, s2, false);
    }

    inline bool StringStartsWithCaseInsensitive(const std::string &s1, const char *s2) {
        return StartsWith(s1, s2, false);
    }

    inline bool StringEndsWithCaseInsensitive(const std::string &s1, const std::string &s2) {
        return EndsWith(s1, s2, false);
    }

    inline bool StringEndsWithCaseInsensitive(const std::string &s1, const char *s2) {
        return EndsWith(s1, s2, false);
    }

    // Wide string versions
    inline bool StringStartsWith(const std::wstring &s1, const std::wstring &s2) {
        return StartsWith(s1, s2);
    }

    inline bool StringStartsWith(const std::wstring &s1, const wchar_t *s2) {
        return StartsWith(s1, s2);
    }

    inline bool StringEndsWith(const std::wstring &s1, const std::wstring &s2) {
        return EndsWith(s1, s2);
    }

    inline bool StringEndsWith(const std::wstring &s1, const wchar_t *s2) {
        return EndsWith(s1, s2);
    }

    inline bool StringStartsWithCaseInsensitive(const std::wstring &s1, const std::wstring &s2) {
        return StartsWith(s1, s2, false);
    }

    inline bool StringStartsWithCaseInsensitive(const std::wstring &s1, const wchar_t *s2) {
        return StartsWith(s1, s2, false);
    }

    inline bool StringEndsWithCaseInsensitive(const std::wstring &s1, const std::wstring &s2) {
        return EndsWith(s1, s2, false);
    }

    inline bool StringEndsWithCaseInsensitive(const std::wstring &s1, const wchar_t *s2) {
        return EndsWith(s1, s2, false);
    }

    // Legacy conversion function compatibility
    inline std::wstring Utf8ToUtf16(const std::string &str) {
        return ToWString(str, true);
    }

    inline std::string Utf16ToUtf8(const std::wstring &wstr) {
        return ToString(wstr, true);
    }

    inline std::wstring AnsiToUtf16(const std::string &str) {
        return ToWString(str, false);
    }

    inline std::string Utf16ToAnsi(const std::wstring &wstr) {
        return ToString(wstr, false);
    }

    // C-style string overloads for backward compatibility
    inline std::wstring Utf8ToUtf16(const char *str) {
        return str ? ToWString(std::string(str), true) : std::wstring();
    }

    inline std::string Utf16ToUtf8(const wchar_t *wstr) {
        return wstr ? ToString(std::wstring(wstr), true) : std::string();
    }

    inline std::wstring AnsiToUtf16(const char *str) {
        return str ? ToWString(std::string(str), false) : std::wstring();
    }

    inline std::string Utf16ToAnsi(const wchar_t *wstr) {
        return wstr ? ToString(std::wstring(wstr), false) : std::string();
    }

    inline std::string Utf8ToAnsi(const char *str) {
        return str ? ToString(ToWString(std::string(str), true), false) : std::string();
    }

    enum CompareFlags : uint32_t {
        kNone                 = 0,
        kLinguisticIgnoreCase = 1u << 0,
        kIgnoreWidth          = 1u << 1,
        kDigitsAsNumbers      = 1u << 2
    };

    constexpr uint32_t kDefaultCompareFlags = kLinguisticIgnoreCase | kIgnoreWidth | kDigitsAsNumbers;

    int CompareString(const std::wstring &a, const std::wstring &b,
                      uint32_t flags = kDefaultCompareFlags, const std::wstring &localeName = std::wstring());

    int CompareString(const std::string &aUtf8, const std::string &bUtf8,
                      uint32_t flags = kDefaultCompareFlags, const std::wstring &localeName = std::wstring());
} // namespace utils

#endif // BML_STRINGUTILS_H
