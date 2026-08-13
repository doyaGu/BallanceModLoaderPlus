// Receives loader events, as an alternative to overriding the IMessageReceiver
// callbacks. The interface struct below is what the loader hands out and what a
// Mod written in C uses directly; the inline C++ namespace under it folds the
// lookup, the handle, and the per-kind reads into one Poll that hands back a
// value, so including this header still costs nothing at link time.
//
// The two routes carry the same events. IMessageReceiver hands you one virtual
// call per event kind and runs inside the loader's own dispatch, so it can only
// be used by a class that derives from IMod. A stream hands you one queue of
// tagged records you drain yourself, which suits code that is not an IMod
// subclass, code that wants to treat every kind uniformly, and code that wants to
// buffer events rather than react inside the loader's call.
//
// The loader owns the queue. Opening a stream tells it to start filling one: it
// pushes every event into every open stream as the event happens, drops the
// oldest event in a stream that is already full, and counts what it dropped. So
// an event published during a frame is pollable for the rest of that frame, and a
// Mod that stops polling costs one bounded queue rather than growing without end.
//
// Threading: the queues carry no locks, so every function here answers
// BML_ERROR_WRONG_THREAD off the game thread. Open a stream, poll it, and close
// it from the game thread only.
//
// Reading one event takes two steps. Poll moves the stream on to the next event
// and says which kind it is; the Read functions then describe the event the
// stream is sitting on. The next Poll replaces that event, so read what is needed
// before polling again. Asking for a payload the current event does not carry, and
// asking for a row past the end of one of its lists, both answer
// BML_ERROR_NOT_FOUND.
//
// Interface.h explains the header, the version rules, BML_IFACE_HAS, and how text
// is written into a fixed-capacity buffer.
#ifndef BML_EVENTS_H
#define BML_EVENTS_H

#include "BML/EventKinds.h"
#include "BML/Interface.h"
#include "BML/Types.h"

BML_BEGIN_CDECLS

#define BML_EVENTS_INTERFACE_ID "bml.events"
#define BML_EVENTS_INTERFACE_MAJOR 1
#define BML_EVENTS_INTERFACE_MINOR 0

// Capacity of every text buffer below, terminator included, and the number of
// undrained events a stream opened with capacity 0 keeps.
#define BML_EVENT_TEXT_CAPACITY 512u
#define BML_EVENT_DEFAULT_CAPACITY 256

// One open queue. The loader owns what is behind it; the handle is dead once
// CloseStream has taken it, and using it afterwards is BML_ERROR_INVALID_HANDLE
// rather than undefined.
typedef struct BML_EventStream_T *BML_EventStream;

// One piece of text out of an event. Value is always terminated and holds at most
// BML_EVENT_TEXT_CAPACITY - 1 characters; Length is how long the text actually is,
// so a longer one is detectable rather than silently short.
typedef struct BML_EventText {
    char Value[BML_EVENT_TEXT_CAPACITY];
    int Length;
} BML_EventText;

// What Poll says about the event the stream has moved on to. Kind is one of the
// BML_EVENT_* values in EventKinds.h and decides which Read functions answer.
// Timestamp is in nanoseconds. Sequence counts the events the loader has
// published, so a gap between two polled events is exactly what the stream
// dropped, which ReadDroppedCount totals.
typedef struct BML_EventInfo {
    int Kind;
    uint64_t Sequence;
    uint64_t Timestamp;
} BML_EventInfo;

// Carried by BML_EVENT_LOAD_OBJECT and BML_EVENT_LOAD_SCRIPT. ObjectCount and
// MasterObject are filled for the object kind, Script for the script kind; the
// other one stays empty. The loaded objects are read with ReadLoadObject.
//
// These are C structs and have no default member initializers: zero them with
// BML_EventLoad load = {0} in C.
typedef struct BML_EventLoad {
    BML_EventText Filename;
    BML_EventText MasterName;
    int IsMap;
    int FilterClass;
    int AddToScene;
    int ReuseMeshes;
    int ReuseMaterials;
    int Dynamic;
    int ObjectCount;
    BML_ObjectRef MasterObject;
    BML_ObjectRef Script;
} BML_EventLoad;

// Carried by BML_EVENT_PHYSICALIZE and BML_EVENT_UNPHYSICALIZE. The unphysicalize
// event only has a target, so every member after Target keeps its zero value
// there. Check the event kind before reading them. The three lists are read with
// ReadPhysicsConvexMesh, ReadPhysicsBall, and ReadPhysicsConcaveMesh; a ball is
// one center and one radius, which is why they share a count.
typedef struct BML_EventPhysics {
    BML_ObjectRef Target;
    int Fixed;
    float Friction;
    float Elasticity;
    float Mass;
    BML_EventText CollisionGroup;
    int StartFrozen;
    int EnableCollision;
    int AutoCalculateMassCenter;
    float LinearDamp;
    float RotDamp;
    BML_EventText CollisionSurface;
    BML_Vec3 MassCenter;
    int ConvexMeshCount;
    int BallCount;
    int ConcaveMeshCount;
} BML_EventPhysics;

// Carried by BML_EVENT_COMMAND_PRE and BML_EVENT_COMMAND_POST. The arguments
// exclude the command name itself and are read with ReadCommandArgument.
typedef struct BML_EventCommand {
    BML_EventText Name;
    int ArgumentCount;
} BML_EventCommand;

// Carried by BML_EVENT_CONFIG_MODIFIED. Value is the new value rendered as text
// whatever the declared Type is.
typedef struct BML_EventConfig {
    BML_EventText Category;
    BML_EventText Key;
    int Type;
    BML_EventText Value;
} BML_EventConfig;

// Carried by BML_EVENT_CHEAT_CHANGED.
typedef struct BML_EventCheat {
    int Enabled;
} BML_EventCheat;

typedef struct BML_EventsInterface {
    BML_InterfaceHeader Header;

    // capacity is how many undrained events the queue keeps. Passing 0 means
    // BML_EVENT_DEFAULT_CAPACITY; a negative value answers
    // BML_ERROR_INVALID_PARAMETER.
    int (*OpenStream)(int capacity, BML_EventStream *out);
    // Discards anything still queued. A handle already closed answers
    // BML_ERROR_INVALID_HANDLE.
    int (*CloseStream)(BML_EventStream stream);

    // Events lost since the stream was opened, because it was full when they
    // were published. Saturates at INT_MAX. A nonzero count means the stream is
    // being polled too slowly.
    int (*ReadDroppedCount)(BML_EventStream stream, int *out);

    // Moves on to the oldest queued event. Answers BML_ERROR_NOT_FOUND when the
    // queue is empty, which is the ordinary end of a drain loop rather than a
    // failure, so do not log it; the event the stream was on stays current.
    int (*Poll)(BML_EventStream stream, BML_EventInfo *out);

    int (*ReadLoad)(BML_EventStream stream, BML_EventLoad *out);
    int (*ReadLoadObject)(BML_EventStream stream, size_t index, BML_ObjectRef *out);

    int (*ReadPhysics)(BML_EventStream stream, BML_EventPhysics *out);
    int (*ReadPhysicsConvexMesh)(BML_EventStream stream, size_t index, BML_ObjectRef *out);
    int (*ReadPhysicsBall)(BML_EventStream stream, size_t index, BML_Vec3 *outCenter,
                           float *outRadius);
    int (*ReadPhysicsConcaveMesh)(BML_EventStream stream, size_t index, BML_ObjectRef *out);

    int (*ReadCommand)(BML_EventStream stream, BML_EventCommand *out);
    int (*ReadCommandArgument)(BML_EventStream stream, size_t index, BML_EventText *out);

    int (*ReadConfig)(BML_EventStream stream, BML_EventConfig *out);
    int (*ReadCheat)(BML_EventStream stream, BML_EventCheat *out);
} BML_EventsInterface;

BML_END_CDECLS

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace BML::Events {

using ObjectRef = BML_ObjectRef;
using Vec3 = BML_Vec3;

// Carried by BML_EVENT_LOAD_OBJECT and BML_EVENT_LOAD_SCRIPT. Objects and
// MasterObject are filled for the object kind, Script for the script kind; the
// other one stays empty.
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

// Carried by BML_EVENT_PHYSICALIZE and BML_EVENT_UNPHYSICALIZE. The unphysicalize
// event only has a target, so every field after Target keeps its default value
// there. Check the event kind before reading them.
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
    // BallCenters and BallRadii are parallel and always the same length.
    std::vector<Vec3> BallCenters;
    std::vector<float> BallRadii;
    std::vector<ObjectRef> ConcaveMeshes;
};

// Carried by BML_EVENT_COMMAND_PRE and BML_EVENT_COMMAND_POST. Arguments excludes
// the command name itself.
struct Command {
    std::string Name;
    std::vector<std::string> Arguments;
};

// Carried by BML_EVENT_CONFIG_MODIFIED. Value is the new value rendered as text
// whatever the declared Type is.
struct Config {
    std::string Category;
    std::string Key;
    int Type = -1;
    std::string Value;
};

// Carried by BML_EVENT_CHEAT_CHANGED.
struct Cheat {
    bool Enabled = false;
};

// Kind is one of the BML_EVENT_* values in EventKinds.h. Most kinds are bare
// notifications and carry none of the optional payloads. Timestamp is in
// nanoseconds, and Sequence is the loader's own event counter, so a gap between
// two polled events is exactly what this stream dropped.
struct Event {
    int Kind = 0;
    std::uint64_t Sequence = 0;
    std::uint64_t Timestamp = 0;
    std::optional<Load> LoadData;
    std::optional<Physics> PhysicsData;
    std::optional<Command> CommandData;
    std::optional<Config> ConfigData;
    std::optional<Cheat> CheatData;
    // True when at least one textual payload field did not fit in the C
    // interface buffer. The corresponding string still contains its prefix.
    bool TextTruncated = false;
};

namespace Detail {

// Looked up once per Mod. The loader's table is static, so a null answer means
// the running loader does not carry this interface at all, which is not something
// that can change later in the process.
inline const BML_EventsInterface *Interface() {
    static const BML_EventsInterface *found =
        FindInterface<BML_EventsInterface>(BML_EVENTS_INTERFACE_ID, BML_EVENTS_INTERFACE_MAJOR);
    return found;
}

inline bool IsLoadKind(int kind) {
    return kind == BML_EVENT_LOAD_OBJECT || kind == BML_EVENT_LOAD_SCRIPT;
}

inline bool IsPhysicsKind(int kind) {
    return kind == BML_EVENT_PHYSICALIZE || kind == BML_EVENT_UNPHYSICALIZE;
}

inline bool IsCommandKind(int kind) {
    return kind == BML_EVENT_COMMAND_PRE || kind == BML_EVENT_COMMAND_POST;
}

inline std::string ToString(const BML_EventText &text, bool &truncated) {
    truncated = truncated || text.Length >= static_cast<int>(BML_EVENT_TEXT_CAPACITY);
    return std::string(text.Value);
}

// Reads one list out of the event the stream is on. count comes from the payload
// struct that was just read, so a row that is suddenly missing is a fault rather
// than a race, and the whole event is rejected.
template <typename Value, typename Read>
int ReadList(int count, std::vector<Value> &out, Read read) {
    if (count <= 0)
        return BML_OK;
    out.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        Value value{};
        const int status = read(static_cast<std::size_t>(index), value);
        if (status != BML_OK)
            return status;
        out.push_back(value);
    }
    return BML_OK;
}

// Turns the event the stream is on into an Event. Every payload the kind carries
// has to be there, so a Read that answers anything but BML_OK rejects the whole
// event rather than handing over a half-filled one.
[[nodiscard]] inline int ReadEvent(const BML_EventsInterface *events, BML_EventStream stream,
                                   const BML_EventInfo &info, Event &out) {
    Event value{};
    value.Kind = info.Kind;
    value.Sequence = info.Sequence;
    value.Timestamp = info.Timestamp;

    if (IsLoadKind(info.Kind)) {
        if (!BML_IFACE_HAS(events, BML_EventsInterface, ReadLoad) ||
            !BML_IFACE_HAS(events, BML_EventsInterface, ReadLoadObject))
            return BML_ERROR_NOT_FOUND;

        BML_EventLoad raw = {};
        int status = events->ReadLoad(stream, &raw);
        if (status != BML_OK)
            return status;

        Load load{};
        load.Filename = ToString(raw.Filename, value.TextTruncated);
        load.IsMap = raw.IsMap != 0;
        load.MasterName = ToString(raw.MasterName, value.TextTruncated);
        load.FilterClass = raw.FilterClass;
        load.AddToScene = raw.AddToScene != 0;
        load.ReuseMeshes = raw.ReuseMeshes != 0;
        load.ReuseMaterials = raw.ReuseMaterials != 0;
        load.Dynamic = raw.Dynamic != 0;
        load.MasterObject = raw.MasterObject;
        load.Script = raw.Script;
        status = ReadList(raw.ObjectCount, load.Objects,
                          [events, stream](std::size_t index, ObjectRef &object) {
                              return events->ReadLoadObject(stream, index, &object);
                          });
        if (status != BML_OK)
            return status;
        value.LoadData = std::move(load);
    }

    if (IsPhysicsKind(info.Kind)) {
        if (!BML_IFACE_HAS(events, BML_EventsInterface, ReadPhysics) ||
            !BML_IFACE_HAS(events, BML_EventsInterface, ReadPhysicsConvexMesh) ||
            !BML_IFACE_HAS(events, BML_EventsInterface, ReadPhysicsBall) ||
            !BML_IFACE_HAS(events, BML_EventsInterface, ReadPhysicsConcaveMesh))
            return BML_ERROR_NOT_FOUND;

        BML_EventPhysics raw = {};
        int status = events->ReadPhysics(stream, &raw);
        if (status != BML_OK)
            return status;

        Physics physics{};
        physics.Target = raw.Target;
        physics.Fixed = raw.Fixed != 0;
        physics.Friction = raw.Friction;
        physics.Elasticity = raw.Elasticity;
        physics.Mass = raw.Mass;
        physics.CollisionGroup = ToString(raw.CollisionGroup, value.TextTruncated);
        physics.StartFrozen = raw.StartFrozen != 0;
        physics.EnableCollision = raw.EnableCollision != 0;
        physics.AutoCalculateMassCenter = raw.AutoCalculateMassCenter != 0;
        physics.LinearDamp = raw.LinearDamp;
        physics.RotDamp = raw.RotDamp;
        physics.CollisionSurface = ToString(raw.CollisionSurface, value.TextTruncated);
        physics.MassCenter = raw.MassCenter;

        status = ReadList(raw.ConvexMeshCount, physics.ConvexMeshes,
                          [events, stream](std::size_t index, ObjectRef &object) {
                              return events->ReadPhysicsConvexMesh(stream, index, &object);
                          });
        if (status != BML_OK)
            return status;
        physics.BallRadii.reserve(raw.BallCount > 0 ? static_cast<std::size_t>(raw.BallCount) : 0u);
        status = ReadList(raw.BallCount, physics.BallCenters,
                          [events, stream, &physics](std::size_t index, Vec3 &center) {
                              float radius = 0.0f;
                              const int rowStatus =
                                  events->ReadPhysicsBall(stream, index, &center, &radius);
                              if (rowStatus != BML_OK)
                                  return rowStatus;
                              physics.BallRadii.push_back(radius);
                              return BML_OK;
                          });
        if (status != BML_OK)
            return status;
        status = ReadList(raw.ConcaveMeshCount, physics.ConcaveMeshes,
                          [events, stream](std::size_t index, ObjectRef &object) {
                              return events->ReadPhysicsConcaveMesh(stream, index, &object);
                          });
        if (status != BML_OK)
            return status;
        value.PhysicsData = std::move(physics);
    }

    if (IsCommandKind(info.Kind)) {
        if (!BML_IFACE_HAS(events, BML_EventsInterface, ReadCommand) ||
            !BML_IFACE_HAS(events, BML_EventsInterface, ReadCommandArgument))
            return BML_ERROR_NOT_FOUND;

        BML_EventCommand raw = {};
        int status = events->ReadCommand(stream, &raw);
        if (status != BML_OK)
            return status;

        Command command{};
        command.Name = ToString(raw.Name, value.TextTruncated);
        status = ReadList(raw.ArgumentCount, command.Arguments,
                          [events, stream, &value](std::size_t index, std::string &argument) {
                              BML_EventText text = {};
                              const int rowStatus =
                                  events->ReadCommandArgument(stream, index, &text);
                              if (rowStatus != BML_OK)
                                  return rowStatus;
                              argument = ToString(text, value.TextTruncated);
                              return BML_OK;
                          });
        if (status != BML_OK)
            return status;
        value.CommandData = std::move(command);
    }

    if (info.Kind == BML_EVENT_CONFIG_MODIFIED) {
        if (!BML_IFACE_HAS(events, BML_EventsInterface, ReadConfig))
            return BML_ERROR_NOT_FOUND;
        BML_EventConfig raw = {};
        const int status = events->ReadConfig(stream, &raw);
        if (status != BML_OK)
            return status;
        Config config{};
        config.Category = ToString(raw.Category, value.TextTruncated);
        config.Key = ToString(raw.Key, value.TextTruncated);
        config.Type = raw.Type;
        config.Value = ToString(raw.Value, value.TextTruncated);
        value.ConfigData = std::move(config);
    }

    if (info.Kind == BML_EVENT_CHEAT_CHANGED) {
        if (!BML_IFACE_HAS(events, BML_EventsInterface, ReadCheat))
            return BML_ERROR_NOT_FOUND;
        BML_EventCheat raw = {};
        const int status = events->ReadCheat(stream, &raw);
        if (status != BML_OK)
            return status;
        value.CheatData = Cheat{raw.Enabled != 0};
    }

    out = std::move(value);
    return BML_OK;
}

} // namespace Detail

// Whether the running loader carries this interface. Stream checks for itself, so
// call this directly only to probe.
[[nodiscard]] inline int RequireApi() {
    return Detail::Interface() != nullptr ? BML_OK : BML_ERROR_NOT_FOUND;
}

// One queue of every event kind, drained with Poll. Not copyable, and not thread
// safe; see the threading note at the top of this file.
class Stream final {
public:
    Stream() = default;
    ~Stream() { (void)Close(); }

    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;

    // capacity is how many undrained events the loader keeps for this stream.
    // Passing 0 means BML_EVENT_DEFAULT_CAPACITY; a negative value is rejected
    // with BML_ERROR_INVALID_PARAMETER. Reopening an already open stream first
    // closes it, and returns the close status if that fails.
    [[nodiscard]] int Open(int capacity = BML_EVENT_DEFAULT_CAPACITY) {
        const int closeStatus = Close();
        if (IsOpen())
            return closeStatus;
        const BML_EventsInterface *events = Detail::Interface();
        if (!BML_IFACE_HAS(events, BML_EventsInterface, OpenStream))
            return BML_ERROR_NOT_FOUND;
        return events->OpenStream(capacity, &m_Handle);
    }

    bool IsOpen() const { return m_Handle != nullptr; }

    // Total events lost since Open, because the queue was full when they were
    // published. Saturates at INT_MAX. A nonzero count means the stream is being
    // polled too slowly.
    [[nodiscard]] int DroppedCount(int &out) const {
        const BML_EventsInterface *events = Detail::Interface();
        if (!BML_IFACE_HAS(events, BML_EventsInterface, ReadDroppedCount))
            return BML_ERROR_NOT_FOUND;
        return events->ReadDroppedCount(m_Handle, &out);
    }

    // Gives up the queue and discards anything still in it. Safe to call on a
    // stream that was never opened.
    [[nodiscard]] int Close() {
        if (!m_Handle)
            return BML_OK;
        const BML_EventsInterface *events = Detail::Interface();
        if (!BML_IFACE_HAS(events, BML_EventsInterface, CloseStream))
            return BML_ERROR_NOT_FOUND;
        const int status = events->CloseStream(m_Handle);
        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE)
            m_Handle = nullptr;
        return status;
    }

    // Takes the oldest queued event. Returns BML_ERROR_NOT_FOUND when the queue
    // is empty, which is the ordinary end of a drain loop rather than a failure,
    // so do not log it. BML_ERROR_INVALID_HANDLE means the stream is not open.
    [[nodiscard]] int Poll(Event &out) {
        out = {};
        const BML_EventsInterface *events = Detail::Interface();
        if (!BML_IFACE_HAS(events, BML_EventsInterface, Poll))
            return BML_ERROR_NOT_FOUND;
        BML_EventInfo info = {};
        const int status = events->Poll(m_Handle, &info);
        if (status != BML_OK)
            return status;
        try {
            return Detail::ReadEvent(events, m_Handle, info, out);
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

private:
    BML_EventStream m_Handle = nullptr;
};

} // namespace BML::Events

#endif // __cplusplus

#endif // BML_EVENTS_H
