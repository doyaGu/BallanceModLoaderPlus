#ifndef BML_TESTS_PLAYER_PLAYERPROBEAPI_H
#define BML_TESTS_PLAYER_PLAYERPROBEAPI_H

#include <cstdint>

// The contract between the Player flow driver and the probe Mods it collects
// verdicts from. The driver owns the shipped menu graph, the tutorial exit and
// the frame captures; a probe owns exactly one subject under test and never
// navigates. Any Mod that exports BMLPlayerProbeRead is treated as a probe, so
// installing one is the only thing needed to make it part of a run.

enum BMLPlayerProbeState : std::uint32_t {
    // Still working. The driver keeps the level on screen and asks again.
    BML_PLAYER_PROBE_PENDING = 0,
    BML_PLAYER_PROBE_PASSED = 1,
    BML_PLAYER_PROBE_FAILED = 2,
    // Deliberately not run in this configuration. Counted as a pass, but
    // reported so a skipped subject can never look like a tested one.
    BML_PLAYER_PROBE_SKIPPED = 3,
};

struct BMLPlayerProbeResult {
    std::uint32_t Size = sizeof(BMLPlayerProbeResult);
    std::uint32_t Version = 1;
    std::uint32_t State = BML_PLAYER_PROBE_PENDING;
    char Detail[96]{};
};

// Handed to a probe when the driver has the level on screen, the tutorial
// exited and gameplay input live. Probes that read gameplay state export
// BMLPlayerProbeStart and wait for this instead of guessing from callbacks.
struct BMLPlayerProbeStartInfo {
    std::uint32_t Size = sizeof(BMLPlayerProbeStartInfo);
    std::uint32_t Version = 1;
};

using BMLPlayerProbeReadFn = int (__cdecl *)(BMLPlayerProbeResult *);
using BMLPlayerProbeStartFn = int (__cdecl *)(const BMLPlayerProbeStartInfo *);

#define BML_PLAYER_PROBE_READ_SYMBOL "BMLPlayerProbeRead"
#define BML_PLAYER_PROBE_START_SYMBOL "BMLPlayerProbeStart"

#endif // BML_TESTS_PLAYER_PLAYERPROBEAPI_H
