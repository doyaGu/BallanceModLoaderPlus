#ifndef BML_ANSI_PALETTE_H
#define BML_ANSI_PALETTE_H

#include <string>
#include "imgui.h"

class Ansi256Palette {
public:
    Ansi256Palette();

    // Ensure default palette is built and file overrides applied once.
    void EnsureInitialized();

    // Force reload overrides from file (rebuilds from default first). Returns true if file existed and parsed.
    bool ReloadFromFile();

    // Write a sample palette file to ModLoader directory if missing. Returns true if written.
    bool SaveSampleIfMissing();

    // Query color by index. Returns true and sets out_col when index is valid.
    bool GetColor(int index, ImU32 &outCol) const;

    // Whether palette is active (defaults or user palette loaded).
    bool IsActive() const;

    // Get config file absolute path (UTF-16). Useful for logs.
    std::wstring GetConfigPathW() const;

    // Utility helpers (need to be public for parser helpers)
    static ImU32 RGBA(unsigned r, unsigned g, unsigned b, unsigned a = 255);
    static ImU32 HexToImU32(const char *hex);

private:
    bool m_Initialized;
    bool m_Active;
    ImU32 m_Palette[256] = {};
    bool m_HasOverride[256] = {};

    void BuildDefault();
    void ParseBuffer(const std::string &buf);
    void RebuildCubeAndGray();
    void ApplyBase16(const std::string &base);
    std::wstring GetFilePathW() const;

    // Parameters for generating cube (16..231) and gray (232..255)
    float m_CubeSat = 0.60f;                           // HSV saturation multiplier
    float m_CubeVal = 1.00f;                           // HSV value multiplier
    int m_CubeValues[6] = {0, 95, 135, 175, 215, 255}; // 6x6x6 base levels

    int m_GrayStart = 8;       // base start value
    int m_GrayStep = 10;       // increment per step
    float m_GrayScale = 0.95f; // scaling factor
    float m_GrayBias = 6.0f;   // bias added after scaling
};

#endif // BML_ANSI_PALETTE_H
