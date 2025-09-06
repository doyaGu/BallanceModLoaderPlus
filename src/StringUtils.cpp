#include "StringUtils.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <codecvt>
#endif

#include <utf8.h>

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
        char buf[4];
        const size_t n = utf8codepointsize(static_cast<utf8_int32_t>(cp));
        utf8catcodepoint(reinterpret_cast<utf8_int8_t*>(buf), static_cast<utf8_int32_t>(cp), n);
        out.append(buf, buf + n);
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

                // Hex escape: \xHH (exactly two uppercase hex digits). If not matched, emit literally.
                case 'x': {
                    ++p;
                    const char *digitsStart = p;
                    auto hexUpper = [](char ch) -> int {
                        if (ch >= '0' && ch <= '9') return ch - '0';
                        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
                        return -1;
                    };
                    unsigned int value = 0;
                    int digits = 0;
                    int hv;
                    while (digits < 2 && (hv = hexUpper(*p)) != -1) {
                        value = (value << 4) + static_cast<unsigned int>(hv);
                        ++p; ++digits;
                    }
                    if (digits == 2) {
                        result += static_cast<char>(value & 0xFF);
                        --p; // point to last consumed so outer ++p moves to next
                    } else {
                        // Not exactly 2 uppercase hex digits: emit literal \x and keep following chars
                        result += '\\';
                        result += 'x';
                        p = digitsStart - 1; // let loop process from the first non-escaped char
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
                    const char *digitsBegin = p;

                    while (digits < need && (hv = HexVal(*p)) != -1) {
                        cp = (cp << 4) + static_cast<unsigned int>(hv);
                        ++p; ++digits;
                    }

                    if (digits == need) {
                        // Handle surrogate pairs for \u
                        if (kind == 'u' && cp >= 0xD800 && cp <= 0xDBFF) {
                            // High surrogate, check for low surrogate
                            const char *savePos = p;
                            if (*p == '\\' && *(p + 1) == 'u') {
                                p += 2;
                                unsigned int lo = 0;
                                int loDigits = 0;
                                while (loDigits < 4 && (hv = HexVal(*p)) != -1) {
                                    lo = (lo << 4) + static_cast<unsigned int>(hv);
                                    ++p; ++loDigits;
                                }
                                if (loDigits == 4 && lo >= 0xDC00 && lo <= 0xDFFF) {
                                    // Valid surrogate pair
                                    unsigned int code = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                    AppendUTF8(result, code);
                                    --p;
                                    break;
                                }
                            }
                            // Not a valid surrogate pair, restore position
                            p = savePos;
                        }

                        if (IsValidCodePoint(cp)) {
                            AppendUTF8(result, cp);
                            --p;
                        } else {
                            // Invalid code point, emit literally
                            result += '\\'; result += kind;
                            while (digitsBegin < p) { result += *digitsBegin++; }
                            --p;
                        }
                    } else {
                        // Incomplete sequence, emit literally
                        result += '\\'; result += kind;
                        while (digitsBegin < p) { result += *digitsBegin++; }
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
        // Inverse of UnescapeString: encode control chars, quotes, backslash,
        // and all non-ASCII as C/Unicode escapes.
        if (!str) return "";

        const auto *p = reinterpret_cast<const uint8_t *>(str);
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

        // Append \xNN for a single raw byte value (uppercase hex, fixed width).
        auto appendX = [&](uint8_t byte) {
            out += "\\x";
            unsigned hi = (byte >> 4) & 0xF;
            unsigned lo = byte & 0xF;
            out += (hi < 10) ? char('0' + hi) : char('A' + (hi - 10));
            out += (lo < 10) ? char('0' + lo) : char('A' + (lo - 10));
        };

        // Emit ASCII with simple escapes when available; otherwise keep byte as-is
        // or use \u00XX for DEL/control. For non-ASCII bytes that are part of
        // a valid UTF-8 sequence, we'll represent the code point as \u/\U below.
        auto escapeAscii = [&](uint8_t c) {
            switch (c) {
            case '\a': out += "\\a"; return;
            case '\b': out += "\\b"; return;
            case '\f': out += "\\f"; return;
            case '\n': out += "\\n"; return;
            case '\r': out += "\\r"; return;
            case '\t': out += "\\t"; return;
            case '\v': out += "\\v"; return;
            case '\\': out += "\\\\"; return;
            case '\"': out += "\\\""; return;
            case '\'': out += "\\\'"; return;
            case '\033': out += "\\e"; return; // ESC, matches EscapeString
            default:
                if (c < 0x20 || c == 0x7F) {
                    // C0 controls and DEL: use \u00XX for readability
                    out += "\\u00";
                    unsigned hi = (c >> 4) & 0xF, lo = c & 0xF;
                    out += (hi < 10) ? char('0' + hi) : char('A' + (hi - 10));
                    out += (lo < 10) ? char('0' + lo) : char('A' + (lo - 10));
                } else {
                    out += char(c);
                }
            }
        };

        while (p < end) {
            if (*p < 0x80) {
                escapeAscii(*p++);
            } else {
                size_t guess = utf8codepointcalcsize(reinterpret_cast<const utf8_int8_t*>(p));
                if (guess == 0 || static_cast<size_t>(end - p) < guess) {
                    appendX(*p);
                    ++p;
                } else if (utf8nvalid(reinterpret_cast<const utf8_int8_t*>(p), guess) != nullptr) {
                    appendX(*p);
                    ++p;
                } else {
                    utf8_int32_t cp2;
                    (void)utf8codepoint(reinterpret_cast<const utf8_int8_t*>(p), &cp2);
                    if ((cp2 >= 0xD800 && cp2 <= 0xDFFF) || cp2 > 0x10FFFF) {
                        appendX(*p);
                        ++p;
                    } else {
                        appendU(static_cast<unsigned int>(cp2));
                        p += guess;
                    }
                }
            }
        }
        return out;
    }

    std::string StripAnsiCodes(const char *str) {
        if (!str) return "";

        auto isFinalByte = [](unsigned char c) {
            return c >= 0x40 && c <= 0x7E; // ECMA-48 final byte
        };
        auto isKnownCSIFinal = [](unsigned char c) {
            // Be conservative: support common CSI finals used by typical terminals
            switch (c) {
                case 'A': case 'B': case 'C': case 'D': // CUU/CUD/CUF/CUB
                case 'E': case 'F': case 'G':           // CNL/CPL/CHA
                case 'H': case 'f':                     // CUP/HVP
                case 'J': case 'K':                     // ED/EL
                case 'S': case 'T':                     // SU/SD
                case 'm':                               // SGR
                case 'n': case 's': case 'u':           // DSR/SCP/RCP
                    return true;
                default:
                    return false;
            }
        };

        std::string out;
        out.reserve(std::strlen(str));

        const auto *p = reinterpret_cast<const unsigned char *>(str);
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
                    const unsigned char *q = p;
                    while (*q && (*q >= 0x30 && *q <= 0x3F)) ++q; // parameters
                    const unsigned char *r = q;
                    while (*q && (*q >= 0x20 && *q <= 0x2F)) ++q; // intermediates
                    if (*q && isFinalByte(*q)) {
                        if (isKnownCSIFinal(*q)) {
                            p = q + 1; // consume full sequence
                        } else {
                            // Unknown final: drop ESC+[ and params only; preserve intermediates and final
                            p = r;
                        }
                    } else {
                        // No final: drop ESC+[ and params only; keep intermediates
                        p = r;
                    }
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
                while (*p && !isFinalByte(*p)) ++p;
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
