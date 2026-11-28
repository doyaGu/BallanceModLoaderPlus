#include "Loader/ModContext.h"
#include "UI/Overlay.h"

#include "UI/Ime/Presentation.h"
#include "UI/OverlayPlatformInput.h"

#include "CKContext.h"

#include "imgui_internal.h"
#include "UI/imgui_impl_ck2.h"
#if BML_RUNTIME_SDL
#include <SDL3/SDL.h>
#include "backends/imgui_impl_sdl3.h"
#else
#include "backends/imgui_impl_win32.h"
#endif

namespace Overlay {
    ImGuiContext *g_ImGuiContext = nullptr;
    bool g_PlatformInitialized = false;
    bool g_RendererInitialized = false;
    bool g_DrawDataReady = false;
    bool g_NewFrame = false;

    ImGuiContext *GetImGuiContext() {
        return g_ImGuiContext;
    }

    bool IsImGuiReady() {
        return g_RendererInitialized;
    }

    bool IsImGuiFrameActive() {
        return g_NewFrame;
    }

    ImGuiContext *ImGuiCreateContext() {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();

        ImGuiContext *previousContext = ImGui::GetCurrentContext();
        g_ImGuiContext = ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::SetCurrentContext(previousContext);

        return g_ImGuiContext;
    }

    void ImGuiDestroyContext() {
        ImGuiContext *context = g_ImGuiContext;
        if (!context)
            return;

        {
            ImGuiContextScope scope;
            ImGuiEndFrame();
        }
        ImGuiShutdownRenderer(nullptr);
        if (!ImGuiShutdownPlatform(nullptr))
            return;

        g_ImGuiContext = nullptr;
        g_DrawDataReady = false;
        g_NewFrame = false;

        ImGui::DestroyContext(context);
    }

    bool ImGuiInitPlatform(CKContext *context) {
        ImGuiContextScope scope;

#if BML_RUNTIME_SDL
        auto *window = context ? static_cast<SDL_Window *>(context->GetMainWindow()) : nullptr;
        if (!window || !ImGui_ImplSDL3_InitForOther(window)) return false;
#else
        void *window = context ? context->GetMainWindow() : nullptr;
        if (!window || !ImGui_ImplWin32_Init(window)) return false;
#endif
        if (!PlatformInput::Attach(window)) {
#if BML_RUNTIME_SDL
            ImGui_ImplSDL3_Shutdown();
#else
            ImGui_ImplWin32_Shutdown();
#endif
            return false;
        }

        g_PlatformInitialized = true;
        return true;
    }

    bool ImGuiInitRenderer(CKContext *context) {
        ImGuiContextScope scope;

        if (!ImGui_ImplCK2_Init(context))
            return false;

        g_DrawDataReady = false;
        g_RendererInitialized = true;

        return true;
    }

    bool ImGuiShutdownPlatform(CKContext *context) {
        (void) context;
        if (!PlatformInput::Detach())
            return false;
        if (!g_PlatformInitialized)
            return true;

        ImGuiContextScope scope;
#if BML_RUNTIME_SDL
        ImGui_ImplSDL3_Shutdown();
#else
        ImGui_ImplWin32_Shutdown();
#endif
        g_PlatformInitialized = false;
        return true;
    }

    void ImGuiShutdownRenderer(CKContext *context) {
        (void) context;
        if (!g_RendererInitialized)
            return;

        ImGuiContextScope scope;

        ImGui_ImplCK2_Shutdown();

        g_DrawDataReady = false;
        g_RendererInitialized = false;
    }

    void ImGuiNewFrame() {
        if (g_RendererInitialized && !g_NewFrame) {
            g_DrawDataReady = false;

            // ImGui's counters are the source of truth. If a Virtools manager
            // callback escaped before Render(), finish that old frame before
            // starting the next one instead of hitting imgui.cpp's hard
            // "Forgot to call Render() or EndFrame()" assertion.
            ImGuiContext *context = ImGui::GetCurrentContext();
            if (context && context->FrameCount != 0 &&
                context->FrameCountEnded != context->FrameCount) {
                ImGui::EndFrame();
            }

            // Old draw data has now been discarded and no business callback is
            // active. Fault cleanup may release code/resources used by that data.
            if (ModContextLease context; context) context->GetModLoader().ProcessFailedNativeMods();

            PlatformInput::ProcessEvents();
#if BML_RUNTIME_SDL
            ImGui_ImplSDL3_NewFrame();
#else
            ImGui_ImplWin32_NewFrame();
#endif
            ImGui_ImplCK2_NewFrame();
            ImGui::NewFrame();

            g_NewFrame = true;
        }
    }

    void ImGuiEndFrame() {
        if (g_NewFrame) {
            ImGui::EndFrame();
            g_NewFrame = false;
        }
    }

    void ImGuiRender() {
        if (g_NewFrame) {
            if (ImGuiContext *context = ImGui::GetCurrentContext())
                Ime::Presentation::Draw(context->PlatformImeData);
            ImGui::Render();
            // Render() completes the ImGui frame. Publish that state before a
            // test callback can trigger a re-entrant Virtools reset/shutdown;
            // otherwise OnCKReset sees a phantom open frame and the next
            // NewFrame() asserts that the previous frame was never ended.
            g_NewFrame = false;
            g_DrawDataReady = true;
        }
    }

    void ImGuiDiscardDrawData() {
        ImGuiContextScope scope;
        if (!scope.IsActive()) return;
        ImGuiEndFrame();
        g_DrawDataReady = false;
        if (ImDrawData *data = ImGui::GetDrawData()) data->Valid = false;
    }

    void ImGuiOnRender() {
        ModContextLease context;
        if (!context) return;
        // Custom draw callbacks are module code too. A reentrant Stop may not
        // unload them while the backend is consuming their command list.
        auto invocation = context->GetModLoader().LockModInvocation();
        if (g_DrawDataReady) {
            ImGuiContextScope scope;
            ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());
        }
    }
}
