#ifndef BML_OVERLAY_PLATFORM_INPUT_H
#define BML_OVERLAY_PLATFORM_INPUT_H

namespace Overlay::PlatformInput {
    // Attach after ImGui_ImplWin32_Init and detach before its shutdown. Both
    // calls must run on the Player window's owning thread and are idempotent
    // for the same window.
    bool Attach(void *window);
    bool Detach();
}

#endif // BML_OVERLAY_PLATFORM_INPUT_H
