#pragma once

#include "PlayerProbeApi.h"

namespace BML::PlayerTest {

// The probe side of the driver contract. One instance per probe Mod lives in
// the Mod's own DLL, so the driver reads a verdict per module and a probe never
// has to know about the others.
class ProbeReport {
public:
    // Called from OnLoad so a reloaded Mod cannot report a stale verdict.
    static void Reset();

    static void Report(std::uint32_t state, const char *detail);
    static void Pass(const char *detail) {
        Report(BML_PLAYER_PROBE_PASSED, detail);
    }
    static void Fail(const char *detail) {
        Report(BML_PLAYER_PROBE_FAILED, detail);
    }
    static void Skip(const char *detail) {
        Report(BML_PLAYER_PROBE_SKIPPED, detail);
    }

    // True once the driver has the level on screen with gameplay input live.
    [[nodiscard]] static bool Started();
    [[nodiscard]] static bool Reported();

    // Both are called through the exported entry points below.
    static bool Read(BMLPlayerProbeResult *result);
    static bool Start(const BMLPlayerProbeStartInfo *info);
};

} // namespace BML::PlayerTest

// Exports the read entry point. Probes that do not read gameplay state use
// this alone and start themselves from the usual Mod callbacks.
#define BML_PLAYER_PROBE_READ_EXPORT()                                        \
    extern "C" __declspec(dllexport) int __cdecl BMLPlayerProbeRead(          \
        BMLPlayerProbeResult *result) {                                       \
        return BML::PlayerTest::ProbeReport::Read(result) ? 1 : 0;            \
    }

// Exports both entry points, so the driver decides when the probe may touch
// the ball, the camera or gameplay input.
#define BML_PLAYER_PROBE_EXPORTS()                                            \
    BML_PLAYER_PROBE_READ_EXPORT()                                            \
    extern "C" __declspec(dllexport) int __cdecl BMLPlayerProbeStart(         \
        const BMLPlayerProbeStartInfo *info) {                                \
        return BML::PlayerTest::ProbeReport::Start(info) ? 1 : 0;             \
    }
