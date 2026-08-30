#ifndef BML_PHYSICSFORCESESSIONS_H
#define BML_PHYSICSFORCESESSIONS_H

#include <unordered_map>

#include "Virtools/BallanceBehaviorPresets.h"

namespace BML::Virtools {

// Physics Force stores its native handle in a local parameter, so Create and
// Shutdown must run on the same configured instance for each target.
class PhysicsForceSessions final {
public:
    PhysicsForceSessions(BehaviorRuntime &runtime, CKIdentityRegistry &identities)
        : m_Runtime(runtime), m_Identities(identities) {}

    ExecutionResult Set(const Presets::ForceOptions &options);
    ExecutionResult Clear(CK3dEntity *target);
    // Must run immediately after the physics post-process seam.  Physics Force
    // creates its native controller from a queued pre-sim callback, so a close
    // request is not safe to execute in the same physics epoch as Create.
    void ProcessFrame();
    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    void Reset();

private:
    struct ObjectKey {
        std::uint32_t Slot = 0;
        std::uint32_t Generation = 0;

        bool operator==(const ObjectKey &other) const noexcept {
            return Slot == other.Slot && Generation == other.Generation;
        }
    };

    struct ObjectKeyHash {
        std::size_t operator()(const ObjectKey &key) const noexcept {
            return static_cast<std::size_t>(key.Generation) * 0x9e3779b1u ^ key.Slot;
        }
    };

    struct StoredOptions {
        BML_ObjectRef Target;
        VxVector Position{0.0f, 0.0f, 0.0f};
        BML_ObjectRef PositionReference;
        bool HasPositionReference = false;
        VxVector Direction{0.0f, 0.0f, 0.0f};
        BML_ObjectRef DirectionReference;
        bool HasDirectionReference = false;
        float Magnitude = 0.0f;
    };

    struct Session {
        BML_ObjectRef Target;
        BehaviorInstance Instance;
        bool Closing = false;
        bool CancellationArmed = false;
        std::uint64_t CloseAfterEpoch = 0;
    };

    [[nodiscard]] StoredOptions Capture(const Presets::ForceOptions &options) const;
    [[nodiscard]] bool Resolve(const StoredOptions &stored, Presets::ForceOptions &options) const;
    [[nodiscard]] static ObjectKey KeyOf(BML_ObjectRef reference);
    [[nodiscard]] bool HasNativeController(Session &session) const;
    ExecutionResult Create(const StoredOptions &stored);
    static ExecutionResult Accepted(const char *message);

    BehaviorRuntime &m_Runtime;
    CKIdentityRegistry &m_Identities;
    std::uint64_t m_PhysicsEpoch = 0;
    std::unordered_map<ObjectKey, Session, ObjectKeyHash> m_Sessions;
};

} // namespace BML::Virtools

#endif // BML_PHYSICSFORCESESSIONS_H
