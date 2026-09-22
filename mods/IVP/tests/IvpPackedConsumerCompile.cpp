#pragma pack(push, 1)

struct CallerPackedBeforeIvp {
    char Tag;
    double Value;
};

#include "BML/IVP/IVP.h"

struct CallerPackedAfterIvp {
    char Tag;
    double Value;
};

#pragma pack(pop)

// The umbrella must temporarily select Ballance's /Zp8-compatible layout and
// then restore the consuming Mod's previous setting without leaking it.
static_assert(sizeof(CallerPackedBeforeIvp) == 9);
static_assert(sizeof(CallerPackedAfterIvp) == 9);
static_assert(sizeof(IVP_Material_Simple) == 0x30);
static_assert(sizeof(IVP_Core) == 0x238);
static_assert(sizeof(IVP_Environment) == 0x178);
