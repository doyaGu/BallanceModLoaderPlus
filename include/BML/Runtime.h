#ifndef BML_RUNTIME_H
#define BML_RUNTIME_H
#include "BML/Generated/bml_runtime_imc.hpp"
namespace BML::Runtime {
struct State { bool InGame = false; bool InLevel = false; bool Paused = false; bool Playing = false; bool CheatEnabled = false; };
struct Clock { float TimeMs = 0.0f; float AbsoluteMs = 0.0f; float DeltaMs = 0.0f; int Frame = 0; };
struct Score { float SR = 0.0f; int HS = 0; };
namespace Detail {
namespace Api = Imc::Generated::Bml::Runtime;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }
}
[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }
[[nodiscard]] inline int ReadState(State &out) { Imc::Generated::Bml::Runtime::RuntimeStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallState(wire); if (status == BML_OK) out = {wire.InGame, wire.InLevel, wire.Paused, wire.Playing, wire.CheatEnabled}; return status; }
[[nodiscard]] inline int ReadClock(Clock &out) { Imc::Generated::Bml::Runtime::ClockStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallClock(wire); if (status == BML_OK) out = {wire.TimeMs, wire.AbsoluteMs, wire.DeltaMs, wire.Frame}; return status; }
[[nodiscard]] inline int ReadScore(Score &out) { Imc::Generated::Bml::Runtime::ScoreStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallScore(wire); if (status == BML_OK) out = {wire.Sr, wire.Hs}; return status; }
} // namespace BML::Runtime
#endif // BML_RUNTIME_H
