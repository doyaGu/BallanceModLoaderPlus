#ifndef BML_BEHAVIOR_PHYSICSFORCE_H
#define BML_BEHAVIOR_PHYSICSFORCE_H

#include <list>
#include <unordered_map>

#include "Behavior/Runtime.h"

namespace BML::Behavior::PhysicsForce {

struct Options {
    CK3dEntity *Target = nullptr;
    VxVector Position{0.0f, 0.0f, 0.0f};
    CK3dEntity *PositionReference = nullptr;
    VxVector Direction{0.0f, 0.0f, 0.0f};
    CK3dEntity *DirectionReference = nullptr;
    float Magnitude = 0.0f;
};

Spec Make(const Options &options);

// Physics Force stores its native handle in a local parameter, so Create and
// Shutdown must run on the same configured instance for each target.
class Sessions final {
public:
    Sessions(CKContext *context, Runtime &runtime)
        : m_Context(context), m_Runtime(runtime) {}

    RunResult Set(const Options &options);
    RunResult Clear(CK3dEntity *target);
    // Must run immediately after the physics post-process seam. Physics Force
    // creates its native controller from a queued pre-sim callback, so a close
    // request is not safe in the same physics epoch as Create.
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
        bool Closing = false;
        bool CancellationArmed = false;
        std::uint64_t CloseAfterEpoch = 0;
    };

    [[nodiscard]] StoredOptions Capture(const Options &options) const;
    [[nodiscard]] bool Resolve(const StoredOptions &stored, Options &options) const;
    [[nodiscard]] EntityStamp Capture(CK3dEntity *entity) const;
    [[nodiscard]] CK3dEntity *Resolve(EntityStamp entity) const;
    [[nodiscard]] static EntityKey KeyOf(EntityStamp reference);
    [[nodiscard]] bool HasNativeController(Session &session) const;
    RunResult Create(const StoredOptions &stored);
    static RunResult Accepted(const char *message);

    CKContext *m_Context = nullptr;
    Runtime &m_Runtime;
    std::uint64_t m_PhysicsEpoch = 0;
    std::unordered_map<EntityKey, Session, EntityKeyHash> m_Sessions;
    std::list<Session> m_Retiring;
};

} // namespace BML::Behavior::PhysicsForce

#endif // BML_BEHAVIOR_PHYSICSFORCE_H
