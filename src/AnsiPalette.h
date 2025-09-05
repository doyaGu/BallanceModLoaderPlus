#ifndef BML_ANSI_PALETTE_H
#define BML_ANSI_PALETTE_H

#include <string>
#include <vector>

#include "imgui.h"

class AnsiPalette {
public:
    AnsiPalette();

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
    // Get themes directory absolute path (UTF-16).
    std::wstring GetThemesDirW() const;

    // Utility helpers (need to be public for parser helpers)
    static ImU32 RGBA(unsigned r, unsigned g, unsigned b, unsigned a = 255);
    static ImU32 HexToImU32(const char *hex);
    // Theme helpers for external tools/commands
    std::wstring ResolveThemePathW(const std::wstring &name) const;
    static std::string ExtractThemeName(const std::string &buf);

    // Allow injecting loader directory provider (returns absolute path to ModLoader dir).
    // If not set, implementation falls back to current working directory.
    static void SetLoaderDirProvider(std::wstring (*provider)());

    struct ThemeChainEntry {
        std::string name;     // theme logical name (UTF-8/ANSI)
        std::wstring path;    // resolved absolute path (UTF-16), empty if unresolved
        bool exists;          // whether the resolved file exists
    };

    // Read main config to get active top theme name ("" or "none" if none)
    std::string GetActiveThemeName() const;

    // Build resolved theme chain from top to root, with paths and existence flags.
    std::vector<ThemeChainEntry> GetResolvedThemeChain() const;

    // List available theme names (basename without extension) under themes directory.
    std::vector<std::string> GetAvailableThemes() const;

    // Set active theme name in main config. Pass empty string or "none" to clear.
    // Returns true if file was written successfully.
    bool SetActiveThemeName(const std::string &name);

private:
    bool m_Initialized;
    bool m_Active;
    ImU32 m_Palette[256] = {};
    bool m_HasOverride[256] = {};

    // Toning configuration
    bool m_ToningEnabled = false;
    float m_ToneBrightness = 0.0f;   // [-1, 1]
    float m_ToneSaturation = 0.0f;   // [-1, 1]

    // Cube generation mode: false -> standard xterm 6x6x6 values; true -> mix from theme primaries
    bool m_CubeMixFromTheme = false;
    // Gray generation mode: false -> standard xterm ramp; true -> mix between theme black/white
    bool m_GrayMixFromTheme = false;
    // Strength of mixing from theme when enabled, 0 = standard, 1 = full theme colors
    float m_MixStrength = 1.0f;     // [0, 1], blend theme-mix vs standard

    void BuildDefault();
    void ParseBuffer(const std::string &buf);
    void RebuildCubeAndGray();
    ImU32 ApplyToning(ImU32 col) const;
    void ApplyThemeChainAndBuffer(const std::string &buf);
    static void RgbToHsv(float r, float g, float b, float &h, float &s, float &v);
    static void HsvToRgb(float h, float s, float v, float &r, float &g, float &b);
    std::wstring GetFilePathW() const;
    std::wstring GetLoaderDirW() const;

    static std::wstring (*s_LoaderDirProvider)();
};

#endif // BML_ANSI_PALETTE_H
