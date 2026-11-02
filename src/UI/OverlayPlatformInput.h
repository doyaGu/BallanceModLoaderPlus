#ifndef BML_OVERLAY_PLATFORM_INPUT_H
#define BML_OVERLAY_PLATFORM_INPUT_H

namespace Overlay::PlatformInput {
    // Attach after the ImGui platform backend and detach before its shutdown. Both
    // calls must run on the Player window's owning thread and are idempotent
    // for the same window.
    bool Attach(void *window);
    bool Detach();
#if BML_RUNTIME_SDL
    // Submit copied event payloads on the game thread, once per frame.
    void ProcessEvents();
#endif
}

#endif // BML_OVERLAY_PLATFORM_INPUT_H
