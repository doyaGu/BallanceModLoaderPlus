#ifndef BML_GAMEPLAY_H
#define BML_GAMEPLAY_H
#include "BML/Generated/bml_gameplay_imc.hpp"
#include <cstddef>
#include <new>
#include <string>
#include <utility>
#include <vector>
namespace BML::Gameplay {
using ObjectRef = BML_ObjectRef; using Mat4 = BML_Mat4;
struct LevelState { int Id = 0; ObjectRef ActiveBall{}; Mat4 ResetMatrix{}; int Points = 0; };
struct EnergyState { int Points = 0; int Lives = 0; int StartPoints = 0; int StartLives = 0; float TimeFactor = 0.0f; int LifeBonus = 0; };
struct CatalogEntry { std::string File; std::string StartBall; std::string Sky; int Bonus = 0; int Music = 0; };
struct Checkpoint { Mat4 Matrix{}; ObjectRef Object{}; };
struct Resetpoint { ObjectRef Object{}; };
namespace Detail {
namespace Api = Imc::Generated::Bml::Gameplay;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
inline int RequireApi() { return ClientState().EnsureOpen(); }
}
inline int RequireApi() { return Detail::RequireApi(); }
inline int ReadLevel(LevelState &out) { Detail::Api::LevelStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallLevel(wire); if (status == BML_OK) out = {wire.Id, wire.ActiveBall, wire.ResetMatrix, wire.Points}; return status; }
inline int ReadEnergy(EnergyState &out) { Detail::Api::EnergyStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallEnergy(wire); if (status == BML_OK) out = {wire.Points, wire.Lives, wire.StartPoints, wire.StartLives, wire.TimeFactor, wire.LifeBonus}; return status; }
inline int ReadCatalog(std::vector<CatalogEntry> &out) {
    Detail::Api::CatalogResponseValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallCatalog(wire); if (status != BML_OK) return status;
    const std::size_t count = wire.Files.size(); if (wire.StartBalls.size() != count || wire.Skies.size() != count || wire.Bonuses.size() != count || wire.Music.size() != count) return BML_ERROR_MALFORMED_MESSAGE;
    std::vector<CatalogEntry> values; try { values.reserve(count); for (std::size_t i = 0; i < count; ++i) values.push_back({std::move(wire.Files[i]), std::move(wire.StartBalls[i]), std::move(wire.Skies[i]), wire.Bonuses[i], wire.Music[i]}); } catch (const std::bad_alloc &) { return BML_ERROR_OUT_OF_MEMORY; }
    out = std::move(values); return BML_OK;
}
inline int ReadCheckpoints(std::vector<Checkpoint> &out) {
    Detail::Api::CheckpointsResponseValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallCheckpoints(wire); if (status != BML_OK) return status;
    if (wire.Matrices.size() != wire.Objects.size()) return BML_ERROR_MALFORMED_MESSAGE;
    std::vector<Checkpoint> values; try { values.reserve(wire.Matrices.size()); for (std::size_t i = 0; i < wire.Matrices.size(); ++i) values.push_back({wire.Matrices[i], wire.Objects[i]}); } catch (const std::bad_alloc &) { return BML_ERROR_OUT_OF_MEMORY; }
    out = std::move(values); return BML_OK;
}
inline int ReadResetpoints(std::vector<Resetpoint> &out) {
    Detail::Api::ResetpointsResponseValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallResetpoints(wire); if (status != BML_OK) return status;
    std::vector<Resetpoint> values; try { values.reserve(wire.Objects.size()); for (const auto &object : wire.Objects) values.push_back({object}); } catch (const std::bad_alloc &) { return BML_ERROR_OUT_OF_MEMORY; }
    out = std::move(values); return BML_OK;
}
} // namespace BML::Gameplay
#endif // BML_GAMEPLAY_H
