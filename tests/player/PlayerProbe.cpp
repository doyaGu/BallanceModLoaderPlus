#include "PlayerProbe.h"

#include <cstring>

namespace BML::PlayerTest {

namespace {

BMLPlayerProbeResult g_Result;
bool g_Started = false;

} // namespace

void ProbeReport::Reset() {
    g_Result = {};
    g_Started = false;
}

void ProbeReport::Report(std::uint32_t state, const char *detail) {
    if (g_Result.State != BML_PLAYER_PROBE_PENDING)
        return;
    const char *text = detail ? detail : "";
    std::size_t length = std::strlen(text);
    if (length > sizeof(g_Result.Detail) - 1)
        length = sizeof(g_Result.Detail) - 1;
    std::memcpy(g_Result.Detail, text, length);
    g_Result.Detail[length] = '\0';
    // The state is published last, so a driver reading between the two writes
    // sees PENDING instead of a verdict without its reason.
    g_Result.State = state;
}

bool ProbeReport::Started() { return g_Started; }

bool ProbeReport::Reported() {
    return g_Result.State != BML_PLAYER_PROBE_PENDING;
}

bool ProbeReport::Read(BMLPlayerProbeResult *result) {
    if (!result || result->Size != sizeof(BMLPlayerProbeResult))
        return false;
    *result = g_Result;
    return true;
}

bool ProbeReport::Start(const BMLPlayerProbeStartInfo *info) {
    if (!info || info->Size != sizeof(BMLPlayerProbeStartInfo) ||
        info->Version != 1)
        return false;
    g_Started = true;
    return true;
}

} // namespace BML::PlayerTest
