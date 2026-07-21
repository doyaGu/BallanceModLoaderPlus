#ifndef BML_EVENTS_H
#define BML_EVENTS_H

#include "BML/Generated/bml_events_api.h"
#include "BML/EventKinds.h"
#include "BML/InteropClient.h"

#include <optional>
#include <new>
#include <string>
#include <utility>
#include <vector>

/*
 * Typed, shallow access to the built-in event API.  This is intentionally
 * a consumer facade rather than a second event ABI: every event remains one
 * immutable bml.events record delivered by Interop::Stream.
 */
namespace BML::Events {

using ObjectRef = Interop::ObjectRef;
using Vec3 = Interop::Vec3;

struct Load {
    std::string Filename;
    bool IsMap = false;
    std::string MasterName;
    int FilterClass = 0;
    bool AddToScene = false;
    bool ReuseMeshes = false;
    bool ReuseMaterials = false;
    bool Dynamic = false;
    std::vector<ObjectRef> Objects;
    ObjectRef MasterObject{};
    ObjectRef Script{};
};

struct Physics {
    ObjectRef Target{};
    bool Fixed = false;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool StartFrozen = false;
    bool EnableCollision = false;
    bool AutoCalculateMassCenter = false;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
    std::string CollisionSurface;
    Vec3 MassCenter{};
    std::vector<ObjectRef> ConvexMeshes;
    std::vector<Vec3> BallCenters;
    std::vector<float> BallRadii;
    std::vector<ObjectRef> ConcaveMeshes;
};

struct Command {
    std::string Name;
    std::vector<std::string> Arguments;
};

struct Config {
    std::string Category;
    std::string Key;
    int Type = -1;
    std::string Value;
};

struct Cheat {
    bool Enabled = false;
};

struct Event {
    int Kind = 0;
    uint64_t Sequence = 0;
    uint64_t Timestamp = 0;
    std::optional<Load> LoadData;
    std::optional<Physics> PhysicsData;
    std::optional<Command> CommandData;
    std::optional<Config> ConfigData;
    std::optional<Cheat> CheatData;
};

namespace Detail {

namespace EventFields = Interop::Generated::Bml::Events::EventField;

inline bool IsLoadKind(int kind) {
    return kind == BML_EVENT_LOAD_OBJECT || kind == BML_EVENT_LOAD_SCRIPT;
}

inline bool IsPhysicsKind(int kind) {
    return kind == BML_EVENT_PHYSICALIZE || kind == BML_EVENT_UNPHYSICALIZE;
}

inline bool IsCommandKind(int kind) {
    return kind == BML_EVENT_COMMAND_PRE || kind == BML_EVENT_COMMAND_POST;
}

template <typename T>
inline int CopyArray(const Interop::Record &record,
                     uint32_t field,
                     BML_INTEROP_FIELD_TYPE type,
                     std::vector<T> &out) {
    const void *data = nullptr;
    size_t count = 0;
    size_t elementSize = 0;
    const int status = record.Borrow(field, type, data, count, elementSize);
    if (status != BML_OK)
        return status;
    if (elementSize != sizeof(T) || (count != 0 && !data))
        return BML_ERROR_INTEROP_RECORD_INVALID;
    if (count == 0) {
        out.clear();
        return BML_OK;
    }
    const T *first = static_cast<const T *>(data);
    try {
        out.assign(first, first + count);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
    return BML_OK;
}

inline int CopyStrings(const Interop::Record &record, uint32_t field, std::vector<std::string> &out) {
    size_t count = 0;
    int status = record.StringArrayCount(field, count);
    if (status != BML_OK)
        return status;
    try {
        std::vector<std::string> values;
        values.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            std::string value;
            status = record.GetStringArrayItem(field, index, value);
            if (status != BML_OK)
                return status;
            values.push_back(std::move(value));
        }
        out = std::move(values);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

inline int DecodeLoad(const Interop::Record &record, int kind, Load &out) {
    Load value{};
    int status = record.GetString(EventFields::Filename, value.Filename);
    if (status == BML_OK) status = record.GetBool(EventFields::IsMap, value.IsMap);
    if (status == BML_OK) status = record.GetString(EventFields::MasterName, value.MasterName);
    if (status == BML_OK) status = record.GetInt(EventFields::FilterClass, value.FilterClass);
    if (status == BML_OK) status = record.GetBool(EventFields::AddToScene, value.AddToScene);
    if (status == BML_OK) status = record.GetBool(EventFields::ReuseMeshes, value.ReuseMeshes);
    if (status == BML_OK) status = record.GetBool(EventFields::ReuseMaterials, value.ReuseMaterials);
    if (status == BML_OK) status = record.GetBool(EventFields::Dynamic, value.Dynamic);
    if (status == BML_OK && kind == BML_EVENT_LOAD_OBJECT) {
        status = CopyArray(record, EventFields::ObjectIds, BML_INTEROP_FIELD_OBJECT_ARRAY, value.Objects);
        if (status == BML_OK) status = record.GetObjectRef(EventFields::MasterObject, value.MasterObject);
    }
    if (status == BML_OK && kind == BML_EVENT_LOAD_SCRIPT)
        status = record.GetObjectRef(EventFields::Script, value.Script);
    if (status == BML_OK) out = std::move(value);
    return status;
}

inline int DecodePhysics(const Interop::Record &record, int kind, Physics &out) {
    Physics value{};
    int status = record.GetObjectRef(EventFields::Target, value.Target);
    if (status == BML_OK && kind == BML_EVENT_PHYSICALIZE) {
        if (status == BML_OK) status = record.GetBool(EventFields::Fixed, value.Fixed);
        if (status == BML_OK) status = record.GetFloat(EventFields::Friction, value.Friction);
        if (status == BML_OK) status = record.GetFloat(EventFields::Elasticity, value.Elasticity);
        if (status == BML_OK) status = record.GetFloat(EventFields::Mass, value.Mass);
        if (status == BML_OK) status = record.GetString(EventFields::CollisionGroup, value.CollisionGroup);
        if (status == BML_OK) status = record.GetBool(EventFields::StartFrozen, value.StartFrozen);
        if (status == BML_OK) status = record.GetBool(EventFields::EnableCollision, value.EnableCollision);
        if (status == BML_OK) status = record.GetBool(EventFields::AutoCalculateMassCenter, value.AutoCalculateMassCenter);
        if (status == BML_OK) status = record.GetFloat(EventFields::LinearDamp, value.LinearDamp);
        if (status == BML_OK) status = record.GetFloat(EventFields::RotDamp, value.RotDamp);
        if (status == BML_OK) status = record.GetString(EventFields::CollisionSurface, value.CollisionSurface);
        if (status == BML_OK) status = record.GetVec3(EventFields::MassCenter, value.MassCenter);
        if (status == BML_OK) status = CopyArray(record, EventFields::ConvexMeshes, BML_INTEROP_FIELD_OBJECT_ARRAY, value.ConvexMeshes);
        if (status == BML_OK) status = CopyArray(record, EventFields::BallCenters, BML_INTEROP_FIELD_VEC3_ARRAY, value.BallCenters);
        if (status == BML_OK) status = CopyArray(record, EventFields::BallRadii, BML_INTEROP_FIELD_FLOAT_ARRAY, value.BallRadii);
        if (status == BML_OK) status = CopyArray(record, EventFields::ConcaveMeshes, BML_INTEROP_FIELD_OBJECT_ARRAY, value.ConcaveMeshes);
    }
    if (status == BML_OK) out = std::move(value);
    return status;
}

inline int DecodeCommand(const Interop::Record &record, Command &out) {
    Command value{};
    int status = record.GetString(EventFields::Command, value.Name);
    if (status == BML_OK) status = CopyStrings(record, EventFields::CommandArgs, value.Arguments);
    if (status == BML_OK) out = std::move(value);
    return status;
}

inline int DecodeConfig(const Interop::Record &record, Config &out) {
    Config value{};
    int status = record.GetString(EventFields::ConfigCategory, value.Category);
    if (status == BML_OK) status = record.GetString(EventFields::ConfigKey, value.Key);
    if (status == BML_OK) status = record.GetInt(EventFields::ConfigType, value.Type);
    if (status == BML_OK) status = record.GetString(EventFields::ConfigValue, value.Value);
    if (status == BML_OK) out = std::move(value);
    return status;
}

inline int Decode(const Interop::Record &record, Event &out) {
    Event value{};
    int status = record.GetInt(EventFields::Kind, value.Kind);
    if (status == BML_OK) status = record.Sequence(value.Sequence);
    if (status == BML_OK) status = record.Timestamp(value.Timestamp);
    if (status == BML_OK && IsLoadKind(value.Kind)) {
        Load payload{};
        status = DecodeLoad(record, value.Kind, payload);
        if (status == BML_OK) value.LoadData = std::move(payload);
    }
    if (status == BML_OK && IsPhysicsKind(value.Kind)) {
        Physics payload{};
        status = DecodePhysics(record, value.Kind, payload);
        if (status == BML_OK) value.PhysicsData = std::move(payload);
    }
    if (status == BML_OK && IsCommandKind(value.Kind)) {
        Command payload{};
        status = DecodeCommand(record, payload);
        if (status == BML_OK) value.CommandData = std::move(payload);
    }
    if (status == BML_OK && value.Kind == BML_EVENT_CONFIG_MODIFIED) {
        Config payload{};
        status = DecodeConfig(record, payload);
        if (status == BML_OK) value.ConfigData = std::move(payload);
    }
    if (status == BML_OK && value.Kind == BML_EVENT_CHEAT_CHANGED) {
        Cheat payload{};
            status = record.GetBool(EventFields::CheatEnabled, payload.Enabled);
        if (status == BML_OK) value.CheatData = payload;
    }
    if (status == BML_OK) out = std::move(value);
    return status;
}

} // namespace Detail

class Stream final {
public:
    int Open(int capacity = 256) {
        Close();
        const int status = Interop::RequireApi(Interop::Generated::Bml::Events::Descriptor);
        return status == BML_OK ? Interop::OpenStream(Interop::Generated::Bml::Events::ApiId, "all", capacity, m_Stream) : status;
    }

    bool IsOpen() const { return m_Stream.Valid(); }
    int DroppedCount(int &out) const { return m_Stream.DroppedCount(out); }
    int Close() { return m_Stream.Close(); }

    /* Returns BML_OK with an empty Event when the stream currently has no
     * record.  A non-empty Event always owns a fully decoded immutable copy. */
    int Poll(Event &out) const {
        out = {};
        Interop::Record record;
        int status = m_Stream.Poll(record);
        if (status != BML_OK || !record.Valid())
            return status;
        return Detail::Decode(record, out);
    }

private:
    Interop::Stream m_Stream;
};

} // namespace BML::Events

#endif // BML_EVENTS_H
