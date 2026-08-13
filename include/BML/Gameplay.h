// Reads Ballance gameplay state. The interface struct below is what the loader
// hands out and what a Mod written in C uses directly; the inline C++ namespace
// under it is the same thing with the lookup and the checks folded in, so
// including this header still costs nothing at link time.
//
// Interface.h explains the header, the version rules, BML_IFACE_HAS, and how text
// is written into a fixed-capacity buffer. Every function here reads one of the
// game's own CKDataArrays, so every one of them answers BML_ERROR_WRONG_THREAD
// when called from any other thread than the game thread.
//
// Those arrays are CurrentLevel, Energy, AllLevel, Checkpoints, and ResetPoints.
// The loader checks the expected column names before reading, so all of these
// answer BML_ERROR_UNAVAILABLE rather than garbage when the array is missing,
// which is the normal state outside a level, and when a custom level has changed
// the column layout. Treat that status as "not available right now" and retry.
//
// Each read fills its out parameter only when it answers BML_OK, and answers
// BML_ERROR_INVALID_PARAMETER for a null argument or BML_ERROR_FAIL before the
// loader has loaded its mods.
//
// The three collections are read as a count followed by one row at a time, and
// each row comes from the array as it is right then. A level change between two
// calls can shift what an index means and shorten the array, so a row index that
// was valid a moment ago answers BML_ERROR_NOT_FOUND. Read the count again rather
// than keeping it.
//
// Nothing here writes: changing gameplay state still goes through the Virtools
// arrays directly.
#ifndef BML_GAMEPLAY_H
#define BML_GAMEPLAY_H

#include "BML/Interface.h"
#include "BML/Types.h"

BML_BEGIN_CDECLS

#define BML_GAMEPLAY_INTERFACE_ID "bml.gameplay"
#define BML_GAMEPLAY_INTERFACE_MAJOR 1
#define BML_GAMEPLAY_INTERFACE_MINOR 0

// Capacity of each of the three name buffers below, terminator included.
#define BML_GAMEPLAY_NAME_CAPACITY 128u

// From the CurrentLevel array. ActiveBall is null between levels. ResetMatrix is
// the Ball_Pos_Frame transform the game respawns the ball at. Points here is the
// level's own counter and is not the same column as the one in
// BML_GameplayEnergyState.
//
// These are C structs and have no default member initializers: zero them with
// BML_GameplayLevelState level = {0} in C, or with LevelState level{} in C++.
typedef struct BML_GameplayLevelState {
    int Id;
    BML_ObjectRef ActiveBall;
    BML_Mat4 ResetMatrix;
    int Points;
} BML_GameplayLevelState;

// From the Energy array, in its column order: current points and lives, the
// values the level started with, the time factor, and the per-life bonus.
typedef struct BML_GameplayEnergyState {
    int Points;
    int Lives;
    int StartPoints;
    int StartLives;
    float TimeFactor;
    int LifeBonus;
} BML_GameplayEnergyState;

// One row of the AllLevel array. File is the level filename, StartBall the ball
// type it starts with, Sky the sky material, and Music the track index. Each of
// the three names is always terminated and holds at most
// BML_GAMEPLAY_NAME_CAPACITY - 1 characters, and each has its own Length member
// saying how long that name actually is.
typedef struct BML_GameplayCatalogEntry {
    char File[BML_GAMEPLAY_NAME_CAPACITY];
    int FileLength;
    char StartBall[BML_GAMEPLAY_NAME_CAPACITY];
    int StartBallLength;
    char Sky[BML_GAMEPLAY_NAME_CAPACITY];
    int SkyLength;
    int Bonus;
    int Music;
} BML_GameplayCatalogEntry;

// One row of the Checkpoints array.
typedef struct BML_GameplayCheckpoint {
    BML_Mat4 Matrix;
    BML_ObjectRef Object;
} BML_GameplayCheckpoint;

// One row of the ResetPoints array.
typedef struct BML_GameplayResetpoint {
    BML_ObjectRef Object;
} BML_GameplayResetpoint;

typedef struct BML_GameplayInterface {
    BML_InterfaceHeader Header;

    int (*ReadLevel)(BML_GameplayLevelState *out);
    int (*ReadEnergy)(BML_GameplayEnergyState *out);

    // How many rows the AllLevel array has right now.
    int (*ReadCatalogCount)(size_t *out);
    // Answers BML_ERROR_NOT_FOUND when index is past the last row.
    int (*ReadCatalogEntry)(size_t index, BML_GameplayCatalogEntry *out);

    int (*ReadCheckpointCount)(size_t *out);
    int (*ReadCheckpoint)(size_t index, BML_GameplayCheckpoint *out);

    int (*ReadResetpointCount)(size_t *out);
    int (*ReadResetpoint)(size_t index, BML_GameplayResetpoint *out);
} BML_GameplayInterface;

BML_END_CDECLS

#ifdef __cplusplus

#include <cstddef>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace BML::Gameplay {

using ObjectRef = BML_ObjectRef;
using Mat4 = BML_Mat4;
using LevelState = BML_GameplayLevelState;
using EnergyState = BML_GameplayEnergyState;
using Checkpoint = BML_GameplayCheckpoint;
using Resetpoint = BML_GameplayResetpoint;

// The C++ shape of BML_GameplayCatalogEntry. Each string contains the text that
// fit in its C buffer; the corresponding Truncated member reports whether the
// source text was longer.
struct CatalogEntry {
    std::string File;
    std::string StartBall;
    std::string Sky;
    int Bonus = 0;
    int Music = 0;
    bool FileTruncated = false;
    bool StartBallTruncated = false;
    bool SkyTruncated = false;
};

namespace Detail {

// Looked up once per Mod. The loader's table is static, so a null answer means
// the running loader does not carry this interface at all, which is not something
// that can change later in the process.
inline const BML_GameplayInterface *Interface() {
    static const BML_GameplayInterface *found = FindInterface<BML_GameplayInterface>(
        BML_GAMEPLAY_INTERFACE_ID, BML_GAMEPLAY_INTERFACE_MAJOR);
    return found;
}

// The shared body of the three list readers: ask for the count, then read one row
// at a time. out is replaced only once the whole list is in hand, so a level
// change partway through leaves the caller's vector as it was.
template <typename Value, typename Row, typename Convert>
int ReadList(int (*readCount)(std::size_t *), int (*readRow)(std::size_t, Row *),
             std::vector<Value> &out, Convert convert) {
    std::size_t count = 0;
    int status = readCount(&count);
    if (status != BML_OK)
        return status;

    std::vector<Value> values;
    try {
        values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            Row row = {};
            status = readRow(index, &row);
            if (status != BML_OK)
                return status;
            values.push_back(convert(row));
        }
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = std::move(values);
    return BML_OK;
}

} // namespace Detail

// Whether the running loader carries this interface. The functions below check
// for themselves, so call this directly only to probe.
[[nodiscard]] inline int RequireApi() {
    return Detail::Interface() != nullptr ? BML_OK : BML_ERROR_NOT_FOUND;
}

[[nodiscard]] inline int ReadLevel(LevelState &out) {
    const BML_GameplayInterface *gameplay = Detail::Interface();
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadLevel))
        return BML_ERROR_NOT_FOUND;
    return gameplay->ReadLevel(&out);
}

[[nodiscard]] inline int ReadEnergy(EnergyState &out) {
    const BML_GameplayInterface *gameplay = Detail::Interface();
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadEnergy))
        return BML_ERROR_NOT_FOUND;
    return gameplay->ReadEnergy(&out);
}

// The three list readers replace the contents of out on success and leave it
// alone on failure.
[[nodiscard]] inline int ReadCatalog(std::vector<CatalogEntry> &out) {
    const BML_GameplayInterface *gameplay = Detail::Interface();
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadCatalogCount) ||
        !BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadCatalogEntry))
        return BML_ERROR_NOT_FOUND;
    return Detail::ReadList(gameplay->ReadCatalogCount, gameplay->ReadCatalogEntry, out,
                            [](const BML_GameplayCatalogEntry &row) {
                                return CatalogEntry{std::string(row.File), std::string(row.StartBall),
                                                    std::string(row.Sky), row.Bonus, row.Music,
                                                    row.FileLength >= static_cast<int>(BML_GAMEPLAY_NAME_CAPACITY),
                                                    row.StartBallLength >= static_cast<int>(BML_GAMEPLAY_NAME_CAPACITY),
                                                    row.SkyLength >= static_cast<int>(BML_GAMEPLAY_NAME_CAPACITY)};
                            });
}

[[nodiscard]] inline int ReadCheckpoints(std::vector<Checkpoint> &out) {
    const BML_GameplayInterface *gameplay = Detail::Interface();
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadCheckpointCount) ||
        !BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadCheckpoint))
        return BML_ERROR_NOT_FOUND;
    return Detail::ReadList(gameplay->ReadCheckpointCount, gameplay->ReadCheckpoint, out,
                            [](const BML_GameplayCheckpoint &row) { return row; });
}

[[nodiscard]] inline int ReadResetpoints(std::vector<Resetpoint> &out) {
    const BML_GameplayInterface *gameplay = Detail::Interface();
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadResetpointCount) ||
        !BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadResetpoint))
        return BML_ERROR_NOT_FOUND;
    return Detail::ReadList(gameplay->ReadResetpointCount, gameplay->ReadResetpoint, out,
                            [](const BML_GameplayResetpoint &row) { return row; });
}

} // namespace BML::Gameplay

#endif // __cplusplus

#endif // BML_GAMEPLAY_H
