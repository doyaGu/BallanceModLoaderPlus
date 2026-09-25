#include "Diagnostics/SystemReport.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winternl.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "BML/BML.h"
#include "CKRenderContext.h"
#include "CKRenderManager.h"
#include "CryptoUtils.h"
#include "Hooks/HookLifecycle.h"
#include "Hooks/RenderHook.h"
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

        class SystemReportBuilder {
        public:
            explicit SystemReportBuilder(ModContext &context) : m_Context(context) {}

            std::vector<std::string> Build() {
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
                line << "), Windows " << WindowsVersion();
                m_Lines.push_back(line.str());
            }

            void AddLocale() {
                std::ostringstream line;
                line << "Locale: " << LocaleName() << ", ACP " << ::GetACP()
                     << ", keyboard 0x" << std::hex << std::uppercase
                     << reinterpret_cast<std::uintptr_t>(::GetKeyboardLayout(0));
                m_Lines.push_back(line.str());
            }

            void AddPaths() {
                const std::wstring gameRoot = m_Context.GetDirectory(BML_DIR_GAME);
                std::string encodedPath;
                const bool ansiSafe = utils::TryEncodePathForActiveCodePage(
                    gameRoot, encodedPath);
                const std::wstring shortPath = ansiSafe ? std::wstring() :
                                                   utils::GetShortPathW(gameRoot);
                const bool shortPathAvailable = !shortPath.empty() && shortPath != gameRoot &&
                    utils::TryEncodePathForActiveCodePage(shortPath, encodedPath);

                std::ostringstream pathLine;
                pathLine << "Game path: ACP-safe=" << YesNo(ansiSafe)
                         << ", short-path=" << (ansiSafe ? "not-needed" :
                                                   shortPathAvailable ? "available" : "unavailable");
                m_Lines.push_back(pathLine.str());

                std::ostringstream hashLine;
                hashLine << "Runtime hashes: Player="
                         << ShortHash(utils::CombinePathW(gameRoot, L"Bin\\Player.exe"))
                         << ", CK2=" << ShortHash(utils::CombinePathW(gameRoot, L"Bin\\CK2.dll"))
                         << ", CK2_3D="
                         << ShortHash(utils::CombinePathW(gameRoot, L"RenderEngines\\CK2_3D.dll"));
                m_Lines.push_back(hashLine.str());
            }

            void AddRenderer() {
                CKRenderContext *renderContext = m_Context.GetRenderContext();
                CKRenderManager *renderManager = m_Context.GetRenderManager();
                const VxDriverDesc *driver = nullptr;
                int driverIndex = -1;
                if (renderContext && renderManager) {
                    driverIndex = renderContext->GetDriverIndex();
                    if (driverIndex >= 0 && driverIndex < renderManager->GetRenderDriverCount())
                        driver = renderManager->GetRenderDriverDescription(driverIndex);
                }

                std::ostringstream rendererLine;
                rendererLine << "Renderer: driver " << driverIndex << ' '
                             << DriverText(driver ? driver->DriverName : nullptr);
                if (driver)
                    rendererLine << " (" << DriverText(driver->DriverDesc) << ')';
                m_Lines.push_back(rendererLine.str());

                std::ostringstream displayLine;
                if (renderContext) {
                    const HWND window = static_cast<HWND>(renderContext->GetWindowHandle());
                    displayLine << "Display: " << renderContext->GetWidth() << 'x'
                                << renderContext->GetHeight() << ", "
                                << (renderContext->IsFullScreen() ? "fullscreen" : "windowed")
                                << ", DPI " << (window ? ::GetDpiForWindow(window) : 0);
                } else {
                    displayLine << "Display: unavailable";
                }
                if (driver) {
                    displayLine << ", texture cap " << driver->Caps3D.MaxTextureWidth << 'x'
                                << driver->Caps3D.MaxTextureHeight;
                }
                m_Lines.push_back(displayLine.str());
            }

            void AddFonts() {
                if (const UI::FontRuntime *fonts = m_Context.GetUiFontRuntime()) {
                    const UI::FontRuntimeSnapshot &snapshot = fonts->Inspect();
                    std::ostringstream line;
                    line << "Fonts: " << FontStateName(snapshot.State)
                         << ", generation " << snapshot.Generation
                         << ", Unicode=" << YesNo(snapshot.SupportsUnicodeScalars)
                         << ", emoji=" << YesNo(snapshot.SupportsCommonEmoji)
                         << ", diagnostics=" << snapshot.Diagnostics.size();
                    m_Lines.push_back(line.str());
                    m_Lines.push_back("Font sources: " + JoinFontSources(snapshot));
                } else {
                    m_Lines.push_back("Fonts: unavailable");
                }

                Overlay::ImGuiContextScope scope;
                if (!scope.IsActive()) {
                    m_Lines.push_back("Font atlas: ImGui unavailable");
                    return;
                }

                std::ostringstream line;
                ImFontAtlas *atlas = ImGui::GetIO().Fonts;
                if (atlas && atlas->TexData) {
                    line << "Font atlas: " << atlas->TexData->Width << 'x'
                         << atlas->TexData->Height << ", "
                         << TextureStatusName(atlas->TexData->Status);
                } else {
                    line << "Font atlas: unavailable";
                }

                ImGui_ImplCK2_Diagnostics renderer{};
                if (ImGui_ImplCK2_GetDiagnostics(&renderer)) {
                    line << ", backend limit " << renderer.TextureMaxWidth << 'x'
                         << renderer.TextureMaxHeight << ", last failure "
                         << renderer.LastTextureFailure << " ("
                         << renderer.LastTextureFailureCount << ')';
                }
                m_Lines.push_back(line.str());
            }

            void AddIme() {
                const Overlay::Ime::Runtime::Diagnostics ime =
                    Overlay::Ime::Runtime::GetDiagnostics();
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
                std::ostringstream line;
                line << "Hooks: input=" << YesNo(IsInputHookInstalled())
                     << ", object-load=" << YesNo(IsObjectLoadHookInstalled())
                     << ", physics-order=" << YesNo(IsPhysicsPostProcessHookInstalled())
                     << ", physicalize=" << YesNo(IsPhysicalizeHookInstalled())
                     << ", render-skip=" << YesNo(RenderHook::IsSkipRenderAvailable());
                m_Lines.push_back(line.str());
            }

            void AddMods() {
                std::ostringstream line;
                line << "Mods: count=" << m_Context.GetModCount()
                     << ", discovered=" << YesNo(m_Context.AreModsLoaded())
                     << ", active=" << YesNo(m_Context.AreModsInited());
#if BML_ENABLE_ANGELSCRIPT
                line << ", AngelScript extension="
                     << YesNo(m_Context.IsAngelScriptExtensionRegistered())
                     << ", bindings=" << YesNo(m_Context.AreAngelScriptBindingsRegistered());
#else
                line << ", AngelScript=disabled";
#endif
                m_Lines.push_back(line.str());
            }

            ModContext &m_Context;
            std::vector<std::string> m_Lines;
        };
    }

    std::vector<std::string> BuildSystemReport(ModContext &context) {
        return SystemReportBuilder(context).Build();
    }
}
