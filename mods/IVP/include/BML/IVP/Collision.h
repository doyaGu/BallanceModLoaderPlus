#ifndef BML_IVP_COLLISION_H
#define BML_IVP_COLLISION_H

#include "BML/IVP/TimeManager.h"

#include <cstddef>

class IVP_Compact_Ledge;
class IVP_Environment;
class IVP_Real_Object;
class IVP_Collision;

// Public collision-delegation contracts. The method order follows the retail
// classes used by the mindist manager; implementations supplied by Mods may be
// called directly through those retail vtables.
class IVP_Collision_Delegator {
public:
    virtual void collision_is_going_to_be_deleted_event(
        class IVP_Collision *) = 0;
    virtual ~IVP_Collision_Delegator() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::CollisionDelegatorDestruct,
            this, [] {});
    }
    // These bookkeeping hooks were added as virtuals in the neighboring
    // Source-era revision. Ballance's retained root-delegator dispatch proves
    // that slot 2 already belongs to object removal, so keep source-compatible
    // defaults without extending the retail two-slot base vtable.
    void change_spawned_mindist_count(int) {}
    int get_spawned_mindist_count() { return -1; }
};

class IVP_Collision_Delegator_Root : public IVP_Collision_Delegator {
public:
    virtual void object_is_removed_from_collision_detection(
        IVP_Real_Object *) = 0;
    virtual IVP_Collision *delegate_collisions_for_object(
        IVP_Real_Object *, IVP_Real_Object *) = 0;
    virtual void environment_is_going_to_be_deleted_event(
        IVP_Environment *) = 0;
};

// Ballance's default root delegator. The retail constructor replaces the
// caller-side vptr with physics_RT's five-slot table; its environment teardown
// callback consequently reaches the DLL's scalar deleting destructor. Keep
// construction and ownership on the retail heap across that boundary.
class IVP_Collision_Delegator_Root_Mindist
    : public IVP_Collision_Delegator_Root {
public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_Collision_Delegator_Root_Mindist() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionDelegatorRootMindistConstruct,
            this);
    }
    ~IVP_Collision_Delegator_Root_Mindist() override = default;

    void collision_is_going_to_be_deleted_event(
        IVP_Collision *collision) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionDelegatorRootMindistCollisionDeleted,
            this, collision);
    }
    void object_is_removed_from_collision_detection(
        IVP_Real_Object *object) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionDelegatorRootMindistObjectRemoved,
            this, object);
    }
    IVP_Collision *delegate_collisions_for_object(
        IVP_Real_Object *first, IVP_Real_Object *second) override {
        return BML::IVP::ABI::InvokeThis<IVP_Collision *>(
            BML::IVP::ABI::Address::CollisionDelegatorRootMindistDelegateCollisions,
            this, first, second);
    }
    void environment_is_going_to_be_deleted_event(
        IVP_Environment *environment) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionDelegatorRootMindistEnvironmentDeleted,
            this, environment);
    }
};

class IVP_Collision : public IVP_Time_Event {
public:
    explicit IVP_Collision(IVP_Collision_Delegator *owner)
        : delegator(owner), fvector_index{-1, -1} {}

    int get_fvector_index(int index) const { return fvector_index[index]; }
    void set_fvector_index(int oldIndex, int newIndex) {
        if (fvector_index[0] == oldIndex)
            fvector_index[0] = newIndex;
        else
            fvector_index[1] = newIndex;
    }

    virtual void get_objects(IVP_Real_Object *objectsOut[2]) = 0;
    virtual void get_ledges(const IVP_Compact_Ledge *ledgesOut[2]) = 0;
    virtual void delegator_is_going_to_be_deleted_event(
        IVP_Collision_Delegator *) {
        delete this;
    }
    virtual ~IVP_Collision() = default;

    IVP_Collision_Delegator *delegator;
    int fvector_index[2];
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Collision_Delegator) == 0x04);
static_assert(sizeof(IVP_Collision_Delegator_Root) == 0x04);
static_assert(sizeof(IVP_Collision_Delegator_Root_Mindist) == 0x04);
static_assert(sizeof(IVP_Collision) == 0x14);
static_assert(offsetof(IVP_Collision, delegator) == 0x08);
static_assert(offsetof(IVP_Collision, fvector_index) == 0x0C);
#endif

#endif // BML_IVP_COLLISION_H
