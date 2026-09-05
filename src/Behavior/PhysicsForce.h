#ifndef BML_BEHAVIOR_PHYSICSFORCE_H
#define BML_BEHAVIOR_PHYSICSFORCE_H

#include <list>
#include <optional>
#include <unordered_map>

#include "Behavior/Blocks.h"
#include "Behavior/Runtime.h"

namespace BML::Behavior::PhysicsForce {

using Options = Blocks::PhysicsForce::Options;

// Physics Force stores its native handle in a local parameter, so Create and
// Shutdown must run on the same configured instance for each target.
class Sessions final {
public:
    Sessions(CKContext *context, Runtime &runtime)
        : m_Context(context), m_Runtime(runtime) {}

    RunResult Set(const Options &options);
    RunResult Clear(CK3dEntity *target);
    // Must run immediately after the physics post-process seam. Physics Force
    // asks the physics callback container to create its controller immediately;
    // only a target without a PhysicsObject leaves that callback pending for a
    // later simulation. A pending callback must observe cancellation before the
    // CKBehavior can be retired.
    void ProcessFrame();
    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    void Reset();

private:
    struct EntityStamp {
        CK_ID Id = 0;
        CK3dEntity *Address = nullptr;
    };

    struct EntityKey {
        CK_ID Id = 0;
        CK3dEntity *Address = nullptr;

        bool operator==(const EntityKey &other) const noexcept {
            return Id == other.Id && Address == other.Address;
        }
    };

    struct EntityKeyHash {
        std::size_t operator()(const EntityKey &key) const noexcept {
            return std::hash<CK3dEntity *>{}(key.Address) ^
                   (static_cast<std::size_t>(key.Id) * 0x9e3779b1u);
        }
    };

    struct StoredOptions {
        EntityStamp Target;
        VxVector Position{0.0f, 0.0f, 0.0f};
        EntityStamp PositionReference;
        bool HasPositionReference = false;
        VxVector Direction{0.0f, 0.0f, 0.0f};
        EntityStamp DirectionReference;
        bool HasDirectionReference = false;
        float Magnitude = 0.0f;
    };

    struct Session {
        EntityStamp Target;
        Instance Block;
        bool Stopping = false;
        bool ShutdownQueued = false;
        std::uint64_t RetireAfterFrame = 0;
        std::optional<StoredOptions> Replacement;
    };

    [[nodiscard]] StoredOptions Capture(const Options &options) const;
    [[nodiscard]] bool Resolve(const StoredOptions &stored, Options &options) const;
    [[nodiscard]] EntityStamp Capture(CK3dEntity *entity) const;
    [[nodiscard]] CK3dEntity *Resolve(EntityStamp entity) const;
    [[nodiscard]] static EntityKey KeyOf(EntityStamp reference);
    [[nodiscard]] bool HasNativeController(Session &session) const;
    RunResult Create(const StoredOptions &stored);
    static RunResult Pending(const char *message);

    CKContext *m_Context = nullptr;
    Runtime &m_Runtime;
    std::uint64_t m_PhysicsFrame = 0;
    std::unordered_map<EntityKey, Session, EntityKeyHash> m_Sessions;
    std::list<Session> m_Retiring;
};

} // namespace BML::Behavior::PhysicsForce

#endif // BML_BEHAVIOR_PHYSICSFORCE_H
