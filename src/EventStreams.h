#ifndef BML_EVENTSTREAMS_H
#define BML_EVENTSTREAMS_H

#include <cstddef>
#include <utility>

#include "BML/Events.h"

#include "EventSnapshot.h"

namespace BML {

/* The loader's own event queues, one per stream a Mod opened.  The hooks push a
 * snapshot in, the bml.events interface in Events.h reads it back out, and
 * nothing here needs the ModContext: the thread rule and the mods-loaded rule
 * are the interface thunks' job. */

/* Whether anything is listening.  The hooks ask first so that no snapshot is
 * built while no stream is open, which is the usual case. */
bool HasEventConsumers() noexcept;

/* Copies the snapshot into every open stream, dropping the oldest event in a
 * stream that is already full and counting that against it.  Never throws: an
 * event nobody could queue is dropped rather than reported. */
void PublishEventSnapshot(const EventSnapshot &snapshot) noexcept;

/* Event telemetry is strictly observational.  Construct the potentially
 * allocating snapshot inside this boundary so an out-of-memory or a queue
 * failure never changes the outcome of an original game hook. */
template <typename Capture>
void CaptureEventNoexcept(Capture &&capture) noexcept {
    try {
        if (!HasEventConsumers())
            return;
        EventSnapshot snapshot;
        std::forward<Capture>(capture)(snapshot);
        PublishEventSnapshot(snapshot);
    } catch (...) {
        // Observability must never escape into the original game callback.
    }
}

/* Drops every stream still open, so a handle a Mod forgot to close answers
 * BML_ERROR_INVALID_HANDLE afterwards instead of naming freed memory. */
void CloseAllEventStreams() noexcept;

/* The bml.events interface answers out of these.  A handle is looked up rather
 * than trusted, so a stale or forged one is BML_ERROR_INVALID_HANDLE. */
int OpenEventStream(int capacity, BML_EventStream &out);
int CloseEventStream(BML_EventStream stream);
int ReadEventStreamDroppedCount(BML_EventStream stream, int &out);
int PollEventStream(BML_EventStream stream, BML_EventInfo &out);
int ReadEventLoad(BML_EventStream stream, BML_EventLoad &out);
int ReadEventLoadObject(BML_EventStream stream, std::size_t index, BML_ObjectRef &out);
int ReadEventPhysics(BML_EventStream stream, BML_EventPhysics &out);
int ReadEventPhysicsConvexMesh(BML_EventStream stream, std::size_t index, BML_ObjectRef &out);
int ReadEventPhysicsBall(BML_EventStream stream, std::size_t index, BML_Vec3 &outCenter,
                         float &outRadius);
int ReadEventPhysicsConcaveMesh(BML_EventStream stream, std::size_t index, BML_ObjectRef &out);
int ReadEventCommand(BML_EventStream stream, BML_EventCommand &out);
int ReadEventCommandArgument(BML_EventStream stream, std::size_t index, BML_EventText &out);
int ReadEventConfig(BML_EventStream stream, BML_EventConfig &out);
int ReadEventCheat(BML_EventStream stream, BML_EventCheat &out);

/* The script bindings live inside the loader, so they take the queued event as
 * the whole C++ value in one step instead of reading it back field by field.
 * Polls the same cursor the reads above answer out of. */
int PollEventStreamValue(BML_EventStream stream, Events::Event &out);

} // namespace BML

#endif // BML_EVENTSTREAMS_H
