#ifndef BML_GAMEPLAY_H
#define BML_GAMEPLAY_H
#include "BML/Generated/bml_gameplay_api.h"
#include "BML/Generated/bml_gameplay_imc.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
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
namespace LegacyApi = Interop::Generated::Bml::Gameplay;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
inline int RequireApi() { const int status = LegacyApi::Require(); return status == BML_OK ? ClientState().EnsureOpen() : status; }
inline int Slice(std::size_t &offset, bool open, std::uint32_t limit, const auto &source, auto &out, bool &complete) {
    if (!open) return BML_ERROR_INVALID_HANDLE; if (limit == 0) return BML_ERROR_INVALID_PARAMETER;
    const std::size_t count = (std::min)(source.size() - offset, static_cast<std::size_t>(limit));
    const std::size_t end = offset + count;
    try { out.assign(source.begin() + static_cast<std::ptrdiff_t>(offset), source.begin() + static_cast<std::ptrdiff_t>(end)); }
    catch (const std::bad_alloc &) { return BML_ERROR_OUT_OF_MEMORY; }
    offset = end; complete = offset == source.size(); return BML_OK;
}
}
inline int RequireApi() { return Detail::RequireApi(); }
inline int ReadLevel(LevelState &out) { Detail::Api::LevelStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().ReadLevel(wire); if (status == BML_OK) out = {wire.Id, wire.ActiveBall, wire.ResetMatrix, wire.Points}; return status; }
inline int ReadEnergy(EnergyState &out) { Detail::Api::EnergyStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().ReadEnergy(wire); if (status == BML_OK) out = {wire.Points, wire.Lives, wire.StartPoints, wire.StartLives, wire.TimeFactor, wire.LifeBonus}; return status; }
inline int ReadCatalog(std::vector<CatalogEntry> &out) {
    std::vector<Detail::Api::CatalogEntryValue> wire; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().ReadCatalog(wire); if (status != BML_OK) return status;
    std::vector<CatalogEntry> values; try { values.reserve(wire.size()); for (auto &item : wire) values.push_back({std::move(item.File), std::move(item.StartBall), std::move(item.Sky), item.Bonus, item.Music}); } catch (const std::bad_alloc &) { return BML_ERROR_OUT_OF_MEMORY; }
    out = std::move(values); return BML_OK;
}
inline int ReadCheckpoints(std::vector<Checkpoint> &out) {
    std::vector<Detail::Api::CheckpointValue> wire; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().ReadCheckpoints(wire); if (status != BML_OK) return status;
    std::vector<Checkpoint> values; try { values.reserve(wire.size()); for (const auto &item : wire) values.push_back({item.Matrix, item.Object}); } catch (const std::bad_alloc &) { return BML_ERROR_OUT_OF_MEMORY; }
    out = std::move(values); return BML_OK;
}
inline int ReadResetpoints(std::vector<Resetpoint> &out) {
    std::vector<Detail::Api::ResetpointValue> wire; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().ReadResetpoints(wire); if (status != BML_OK) return status;
    std::vector<Resetpoint> values; try { values.reserve(wire.size()); for (const auto &item : wire) values.push_back({item.Object}); } catch (const std::bad_alloc &) { return BML_ERROR_OUT_OF_MEMORY; }
    out = std::move(values); return BML_OK;
}
class Catalog final {
public:
    int Open() { std::vector<CatalogEntry> values; const int status = ReadCatalog(values); if (status == BML_OK) { m_Values = std::move(values); m_Offset = 0; m_Open = true; } return status; }
    bool IsOpen() const { return m_Open; }
    int Close() { m_Values.clear(); m_Offset = 0; m_Open = false; return BML_OK; }
    int Next(std::uint32_t limit, std::vector<CatalogEntry> &out, bool &complete) { return Detail::Slice(m_Offset, m_Open, limit, m_Values, out, complete); }
private: std::vector<CatalogEntry> m_Values; std::size_t m_Offset = 0; bool m_Open = false;
};
class Checkpoints final {
public:
    int Open() { std::vector<Checkpoint> values; const int status = ReadCheckpoints(values); if (status == BML_OK) { m_Values = std::move(values); m_Offset = 0; m_Open = true; } return status; }
    bool IsOpen() const { return m_Open; }
    int Close() { m_Values.clear(); m_Offset = 0; m_Open = false; return BML_OK; }
    int Next(std::uint32_t limit, std::vector<Checkpoint> &out, bool &complete) { return Detail::Slice(m_Offset, m_Open, limit, m_Values, out, complete); }
private: std::vector<Checkpoint> m_Values; std::size_t m_Offset = 0; bool m_Open = false;
};
class Resetpoints final {
public:
    int Open() { std::vector<Resetpoint> values; const int status = ReadResetpoints(values); if (status == BML_OK) { m_Values = std::move(values); m_Offset = 0; m_Open = true; } return status; }
    bool IsOpen() const { return m_Open; }
    int Close() { m_Values.clear(); m_Offset = 0; m_Open = false; return BML_OK; }
    int Next(std::uint32_t limit, std::vector<Resetpoint> &out, bool &complete) { return Detail::Slice(m_Offset, m_Open, limit, m_Values, out, complete); }
private: std::vector<Resetpoint> m_Values; std::size_t m_Offset = 0; bool m_Open = false;
};
} // namespace BML::Gameplay
#endif // BML_GAMEPLAY_H
