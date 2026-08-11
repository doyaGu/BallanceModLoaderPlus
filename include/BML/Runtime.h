// Reads loader runtime state over IMC. This is a thin inline wrapper around the
// generated bml.runtime client, so including it costs nothing at link time.
//
// Every function returns a BML_* status code and leaves its out parameter
// untouched unless it returns BML_OK. The first call opens the IMC client and
// caches it; BML_ERROR_FROZEN or BML_ERROR_IMC_ENDPOINT_NOT_FOUND means the
// loader has not published these routes yet, which is the case before mods
// finish loading.
//
// Calling from the main thread is safe: the loader dispatches the handler inline
// there, so the call completes without blocking. Off the main thread the call
// blocks until the loader answers or the 5000 ms default timeout expires, and
// then returns BML_ERROR_TIMEOUT.
//
// These reads and the IBML getters are not two sources of truth. ReadState and
// IBML::IsIngame both come out of the same ModContext snapshot, so they cannot
// disagree. Use whichever fits the calling code; ReadState is one call for all
// five flags and reports why it failed, IBML::IsIngame is one bool with no
// error channel.
#ifndef BML_RUNTIME_H
#define BML_RUNTIME_H

#include "BML/Generated/bml_runtime_imc.hpp"

namespace BML::Runtime {

struct State {
    bool InGame = false;
    bool InLevel = false;
    bool Paused = false;
    bool Playing = false;
    bool CheatEnabled = false;
};

// TimeMs and AbsoluteMs are the CKTimeManager clocks; DeltaMs is the last frame
// delta. Frame is the main tick count, saturated at INT_MAX.
struct Clock {
    float TimeMs = 0.0f;
    float AbsoluteMs = 0.0f;
    float DeltaMs = 0.0f;
    int Frame = 0;
};

// Despite the struct name, SR is not a score: it is the elapsed speedrun time in
// milliseconds, taken from the loader's own SR timer rather than from
// CKTimeManager. HS is the highscore value the game computes as points plus 200
// per remaining life.
struct Score {
    float SR = 0.0f;
    int HS = 0;
};

namespace Detail {

namespace Api = Imc::Generated::Bml::Runtime;

inline Imc::LazyClient<Api::Client> &ClientState() {
    static Imc::LazyClient<Api::Client> state;
    return state;
}

inline Api::Client &Client() { return ClientState().Get(); }

[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }

} // namespace Detail

// Opens the client if it is not open yet. The Read functions below already do
// this, so call it directly only to probe whether the routes exist.
[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }

[[nodiscard]] inline int ReadState(State &out) {
    Imc::Generated::Bml::Runtime::RuntimeStateValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallState(wire);
    if (status == BML_OK)
        out = {wire.InGame, wire.InLevel, wire.Paused, wire.Playing, wire.CheatEnabled};
    return status;
}

[[nodiscard]] inline int ReadClock(Clock &out) {
    Imc::Generated::Bml::Runtime::ClockStateValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallClock(wire);
    if (status == BML_OK)
        out = {wire.TimeMs, wire.AbsoluteMs, wire.DeltaMs, wire.Frame};
    return status;
}

[[nodiscard]] inline int ReadScore(Score &out) {
    Imc::Generated::Bml::Runtime::ScoreStateValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallScore(wire);
    if (status == BML_OK)
        out = {wire.Sr, wire.Hs};
    return status;
}

} // namespace BML::Runtime

#endif // BML_RUNTIME_H
