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
#include "bml_imgui.h"
#include "bml_interface.h"
#include "bml_ui_host.h"
#include "bml_config.hpp"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include "Overlay.h"

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
constexpr bool kDebugOverlayMarker = false;
constexpr bool kDebugSubmitOnPostRender = true;

std::string BuildFontPath(const std::string &loaderDirUtf8, const char *filename) {
    if (!filename || filename[0] == '\0') {
        return "";
    }

    std::string path = filename;
    if (!utils::FileExistsUtf8(path)) {
        path = loaderDirUtf8;
        path.append("\\Fonts\\").append(filename);
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
    BML_UILayer layer{BML_UI_LAYER_WINDOW};
    int32_t priority{0};
    uint32_t order{0};
    PFN_BML_UIDrawCallback callback{nullptr};
    void *user_data{nullptr};
};
extern const BML_UIDrawRegistry g_WindowRegistry;
extern const BML_UIDrawRegistry g_OverlayRegistry;
extern const BML_ImGuiApi g_ImGuiApi;

class UIMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    bool m_ContextCreated = false;
    bool m_PlatformReady = false;
    bool m_RendererReady = false;
    bool m_GuiResourcesReady = false;
    bool m_LoggedFirstUIDraw = false;
    bool m_LoggedFirstPostRenderSubmit = false;
    bool m_LoggedFirstSpriteSubmit = false;
    bool m_LoggedValidSpriteSubmit = false;
    bool m_LoggedFirstRenderData = false;
    bool m_LoggedValidRenderData = false;
    bool m_LoggedDisplaySizeFallback = false;
    bool m_LoggedPreProcessEntry = false;
    bool m_LoggedPreProcessMetrics = false;
    bool m_LoggedPostProcessMetrics = false;
    bool m_LoggedPostProcessBootstrap = false;
    bool m_LoggedFirstPostRenderPayload = false;
    bool m_LoggedFirstPostSpritePayload = false;
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
    std::string m_ProviderId;
    std::mutex m_DrawMutex;
    std::vector<UIDrawContribution> m_WindowContributions;
    std::vector<UIDrawContribution> m_OverlayContributions;
    uint32_t m_NextDrawOrder = 1;
    uint64_t m_FrameId = 0;
    DWORD m_UiThreadId = 0;
    bml::InterfaceLease<BML_HostRuntimeInterface> m_HostRuntime;
    bml::PublishedInterface m_WindowRegistryInterface;
    bml::PublishedInterface m_OverlayRegistryInterface;
    bml::PublishedInterface m_ImGuiInterface;

    void EnsureDefaultConfig() {
        auto config = Services().Config();

        if (!config.GetString("GUI", "FontFilename").has_value()) {
            config.SetString("GUI", "FontFilename", "unifont.otf");
        }
        if (!config.GetFloat("GUI", "FontSize").has_value()) {
            config.SetFloat("GUI", "FontSize", 32.0f);
        }
        if (!config.GetString("GUI", "FontRanges").has_value()) {
            config.SetString("GUI", "FontRanges", "ChineseFull");
        }
        if (!config.GetBool("GUI", "EnableSecondaryFont").has_value()) {
            config.SetBool("GUI", "EnableSecondaryFont", false);
        }
        if (!config.GetString("GUI", "SecondaryFontFilename").has_value()) {
            config.SetString("GUI", "SecondaryFontFilename", "unifont.otf");
        }
        if (!config.GetFloat("GUI", "SecondaryFontSize").has_value()) {
            config.SetFloat("GUI", "SecondaryFontSize", 32.0f);
        }
        if (!config.GetString("GUI", "SecondaryFontRanges").has_value()) {
            config.SetString("GUI", "SecondaryFontRanges", "ChineseFull");
        }
        if (!config.GetBool("GUI", "EnableIniSettings").has_value()) {
            config.SetBool("GUI", "EnableIniSettings", true);
        }
    }

    void RefreshGuiConfig() {
        auto config = Services().Config();
        m_FontFilename = config.GetString("GUI", "FontFilename").value_or("unifont.otf");
        m_FontSize = config.GetFloat("GUI", "FontSize").value_or(32.0f);
        m_FontRanges = config.GetString("GUI", "FontRanges").value_or("ChineseFull");
        m_EnableSecondaryFont = config.GetBool("GUI", "EnableSecondaryFont").value_or(false);
        m_SecondaryFontFilename = config.GetString("GUI", "SecondaryFontFilename").value_or("unifont.otf");
        m_SecondaryFontSize = config.GetFloat("GUI", "SecondaryFontSize").value_or(32.0f);
        m_SecondaryFontRanges = config.GetString("GUI", "SecondaryFontRanges").value_or("ChineseFull");
        m_EnableIniSettings = config.GetBool("GUI", "EnableIniSettings").value_or(true);
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
        if (!m_EnableIniSettings || m_ImGuiIniFilename.empty() || !Overlay::GetImGuiContext()) {
            return;
        }

        Overlay::ImGuiContextScope scope;
        ImGui::SaveIniSettingsToDisk(m_ImGuiIniFilename.c_str());
    }

    void EnsureGuiResourcesReady() {
        if (m_GuiResourcesReady || !Overlay::GetImGuiContext()) {
            return;
        }

        InitializeLoaderPaths();
        RefreshGuiConfig();

        Overlay::ImGuiContextScope scope;
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        m_ImGuiIniFilename = m_LoaderDirUtf8 + "\\ImGui.ini";
        m_ImGuiLogFilename = m_LoaderDirUtf8 + "\\ImGui.log";
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
            "GUI resources initialized: font='%s' size=%.1f scale=%.3f ini=%d",
            m_FontFilename.c_str(),
            m_FontSize,
            ImGui::GetStyle().FontScaleMain,
            m_EnableIniSettings ? 1 : 0);
    }

    void OnResize() {
        if (m_WindowRect.GetHeight() > 0) {
            ImGui::GetStyle().FontScaleMain = static_cast<float>(m_WindowRect.GetHeight()) / 1200.0f;
        }
    }

    void SyncFrameMetrics() {
        if (!m_RenderContext || !Overlay::GetImGuiContext()) {
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
        if (!m_LoggedDisplaySizeFallback) {
            m_LoggedDisplaySizeFallback = true;
            Services().Log().Info( "Applied frame metrics from render root: %.0fx%.0f", width, height);
        }
    }

    bool RefreshRenderContextFromContext() {
        if (m_RenderContext || !m_CKContext) {
            return m_RenderContext != nullptr;
        }

        m_RenderContext = m_CKContext->GetPlayerRenderContext();
        return m_RenderContext != nullptr;
    }

    bool EnsureContextCreated() {
        if (m_ContextCreated) {
            return true;
        }

        if (!Overlay::ImGuiCreateContext()) {
            Services().Log().Error( "Failed to create ImGui context");
            return false;
        }

        m_ContextCreated = true;
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

        if (!Overlay::ImGuiInitPlatform(m_CKContext)) {
            Services().Log().Error( "Failed to initialize ImGui platform backend");
            DestroyAllUiState();
            return false;
        }

        m_PlatformReady = true;
        Services().Log().Info( "ImGui platform initialized successfully");
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

        if (!Overlay::ImGuiInitRenderer(m_CKContext)) {
            Services().Log().Error( "Failed to initialize ImGui renderer backend");
            return false;
        }

        EnsureGuiResourcesReady();

        m_RendererReady = true;
        Services().Log().Info( "ImGui renderer initialized successfully");
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
        if (!m_CKContext || !m_RendererReady || !Overlay::GetImGuiContext()) {
            m_RendererReady = false;
            return;
        }

        Overlay::ImGuiShutdownRenderer(m_CKContext);
        m_RendererReady = false;
    }

    void DestroyAllUiState() {
        DestroyRenderer();

        if (m_CKContext && m_PlatformReady && Overlay::GetImGuiContext()) {
            Overlay::ImGuiShutdownPlatform(m_CKContext);
        }
        m_PlatformReady = false;

        m_GuiResourcesReady = false;
        if (m_ContextCreated && Overlay::GetImGuiContext()) {
            Overlay::ImGuiDestroyContext();
        }
        m_ContextCreated = false;
    }

    void HandleEngineInit(const bml::imc::Message &msg) {
        auto *payload = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
        if (!payload) {
            Services().Log().Warn( "Engine/Init message has null CKContext");
            return;
        }

        m_CKContext = payload->context;
        EnsurePlatformReady(m_CKContext);
    }

    void HandleEnginePlay(const bml::imc::Message &msg) {
        auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
        if (!payload) {
            Services().Log().Warn( "Engine/Play payload missing CKContext");
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
            Overlay::ImGuiContextScope scope;
            SyncFrameMetrics();
            Overlay::ImGuiNewFrame();
            SyncFrameMetrics();
        }
    }

    void HandleEngineReset() {
        if (m_RendererReady) {
            Services().Log().Info( "Shutting down ImGui renderer");
            Overlay::ImGuiContextScope scope;
            Overlay::ImGuiEndFrame();
        }
        DestroyRenderer();
        m_RenderContext = nullptr;
    }

    void HandleEngineShutdown() {
        if (m_ContextCreated || m_PlatformReady || m_RendererReady) {
            Services().Log().Info( "Shutting down ImGui");
        }
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
        if (!m_RendererReady || !Overlay::GetImGuiContext() || !Overlay::ImGuiHasPendingFrame()) {
            return BML_RESULT_INVALID_STATE;
        }
        return BML_RESULT_OK;
    }

    BML_Result RegisterDrawContribution(bool overlay,
                                        const BML_UIDrawDesc *desc,
                                        BML_InterfaceRegistration *out_registration) {
        if (!desc || desc->struct_size < sizeof(BML_UIDrawDesc) || !desc->id ||
            !desc->callback || !out_registration) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!m_HostRuntime) {
            return BML_RESULT_NOT_SUPPORTED;
        }

        const char *interfaceId = overlay ? BML_UI_OVERLAY_REGISTRY_INTERFACE_ID
                                          : BML_UI_WINDOW_REGISTRY_INTERFACE_ID;
        BML_Result registration_result = m_HostRuntime->RegisterContribution(m_Handle, interfaceId, out_registration);
        if (registration_result != BML_RESULT_OK) {
            return registration_result;
        }

        UIDrawContribution contribution;
        contribution.registration = *out_registration;
        contribution.id = desc->id;
        contribution.layer = desc->layer;
        contribution.priority = desc->priority;
        contribution.order = m_NextDrawOrder++;
        contribution.callback = desc->callback;
        contribution.user_data = desc->user_data;

        try {
            std::lock_guard<std::mutex> lock(m_DrawMutex);
            auto &container = overlay ? m_OverlayContributions : m_WindowContributions;
            container.push_back(std::move(contribution));
        } catch (...) {
            (void) m_HostRuntime->UnregisterContribution(*out_registration);
            *out_registration = nullptr;
            return BML_RESULT_OUT_OF_MEMORY;
        }
        return BML_RESULT_OK;
    }

    BML_Result UnregisterDrawContribution(bool overlay, BML_InterfaceRegistration registration) {
        if (!registration) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!m_HostRuntime) {
            return BML_RESULT_NOT_SUPPORTED;
        }

        std::lock_guard<std::mutex> lock(m_DrawMutex);
        auto &container = overlay ? m_OverlayContributions : m_WindowContributions;
        auto it = std::find_if(container.begin(), container.end(),
                               [registration](const UIDrawContribution &entry) {
                                   return entry.registration == registration;
                               });
        if (it == container.end()) {
            return BML_RESULT_NOT_FOUND;
        }

        BML_Result unregister_result = m_HostRuntime->UnregisterContribution(registration);
        if (unregister_result != BML_RESULT_OK) {
            return unregister_result;
        }

        container.erase(it);
        return BML_RESULT_OK;
    }

    void ExecuteDrawContributions(bool overlay) {
        std::vector<UIDrawContribution> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_DrawMutex);
            snapshot = overlay ? m_OverlayContributions : m_WindowContributions;
        }

        std::stable_sort(snapshot.begin(), snapshot.end(),
                         [](const UIDrawContribution &lhs, const UIDrawContribution &rhs) {
                             if (lhs.priority != rhs.priority) {
                                 return lhs.priority < rhs.priority;
                             }
                             return lhs.order < rhs.order;
                         });

        for (const auto &entry : snapshot) {
            if (!entry.callback) {
                continue;
            }

            try {
                ImGuiIO &io = ImGui::GetIO();
                ImGuiViewport *viewport = ImGui::GetMainViewport();
                BML_UIDrawContext ctx = BML_UI_DRAW_CONTEXT_INIT;
                ctx.imgui = &g_ImGuiApi;
                ctx.layer = entry.layer;
                if (viewport) {
                    ctx.viewport_pos.x = viewport->Pos.x;
                    ctx.viewport_pos.y = viewport->Pos.y;
                    ctx.viewport_size.x = viewport->Size.x;
                    ctx.viewport_size.y = viewport->Size.y;
                }
                ctx.input.delta_time = io.DeltaTime;
                ctx.input.mouse_wheel = io.MouseWheel;
                ctx.input.key_ctrl = io.KeyCtrl ? BML_TRUE : BML_FALSE;
                ctx.input.key_shift = io.KeyShift ? BML_TRUE : BML_FALSE;
                ctx.input.key_alt = io.KeyAlt ? BML_TRUE : BML_FALSE;
                ctx.input.key_super = io.KeySuper ? BML_TRUE : BML_FALSE;
                ctx.frame_id = m_FrameId;
                entry.callback(&ctx, entry.user_data);
            } catch (...) {
                Services().Log().Error( "Draw contribution '%s' raised an exception", entry.id.c_str());
            }
        }
    }

    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();
        m_HostRuntime = Acquire<BML_HostRuntimeInterface>(BML_CORE_HOST_RUNTIME_INTERFACE_ID, 1);
        if (!m_HostRuntime) {
            return BML_RESULT_NOT_FOUND;
        }

        Services().Log().Info("Initializing BML UI Module v0.4.0");
        m_UiThreadId = GetCurrentThreadId();
        const char *mod_id = nullptr;
        if (Services().Builtins().Module->GetModId(m_Handle, &mod_id) == BML_RESULT_OK && mod_id) {
            m_ProviderId = mod_id;
        }
        EnsureDefaultConfig();
        InitializeLoaderPaths();

        if (MH_Initialize() != MH_OK) {
            Services().Log().Error( "Failed to initialize MinHook");
            return BML_RESULT_INTERNAL_ERROR;
        }

        if (!Overlay::ImGuiInstallWin32Hooks()) {
            Services().Log().Error( "Failed to install Win32 hooks for ImGui");
            MH_Uninitialize();
            return BML_RESULT_INTERNAL_ERROR;
        }

        m_WindowRegistryInterface = Publish(
            BML_UI_WINDOW_REGISTRY_INTERFACE_ID,
            &g_WindowRegistry,
            1,
            0,
            0,
            BML_INTERFACE_FLAG_IMMUTABLE | BML_INTERFACE_FLAG_HOST_OWNED | BML_INTERFACE_FLAG_MAIN_THREAD_ONLY);
        if (!m_WindowRegistryInterface) {
            Overlay::ImGuiUninstallWin32Hooks();
            MH_Uninitialize();
            return BML_RESULT_FAIL;
        }

        m_OverlayRegistryInterface = Publish(
            BML_UI_OVERLAY_REGISTRY_INTERFACE_ID,
            &g_OverlayRegistry,
            1,
            0,
            0,
            BML_INTERFACE_FLAG_IMMUTABLE | BML_INTERFACE_FLAG_HOST_OWNED | BML_INTERFACE_FLAG_MAIN_THREAD_ONLY);
        if (!m_OverlayRegistryInterface) {
            (void) m_WindowRegistryInterface.Reset();
            Overlay::ImGuiUninstallWin32Hooks();
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
            (void) m_OverlayRegistryInterface.Reset();
            (void) m_WindowRegistryInterface.Reset();
            Overlay::ImGuiUninstallWin32Hooks();
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
                Overlay::ImGuiContextScope scope;
                if (!m_LoggedPreProcessEntry) {
                    m_LoggedPreProcessEntry = true;
                    Services().Log().Info(
                        "Pre-process entry: ctx=%p render_ctx=%p platform=%d renderer=%d",
                        m_CKContext,
                        m_RenderContext,
                        m_PlatformReady ? 1 : 0,
                        m_RendererReady ? 1 : 0);
                }
                Overlay::ImGuiNewFrame();
                SyncFrameMetrics();
                if (!m_LoggedPreProcessMetrics) {
                    m_LoggedPreProcessMetrics = true;
                    ImGuiIO &io = ImGui::GetIO();
                    ImGuiViewport *viewport = ImGui::GetMainViewport();
                    Services().Log().Info(
                        "Pre-process metrics: io_display=(%.1f,%.1f) viewport=(%.1f,%.1f)",
                        io.DisplaySize.x,
                        io.DisplaySize.y,
                        viewport ? viewport->Size.x : -1.0f,
                        viewport ? viewport->Size.y : -1.0f);
                }
            }
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
            if (m_RendererReady) {
                Overlay::ImGuiContextScope scope;
                SyncFrameMetrics();
                if (!m_LoggedPostProcessMetrics) {
                    m_LoggedPostProcessMetrics = true;
                    ImGuiIO &io = ImGui::GetIO();
                    ImGuiViewport *viewport = ImGui::GetMainViewport();
                    Services().Log().Info(
                        "Post-process metrics: io_display=(%.1f,%.1f) viewport=(%.1f,%.1f)",
                        io.DisplaySize.x,
                        io.DisplaySize.y,
                        viewport ? viewport->Size.x : -1.0f,
                        viewport ? viewport->Size.y : -1.0f);
                }
                ExecuteDrawContributions(false);
                ExecuteDrawContributions(true);
                ++m_FrameId;
                if (!m_LoggedFirstUIDraw) {
                    m_LoggedFirstUIDraw = true;
                    Services().Log().Info( "First typed UI registry frame executed");
                }
                if (kDebugOverlayMarker) {
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

        m_Subs.Add(BML_TOPIC_ENGINE_POST_RENDER, [this](const bml::imc::Message &msg) {
            if (!m_RendererReady || !kDebugSubmitOnPostRender) {
                return;
            }

            CKRenderContext *payloadDev = nullptr;
            (void)msg.CopyTo(payloadDev);
            if (!m_LoggedFirstPostRenderPayload) {
                m_LoggedFirstPostRenderPayload = true;
                Services().Log().Info(
                    "Post-render payload: event_dev=%p cached_render_ctx=%p",
                    payloadDev,
                    m_RenderContext);
            }

            Overlay::ImGuiContextScope scope;
            SyncFrameMetrics();
            if (!Overlay::ImGuiHasPendingFrame()) {
                return;
            }

            Overlay::ImGuiRender();
            if (!m_LoggedFirstRenderData) {
                m_LoggedFirstRenderData = true;
                ImDrawData *drawData = ImGui::GetDrawData();
                ImGuiIO &io = ImGui::GetIO();
                Services().Log().Info(
                    "First render data: ctx=%p draw_ctx=%p cmd_lists=%d total_vtx=%d total_idx=%d fonts=%d tex_id=%p draw_display=(%.1f,%.1f) io_display=(%.1f,%.1f) viewport=(%.1f,%.1f)",
                    ImGui::GetCurrentContext(),
                    Overlay::GetImGuiContext(),
                    drawData ? drawData->CmdListsCount : -1,
                    drawData ? drawData->TotalVtxCount : -1,
                    drawData ? drawData->TotalIdxCount : -1,
                    io.Fonts ? io.Fonts->Fonts.Size : -1,
                    io.Fonts ? io.Fonts->TexID : nullptr,
                    drawData ? drawData->DisplaySize.x : -1.0f,
                    drawData ? drawData->DisplaySize.y : -1.0f,
                    io.DisplaySize.x,
                    io.DisplaySize.y,
                    ImGui::GetMainViewport() ? ImGui::GetMainViewport()->Size.x : -1.0f,
                    ImGui::GetMainViewport() ? ImGui::GetMainViewport()->Size.y : -1.0f);
            }
            if (!m_LoggedValidRenderData) {
                ImDrawData *drawData = ImGui::GetDrawData();
                if (drawData && drawData->CmdListsCount > 0 &&
                    drawData->DisplaySize.x > 0.0f && drawData->DisplaySize.y > 0.0f) {
                    m_LoggedValidRenderData = true;
                    Services().Log().Info(
                        "First valid render data: cmd_lists=%d total_vtx=%d draw_display=(%.1f,%.1f) io_display=(%.1f,%.1f) viewport=(%.1f,%.1f)",
                        drawData->CmdListsCount,
                        drawData->TotalVtxCount,
                        drawData->DisplaySize.x,
                        drawData->DisplaySize.y,
                        ImGui::GetIO().DisplaySize.x,
                        ImGui::GetIO().DisplaySize.y,
                        ImGui::GetMainViewport() ? ImGui::GetMainViewport()->Size.x : -1.0f,
                        ImGui::GetMainViewport() ? ImGui::GetMainViewport()->Size.y : -1.0f);
                }
            }
            if (!m_LoggedFirstPostRenderSubmit) {
                m_LoggedFirstPostRenderSubmit = true;
                Services().Log().Info( "Finalized ImGui frame on post-render");
            }
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_SPRITE_RENDER, [this](const bml::imc::Message &msg) {
            if (!m_RendererReady) {
                TryBootstrapFromRuntime();
            }
            if (m_RendererReady) {
                CKRenderContext *payloadDev = nullptr;
                (void)msg.CopyTo(payloadDev);
                if (!m_LoggedFirstPostSpritePayload) {
                    m_LoggedFirstPostSpritePayload = true;
                    Services().Log().Info(
                        "Post-sprite payload: event_dev=%p cached_render_ctx=%p",
                        payloadDev,
                        m_RenderContext);
                }
                const bool hadRenderableDrawData = Overlay::ImGuiHasRenderableDrawData();
                Overlay::ImGuiOnRender();
                if (!m_LoggedFirstSpriteSubmit) {
                    m_LoggedFirstSpriteSubmit = true;
                    Services().Log().Info( "First post-sprite render submit");
                }
                if (hadRenderableDrawData && !m_LoggedValidSpriteSubmit) {
                    m_LoggedValidSpriteSubmit = true;
                    Services().Log().Info( "Submitted valid draw data on post-sprite render");
                }
            }
        });

        if (m_Subs.Count() < 2) {
            Services().Log().Error("Failed to subscribe to engine lifecycle events");
            (void) m_ImGuiInterface.Reset();
            (void) m_OverlayRegistryInterface.Reset();
            (void) m_WindowRegistryInterface.Reset();
            Overlay::ImGuiUninstallWin32Hooks();
            MH_Uninitialize();
            return BML_RESULT_FAIL;
        }

        TryBootstrapFromRuntime();

        Services().Log().Info("BML UI Module initialized - waiting for engine events");
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        Services().Log().Info("Shutting down BML UI Module");

        if ((m_ContextCreated || m_PlatformReady || m_RendererReady) && Overlay::GetImGuiContext()) {
            SaveIniSettings();
            DestroyAllUiState();
        }

        Overlay::ImGuiUninstallWin32Hooks();
        MH_Uninitialize();
        (void) m_ImGuiInterface.Reset();
        (void) m_OverlayRegistryInterface.Reset();
        (void) m_WindowRegistryInterface.Reset();

        {
            std::lock_guard<std::mutex> lock(m_DrawMutex);
            m_WindowContributions.clear();
            m_OverlayContributions.clear();
            m_NextDrawOrder = 1;
        }

        m_CKContext = nullptr;
        m_RenderContext = nullptr;
        m_ContextCreated = false;
        m_PlatformReady = false;
        m_RendererReady = false;
        m_GuiResourcesReady = false;
        m_LoggedFirstUIDraw = false;
        m_LoggedFirstSpriteSubmit = false;
        m_LoggedValidSpriteSubmit = false;
        m_LoggedFirstRenderData = false;
        m_LoggedValidRenderData = false;
        m_LoggedDisplaySizeFallback = false;
        m_LoggedPreProcessEntry = false;
        m_LoggedPreProcessMetrics = false;
        m_LoggedPostProcessMetrics = false;
        m_LoggedPostProcessBootstrap = false;
        m_LoggedFirstPostRenderPayload = false;
        m_LoggedFirstPostSpritePayload = false;
        m_WindowRect = VxRect();
        m_OldWindowRect = VxRect();
        m_LoggedFirstPostRenderSubmit = false;
        m_FrameId = 0;
        m_HostRuntime.Reset();
    }
};

// =============================================================================
// Public API Implementation Functions
// =============================================================================

static UIMod *GetUI() {
    return bml::detail::ModuleEntryHelper<UIMod>::GetInstance();
}

static BML_Result API_RegisterWindowContribution(const BML_UIDrawDesc *desc,
                                                 BML_InterfaceRegistration *out_registration) {
    auto *ui = GetUI();
    return ui ? ui->RegisterDrawContribution(false, desc, out_registration) : BML_RESULT_NOT_FOUND;
}

static BML_Result API_UnregisterWindowContribution(BML_InterfaceRegistration registration) {
    auto *ui = GetUI();
    return ui ? ui->UnregisterDrawContribution(false, registration) : BML_RESULT_NOT_FOUND;
}

static BML_Result API_RegisterOverlayContribution(const BML_UIDrawDesc *desc,
                                                  BML_InterfaceRegistration *out_registration) {
    auto *ui = GetUI();
    return ui ? ui->RegisterDrawContribution(true, desc, out_registration) : BML_RESULT_NOT_FOUND;
}

static BML_Result API_UnregisterOverlayContribution(BML_InterfaceRegistration registration) {
    auto *ui = GetUI();
    return ui ? ui->UnregisterDrawContribution(true, registration) : BML_RESULT_NOT_FOUND;
}

const BML_UIDrawRegistry g_WindowRegistry = {
    1,
    0,
    API_RegisterWindowContribution,
    API_UnregisterWindowContribution
};

const BML_UIDrawRegistry g_OverlayRegistry = {
    1,
    0,
    API_RegisterOverlayContribution,
    API_UnregisterOverlayContribution
};

static BML_Result API_ValidateMainThreadAccess() {
    auto *ui = GetUI();
    return ui ? ui->ValidateMainThreadAccess() : BML_RESULT_NOT_FOUND;
}

static BML_Result API_ValidateCurrentFrameAccess() {
    auto *ui = GetUI();
    return ui ? ui->ValidateCurrentFrameAccess() : BML_RESULT_NOT_FOUND;
}

const BML_ImGuiApi g_ImGuiApi = {
    sizeof(BML_ImGuiApi),
    1,
    0,
    ImGui::GetVersion(),
    "cimgui",
    API_ValidateMainThreadAccess,
    API_ValidateCurrentFrameAccess,
#include "bml_imgui_api_init.inc"
};

BML_DEFINE_MODULE(UIMod)
