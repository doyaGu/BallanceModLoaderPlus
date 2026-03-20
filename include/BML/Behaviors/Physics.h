// Auto-generated from physics_RT.dll -- do not edit manually.
#ifndef BML_BEHAVIORS_PHYSICS_H
#define BML_BEHAVIORS_PHYSICS_H

#include "BML/Behavior.h"
#include "CKAll.h"

namespace bml::Physics {

// ============================================================================
// Physicalize
//
// Makes an object physical
// ============================================================================

class Physicalize {
public:
    static constexpr CKGUID Guid = CKGUID(0x4B936301, 0x5381C0FE);

    Physicalize &Fixed(CKBOOL v) { m_Fixed = v; return *this; }
    Physicalize &Friction(float v) { m_Friction = v; return *this; }
    Physicalize &Elasticity(float v) { m_Elasticity = v; return *this; }
    Physicalize &Mass(float v) { m_Mass = v; return *this; }
    Physicalize &CollisionGroup(const char* v) { m_CollisionGroup = v; return *this; }
    Physicalize &StartFrozen(CKBOOL v) { m_StartFrozen = v; return *this; }
    Physicalize &EnableCollision(CKBOOL v) { m_EnableCollision = v; return *this; }
    Physicalize &CalcMassCenter(CKBOOL v) { m_CalcMassCenter = v; return *this; }
    Physicalize &LinearDamping(float v) { m_LinearDamping = v; return *this; }
    Physicalize &RotationDamping(float v) { m_RotationDamping = v; return *this; }
    Physicalize &CollisionSurface(const char* v) { m_CollisionSurface = v; return *this; }
    Physicalize &Mesh(CKMesh* v) { m_Mesh = v; return *this; }

    Physicalize &SettingBall(CKBOOL v) { m_SettBall = v; m_HasSettings = true; return *this; }

    void Run(BehaviorCache &cache, CK3dEntity *target, int slot = 0) const {
        auto runner = cache.Get(Guid, true);
        Run(runner, target, slot);
    }

    void Run(BehaviorRunner &runner, CK3dEntity *target, int slot = 0) const {
        runner.Target(target);
        if (m_HasSettings)
            ApplySettings(runner);
        Apply(runner);
        runner.Execute(slot);
    }

    CKBehavior *Build(CKBehavior *script, CK3dEntity *target) const {
        BehaviorFactory factory(script, Guid, true);
        factory.Target(target);
        if (m_HasSettings)
            ApplySettings(factory);
        Apply(factory);
        return factory.Build();
    }

private:
    template <typename Builder>
    void Apply(Builder &bb) const {
        bb.Input(0, m_Fixed)
          .Input(1, m_Friction)
          .Input(2, m_Elasticity)
          .Input(3, m_Mass)
          .Input(4, m_CollisionGroup)
          .Input(5, m_StartFrozen)
          .Input(6, m_EnableCollision)
          .Input(7, m_CalcMassCenter)
          .Input(8, m_LinearDamping)
          .Input(9, m_RotationDamping)
          .Input(10, m_CollisionSurface)
          .Input(11, m_Mesh);
    }

    void ApplySettings(BehaviorFactory &factory) const {
        factory.Local(1, m_SettBall);
        factory.ApplySettings();
    }

    void ApplySettings(BehaviorRunner &runner) const {
        runner.Local(1, m_SettBall);
        runner.ApplySettings();
    }

    CKBOOL m_Fixed = FALSE;
    float m_Friction = 0.0f;
    float m_Elasticity = 0.0f;
    float m_Mass = 0.0f;
    const char* m_CollisionGroup = "";
    CKBOOL m_StartFrozen = FALSE;
    CKBOOL m_EnableCollision = FALSE;
    CKBOOL m_CalcMassCenter = FALSE;
    float m_LinearDamping = 0.0f;
    float m_RotationDamping = 0.0f;
    const char* m_CollisionSurface = "";
    CKMesh* m_Mesh = nullptr;

    bool m_HasSettings = false;
    CKBOOL m_SettBall = FALSE;
};

// ============================================================================
// PhysicsForce
//
// Applies a constant force to an object
// ============================================================================

class PhysicsForce {
public:
    static constexpr CKGUID Guid = CKGUID(0x53794401, 0x5381C0FE);

    PhysicsForce &Position(VxVector v) { m_Position = v; return *this; }
    PhysicsForce &PositionReferential(CK3dEntity* v) { m_PositionReferential = v; return *this; }
    PhysicsForce &Direction(VxVector v) { m_Direction = v; return *this; }
    PhysicsForce &DirectionReferential(CK3dEntity* v) { m_DirectionReferential = v; return *this; }
    PhysicsForce &Force(float v) { m_Force = v; return *this; }

    void Run(BehaviorCache &cache, CK3dEntity *target, int slot = 0) const {
        auto runner = cache.Get(Guid, true);
        Run(runner, target, slot);
    }

    void Run(BehaviorRunner &runner, CK3dEntity *target, int slot = 0) const {
        runner.Target(target);
        Apply(runner);
        runner.Execute(slot);
    }

    CKBehavior *Build(CKBehavior *script, CK3dEntity *target) const {
        BehaviorFactory factory(script, Guid, true);
        factory.Target(target);
        Apply(factory);
        return factory.Build();
    }

private:
    template <typename Builder>
    void Apply(Builder &bb) const {
        bb.Input(0, m_Position)
          .Input(1, m_PositionReferential)
          .Input(2, m_Direction)
          .Input(3, m_DirectionReferential)
          .Input(4, m_Force);
    }

    VxVector m_Position = VxVector(0,0,0);
    CK3dEntity* m_PositionReferential = nullptr;
    VxVector m_Direction = VxVector(0,0,0);
    CK3dEntity* m_DirectionReferential = nullptr;
    float m_Force = 0.0f;
};

// ============================================================================
// PhysicsImpulse
//
// Applies an instantaneous impulse to an object
// ============================================================================

class PhysicsImpulse {
public:
    static constexpr CKGUID Guid = CKGUID(0x537A4401, 0x5381C0FE);

    PhysicsImpulse &Position(VxVector v) { m_Position = v; return *this; }
    PhysicsImpulse &PositionReferential(CK3dEntity* v) { m_PositionReferential = v; return *this; }
    PhysicsImpulse &Direction(VxVector v) { m_Direction = v; return *this; }
    PhysicsImpulse &DirectionReferential(CK3dEntity* v) { m_DirectionReferential = v; return *this; }
    PhysicsImpulse &Impulse(float v) { m_Impulse = v; return *this; }

    void Run(BehaviorCache &cache, CK3dEntity *target, int slot = 0) const {
        auto runner = cache.Get(Guid, true);
        Run(runner, target, slot);
    }

    void Run(BehaviorRunner &runner, CK3dEntity *target, int slot = 0) const {
        runner.Target(target);
        Apply(runner);
        runner.Execute(slot);
    }

    CKBehavior *Build(CKBehavior *script, CK3dEntity *target) const {
        BehaviorFactory factory(script, Guid, true);
        factory.Target(target);
        Apply(factory);
        return factory.Build();
    }

private:
    template <typename Builder>
    void Apply(Builder &bb) const {
        bb.Input(0, m_Position)
          .Input(1, m_PositionReferential)
          .Input(2, m_Direction)
          .Input(3, m_DirectionReferential)
          .Input(4, m_Impulse);
    }

    VxVector m_Position = VxVector(0,0,0);
    CK3dEntity* m_PositionReferential = nullptr;
    VxVector m_Direction = VxVector(0,0,0);
    CK3dEntity* m_DirectionReferential = nullptr;
    float m_Impulse = 0.0f;
};

// ============================================================================
// PhysicsWakeUp
//
// Wakes up a sleeping physical object
// ============================================================================

class PhysicsWakeUp {
public:
    static constexpr CKGUID Guid = CKGUID(0x4B836681, 0x5381C0FE);

    void Run(BehaviorCache &cache, CK3dEntity *target, int slot = 0) const {
        auto runner = cache.Get(Guid, true);
        Run(runner, target, slot);
    }

    void Run(BehaviorRunner &runner, CK3dEntity *target, int slot = 0) const {
        runner.Target(target);
        Apply(runner);
        runner.Execute(slot);
    }

    CKBehavior *Build(CKBehavior *script, CK3dEntity *target) const {
        BehaviorFactory factory(script, Guid, true);
        factory.Target(target);
        Apply(factory);
        return factory.Build();
    }

private:
    template <typename Builder>
    void Apply(Builder &bb) const {
        (void)bb;
    }
};

} // namespace bml::Physics

#endif // BML_BEHAVIORS_PHYSICS_H
