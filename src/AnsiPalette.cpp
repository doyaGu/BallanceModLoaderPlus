#include "AnsiPalette.h"

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cmath>
#include <string>
#include <algorithm>
#include <set>

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
    m_MixStrength = 1.0f;
}

// Optional external provider for ModLoader directory
std::wstring (*AnsiPalette::s_LoaderDirProvider)() = nullptr;

void AnsiPalette::SetLoaderDirProvider(std::wstring (*provider)()) {
    s_LoaderDirProvider = provider;
}

ImU32 AnsiPalette::RGBA(unsigned r, unsigned g, unsigned b, unsigned a) {
    return IM_COL32((int)r, (int)g, (int)b, (int)a);
}

ImU32 AnsiPalette::HexToImU32(const char *hex) {
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

static std::string ToLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char) std::tolower(c); });
    return s;
}

static bool ParseColorVal(const std::string &val, ImU32 &out) {
    if (!val.empty() && val[0] == '#') {
        out = AnsiPalette::HexToImU32(val.c_str() + 1);
        return true;
    }
    int r = 0, g = 0, b = 0, a = 255;
    if (sscanf(val.c_str(), "%d%*[, ]%d%*[, ]%d%*[, ]%d", &r, &g, &b, &a) == 4) {
        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);
        a = std::clamp(a, 0, 255);
        out = AnsiPalette::RGBA((unsigned) r, (unsigned) g, (unsigned) b, (unsigned) a);
        return true;
    } else if (sscanf(val.c_str(), "%d%*[, ]%d%*[, ]%d", &r, &g, &b) == 3) {
        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);
        out = AnsiPalette::RGBA((unsigned) r, (unsigned) g, (unsigned) b, 255);
        return true;
    }
    return false;
}

void AnsiPalette::ParseBuffer(const std::string &buf) {
    std::string section;
    size_t pos = 0;
    auto ltrim = [](std::string &s) {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        s.erase(0, i);
    };
    auto rtrim = [](std::string &s) {
        size_t i = s.size();
        while (i > 0 && (s[i - 1] == ' ' || s[i - 1] == '\t' || s[i - 1] == '\r')) --i;
        s.erase(i);
    };
    auto isNumber = [](const std::string &s) -> bool {
        if (s.empty()) return false;
        for (unsigned char ch : s) {
            if (ch < '0' || ch > '9') return false;
        }
        return true;
    };
    while (pos < buf.size()) {
        size_t line_end = buf.find_first_of("\r\n", pos);
        size_t n = (line_end == std::string::npos) ? (buf.size() - pos) : (line_end - pos);
        std::string line = buf.substr(pos, n);
        pos = (line_end == std::string::npos) ? buf.size() : (line_end + 1);
        ltrim(line);
        rtrim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = ToLowerStr(line.substr(1, line.size() - 2));
            continue;
        }

        // Backward-compat: no section provided -> treat as overrides of index
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        ltrim(key);
        rtrim(key);
        ltrim(val);
        rtrim(val);
        std::string lkey = ToLowerStr(key);

        auto setIndexColor = [&](int idx, const std::string &v) {
            ImU32 c;
            if (ParseColorVal(v, c)) {
                m_Palette[idx] = c;
                m_HasOverride[idx] = true;
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
            if (isNumber(lkey)) {
                int idx = std::stoi(lkey);
                if ((section == "standard" && idx >= 0 && idx <= 7) || (section == "bright" && idx >= 8 && idx <= 15)) {
                    setIndexColor(idx, val);
                }
            }
        } else if (section == "cube") {
            // Index override within cube range only (no tuning keys)
            if (isNumber(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 16 && idx <= 231) setIndexColor(idx, val);
            }
        } else if (section == "gray" || section == "grayscale") {
            // Index override within gray range only (no tuning keys)
            if (isNumber(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 232 && idx <= 255) setIndexColor(idx, val);
            }
        } else if (section == "overrides") {
            size_t dash = lkey.find('-');
            if (dash != std::string::npos) {
                std::string as = lkey.substr(0, dash);
                std::string bs = lkey.substr(dash + 1);
                if (isNumber(as) && isNumber(bs)) {
                    int a = std::stoi(as);
                    int b = std::stoi(bs);
                    if (a > b) std::swap(a, b);
                    a = std::max(0, a);
                    b = std::min(255, b);
                    for (int i = a; i <= b; ++i) setIndexColor(i, val);
                }
            } else if (isNumber(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 0 && idx <= 255) {
                    setIndexColor(idx, val);
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
                }
            } else if (lkey == "gray" || lkey == "grey" || lkey == "gray_mode" || lkey == "grey_mode" || lkey == "gray_from_theme" || lkey == "grey_from_theme") {
                std::string v = ToLowerStr(val);
                if (v == "theme" || v == "mix" || v == "1" || v == "true" || v == "on" || v == "yes") {
                    m_GrayMixFromTheme = true;
                } else if (v == "standard" || v == "xterm" || v == "0" || v == "false" || v == "off" || v == "no") {
                    m_GrayMixFromTheme = false;
                }
            } else if (lkey == "tone_brightness") {
                try {
                    m_ToneBrightness = std::clamp(std::stof(val), -1.0f, 1.0f);
                } catch (...) {}
            } else if (lkey == "tone_saturation") {
                try {
                    m_ToneSaturation = std::clamp(std::stof(val), -1.0f, 1.0f);
                } catch (...) {}
            } else if (lkey == "mix_strength" || lkey == "mix" || lkey == "mix_intensity" || lkey == "mix_saturation") {
                try {
                    float f = std::stof(val);
                    if (f > 1.0f) f *= 0.01f; // allow percent
                    m_MixStrength = std::clamp(f, 0.0f, 1.0f);
                } catch (...) {}
            }
        } else {
            // Backward compatible: key as index only if numeric
            if (isNumber(lkey)) {
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
        // Mix cube colors from theme primaries (bright red/green/blue),
        // then blend with standard cube by m_MixStrength.
        auto to_f = [](ImU32 c, int shift) -> float {
            return ((c >> shift) & 0xFF) / 255.0f;
        };
        ImU32 redC   = m_Palette[8 + 1]; // bright red index 9
        ImU32 greenC = m_Palette[8 + 2]; // bright green index 10
        ImU32 blueC  = m_Palette[8 + 4]; // bright blue index 12
        float Rr = to_f(redC,   IM_COL32_R_SHIFT), Rg = to_f(redC,   IM_COL32_G_SHIFT), Rb = to_f(redC,   IM_COL32_B_SHIFT);
        float Gr = to_f(greenC, IM_COL32_R_SHIFT), Gg = to_f(greenC, IM_COL32_G_SHIFT), Gb = to_f(greenC, IM_COL32_B_SHIFT);
        float Br = to_f(blueC,  IM_COL32_R_SHIFT), Bg = to_f(blueC,  IM_COL32_G_SHIFT), Bb = to_f(blueC,  IM_COL32_B_SHIFT);
        float w[6]; for (int i = 0; i < 6; ++i) w[i] = values[i] / 255.0f;
        for (int idx = 16; idx < 232; ++idx) {
            if (m_HasOverride[idx]) continue;
            int v = idx - 16;
            int r6 = (v / 36) % 6;
            int g6 = ((v % 36) / 6) % 6;
            int b6 = v % 6;
            // Theme-mix in 0..1
            float wR = w[r6], wG = w[g6], wB = w[b6];
            float rr = wR * Rr + wG * Gr + wB * Br;
            float gg = wR * Rg + wG * Gg + wB * Bg;
            float bb = wR * Rb + wG * Gb + wB * Bb;
            // Standard in 0..1
            float sr = w[r6];
            float sg = w[g6];
            float sb = w[b6];
            // Blend
            float t = m_MixStrength;
            float fr = sr * (1.0f - t) + rr * t;
            float fg = sg * (1.0f - t) + gg * t;
            float fb = sb * (1.0f - t) + bb * t;
            int ri = (int)std::round(std::clamp(fr, 0.0f, 1.0f) * 255.0f);
            int gi = (int)std::round(std::clamp(fg, 0.0f, 1.0f) * 255.0f);
            int bi = (int)std::round(std::clamp(fb, 0.0f, 1.0f) * 255.0f);
            m_Palette[idx] = RGBA((unsigned)ri, (unsigned)gi, (unsigned)bi);
        }
    }
    // Gray 232..255
    if (!m_GrayMixFromTheme) {
        // standard xterm ramp (8..238 step 10)
        for (int idx = 232; idx < 256; ++idx) {
            if (m_HasOverride[idx]) continue;
            int gray = 8 + (idx - 232) * 10;
            m_Palette[idx] = RGBA(gray, gray, gray);
        }
    } else {
        // mix between theme black (index 0) and theme white (index 7),
        // then blend with standard gray by m_MixStrength.
        ImU32 cBlack = m_Palette[0];
        ImU32 cWhite = m_Palette[7];
        auto chan = [](ImU32 c, int sh){ return (float)((c >> sh) & 0xFF) / 255.0f; };
        float br = chan(cBlack, IM_COL32_R_SHIFT), bg = chan(cBlack, IM_COL32_G_SHIFT), bb = chan(cBlack, IM_COL32_B_SHIFT);
        float wr = chan(cWhite, IM_COL32_R_SHIFT), wg = chan(cWhite, IM_COL32_G_SHIFT), wb = chan(cWhite, IM_COL32_B_SHIFT);
        for (int idx = 232; idx < 256; ++idx) {
            if (m_HasOverride[idx]) continue;
            int gray = 8 + (idx - 232) * 10; // 8..238
            float t = (float)gray / 255.0f;   // map to 0..~0.933 to keep xterm-style headroom
            float tr = br * (1.0f - t) + wr * t;
            float tg = bg * (1.0f - t) + wg * t;
            float tb = bb * (1.0f - t) + wb * t;
            float sr = gray / 255.0f, sg = sr, sb = sr;
            float ms = m_MixStrength;
            float fr = sr * (1.0f - ms) + tr * ms;
            float fg = sg * (1.0f - ms) + tg * ms;
            float fb = sb * (1.0f - ms) + tb * ms;
            int ri = (int)std::round(std::clamp(fr, 0.0f, 1.0f) * 255.0f);
            int gi = (int)std::round(std::clamp(fg, 0.0f, 1.0f) * 255.0f);
            int bi = (int)std::round(std::clamp(fb, 0.0f, 1.0f) * 255.0f);
            m_Palette[idx] = RGBA((unsigned)ri, (unsigned)gi, (unsigned)bi);
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
                if (!buf.empty()) ApplyThemeChainAndBuffer(buf);
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
    if (!fp) { m_Initialized = true; return false; }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);
    bool ok = false;
    if (len > 0) {
        std::string buf; buf.resize((size_t) len);
        if (fread(buf.data(), 1, (size_t) len, fp) == (size_t) len) {
            ApplyThemeChainAndBuffer(buf);
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
            "# Colors accept #RRGGBB, #AARRGGBB, or R,G,B[,A]. Lines starting with # or ; are comments.\n\n"
            "[theme]\n"
            "# Import a theme by uncommenting the next line:\n"
            "# base = nord\n"
            "# Optional toning (applied to palette indices only).\n"
            "toning = off\n"
            "tone_brightness = 0\n"
            "tone_saturation = 0\n"
            "# Mix strength for theme-based cube/gray [0..1] or percent\n"
            "mix_strength = 1\n"
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
        std::wstring p = themesDir; p.append(L"\\"); p.append(name).append(L".ini");
        if (!utils::FileExistsW(p)) {
            FILE *f = _wfopen(p.c_str(), L"wb"); if (!f) return; fwrite(content, 1, strlen(content), f); fclose(f);
        }
    };

    // Provide a Nord-style theme as a sample file (explicit colors)
    const char *nordTheme =
        "# theme: nord\n"
        "[theme]\n"
        "# You may chain to a parent theme here, e.g.:\n"
        "# base = parent-theme-name\n"
        "mix_strength = 1\n"
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
        "mix_strength = 1\n"
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
        std::transform(el.begin(), el.end(), el.begin(), [](wchar_t c){ return (wchar_t)std::towlower(c); });
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
    auto ltrim = [](std::string &s) {
        size_t i = 0; while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i; s.erase(0, i);
    };
    auto rtrim = [](std::string &s) {
        size_t i = s.size(); while (i > 0 && (s[i-1] == ' ' || s[i-1] == '\t' || s[i-1] == '\r')) --i; s.erase(i);
    };
    while (pos < buf.size()) {
        size_t line_end = buf.find_first_of("\r\n", pos);
        size_t n = (line_end == std::string::npos) ? (buf.size() - pos) : (line_end - pos);
        std::string line = buf.substr(pos, n);
        pos = (line_end == std::string::npos) ? buf.size() : (line_end + 1);
        ltrim(line); rtrim(line);
        if (line.empty() || line[0]=='#' || line[0]==';') continue;
        if (line.front()=='[' && line.back()==']') { section = ToLowerStr(line.substr(1, line.size()-2)); continue; }
        size_t eq = line.find('='); if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        ltrim(key); rtrim(key); ltrim(val); rtrim(val);
        std::string lkey = ToLowerStr(key);
        if ((section == "theme") && (lkey == "theme" || lkey == "base")) {
            return ToLowerStr(val);
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
            if (themePath.empty() || !utils::FileExistsW(themePath) || visited.count(themePath)) return;
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
                    ParseBuffer(tbuf);
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
    if (d == 0.0f) { h = 0.0f; return; }
    if (mx == r) h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else h = (r - g) / d + 4.0f;
    h /= 6.0f;
}

void AnsiPalette::HsvToRgb(float h, float s, float v, float &r, float &g, float &b) {
    if (s <= 0.0f) { r = g = b = v; return; }
    h = std::fmodf(h, 1.0f); if (h < 0.0f) h += 1.0f;
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
    return ExtractThemeName(content);
}

std::vector<AnsiPalette::ThemeChainEntry> AnsiPalette::GetResolvedThemeChain() const {
    std::vector<ThemeChainEntry> chain;
    std::string top = GetActiveThemeName();
    if (top.empty() || top == "none") return chain;

    auto toW = [](const std::string &s){ return std::wstring(s.begin(), s.end()); };
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
    std::wstring cfg = GetFilePathW();
    if (!utils::FileExistsW(cfg)) {
        // Try to generate a sample config first
        if (!SaveSampleIfMissing()) {
            // Ensure directory exists at least
            std::wstring dir = GetLoaderDirW();
            if (!utils::DirectoryExistsW(dir)) utils::CreateDirectoryW(dir);
            // Create a minimal file
            utils::WriteTextFileW(cfg, L"[theme]\n");
        }
    }

    std::wstring contentW = utils::ReadTextFileW(cfg);
    std::string content = utils::Utf16ToAnsi(contentW);
    if (content.empty()) content = "";

    const std::string nameLower = utils::ToLower(name);
    const bool clear = nameLower.empty() || nameLower == "none";

    std::string out;
    std::string section;
    bool inTheme = false;
    bool themeSectionExists = false;
    bool baseInsertedInTheme = false;

    size_t pos = 0;
    auto append_newline_if_needed = [&](const std::string &s) {
        if (!s.empty() && s.back() != '\n' && s.back() != '\r') out += '\n';
    };

    while (pos <= content.size()) {
        size_t line_end = content.find_first_of("\r\n", pos);
        size_t n = (line_end == std::string::npos) ? (content.size() - pos) : (line_end - pos);
        std::string line = content.substr(pos, n);
        pos = (line_end == std::string::npos) ? content.size() + 1 : (line_end + 1);

        std::string trimmed = utils::TrimStringCopy(line);
        // Section header
        if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
            // Before switching out of [theme], if we wanted to set and haven't inserted base, we already insert at header time
            section = utils::ToLower(trimmed.substr(1, trimmed.size() - 2));
            inTheme = (section == "theme");
            if (inTheme) {
                themeSectionExists = true;
                baseInsertedInTheme = false;
                // Write header line
                out += line;
                append_newline_if_needed(line);
                // Insert base line at start of [theme] if setting
                if (!clear) {
                    out += "base = "; out += name; out += "\n";
                    baseInsertedInTheme = true;
                }
                continue;
            }
            // Non-theme section: just copy header
            out += line; append_newline_if_needed(line);
            continue;
        }

        // Within a section: possibly drop/replace lines
        if (!section.empty()) {
            std::string l = utils::ToLower(trimmed);
            if (section == "theme") {
                if (!l.empty() && (utils::StartsWith(l, std::string("base")) || utils::StartsWith(l, std::string("theme")))) {
                    // Drop existing base/theme lines. We already inserted a fresh base at section start if needed.
                    continue;
                }
            }
        }
        // Default: copy line
        out += line; if (line_end != std::string::npos) out += "\n";
    }

    // If setting and no [theme] section existed, append it at end
    if (!clear && !themeSectionExists) {
        if (!out.empty() && out.back() != '\n') out += '\n';
        out += "[theme]\n";
        out += "base = "; out += name; out += "\n";
    }

    std::wstring outW(out.begin(), out.end());
    return utils::WriteTextFileW(cfg, outW);
}
