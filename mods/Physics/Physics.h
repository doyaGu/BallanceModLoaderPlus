#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>

#include <BML/BMLAll.h>

class IPhysicsObject
{
public:
    virtual const char *GetName() const = 0;
    virtual CK3dEntity *GetEntity() const = 0;

    virtual void SetGameData(void *data) = 0;
    virtual void *GetGameData() const = 0;

    virtual void SetGameFlags(unsigned int flags) = 0;
    virtual unsigned int GetGameFlags() const = 0;

    virtual void Wake() = 0;
    virtual void Sleep() = 0;

    virtual bool IsStatic() const = 0;
    virtual bool IsMovable() const = 0;
    virtual bool IsCollisionEnabled() const = 0;
    virtual bool IsGravityEnabled() const = 0;
    virtual bool IsMotionEnabled() const = 0;

    virtual void EnableCollisions(bool enable) = 0;
    virtual void EnableGravity(bool enable) = 0;
    virtual void EnableMotion(bool enable) = 0;

    virtual void RecheckCollisionFilter() = 0;

    virtual float GetMass() const = 0;
    virtual float GetInvMass() const = 0;
    virtual void SetMass(float mass) = 0;

    virtual void GetInertia(VxVector &inertia) const = 0;
    virtual void GetInvInertia(VxVector &inertia) const = 0;
    virtual void SetInertia(const VxVector &inertia) = 0;

    virtual void GetDamping(float *speed, float *rot) = 0;
    virtual void SetDamping(const float *speed, const float *rot) = 0;

    virtual void ApplyForceCenter(const VxVector &forceVector) = 0;
    virtual void ApplyForceOffset(const VxVector &forceVector, const VxVector &worldPosition) = 0;
    virtual void ApplyTorqueCenter(const VxVector &torqueImpulse) = 0;

    virtual void CalculateForceOffset(const VxVector &forceVector, const VxVector &worldPosition, VxVector &centerForce, VxVector &centerTorque) = 0;
    virtual void CalculateVelocityOffset(const VxVector &forceVector, const VxVector &worldPosition, VxVector &centerVelocity, VxVector &centerAngularVelocity) = 0;

    virtual void GetPosition(VxVector *worldPosition, VxVector *angles) = 0;
    virtual void GetPositionMatrix(VxMatrix& positionMatrix) = 0;

    virtual void SetPosition(const VxVector &worldPosition, const VxVector &angles, bool isTeleport) = 0;
    virtual void SetPositionMatrix(const VxMatrix& matrix, bool isTeleport) = 0;

    virtual void GetVelocity(VxVector *velocity, VxVector *angularVelocity) = 0;
    virtual void GetVelocityAtPoint(const VxVector &worldPosition, VxVector &velocity) = 0;
    virtual void SetVelocity(const VxVector *velocity, const VxVector *angularVelocity) = 0;
    virtual void AddVelocity(const VxVector *velocity, const VxVector *angularVelocity) = 0;

    virtual float GetEnergy() = 0;
};

class CKIpionManager : public CKBaseManager {
public:
    virtual void Reset() = 0;

    virtual IPhysicsObject *GetPhysicsObject(CK3dEntity *entity) = 0;

    virtual void ResetSimulationClock() = 0;

    virtual double GetSimulationTime() const = 0;

    virtual float GetSimulationTimeStep() const = 0;
    virtual void SetSimulationTimeStep(float step) = 0;

    virtual float GetDeltaTime() const = 0;
    virtual void SetDeltaTime(float delta) = 0;

    virtual float GetTimeFactor() const = 0;
    virtual void SetTimeFactor(float factor) = 0;

    virtual void GetGravity(VxVector &gravity) const = 0;
    virtual void SetGravity(const VxVector &gravity) = 0;
};

struct PhysicsData {
    bool valid = false;
    bool collisionEnabled = false;
    bool gravityEnabled = false;
    bool motionEnabled = false;
    const char *name = nullptr;
    float mass = 0.0f;
    VxVector inertia;
    float speedDamping = 0.0f;
    float rotDamping = 0.0f;
    VxVector position;
    VxVector angles;
    VxVector velocity;
    VxVector angularVelocity;

    void Acquire(IPhysicsObject *obj) {
        if (!obj) {
            if (valid) {
                valid = false;
                collisionEnabled = false;
                gravityEnabled = false;
                motionEnabled = false;
                name = nullptr;
                mass = 0.0f;
                inertia.Set(0.0f, 0.0f, 0.0f);
                speedDamping = 0.0f;
                rotDamping = 0.0f;
                position.Set(0.0f, 0.0f, 0.0f);
                angles.Set(0.0f, 0.0f, 0.0f);
                velocity.Set(0.0f, 0.0f, 0.0f);
                angularVelocity.Set(0.0f, 0.0f, 0.0f);
            }
        } else {
            valid = true;
            collisionEnabled = obj->IsCollisionEnabled();
            gravityEnabled = obj->IsGravityEnabled();
            motionEnabled = obj->IsMotionEnabled();
            name = obj->GetName();
            mass = obj->GetMass();
            obj->GetInertia(inertia);
            obj->GetDamping(&speedDamping, &rotDamping);
            obj->GetPosition(&position, &angles);
            obj->GetVelocity(&velocity, &angularVelocity);
        }
    }

    void Apply(IPhysicsObject *obj) {
        if (!obj)
            return;

        obj->EnableCollisions(collisionEnabled);
        obj->EnableGravity(gravityEnabled);
        obj->EnableMotion(motionEnabled);

        obj->SetMass(mass);
        obj->SetInertia(inertia);
        obj->SetDamping(&speedDamping, &rotDamping);

        obj->SetPosition(position, angles, true);
        obj->SetVelocity(&velocity, &angularVelocity);
    }

    void ApplyDiff(IPhysicsObject *obj, PhysicsData &n) const {
        if (!obj || !n.valid)
            return;

        if (collisionEnabled != n.collisionEnabled)
            obj->EnableCollisions(n.collisionEnabled);
        if (gravityEnabled != n.gravityEnabled)
            obj->EnableGravity(n.gravityEnabled);
        if (motionEnabled != n.motionEnabled)
            obj->EnableMotion(n.motionEnabled);

        if (mass != n.mass)
            obj->SetMass(n.mass);
        if (inertia != n.inertia)
            obj->SetInertia(n.inertia);
        if (speedDamping != n.speedDamping)
            obj->SetDamping(&n.speedDamping, nullptr);
        if (rotDamping != n.rotDamping)
            obj->SetDamping(nullptr, &n.rotDamping);
    }
};

class PhysicsMod : public IMod {
public:
    explicit PhysicsMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "Physics"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Physics"; }
    const char *GetAuthor() override { return "Kakuty"; }
    const char *GetDescription() override { return "Display Physics Info."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

    void OnProcess() override;

    void OnStartLevel() override;
    void OnPreResetLevel() override;
    void OnPreExitLevel() override;

    void OnDraw();

    CK3dEntity *GetActiveBall() const {
        if (m_ActiveBall) {
            return (CK3dEntity *)m_ActiveBall->GetValueObject();
        }

        return nullptr;
    }

    IPhysicsObject *GetPhysicsBall() const {
        if (m_ActiveBall) {
            auto *ball = (CK3dEntity *)m_ActiveBall->GetValueObject();
            if (ball) {
                return m_IpionManager->GetPhysicsObject(ball);
            }
        }

        return nullptr;
    }

    CKIpionManager *m_IpionManager = nullptr;
    InputHook *m_InputHook = nullptr;

    CKDataArray *m_CurLevel = nullptr;
    std::string m_MapName;

    bool m_ShowWindow = false;
    bool m_BallReset = true;
    CKParameter *m_ActiveBall = nullptr;
    PhysicsData m_BallData;
    PhysicsData m_BallDataLast;
    PhysicsData m_BallDataOrig;

    IProperty *m_Enabled = nullptr;
};

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);