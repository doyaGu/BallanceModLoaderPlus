#ifndef BML_IVP_PHYSICS_RT_H
#define BML_IVP_PHYSICS_RT_H

#include "BML/IVP/Object.h"

#include <cstddef>
#include <cstdint>

class CK3dEntity;
class CKBehavior;
class PhysicsContactData;

// The value copied into CKIpionManager's entity hash. The DWORD at 0x1C is
// intentionally unnamed: the CK object id is the hash key, not this field.
struct BML_IVP_PhysicsObject {
    CKBehavior *Behavior;
    IVP_Real_Object *RealObject;
    void *InternalStorage;
    IVP_FLOAT Scale[3];
    std::uint32_t FrictionCount;
    std::uint32_t Unknown1C;
    IVP_Time CurrentTime;
    std::uint32_t Unknown28;
    PhysicsContactData *ContactData;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(BML_IVP_PhysicsObject) == 0x30);
static_assert(offsetof(BML_IVP_PhysicsObject, RealObject) == 0x04);
static_assert(offsetof(BML_IVP_PhysicsObject, Scale) == 0x0C);
static_assert(offsetof(BML_IVP_PhysicsObject, Unknown1C) == 0x1C);
static_assert(offsetof(BML_IVP_PhysicsObject, CurrentTime) == 0x20);
static_assert(offsetof(BML_IVP_PhysicsObject, ContactData) == 0x2C);
#endif

#endif // BML_IVP_PHYSICS_RT_H
