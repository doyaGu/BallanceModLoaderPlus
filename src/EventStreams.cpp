#include "EventStreams.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <vector>

/* One open queue.  Every stream shares the published snapshot rather than
 * copying it, so a Mod that opens a second stream costs a pointer per event. */
struct BML_EventStream_T {
    struct Entry {
        std::shared_ptr<const BML::EventSnapshot> Snapshot;
        std::uint64_t Sequence = 0;
        std::uint64_t Timestamp = 0;
    };

    std::size_t Capacity = 0;
    std::uint64_t Dropped = 0;
    std::deque<Entry> Queue;
    Entry Current;
};

namespace BML {

namespace {

using Entry = BML_EventStream_T::Entry;

std::vector<std::unique_ptr<BML_EventStream_T>> &Streams() {
    static std::vector<std::unique_ptr<BML_EventStream_T>> streams;
    return streams;
}

std::uint64_t &NextSequence() {
    static std::uint64_t sequence = 1;
    return sequence;
}

std::uint64_t TimestampNs() noexcept {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

/* A Mod hands back a raw handle, so every entry point looks it up instead of
 * dereferencing it. */
BML_EventStream_T *Find(BML_EventStream stream) {
    if (!stream)
        return nullptr;
    for (const auto &candidate : Streams()) {
        if (candidate.get() == stream)
            return candidate.get();
    }
    return nullptr;
}

/* The event the stream is on, or null when it has not polled one yet or the one
 * it polled is not of the kind being asked about. */
template <typename Predicate>
const EventSnapshot *Current(BML_EventStream stream, Predicate carriesPayload) {
    BML_EventStream_T *self = Find(stream);
    if (!self || !self->Current.Snapshot)
        return nullptr;
    const EventSnapshot *snapshot = self->Current.Snapshot.get();
    return carriesPayload(snapshot->Kind) ? snapshot : nullptr;
}

int ClampCount(std::size_t count) {
    const auto limit = static_cast<std::size_t>((std::numeric_limits<int>::max)());
    return count > limit ? (std::numeric_limits<int>::max)() : static_cast<int>(count);
}

/* Length is the whole length, so text too long for the buffer is detectable
 * instead of silently short. */
void WriteText(BML_EventText &out, const std::string &text) {
    const std::size_t copied = (std::min)(text.size(), sizeof(out.Value) - 1u);
    if (copied != 0u)
        std::memcpy(out.Value, text.data(), copied);
    out.Value[copied] = 0;
    out.Length = ClampCount(text.size());
}

template <typename Value>
int ReadRow(const std::vector<Value> &values, std::size_t index, Value &out) {
    if (index >= values.size())
        return BML_ERROR_NOT_FOUND;
    out = values[index];
    return BML_OK;
}

/* Moves the stream on to its oldest queued event and hands that entry over. */
int Advance(BML_EventStream stream, Entry &out) {
    BML_EventStream_T *self = Find(stream);
    if (!self)
        return BML_ERROR_INVALID_HANDLE;
    if (self->Queue.empty())
        return BML_ERROR_NOT_FOUND;
    self->Current = std::move(self->Queue.front());
    self->Queue.pop_front();
    out = self->Current;
    return BML_OK;
}

bool IsConfigKind(int kind) { return kind == BML_EVENT_CONFIG_MODIFIED; }
bool IsCheatKind(int kind) { return kind == BML_EVENT_CHEAT_CHANGED; }

} // namespace

bool HasEventConsumers() noexcept {
    try {
        return !Streams().empty();
    } catch (...) {
        return false;
    }
}

void PublishEventSnapshot(const EventSnapshot &snapshot) noexcept {
    try {
        auto &streams = Streams();
        if (streams.empty())
            return;

        const auto shared = std::make_shared<const EventSnapshot>(snapshot);
        const std::uint64_t sequence = NextSequence()++;
        const std::uint64_t timestamp = TimestampNs();
        for (auto &stream : streams) {
            try {
                if (stream->Queue.size() >= stream->Capacity) {
                    stream->Queue.pop_front();
                    ++stream->Dropped;
                }
                stream->Queue.push_back(Entry{shared, sequence, timestamp});
            } catch (...) {
                ++stream->Dropped;
            }
        }
    } catch (...) {
        // The snapshot itself could not be shared, so no stream gets this event.
    }
}

void CloseAllEventStreams() noexcept {
    try {
        Streams().clear();
    } catch (...) {
    }
}

int OpenEventStream(int capacity, BML_EventStream &out) {
    out = nullptr;
    if (capacity < 0)
        return BML_ERROR_INVALID_PARAMETER;
    if (capacity == 0)
        capacity = BML_EVENT_DEFAULT_CAPACITY;

    auto stream = std::make_unique<BML_EventStream_T>();
    stream->Capacity = static_cast<std::size_t>(capacity);
    Streams().push_back(std::move(stream));
    out = Streams().back().get();
    return BML_OK;
}

int CloseEventStream(BML_EventStream stream) {
    auto &streams = Streams();
    const auto found = std::find_if(streams.begin(), streams.end(),
                                    [stream](const std::unique_ptr<BML_EventStream_T> &candidate) {
                                        return candidate.get() == stream;
                                    });
    if (found == streams.end())
        return BML_ERROR_INVALID_HANDLE;
    streams.erase(found);
    return BML_OK;
}

int ReadEventStreamDroppedCount(BML_EventStream stream, int &out) {
    const BML_EventStream_T *self = Find(stream);
    if (!self)
        return BML_ERROR_INVALID_HANDLE;
    const auto limit = static_cast<std::uint64_t>((std::numeric_limits<int>::max)());
    out = self->Dropped > limit ? (std::numeric_limits<int>::max)()
                                : static_cast<int>(self->Dropped);
    return BML_OK;
}

int PollEventStream(BML_EventStream stream, BML_EventInfo &out) {
    Entry entry;
    const int status = Advance(stream, entry);
    if (status != BML_OK)
        return status;
    out.Kind = entry.Snapshot->Kind;
    out.Sequence = entry.Sequence;
    out.Timestamp = entry.Timestamp;
    return BML_OK;
}

int ReadEventLoad(BML_EventStream stream, BML_EventLoad &out) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsLoadKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    WriteText(out.Filename, snapshot->Filename);
    WriteText(out.MasterName, snapshot->MasterName);
    out.IsMap = snapshot->IsMap ? 1 : 0;
    out.FilterClass = snapshot->FilterClass;
    out.AddToScene = snapshot->AddToScene ? 1 : 0;
    out.ReuseMeshes = snapshot->ReuseMeshes ? 1 : 0;
    out.ReuseMaterials = snapshot->ReuseMaterials ? 1 : 0;
    out.Dynamic = snapshot->IsDynamic ? 1 : 0;
    out.ObjectCount = ClampCount(snapshot->ObjectIds.size());
    out.MasterObject = snapshot->MasterObject;
    out.Script = snapshot->Script;
    return BML_OK;
}

int ReadEventLoadObject(BML_EventStream stream, std::size_t index, BML_ObjectRef &out) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsLoadKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    return ReadRow(snapshot->ObjectIds, index, out);
}

int ReadEventPhysics(BML_EventStream stream, BML_EventPhysics &out) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsPhysicsKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    out.Target = snapshot->Target;
    out.Fixed = snapshot->Fixed ? 1 : 0;
    out.Friction = snapshot->Friction;
    out.Elasticity = snapshot->Elasticity;
    out.Mass = snapshot->Mass;
    WriteText(out.CollisionGroup, snapshot->CollisionGroup);
    out.StartFrozen = snapshot->StartFrozen ? 1 : 0;
    out.EnableCollision = snapshot->EnableCollision ? 1 : 0;
    out.AutoCalculateMassCenter = snapshot->AutoCalculateMassCenter ? 1 : 0;
    out.LinearDamp = snapshot->LinearDamp;
    out.RotDamp = snapshot->RotDamp;
    WriteText(out.CollisionSurface, snapshot->CollisionSurface);
    out.MassCenter = snapshot->MassCenter;
    out.ConvexMeshCount = ClampCount(snapshot->ConvexMeshes.size());
    out.BallCount = ClampCount((std::min)(snapshot->BallCenters.size(), snapshot->BallRadii.size()));
    out.ConcaveMeshCount = ClampCount(snapshot->ConcaveMeshes.size());
    return BML_OK;
}

int ReadEventPhysicsConvexMesh(BML_EventStream stream, std::size_t index, BML_ObjectRef &out) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsPhysicsKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    return ReadRow(snapshot->ConvexMeshes, index, out);
}

int ReadEventPhysicsBall(BML_EventStream stream, std::size_t index, BML_Vec3 &outCenter,
                         float &outRadius) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsPhysicsKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    if (index >= snapshot->BallCenters.size() || index >= snapshot->BallRadii.size())
        return BML_ERROR_NOT_FOUND;
    outCenter = snapshot->BallCenters[index];
    outRadius = snapshot->BallRadii[index];
    return BML_OK;
}

int ReadEventPhysicsConcaveMesh(BML_EventStream stream, std::size_t index, BML_ObjectRef &out) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsPhysicsKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    return ReadRow(snapshot->ConcaveMeshes, index, out);
}

int ReadEventCommand(BML_EventStream stream, BML_EventCommand &out) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsCommandKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    WriteText(out.Name, snapshot->Command);
    out.ArgumentCount = ClampCount(snapshot->CommandArgs.size());
    return BML_OK;
}

int ReadEventCommandArgument(BML_EventStream stream, std::size_t index, BML_EventText &out) {
    const EventSnapshot *snapshot = Current(stream, Events::Detail::IsCommandKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    if (index >= snapshot->CommandArgs.size())
        return BML_ERROR_NOT_FOUND;
    WriteText(out, snapshot->CommandArgs[index]);
    return BML_OK;
}

int ReadEventConfig(BML_EventStream stream, BML_EventConfig &out) {
    const EventSnapshot *snapshot = Current(stream, IsConfigKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    WriteText(out.Category, snapshot->ConfigCategory);
    WriteText(out.Key, snapshot->ConfigKey);
    out.Type = snapshot->ConfigType;
    WriteText(out.Value, snapshot->ConfigValue);
    return BML_OK;
}

int ReadEventCheat(BML_EventStream stream, BML_EventCheat &out) {
    const EventSnapshot *snapshot = Current(stream, IsCheatKind);
    if (!snapshot)
        return BML_ERROR_NOT_FOUND;
    out.Enabled = snapshot->CheatEnabled ? 1 : 0;
    return BML_OK;
}

int PollEventStreamValue(BML_EventStream stream, Events::Event &out) {
    Entry entry;
    const int status = Advance(stream, entry);
    if (status != BML_OK)
        return status;

    const EventSnapshot &snapshot = *entry.Snapshot;
    Events::Event value{};
    value.Kind = snapshot.Kind;
    value.Sequence = entry.Sequence;
    value.Timestamp = entry.Timestamp;

    if (Events::Detail::IsLoadKind(snapshot.Kind)) {
        Events::Load load{};
        load.Filename = snapshot.Filename;
        load.IsMap = snapshot.IsMap;
        load.MasterName = snapshot.MasterName;
        load.FilterClass = snapshot.FilterClass;
        load.AddToScene = snapshot.AddToScene;
        load.ReuseMeshes = snapshot.ReuseMeshes;
        load.ReuseMaterials = snapshot.ReuseMaterials;
        load.Dynamic = snapshot.IsDynamic;
        load.Objects = snapshot.ObjectIds;
        load.MasterObject = snapshot.MasterObject;
        load.Script = snapshot.Script;
        value.LoadData = std::move(load);
    }

    if (Events::Detail::IsPhysicsKind(snapshot.Kind)) {
        Events::Physics physics{};
        physics.Target = snapshot.Target;
        physics.Fixed = snapshot.Fixed;
        physics.Friction = snapshot.Friction;
        physics.Elasticity = snapshot.Elasticity;
        physics.Mass = snapshot.Mass;
        physics.CollisionGroup = snapshot.CollisionGroup;
        physics.StartFrozen = snapshot.StartFrozen;
        physics.EnableCollision = snapshot.EnableCollision;
        physics.AutoCalculateMassCenter = snapshot.AutoCalculateMassCenter;
        physics.LinearDamp = snapshot.LinearDamp;
        physics.RotDamp = snapshot.RotDamp;
        physics.CollisionSurface = snapshot.CollisionSurface;
        physics.MassCenter = snapshot.MassCenter;
        physics.ConvexMeshes = snapshot.ConvexMeshes;
        physics.BallCenters = snapshot.BallCenters;
        physics.BallRadii = snapshot.BallRadii;
        physics.ConcaveMeshes = snapshot.ConcaveMeshes;
        value.PhysicsData = std::move(physics);
    }

    if (Events::Detail::IsCommandKind(snapshot.Kind))
        value.CommandData = Events::Command{snapshot.Command, snapshot.CommandArgs};

    if (IsConfigKind(snapshot.Kind))
        value.ConfigData = Events::Config{snapshot.ConfigCategory, snapshot.ConfigKey,
                                          snapshot.ConfigType, snapshot.ConfigValue};

    if (IsCheatKind(snapshot.Kind))
        value.CheatData = Events::Cheat{snapshot.CheatEnabled};

    out = std::move(value);
    return BML_OK;
}

} // namespace BML
