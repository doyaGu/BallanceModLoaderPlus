#include "Diagnostics/SystemReport.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winternl.h>

#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "BML/BML.h"
#include "CKRenderContext.h"
#include "CKRenderManager.h"
#include "CryptoUtils.h"
#include "Hooks/HookLifecycle.h"
#include "Loader/ModContext.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "UI/FontRuntime.h"
#include "UI/Ime/Runtime.h"
#include "UI/Overlay.h"
#include "UI/imgui_impl_ck2.h"
#include "imgui.h"

namespace BML::Diagnostics {
    namespace {
        using RtlGetVersionFunction = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);

        const char *YesNo(bool value) {
            return value ? "yes" : "no";
        }

        const char *FontStateName(UI::FontRuntimeState state) {
            switch (state) {
            case UI::FontRuntimeState::Unconfigured: return "unconfigured";
            case UI::FontRuntimeState::Pending: return "pending";
            case UI::FontRuntimeState::Ready: return "ready";
            case UI::FontRuntimeState::Degraded: return "degraded";
            }
            return "unknown";
        }

        const char *TextureStatusName(ImTextureStatus status) {
            switch (status) {
            case ImTextureStatus_OK: return "ready";
            case ImTextureStatus_Destroyed: return "destroyed";
            case ImTextureStatus_WantCreate: return "pending-create";
            case ImTextureStatus_WantUpdates: return "pending-update";
            case ImTextureStatus_WantDestroy: return "pending-destroy";
            }
            return "unknown";
        }

        std::string WindowsVersion() {
            RTL_OSVERSIONINFOW version{};
            version.dwOSVersionInfoSize = sizeof(version);
            const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
            const auto getVersion = ntdll ? reinterpret_cast<RtlGetVersionFunction>(
                                                ::GetProcAddress(ntdll, "RtlGetVersion")) :
                                            nullptr;
            if (!getVersion || getVersion(&version) != 0)
                return "unknown";

            std::ostringstream output;
            output << version.dwMajorVersion << '.' << version.dwMinorVersion
                   << " build " << version.dwBuildNumber;
            return output.str();
        }

        std::string ShortHash(const std::wstring &path) {
            const std::string hash = utils::Sha256FileHex(path);
            return hash.empty() ? "unavailable" : hash.substr(0, 12);
        }

        struct InstallFingerprint {
            bool AnsiSafe = false;
            bool ShortPathAvailable = false;
            std::string PlayerHash;
            std::string Ck2Hash;
            std::string RendererHash;
        };

        InstallFingerprint BuildInstallFingerprint(const std::wstring &gameRoot) {
            InstallFingerprint fingerprint;
            std::string encodedPath;
            fingerprint.AnsiSafe = utils::TryEncodePathForActiveCodePage(gameRoot, encodedPath);
            if (!fingerprint.AnsiSafe) {
                const std::wstring shortPath = utils::GetShortPathW(gameRoot);
                fingerprint.ShortPathAvailable = !shortPath.empty() && shortPath != gameRoot &&
                    utils::TryEncodePathForActiveCodePage(shortPath, encodedPath);
            }
            fingerprint.PlayerHash = ShortHash(utils::CombinePathW(gameRoot, L"Bin\\Player.exe"));
            fingerprint.Ck2Hash = ShortHash(utils::CombinePathW(gameRoot, L"Bin\\CK2.dll"));
            fingerprint.RendererHash = ShortHash(
                utils::CombinePathW(gameRoot, L"RenderEngines\\CK2_3D.dll"));
            return fingerprint;
        }

        InstallFingerprint GetInstallFingerprint(const std::wstring &gameRoot) {
            static std::mutex mutex;
            static std::wstring cachedRoot;
            static InstallFingerprint cached;

            std::lock_guard<std::mutex> lock(mutex);
            if (cachedRoot != gameRoot) {
                cached = BuildInstallFingerprint(gameRoot);
                cachedRoot = gameRoot;
            }
            return cached;
        }

        std::string LocaleName() {
            wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
            if (::GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) <= 0)
                return "unknown";
            return utils::Utf16ToUtf8(name);
        }

        std::string DriverText(const char *text) {
            if (!text || !*text)
                return "unknown";
            const std::string converted = utils::Utf16ToUtf8(utils::AnsiToUtf16(text));
            return converted.empty() ? "unknown" : converted;
        }

        std::string JoinFontSources(const UI::FontRuntimeSnapshot &snapshot) {
            if (snapshot.Sources.empty())
                return "none";

            std::string result;
            for (const UI::FontSourceStatus &source : snapshot.Sources) {
                if (!result.empty())
                    result += ", ";
                const std::size_t separator = source.RequestedFace.find_last_of("\\/");
                const std::string name = separator == std::string::npos ? source.RequestedFace :
                                         source.RequestedFace.substr(separator + 1);
                result += name.empty() ? "(unnamed)" : name;
                result += source.Loaded ? " [loaded]" : " [failed]";
            }
            return result;
        }

        struct RuntimeState {
            std::string Windows;
        };

        struct LocaleState {
            std::string Name;
            unsigned int AnsiCodePage = 0;
            std::uintptr_t KeyboardLayout = 0;
        };

        struct RendererState {
            int DriverIndex = -1;
            std::string DriverName = "unknown";
            std::string DriverDescription = "unknown";
            bool DriverAvailable = false;
            bool DisplayAvailable = false;
            int Width = 0;
            int Height = 0;
            bool Fullscreen = false;
            unsigned int Dpi = 0;
            unsigned int TextureMaxWidth = 0;
            unsigned int TextureMaxHeight = 0;
        };

        struct FontState {
            bool RuntimeAvailable = false;
            UI::FontRuntimeState Runtime = UI::FontRuntimeState::Unconfigured;
            std::uint64_t Generation = 0;
            bool SupportsUnicodeScalars = false;
            bool SupportsCommonEmoji = false;
            std::size_t DiagnosticCount = 0;
            std::string Sources;
            bool ImGuiAvailable = false;
            bool AtlasAvailable = false;
            int AtlasWidth = 0;
            int AtlasHeight = 0;
            ImTextureStatus AtlasStatus = ImTextureStatus_Destroyed;
            bool BackendAvailable = false;
            int BackendMaxWidth = 0;
            int BackendMaxHeight = 0;
            std::string LastBackendFailure;
            unsigned int LastBackendFailureCount = 0;
        };

        struct ModState {
            int Count = 0;
            bool Discovered = false;
            bool Active = false;
#if BML_ENABLE_ANGELSCRIPT
            bool AngelScriptExtension = false;
            bool AngelScriptBindings = false;
#endif
        };

        struct SystemReportData {
            RuntimeState Runtime;
            LocaleState Locale;
            InstallFingerprint Install;
            RendererState Renderer;
            FontState Fonts;
            Overlay::Ime::Runtime::Diagnostics Ime;
            HookSnapshot Hooks;
            ModState Mods;
        };

        SystemReportData CaptureSystemReport(ModContext &context) {
            SystemReportData data;
            data.Runtime.Windows = WindowsVersion();
            data.Locale.Name = LocaleName();
            data.Locale.AnsiCodePage = ::GetACP();
            data.Locale.KeyboardLayout = reinterpret_cast<std::uintptr_t>(::GetKeyboardLayout(0));
            data.Install = GetInstallFingerprint(context.GetDirectory(BML_DIR_GAME));

            CKRenderContext *renderContext = context.GetRenderContext();
            CKRenderManager *renderManager = context.GetRenderManager();
            const VxDriverDesc *driver = nullptr;
            if (renderContext && renderManager) {
                data.Renderer.DriverIndex = renderContext->GetDriverIndex();
                if (data.Renderer.DriverIndex >= 0 &&
                    data.Renderer.DriverIndex < renderManager->GetRenderDriverCount()) {
                    driver = renderManager->GetRenderDriverDescription(data.Renderer.DriverIndex);
                }
            }
            if (driver) {
                data.Renderer.DriverAvailable = true;
                data.Renderer.DriverName = DriverText(driver->DriverName);
                data.Renderer.DriverDescription = DriverText(driver->DriverDesc);
                data.Renderer.TextureMaxWidth = driver->Caps3D.MaxTextureWidth;
                data.Renderer.TextureMaxHeight = driver->Caps3D.MaxTextureHeight;
            }
            if (renderContext) {
                const HWND window = static_cast<HWND>(renderContext->GetWindowHandle());
                data.Renderer.DisplayAvailable = true;
                data.Renderer.Width = renderContext->GetWidth();
                data.Renderer.Height = renderContext->GetHeight();
                data.Renderer.Fullscreen = renderContext->IsFullScreen() != FALSE;
                data.Renderer.Dpi = window ? ::GetDpiForWindow(window) : 0;
            }

            if (const UI::FontRuntime *fonts = context.GetUiFontRuntime()) {
                const UI::FontRuntimeSnapshot &snapshot = fonts->Inspect();
                data.Fonts.RuntimeAvailable = true;
                data.Fonts.Runtime = snapshot.State;
                data.Fonts.Generation = snapshot.Generation;
                data.Fonts.SupportsUnicodeScalars = snapshot.SupportsUnicodeScalars;
                data.Fonts.SupportsCommonEmoji = snapshot.SupportsCommonEmoji;
                data.Fonts.DiagnosticCount = snapshot.Diagnostics.size();
                data.Fonts.Sources = JoinFontSources(snapshot);
            }

            {
                Overlay::ImGuiContextScope scope;
                data.Fonts.ImGuiAvailable = scope.IsActive();
                if (scope.IsActive()) {
                    ImFontAtlas *atlas = ImGui::GetIO().Fonts;
                    if (atlas && atlas->TexData) {
                        data.Fonts.AtlasAvailable = true;
                        data.Fonts.AtlasWidth = atlas->TexData->Width;
                        data.Fonts.AtlasHeight = atlas->TexData->Height;
                        data.Fonts.AtlasStatus = atlas->TexData->Status;
                    }

                    ImGui_ImplCK2_Diagnostics backend{};
                    if (ImGui_ImplCK2_GetDiagnostics(&backend)) {
                        data.Fonts.BackendAvailable = true;
                        data.Fonts.BackendMaxWidth = backend.TextureMaxWidth;
                        data.Fonts.BackendMaxHeight = backend.TextureMaxHeight;
                        data.Fonts.LastBackendFailure = backend.LastTextureFailure;
                        data.Fonts.LastBackendFailureCount = backend.LastTextureFailureCount;
                    }
                }
            }

            data.Ime = Overlay::Ime::Runtime::GetDiagnostics();
            data.Hooks = context.InspectHooks();
            data.Mods.Count = context.GetModCount();
            data.Mods.Discovered = context.AreModsLoaded();
            data.Mods.Active = context.AreModsInited();
#if BML_ENABLE_ANGELSCRIPT
            data.Mods.AngelScriptExtension = context.IsAngelScriptExtensionRegistered();
            data.Mods.AngelScriptBindings = context.AreAngelScriptBindingsRegistered();
#endif
            return data;
        }

        class SystemReportFormatter {
        public:
            explicit SystemReportFormatter(const SystemReportData &data) : m_Data(data) {}

            std::vector<std::string> Format() {
                AddRuntime();
                AddLocale();
                AddPaths();
                AddRenderer();
                AddFonts();
                AddIme();
                AddHooks();
                AddMods();
                return std::move(m_Lines);
            }

        private:
            void AddRuntime() {
                std::ostringstream line;
                line << "BML+ " BML_VERSION " (Win32, "
#ifdef _DEBUG
                     << "Debug";
#else
                     << "Release";
#endif
                line << "), Windows " << m_Data.Runtime.Windows;
                m_Lines.push_back(line.str());
            }

            void AddLocale() {
                std::ostringstream line;
                line << "Locale: " << m_Data.Locale.Name << ", ACP "
                     << m_Data.Locale.AnsiCodePage
                     << ", keyboard 0x" << std::hex << std::uppercase
                     << m_Data.Locale.KeyboardLayout;
                m_Lines.push_back(line.str());
            }

            void AddPaths() {
                std::ostringstream pathLine;
                pathLine << "Game path: ACP-safe=" << YesNo(m_Data.Install.AnsiSafe)
                         << ", short-path=" << (m_Data.Install.AnsiSafe ? "not-needed" :
                             m_Data.Install.ShortPathAvailable ? "available" : "unavailable");
                m_Lines.push_back(pathLine.str());

                std::ostringstream hashLine;
                hashLine << "Runtime hashes: Player=" << m_Data.Install.PlayerHash
                         << ", CK2=" << m_Data.Install.Ck2Hash
                         << ", CK2_3D=" << m_Data.Install.RendererHash;
                m_Lines.push_back(hashLine.str());
            }

            void AddRenderer() {
                std::ostringstream rendererLine;
                rendererLine << "Renderer: driver " << m_Data.Renderer.DriverIndex << ' '
                             << m_Data.Renderer.DriverName;
                if (m_Data.Renderer.DriverAvailable)
                    rendererLine << " (" << m_Data.Renderer.DriverDescription << ')';
                m_Lines.push_back(rendererLine.str());

                std::ostringstream displayLine;
                if (m_Data.Renderer.DisplayAvailable) {
                    displayLine << "Display: " << m_Data.Renderer.Width << 'x'
                                << m_Data.Renderer.Height << ", "
                                << (m_Data.Renderer.Fullscreen ? "fullscreen" : "windowed")
                                << ", DPI " << m_Data.Renderer.Dpi;
                } else {
                    displayLine << "Display: unavailable";
                }
                if (m_Data.Renderer.DriverAvailable) {
                    displayLine << ", texture cap " << m_Data.Renderer.TextureMaxWidth << 'x'
                                << m_Data.Renderer.TextureMaxHeight;
                }
                m_Lines.push_back(displayLine.str());
            }

            void AddFonts() {
                if (m_Data.Fonts.RuntimeAvailable) {
                    std::ostringstream line;
                    line << "Fonts: " << FontStateName(m_Data.Fonts.Runtime)
                         << ", generation " << m_Data.Fonts.Generation
                         << ", Unicode=" << YesNo(m_Data.Fonts.SupportsUnicodeScalars)
                         << ", emoji=" << YesNo(m_Data.Fonts.SupportsCommonEmoji)
                         << ", diagnostics=" << m_Data.Fonts.DiagnosticCount;
                    m_Lines.push_back(line.str());
                    m_Lines.push_back("Font sources: " + m_Data.Fonts.Sources);
                } else {
                    m_Lines.push_back("Fonts: unavailable");
                }

                if (!m_Data.Fonts.ImGuiAvailable) {
                    m_Lines.push_back("Font atlas: ImGui unavailable");
                    return;
                }

                std::ostringstream line;
                if (m_Data.Fonts.AtlasAvailable) {
                    line << "Font atlas: " << m_Data.Fonts.AtlasWidth << 'x'
                         << m_Data.Fonts.AtlasHeight << ", "
                         << TextureStatusName(m_Data.Fonts.AtlasStatus);
                } else {
                    line << "Font atlas: unavailable";
                }

                if (m_Data.Fonts.BackendAvailable) {
                    line << ", backend limit " << m_Data.Fonts.BackendMaxWidth << 'x'
                         << m_Data.Fonts.BackendMaxHeight << ", last failure "
                         << m_Data.Fonts.LastBackendFailure << " ("
                         << m_Data.Fonts.LastBackendFailureCount << ')';
                }
                m_Lines.push_back(line.str());
            }

            void AddIme() {
                const Overlay::Ime::Runtime::Diagnostics &ime = m_Data.Ime;
                std::ostringstream line;
                line << "IME: visible=" << YesNo(ime.PresentationVisible)
                     << ", composition=" << YesNo(ime.CompositionActive)
                     << ", IMM=" << YesNo(ime.ImmContextAvailable)
                     << ", IMM candidates=" << YesNo(ime.HasCandidates)
                     << ", TSF=" << YesNo(ime.TsfAttached)
                     << ", TSF candidates=" << YesNo(ime.TsfCandidates)
                     << ", focus=" << (ime.FocusWindow == 0 ? "none" :
                                          ime.FocusWindow == ime.PresentationWindow ? "input" : "other");
                m_Lines.push_back(line.str());
            }

            void AddHooks() {
                const HookSnapshot &hooks = m_Data.Hooks;
                std::ostringstream line;
                line << "Hooks: input=" << YesNo(hooks.Input)
                     << ", object-load=" << YesNo(hooks.ObjectLoad)
                     << ", physics-order=" << YesNo(hooks.PhysicsPostProcess)
                     << ", physicalize=" << YesNo(hooks.Physicalize)
                     << ", render-skip=" << YesNo(hooks.RenderSkip);
                m_Lines.push_back(line.str());
            }

            void AddMods() {
                std::ostringstream line;
                line << "Mods: count=" << m_Data.Mods.Count
                     << ", discovered=" << YesNo(m_Data.Mods.Discovered)
                     << ", active=" << YesNo(m_Data.Mods.Active);
#if BML_ENABLE_ANGELSCRIPT
                line << ", AngelScript extension="
                     << YesNo(m_Data.Mods.AngelScriptExtension)
                     << ", bindings=" << YesNo(m_Data.Mods.AngelScriptBindings);
#else
                line << ", AngelScript=disabled";
#endif
                m_Lines.push_back(line.str());
            }

            const SystemReportData &m_Data;
            std::vector<std::string> m_Lines;
        };
    }

    std::vector<std::string> BuildSystemReport(ModContext &context) {
        const SystemReportData data = CaptureSystemReport(context);
        return SystemReportFormatter(data).Format();
    }
}
