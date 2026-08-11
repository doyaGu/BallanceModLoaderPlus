// Reads Ballance gameplay state over IMC. This is a thin inline wrapper around
// the generated bml.gameplay client, so including it costs nothing at link time.
//
// Status handling, first-call client opening, and the thread rules are the same
// as in Runtime.h.
//
// Every function here is backed by one of the game's own CKDataArrays:
// CurrentLevel, Energy, AllLevel, Checkpoints, and ResetPoints. The loader checks
// the expected column names before reading, so all of these return
// BML_ERROR_IMC_UNSUPPORTED rather than garbage when the array is missing, which
// is the normal state outside a level, and when a custom level has changed the
// column layout. Treat that status as "not available right now" and retry later.
//
// Reads are snapshots taken at call time, not live views. Nothing here writes:
// changing gameplay state still goes through the Virtools arrays directly.
#ifndef BML_GAMEPLAY_H
#define BML_GAMEPLAY_H

#include "BML/Generated/bml_gameplay_imc.hpp"

#include <cstddef>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace BML::Gameplay {

using ObjectRef = BML_ObjectRef;
using Mat4 = BML_Mat4;

// From the CurrentLevel array. ActiveBall is null between levels. ResetMatrix is
// the Ball_Pos_Frame transform the game respawns the ball at. Points here is the
// level's own counter and is not the same column as EnergyState::Points.
struct LevelState {
    int Id = 0;
    ObjectRef ActiveBall{};
    Mat4 ResetMatrix{};
    int Points = 0;
};

// From the Energy array, in its column order: current points and lives, the
// values the level started with, the time factor, and the per-life bonus.
struct EnergyState {
    int Points = 0;
    int Lives = 0;
    int StartPoints = 0;
    int StartLives = 0;
    float TimeFactor = 0.0f;
    int LifeBonus = 0;
};

// One row of the AllLevel array. File is the level filename, StartBall the ball
// type it starts with, Sky the sky material, and Music the track index.
struct CatalogEntry {
    std::string File;
    std::string StartBall;
    std::string Sky;
    int Bonus = 0;
    int Music = 0;
};

struct Checkpoint {
    Mat4 Matrix{};
    ObjectRef Object{};
};

struct Resetpoint {
    ObjectRef Object{};
};

namespace Detail {

namespace Api = Imc::Generated::Bml::Gameplay;

inline Imc::LazyClient<Api::Client> &ClientState() {
    static Imc::LazyClient<Api::Client> state;
    return state;
}

inline Api::Client &Client() { return ClientState().Get(); }

[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }

} // namespace Detail

// Opens the client if it is not open yet. The functions below already do this,
// so call it directly only to probe whether the routes exist.
[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }

[[nodiscard]] inline int ReadLevel(LevelState &out) {
    Detail::Api::LevelStateValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallLevel(wire);
    if (status == BML_OK)
        out = {wire.Id, wire.ActiveBall, wire.ResetMatrix, wire.Points};
    return status;
}

[[nodiscard]] inline int ReadEnergy(EnergyState &out) {
    Detail::Api::EnergyStateValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallEnergy(wire);
    if (status == BML_OK)
        out = {wire.Points, wire.Lives, wire.StartPoints, wire.StartLives, wire.TimeFactor, wire.LifeBonus};
    return status;
}

// The three list readers replace the contents of out on success and leave it
// alone on failure. They arrive over the wire as parallel arrays, so a length
// mismatch between those arrays is reported as BML_ERROR_MALFORMED_MESSAGE.
[[nodiscard]] inline int ReadCatalog(std::vector<CatalogEntry> &out) {
    Detail::Api::CatalogResponseValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallCatalog(wire);
    if (status != BML_OK)
        return status;

    const std::size_t count = wire.Files.size();
    if (wire.StartBalls.size() != count || wire.Skies.size() != count ||
        wire.Bonuses.size() != count || wire.Music.size() != count)
        return BML_ERROR_MALFORMED_MESSAGE;

    std::vector<CatalogEntry> values;
    try {
        values.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            values.push_back({std::move(wire.Files[i]), std::move(wire.StartBalls[i]),
                              std::move(wire.Skies[i]), wire.Bonuses[i], wire.Music[i]});
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = std::move(values);
    return BML_OK;
}

[[nodiscard]] inline int ReadCheckpoints(std::vector<Checkpoint> &out) {
    Detail::Api::CheckpointsResponseValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallCheckpoints(wire);
    if (status != BML_OK)
        return status;

    if (wire.Matrices.size() != wire.Objects.size())
        return BML_ERROR_MALFORMED_MESSAGE;

    std::vector<Checkpoint> values;
    try {
        values.reserve(wire.Matrices.size());
        for (std::size_t i = 0; i < wire.Matrices.size(); ++i)
            values.push_back({wire.Matrices[i], wire.Objects[i]});
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = std::move(values);
    return BML_OK;
}

[[nodiscard]] inline int ReadResetpoints(std::vector<Resetpoint> &out) {
    Detail::Api::ResetpointsResponseValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallResetpoints(wire);
    if (status != BML_OK)
        return status;

    std::vector<Resetpoint> values;
    try {
        values.reserve(wire.Objects.size());
        for (const auto &object : wire.Objects)
            values.push_back({object});
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = std::move(values);
    return BML_OK;
}

} // namespace BML::Gameplay

#endif // BML_GAMEPLAY_H
