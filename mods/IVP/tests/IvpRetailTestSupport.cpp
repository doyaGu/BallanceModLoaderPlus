#include "BML/IVP/Calls.h"

#include <cstdint>

std::uintptr_t gRetailModuleBase = 0;

extern "C" std::uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return gRetailModuleBase ? gRetailModuleBase + rva : 0;
}
