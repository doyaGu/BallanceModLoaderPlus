#ifndef BML_EVENTS_H
#define BML_EVENTS_H
#include "BML/Generated/bml_events_imc.hpp"
#include "BML/EventKinds.h"
#include <cstdint>
#include <deque>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace BML::Events {
using ObjectRef = BML_ObjectRef; using Vec3 = BML_Vec3;
struct Load { std::string Filename; bool IsMap = false; std::string MasterName; int FilterClass = 0; bool AddToScene = false; bool ReuseMeshes = false; bool ReuseMaterials = false; bool Dynamic = false; std::vector<ObjectRef> Objects; ObjectRef MasterObject{}; ObjectRef Script{}; };
struct Physics { ObjectRef Target{}; bool Fixed = false; float Friction = 0.0f; float Elasticity = 0.0f; float Mass = 0.0f; std::string CollisionGroup; bool StartFrozen = false; bool EnableCollision = false; bool AutoCalculateMassCenter = false; float LinearDamp = 0.0f; float RotDamp = 0.0f; std::string CollisionSurface; Vec3 MassCenter{}; std::vector<ObjectRef> ConvexMeshes; std::vector<Vec3> BallCenters; std::vector<float> BallRadii; std::vector<ObjectRef> ConcaveMeshes; };
struct Command { std::string Name; std::vector<std::string> Arguments; };
struct Config { std::string Category; std::string Key; int Type = -1; std::string Value; };
struct Cheat { bool Enabled = false; };
struct Event { int Kind = 0; std::uint64_t Sequence = 0; std::uint64_t Timestamp = 0; std::optional<Load> LoadData; std::optional<Physics> PhysicsData; std::optional<Command> CommandData; std::optional<Config> ConfigData; std::optional<Cheat> CheatData; };
namespace Detail {
namespace Api = Imc::Generated::Bml::Events;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
inline int RequireApi() { return ClientState().EnsureOpen(); }
inline bool IsLoadKind(int kind) { return kind == BML_EVENT_LOAD_OBJECT || kind == BML_EVENT_LOAD_SCRIPT; }
inline bool IsPhysicsKind(int kind) { return kind == BML_EVENT_PHYSICALIZE || kind == BML_EVENT_UNPHYSICALIZE; }
inline bool IsCommandKind(int kind) { return kind == BML_EVENT_COMMAND_PRE || kind == BML_EVENT_COMMAND_POST; }
inline int Decode(Api::EventValue &&wire, const BML_ImcMessage &message, Event &out) {
    Event value{}; value.Kind = wire.Kind; value.Sequence = message.MessageId; value.Timestamp = message.TimestampNs;
    if (IsLoadKind(value.Kind)) {
        if (!wire.HasFilename || !wire.HasIsMap || !wire.HasMasterName || !wire.HasFilterClass || !wire.HasAddToScene || !wire.HasReuseMeshes || !wire.HasReuseMaterials || !wire.HasDynamic) return BML_ERROR_MALFORMED_MESSAGE;
        Load load{}; load.Filename = std::move(wire.Filename); load.IsMap = wire.IsMap; load.MasterName = std::move(wire.MasterName); load.FilterClass = wire.FilterClass; load.AddToScene = wire.AddToScene; load.ReuseMeshes = wire.ReuseMeshes; load.ReuseMaterials = wire.ReuseMaterials; load.Dynamic = wire.Dynamic;
        if (value.Kind == BML_EVENT_LOAD_OBJECT) { if (!wire.HasObjectIds || !wire.HasMasterObject) return BML_ERROR_MALFORMED_MESSAGE; load.Objects = std::move(wire.ObjectIds); load.MasterObject = wire.MasterObject; }
        else { if (!wire.HasScript) return BML_ERROR_MALFORMED_MESSAGE; load.Script = wire.Script; }
        value.LoadData = std::move(load);
    }
    if (IsPhysicsKind(value.Kind)) {
        if (!wire.HasTarget) return BML_ERROR_MALFORMED_MESSAGE;
        Physics physics{}; physics.Target = wire.Target;
        if (value.Kind == BML_EVENT_PHYSICALIZE) {
            if (!wire.HasFixed || !wire.HasFriction || !wire.HasElasticity || !wire.HasMass || !wire.HasCollisionGroup || !wire.HasStartFrozen || !wire.HasEnableCollision || !wire.HasAutoCalculateMassCenter || !wire.HasLinearDamp || !wire.HasRotDamp || !wire.HasCollisionSurface || !wire.HasMassCenter || !wire.HasConvexMeshes || !wire.HasBallCenters || !wire.HasBallRadii || !wire.HasConcaveMeshes) return BML_ERROR_MALFORMED_MESSAGE;
            physics.Fixed = wire.Fixed; physics.Friction = wire.Friction; physics.Elasticity = wire.Elasticity; physics.Mass = wire.Mass; physics.CollisionGroup = std::move(wire.CollisionGroup); physics.StartFrozen = wire.StartFrozen; physics.EnableCollision = wire.EnableCollision; physics.AutoCalculateMassCenter = wire.AutoCalculateMassCenter; physics.LinearDamp = wire.LinearDamp; physics.RotDamp = wire.RotDamp; physics.CollisionSurface = std::move(wire.CollisionSurface); physics.MassCenter = wire.MassCenter; physics.ConvexMeshes = std::move(wire.ConvexMeshes); physics.BallCenters = std::move(wire.BallCenters); physics.BallRadii = std::move(wire.BallRadii); physics.ConcaveMeshes = std::move(wire.ConcaveMeshes);
        }
        value.PhysicsData = std::move(physics);
    }
    if (IsCommandKind(value.Kind)) { if (!wire.HasCommand || !wire.HasCommandArgs) return BML_ERROR_MALFORMED_MESSAGE; value.CommandData = Command{std::move(wire.Command), std::move(wire.CommandArgs)}; }
    if (value.Kind == BML_EVENT_CONFIG_MODIFIED) { if (!wire.HasConfigCategory || !wire.HasConfigKey || !wire.HasConfigType || !wire.HasConfigValue) return BML_ERROR_MALFORMED_MESSAGE; value.ConfigData = Config{std::move(wire.ConfigCategory), std::move(wire.ConfigKey), wire.ConfigType, std::move(wire.ConfigValue)}; }
    if (value.Kind == BML_EVENT_CHEAT_CHANGED) { if (!wire.HasCheatEnabled) return BML_ERROR_MALFORMED_MESSAGE; value.CheatData = Cheat{wire.CheatEnabled}; }
    out = std::move(value); return BML_OK;
}
} // namespace Detail
class Stream final {
public:
    int Open(int capacity = 256) {
        const int closeStatus = Close(); if (IsOpen()) return closeStatus;
        if (capacity < 0) return BML_ERROR_INVALID_PARAMETER; if (capacity == 0) capacity = 256;
        int status = Detail::RequireApi(); if (status != BML_OK) return status;
        m_Capacity = static_cast<std::size_t>(capacity); m_LocalDropped = 0; m_PendingError = BML_OK;
        return Detail::Client().SubscribeAll(m_Subscription, &OnEvent, this, static_cast<std::uint32_t>(capacity));
    }
    bool IsOpen() const { return m_Subscription.IsOpen(); }
    int DroppedCount(int &out) const {
        std::uint64_t runtime = 0; const int status = m_Subscription.DroppedCount(runtime); if (status != BML_OK) return status;
        const std::uint64_t total = runtime + m_LocalDropped; out = total > static_cast<std::uint64_t>((std::numeric_limits<int>::max)()) ? (std::numeric_limits<int>::max)() : static_cast<int>(total); return BML_OK;
    }
    int Close() { const int status = m_Subscription.Close(); if (!m_Subscription.IsOpen()) { m_Queue.clear(); m_Capacity = 0; m_LocalDropped = 0; m_PendingError = BML_OK; } return status; }
    int Poll(Event &out) const {
        out = {}; if (m_PendingError != BML_OK) { const int status = m_PendingError; m_PendingError = BML_OK; return status; }
        if (m_Queue.empty()) return BML_OK; out = std::move(m_Queue.front()); m_Queue.pop_front(); return BML_OK;
    }
private:
    static void OnEvent(int status, Detail::Api::EventValue *wire, const BML_ImcMessage *message, void *userdata) noexcept {
        auto *self = static_cast<Stream *>(userdata); if (!self) return;
        if (status != BML_OK || !wire || !message) { self->m_PendingError = status == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : status; return; }
        try {
            Event event{}; status = Detail::Decode(std::move(*wire), *message, event); if (status != BML_OK) { self->m_PendingError = status; return; }
            if (self->m_Queue.size() >= self->m_Capacity) { self->m_Queue.pop_front(); ++self->m_LocalDropped; }
            self->m_Queue.push_back(std::move(event));
        } catch (const std::bad_alloc &) { self->m_PendingError = BML_ERROR_OUT_OF_MEMORY; } catch (...) { self->m_PendingError = BML_ERROR_FAIL; }
    }
    Imc::Generated::Bml::Events::AllSubscription m_Subscription;
    mutable std::deque<Event> m_Queue; std::size_t m_Capacity = 0; mutable std::uint64_t m_LocalDropped = 0; mutable int m_PendingError = BML_OK;
};
} // namespace BML::Events
#endif // BML_EVENTS_H
