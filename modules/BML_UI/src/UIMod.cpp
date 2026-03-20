/**
 * @file UIMod.cpp
 * @brief BML_UI Module Entry Point and API Implementation
 *
 * This module provides the core UI infrastructure for BML:
 * - ImGui overlay lifecycle management (internal)
 * - Typed UI draw registries (public interface)
 * - Win32 input hooks for ImGui (internal)
 *
 * ImGui is exposed through the typed interface runtime.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_config_bind.hpp"
#include "bml_imgui_api.h"
#include "bml_interface.h"
#include "bml_ui.h"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_ck2.h"
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include "backends/imgui_impl_win32.h"

#include "Win32Hooks.h"

#include "CKContext.h"
#include "CK2dEntity.h"
#include "CKRenderContext.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include <MinHook.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <vector>

namespace {
constexpr bool kShowOverlayDebugMarker = false; // visual debug marker
constexpr bool kSubmitOnPostRender = true;       // production render path toggle

std::string BuildFontPath(const std::string &loaderDirUtf8, const char *filename) {
    if (!filename || filename[0] == '\0') {
        return "";
    }

    std::string path = filename;
    if (!utils::FileExistsUtf8(path)) {
        path = (std::filesystem::path(loaderDirUtf8) / "Fonts" / filename).string();
    }

    return path;
}

ImFont *LoadConfiguredFont(const std::string &loaderDirUtf8, const char *filename, float size, const char *ranges, bool merge = false) {
    ImGuiIO &io = ImGui::GetIO();
    if (size <= 0.0f) {
        size = 32.0f;
    }

    auto addDefaultFont = [&]() -> ImFont * {
        ImFontConfig config;
        config.SizePixels = size;
        config.MergeMode = false;
        return io.Fonts->AddFontDefault(&config);
    };

    std::string path = BuildFontPath(loaderDirUtf8, filename);
    if (path.empty() || !utils::FileExistsUtf8(path)) {
        return addDefaultFont();
    }

    const ImWchar *glyphRanges = nullptr;
    if (_strnicmp(ranges, "ChineseFull", 11) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    } else if (_strnicmp(ranges, "Chinese", 7) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    } else if (_strnicmp(ranges, "Cyrillic", 8) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesCyrillic();
    } else if (_strnicmp(ranges, "Greek", 5) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesGreek();
    } else if (_strnicmp(ranges, "Korean", 6) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesKorean();
    } else if (_strnicmp(ranges, "Japanese", 8) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesJapanese();
    } else if (_strnicmp(ranges, "Thai", 4) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesThai();
    } else if (_strnicmp(ranges, "Vietnamese", 10) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesVietnamese();
    }

    ImFontConfig config;
    config.SizePixels = size;
    config.MergeMode = merge;
    if (config.MergeMode && io.Fonts->Fonts.empty()) {
        config.MergeMode = false;
    }

    ImFont *font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &config, glyphRanges);
    return font ? font : addDefaultFont();
}
}

// =============================================================================
// UIMod Class
// =============================================================================

struct UIDrawContribution {
    BML_InterfaceRegistration registration{nullptr};
    std::string id;
    int32_t priority{0};
    uint32_t order{0};
    PFN_BML_UIDrawCallback callback{nullptr};
    void *user_data{nullptr};
};
extern const BML_UIDrawRegistry g_DrawRegistry;
extern const BML_ImGuiApi g_ImGuiApi;

class UIMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;

    // ImGui lifecycle state (replaces Overlay globals)
    ImGuiContext *m_ImGuiCtx = nullptr;     // null = not created
    bool m_PlatformReady = false;
    bool m_RendererReady = false;
    bool m_GuiResourcesReady = false;
    bool m_FrameActive = false;
    bool m_FrameEnded = false;
    bool m_RenderDataReady = false;

    VxRect m_WindowRect;
    VxRect m_OldWindowRect;
    std::wstring m_LoaderDirW;
    std::string m_LoaderDirUtf8;
    std::string m_ImGuiIniFilename;
    std::string m_ImGuiLogFilename;
    std::string m_FontFilename = "unifont.otf";
    float m_FontSize = 32.0f;
    std::string m_FontRanges = "ChineseFull";
    bool m_EnableSecondaryFont = false;
    std::string m_SecondaryFontFilename = "unifont.otf";
    float m_SecondaryFontSize = 32.0f;
    std::string m_SecondaryFontRanges = "ChineseFull";
    bool m_EnableIniSettings = true;
    bml::ConfigBindings m_Cfg;
    std::string m_ProviderId;
    std::mutex m_DrawMutex;
    std::vector<UIDrawContribution> m_Contributions;
    uint32_t m_NextDrawOrder = 1;
    bool m_DrawOrderDirty = false;
    DWORD m_UiThreadId = 0;
    bml::InterfaceLease<BML_HostRuntimeInterface> m_HostRuntime;
    bml::PublishedInterface m_DrawRegistryInterface;
    bml::PublishedInterface m_ImGuiInterface;

    // =========================================================================
    // ImGui context scope helpers
    // =========================================================================

    struct ContextScope {
        ImGuiContext *prev;
        explicit ContextScope(ImGuiContext *ctx) : prev(ImGui::GetCurrentContext()) {
            ImGui::SetCurrentContext(ctx);
        }
        ~ContextScope() { ImGui::SetCurrentContext(prev); }
        ContextScope(const ContextScope &) = delete;
        ContextScope &operator=(const ContextScope &) = delete;
    };

    // =========================================================================
    // ImGui lifecycle (absorbed from Overlay)
    // =========================================================================

    bool CreateImGuiContext() {
        IMGUI_CHECKVERSION();

        ImGuiContext *previousContext = ImGui::GetCurrentContext();
        m_ImGuiCtx = ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::SetCurrentContext(previousContext);

        bml_ui::SetHookTarget(nullptr, m_ImGuiCtx);
        return m_ImGuiCtx != nullptr;
    }

    void DestroyImGuiContext() {
        if (!m_ImGuiCtx) {
            return;
        }
        ContextScope scope(m_ImGuiCtx);
        ImGui::DestroyContext();
        m_ImGuiCtx = nullptr;
        bml_ui::SetHookTarget(nullptr, nullptr);
    }

    bool InitPlatform() {
        ContextScope scope(m_ImGuiCtx);

        HWND hwnd = static_cast<HWND>(m_CKContext->GetMainWindow());
        if (!ImGui_ImplWin32_Init(hwnd)) {
            return false;
        }

        bml_ui::SetHookTarget(hwnd, m_ImGuiCtx);
        m_PlatformReady = true;
        Services().Log().Info("ImGui platform ready");
        return true;
    }

    bool InitRenderer() {
        ContextScope scope(m_ImGuiCtx);

        if (!ImGui_ImplCK2_Init(m_CKContext)) {
            return false;
        }

        m_RenderDataReady = false;
        m_RendererReady = true;
        Services().Log().Info("ImGui renderer ready");
        return true;
    }

    void ShutdownPlatform() {
        if (!m_PlatformReady) {
            return;
        }
        ContextScope scope(m_ImGuiCtx);
        ImGui_ImplWin32_Shutdown();
        bml_ui::SetHookTarget(nullptr, m_ImGuiCtx);
        m_PlatformReady = false;
    }

    void ShutdownRenderer() {
        if (!m_RendererReady) {
            return;
        }
        ContextScope scope(m_ImGuiCtx);
        ImGui_ImplCK2_Shutdown();
        m_RendererReady = false;
        m_RenderDataReady = false;
    }

    void BeginFrame() {
        if (!m_RendererReady || m_FrameActive) {
            return;
        }
        m_RenderDataReady = false;
        m_FrameEnded = false;

        ImGui_ImplWin32_NewFrame();
        ImGui_ImplCK2_NewFrame();
        ImGui::NewFrame();

        m_FrameActive = true;
    }

    void EndFrame() {
        if (!m_FrameActive || m_FrameEnded) {
            return;
        }
        ImGui::EndFrame();
        m_FrameEnded = true;
    }

    void RenderFrame() {
        if (!m_FrameActive) {
            return;
        }
        ContextScope scope(m_ImGuiCtx);
        if (!m_FrameEnded) {
            ImGui::EndFrame();
        }
        ImGui::Render();
        m_FrameActive = false;
        m_FrameEnded = false;
        m_RenderDataReady = true;
    }

    void SubmitRenderData() {
        if (!m_RenderDataReady) {
            return;
        }
        ContextScope scope(m_ImGuiCtx);
        ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());
    }

    // =========================================================================
    // Config and resource management
    // =========================================================================

    void InitConfigBindings() {
        m_Cfg.Clear();
        m_Cfg.Bind("GUI", "FontFilename", m_FontFilename, "unifont.otf");
        m_Cfg.Bind("GUI", "FontSize", m_FontSize, 32.0f);
        m_Cfg.Bind("GUI", "FontRanges", m_FontRanges, "ChineseFull");
        m_Cfg.Bind("GUI", "EnableSecondaryFont", m_EnableSecondaryFont, false);
        m_Cfg.Bind("GUI", "SecondaryFontFilename", m_SecondaryFontFilename, "unifont.otf");
        m_Cfg.Bind("GUI", "SecondaryFontSize", m_SecondaryFontSize, 32.0f);
        m_Cfg.Bind("GUI", "SecondaryFontRanges", m_SecondaryFontRanges, "ChineseFull");
        m_Cfg.Bind("GUI", "EnableIniSettings", m_EnableIniSettings, true);
    }

    void RefreshGuiConfig() {
        m_Cfg.Refresh(Services().Config());
    }

    void InitializeLoaderPaths() {
        if (!m_LoaderDirW.empty()) {
            return;
        }

        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        std::filesystem::path loaderPath = std::filesystem::path(modulePath).parent_path().parent_path() / "ModLoader";
        m_LoaderDirW = loaderPath.wstring();
        m_LoaderDirUtf8 = utils::Utf16ToUtf8(m_LoaderDirW);
    }

    void SaveIniSettings() {
        if (!m_EnableIniSettings || m_ImGuiIniFilename.empty() || !m_ImGuiCtx) {
            return;
        }

        ContextScope scope(m_ImGuiCtx);
        ImGui::SaveIniSettingsToDisk(m_ImGuiIniFilename.c_str());
    }

    void EnsureGuiResourcesReady() {
        if (m_GuiResourcesReady || !m_ImGuiCtx) {
            return;
        }

        InitializeLoaderPaths();
        RefreshGuiConfig();

        ContextScope scope(m_ImGuiCtx);
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        m_ImGuiIniFilename = (std::filesystem::path(m_LoaderDirUtf8) / "ImGui.ini").string();
        m_ImGuiLogFilename = (std::filesystem::path(m_LoaderDirUtf8) / "ImGui.log").string();
        io.LogFilename = m_ImGuiLogFilename.c_str();

        if (m_EnableIniSettings && utils::FileExistsUtf8(m_ImGuiIniFilename)) {
            ImGui::LoadIniSettingsFromDisk(m_ImGuiIniFilename.c_str());
        }

        if (m_RenderContext) {
            VxRect windowRect;
            if (auto *root = m_RenderContext->Get2dRoot(TRUE)) {
                root->GetRect(windowRect);
                if (windowRect.GetHeight() > 0) {
                    ImGui::GetStyle().FontScaleMain = static_cast<float>(windowRect.GetHeight()) / 1200.0f;
                }
            }
        }

        io.Fonts->Clear();
        LoadConfiguredFont(m_LoaderDirUtf8, m_FontFilename.c_str(), m_FontSize, m_FontRanges.c_str());
        if (m_EnableSecondaryFont) {
            const bool sameFont = m_FontFilename == m_SecondaryFontFilename && m_FontRanges == m_SecondaryFontRanges;
            if (!sameFont) {
                LoadConfiguredFont(
                    m_LoaderDirUtf8,
                    m_SecondaryFontFilename.c_str(),
                    m_SecondaryFontSize,
                    m_SecondaryFontRanges.c_str(),
                    true);
            }
        }

        io.Fonts->Build();
        m_GuiResourcesReady = true;
        Services().Log().Info(
            "GUI resources initialized: font='%s' size=%.1f scale=%.3f",
            m_FontFilename.c_str(),
            m_FontSize,
            ImGui::GetStyle().FontScaleMain);
    }

    // =========================================================================
    // Frame metrics
    // =========================================================================

    void OnResize() {
        if (m_WindowRect.GetHeight() > 0) {
            ImGui::GetStyle().FontScaleMain = static_cast<float>(m_WindowRect.GetHeight()) / 1200.0f;
        }
    }

    void SyncFrameMetrics() {
        if (!m_RenderContext || !m_ImGuiCtx) {
            return;
        }

        auto *root = m_RenderContext->Get2dRoot(TRUE);
        if (!root) {
            return;
        }

        m_OldWindowRect = m_WindowRect;
        root->GetRect(m_WindowRect);
        if (m_WindowRect != m_OldWindowRect) {
            OnResize();
        }

        const float width = static_cast<float>(m_WindowRect.GetWidth());
        const float height = static_cast<float>(m_WindowRect.GetHeight());
        if (width <= 0.0f || height <= 0.0f) {
            return;
        }

        ImGuiIO &io = ImGui::GetIO();
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        io.DisplaySize = ImVec2(width, height);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        viewport->Pos = ImVec2(0.0f, 0.0f);
        viewport->Size = ImVec2(width, height);
    }

    // =========================================================================
    // Bootstrap and teardown
    // =========================================================================

    bool RefreshRenderContextFromContext() {
        if (m_RenderContext || !m_CKContext) {
            return m_RenderContext != nullptr;
        }

        m_RenderContext = m_CKContext->GetPlayerRenderContext();
        return m_RenderContext != nullptr;
    }

    bool EnsureContextCreated() {
        if (m_ImGuiCtx) {
            return true;
        }

        if (!CreateImGuiContext()) {
            Services().Log().Error("Failed to create ImGui context");
            return false;
        }

        return true;
    }

    bool EnsurePlatformReady(CKContext *context) {
        if (!context) {
            return false;
        }

        m_CKContext = context;
        if (m_PlatformReady) {
            return true;
        }

        if (!EnsureContextCreated()) {
            return false;
        }

        if (!InitPlatform()) {
            Services().Log().Error("Failed to initialize ImGui platform backend");
            DestroyAllUiState();
            return false;
        }

        return true;
    }

    bool EnsureRendererReady() {
        if (m_RendererReady) {
            return true;
        }

        RefreshRenderContextFromContext();
        if (!m_CKContext || !m_RenderContext) {
            return false;
        }

        if (!EnsurePlatformReady(m_CKContext)) {
            return false;
        }

        if (!InitRenderer()) {
            Services().Log().Error("Failed to initialize ImGui renderer backend");
            return false;
        }

        EnsureGuiResourcesReady();
        return true;
    }

    void TryBootstrapFromRuntime() {
        if (!m_CKContext) {
            m_CKContext = bml::virtools::GetCKContext(Services());
        }

        if (!m_RenderContext) {
            m_RenderContext = bml::virtools::GetRenderContext(Services());
        }

        RefreshRenderContextFromContext();

        if (m_CKContext) {
            EnsurePlatformReady(m_CKContext);
        }

        if (m_CKContext && m_RenderContext) {
            EnsureRendererReady();
        }
    }

    void DestroyRenderer() {
        if (!m_CKContext || !m_RendererReady || !m_ImGuiCtx) {
            m_RendererReady = false;
            m_RenderDataReady = false;
            return;
        }

        ShutdownRenderer();
    }

    void DestroyAllUiState() {
        DestroyRenderer();

        if (m_CKContext && m_PlatformReady && m_ImGuiCtx) {
            ShutdownPlatform();
        }
        m_PlatformReady = false;

        m_GuiResourcesReady = false;
        m_FrameActive = false;
        m_FrameEnded = false;
        m_RenderDataReady = false;
        DestroyImGuiContext();
    }

    // =========================================================================
    // Engine event handlers
    // =========================================================================

    void HandleEngineInit(const bml::imc::Message &msg) {
        auto *payload = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
        if (!payload) {
            Services().Log().Warn("Engine/Init message has null CKContext");
            return;
        }

        m_CKContext = payload->context;
        EnsurePlatformReady(m_CKContext);
    }

    void HandleEnginePlay(const bml::imc::Message &msg) {
        auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
        if (!payload) {
            Services().Log().Warn("Engine/Play payload missing CKContext");
            return;
        }

        m_CKContext = payload->context;
        if (payload->render_context) {
            m_RenderContext = payload->render_context;
        } else {
            RefreshRenderContextFromContext();
        }

        if (!EnsurePlatformReady(m_CKContext)) {
            return;
        }
        if (EnsureRendererReady()) {
            ContextScope scope(m_ImGuiCtx);
            SyncFrameMetrics();
            BeginFrame();
            SyncFrameMetrics();
        }
    }

    void HandleEngineReset() {
        if (m_RendererReady) {
            ContextScope scope(m_ImGuiCtx);
            EndFrame();
        }
        DestroyRenderer();
        m_RenderContext = nullptr;
    }

    void HandleEngineShutdown() {
        DestroyAllUiState();
        m_CKContext = nullptr;
        m_RenderContext = nullptr;
    }

public:
    bool IsInitialized() const { return m_RendererReady; }
    const char *GetProviderId() const { return m_ProviderId.c_str(); }

    BML_Result ValidateMainThreadAccess() const {
        return GetCurrentThreadId() == m_UiThreadId ? BML_RESULT_OK : BML_RESULT_WRONG_THREAD;
    }

    BML_Result ValidateCurrentFrameAccess() const {
        if (ValidateMainThreadAccess() != BML_RESULT_OK) {
            return BML_RESULT_WRONG_THREAD;
        }
        if (!m_RendererReady || !m_ImGuiCtx || !m_FrameActive) {
            return BML_RESULT_INVALID_STATE;
        }
        return BML_RESULT_OK;
    }

    BML_Result RegisterDrawContribution(const BML_UIDrawDesc *desc,
                                        BML_InterfaceRegistration *out_registration) {
        if (!desc || desc->struct_size < sizeof(BML_UIDrawDesc) || !desc->id ||
            !desc->callback || !out_registration) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!m_HostRuntime) {
            return BML_RESULT_NOT_SUPPORTED;
        }

        BML_Result registration_result = m_HostRuntime->RegisterContribution(
            m_Handle, BML_UI_DRAW_REGISTRY_INTERFACE_ID, out_registration);
        if (registration_result != BML_RESULT_OK) {
            return registration_result;
        }

        UIDrawContribution contribution;
        contribution.registration = *out_registration;
        contribution.id = desc->id;
        contribution.priority = desc->priority;
        contribution.order = m_NextDrawOrder++;
        contribution.callback = desc->callback;
        contribution.user_data = desc->user_data;

        try {
            std::lock_guard<std::mutex> lock(m_DrawMutex);
            m_Contributions.push_back(std::move(contribution));
            m_DrawOrderDirty = true;
        } catch (...) {
            (void) m_HostRuntime->UnregisterContribution(*out_registration);
            *out_registration = nullptr;
            return BML_RESULT_OUT_OF_MEMORY;
        }
        return BML_RESULT_OK;
    }

    BML_Result UnregisterDrawContribution(BML_InterfaceRegistration registration) {
        if (!registration) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!m_HostRuntime) {
            return BML_RESULT_NOT_SUPPORTED;
        }

        std::lock_guard<std::mutex> lock(m_DrawMutex);
        auto it = std::find_if(m_Contributions.begin(), m_Contributions.end(),
                               [registration](const UIDrawContribution &entry) {
                                   return entry.registration == registration;
                               });
        if (it == m_Contributions.end()) {
            return BML_RESULT_NOT_FOUND;
        }

        BML_Result unregister_result = m_HostRuntime->UnregisterContribution(registration);
        if (unregister_result != BML_RESULT_OK) {
            return unregister_result;
        }

        m_Contributions.erase(it);
        m_DrawOrderDirty = true;
        return BML_RESULT_OK;
    }

    void ExecuteDrawContributions() {
        std::vector<UIDrawContribution> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_DrawMutex);
            if (m_DrawOrderDirty) {
                std::stable_sort(m_Contributions.begin(), m_Contributions.end(),
                    [](const UIDrawContribution &lhs, const UIDrawContribution &rhs) {
                        if (lhs.priority != rhs.priority) {
                            return lhs.priority < rhs.priority;
                        }
                        return lhs.order < rhs.order;
                    });
                m_DrawOrderDirty = false;
            }
            snapshot = m_Contributions;
        }

        BML_UIDrawContext ctx = BML_UI_DRAW_CONTEXT_INIT;
        ctx.imgui = &g_ImGuiApi;

        for (const auto &entry : snapshot) {
            if (entry.callback) {
                entry.callback(&ctx, entry.user_data);
            }
        }
    }

    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();
        m_HostRuntime = Services().Acquire<BML_HostRuntimeInterface>();
        if (!m_HostRuntime) {
            return BML_RESULT_NOT_FOUND;
        }

        Services().Log().Info("BML UI Module initialized");
        m_UiThreadId = GetCurrentThreadId();
        const char *mod_id = nullptr;
        if (Services().Builtins().Module->GetModId(m_Handle, &mod_id) == BML_RESULT_OK && mod_id) {
            m_ProviderId = mod_id;
        }
        InitConfigBindings();
        m_Cfg.Sync(Services().Config());
        InitializeLoaderPaths();

        if (MH_Initialize() != MH_OK) {
            Services().Log().Error("Failed to initialize MinHook");
            return BML_RESULT_INTERNAL_ERROR;
        }

        if (!bml_ui::InstallWin32Hooks()) {
            Services().Log().Error("Failed to install Win32 hooks for ImGui");
            MH_Uninitialize();
            return BML_RESULT_INTERNAL_ERROR;
        }

        m_DrawRegistryInterface = Publish(
            BML_UI_DRAW_REGISTRY_INTERFACE_ID,
            &g_DrawRegistry,
            1,
            0,
            0,
            BML_INTERFACE_FLAG_IMMUTABLE | BML_INTERFACE_FLAG_HOST_OWNED | BML_INTERFACE_FLAG_MAIN_THREAD_ONLY);
        if (!m_DrawRegistryInterface) {
            bml_ui::UninstallWin32Hooks();
            MH_Uninitialize();
            return BML_RESULT_FAIL;
        }

        m_ImGuiInterface = Publish(
            BML_UI_IMGUI_INTERFACE_ID,
            &g_ImGuiApi,
            1,
            0,
            0,
            BML_INTERFACE_FLAG_IMMUTABLE | BML_INTERFACE_FLAG_MAIN_THREAD_ONLY);
        if (!m_ImGuiInterface) {
            (void) m_DrawRegistryInterface.Reset();
            bml_ui::UninstallWin32Hooks();
            MH_Uninitialize();
            return BML_RESULT_FAIL;
        }

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &msg) {
            HandleEngineInit(msg);
        });

        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            HandleEnginePlay(msg);
        });

        m_Subs.Add(BML_TOPIC_ENGINE_RESET, [this](const bml::imc::Message &) {
            HandleEngineReset();
        });

        m_Subs.Add(BML_TOPIC_ENGINE_END, [this](const bml::imc::Message &) {
            HandleEngineShutdown();
        });

        m_Subs.Add(BML_TOPIC_ENGINE_PRE_PROCESS, [this](const bml::imc::Message &) {
            if (m_RendererReady) {
                ContextScope scope(m_ImGuiCtx);
                BeginFrame();
                SyncFrameMetrics();
            }
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
            if (m_RendererReady) {
                ContextScope scope(m_ImGuiCtx);
                SyncFrameMetrics();
                ExecuteDrawContributions();
                if (kShowOverlayDebugMarker) {
                    ImDrawList *drawList = ImGui::GetForegroundDrawList();
                    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                    drawList->AddRectFilled(
                        ImVec2(center.x - 160.0f, center.y - 70.0f),
                        ImVec2(center.x + 160.0f, center.y + 70.0f),
                        IM_COL32(255, 32, 32, 160),
                        12.0f);
                    drawList->AddText(
                        ImVec2(center.x - 110.0f, center.y - 12.0f),
                        IM_COL32_WHITE,
                        "BML_UI DEBUG");
                }
            }
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_RENDER, [this](const bml::imc::Message &) {
            if (!m_RendererReady || !kSubmitOnPostRender || !m_FrameActive) {
                return;
            }

            ContextScope scope(m_ImGuiCtx);
            SyncFrameMetrics();
            RenderFrame();
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_SPRITE_RENDER, [this](const bml::imc::Message &) {
            if (!m_RendererReady) {
                TryBootstrapFromRuntime();
            }
            if (m_RendererReady && m_RenderDataReady) {
                ContextScope scope(m_ImGuiCtx);
                SubmitRenderData();
            }
        });

        if (m_Subs.Count() < 2) {
            Services().Log().Error("Failed to subscribe to engine lifecycle events");
            (void) m_ImGuiInterface.Reset();
            (void) m_DrawRegistryInterface.Reset();
            bml_ui::UninstallWin32Hooks();
            MH_Uninitialize();
            return BML_RESULT_FAIL;
        }

        TryBootstrapFromRuntime();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        if (m_ImGuiCtx) {
            SaveIniSettings();
            DestroyAllUiState();
        }

        bml_ui::UninstallWin32Hooks();
        MH_Uninitialize();
        (void) m_ImGuiInterface.Reset();
        (void) m_DrawRegistryInterface.Reset();

        {
            std::lock_guard<std::mutex> lock(m_DrawMutex);
            m_Contributions.clear();
            m_NextDrawOrder = 1;
            m_DrawOrderDirty = false;
        }

        m_CKContext = nullptr;
        m_RenderContext = nullptr;
        m_ImGuiCtx = nullptr;
        m_PlatformReady = false;
        m_RendererReady = false;
        m_GuiResourcesReady = false;
        m_FrameActive = false;
        m_FrameEnded = false;
        m_RenderDataReady = false;
        m_WindowRect = VxRect();
        m_OldWindowRect = VxRect();
        m_Cfg.Clear();
        m_HostRuntime.Reset();

        Services().Log().Info("BML UI Module shut down");
    }
};

// =============================================================================
// Public API Implementation Functions
// =============================================================================

static UIMod *GetUI() {
    return bml::detail::ModuleEntryHelper<UIMod>::GetInstance();
}

static BML_Result API_RegisterContribution(const BML_UIDrawDesc *desc,
                                           BML_InterfaceRegistration *out_registration) {
    auto *ui = GetUI();
    return ui ? ui->RegisterDrawContribution(desc, out_registration) : BML_RESULT_NOT_FOUND;
}

static BML_Result API_UnregisterContribution(BML_InterfaceRegistration registration) {
    auto *ui = GetUI();
    return ui ? ui->UnregisterDrawContribution(registration) : BML_RESULT_NOT_FOUND;
}

const BML_UIDrawRegistry g_DrawRegistry = {
    BML_IFACE_HEADER(BML_UIDrawRegistry, BML_UI_DRAW_REGISTRY_INTERFACE_ID, 1, 0),
    API_RegisterContribution,
    API_UnregisterContribution
};

static BML_Result API_ValidateMainThreadAccess() {
    auto *ui = GetUI();
    return ui ? ui->ValidateMainThreadAccess() : BML_RESULT_NOT_FOUND;
}

static BML_Result API_ValidateCurrentFrameAccess() {
    auto *ui = GetUI();
    return ui ? ui->ValidateCurrentFrameAccess() : BML_RESULT_NOT_FOUND;
}

#include "bml_imgui_subtables.inc"

BML_DEFINE_MODULE(UIMod)
