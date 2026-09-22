#ifndef BML_IVP_LISTENERS_H
#define BML_IVP_LISTENERS_H

#include "BML/IVP/Object.h"

#include <cstddef>
#include <cstdint>

class IVP_Compact_Edge;
class IVP_Contact_Point;
class IVP_Constraint;
class IVP_Controller_Phantom;
class IVP_Mindist_Base;

struct IVP_Event_PSI {
    IVP_Environment *environment;
};

class IVP_Listener_PSI {
public:
    virtual void event_PSI(IVP_Event_PSI *) = 0;
    virtual void environment_will_be_deleted(IVP_Environment *) = 0;
};

class IVP_Listener_Constraint {
public:
    virtual void event_constraint_broken(IVP_Constraint *) = 0;
};

class IVP_Listener_Phantom {
public:
    IVP_Listener_Phantom() {}

    virtual void mindist_entered_volume(
        class IVP_Controller_Phantom *, class IVP_Mindist_Base *) = 0;
    virtual void mindist_left_volume(
        class IVP_Controller_Phantom *, class IVP_Mindist_Base *) = 0;
    virtual void core_entered_volume(
        class IVP_Controller_Phantom *, class IVP_Core *) = 0;
    virtual void core_left_volume(
        class IVP_Controller_Phantom *, class IVP_Core *) = 0;
    virtual void phantom_is_going_to_be_deleted_event(
        class IVP_Controller_Phantom *) = 0;
};

struct IVP_Contact_Situation {
    IVP_Contact_Situation() = default;

    IVP_U_Float_Point surf_normal;
    IVP_U_Float_Point speed;
    IVP_U_Point contact_point_ws;
    IVP_Real_Object *objects[2];
    const IVP_Compact_Edge *compact_edges[2];
    IVP_Material *materials[2];
};

// IVP_Contact_Point is only forward-declared at this layer to avoid the
// Listeners/Friction include cycle; Friction.h provides its verified 0x78
// definition. These source-public accessors use the same Ballance offsets. In
// particular, the two retail IVP_Synapse_Friction members are 0x14 bytes each,
// placing tmp_contact_info at 0x40, integrated_destroyed_energy at 0x48, and
// now_friction_pressure at 0x54.
class IVP_Contact_Point_API {
public:
    static IVP_FLOAT get_eliminated_energy(
        IVP_Contact_Point *frictionHandle) {
        return float_field(frictionHandle, 0x48u);
    }

    static void reset_eliminated_energy(
        IVP_Contact_Point *frictionHandle) {
        float_field(frictionHandle, 0x48u) = 0.0f;
    }

    static IVP_FLOAT get_vert_force(IVP_Contact_Point *frictionHandle) {
        return float_field(frictionHandle, 0x54u);
    }

    static void get_surface_normal_ws(
        IVP_Contact_Point *frictionHandle, IVP_U_Float_Point *normal) {
        auto *contactSituation = *reinterpret_cast<IVP_Contact_Situation **>(
            bytes(frictionHandle) + 0x40u);
        normal->set(&contactSituation->surf_normal);
    }

private:
    static std::byte *bytes(IVP_Contact_Point *contactPoint) {
        return reinterpret_cast<std::byte *>(contactPoint);
    }
    static IVP_FLOAT &float_field(
        IVP_Contact_Point *contactPoint, std::size_t offset) {
        return *reinterpret_cast<IVP_FLOAT *>(bytes(contactPoint) + offset);
    }
};

struct IVP_Event_Object {
    IVP_Environment *environment;
    IVP_Real_Object *real_object;
};

struct IVP_Event_Collision {
    IVP_FLOAT d_time_since_last_collision;
    IVP_Environment *environment;
    IVP_Contact_Situation *contact_situation;
};

struct IVP_Event_Friction {
    IVP_Environment *environment;
    IVP_Contact_Situation *contact_situation;
    IVP_Contact_Point *friction_handle;
};

class IVP_Listener_Object {
public:
    virtual void event_object_deleted(IVP_Event_Object *) = 0;
    virtual void event_object_created(IVP_Event_Object *) = 0;
    virtual void event_object_revived(IVP_Event_Object *) = 0;
    virtual void event_object_frozen(IVP_Event_Object *) = 0;
    virtual ~IVP_Listener_Object() = default;
};

enum IVP_LISTENER_COLLISION_CALLBACKS : std::int32_t {
    IVP_LISTENER_COLLISION_CALLBACK_POST_COLLISION = 0x01,
    IVP_LISTENER_COLLISION_CALLBACK_OBJECT_DELETED = 0x02,
    IVP_LISTENER_COLLISION_CALLBACK_FRICTION = 0x04,
    IVP_LISTENER_COLLISION_CALLBACK_PRE_COLLISION = 0x08,
};

// Ballance's vtable has these four callbacks followed by the destructor. Its
// order differs from the nearby source because pre-collision and friction-pair
// callbacks are absent, while object-deleted occupies slot 1. The surviving
// callbacks otherwise retain their source-relative order.
class IVP_Listener_Collision {
    friend struct BML_IvpCollisionListenerLayoutCheck;

public:
    explicit IVP_Listener_Collision(int callbacks =
        IVP_LISTENER_COLLISION_CALLBACK_POST_COLLISION)
        : enabled_callbacks(callbacks) {}

    int get_enabled_callbacks() { return enabled_callbacks; }
    int get_enabled_callbacks() const { return enabled_callbacks; }

    virtual void event_post_collision(IVP_Event_Collision *) {}
    virtual void event_collision_object_deleted(IVP_Real_Object *) {}
    virtual void event_friction_created(IVP_Event_Friction *) {}
    virtual void event_friction_deleted(IVP_Event_Friction *) {}
    virtual ~IVP_Listener_Collision() = default;

private:
    int enabled_callbacks;
};

struct BML_IvpCollisionListenerLayoutCheck {
    static constexpr std::size_t callbacks =
        offsetof(IVP_Listener_Collision, enabled_callbacks);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Contact_Situation) == 0x58);
static_assert(alignof(IVP_Contact_Situation) == 0x08);
static_assert(offsetof(IVP_Contact_Situation, surf_normal) == 0x00);
static_assert(offsetof(IVP_Contact_Situation, speed) == 0x10);
static_assert(offsetof(IVP_Contact_Situation, contact_point_ws) == 0x20);
static_assert(offsetof(IVP_Contact_Situation, objects) == 0x40);
static_assert(offsetof(IVP_Contact_Situation, compact_edges) == 0x48);
static_assert(offsetof(IVP_Contact_Situation, materials) == 0x50);
static_assert(sizeof(IVP_Contact_Point_API) == 0x01);
static_assert(sizeof(IVP_Event_PSI) == 0x04);
static_assert(offsetof(IVP_Event_PSI, environment) == 0x00);
static_assert(sizeof(IVP_Listener_PSI) == 0x04);
static_assert(sizeof(IVP_Listener_Constraint) == 0x04);
static_assert(sizeof(IVP_Listener_Phantom) == 0x04);
static_assert(sizeof(IVP_Event_Object) == 0x08);
static_assert(offsetof(IVP_Event_Object, environment) == 0x00);
static_assert(offsetof(IVP_Event_Object, real_object) == 0x04);
static_assert(sizeof(IVP_Event_Collision) == 0x0C);
static_assert(offsetof(IVP_Event_Collision,
                       d_time_since_last_collision) == 0x00);
static_assert(offsetof(IVP_Event_Collision, environment) == 0x04);
static_assert(offsetof(IVP_Event_Collision, contact_situation) == 0x08);
static_assert(sizeof(IVP_Event_Friction) == 0x0C);
static_assert(offsetof(IVP_Event_Friction, environment) == 0x00);
static_assert(offsetof(IVP_Event_Friction, contact_situation) == 0x04);
static_assert(offsetof(IVP_Event_Friction, friction_handle) == 0x08);
static_assert(sizeof(IVP_Listener_Object) == 0x04);
static_assert(sizeof(IVP_Listener_Collision) == 0x08);
static_assert(BML_IvpCollisionListenerLayoutCheck::callbacks == 0x04);
#endif

#endif // BML_IVP_LISTENERS_H
