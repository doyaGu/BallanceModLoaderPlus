#include "UI/OverlayPlatformInput.h"
#include "UI/Ime/Runtime.h"
#include "UI/Overlay.h"
#include "backends/imgui_impl_sdl3.h"
#include <SDL3/SDL.h>
#include <mutex>
#include <string>
#include <vector>

namespace Overlay::PlatformInput {
namespace {
struct PendingEvent {
    SDL_Event Event{};
    std::string Text;
};
std::mutex g_Mutex;
std::vector<PendingEvent> g_Events;
SDL_Window *g_Window = nullptr;
bool g_CaptureFailed = false;

bool SDLCALL Watch(void *, SDL_Event *event) {
    // SDL may invoke watches from other threads. Never touch ImGui here.
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION: case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT: case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_WINDOW_MOUSE_ENTER: case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_FOCUS_GAINED: case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_GAMEPAD_ADDED: case SDL_EVENT_GAMEPAD_REMOVED:
        break;
    default: return true;
    }
    std::lock_guard lock(g_Mutex);
    try {
        if (g_Events.size() >= 4096) {
            g_Events.clear();
            g_CaptureFailed = true;
        }
        PendingEvent copy{*event, {}};
        if (event->type == SDL_EVENT_TEXT_INPUT && event->text.text)
            copy.Text = event->text.text;
        if (event->type == SDL_EVENT_TEXT_EDITING && event->edit.text)
            copy.Text = event->edit.text;
        g_Events.push_back(std::move(copy));
    } catch (...) {
        g_Events.clear();
        g_CaptureFailed = true;
    }
    // A watch observes only; Player owns draining the SDL event queue.
    return true;
}
}

bool Attach(void *window) {
    if (g_Window) return g_Window == window;
    if (!window || !SDL_AddEventWatch(Watch, nullptr)) return false;
    g_Window = static_cast<SDL_Window *>(window);
    Ime::Runtime::Attach(window, window);
    return true;
}

bool Detach() {
    if (!g_Window) return true;
    SDL_RemoveEventWatch(Watch, nullptr);
    Ime::Runtime::Detach();
    g_Window = nullptr;
    std::lock_guard lock(g_Mutex);
    g_Events.clear();
    g_CaptureFailed = false;
    return true;
}

void ProcessEvents() {
    std::vector<PendingEvent> pending;
    bool failed;
    {
        std::lock_guard lock(g_Mutex);
        pending.swap(g_Events);
        failed = g_CaptureFailed;
        g_CaptureFailed = false;
    }
    if (failed) {
        ImGui::GetIO().ClearInputKeys();
        ImGui::GetIO().ClearInputMouse();
    }
    for (auto &copy : pending) {
        if (copy.Event.type == SDL_EVENT_TEXT_INPUT) copy.Event.text.text = copy.Text.c_str();
        if (copy.Event.type == SDL_EVENT_TEXT_EDITING) copy.Event.edit.text = copy.Text.c_str();
        Ime::Runtime::ObserveEvent(copy.Event);
        ImGui_ImplSDL3_ProcessEvent(&copy.Event);
    }
}
}
