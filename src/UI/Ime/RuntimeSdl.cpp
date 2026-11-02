#include "UI/Ime/Runtime.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <utf8.h>

namespace Overlay::Ime::Runtime {
namespace {
SDL_Window *g_Window = nullptr;
State g_State;
bool g_Visible = false;

std::u16string Decode(const char *text, int cursor, std::uint32_t &utf16Cursor) {
    std::u16string result;
    utf16Cursor = 0;
    if (!text) return result;
    const auto *p = reinterpret_cast<const utf8_int8_t *>(text);
    if (utf8valid(p)) return result;
    int index = 0;
    while (*p) {
        if (index++ == cursor) utf16Cursor = static_cast<std::uint32_t>(result.size());
        utf8_int32_t point = 0;
        p = utf8codepoint(p, &point);
        if (point > 0xffff) {
            point -= 0x10000;
            result += static_cast<char16_t>(0xd800 + (point >> 10));
            result += static_cast<char16_t>(0xdc00 + (point & 0x3ff));
        } else result += static_cast<char16_t>(point);
    }
    if (cursor >= index) utf16Cursor = static_cast<std::uint32_t>(result.size());
    return result;
}
}

void Attach(void *, void *window) {
    g_Window = static_cast<SDL_Window *>(window);
    g_State.Reset();
    g_Visible = false;
}
void Detach() {
    if (g_Window && SDL_TextInputActive(g_Window)) SDL_StopTextInput(g_Window);
    g_Window = nullptr;
    g_State.Reset();
    g_Visible = false;
}
void ObserveEvent(const SDL_Event &event) {
    if (!g_Window) return;
    const auto id = SDL_GetWindowID(g_Window);
    if (event.type == SDL_EVENT_TEXT_EDITING && event.edit.windowID == id) {
        if (!event.edit.text || !*event.edit.text) { g_State.Reset(); return; }
        CompositionUpdate update;
        update.hasText = update.hasCursor = update.hasTargetRanges = true;
        update.text = Decode(event.edit.text, std::max(0, event.edit.start), update.cursor);
        std::uint32_t end = update.cursor;
        Decode(event.edit.text, std::max(0, event.edit.start) + std::max(0, event.edit.length), end);
        if (end > update.cursor) update.targetRanges.push_back({update.cursor, end});
        g_State.ApplyComposition(std::move(update));
    } else if ((event.type == SDL_EVENT_TEXT_INPUT && event.text.windowID == id) ||
               (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && event.window.windowID == id)) {
        g_State.Reset();
    }
}
NativePresentation::MessageDisposition HandleNativeMessage(void *, std::uint32_t, std::uintptr_t, std::intptr_t) { return {}; }
// Candidate navigation and candidate windows belong to the system IME.
bool WantsCandidateNavigation() { return false; }
bool MoveCandidate(CandidateDirection) { return false; }
bool PreparePresentationFrame(bool visible, PresentationFrame &frame) {
    g_Visible = visible && g_Window && SDL_GetKeyboardFocus() == g_Window;
    if (!g_Visible) g_State.Reset();
    frame.snapshot = g_State.GetSnapshot();
    frame.revision = g_State.GetRevision();
    frame.candidateRevision = g_State.GetCandidateRevision();
    return g_Visible && frame.snapshot.HasContent();
}
bool IsPresentationActive() { return g_Visible && g_State.GetSnapshot().IsActive(); }
Diagnostics GetDiagnostics() {
    Diagnostics result;
    result.RootWindow = result.PresentationWindow = reinterpret_cast<std::uintptr_t>(g_Window);
    result.FocusWindow = reinterpret_cast<std::uintptr_t>(SDL_GetKeyboardFocus());
    result.PresentationVisible = g_Visible;
    result.CompositionActive = g_State.GetSnapshot().composing;
    return result;
}
}
