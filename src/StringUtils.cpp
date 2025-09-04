#include "StringUtils.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <codecvt>
#endif


namespace {
    int HexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    bool IsValidCodePoint(unsigned int cp) {
        // Valid range per Unicode/UTF-8: <= 0x10FFFF and not a surrogate.
        if (cp > 0x10FFFF) return false;                // RFC 3629 limit
        if (cp >= 0xD800 && cp <= 0xDFFF) return false; // surrogate range
        return true;
    }

    void AppendUTF8(std::string &out, unsigned int cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            // cp <= 0x10FFFF
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
}

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

    std::string UnescapeString(const char *str) {
        if (!str) return "";

        std::string result;
        result.reserve(std::strlen(str));

        const char *p = str;
        while (*p) {
            if (*p == '\\' && *(p + 1)) {
                ++p; // Skip backslash
                switch (*p) {
                    case 'a': result += '\a'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'v': result += '\v'; break;
                    case '\\': result += '\\'; break;
                    case '\'': result += '\''; break;
                    case '"': result += '"'; break;
                    case '?': result += '?'; break;
                    case 'e': result += '\033'; break; // ESC character

                    // Octal escape: up to 3 octal digits (includes \0)
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7': {
                        unsigned int value = 0;
                        int digits = 0;
                        while (digits < 3 && *p >= '0' && *p <= '7') {
                            value = (value * 8) + static_cast<unsigned int>(*p - '0');
                            ++p; ++digits;
                        }
                        result += static_cast<char>(value & 0xFF);
                        --p; // will be incremented below
                        break;
                    }

                    // Hex escape: \xHH (unlimited length as per C standard)
                    case 'x': {
                        ++p;
                        unsigned int value = 0;
                        int digits = 0, hv;
                        while ((hv = HexVal(*p)) != -1) {
                            value = (value << 4) + static_cast<unsigned int>(hv);
                            ++p; ++digits;
                        }
                        if (digits > 0) {
                            result += static_cast<char>(value & 0xFF);
                            --p;
                        } else {
                            result += '\\'; result += 'x'; --p;
                        }
                        break;
                    }

                    // Unicode escapes: \uXXXX (4 hex) and \UXXXXXXXX (8 hex)
                    case 'u':
                    case 'U': {
                        const char kind = *p;
                        const int need = (kind == 'u') ? 4 : 8;
                        ++p;
                        unsigned int cp = 0;
                        int digits = 0, hv;
                        const char *digits_begin = p;

                        while (digits < need && (hv = HexVal(*p)) != -1) {
                            cp = (cp << 4) + static_cast<unsigned int>(hv);
                            ++p; ++digits;
                        }

                        if (digits == need) {
                            // Handle surrogate pairs for \u
                            if (kind == 'u' && cp >= 0xD800 && cp <= 0xDBFF) {
                                // High surrogate, check for low surrogate
                                const char* save_pos = p;
                                if (*p == '\\' && *(p + 1) == 'u') {
                                    p += 2;
                                    unsigned int lo = 0;
                                    int lo_digits = 0;
                                    while (lo_digits < 4 && (hv = HexVal(*p)) != -1) {
                                        lo = (lo << 4) + static_cast<unsigned int>(hv);
                                        ++p; ++lo_digits;
                                    }
                                    if (lo_digits == 4 && lo >= 0xDC00 && lo <= 0xDFFF) {
                                        // Valid surrogate pair
                                        unsigned int code = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                        AppendUTF8(result, code);
                                        --p;
                                        break;
                                    }
                                }
                                // Not a valid surrogate pair, restore position
                                p = save_pos;
                            }

                            if (IsValidCodePoint(cp)) {
                                AppendUTF8(result, cp);
                                --p;
                            } else {
                                // Invalid code point, emit literally
                                result += '\\'; result += kind;
                                while (digits_begin < p) { result += *digits_begin++; }
                                --p;
                            }
                        } else {
                            // Incomplete sequence, emit literally
                            result += '\\'; result += kind;
                            while (digits_begin < p) { result += *digits_begin++; }
                            --p;
                        }
                        break;
                    }

                    default:
                        // Unknown escape: keep backslash and char
                        result += '\\';
                        result += *p;
                        break;
                }
                ++p;
            } else {
                result += *p++;
            }
        }

        return result;
    }

    std::string EscapeString(const char *str) {
        // Inverse of EscapeString: encode control chars, quotes, backslash,
        // and all non-ASCII as C/Unicode escapes. Avoid \x to prevent
        // ambiguous concatenation with following hex digits.
        if (!str) return "";

        const uint8_t *p = reinterpret_cast<const uint8_t *>(str);
        const uint8_t *end = p + std::strlen(str);

        std::string out;
        out.reserve(end - p);

        // Append \uXXXX or \UXXXXXXXX (uppercase hex, fixed width).
        auto appendU = [&](unsigned int cp) {
            if (cp <= 0xFFFF) {
                out += "\\u";
                for (int i = 3; i >= 0; --i) {
                    unsigned v = (cp >> (i * 4)) & 0xF;
                    out += (v < 10) ? char('0' + v) : char('A' + (v - 10));
                }
            } else {
                out += "\\U";
                for (int i = 7; i >= 0; --i) {
                    unsigned v = (cp >> (i * 4)) & 0xF;
                    out += (v < 10) ? char('0' + v) : char('A' + (v - 10));
                }
            }
        };

        // Emit ASCII with simple escapes when available; otherwise \u00XX.
        auto escapeAscii = [&](uint8_t c) {
            switch (c) {
            case '\a': out += "\\a";
                return;
            case '\b': out += "\\b";
                return;
            case '\f': out += "\\f";
                return;
            case '\n': out += "\\n";
                return;
            case '\r': out += "\\r";
                return;
            case '\t': out += "\\t";
                return;
            case '\v': out += "\\v";
                return;
            case '\\': out += "\\\\";
                return;
            case '\"': out += "\\\"";
                return;
            case '\'': out += "\\\'";
                return;
            case '\033': out += "\\e";
                return; // ESC, matches EscapeString
            default:
                if (c < 0x20 || c == 0x7F) {
                    // Use \u00XX to avoid \x... ambiguity.
                    out += "\\u00";
                    unsigned hi = (c >> 4) & 0xF, lo = c & 0xF;
                    out += (hi < 10) ? char('0' + hi) : char('A' + (hi - 10));
                    out += (lo < 10) ? char('0' + lo) : char('A' + (lo - 10));
                } else {
                    out += char(c);
                }
            }
        };

        // UTF-8 decode (RFC 3629; 1–4 bytes; disallow surrogates/overlong).
        auto decodeUtf8 = [&](const uint8_t *s, const uint8_t *e, unsigned int &cp, int &len) -> bool {
            if (s >= e) return false;
            uint8_t c0 = s[0];
            if (c0 < 0x80) {
                cp = c0;
                len = 1;
                return true;
            }

            auto need_cont = [&](int i) {
                return (s + i < e) && (s[i] >= 0x80) && (s[i] <= 0xBF);
            };

            if (c0 >= 0xC2 && c0 <= 0xDF) {
                // 2-byte
                if (!need_cont(1)) return false;
                cp = ((c0 & 0x1F) << 6) | (s[1] & 0x3F);
                len = 2;
                return true;
            } else if (c0 >= 0xE0 && c0 <= 0xEF) {
                // 3-byte
                if (!need_cont(1) || !need_cont(2)) return false;
                uint8_t c1 = s[1];
                if (c0 == 0xE0 && c1 < 0xA0) return false; // overlong
                if (c0 == 0xED && c1 > 0x9F) return false; // surrogate range
                cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (s[2] & 0x3F);
                len = 3;
                return true;
            } else if (c0 >= 0xF0 && c0 <= 0xF4) {
                // 4-byte (<= U+10FFFF)
                if (!need_cont(1) || !need_cont(2) || !need_cont(3)) return false;
                uint8_t c1 = s[1];
                if (c0 == 0xF0 && c1 < 0x90) return false; // overlong
                if (c0 == 0xF4 && c1 > 0x8F) return false; // > U+10FFFF
                cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12)
                    | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
                len = 4;
                return true;
            }
            return false;
        };

        while (p < end) {
            if (*p < 0x80) {
                escapeAscii(*p++);
            } else {
                unsigned int cp = 0;
                int n = 0;
                if (decodeUtf8(p, end, cp, n)) {
                    appendU(cp);
                    p += n;
                } else {
                    // Invalid UTF-8: preserve byte exactly.
                    appendU(*p);
                    ++p;
                }
            }
        }
        return out;
    }

    std::string StripAnsiCodes(const char *str) {
        if (!str) return "";

        auto is_final_byte = [](unsigned char c) {
            return c >= 0x40 && c <= 0x7E; // ECMA-48 final byte
        };

        std::string out;
        out.reserve(std::strlen(str));

        const unsigned char *p = reinterpret_cast<const unsigned char *>(str);
        while (*p) {
            unsigned char c = *p;
            if (c == 0x1B) {
                // ESC ...
                if (!p[1]) break; // dangling ESC at end
                unsigned char a = p[1];

                // CSI: ESC [ ... final
                if (a == '[') {
                    p += 2;
                    // parameters (0x30-0x3F), intermediates (0x20-0x2F) then final (0x40-0x7E)
                    while (*p && !is_final_byte(*p)) ++p;
                    if (*p) ++p; // consume final
                    continue;
                }

                // OSC: ESC ] ... BEL (0x07) or ST (ESC \\)
                if (a == ']') {
                    p += 2;
                    while (*p) {
                        if (*p == 0x07) { ++p; break; } // BEL terminator
                        if (*p == 0x1B && p[1] == '\\') { p += 2; break; } // ST
                        ++p;
                    }
                    continue;
                }

                // DCS, SOS, PM, APC: ESC P/X/^/_ ... ST (ESC \\)
                if (a == 'P' || a == 'X' || a == '^' || a == '_') {
                    p += 2;
                    while (*p) {
                        if (*p == 0x1B && p[1] == '\\') { p += 2; break; }
                        ++p;
                    }
                    continue;
                }

                // Two-byte control functions frequently used
                if (a == 'N' || a == 'O' || a == 'c' || a == '7' || a == '8' || a == '=' || a == '>' ||
                    a == 'D' || a == 'E' || a == 'H' || a == 'M' || a == 'Z') {
                    p += 2; // drop ESC <char>
                    continue;
                }

                // Designate, charset, S7C1T/S8C1T, DEC line attributes: ESC <prefix> <final>
                if (a == '#' || a == '(' || a == ')' || a == '*' || a == '+' || a == '-' || a == '.' || a == ' ') {
                    // Consume ESC, prefix, and at most one following byte if present
                    if (p[2]) p += 3; else p += 2;
                    continue;
                }

                // Fallback: unknown ESC sequence, drop ESC and next byte if present
                p += p[1] ? 2 : 1;
                continue;
            }

            // 8-bit C1 control codes (ECMA-48)
            if (c == 0x9B) { // CSI
                ++p;
                while (*p && !is_final_byte(*p)) ++p;
                if (*p) ++p;
                continue;
            } else if (c == 0x9D) { // OSC (String to BEL or ST)
                ++p;
                while (*p) {
                    if (*p == 0x9C || *p == 0x07) { ++p; break; } // ST or BEL
                    if (*p == 0x1B && p[1] == '\\') { p += 2; break; }
                    ++p;
                }
                continue;
            } else if (c == 0x90) { // DCS ... ST
                ++p;
                while (*p) {
                    if (*p == 0x9C) { ++p; break; }
                    if (*p == 0x1B && p[1] == '\\') { p += 2; break; }
                    ++p;
                }
                continue;
            } else if (c == 0x98 || c == 0x9E || c == 0x9F) { // SOS, PM, APC ... ST
                ++p;
                while (*p) {
                    if (*p == 0x9C) { ++p; break; }
                    if (*p == 0x1B && p[1] == '\\') { p += 2; break; }
                    ++p;
                }
                continue;
            }

            // Default: copy byte as-is
            out.push_back(static_cast<char>(c));
            ++p;
        }
        return out;
    }

    DWORD MapFlags(uint32_t f) {
        DWORD w = 0;
        if (f & kLinguisticIgnoreCase) w |= LINGUISTIC_IGNORECASE;
        if (f & kIgnoreWidth) w |= NORM_IGNOREWIDTH;
        if (f & kDigitsAsNumbers) w |= SORT_DIGITSASNUMBERS;
        return w;
    }

    inline LPCWSTR ToLocaleName(const std::wstring &name) {
        return name.empty() ? LOCALE_NAME_USER_DEFAULT : name.c_str();
    }

    inline int ToTri(const int cstr) {
        if (cstr == 0) return 0;
        return cstr - 2;
    }

#undef CompareString

    int CompareString(const std::wstring &a, const std::wstring &b,
                      uint32_t flags, const std::wstring &localeName) {
        const int r = ::CompareStringEx(ToLocaleName(localeName), MapFlags(flags),
            a.c_str(), -1,
            b.c_str(), -1,
            nullptr, nullptr, 0);
        if (r == 0) {
            return (a < b) ? -1 : (a > b ? 1 : 0);
        }
        return ToTri(r);
    }

    int CompareString(const std::string &aUtf8, const std::string &bUtf8,
                      uint32_t flags, const std::wstring &localeName) {
        const std::wstring wa = Utf8ToUtf16(aUtf8);
        const std::wstring wb = Utf8ToUtf16(bUtf8);

        const int r = ::CompareStringEx(ToLocaleName(localeName), MapFlags(flags),
            wa.c_str(), -1,
            wb.c_str(), -1,
            nullptr, nullptr, 0);
        if (r == 0) {
            return (aUtf8 < bUtf8) ? -1 : (aUtf8 > bUtf8 ? 1 : 0);
        }
        return ToTri(r);
    }
}
