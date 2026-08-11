// Drives the loader's speedrun timer over IMC. This is a thin inline wrapper
// around the generated bml.speedrun client, so including it costs nothing at
// link time.
//
// Status handling, first-call client opening, and the thread rules are the same
// as in Runtime.h.
//
// There is exactly one timer, the one the loader shows on its own HUD, and the
// loader keeps driving it: it resets on level start, pauses on level pause and
// when the counter goes inactive, and starts again on unpause and when the
// counter goes active. A mod that starts, pauses, or resets the timer is
// therefore changing shared state that the loader will overwrite at its next
// gameplay event. Read the elapsed time freely; write only if the mod is
// deliberately taking the timer over.
#ifndef BML_SPEEDRUN_H
#define BML_SPEEDRUN_H

#include "BML/Generated/bml_speedrun_imc.hpp"

namespace BML::Speedrun {

// ElapsedTime is in milliseconds, and is the same value Runtime::ReadScore
// reports as Score::SR.
struct TimerState {
    float ElapsedTime = 0.0f;
};

namespace Detail {

namespace Api = Imc::Generated::Bml::Speedrun;

inline Imc::LazyClient<Api::Client> &ClientState() {
    static Imc::LazyClient<Api::Client> state;
    return state;
}

inline Api::Client &Client() { return ClientState().Get(); }

[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }

// Shared body for the argument-free routes. The 5000u is the call timeout in
// milliseconds, matching the generated client default.
[[nodiscard]] inline int EmptyCommand(int (Api::Client::*command)(std::uint32_t)) {
    int status = RequireApi();
    return status == BML_OK ? (Client().*command)(5000u) : status;
}

} // namespace Detail

// Opens the client if it is not open yet. The functions below already do this,
// so call it directly only to probe whether the routes exist.
[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }

[[nodiscard]] inline int ReadTimerState(TimerState &out) {
    Detail::Api::TimerStateValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallState(wire);
    if (status == BML_OK)
        out.ElapsedTime = wire.ElapsedTime;
    return status;
}

// Toggles only the visibility of the HUD timer element, without writing the
// loader's ShowSR configuration entry. The loader restores the configured value
// on the next level start, and hides the element on level exit. Use
// UI::SetHUDMode with HUD_SR for a lasting change.
[[nodiscard]] inline int SetTimerVisible(bool visible) {
    Detail::Api::VisibleInputValue input{};
    input.Visible = visible;
    int status = RequireApi();
    return status == BML_OK ? Detail::Client().CallSetTimerVisible(input) : status;
}

// Resumes counting from the current elapsed time rather than from zero. Call
// ResetTimer first for a fresh run.
[[nodiscard]] inline int StartTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallStartTimer); }

// Stops counting and keeps the elapsed time.
[[nodiscard]] inline int PauseTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallPauseTimer); }

// Zeroes the elapsed time and leaves the timer stopped.
[[nodiscard]] inline int ResetTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallResetTimer); }

} // namespace BML::Speedrun

#endif // BML_SPEEDRUN_H
