#ifndef BML_CORE_H
#define BML_CORE_H

#include "BML/Import.h"

namespace BML {
namespace Core {

namespace Detail {
static constexpr const char *BuiltinModId = "BML";
}

namespace UI {

inline int SendMessage(const char *message) {
    static Import<void(const char *)> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.message.add", "void(string)");
    return import.Invoke(message);
}

inline int ClearMessages() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.message.clear", "void()");
    return import.Invoke();
}

} // namespace UI

namespace Menu {

inline int OpenModsMenu() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.menu.mods.open", "void()");
    return import.Invoke();
}

inline int CloseModsMenu() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.menu.mods.close", "void()");
    return import.Invoke();
}

inline int OpenMapMenu() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.menu.maps.open", "void()");
    return import.Invoke();
}

inline int CloseMapMenu() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.menu.maps.close", "void()");
    return import.Invoke();
}

} // namespace Menu

namespace HUD {

inline int GetMode(int fallback = 0) {
    static Import<int()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.get", "int()");
    return import.InvokeOr(fallback);
}

inline int SetMode(int mode) {
    static Import<void(int)> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.set", "void(int)");
    return import.Invoke(mode);
}

inline int ShowTitle(bool show) {
    static Import<void(bool)> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.title.show", "void(bool)");
    return import.Invoke(show);
}

inline int ShowFPS(bool show) {
    static Import<void(bool)> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.fps.show", "void(bool)");
    return import.Invoke(show);
}

inline int ShowSRTimer(bool show) {
    static Import<void(bool)> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.sr.show", "void(bool)");
    return import.Invoke(show);
}

inline int StartSRTimer() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.sr.start", "void()");
    return import.Invoke();
}

inline int PauseSRTimer() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.sr.pause", "void()");
    return import.Invoke();
}

inline int ResetSRTimer() {
    static Import<void()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.sr.reset", "void()");
    return import.Invoke();
}

inline float GetSRTime(float fallback = 0.0f) {
    static Import<float()> import;
    if (!import.IsBound())
        import.Bind(Detail::BuiltinModId, "ui.hud.sr.time", "float()");
    return import.InvokeOr(fallback);
}

} // namespace HUD

} // namespace Core
} // namespace BML

#endif // BML_CORE_H
