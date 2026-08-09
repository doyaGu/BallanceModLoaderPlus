#ifndef BML_SPEEDRUN_H
#define BML_SPEEDRUN_H

#include "BML/Generated/bml_speedrun_imc.hpp"

namespace BML::Speedrun {
struct TimerState { float ElapsedTime = 0.0f; };

namespace Detail {
namespace Api = Imc::Generated::Bml::Speedrun;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }
[[nodiscard]] inline int EmptyCommand(int (Api::Client::*command)(std::uint32_t)) { int status = RequireApi(); return status == BML_OK ? (Client().*command)(5000u) : status; }
}

[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }
[[nodiscard]] inline int ReadTimerState(TimerState &out) { Detail::Api::TimerStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallState(wire); if (status == BML_OK) out.ElapsedTime = wire.ElapsedTime; return status; }
[[nodiscard]] inline int SetTimerVisible(bool visible) { Detail::Api::VisibleInputValue input{}; input.Visible = visible; int status = RequireApi(); return status == BML_OK ? Detail::Client().CallSetTimerVisible(input) : status; }
[[nodiscard]] inline int StartTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallStartTimer); }
[[nodiscard]] inline int PauseTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallPauseTimer); }
[[nodiscard]] inline int ResetTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallResetTimer); }
} // namespace BML::Speedrun

#endif // BML_SPEEDRUN_H
