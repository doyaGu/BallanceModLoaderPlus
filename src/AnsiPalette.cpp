#include "AnsiPalette.h"

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cmath>
#include <string>
#include <algorithm>
#include <set>
#include <map>

#include "PathUtils.h"
#include "StringUtils.h"

AnsiPalette::AnsiPalette() : m_Initialized(false), m_Active(false) {
    for (int i = 0; i < 256; ++i) {
        m_Palette[i] = IM_COL32_WHITE;
        m_HasOverride[i] = false;
    }

    // Toning defaults
    m_ToningEnabled = false;
    m_ToneBrightness = 0.0f;
    m_ToneSaturation = 0.0f;

    // Generation defaults
    m_CubeMixFromTheme = false;
    m_GrayMixFromTheme = false;
    m_MixStrength = 0.65f;
    m_LinearMix = false;
}

// Optional external providers
AnsiPalette::LoaderDirProvider AnsiPalette::s_LoaderDirProvider = nullptr;
AnsiPalette::LogProvider AnsiPalette::s_LogProvider = nullptr;

void AnsiPalette::SetLoaderDirProvider(LoaderDirProvider provider) {
    s_LoaderDirProvider = provider;
}

AnsiPalette::LoaderDirProvider AnsiPalette::GetLoaderDirProvider() { return s_LoaderDirProvider; }

void AnsiPalette::SetLoggerProvider(LogProvider logger) {
    s_LogProvider = logger;
}

AnsiPalette::LogProvider AnsiPalette::GetLoggerProvider() { return s_LogProvider; }

// internal logging helper
static void AP_Log(int level, const std::string &msg) {
    const auto logger = AnsiPalette::GetLoggerProvider();
    if (logger) {
        logger(level, msg.c_str());
    }
}

ImU32 AnsiPalette::RGBA(unsigned r, unsigned g, unsigned b, unsigned a) {
    return IM_COL32((int)r, (int)g, (int)b, (int)a);
}

ImU32 AnsiPalette::HexToImU32(const char *hex) {
    if (!hex) return IM_COL32_WHITE;
    auto hv = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    size_t n = strlen(hex);
    unsigned r = 0, g = 0, b = 0, a = 255;
    if (n == 6) {
        int v0 = hv(hex[0]), v1 = hv(hex[1]), v2 = hv(hex[2]), v3 = hv(hex[3]), v4 = hv(hex[4]), v5 = hv(hex[5]);
        if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0 || v4 < 0 || v5 < 0) return IM_COL32_WHITE;
        r = (unsigned) (v0 * 16 + v1);
        g = (unsigned) (v2 * 16 + v3);
        b = (unsigned) (v4 * 16 + v5);
    } else if (n == 8) {
        int v0 = hv(hex[0]), v1 = hv(hex[1]), v2 = hv(hex[2]), v3 = hv(hex[3]), v4 = hv(hex[4]), v5 = hv(hex[5]), v6 =
                hv(hex[6]), v7 = hv(hex[7]);
        if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0 || v4 < 0 || v5 < 0 || v6 < 0 || v7 < 0) return IM_COL32_WHITE;
        a = (unsigned) (v0 * 16 + v1);
        r = (unsigned) (v2 * 16 + v3);
        g = (unsigned) (v4 * 16 + v5);
        b = (unsigned) (v6 * 16 + v7);
    } else {
        return IM_COL32_WHITE;
    }
    return RGBA(r, g, b, a);
}

std::wstring AnsiPalette::GetLoaderDirW() const {
    if (s_LoaderDirProvider) return s_LoaderDirProvider();
    return utils::GetCurrentDirectoryW();
}

std::wstring AnsiPalette::GetFilePathW() const {
    std::wstring path = GetLoaderDirW();
    path.append(L"\\palette.ini");
    return path;
}

std::wstring AnsiPalette::GetThemesDirW() const {
    std::wstring path = GetLoaderDirW();
    path.append(L"\\Themes");
    return path;
}

void AnsiPalette::BuildDefault() {
    // Standard 0-7 (xterm-like)
    ImU32 stdc[8] = {
        RGBA(0x00, 0x00, 0x00), // black
        RGBA(0x80, 0x00, 0x00), // red
        RGBA(0x00, 0x80, 0x00), // green
        RGBA(0x80, 0x80, 0x00), // yellow
        RGBA(0x00, 0x00, 0x80), // blue
        RGBA(0x80, 0x00, 0x80), // magenta
        RGBA(0x00, 0x80, 0x80), // cyan
        RGBA(0xC0, 0xC0, 0xC0)  // white
    };
    ImU32 brtc[8] = {
        RGBA(0x80, 0x80, 0x80), // bright black
        RGBA(0xFF, 0x00, 0x00), // bright red
        RGBA(0x00, 0xFF, 0x00), // bright green
        RGBA(0xFF, 0xFF, 0x00), // bright yellow
        RGBA(0x00, 0x00, 0xFF), // bright blue
        RGBA(0xFF, 0x00, 0xFF), // bright magenta
        RGBA(0x00, 0xFF, 0xFF), // bright cyan
        RGBA(0xFF, 0xFF, 0xFF)  // bright white
    };
    for (int i = 0; i < 256; ++i) {
        m_HasOverride[i] = false;
        m_Palette[i] = IM_COL32_WHITE;
    }
    for (int i = 0; i < 8; ++i) { m_Palette[i] = stdc[i]; }
    for (int i = 0; i < 8; ++i) { m_Palette[i + 8] = brtc[i]; }
    // 6x6x6 cube 16..231 (xterm standard)
    static const int values[6] = {0, 95, 135, 175, 215, 255};
    for (int idx = 16; idx < 232; ++idx) {
        int v = idx - 16;
        int r6 = (v / 36) % 6;
        int g6 = ((v % 36) / 6) % 6;
        int b6 = v % 6;
        int r = values[r6];
        int g = values[g6];
        int b = values[b6];
        m_Palette[idx] = RGBA(r, g, b);
    }
    // Gray 232..255 (xterm standard)
    for (int idx = 232; idx < 256; ++idx) {
        int gray = 8 + (idx - 232) * 10;
        m_Palette[idx] = RGBA(gray, gray, gray);
    }
    m_Active = true;
}

// -----------
// Local helpers (INI parsing utilities)
// -----------

static std::string ToLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char) std::tolower(c); });
    return s;
}

static void LTrim(std::string &s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i) s.erase(0, i);
}

static void RTrim(std::string &s) {
    size_t i = s.size();
    while (i > 0 && (s[i - 1] == ' ' || s[i - 1] == '\t' || s[i - 1] == '\r')) --i;
    if (i < s.size()) s.erase(i);
}

static void Trim(std::string &s) {
    RTrim(s);
    LTrim(s);
}

static bool IsNumberStr(const std::string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') {
        if (s.size() == 1) return false;
        i = 1;
    }
    for (; i < s.size(); ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < '0' || ch > '9') return false;
    }
    return true;
}

static bool ParseColorVal(const std::string &val, ImU32 &out) {
    if (!val.empty() && val[0] == '#') {
        out = AnsiPalette::HexToImU32(val.c_str() + 1);
        return true;
    }
    // Flexible numeric parsing: accept separators such as commas, spaces, semicolons, tabs, slashes, pipes, colons
    int vals[4] = {0, 0, 0, 255};
    int count = 0;
    int cur = 0;
    bool in_num = false;
    for (size_t i = 0; i < val.size(); ++i) {
        unsigned char ch = (unsigned char)val[i];
        if (ch >= '0' && ch <= '9') {
            in_num = true;
            cur = cur * 10 + (ch - '0');
            if (cur > 9999) cur = 9999; // clamp to avoid overflow
        } else {
            if (in_num) {
                if (count < 4) vals[count++] = cur;
                cur = 0;
                in_num = false;
            }
            // else skip separator character
        }
    }
    if (in_num && count < 4) vals[count++] = cur;
    if (count == 3 || count == 4) {
        int r = std::clamp(vals[0], 0, 255);
        int g = std::clamp(vals[1], 0, 255);
        int b = std::clamp(vals[2], 0, 255);
        int a = (count == 4) ? std::clamp(vals[3], 0, 255) : 255;
        out = AnsiPalette::RGBA((unsigned) r, (unsigned) g, (unsigned) b, (unsigned) a);
        return true;
    }
    return false;
}

void AnsiPalette::ParseBuffer(const std::string &buf) {
    std::string section;
    size_t pos = 0;
    while (pos < buf.size()) {
        size_t line_end = buf.find_first_of("\r\n", pos);
        size_t n = (line_end == std::string::npos) ? (buf.size() - pos) : (line_end - pos);
        std::string line = buf.substr(pos, n);
        pos = (line_end == std::string::npos) ? buf.size() : (line_end + 1);
        LTrim(line);
        RTrim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = ToLowerStr(line.substr(1, line.size() - 2));
            continue;
        }

        // Backward-compat: no section provided -> treat as overrides of index
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        LTrim(key);
        RTrim(key);
        LTrim(val);
        RTrim(val);
        std::string lkey = ToLowerStr(key);

        auto setIndexColor = [&](int idx, const std::string &v) {
            ImU32 c;
            if (ParseColorVal(v, c)) {
                m_Palette[idx] = c;
                m_HasOverride[idx] = true;
            } else {
                if (!m_ParseOrigin.empty()) {
                    AP_Log(1, std::string("AnsiPalette: invalid color value '") + v + "' for index " + std::to_string(idx) +
                                 " in [" + section + "] at " + m_ParseOrigin);
                } else {
                    AP_Log(1, std::string("AnsiPalette: invalid color value '") + v + "' for index " + std::to_string(idx));
                }
            }
        };

        if (section == "standard" || section == "bright") {
            int base = (section == "standard") ? 0 : 8;
            int nameIndex = -1;
            if (lkey == "black") nameIndex = 0;
            else if (lkey == "red") nameIndex = 1;
            else if (lkey == "green") nameIndex = 2;
            else if (lkey == "yellow") nameIndex = 3;
            else if (lkey == "blue") nameIndex = 4;
            else if (lkey == "magenta") nameIndex = 5;
            else if (lkey == "cyan") nameIndex = 6;
            else if (lkey == "white") nameIndex = 7;
            if (nameIndex >= 0) {
                setIndexColor(base + nameIndex, val);
                continue;
            }
            // Allow numeric indices too
            if (IsNumberStr(lkey)) {
                int idx = std::stoi(lkey);
                if ((section == "standard" && idx >= 0 && idx <= 7) || (section == "bright" && idx >= 8 && idx <= 15)) {
                    setIndexColor(idx, val);
                }
            }
        } else if (section == "cube") {
            // Index override within cube range only (no tuning keys)
            if (IsNumberStr(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 16 && idx <= 231) setIndexColor(idx, val);
            }
        } else if (section == "gray" || section == "grayscale") {
            // Index override within gray range only (no tuning keys)
            if (IsNumberStr(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 232 && idx <= 255) setIndexColor(idx, val);
            }
        } else if (section == "overrides") {
            size_t dash = lkey.rfind('-');
            if (dash != std::string::npos) {
                std::string as = lkey.substr(0, dash);
                std::string bs = lkey.substr(dash + 1);
                Trim(as);
                Trim(bs);
                if (IsNumberStr(as) && IsNumberStr(bs)) {
                    int a = std::stoi(as);
                    int b = std::stoi(bs);
                    if (a > b) std::swap(a, b);
                    a = std::max(0, a);
                    b = std::min(255, b);
                    for (int i = a; i <= b; ++i) setIndexColor(i, val);
                } else {
                    AP_Log(1, std::string("AnsiPalette: invalid range '") + lkey + "' in [overrides]" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            } else if (IsNumberStr(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 0 && idx <= 255) {
                    setIndexColor(idx, val);
                } else {
                    AP_Log(1, std::string("AnsiPalette: override index out of range '") + lkey + "'" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            }
        } else if (section == "theme") {
            if (lkey == "toning" || lkey == "tone_enable" || lkey == "enable_toning") {
                std::string v = ToLowerStr(val);
                m_ToningEnabled = (v == "1" || v == "true" || v == "on" || v == "yes");
            } else if (lkey == "cube" || lkey == "cube_mode" || lkey == "cube_from_theme") {
                std::string v = ToLowerStr(val);
                if (v == "theme" || v == "mix" || v == "1" || v == "true" || v == "on" || v == "yes") {
                    m_CubeMixFromTheme = true;
                } else if (v == "standard" || v == "xterm" || v == "0" || v == "false" || v == "off" || v == "no") {
                    m_CubeMixFromTheme = false;
                } else {
                    AP_Log(1, std::string("AnsiPalette: invalid value for 'cube': '") + val + "'" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            } else if (lkey == "gray" || lkey == "grey" || lkey == "gray_mode" || lkey == "grey_mode" || lkey ==
                "gray_from_theme" || lkey == "grey_from_theme") {
                std::string v = ToLowerStr(val);
                if (v == "theme" || v == "mix" || v == "1" || v == "true" || v == "on" || v == "yes") {
                    m_GrayMixFromTheme = true;
                } else if (v == "standard" || v == "xterm" || v == "0" || v == "false" || v == "off" || v == "no") {
                    m_GrayMixFromTheme = false;
                } else {
                    AP_Log(1, std::string("AnsiPalette: invalid value for 'gray': '") + val + "'" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            } else if (lkey == "tone_brightness") {
                try {
                    m_ToneBrightness = std::clamp(std::stof(val), -1.0f, 1.0f);
                } catch (...) {
                    AP_Log(1, std::string("AnsiPalette: invalid float for 'tone_brightness': '") + val + "' (range -1..1)" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            } else if (lkey == "tone_saturation") {
                try {
                    m_ToneSaturation = std::clamp(std::stof(val), -1.0f, 1.0f);
                } catch (...) {
                    AP_Log(1, std::string("AnsiPalette: invalid float for 'tone_saturation': '") + val + "' (range -1..1)" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            } else if (lkey == "mix_strength" || lkey == "mix" || lkey == "mix_intensity" || lkey == "mix_saturation") {
                try {
                    float f = std::stof(val);
                    if (f > 1.0f) f *= 0.01f; // allow percent
                    m_MixStrength = std::clamp(f, 0.0f, 1.0f);
                } catch (...) {
                    AP_Log(1, std::string("AnsiPalette: invalid float for 'mix_strength': '") + val + "' (range 0..1 or percent)" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            } else if (lkey == "linear_mix" || lkey == "linear" || lkey == "mix_linear") {
                std::string v = ToLowerStr(val);
                m_LinearMix = (v == "1" || v == "true" || v == "on" || v == "yes");
            } else if (lkey == "mix_space" || lkey == "mix_colorspace" || lkey == "mix_colourspace" || lkey ==
                "color_space" || lkey == "colour_space") {
                std::string v = ToLowerStr(val);
                if (v == "linear" || v == "linear-light" || v == "linear_rgb" || v == "linear-rgb") m_LinearMix = true;
                else if (v == "srgb" || v == "gamma" || v == "perceptual") m_LinearMix = false;
                else {
                    AP_Log(1, std::string("AnsiPalette: invalid value for 'mix_space': '") + val + "' (srgb|linear)" +
                                 (m_ParseOrigin.empty() ? std::string("") : (" at " + m_ParseOrigin)));
                }
            }
        } else {
            // Backward compatible: key as index only if numeric
            if (IsNumberStr(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 0 && idx <= 255) {
                    setIndexColor(idx, val);
                }
            }
        }
    }
}

void AnsiPalette::RebuildCubeAndGray() {
    // Cube 16..231
    static const int values[6] = {0, 95, 135, 175, 215, 255};
    if (!m_CubeMixFromTheme) {
        // Standard xterm cube
        for (int idx = 16; idx < 232; ++idx) {
            if (m_HasOverride[idx]) continue;
            int v = idx - 16;
            int r6 = (v / 36) % 6;
            int g6 = ((v % 36) / 6) % 6;
            int b6 = v % 6;
            int r = values[r6];
            int g = values[g6];
            int b = values[b6];
            m_Palette[idx] = RGBA(r, g, b);
        }
    } else {
        // Mix cube colors by trilinear interpolation of theme corners:
        // corners: black(0), bright primaries (9,10,12), bright secondaries (11,13,14), bright white(15).
        const bool useLinear = m_LinearMix;
        auto to_f = [](ImU32 c, int shift) -> float { return ((c >> shift) & 0xFF) / 255.0f; };
        auto colf = [&](int idx, float &r, float &g, float &b) {
            ImU32 c = m_Palette[idx];
            r = to_f(c, IM_COL32_R_SHIFT);
            g = to_f(c, IM_COL32_G_SHIFT);
            b = to_f(c, IM_COL32_B_SHIFT);
        };
        // Corner colors as floats
        float c000_r, c000_g, c000_b;
        colf(0, c000_r, c000_g, c000_b); // black
        float c100_r, c100_g, c100_b;
        colf(8 + 1, c100_r, c100_g, c100_b); // bright red
        float c010_r, c010_g, c010_b;
        colf(8 + 2, c010_r, c010_g, c010_b); // bright green
        float c001_r, c001_g, c001_b;
        colf(8 + 4, c001_r, c001_g, c001_b); // bright blue
        float c110_r, c110_g, c110_b;
        colf(8 + 3, c110_r, c110_g, c110_b); // bright yellow
        float c101_r, c101_g, c101_b;
        colf(8 + 5, c101_r, c101_g, c101_b); // bright magenta
        float c011_r, c011_g, c011_b;
        colf(8 + 6, c011_r, c011_g, c011_b); // bright cyan
        float c111_r, c111_g, c111_b;
        colf(8 + 7, c111_r, c111_g, c111_b); // bright white

        if (useLinear) {
            // Convert corners to linear-light space
            auto conv = [&](float &r, float &g, float &b) {
                r = SrgbToLinear(r);
                g = SrgbToLinear(g);
                b = SrgbToLinear(b);
            };
            conv(c000_r, c000_g, c000_b);
            conv(c100_r, c100_g, c100_b);
            conv(c010_r, c010_g, c010_b);
            conv(c001_r, c001_g, c001_b);
            conv(c110_r, c110_g, c110_b);
            conv(c101_r, c101_g, c101_b);
            conv(c011_r, c011_g, c011_b);
            conv(c111_r, c111_g, c111_b);
        }

        float w6[6];
        for (int i = 0; i < 6; ++i) w6[i] = values[i] / 255.0f;
        for (int idx = 16; idx < 232; ++idx) {
            if (m_HasOverride[idx]) continue;
            int v = idx - 16;
            int r6 = (v / 36) % 6;
            int g6 = ((v % 36) / 6) % 6;
            int b6 = v % 6;

            // Coordinates in 0..1 along each axis; if linear mixing, linearize coordinates too
            float r = useLinear ? SrgbToLinear(w6[r6]) : w6[r6];
            float g = useLinear ? SrgbToLinear(w6[g6]) : w6[g6];
            float b = useLinear ? SrgbToLinear(w6[b6]) : w6[b6];
            float ir = 1.0f - r, ig = 1.0f - g, ib = 1.0f - b;

            // Trilinear interpolation from 8 corners
            float rr =
                c000_r * (ir * ig * ib) + c100_r * (r * ig * ib) + c010_r * (ir * g * ib) + c001_r * (ir * ig * b) +
                c110_r * (r * g * ib) + c101_r * (r * ig * b) + c011_r * (ir * g * b) + c111_r * (r * g * b);
            float gg =
                c000_g * (ir * ig * ib) + c100_g * (r * ig * ib) + c010_g * (ir * g * ib) + c001_g * (ir * ig * b) +
                c110_g * (r * g * ib) + c101_g * (r * ig * b) + c011_g * (ir * g * b) + c111_g * (r * g * b);
            float bb =
                c000_b * (ir * ig * ib) + c100_b * (r * ig * ib) + c010_b * (ir * g * ib) + c001_b * (ir * ig * b) +
                c110_b * (r * g * ib) + c101_b * (r * ig * b) + c011_b * (ir * g * b) + c111_b * (r * g * b);

            // Standard cube in 0..1 for blending
            float sr = r, sg = g, sb = b;
            float t = m_MixStrength;
            float fr = sr * (1.0f - t) + rr * t;
            float fg = sg * (1.0f - t) + gg * t;
            float fb = sb * (1.0f - t) + bb * t;
            // Convert back to sRGB if we mixed in linear space
            if (useLinear) {
                fr = LinearToSrgb(fr);
                fg = LinearToSrgb(fg);
                fb = LinearToSrgb(fb);
            }
            int ri = (int) std::round(std::clamp(fr, 0.0f, 1.0f) * 255.0f);
            int gi = (int) std::round(std::clamp(fg, 0.0f, 1.0f) * 255.0f);
            int bi = (int) std::round(std::clamp(fb, 0.0f, 1.0f) * 255.0f);
            m_Palette[idx] = RGBA((unsigned) ri, (unsigned) gi, (unsigned) bi);
        }
    }
    // Gray 232..255
    if (!m_GrayMixFromTheme) {
        // Standard xterm ramp (8..238 step 10)
        for (int idx = 232; idx < 256; ++idx) {
            if (m_HasOverride[idx]) continue;
            int gray = 8 + (idx - 232) * 10;
            m_Palette[idx] = RGBA(gray, gray, gray);
        }
    } else {
        // Mix between theme black (index 0) and theme white (index 7),
        // then blend with standard gray by m_MixStrength.
        const bool useLinear = m_LinearMix;
        ImU32 cBlack = m_Palette[0];
        ImU32 cWhite = m_Palette[7];
        auto chan = [](ImU32 c, int sh) { return (float) ((c >> sh) & 0xFF) / 255.0f; };
        float br = chan(cBlack, IM_COL32_R_SHIFT), bg = chan(cBlack, IM_COL32_G_SHIFT), bb = chan(
                  cBlack, IM_COL32_B_SHIFT);
        float wr = chan(cWhite, IM_COL32_R_SHIFT), wg = chan(cWhite, IM_COL32_G_SHIFT), wb = chan(
                  cWhite, IM_COL32_B_SHIFT);
        if (useLinear) {
            br = SrgbToLinear(br);
            bg = SrgbToLinear(bg);
            bb = SrgbToLinear(bb);
            wr = SrgbToLinear(wr);
            wg = SrgbToLinear(wg);
            wb = SrgbToLinear(wb);
        }
        for (int idx = 232; idx < 256; ++idx) {
            if (m_HasOverride[idx]) continue;
            int gray = 8 + (idx - 232) * 10; // 8..238
            // Normalize 8..238 onto 0..1 for theme interpolation, so corners hit theme black/white.
            float t = (float) (gray - 8) / 230.0f;
            float tr = br * (1.0f - t) + wr * t;
            float tg = bg * (1.0f - t) + wg * t;
            float tb = bb * (1.0f - t) + wb * t;
            // Standard gray in chosen space
            float sr = gray / 255.0f;
            if (useLinear) sr = SrgbToLinear(sr);
            float sg = sr, sb = sr;
            float ms = m_MixStrength;
            float fr = sr * (1.0f - ms) + tr * ms;
            float fg = sg * (1.0f - ms) + tg * ms;
            float fb = sb * (1.0f - ms) + tb * ms;
            if (useLinear) {
                fr = LinearToSrgb(fr);
                fg = LinearToSrgb(fg);
                fb = LinearToSrgb(fb);
            }
            int ri = (int) std::round(std::clamp(fr, 0.0f, 1.0f) * 255.0f);
            int gi = (int) std::round(std::clamp(fg, 0.0f, 1.0f) * 255.0f);
            int bi = (int) std::round(std::clamp(fb, 0.0f, 1.0f) * 255.0f);
            m_Palette[idx] = RGBA((unsigned) ri, (unsigned) gi, (unsigned) bi);
        }
    }
}

void AnsiPalette::EnsureInitialized() {
    if (m_Initialized) return;
    BuildDefault();
    // Load theme chain + overrides if file exists (chain supports nested base references)
    std::wstring path = GetFilePathW();
    if (utils::FileExistsW(path)) {
        FILE *fp = _wfopen(path.c_str(), L"rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long len = ftell(fp);
            rewind(fp);
            if (len > 0) {
                std::string buf;
                buf.resize((size_t) len);
                if (fread(buf.data(), 1, (size_t) len, fp) != (size_t) len) buf.clear();
                if (!buf.empty()) {
                    const std::string old = m_ParseOrigin;
                    m_ParseOrigin = utils::Utf16ToUtf8(path);
                    ApplyThemeChainAndBuffer(buf);
                    m_ParseOrigin = old;
                }
            }
            fclose(fp);
        }
    }
    // Else: no config file -> keep default xterm-like palette (no implicit theme)
    RebuildCubeAndGray();
    m_Initialized = true;
}

bool AnsiPalette::ReloadFromFile() {
    BuildDefault();
    std::wstring path = GetFilePathW();
    if (!utils::FileExistsW(path)) {
        m_Initialized = true; // still initialized, only defaults
        return false;
    }
    FILE *fp = _wfopen(path.c_str(), L"rb");
    if (!fp) {
        m_Initialized = true;
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);
    bool ok = false;
    if (len > 0) {
        std::string buf;
        buf.resize((size_t) len);
        if (fread(buf.data(), 1, (size_t) len, fp) == (size_t) len) {
            std::string old = m_ParseOrigin;
            m_ParseOrigin = utils::Utf16ToUtf8(path);
            ApplyThemeChainAndBuffer(buf);
            m_ParseOrigin = old;
            ok = true;
        }
    }
    fclose(fp);
    RebuildCubeAndGray();
    m_Initialized = true;
    return ok;
}

bool AnsiPalette::SaveSampleIfMissing() {
    std::wstring path = GetFilePathW();
    bool wrote = false;
    // Ensure ModLoader dir exists
    std::wstring dir = GetLoaderDirW();
    if (!utils::DirectoryExistsW(dir)) utils::CreateDirectoryW(dir);
    if (!utils::FileExistsW(path)) {
        FILE *fp = _wfopen(path.c_str(), L"wb");
        if (!fp) return false;
        const char *sample =
            "# palette.ini\n"
            "# Sections: [theme], [standard], [bright], [cube], [gray], [overrides]\n"
            "# Colors accept #RRGGBB, #AARRGGBB, or R,G,B[,A] with flexible separators (space/comma/semicolon).\n"
            "# Lines starting with # or ; are comments.\n\n"
            "[theme]\n"
            "# Import the nord theme by default:\n"
            "base = nord\n"
            "# Optional toning (applied to palette indices only).\n"
            "toning = off\n"
            "tone_brightness = 0\n"
            "tone_saturation = 0\n"
            "# Mix strength for theme-based cube/gray [0..1] or percent\n"
            "mix_strength = 0.65\n"
            "# Mix color space: srgb (default) or linear\n"
            "# mix_space = linear\n"
            "# Or enable with boolean: linear_mix = on\n"
            "# Cube mode: standard (xterm) or theme (mix from primaries)\n"
            "# cube = standard\n"
            "# Gray mode: standard (xterm) or theme (mix between theme black/white)\n"
            "# gray = standard\n\n"
            "# Optional overrides. Uncomment examples below as needed.\n"
            "[standard]  # indices 0..7\n"
            "# red = #FF0000\n"
            "# 1 = 255,0,0,255\n\n"
            "[bright]    # indices 8..15\n"
            "# black = #808080\n"
            "# 8 = 128,128,128\n\n"
            "[overrides] # override any index or range (0..255)\n"
            "# 196 = #FF0000\n"
            "# 232-239 = 180,180,180,128\n";
        fwrite(sample, 1, strlen(sample), fp);
        fclose(fp);
        wrote = true;
    }

    // Ensure a couple of sample themes exist
    std::wstring themesDir = GetThemesDirW();
    if (!utils::DirectoryExistsW(themesDir)) utils::CreateDirectoryW(themesDir);
    auto write_theme_if_missing = [&](const wchar_t *name, const char *content) {
        std::wstring p = themesDir;
        p.append(L"\\");
        p.append(name).append(L".ini");
        if (!utils::FileExistsW(p)) {
            FILE *f = _wfopen(p.c_str(), L"wb");
            if (!f) return;
            fwrite(content, 1, strlen(content), f);
            fclose(f);
        }
    };

    // Provide a Nord-style theme as a sample file (explicit colors)
    const char *nordTheme =
        "# theme: nord\n"
        "[theme]\n"
        "# You may chain to a parent theme here, e.g.:\n"
        "# base = parent-theme-name\n"
        "mix_strength = 0.65\n"
        "mix_space = linear\n"
        "cube = theme\n"
        "gray = theme\n\n"
        "# Standard (0..7)\n"
        "[standard]\n"
        "black  = #15171C\n"
        "red    = #F2778F\n"
        "green  = #B8E98E\n"
        "yellow = #F2C568\n"
        "blue   = #8EC1F2\n"
        "magenta= #F2B5E7\n"
        "cyan   = #88DBF2\n"
        "white  = #DADDE4\n\n"
        "# Bright (8..15)\n"
        "[bright]\n"
        "black  = #171920\n"
        "red    = #FF839F\n"
        "green  = #CCFF9C\n"
        "yellow = #FFD974\n"
        "blue   = #9ED5FF\n"
        "magenta= #FFC9FF\n"
        "cyan   = #96F3FF\n"
        "white  = #F0F5FC\n";
    write_theme_if_missing(L"nord", nordTheme);

    // Provide an Atom One Dark-style theme as a sample file
    const char *oneDarkTheme =
        "# theme: one-dark\n"
        "[theme]\n"
        "# Atom One Dark inspired ANSI palette\n"
        "mix_strength = 0.65\n"
        "mix_space = linear\n"
        "cube = theme\n"
        "gray = theme\n\n"
        "# Standard (0..7)\n"
        "[standard]\n"
        "black   = #1C1F25\n"
        "red     = #F27781\n"
        "green   = #8EE94D\n"
        "yellow  = #F2B23A\n"
        "blue    = #61B1F2\n"
        "magenta = #DB91F2\n"
        "cyan    = #53E3EB\n"
        "white   = #BBC4D4\n\n"
        "# Bright (8..15)\n"
        "[bright]\n"
        "black   = #202329\n"
        "red     = #FF838F\n"
        "green   = #9CFF55\n"
        "yellow  = #FFC440\n"
        "blue    = #6BC3FF\n"
        "magenta = #F3A1FF\n"
        "cyan    = #5BFBFF\n"
        "white   = #CFD8EA\n";
    write_theme_if_missing(L"one-dark", oneDarkTheme);

    return wrote;
}

bool AnsiPalette::GetColor(int index, ImU32 &outCol) const {
    if (index < 0 || index > 255) {
        outCol = IM_COL32_WHITE;
        return false;
    }
    ImU32 c = m_Palette[index];
    outCol = m_ToningEnabled ? ApplyToning(c) : c;
    return true;
}

bool AnsiPalette::IsActive() const {
    return m_Active;
}

std::wstring AnsiPalette::GetConfigPathW() const { return GetFilePathW(); }

// Forward declarations for helpers used before their definitions
static void AppendWithNewline(std::string &out, const std::string &line);
static std::string ApplyThemeMutationsToContent(const std::string &content, const std::vector<std::pair<std::string, std::string>> &mut);

std::wstring AnsiPalette::ResolveThemePathW(const std::wstring &name) const {
    if (name.empty()) return L"";
    std::wstring themes = GetThemesDirW();
    std::wstring cand;
    auto try_name = [&](const std::wstring &fn) -> bool {
        cand = themes + L"\\" + fn;
        return utils::FileExistsW(cand);
    };
    // If an explicit extension is provided, only accept .ini and .theme
    std::wstring ext = utils::GetExtensionW(name);
    if (!ext.empty()) {
        std::wstring el = ext;
        std::transform(el.begin(), el.end(), el.begin(), [](wchar_t c) { return (wchar_t) std::towlower(c); });
        if (el == L".ini" || el == L".theme") {
            if (try_name(name)) return cand;
        }
        return L"";
    }
    if (try_name(name + L".ini")) return cand;
    if (try_name(name + L".theme")) return cand;
    return L"";
}

std::string AnsiPalette::ExtractThemeName(const std::string &buf) {
    std::string section;
    size_t pos = 0;
    while (pos < buf.size()) {
        size_t line_end = buf.find_first_of("\r\n", pos);
        size_t n = (line_end == std::string::npos) ? (buf.size() - pos) : (line_end - pos);
        std::string line = buf.substr(pos, n);
        pos = (line_end == std::string::npos) ? buf.size() : (line_end + 1);
        LTrim(line);
        RTrim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = ToLowerStr(line.substr(1, line.size() - 2));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        LTrim(key);
        RTrim(key);
        LTrim(val);
        RTrim(val);
        std::string lkey = ToLowerStr(key);
        if ((section == "theme") && (lkey == "theme" || lkey == "base")) {
            std::string v = ToLowerStr(val);
            // Treat explicit 'none' as empty (no active theme)
            if (v == "none") return {};
            return v;
        }
    }
    return {};
}

void AnsiPalette::ApplyThemeChainAndBuffer(const std::string &buf) {
    std::string topTheme = ExtractThemeName(buf);
    if (!topTheme.empty() && topTheme != "none") {
        std::set<std::wstring> visited;
        auto toW = [](const std::string &s) { return std::wstring(s.begin(), s.end()); };
        auto applyThemeRecursive = [&](auto &self, const std::wstring &nameW) -> void {
            std::wstring themePath = ResolveThemePathW(nameW);
            if (themePath.empty() || !utils::FileExistsW(themePath)) {
                AP_Log(1, std::string("AnsiPalette: theme not found: ") + utils::Utf16ToUtf8(nameW));
                return;
            }
            if (visited.count(themePath)) {
                AP_Log(1, std::string("AnsiPalette: theme cycle detected at ") + utils::Utf16ToUtf8(themePath));
                return;
            }
            visited.insert(themePath);
            FILE *ft = _wfopen(themePath.c_str(), L"rb");
            if (!ft) return;
            fseek(ft, 0, SEEK_END);
            long tlen = ftell(ft);
            rewind(ft);
            if (tlen > 0) {
                std::string tbuf;
                tbuf.resize((size_t) tlen);
                if (fread(tbuf.data(), 1, (size_t) tlen, ft) == (size_t) tlen) {
                    std::string parent = ExtractThemeName(tbuf);
                    if (!parent.empty() && parent != "none") {
                        self(self, toW(parent));
                    }
                    std::string old = m_ParseOrigin;
                    m_ParseOrigin = utils::Utf16ToUtf8(themePath);
                    ParseBuffer(tbuf);
                    m_ParseOrigin = old;
                }
            }
            fclose(ft);
        };
        applyThemeRecursive(applyThemeRecursive, toW(topTheme));
    }
    // Apply main buffer overrides last
    ParseBuffer(buf);
}

void AnsiPalette::RgbToHsv(float r, float g, float b, float &h, float &s, float &v) {
    const float mx = std::max(r, std::max(g, b));
    const float mn = std::min(r, std::min(g, b));
    const float d = mx - mn;
    v = mx;
    s = (mx == 0.0f) ? 0.0f : d / mx;
    if (d == 0.0f) {
        h = 0.0f;
        return;
    }
    if (mx == r) h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else h = (r - g) / d + 4.0f;
    h /= 6.0f;
}

void AnsiPalette::HsvToRgb(float h, float s, float v, float &r, float &g, float &b) {
    if (s <= 0.0f) {
        r = g = b = v;
        return;
    }
    h = std::fmodf(h, 1.0f);
    if (h < 0.0f) h += 1.0f;
    const float i = std::floor(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    switch ((int)i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
}

// sRGB <-> Linear conversions (component in [0,1])
float AnsiPalette::SrgbToLinear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float AnsiPalette::LinearToSrgb(float c) {
    c = std::max(0.0f, std::min(1.0f, c));
    if (c <= 0.0031308f) return 12.92f * c;
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

ImU32 AnsiPalette::ApplyToning(ImU32 col) const {
    if (!m_ToningEnabled) return col;
    float a = ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    float r = ((col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
    float g = ((col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
    float b = ((col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    // HSV adjust
    float h, s, v; RgbToHsv(r, g, b, h, s, v);
    if (m_ToneBrightness != 0.0f) v = std::clamp(v + m_ToneBrightness, 0.0f, 1.0f);
    if (m_ToneSaturation != 0.0f) s = std::clamp(s + m_ToneSaturation, 0.0f, 1.0f);
    HsvToRgb(h, s, v, r, g, b);
    // Keep original alpha; no blending
    return IM_COL32((int)std::round(r * 255.0f), (int)std::round(g * 255.0f), (int)std::round(b * 255.0f), (int)std::round(a * 255.0f));
}

std::string AnsiPalette::GetActiveThemeName() const {
    std::wstring cfg = GetFilePathW();
    if (!utils::FileExistsW(cfg)) return {};
    std::wstring contentW = utils::ReadTextFileW(cfg);
    if (contentW.empty()) return {};
    std::string content = utils::Utf16ToAnsi(contentW);
    if (content.empty()) return {};
    std::string name = ExtractThemeName(content);
    if (name == "none") return {}; // normalize to empty
    return name;
}

std::vector<AnsiPalette::ThemeChainEntry> AnsiPalette::GetResolvedThemeChain() const {
    std::vector<ThemeChainEntry> chain;
    std::string top = GetActiveThemeName();
    if (top.empty() || top == "none") return chain;

    auto toW = [](const std::string &s) { return std::wstring(s.begin(), s.end()); };
    std::set<std::wstring> visited;
    std::string cur = top;
    while (true) {
        std::wstring p = ResolveThemePathW(toW(cur));
        if (p.empty() || !utils::FileExistsW(p)) {
            chain.push_back(ThemeChainEntry{cur, p, false});
            break;
        }
        if (visited.count(p)) {
            // Cycle detected, push and stop
            chain.push_back(ThemeChainEntry{cur, p, true});
            break;
        }
        visited.insert(p);

        // Push current
        chain.push_back(ThemeChainEntry{cur, p, true});

        // Read parent
        std::wstring w = utils::ReadTextFileW(p);
        if (w.empty()) break;
        std::string b = utils::Utf16ToAnsi(w);
        if (b.empty()) break;
        std::string parent = ExtractThemeName(b);
        if (parent.empty() || parent == "none") break;
        cur = parent;
    }
    return chain;
}

std::vector<std::string> AnsiPalette::GetAvailableThemes() const {
    std::vector<std::string> out;
    std::set<std::wstring> uniq;
    std::wstring dir = GetThemesDirW();
    if (utils::DirectoryExistsW(dir)) {
        auto add = [&](const std::wstring &pattern) {
            for (const auto &p : utils::ListFilesW(dir, pattern)) {
                std::wstring base = utils::RemoveExtensionW(utils::GetFileNameW(p));
                if (!base.empty()) uniq.insert(base);
            }
        };
        add(L"*.ini"); add(L"*.theme");
    }
    out.reserve(uniq.size());
    for (const auto &w : uniq) {
        std::string name = utils::Utf16ToAnsi(w);
        if (!name.empty()) out.push_back(name);
    }
    return out;
}

bool AnsiPalette::SetActiveThemeName(const std::string &name) {
    EnsureConfigExists();
    std::wstring cfg = GetFilePathW();

    std::wstring contentW = utils::ReadTextFileW(cfg);
    std::string content = utils::Utf16ToAnsi(contentW);
    if (content.empty()) content = "";

    const std::string nameLower = utils::ToLower(name);
    const bool clear = nameLower.empty() || nameLower == "none";

    std::vector<std::pair<std::string, std::string>> mut;
    if (clear) {
        mut.emplace_back("base", std::string("\x01__REMOVE__"));
        mut.emplace_back("theme", std::string("\x01__REMOVE__"));
    } else {
        mut.emplace_back("base", name);
    }

    std::string out = ApplyThemeMutationsToContent(content, mut);
    std::wstring outW(out.begin(), out.end());
    return utils::WriteTextFileW(cfg, outW);
}

// Internal helpers for editing [theme] section
static void AppendWithNewline(std::string &out, const std::string &line) {
    out += line;
    if (!line.empty() && line.back() != '\n' && line.back() != '\r')
        out += '\n';
}

static std::string CanonThemeKey(const std::string &key) {
    std::string k = utils::ToLower(key);
    if (k == "grey") k = "gray";
    if (k == "linear" || k == "linear_mix" || k == "mix_linear" || k == "mix_colorspace" || k == "mix_colourspace" || k
        == "color_space" || k == "colour_space") k = "mix_space";
    if (k == "mix" || k == "mix_intensity" || k == "mix_saturation") k = "mix_strength";
    if (k == "tone_enable" || k == "enable_toning") k = "toning";
    if (k == "cube_mode" || k == "cube_from_theme") k = "cube";
    if (k == "gray_mode" || k == "grey_mode" || k == "gray_from_theme" || k == "grey_from_theme") k = "gray";
    return k;
}

static bool ThemeKeyMatches(const std::string &lhs, const std::string &canonical) {
    std::string l = utils::ToLower(lhs);
    if (l == canonical) return true;
    if (canonical == "cube") return (l == "cube_mode" || l == "cube_from_theme");
    if (canonical == "gray") return (l == "grey" || l == "gray_mode" || l == "grey_mode" || l == "gray_from_theme" || l
        == "grey_from_theme");
    if (canonical == "toning") return (l == "tone_enable" || l == "enable_toning");
    if (canonical == "mix_strength") return (l == "mix" || l == "mix_intensity" || l == "mix_saturation");
    if (canonical == "mix_space") return (l == "mix_colorspace" || l == "mix_colourspace" || l == "color_space" || l ==
        "colour_space" || l == "linear_mix" || l == "linear" || l == "mix_linear");
    return false;
}

// Validate and normalize value for a given canonical key. Returns false if invalid.
static bool ValidateThemeOptionValue(const std::string &ckey, const std::string &input, std::string &out_value) {
    const std::string v = utils::ToLower(utils::TrimStringCopy(input));
    if (ckey == "cube" || ckey == "gray") {
        if (v == "standard" || v == "xterm" || v == "0" || v == "false" || v == "off" || v == "no") {
            out_value = "standard";
            return true;
        }
        if (v == "theme" || v == "mix" || v == "1" || v == "true" || v == "on" || v == "yes") {
            out_value = "theme";
            return true;
        }
        return false;
    }
    if (ckey == "mix_space") {
        if (v == "linear" || v == "on" || v == "true" || v == "yes") {
            out_value = "linear";
            return true;
        }
        if (v == "srgb" || v == "gamma" || v == "off" || v == "false" || v == "no") {
            out_value = "srgb";
            return true;
        }
        return false;
    }
    if (ckey == "toning") {
        if (v == "1" || v == "true" || v == "on" || v == "yes") {
            out_value = "on";
            return true;
        }
        if (v == "0" || v == "false" || v == "off" || v == "no") {
            out_value = "off";
            return true;
        }
        return false;
    }
    if (ckey == "mix_strength") {
        try {
            std::string vv = v;
            if (!vv.empty() && vv.back() == '%') vv.pop_back();
            float f = std::stof(vv);
            if (!std::isfinite(f)) return false;
            if (!v.empty() && v.back() == '%') {
                f *= 0.01f; // percent input
            } else if (f > 1.0f) {
                f *= 0.01f; // plain number >1, treat as percent for convenience
            }
            f = std::clamp(f, 0.0f, 1.0f);
            std::string buf(32, '\0');
            snprintf(buf.data(), buf.size(), "%.3f", f);
            out_value = utils::TrimStringCopy(buf);
            return true;
        } catch (...) { return false; }
    }
    if (ckey == "tone_brightness" || ckey == "tone_saturation") {
        try {
            float f = std::stof(v);
            if (!std::isfinite(f)) return false;
            f = std::clamp(f, -1.0f, 1.0f);
            std::string buf(32, '\0');
            snprintf(buf.data(), buf.size(), "%.3f", f);
            out_value = utils::TrimStringCopy(buf);
            return true;
        } catch (...) { return false; }
    }
    if (ckey == "base") {
        // Accept any non-empty token (theme name), normalize to lower-case base? preserve
        out_value = input;
        return true;
    }
    // Unknown keys: allow passthrough
    out_value = input;
    return true;
}

// Apply mutations to [theme] section. When removing keys, pass empty optional (value == "\x01__REMOVE__").
static std::string ApplyThemeMutationsToContent(const std::string &content, const std::vector<std::pair<std::string, std::string>> &mut) {
    // Build maps of canonical keys -> value or removal marker
    std::map<std::string, std::string> upserts; // key -> value ; value=="\x01__REMOVE__" means remove
    for (auto &kv : mut) {
        std::string ckey = CanonThemeKey(kv.first);
        // normalize value now? keep raw; SetThemeOption will pre-validate
        upserts[ckey] = kv.second;
    }

    std::string out;
    std::string section;
    bool inTheme = false;
    bool touched = false; // whether we replaced/removed/inserted

    size_t pos = 0;
    while (pos <= content.size()) {
        size_t line_end = content.find_first_of("\r\n", pos);
        size_t n = (line_end == std::string::npos) ? (content.size() - pos) : (line_end - pos);
        std::string line = content.substr(pos, n);
        pos = (line_end == std::string::npos) ? content.size() + 1 : (line_end + 1);

        std::string trimmed = utils::TrimStringCopy(line);
        // Section header
        if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
            // On exit of [theme] without touching, we may need to inject
            if (inTheme) {
                for (auto &kv : upserts) {
                    if (kv.second != "\x01__REMOVE__") {
                        AppendWithNewline(out, kv.first + " = " + kv.second);
                        touched = true;
                    }
                }
                // mark as consumed by clearing map so we don't append again at end
                upserts.clear();
            }
            section = utils::ToLower(trimmed.substr(1, trimmed.size() - 2));
            inTheme = (section == "theme");
            AppendWithNewline(out, line);
            continue;
        }

        if (inTheme) {
            // For each line inside theme, decide if to drop/replace
            size_t eq = trimmed.find('=');
            std::string lhs = (eq != std::string::npos)
                                  ? utils::TrimStringCopy(trimmed.substr(0, eq))
                                  : utils::TrimStringCopy(trimmed);
            bool matched = false;
            for (auto it = upserts.begin(); it != upserts.end();) {
                if (!lhs.empty() && (ThemeKeyMatches(lhs, it->first) || utils::ToLower(lhs) == it->first)) {
                    // replace or remove
                    if (it->second != "\x01__REMOVE__") { AppendWithNewline(out, it->first + " = " + it->second); }
                    touched = true;
                    it = upserts.erase(it);
                    matched = true;
                    break;
                } else ++it;
            }
            if (matched) continue; // line consumed
        }

        AppendWithNewline(out, line);
    }

    // If upserts remain and no [theme] encountered or we didn't inject yet
    if (!upserts.empty()) {
        if (!out.empty() && out.back() != '\n') out += '\n';
        out += "[theme]\n";
        for (auto &kv : upserts) {
            if (kv.second != "\x01__REMOVE__") {
                out += kv.first + " = " + kv.second + "\n";
                touched = true;
            }
        }
    }
    return out;
}

bool AnsiPalette::SetThemeOption(const std::string &key, const std::string &value) {
    EnsureConfigExists();
    std::wstring cfg = GetFilePathW();

    std::wstring contentW = utils::ReadTextFileW(cfg);
    std::string content = utils::Utf16ToAnsi(contentW);
    if (content.empty()) content = "";

    // Canonical key + validation
    std::string ckey = CanonThemeKey(key);
    std::string vnorm;
    if (!ValidateThemeOptionValue(ckey, value, vnorm)) return false;

    // Apply generic mutation (upsert or remove when value marker provided)
    std::vector<std::pair<std::string, std::string>> mut;
    mut.emplace_back(ckey, vnorm);
    std::string out = ApplyThemeMutationsToContent(content, mut);
    std::wstring outW(out.begin(), out.end());
    return utils::WriteTextFileW(cfg, outW);
}

bool AnsiPalette::ResetThemeOptions() {
    std::wstring cfg = GetFilePathW();
    if (!utils::FileExistsW(cfg)) return true; // nothing to reset
    std::wstring contentW = utils::ReadTextFileW(cfg);
    std::string content = utils::Utf16ToAnsi(contentW);
    if (content.empty()) return true;

    std::string out;
    bool inTheme = false;
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t line_end = content.find_first_of("\r\n", pos);
        size_t n = (line_end == std::string::npos) ? (content.size() - pos) : (line_end - pos);
        std::string line = content.substr(pos, n);
        pos = (line_end == std::string::npos) ? content.size() + 1 : (line_end + 1);

        std::string trimmed = utils::TrimStringCopy(line);
        if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
            std::string section = utils::ToLower(trimmed.substr(1, trimmed.size() - 2));
            inTheme = (section == "theme");
            if (inTheme) {
                // Skip header and all subsequent lines until next section
                continue;
            } else {
                AppendWithNewline(out, line);
                continue;
            }
        }
        if (inTheme) {
            // skip lines inside [theme]
            continue;
        }
        AppendWithNewline(out, line);
    }

    std::wstring outW(out.begin(), out.end());
    return utils::WriteTextFileW(cfg, outW);
}

void AnsiPalette::EnsureConfigExists() {
    std::wstring cfg = GetFilePathW();
    if (utils::FileExistsW(cfg)) return;
    // Try writing sample, otherwise ensure dir and minimal header
    if (!SaveSampleIfMissing()) {
        std::wstring dir = GetLoaderDirW();
        if (!utils::DirectoryExistsW(dir)) utils::CreateDirectoryW(dir);
        utils::WriteTextFileW(cfg, L"[theme]\n");
    }
}
