#include "Loader/ModContext.h"
#include "UI/Overlay.h"

#include "UI/Ime/Presentation.h"
#include "UI/OverlayPlatformInput.h"

#include "CKContext.h"

#include "HookUtils.h"

#include "imgui_internal.h"
#include "UI/imgui_impl_ck2.h"
#include <SDL3/SDL.h>
#include "backends/imgui_impl_sdl3.h"

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
        ImGuiShutdownPlatform(nullptr);

        g_ImGuiContext = nullptr;
        g_DrawDataReady = false;
        g_NewFrame = false;

        ImGui::DestroyContext(context);
    }

    bool ImGuiInitPlatform(CKContext *context) {
        ImGuiContextScope scope;

        auto *window = context ? static_cast<SDL_Window *>(context->GetMainWindow()) : nullptr;
        if (!window || !ImGui_ImplSDL3_InitForOther(window)) return false;
        if (!PlatformInput::Attach(window)) {
            ImGui_ImplSDL3_Shutdown();
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

    void ImGuiShutdownPlatform(CKContext *context) {
        (void) context;
        if (!g_PlatformInitialized) {
            if (!PlatformInput::Detach())
                utils::OutputDebugA("BML: Unable to detach Overlay platform input.\n");
            return;
        }

        ImGuiContextScope scope;

        if (!PlatformInput::Detach())
            utils::OutputDebugA("BML: Unable to detach Overlay platform input.\n");
        ImGui_ImplSDL3_Shutdown();
        g_PlatformInitialized = false;
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
            ImGui_ImplSDL3_NewFrame();
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
        auto invocation = context->LockModInvocation();
        if (g_DrawDataReady) {
            ImGuiContextScope scope;
            ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());
        }
    }
}
