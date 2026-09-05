#ifndef BML_BEHAVIOR_BLOCKS_H
#define BML_BEHAVIOR_BLOCKS_H

// The Loader uses the same per-Block field definitions as Native Mods, lowered
// to the private BlockSpec instead of the public C interface.
#define BML_BEHAVIOR_INTERNAL
#include "BML/Behavior/Blocks.hpp"
#undef BML_BEHAVIOR_INTERNAL

#endif // BML_BEHAVIOR_BLOCKS_H
