// Reads Ballance gameplay state. The interface struct below is what the loader
// hands out and what a Mod written in C uses directly; the inline C++ namespace
// under it is the same thing with the lookup and the checks folded in, so
// including this header still costs nothing at link time.
//
// Interface.h explains the header, the version rules, BML_IFACE_HAS, and how text
// is written into a fixed-capacity buffer. Reads use game-thread state and
// answer BML_ERROR_WRONG_THREAD on any other thread.
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
#include "BML/Mod.h"

BML_BEGIN_CDECLS
#define BML_BALL_TYPE_DEFINITION_1_0_SIZE (offsetof(BML_BallTypeDefinition, Radius) + sizeof(((BML_BallTypeDefinition *)0)->Radius))
#define BML_BALL_TYPE_DEFINITION_INIT { .StructSize = sizeof(BML_BallTypeDefinition) }
#define BML_FLOOR_TYPE_DEFINITION_1_0_SIZE (offsetof(BML_FloorTypeDefinition, EnableCollision) + sizeof(((BML_FloorTypeDefinition *)0)->EnableCollision))
#define BML_FLOOR_TYPE_DEFINITION_INIT { .StructSize = sizeof(BML_FloorTypeDefinition) }
#define BML_MODULE_BALL_DEFINITION_1_0_SIZE (offsetof(BML_ModuleBallDefinition, Radius) + sizeof(((BML_ModuleBallDefinition *)0)->Radius))
#define BML_MODULE_BALL_DEFINITION_INIT { .StructSize = sizeof(BML_ModuleBallDefinition) }
#define BML_MODULE_CONVEX_DEFINITION_1_0_SIZE (offsetof(BML_ModuleConvexDefinition, RotDamp) + sizeof(((BML_ModuleConvexDefinition *)0)->RotDamp))
#define BML_MODULE_CONVEX_DEFINITION_INIT { .StructSize = sizeof(BML_ModuleConvexDefinition) }
#define BML_TRAFO_DEFINITION_1_0_SIZE (offsetof(BML_TrafoDefinition, Name) + sizeof(((BML_TrafoDefinition *)0)->Name))
#define BML_TRAFO_DEFINITION_INIT { .StructSize = sizeof(BML_TrafoDefinition) }
#define BML_MODULE_DEFINITION_1_0_SIZE (offsetof(BML_ModuleDefinition, Name) + sizeof(((BML_ModuleDefinition *)0)->Name))
#define BML_MODULE_DEFINITION_INIT { .StructSize = sizeof(BML_ModuleDefinition) }
#define BML_GAMEPLAY_STATE_1_0_SIZE (offsetof(BML_GameplayState, Playing) + sizeof(((BML_GameplayState *)0)->Playing))
#define BML_GAMEPLAY_STATE_INIT { .StructSize = sizeof(BML_GameplayState) }

#pragma pack(push, 8)

#define BML_GAMEPLAY_INTERFACE_ID "bml.gameplay"
#define BML_GAMEPLAY_INTERFACE_MAJOR 1
#define BML_GAMEPLAY_INTERFACE_MINOR 2

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

typedef struct BML_BallTypeDefinition {
    size_t StructSize;
    const char *BallFile, *BallId, *BallName, *ObjectName;
    float Friction, Elasticity, Mass;
    const char *CollisionGroup;
    float LinearDamp, RotDamp, Force, Radius;
} BML_BallTypeDefinition;
typedef struct BML_FloorTypeDefinition {
    size_t StructSize;
    const char *Name;
    float Friction, Elasticity, Mass;
    const char *CollisionGroup;
    uint32_t EnableCollision;
} BML_FloorTypeDefinition;
typedef struct BML_ModuleBallDefinition {
    size_t StructSize;
    const char *Name;
    uint32_t Fixed;
    float Friction, Elasticity, Mass;
    const char *CollisionGroup;
    uint32_t StartFrozen, EnableCollision, CalcMassCenter;
    float LinearDamp, RotDamp, Radius;
} BML_ModuleBallDefinition;
typedef struct BML_ModuleConvexDefinition {
    size_t StructSize;
    const char *Name;
    uint32_t Fixed;
    float Friction, Elasticity, Mass;
    const char *CollisionGroup;
    uint32_t StartFrozen, EnableCollision, CalcMassCenter;
    float LinearDamp, RotDamp;
} BML_ModuleConvexDefinition;
typedef struct BML_TrafoDefinition {
    size_t StructSize;
    const char *Name;
} BML_TrafoDefinition;
typedef struct BML_ModuleDefinition {
    size_t StructSize;
    const char *Name;
} BML_ModuleDefinition;
typedef struct BML_GameplayState {
    size_t StructSize;
    uint32_t Ingame, Paused, Playing;
} BML_GameplayState;


typedef struct BML_GameplayInterface {
    BML_InterfaceHeader Header;

    int (BML_CDECL *ReadLevel)(BML_GameplayLevelState *out);
    int (BML_CDECL *ReadEnergy)(BML_GameplayEnergyState *out);

    // How many rows the AllLevel array has right now.
    int (BML_CDECL *ReadCatalogCount)(size_t *out);
    // Answers BML_ERROR_NOT_FOUND when index is past the last row.
    int (BML_CDECL *ReadCatalogEntry)(size_t index, BML_GameplayCatalogEntry *out);

    int (BML_CDECL *ReadCheckpointCount)(size_t *out);
    int (BML_CDECL *ReadCheckpoint)(size_t index, BML_GameplayCheckpoint *out);

    int (BML_CDECL *ReadResetpointCount)(size_t *out);
    int (BML_CDECL *ReadResetpoint)(size_t index, BML_GameplayResetpoint *out);

    // ReadCheatEnabled is available whenever the loader is active.
    // ReadHighScore validates Energy and answers
    // BML_ERROR_UNAVAILABLE outside a level.
    int (BML_CDECL *ReadHighScore)(int *out);
    int (BML_CDECL *ReadCheatEnabled)(int *out);
    int (BML_CDECL *RegisterBallType)(BML_ModContextHandle, const BML_BallTypeDefinition *);
    int (BML_CDECL *RegisterFloorType)(BML_ModContextHandle, const BML_FloorTypeDefinition *);
    int (BML_CDECL *RegisterModuleBall)(BML_ModContextHandle, const BML_ModuleBallDefinition *);
    int (BML_CDECL *RegisterModuleConvex)(BML_ModContextHandle, const BML_ModuleConvexDefinition *);
    int (BML_CDECL *RegisterTrafo)(BML_ModContextHandle, const BML_TrafoDefinition *);
    int (BML_CDECL *RegisterModule)(BML_ModContextHandle, const BML_ModuleDefinition *);
    int (BML_CDECL *ReadState)(BML_GameplayState *out);
    int (BML_CDECL *EnableCheat)(BML_ModContextHandle, uint32_t enabled);
    int (BML_CDECL *ExitGame)(BML_ModContextHandle);
} BML_GameplayInterface;

#pragma pack(pop)

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
int ReadList(int (BML_CDECL *readCount)(std::size_t *),
             int (BML_CDECL *readRow)(std::size_t, Row *),
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

[[nodiscard]] inline int ReadHighScore(int &out) {
    const BML_GameplayInterface *gameplay = Detail::Interface();
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadHighScore))
        return BML_ERROR_NOT_FOUND;
    return gameplay->ReadHighScore(&out);
}

[[nodiscard]] inline int ReadCheatEnabled(bool &out) {
    const BML_GameplayInterface *gameplay = Detail::Interface();
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadCheatEnabled))
        return BML_ERROR_NOT_FOUND;
    int enabled = 0;
    const int status = gameplay->ReadCheatEnabled(&enabled);
    if (status == BML_OK)
        out = enabled != 0;
    return status;
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


#include "BML/Mod.hpp"
namespace BML::Gameplay {
struct BallTypeDefinition {
    std::string BallFile;
    std::string BallId;
    std::string BallName;
    std::string ObjectName;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
    float Force = 0.0f;
    float Radius = 0.0f;
};
[[nodiscard]] inline int RegisterBallType(const Mod::Context &ctx, const BallTypeDefinition &value) {
    if (!BML::Detail::IsText(value.BallFile) || !BML::Detail::IsText(value.BallId) || !BML::Detail::IsText(value.BallName) || !BML::Detail::IsText(value.ObjectName) || !BML::Detail::IsText(value.CollisionGroup)) return BML_ERROR_INVALID_PARAMETER;
    const BML_BallTypeDefinition record{sizeof(BML_BallTypeDefinition), value.BallFile.c_str(), value.BallId.c_str(), value.BallName.c_str(), value.ObjectName.c_str(), value.Friction, value.Elasticity, value.Mass, value.CollisionGroup.c_str(), value.LinearDamp, value.RotDamp, value.Force, value.Radius};
    const auto *api = Detail::Interface();
    return BML_IFACE_HAS(api, BML_GameplayInterface, RegisterBallType) ? api->RegisterBallType(ctx.Handle(), &record) : BML_ERROR_UNAVAILABLE;
}
struct FloorTypeDefinition {
    std::string Name;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool EnableCollision = true;
};
[[nodiscard]] inline int RegisterFloorType(const Mod::Context &ctx, const FloorTypeDefinition &value) {
    if (!BML::Detail::IsText(value.Name) || !BML::Detail::IsText(value.CollisionGroup)) return BML_ERROR_INVALID_PARAMETER;
    const BML_FloorTypeDefinition record{sizeof(BML_FloorTypeDefinition), value.Name.c_str(), value.Friction, value.Elasticity, value.Mass, value.CollisionGroup.c_str(), value.EnableCollision ? 1u : 0u};
    const auto *api = Detail::Interface();
    return BML_IFACE_HAS(api, BML_GameplayInterface, RegisterFloorType) ? api->RegisterFloorType(ctx.Handle(), &record) : BML_ERROR_UNAVAILABLE;
}
struct ModuleBallDefinition {
    std::string Name;
    bool Fixed = false;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool StartFrozen = false;
    bool EnableCollision = true;
    bool CalcMassCenter = false;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
    float Radius = 0.0f;
};
[[nodiscard]] inline int RegisterModuleBall(const Mod::Context &ctx, const ModuleBallDefinition &value) {
    if (!BML::Detail::IsText(value.Name) || !BML::Detail::IsText(value.CollisionGroup)) return BML_ERROR_INVALID_PARAMETER;
    const BML_ModuleBallDefinition record{sizeof(BML_ModuleBallDefinition), value.Name.c_str(), value.Fixed ? 1u : 0u, value.Friction, value.Elasticity, value.Mass, value.CollisionGroup.c_str(), value.StartFrozen ? 1u : 0u, value.EnableCollision ? 1u : 0u, value.CalcMassCenter ? 1u : 0u, value.LinearDamp, value.RotDamp, value.Radius};
    const auto *api = Detail::Interface();
    return BML_IFACE_HAS(api, BML_GameplayInterface, RegisterModuleBall) ? api->RegisterModuleBall(ctx.Handle(), &record) : BML_ERROR_UNAVAILABLE;
}
struct ModuleConvexDefinition {
    std::string Name;
    bool Fixed = false;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool StartFrozen = false;
    bool EnableCollision = true;
    bool CalcMassCenter = false;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
};
[[nodiscard]] inline int RegisterModuleConvex(const Mod::Context &ctx, const ModuleConvexDefinition &value) {
    if (!BML::Detail::IsText(value.Name) || !BML::Detail::IsText(value.CollisionGroup)) return BML_ERROR_INVALID_PARAMETER;
    const BML_ModuleConvexDefinition record{sizeof(BML_ModuleConvexDefinition), value.Name.c_str(), value.Fixed ? 1u : 0u, value.Friction, value.Elasticity, value.Mass, value.CollisionGroup.c_str(), value.StartFrozen ? 1u : 0u, value.EnableCollision ? 1u : 0u, value.CalcMassCenter ? 1u : 0u, value.LinearDamp, value.RotDamp};
    const auto *api = Detail::Interface();
    return BML_IFACE_HAS(api, BML_GameplayInterface, RegisterModuleConvex) ? api->RegisterModuleConvex(ctx.Handle(), &record) : BML_ERROR_UNAVAILABLE;
}
struct TrafoDefinition {
    std::string Name;
};
[[nodiscard]] inline int RegisterTrafo(const Mod::Context &ctx, const TrafoDefinition &value) {
    if (!BML::Detail::IsText(value.Name)) return BML_ERROR_INVALID_PARAMETER;
    const BML_TrafoDefinition record{sizeof(BML_TrafoDefinition), value.Name.c_str()};
    const auto *api = Detail::Interface();
    return BML_IFACE_HAS(api, BML_GameplayInterface, RegisterTrafo) ? api->RegisterTrafo(ctx.Handle(), &record) : BML_ERROR_UNAVAILABLE;
}
struct ModuleDefinition {
    std::string Name;
};
[[nodiscard]] inline int RegisterModule(const Mod::Context &ctx, const ModuleDefinition &value) {
    if (!BML::Detail::IsText(value.Name)) return BML_ERROR_INVALID_PARAMETER;
    const BML_ModuleDefinition record{sizeof(BML_ModuleDefinition), value.Name.c_str()};
    const auto *api = Detail::Interface();
    return BML_IFACE_HAS(api, BML_GameplayInterface, RegisterModule) ? api->RegisterModule(ctx.Handle(), &record) : BML_ERROR_UNAVAILABLE;
}
using State = BML_GameplayState;
[[nodiscard]] inline int ReadState(State &out) {
    State value{}; value.StructSize = sizeof(value); const auto *api = Detail::Interface();
    if (!BML_IFACE_HAS(api, BML_GameplayInterface, ReadState)) return BML_ERROR_UNAVAILABLE;
    const int code = api->ReadState(&value); if (code == BML_OK) out = value; return code;
}
[[nodiscard]] inline int EnableCheat(const Mod::Context &ctx, bool enabled) {
    const auto *api = Detail::Interface(); return BML_IFACE_HAS(api, BML_GameplayInterface, EnableCheat) ? api->EnableCheat(ctx.Handle(), enabled ? 1u : 0u) : BML_ERROR_UNAVAILABLE;
}
[[nodiscard]] inline int ExitGame(const Mod::Context &ctx) {
    const auto *api = Detail::Interface(); return BML_IFACE_HAS(api, BML_GameplayInterface, ExitGame) ? api->ExitGame(ctx.Handle()) : BML_ERROR_UNAVAILABLE;
}
} // namespace BML::Gameplay

#endif // __cplusplus

#endif // BML_GAMEPLAY_H
