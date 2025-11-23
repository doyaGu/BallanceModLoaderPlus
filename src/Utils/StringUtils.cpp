#include "StringUtils.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <climits>
#include <locale>
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
        char buf[5] = {0}; // Ensure null termination
        const size_t n = utf8codepointsize(static_cast<utf8_int32_t>(cp));
        if (n > 0 && n <= 4) {
            utf8catcodepoint(reinterpret_cast<utf8_int8_t*>(buf), static_cast<utf8_int32_t>(cp), n);
            out.append(buf, n);
        }
    }

#ifndef _WIN32
    std::wstring Utf8ToWideFallback(const std::string &value) {
        if (value.empty())
            return L"";

        std::wstring result;
        result.reserve(value.size());
        const auto *cur = reinterpret_cast<const utf8_int8_t *>(value.data());
        const auto *end = cur + value.size();
        while (cur < end) {
            size_t remaining = static_cast<size_t>(end - cur);
            size_t needed = utf8codepointcalcsize(cur);
            if (needed == 0 || needed > remaining) {
                ++cur;
                result.push_back(static_cast<wchar_t>(0xFFFD));
                continue;
            }

            utf8_int32_t cp = 0;
            const auto *next = utf8codepoint(cur, &cp);
            if (next == cur) {
                ++cur;
                continue;
            }

            cur = next;
            if (cp < 0 || cp > 0x10FFFF)
                cp = 0xFFFD;

#if WCHAR_MAX <= 0xFFFF
            if (cp >= 0x10000) {
                cp -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xD800 + ((cp >> 10) & 0x3FF)));
                result.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
            } else {
                result.push_back(static_cast<wchar_t>(cp));
            }
#else
            result.push_back(static_cast<wchar_t>(cp));
#endif
        }

        return result;
    }

    std::wstring LocaleToWideFallback(const std::string &value) {
        if (value.empty())
            return L"";

        std::wstring result;
        result.reserve(value.size());
        std::locale loc("");
        const auto &facet = std::use_facet<std::ctype<wchar_t>>(loc);
        for (char ch : value)
            result.push_back(facet.widen(ch));
        return result;
    }

    std::string WideToUtf8Fallback(const std::wstring &value) {
        if (value.empty())
            return {};

        std::string result;
        result.reserve(value.size());
#if WCHAR_MAX <= 0xFFFF
        for (size_t i = 0; i < value.size(); ++i) {
            unsigned int cp = static_cast<unsigned int>(value[i]);
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                if (i + 1 < value.size()) {
                    unsigned int low = static_cast<unsigned int>(value[i + 1]);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        ++i;
                    } else {
                        cp = 0xFFFD;
                    }
                } else {
                    cp = 0xFFFD;
                }
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                cp = 0xFFFD;
            }
            AppendUTF8(result, cp);
        }
#else
        for (wchar_t ch : value)
            AppendUTF8(result, static_cast<unsigned int>(ch));
#endif
        return result;
    }

    std::string WideToLocaleFallback(const std::wstring &value) {
        if (value.empty())
            return {};

        std::string result;
        result.reserve(value.size());
        std::locale loc("");
        const auto &facet = std::use_facet<std::ctype<wchar_t>>(loc);
        for (wchar_t ch : value)
            result.push_back(facet.narrow(ch, '?'));
        return result;
    }
#endif // _WIN32
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
    std::wstring ToWString(const std::string &str, bool isUtf8) {
        if (str.empty()) return L"";
        return isUtf8 ? Utf8ToWideFallback(str) : LocaleToWideFallback(str);
    }

    std::string ToString(const std::wstring &wstr, bool toUtf8) {
        if (wstr.empty()) return "";
        return toUtf8 ? WideToUtf8Fallback(wstr) : WideToLocaleFallback(wstr);
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

                // Hex escape: \xHH (exactly two hex digits). If not matched, emit literally.
                case 'x': {
                    ++p;
                    const char *digitsStart = p;
                    unsigned int value = 0;
                    int digits = 0;
                    int hv;
                    while (digits < 2 && (hv = HexVal(*p)) != -1) {
                        value = (value << 4) + static_cast<unsigned int>(hv);
                        ++p; ++digits;
                    }
                    if (digits == 2) {
                        result += static_cast<char>(value & 0xFF);
                        --p; // point to last consumed so outer ++p moves to next
                    } else {
                        // Not exactly 2 hex digits: emit literal \x and keep following chars
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
                    if (cp2 < 0 || (cp2 >= 0xD800 && cp2 <= 0xDFFF) || cp2 > 0x10FFFF) {
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
        if (!str) return {};
        auto isFinal = [](unsigned char c){ return c >= 0x40 && c <= 0x7E; };  // ECMA-48 final
        auto isParam = [](unsigned char c){ return c >= 0x30 && c <= 0x3F; };  // 0..9:;<=>?
        auto isInter = [](unsigned char c){ return c >= 0x20 && c <= 0x2F; };  // SP..'/'
        auto isCsiInter = [](unsigned char c){ return c >= 0x21 && c <= 0x2F; }; // exclude SP to avoid eating text

        std::string out;
        out.reserve(std::strlen(str));

        const auto *p = reinterpret_cast<const unsigned char *>(str);

        auto consumeCsi = [&](size_t lead) {
            const unsigned char *seq = p + lead;
            if (!*seq) { p = seq; return; }
            while (*seq && isParam(*seq)) ++seq;
            const unsigned char *interBegin = seq;
            while (*seq && isCsiInter(*seq)) ++seq;
            if (*seq && isFinal(*seq)) {
                p = seq + 1;
            } else {
                p = interBegin;
            }
        };

        auto consumeOsc = [&](size_t lead) {
            const unsigned char *seq = p + lead;
            while (*seq) {
                if (*seq == 0x07) { p = seq + 1; return; } // BEL terminator
                if (*seq == 0x1B && seq[1] == '\\') { p = seq + 2; return; } // ESC \ (ST)
                if (*seq == 0x9C) { p = seq + 1; return; } // 8-bit ST
                ++seq;
            }
            p = seq; // ran off end, drop the sequence
        };

        auto consumeStTerminated = [&](size_t lead) {
            const unsigned char *seq = p + lead;
            while (*seq) {
                if (*seq == 0x1B && seq[1] == '\\') { p = seq + 2; return; }
                if (*seq == 0x9C) { p = seq + 1; return; }
                ++seq;
            }
            p = seq;
        };

        while (*p) {
            const unsigned char c = *p;

            if (c == 0x1B) { // ESC ...
                if (!p[1]) break;
                const unsigned char a = p[1];

                // CSI: ESC [ params inter final
                if (a == '[') {
                    consumeCsi(2);
                    continue;
                }

                // OSC: ESC ] ... (terminated by BEL or ST `ESC \\`/0x9C)
                if (a == ']') {
                    consumeOsc(2);
                    continue;
                }

                // DCS/SOS/PM/APC: ESC P/X/^/_ ... ST
                if (a == 'P' || a == 'X' || a == '^' || a == '_') {
                    consumeStTerminated(2);
                    continue;
                }

                // Generic ESC: ESC inter* final?
                ++p; // after ESC
                while (*p && isInter(*p)) ++p; // intermediates
                if (*p && isFinal(*p)) ++p; // optional final
                else if (*p) ++p; // malformed: drop one byte
                continue;
            }

            // C1 control equivalents (8-bit forms)
            if (c == 0x9B) { consumeCsi(1); continue; }           // CSI
            if (c == 0x9D) { consumeOsc(1); continue; }            // OSC
            if (c == 0x90 || c == 0x98 || c == 0x9E || c == 0x9F) {
                consumeStTerminated(1);                            // DCS/SOS/PM/APC
                continue;
            }
            if (c == 0x9C) { ++p; continue; }                      // ST terminator

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
