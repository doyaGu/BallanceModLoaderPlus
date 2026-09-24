#include "UI/FontRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string_view>
#include <utility>

#include <utf8.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#ifdef CompareString
#undef CompareString
#endif

#include "imgui.h"

#include "PathUtils.h"
#include "StringUtils.h"

namespace BML::UI {
namespace {
    constexpr float DefaultReferenceSize = 32.0f;
    constexpr float ReferenceViewportHeight = 1200.0f;

    struct WindowsFallbackFace {
        const wchar_t *Filename;
        const char *Label;
    };

    constexpr WindowsFallbackFace WindowsFallbackFaces[] = {
        {L"seguiemj.ttf", "Segoe UI Emoji"},
        {L"seguisym.ttf", "Segoe UI Symbol"},
    };

    class CurrentContext final {
    public:
        explicit CurrentContext(ImGuiContext &context)
            : m_Previous(ImGui::GetCurrentContext()) {
            if (m_Previous != &context)
                ImGui::SetCurrentContext(&context);
        }

        ~CurrentContext() {
            if (ImGui::GetCurrentContext() != m_Previous)
                ImGui::SetCurrentContext(m_Previous);
        }

        CurrentContext(const CurrentContext &) = delete;
        CurrentContext &operator=(const CurrentContext &) = delete;

    private:
        ImGuiContext *m_Previous;
    };

    bool EqualFace(const std::string &left, const std::string &right) {
        return utils::CompareString(left, right) == 0;
    }

    bool FaceLess(const std::string &left, const std::string &right) {
        return utils::CompareString(left, right) < 0;
    }

    bool ContainsFace(const std::vector<std::string> &faces, const std::string &candidate) {
        for (const std::string &face : faces) {
            if (EqualFace(face, candidate))
                return true;
        }
        return false;
    }

    std::string ResolveConfiguredFace(const std::string &loaderFontDirectory, const std::string &requested,
                                      FontSourceOrigin &origin) {
        if (requested.empty())
            return {};

        if (utils::IsAbsolutePathUtf8(requested)) {
            origin = FontSourceOrigin::Configured;
            return utils::FileExistsUtf8(requested) ? requested : std::string{};
        }

        if (utils::GetFileNameUtf8(requested) != requested)
            return {};

        origin = FontSourceOrigin::LoaderCatalog;
        const std::string loaderPath = utils::CombinePathUtf8(loaderFontDirectory, requested);
        if (utils::FileExistsUtf8(loaderPath))
            return loaderPath;

        return {};
    }

    std::string ResolveWindowsFace(std::wstring_view filename) {
        std::array<wchar_t, MAX_PATH> windowsDirectory{};
        const UINT length = GetWindowsDirectoryW(windowsDirectory.data(),
                                                static_cast<UINT>(windowsDirectory.size()));
        if (length == 0 || length >= windowsDirectory.size())
            return {};

        std::wstring path(windowsDirectory.data(), length);
        path.append(L"\\Fonts\\");
        path.append(filename);
        return utils::FileExistsW(path) ? utils::Utf16ToUtf8(path) : std::string{};
    }

    std::string FaceName(const std::string &path) {
        const std::string::size_type separator = path.find_last_of("/\\");
        return separator == std::string::npos ? path : path.substr(separator + 1);
    }

    ImFont *AddPrimary(ImFontAtlas &atlas, const std::string &path, float referenceSize) {
        ImFontConfig config;
        config.SizePixels = referenceSize;
        config.Flags |= ImFontFlags_NoLoadError;
        std::snprintf(config.Name, sizeof(config.Name), "%s", FaceName(path).c_str());
        return atlas.AddFontFromFileTTF(path.c_str(), referenceSize, &config, nullptr);
    }

    std::optional<float> ReadAscent(ImFont *font, float referenceSize) {
        if (!font || referenceSize <= 0.0f)
            return std::nullopt;

        ImFontBaked *baked = font->GetFontBaked(referenceSize);
        if (!baked || !std::isfinite(baked->Ascent))
            return std::nullopt;
        return baked->Ascent;
    }

    std::optional<float> MeasureFaceAscent(const ImFontAtlas &activeAtlas, const std::string &path, float referenceSize) {
        // Ask the same ImGui font loader for the source's standalone metrics,
        // but keep the probe out of the live dynamic atlas. This preserves the
        // renderer texture queue and fonts registered by Native Mods.
        ImFontAtlas probeAtlas;
        probeAtlas.SetFontLoader(activeAtlas.FontLoader);
        probeAtlas.FontLoaderFlags = activeAtlas.FontLoaderFlags;

        ImFontConfig config;
        config.SizePixels = referenceSize;
        config.Flags |= ImFontFlags_NoLoadError;
        ImFont *font = probeAtlas.AddFontFromFileTTF(path.c_str(), referenceSize, &config, nullptr);
        if (!font)
            return std::nullopt;
        return ReadAscent(font, referenceSize);
    }

    bool MergeFace(ImFontAtlas &atlas, ImFont *destination, const std::string &path, float referenceSize,
                   const std::optional<float> &primaryAscent) {
        if (!destination || path.empty())
            return false;

        const std::optional<float> fallbackAscent = MeasureFaceAscent(atlas, path, referenceSize);
        if (!primaryAscent || !fallbackAscent)
            return false;

        ImFontConfig config;
        config.MergeMode = true;
        config.DstFont = destination;
        config.SizePixels = referenceSize;
        // Preserve the fallback's standalone baseline. Without this offset,
        // ImGui aligns every merged source to the primary font's ascent and
        // may place CJK or emoji glyphs above ordinary widget clip rectangles.
        config.GlyphOffset.y = *fallbackAscent - *primaryAscent;
        config.Flags |= ImFontFlags_NoLoadError;
        std::snprintf(config.Name, sizeof(config.Name), "%s fallback", FaceName(path).c_str());
        ImFont *font = atlas.AddFontFromFileTTF(path.c_str(), referenceSize, &config, nullptr);
        return font != nullptr;
    }

    bool LoadFallback(ImFontAtlas &atlas, ImFont *destination, const std::string &path, float referenceSize,
                      const std::optional<float> &primaryAscent,
                      std::vector<std::string> &loadedPaths) {
        if (path.empty())
            return false;
        if (ContainsFace(loadedPaths, path))
            return true;
        if (!MergeFace(atlas, destination, path, referenceSize, primaryAscent))
            return false;
        loadedPaths.push_back(path);
        return true;
    }

    void AddDiagnostic(FontRuntimeSnapshot &snapshot, std::string diagnostic) {
        snapshot.Diagnostics.push_back(std::move(diagnostic));
        snapshot.State = FontRuntimeState::Degraded;
    }
}

FontRuntime::FontRuntime(std::string loaderFontDirectory)
    : m_LoaderFontDirectory(std::move(loaderFontDirectory)) {}

FontProfile FontRuntime::Normalize(FontProfile profile) {
    utils::TrimString(profile.PrimaryFace);
    if (profile.PrimaryFace.empty())
        profile.PrimaryFace = "unifont.otf";

    if (!std::isfinite(profile.ReferenceSize))
        profile.ReferenceSize = DefaultReferenceSize;
    profile.ReferenceSize = std::clamp(profile.ReferenceSize, MinimumFontReferenceSize, MaximumFontReferenceSize);
    if (!std::isfinite(profile.FallbackReferenceSize))
        profile.FallbackReferenceSize = DefaultReferenceSize;
    profile.FallbackReferenceSize = std::clamp(profile.FallbackReferenceSize, MinimumFontReferenceSize, MaximumFontReferenceSize);

    std::vector<std::string> fallbacks;
    fallbacks.reserve(profile.FallbackFaces.size());
    for (std::string fallback : profile.FallbackFaces) {
        utils::TrimString(fallback);
        if (fallback.empty() || EqualFace(fallback, profile.PrimaryFace))
            continue;
        if (!ContainsFace(fallbacks, fallback))
            fallbacks.push_back(std::move(fallback));
    }
    profile.FallbackFaces = std::move(fallbacks);
    return profile;
}

void FontRuntime::Configure(FontProfile profile) {
    profile = Normalize(std::move(profile));
    if (m_HasRequestedProfile && profile == m_RequestedProfile)
        return;

    m_RequestedProfile = std::move(profile);
    m_HasRequestedProfile = true;
    m_Snapshot.State = FontRuntimeState::Pending;
}

bool FontRuntime::RequestReload() {
    if (!m_HasRequestedProfile)
        return false;

    m_HasAppliedProfile = false;
    m_Snapshot.State = FontRuntimeState::Pending;
    return true;
}

FontCoverage FontRuntime::InspectText(const std::string &text) const {
    FontCoverage coverage;
    coverage.ValidUtf8 = utf8valid(reinterpret_cast<const utf8_int8_t *>(text.c_str())) == nullptr;
    coverage.FontAvailable = m_Atlas && m_RuntimeFont && m_Atlas->Fonts.contains(m_RuntimeFont);
    if (!coverage.ValidUtf8 || !coverage.FontAvailable)
        return coverage;

    const utf8_int8_t *cursor = reinterpret_cast<const utf8_int8_t *>(text.c_str());
    while (*cursor != '\0') {
        utf8_int32_t decoded = 0;
        cursor = utf8codepoint(cursor, &decoded);
        const std::uint32_t codepoint = static_cast<std::uint32_t>(decoded);
        ++coverage.CodepointCount;
        if (m_RuntimeFont->IsGlyphInFont(static_cast<ImWchar>(codepoint)))
            continue;
        if (std::find(coverage.MissingCodepoints.begin(),
                      coverage.MissingCodepoints.end(), codepoint) ==
            coverage.MissingCodepoints.end()) {
            coverage.MissingCodepoints.push_back(codepoint);
        }
    }
    return coverage;
}

std::vector<std::string> FontRuntime::ListLoaderFaces() const {
    std::vector<std::string> faces;
    for (std::string filename : utils::ListFilesUtf8(m_LoaderFontDirectory, "*")) {
        if (!utils::EndsWith(filename, ".ttf", false) &&
            !utils::EndsWith(filename, ".otf", false) &&
            !utils::EndsWith(filename, ".ttc", false))
            continue;
        faces.push_back(std::move(filename));
    }

    std::sort(faces.begin(), faces.end(), FaceLess);
    faces.erase(std::unique(faces.begin(), faces.end(), EqualFace), faces.end());
    return faces;
}

void FontRuntime::Synchronize(ImGuiContext &context, float viewportHeight) {
    const CurrentContext currentContext(context);

    if (viewportHeight > 0.0f && std::isfinite(viewportHeight))
        ImGui::GetStyle().FontScaleMain = viewportHeight / ReferenceViewportHeight;

    if (m_HasRequestedProfile && (!m_HasAppliedProfile || !(m_AppliedProfile == m_RequestedProfile))) {
        ImFontAtlas *atlas = ImGui::GetIO().Fonts;
        if (atlas && !atlas->Locked)
            Apply();
    }
}

void FontRuntime::Apply() {
    ImGuiIO &io = ImGui::GetIO();
    ImFontAtlas &atlas = *io.Fonts;

    FontRuntimeSnapshot next;
    next.State = FontRuntimeState::Ready;
    next.Generation = m_Snapshot.Generation + 1;
    next.ReferenceSize = m_RequestedProfile.ReferenceSize;
    next.FallbackReferenceSize = m_RequestedProfile.FallbackReferenceSize;
    next.SupportsUnicodeScalars = sizeof(ImWchar) == sizeof(ImWchar32);
    next.SupportsColorEmoji = false;

    if (m_Atlas == &atlas && m_RuntimeFont && atlas.Fonts.contains(m_RuntimeFont))
        atlas.RemoveFont(m_RuntimeFont);
    m_Atlas = &atlas;
    m_RuntimeFont = nullptr;

    FontSourceOrigin primaryOrigin = FontSourceOrigin::Configured;
    const std::string primaryPath = ResolveConfiguredFace(
        m_LoaderFontDirectory, m_RequestedProfile.PrimaryFace, primaryOrigin);
    FontSourceStatus primaryStatus{
        m_RequestedProfile.PrimaryFace, primaryPath, primaryOrigin, false};

    ImFont *font = nullptr;
    if (!primaryPath.empty())
        font = AddPrimary(atlas, primaryPath, m_RequestedProfile.ReferenceSize);
    primaryStatus.Loaded = font != nullptr;
    next.Sources.push_back(primaryStatus);

    if (!font) {
        ImFontConfig config;
        config.SizePixels = m_RequestedProfile.ReferenceSize;
        font = atlas.AddFontDefaultVector(&config);
        next.Sources.push_back({"ImGui embedded vector font", {},
                                FontSourceOrigin::Embedded, font != nullptr});
        AddDiagnostic(next, "Primary font could not be loaded: " +
                            m_RequestedProfile.PrimaryFace);
    }

    std::vector<std::string> loadedPaths;
    if (!primaryPath.empty() && primaryStatus.Loaded)
        loadedPaths.push_back(primaryPath);
    const std::optional<float> primaryAscent = ReadAscent(font, m_RequestedProfile.ReferenceSize);

    for (const std::string &requested : m_RequestedProfile.FallbackFaces) {
        FontSourceOrigin origin = FontSourceOrigin::Configured;
        const std::string path = ResolveConfiguredFace(
            m_LoaderFontDirectory, requested, origin);
        const bool loaded = LoadFallback(
            atlas, font, path, m_RequestedProfile.FallbackReferenceSize,
            primaryAscent, loadedPaths);
        next.Sources.push_back({requested, path, origin, loaded});
        if (!loaded)
            AddDiagnostic(next, "Fallback font could not be loaded: " + requested);
    }

    if (m_RequestedProfile.UseWindowsFallbacks) {
        for (const WindowsFallbackFace &windowsFace : WindowsFallbackFaces) {
            const std::string path = ResolveWindowsFace(windowsFace.Filename);
            const bool loaded = LoadFallback(
                atlas, font, path, m_RequestedProfile.FallbackReferenceSize,
                primaryAscent, loadedPaths);
            next.Sources.push_back({
                windowsFace.Label, path, FontSourceOrigin::WindowsCatalog, loaded});
        }
    }

    io.FontDefault = font;
    m_RuntimeFont = font;
    next.SupportsCommonEmoji = font && next.SupportsUnicodeScalars &&
        font->IsGlyphInFont(static_cast<ImWchar>(0x1F600)) &&
        font->IsGlyphInFont(static_cast<ImWchar>(0x1F44D)) &&
        font->IsGlyphInFont(static_cast<ImWchar>(0x1F680));
    if (!next.SupportsUnicodeScalars)
        AddDiagnostic(next, "ImGui was built without supplementary-plane Unicode support.");
    if (!next.SupportsCommonEmoji)
        AddDiagnostic(next, "No loaded fallback covers the common emoji probe set.");

    m_AppliedProfile = m_RequestedProfile;
    m_HasAppliedProfile = true;
    m_Snapshot = std::move(next);
}

} // namespace BML::UI
