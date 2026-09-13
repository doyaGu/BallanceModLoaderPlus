#include "UI/Overlay.h"

#include "UI/Ime/Presentation.h"
#include "UI/OverlayPlatformInput.h"

#include "CKContext.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "imgui_internal.h"
#include "UI/imgui_impl_ck2.h"
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include "backends/imgui_impl_win32.h"

namespace Overlay {
    ImGuiContext *g_ImGuiContext = nullptr;
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

        // Platform shutdown normally owns detachment. Keep destruction defensive
        // so no thread hook can retain a path to a freed ImGui context.
        if (!PlatformInput::Detach())
            ::OutputDebugStringA("BML: Unable to detach Overlay platform input.\n");
        g_ImGuiContext = nullptr;
        g_RendererInitialized = false;
        g_DrawDataReady = false;
        g_NewFrame = false;

        ImGui::DestroyContext(context);
    }

    bool ImGuiInitPlatform(CKContext *context) {
        ImGuiContextScope scope;

        HWND window = context ? static_cast<HWND>(context->GetMainWindow()) : nullptr;
        if (!window || !ImGui_ImplWin32_Init(window))
            return false;
        if (!PlatformInput::Attach(window)) {
            ImGui_ImplWin32_Shutdown();
            return false;
        }

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
        ImGuiContextScope scope;

        if (!PlatformInput::Detach())
            ::OutputDebugStringA("BML: Unable to detach Overlay platform input.\n");
        ImGui_ImplWin32_Shutdown();
    }

    void ImGuiShutdownRenderer(CKContext *context) {
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

            ImGui_ImplWin32_NewFrame();
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

    void ImGuiOnRender() {
        if (g_DrawDataReady) {
            ImGuiContextScope scope;
            ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());
        }
    }
}
