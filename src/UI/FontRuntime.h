#ifndef BML_UI_FONT_RUNTIME_H
#define BML_UI_FONT_RUNTIME_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ImGuiContext;
struct ImFont;
struct ImFontAtlas;

namespace BML::UI {

inline constexpr float MinimumFontReferenceSize = 8.0f;
inline constexpr float MaximumFontReferenceSize = 96.0f;

struct FontProfile {
    std::string PrimaryFace = "unifont.otf";
    std::vector<std::string> FallbackFaces;
    float ReferenceSize = 32.0f;
    bool UseWindowsFallbacks = true;

    bool operator==(const FontProfile &) const = default;
};

enum class FontRuntimeState {
    Unconfigured,
    Pending,
    Ready,
    Degraded,
};

enum class FontSourceOrigin {
    Configured,
    LoaderCatalog,
    WindowsCatalog,
    Embedded,
};

struct FontSourceStatus {
    std::string RequestedFace;
    std::string ResolvedPath;
    FontSourceOrigin Origin = FontSourceOrigin::Configured;
    bool Loaded = false;
};

struct FontRuntimeSnapshot {
    FontRuntimeState State = FontRuntimeState::Unconfigured;
    std::uint64_t Generation = 0;
    float ReferenceSize = 32.0f;
    std::vector<FontSourceStatus> Sources;
    std::vector<std::string> Diagnostics;
    bool SupportsUnicodeScalars = false;
    bool SupportsCommonEmoji = false;
    bool SupportsColorEmoji = false;
};

struct FontCoverage {
    bool FontAvailable = false;
    bool ValidUtf8 = false;
    std::size_t CodepointCount = 0;
    std::vector<std::uint32_t> MissingCodepoints;
};

// Owns the configured ImGui font as loader-lifetime state. Configure() may be
// called from inside a frame; Synchronize() applies the latest profile only at
// the safe seam immediately before the next ImGui frame.
class FontRuntime {
public:
    explicit FontRuntime(std::string loaderFontDirectory);

    void Configure(FontProfile profile);
    bool RequestReload();
    void Synchronize(ImGuiContext &context, float viewportHeight);
    std::vector<std::string> ListLoaderFaces() const;
    FontCoverage InspectText(const std::string &text) const;
    const FontRuntimeSnapshot &Inspect() const noexcept { return m_Snapshot; }

private:
    static FontProfile Normalize(FontProfile profile);
    void Apply();

    std::string m_LoaderFontDirectory;
    FontProfile m_RequestedProfile;
    FontProfile m_AppliedProfile;
    FontRuntimeSnapshot m_Snapshot;
    ImFontAtlas *m_Atlas = nullptr;
    ImFont *m_RuntimeFont = nullptr;
    bool m_HasRequestedProfile = false;
    bool m_HasAppliedProfile = false;
};

} // namespace BML::UI

#endif // BML_UI_FONT_RUNTIME_H
