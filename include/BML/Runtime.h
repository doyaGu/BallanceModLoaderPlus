// Reads loader runtime state. The interface struct below is what the loader hands
// out and what a Mod written in C uses directly; the inline C++ namespace under it
// is the same thing with the lookup and the checks folded in, so including this
// header still costs nothing at link time.
//
// Interface.h explains the header, the version rules, and BML_IFACE_HAS. Every
// function here runs on the calling thread, so call them from the game thread.
//
// Each read fills its out parameter only when it answers BML_OK, and answers
// BML_ERROR_INVALID_PARAMETER for a null out or BML_ERROR_FAIL before the loader
// has loaded its mods.
//
// These reads and the IBML getters are not two sources of truth. ReadState and
// IBML::IsIngame both come out of the same ModContext snapshot, so they cannot
// disagree. Use whichever fits the calling code; ReadState is one call for all
// five flags and reports why it failed, IBML::IsIngame is one bool with no
// error channel.
#ifndef BML_RUNTIME_H
#define BML_RUNTIME_H

#include "BML/Interface.h"

BML_BEGIN_CDECLS

#define BML_RUNTIME_INTERFACE_ID "bml.runtime"
#define BML_RUNTIME_INTERFACE_MAJOR 1
#define BML_RUNTIME_INTERFACE_MINOR 0

// Each flag is 0 or 1. These are C structs and have no default member
// initializers: zero them with BML_RuntimeState state = {0} in C, or with
// State state{} in C++.
typedef struct BML_RuntimeState {
    int InGame;
    int InLevel;
    int Paused;
    int Playing;
    int CheatEnabled;
} BML_RuntimeState;

// TimeMs and AbsoluteMs are the CKTimeManager clocks; DeltaMs is the last frame
// delta. Frame is the main tick count, saturated at INT_MAX.
typedef struct BML_RuntimeClock {
    float TimeMs;
    float AbsoluteMs;
    float DeltaMs;
    int Frame;
} BML_RuntimeClock;

// Despite the struct name, SR is not a score: it is the elapsed speedrun time in
// milliseconds, taken from the loader's own SR timer rather than from
// CKTimeManager. HS is the highscore value the game computes as points plus 200
// per remaining life.
typedef struct BML_RuntimeScore {
    float SR;
    int HS;
} BML_RuntimeScore;

typedef struct BML_RuntimeInterface {
    BML_InterfaceHeader Header;

    int (*ReadState)(BML_RuntimeState *out);

    // Answers BML_ERROR_UNAVAILABLE when the Virtools time manager is not up yet.
    int (*ReadClock)(BML_RuntimeClock *out);

    int (*ReadScore)(BML_RuntimeScore *out);
} BML_RuntimeInterface;

BML_END_CDECLS

#ifdef __cplusplus

namespace BML::Runtime {

using State = BML_RuntimeState;
using Clock = BML_RuntimeClock;
using Score = BML_RuntimeScore;

namespace Detail {

// Looked up once per Mod. The loader's table is static, so a null answer means
// the running loader does not carry this interface at all, which is not something
// that can change later in the process.
inline const BML_RuntimeInterface *Interface() {
    static const BML_RuntimeInterface *found =
        FindInterface<BML_RuntimeInterface>(BML_RUNTIME_INTERFACE_ID, BML_RUNTIME_INTERFACE_MAJOR);
    return found;
}

} // namespace Detail

// Whether the running loader carries this interface. The functions below check
// for themselves, so call this directly only to probe.
[[nodiscard]] inline int RequireApi() {
    return Detail::Interface() != nullptr ? BML_OK : BML_ERROR_NOT_FOUND;
}

[[nodiscard]] inline int ReadState(State &out) {
    const BML_RuntimeInterface *runtime = Detail::Interface();
    if (!BML_IFACE_HAS(runtime, BML_RuntimeInterface, ReadState))
        return BML_ERROR_NOT_FOUND;
    return runtime->ReadState(&out);
}

[[nodiscard]] inline int ReadClock(Clock &out) {
    const BML_RuntimeInterface *runtime = Detail::Interface();
    if (!BML_IFACE_HAS(runtime, BML_RuntimeInterface, ReadClock))
        return BML_ERROR_NOT_FOUND;
    return runtime->ReadClock(&out);
}

[[nodiscard]] inline int ReadScore(Score &out) {
    const BML_RuntimeInterface *runtime = Detail::Interface();
    if (!BML_IFACE_HAS(runtime, BML_RuntimeInterface, ReadScore))
        return BML_ERROR_NOT_FOUND;
    return runtime->ReadScore(&out);
}

} // namespace BML::Runtime

#endif // __cplusplus

#endif // BML_RUNTIME_H
