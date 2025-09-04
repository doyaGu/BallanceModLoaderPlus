#include "AnsiPalette.h"

#include <cstdio>
#include <cwchar>
#include <string>
#include <algorithm>

#include "ModContext.h"
#include "PathUtils.h"

Ansi256Palette::Ansi256Palette() : m_Initialized(false), m_Active(false) {
    for (int i = 0; i < 256; ++i) {
        m_Palette[i] = IM_COL32_WHITE;
        m_HasOverride[i] = false;
    }
}

ImU32 Ansi256Palette::RGBA(unsigned r, unsigned g, unsigned b, unsigned a) {
    return IM_COL32((int)r, (int)g, (int)b, (int)a);
}

ImU32 Ansi256Palette::HexToImU32(const char *hex) {
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

std::wstring Ansi256Palette::GetFilePathW() const {
    std::wstring path = BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
    path.append(L"\\Ansi256Palette.cfg");
    return path;
}

void Ansi256Palette::BuildDefault() {
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
        out = Ansi256Palette::HexToImU32(val.c_str() + 1);
        return true;
    }
    int r = 0, g = 0, b = 0;
    if (sscanf(val.c_str(), "%d%*[, ]%d%*[, ]%d", &r, &g, &b) == 3) {
        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);
        out = Ansi256Palette::RGBA((unsigned) r, (unsigned) g, (unsigned) b, 255);
        return true;
    }
    return false;
}

void Ansi256Palette::ApplyBase16(const std::string &base) {
    const std::string b = ToLowerStr(base);
    if (b == "onedark" || b == "one-dark" || b == "one-dark-pro" || b == "onedarkpro") {
        // One Dark Pro-ish
        ImU32 stdc[8] = {
            RGBA(0x28, 0x2C, 0x34), // black
            RGBA(0xE0, 0x6C, 0x75), // red
            RGBA(0x98, 0xC3, 0x79), // green
            RGBA(0xE5, 0xC0, 0x7B), // yellow
            RGBA(0x61, 0xAF, 0xEF), // blue
            RGBA(0xC6, 0x78, 0xDD), // magenta
            RGBA(0x56, 0xB6, 0xC2), // cyan
            RGBA(0xAB, 0xB2, 0xBF)  // white
        };
        ImU32 brtc[8] = {
            RGBA(0x5C, 0x63, 0x70), // bright black
            RGBA(0xE0, 0x6C, 0x75), // bright red
            RGBA(0x98, 0xC3, 0x79), // bright green
            RGBA(0xD1, 0x9A, 0x66), // bright yellow (alt)
            RGBA(0x61, 0xAF, 0xEF), // bright blue
            RGBA(0xC6, 0x78, 0xDD), // bright magenta
            RGBA(0x56, 0xB6, 0xC2), // bright cyan
            RGBA(0xD7, 0xDA, 0xE0)  // bright white
        };
        for (int i = 0; i < 8; ++i) { m_Palette[i] = stdc[i]; }
        for (int i = 0; i < 8; ++i) { m_Palette[i + 8] = brtc[i]; }
    } else if (b == "nord") {
        // Nord base 16 (only set 0..15, do not reset cube/gray/overrides)
        ImU32 stdc[8] = {
            RGBA(0x3B, 0x42, 0x52), // black
            RGBA(0xBF, 0x61, 0x6A), // red
            RGBA(0xA3, 0xBE, 0x8C), // green
            RGBA(0xEB, 0xCB, 0x8B), // yellow
            RGBA(0x81, 0xA1, 0xC1), // blue
            RGBA(0xB4, 0x8E, 0xAD), // magenta
            RGBA(0x88, 0xC0, 0xD0), // cyan
            RGBA(0xE5, 0xE9, 0xF0)  // white
        };
        ImU32 brtc[8] = {
            RGBA(0x4C, 0x56, 0x6A), // bright black
            RGBA(0xBF, 0x61, 0x6A), // bright red
            RGBA(0xA3, 0xBE, 0x8C), // bright green
            RGBA(0xEB, 0xCB, 0x8B), // bright yellow
            RGBA(0x81, 0xA1, 0xC1), // bright blue
            RGBA(0xB4, 0x8E, 0xAD), // bright magenta
            RGBA(0x8F, 0xBC, 0xBB), // bright cyan
            RGBA(0xEC, 0xEF, 0xF4)  // bright white
        };
        for (int i = 0; i < 8; ++i) { m_Palette[i] = stdc[i]; }
        for (int i = 0; i < 8; ++i) { m_Palette[i + 8] = brtc[i]; }
    }
}

void Ansi256Palette::ParseBuffer(const std::string &buf) {
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
            // allow numeric indices too
            if (isNumber(lkey)) {
                int idx = std::stoi(lkey);
                if ((section == "standard" && idx >= 0 && idx <= 7) || (section == "bright" && idx >= 8 && idx <= 15)) {
                    setIndexColor(idx, val);
                }
            }
        } else if (section == "cube") {
            // index override within cube range only (no tuning keys)
            if (isNumber(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 16 && idx <= 231) setIndexColor(idx, val);
            }
        } else if (section == "gray" || section == "grayscale") {
            // index override within gray range only (no tuning keys)
            if (isNumber(lkey)) {
                int idx = std::stoi(lkey);
                if (idx >= 232 && idx <= 255) setIndexColor(idx, val);
            }
        } else if (section == "overrides") {
            size_t dash = lkey.find('-');
            if (dash != std::string::npos) {
                int a = std::stoi(lkey.substr(0, dash));
                int b = std::stoi(lkey.substr(dash + 1));
                if (a > b) std::swap(a, b);
                a = std::max(0, a);
                b = std::min(255, b);
                for (int i = a; i <= b; ++i) setIndexColor(i, val);
            } else {
                int idx = std::stoi(lkey);
                if (idx >= 0 && idx <= 255) {
                    setIndexColor(idx, val);
                }
            }
        } else if (section == "meta") {
            if (lkey == "base") {
                ApplyBase16(ToLowerStr(val));
            }
        } else {
            // Backward compatible: key as index
            int idx = std::stoi(lkey);
            if (idx >= 0 && idx <= 255) {
                setIndexColor(idx, val);
            }
        }
    }
}

void Ansi256Palette::RebuildCubeAndGray() {
    // Cube 16..231 (standard xterm)
    for (int idx = 16; idx < 232; ++idx) {
        if (m_HasOverride[idx]) continue;
        int v = idx - 16;
        int r6 = (v / 36) % 6;
        int g6 = ((v % 36) / 6) % 6;
        int b6 = v % 6;
        static const int values[6] = {0, 95, 135, 175, 215, 255};
        int r = values[r6];
        int g = values[g6];
        int b = values[b6];
        m_Palette[idx] = RGBA(r, g, b);
    }
    // Gray 232..255 (standard xterm)
    for (int idx = 232; idx < 256; ++idx) {
        if (m_HasOverride[idx]) continue;
        int gray = 8 + (idx - 232) * 10;
        m_Palette[idx] = RGBA(gray, gray, gray);
    }
}

void Ansi256Palette::EnsureInitialized() {
    if (m_Initialized) return;
    BuildDefault();
    // Load overrides if file exists
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
                if (fread(buf.data(), 1, (size_t) len, fp) == (size_t) len) {
                    ParseBuffer(buf);
                }
            }
            fclose(fp);
        }
    }
    RebuildCubeAndGray();
    m_Initialized = true;
}

bool Ansi256Palette::ReloadFromFile() {
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
            ParseBuffer(buf);
            ok = true;
        }
    }
    fclose(fp);
    RebuildCubeAndGray();
    m_Initialized = true;
    return ok;
}

bool Ansi256Palette::SaveSampleIfMissing() {
    std::wstring path = GetFilePathW();
    if (utils::FileExistsW(path)) return false;
    // ensure ModLoader dir exists
    std::wstring dir = BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
    if (!utils::DirectoryExistsW(dir)) utils::CreateDirectoryW(dir);
    FILE *fp = _wfopen(path.c_str(), L"wb");
    if (!fp) return false;
    const char *sample =
        "# Ansi256Palette.cfg\n"
        "# Sections: [meta], [standard], [bright], [overrides]\n"
        "# Colors accept #RRGGBB or R,G,B. Lines starting with # or ; are comments.\n\n"
        "[meta]\n"
        "# Optional base preset for 0..15: nord | onedark\n"
        "base = nord\n\n"
        "[standard]  # indices 0..7\n"
        "black  = #3B4252\n"
        "red    = #BF616A\n"
        "green  = #A3BE8C\n"
        "yellow = #EBCB8B\n"
        "blue   = #81A1C1\n"
        "magenta= #B48EAD\n"
        "cyan   = #88C0D0\n"
        "white  = #E5E9F0\n\n"
        "[bright]    # indices 8..15\n"
        "black  = #4C566A\n"
        "red    = #BF616A\n"
        "green  = #A3BE8C\n"
        "yellow = #EBCB8B\n"
        "blue   = #81A1C1\n"
        "magenta= #B48EAD\n"
        "cyan   = #8FBCBB\n"
        "white  = #ECEFF4\n\n"
        "[overrides] # override any index or range (0..255)\n"
        "# 196 = #FF0000\n"
        "# 232-239 = 180,180,180\n";
    fwrite(sample, 1, strlen(sample), fp);
    fclose(fp);
    return true;
}

bool Ansi256Palette::GetColor(int index, ImU32 &outCol) const {
    if (index < 0 || index > 255) {
        outCol = IM_COL32_WHITE;
        return false;
    }
    outCol = m_Palette[index];
    return true;
}

bool Ansi256Palette::IsActive() const {
    return m_Active;
}

std::wstring Ansi256Palette::GetConfigPathW() const { return GetFilePathW(); }
