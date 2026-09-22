#ifndef BML_IVP_MINDIST_H
#define BML_IVP_MINDIST_H

#include "BML/IVP/Broadphase.h"
#include "BML/IVP/Collision.h"
#include "BML/IVP/Object.h"

#include <cstddef>
#include <cstdint>

class IVP_Mindist;
class IVP_Mindist_Base;
class IVP_Mindist_Manager;
class IVP_Contact_Point;
class IVP_Friction_System;
class IVP_Simulation_Unit;

enum IVP_SYNAPSE_POLYGON_STATUS : std::int32_t {
    IVP_ST_POINT = 0,
    IVP_ST_EDGE = 1,
    IVP_ST_TRIANGLE = 2,
    IVP_ST_BALL = 3,
    IVP_ST_MAX_LEGAL = 4,
    IVP_ST_BACKSIDE = 5,
};

// Borrowed synapses are owned by the retail collision system. The retail
// implementation uses a 0x1c-byte stride; the nearby source's "32 byte"
// comment is stale for Ballance.
class IVP_Synapse : public IVP_Listener_Hull {
public:
    IVP_Synapse *next;
    IVP_Synapse *prev;
    IVP_Real_Object *l_obj;
    const IVP_Compact_Edge *edge;

    IVP_Real_Object *get_object() { return l_obj; }
    IVP_Real_Object *get_object() const { return l_obj; }
    IVP_SYNAPSE_POLYGON_STATUS get_status() const {
        return static_cast<IVP_SYNAPSE_POLYGON_STATUS>(status);
    }
    const IVP_Compact_Ledge *get_ledge() const {
        return BML::IVP::ABI::InvokeThis<const IVP_Compact_Ledge *>(
            BML::IVP::ABI::Address::SynapseGetLedge,
            const_cast<IVP_Synapse *>(this));
    }
    const IVP_Compact_Edge *get_edge() const { return edge; }
    IVP_Mindist_Base *get_synapse_mindist() const {
        return reinterpret_cast<IVP_Mindist_Base *>(
            reinterpret_cast<std::uintptr_t>(this) + mindist_offset);
    }
    void set_synapse_mindist(IVP_Mindist_Base *mindist) {
        mindist_offset = static_cast<std::int16_t>(
            reinterpret_cast<std::intptr_t>(mindist) -
            reinterpret_cast<std::intptr_t>(this));
    }
    void init_synapse_real(
        IVP_Mindist_Base *mindist, IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SynapseInitialize,
            this, mindist, object);
    }

    virtual ~IVP_Synapse() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::SynapseDestruct, this, [] {});
    }

protected:
    IVP_Synapse() = default;

    IVP_HULL_ELEM_TYPE get_type() override {
        return IVP_HULL_ELEM_POLYGON;
    }
    void hull_limit_exceeded_event(
        IVP_Hull_Manager *manager, IVP_HTIME intrusion) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SynapseHullLimitExceeded,
            this, manager, intrusion);
    }
    void hull_manager_is_going_to_be_deleted_event(
        IVP_Hull_Manager *manager) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SynapseHullManagerDeleted,
            this, manager);
    }
    void hull_manager_is_reset(
        IVP_FLOAT deltaTime, IVP_FLOAT centerDeltaTime) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SynapseHullManagerReset,
            this, deltaTime, centerDeltaTime);
    }

    std::int16_t mindist_offset;
    std::int16_t status;
};

// The retail type deliberately adds no storage to IVP_Synapse.
class IVP_Synapse_Real : public IVP_Synapse {
    friend class IVP_Mindist_Base;

public:
    void update_synapse(
        const IVP_Compact_Edge *newEdge,
        IVP_SYNAPSE_POLYGON_STATUS newStatus) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SynapseRealUpdate,
            this, newEdge, newStatus);
    }
    IVP_Core *get_core() const {
        return l_obj ? l_obj->get_core() : nullptr;
    }
    IVP_Synapse_Real *get_next() {
        return static_cast<IVP_Synapse_Real *>(next);
    }
    IVP_Synapse_Real *get_prev() {
        return static_cast<IVP_Synapse_Real *>(prev);
    }
    IVP_Mindist *get_mindist() const {
        return reinterpret_cast<IVP_Mindist *>(get_synapse_mindist());
    }

protected:
    IVP_Synapse_Real() = default;
    ~IVP_Synapse_Real() override = default;
};

enum IVP_MINIMAL_DIST_STATUS : std::int32_t {
    IVP_MD_UNINITIALIZED = 0,
    IVP_MD_INVALID = 2,
    IVP_MD_EXACT = 3,
    IVP_MD_HULL_RECURSIVE = 4,
    IVP_MD_HULL = 5,
};

enum IVP_MINIMAL_DIST_RECALC_RESULT : std::int32_t {
    IVP_MDRR_OK = 0,
    IVP_MDRR_INTRUSION = 1,
};

enum IVP_COLL_TYPE : std::int32_t {
    IVP_COLL_NONE = 0x00,
    IVP_COLL_PP_COLL = 0x10,
    IVP_COLL_PP_PK = 0x11,
    IVP_COLL_PF_COLL = 0x20,
    IVP_COLL_PF_NPF = 0x21,
    IVP_COLL_PK_COLL = 0x30,
    IVP_COLL_PK_PF = 0x31,
    IVP_COLL_PK_KK = 0x32,
    IVP_COLL_PK_NOT_MORE_PARALLEL = 0x33,
    IVP_COLL_KK_COLL = 0x40,
    IVP_COLL_KK_PARALLEL = 0x41,
    IVP_COLL_KK_PF = 0x42,
};

enum IVP_MINDIST_FUNCTION : std::int32_t {
    IVP_MF_COLLISION = 0,
    IVP_MF_PHANTOM = 1,
};

enum IVP_MRC_TYPE : std::int32_t {
    IVP_MRC_UNINITIALIZED = 0,
    IVP_MRC_OK = 1,
    IVP_MRC_ENDLESS_LOOP = 2,
    IVP_MRC_BACKSIDE = 3,
    IVP_MRC_ALREADY_CALCULATED = 4,
    IVP_MRC_ILLEGAL = 5,
};

enum IVP_MINDIST_EVENT_HINT : std::int32_t {
    IVP_EH_NOW = 0,
    IVP_EH_SMALL_DELAY = 1,
    IVP_EH_BIG_DELAY = 2,
};

// Mindists are owned by the retail collision manager. Construction and
// destruction remain unavailable, while observed retail methods can be called
// through version-locked entry points.
class IVP_Mindist_Base : public IVP_Collision {
public:
    IVP_Synapse *get_mindist_synapse(int index) {
        return &synapse[index];
    }
    const IVP_Synapse *get_mindist_synapse(int index) const {
        return &synapse[index];
    }
    IVP_FLOAT get_length() const { return len_numerator; }
    IVP_COLL_TYPE get_collision_type() const {
        return static_cast<IVP_COLL_TYPE>(flags & 0xFFu);
    }
    unsigned int get_synapse_sort_flag() const {
        return (flags >> 8u) & 0x03u;
    }
    IVP_BOOL get_is_in_phantom_set() const {
        return static_cast<IVP_BOOL>((flags >> 10u) & 0x03u);
    }
    IVP_MINDIST_FUNCTION get_mindist_function() const {
        return static_cast<IVP_MINDIST_FUNCTION>((flags >> 12u) & 0x03u);
    }
    IVP_MINIMAL_DIST_RECALC_RESULT get_recalc_result() const {
        return static_cast<IVP_MINIMAL_DIST_RECALC_RESULT>(
            (flags >> 14u) & 0x03u);
    }
    IVP_BOOL get_disable_halfspace_optimization() const {
        return static_cast<IVP_BOOL>((flags >> 16u) & 0x03u);
    }
    IVP_MINIMAL_DIST_STATUS get_mindist_status() const {
        return static_cast<IVP_MINIMAL_DIST_STATUS>(
            (flags >> 18u) & 0x0Fu);
    }
    unsigned int get_collision_distance_selector() const {
        return (flags >> 22u) & 0xFFu;
    }
    const IVP_U_Float_Point *get_contact_plane() const {
        return &contact_plane;
    }

    void get_objects(IVP_Real_Object *objectsOut[2]) override {
        objectsOut[0] = synapse[0].get_object();
        objectsOut[1] = synapse[1].get_object();
    }
    void get_ledges(const IVP_Compact_Ledge *ledgesOut[2]) override {
        ledgesOut[0] = synapse[0].get_ledge();
        ledgesOut[1] = synapse[1].get_ledge();
    }

protected:
    ~IVP_Mindist_Base() override = default;

    std::uint32_t flags;
    IVP_Synapse_Real synapse[2];
    IVP_FLOAT sum_extra_radius;
    IVP_FLOAT len_numerator;
    IVP_FLOAT contact_dot_diff_center;
    std::uint32_t reserved_5C;
    IVP_DOUBLE sum_angular_hull_time;
    IVP_U_Float_Point contact_plane;
};

class IVP_Mindist : public IVP_Mindist_Base {
    friend class IVP_Mindist_Manager;

protected:
    // These declarations preserve the retail derived-vtable order. They are
    // engine callbacks, not Mod-owned extension points.
    virtual void mindist_rescue_push() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRescuePush, this);
    }
    IVP_Time_CODE recalc_time_stamp;

public:
    IVP_Synapse_Real *get_synapse(int index) const {
        return const_cast<IVP_Synapse_Real *>(&synapse[index]);
    }
    IVP_Synapse_Real *get_sorted_synapse(int index) const {
        return BML::IVP::ABI::InvokeThis<IVP_Synapse_Real *>(
            BML::IVP::ABI::Address::MindistGetSortedSynapse,
            const_cast<IVP_Mindist *>(this), index);
    }
    IVP_Environment *get_environment() const {
        IVP_Real_Object *object = synapse[0].get_object();
        return object ? object->get_environment() : nullptr;
    }
    void init_mindist(
        IVP_Real_Object *first, IVP_Real_Object *second,
        const IVP_Compact_Edge *firstEdge,
        const IVP_Compact_Edge *secondEdge) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistInitialize,
            this, first, second, firstEdge, secondEdge);
    }
    void update_exact_mindist_events(
        IVP_BOOL allowHullConversion, IVP_MINDIST_EVENT_HINT hint) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistUpdateExactEvents,
            this, allowHullConversion, hint);
    }
    IVP_MRC_TYPE recalc_mindist() {
        return BML::IVP::ABI::InvokeThis<IVP_MRC_TYPE>(
            BML::IVP::ABI::Address::MindistRecalc, this);
    }
    IVP_MRC_TYPE recalc_invalid_mindist() {
        return BML::IVP::ABI::InvokeThis<IVP_MRC_TYPE>(
            BML::IVP::ABI::Address::MindistRecalcInvalid, this);
    }
    IVP_Contact_Point *try_to_generate_managed_friction(
        IVP_Friction_System **associatedSystem, IVP_BOOL *havingNew,
        IVP_Simulation_Unit *simulationUnitNotDestroy,
        IVP_BOOL callRecalculateSvals) {
        return BML::IVP::ABI::InvokeThis<IVP_Contact_Point *>(
            BML::IVP::ABI::Address::MindistTryManagedFriction,
            this, associatedSystem, havingNew, simulationUnitNotDestroy,
            callRecalculateSvals);
    }
    void create_cp_in_advance_pretension(
        IVP_Real_Object *object, IVP_FLOAT gapLength) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistCreatePretensionContact,
            this, object, gapLength);
    }

    void simulate_time_event(IVP_Environment *environment) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistSimulateTimeEvent,
            this, environment);
    }
    // Ballance removed the neighboring source version's is_recursive vtable
    // slot. Keep the source-level query without changing the eight-slot retail
    // table, and identify the only retained recursive concrete type by its
    // version-locked primary address point.
    IVP_BOOL is_recursive() {
        using Impact = void (__thiscall *)(IVP_Mindist *);
        Impact impact = BML::IVP::ABI::Resolve<Impact>(
            BML::IVP::ABI::Address::MindistDoImpact);
        if (!impact)
            return IVP_FALSE;
        const std::uintptr_t imageBase =
            reinterpret_cast<std::uintptr_t>(impact) -
            static_cast<std::uint32_t>(
                BML::IVP::ABI::Address::MindistDoImpact);
        const void *recursiveVtable = reinterpret_cast<const void *>(
            imageBase + BML::IVP::ABI::MindistRecursiveVtableRva);
        const void *objectVtable =
            *reinterpret_cast<const void *const *>(this);
        return objectVtable == recursiveVtable
            ? IVP_TRUE
            : IVP_FALSE;
    }
    virtual void exact_mindist_went_invalid(IVP_Mindist_Manager *manager) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistExactWentInvalid,
            this, manager);
    }
    virtual void do_impact() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistDoImpact, this);
    }

    IVP_Mindist *next;
    IVP_Mindist *prev;
    const IVP_Compact_Edge *last_visited_triangle;

protected:
    ~IVP_Mindist() override = default;
};

enum IVP_MINDIST_RECURSIVE_TYPES : std::int32_t {
    IVP_MR_NORMAL = -1,
    IVP_MR_FIRST_SYNAPSE_RECURSIVE = 0,
    IVP_MR_SECOND_SYNAPSE_RECURSIVE = 1,
};

// Borrowed recursive collision node. Ballance allocates exactly 0x98 bytes;
// the neighboring revision's spawned_mindist_count tail is not present. The
// complete retail constructor calls two complete bases and therefore cannot
// be expressed as an ordinary C++ forwarding constructor without duplicating
// base construction across the DLL boundary.
class IVP_Mindist_Recursive : public IVP_Mindist,
                              public IVP_Collision_Delegator {
private:
    void delete_all_children() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRecursiveDeleteAllChildren, this);
    }
    void collision_is_going_to_be_deleted_event(
        IVP_Collision *collision) override {
        // The retained body is compiled for the adjusted secondary-base ECX,
        // not the complete-object pointer used inside this C++ override.
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRecursiveCollisionDeleted,
            static_cast<IVP_Collision_Delegator *>(this), collision);
    }
    void recheck_recursive_childs(IVP_DOUBLE hullDistance) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRecursiveRecheckChildren,
            this, hullDistance);
    }
    void invalid_mindist_went_exact() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRecursiveInvalidWentExact, this);
    }

public:
    // The retained override is a folded no-op body.
    void mindist_rescue_push() override {}
    void rec_hull_limit_exceeded_event() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRecursiveHullLimitExceeded, this);
    }
    void exact_mindist_went_invalid(IVP_Mindist_Manager *manager) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRecursiveExactWentInvalid,
            this, manager);
    }
    void do_impact() override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistRecursiveDoImpact, this);
    }

    IVP_MINDIST_RECURSIVE_TYPES recursive_status;
    IVP_U_FVector<IVP_Collision> mindists;

    IVP_Mindist_Recursive(
        IVP_Environment *, IVP_Collision_Delegator *) = delete;

protected:
    ~IVP_Mindist_Recursive() override = default;
};

// Environment-owned submanager that may also be constructed explicitly. The
// retail layout is 0x18 bytes: scanning flag, environment, exact list, vector,
// then invalid list.
class IVP_Mindist_Manager {
    friend class IVP_Real_Object;

private:
    IVP_BOOL scanning_universe;

public:
    IVP_Environment *environment;
    IVP_Mindist *exact_mindists;
    // The retained complete constructor at RVA 0x186E0 initializes this
    // vector as part of its full 0x18-byte clear. Keep the member in inactive
    // union storage so the caller's compiler cannot preconstruct it before
    // crossing into physics_RT.dll.
    union {
        IVP_U_Vector<IVP_Mindist> wheel_look_ahead_mindists;
    };
    IVP_Mindist *invalid_mindists;

    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    explicit IVP_Mindist_Manager(IVP_Environment *owner) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MindistManagerConstruct, this,
            [this, owner] {
                scanning_universe = IVP_FALSE;
                environment = owner;
                exact_mindists = nullptr;
                ::new (static_cast<void *>(&wheel_look_ahead_mindists))
                    IVP_U_Vector<IVP_Mindist>();
                invalid_mindists = nullptr;
            },
            owner);
    }

    ~IVP_Mindist_Manager() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MindistManagerDestruct, this,
            [this] {
                while (exact_mindists) {
                    IVP_Mindist *next = exact_mindists->next;
                    delete exact_mindists;
                    exact_mindists = next;
                }
                while (invalid_mindists) {
                    IVP_Mindist *next = invalid_mindists->next;
                    delete invalid_mindists;
                    invalid_mindists = next;
                }
                wheel_look_ahead_mindists
                    .~IVP_U_Vector<IVP_Mindist>();
            });
    }

    static void create_exact_mindists(
        IVP_Real_Object *first, IVP_Real_Object *second,
        IVP_DOUBLE scanRadius,
        IVP_U_FVector<IVP_Collision> *existingCollisions,
        const IVP_Compact_Ledge *singleLedgeFirst,
        const IVP_Compact_Ledge *singleLedgeSecond,
        const IVP_Compact_Ledge *rootLedgeFirst,
        const IVP_Compact_Ledge *rootLedgeSecond,
        IVP_Collision_Delegator *delegator) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::MindistManagerCreateExact,
            first, second, scanRadius, existingCollisions,
            singleLedgeFirst, singleLedgeSecond,
            rootLedgeFirst, rootLedgeSecond, delegator);
    }
    void insert_exact_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerInsertExact,
            this, mindist);
    }
    void insert_and_recalc_exact_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerInsertAndRecalcExact,
            this, mindist);
    }
    void insert_and_recalc_phantom_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerInsertAndRecalcPhantom,
            this, mindist);
    }
    void insert_invalid_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerInsertInvalid,
            this, mindist);
    }
    void remove_exact_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRemoveExact,
            this, mindist);
    }
    void remove_invalid_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRemoveInvalid,
            this, mindist);
    }
    void remove_hull_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRemoveHull,
            this, mindist);
    }
    static void mindist_entered_phantom(IVP_Mindist *mindist) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::MindistManagerEnteredPhantom,
            mindist);
    }
    static void mindist_left_phantom(IVP_Mindist *mindist) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::MindistManagerLeftPhantom,
            mindist);
    }
    static void insert_hull_mindist(
        IVP_Mindist *mindist, IVP_HTIME hullTime) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::MindistManagerInsertHull,
            mindist, hullTime);
    }
    static void insert_hull_mindist(
        IVP_Mindist *mindist, IVP_HTIME firstHullTime,
        IVP_HTIME secondHullTime) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::MindistManagerInsertHullPair,
            mindist, firstHullTime, secondHullTime);
    }
    static void insert_lazy_hull_mindist(
        IVP_Mindist *mindist, IVP_HTIME firstHullTime,
        IVP_HTIME secondHullTime) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::MindistManagerInsertLazyHullPair,
            mindist, firstHullTime, secondHullTime);
    }
    static void insert_lazy_hull_mindist(
        IVP_Mindist *mindist, IVP_HTIME hullTime) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::MindistManagerInsertLazyHull,
            mindist, hullTime);
    }
    void recalc_all_exact_mindists() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRecalcAllExact, this);
    }
    void recalc_exact_mindist(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRecalcExact,
            this, mindist);
    }
    void recalc_all_exact_wheel_mindist() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRecalcAllWheels, this);
    }
    void recalc_all_exact_mindists_events() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRecalcAllEvents, this);
    }
    void enable_collision_detection_for_object(IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerEnableCollision,
            this, object);
    }
    void recheck_ov_element(IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MindistManagerRecheckOvElement,
            this, object);
    }
};

// This is another borrowed node type. Retail accesses status at +0x0e and
// edge at +0x10, yielding a 0x14-byte node rather than the nearby source
// comment's claimed 32 bytes.
class IVP_Synapse_Friction {
public:
    IVP_Synapse_Friction *next;
    IVP_Synapse_Friction *prev;
    IVP_Real_Object *l_obj;

protected:
    std::int16_t contact_point_offset;
    std::int8_t status;
    std::uint8_t reserved_0F;

public:
    const IVP_Compact_Edge *edge;

    IVP_Real_Object *get_object() { return l_obj; }
    IVP_Synapse_Friction *get_next() { return next; }
    IVP_Synapse_Friction *get_prev() { return prev; }
    IVP_Contact_Point *get_contact_point() const {
        return reinterpret_cast<IVP_Contact_Point *>(
            reinterpret_cast<std::uintptr_t>(this) +
            contact_point_offset);
    }
    void set_contact_point(IVP_Contact_Point *contactPoint) {
        contact_point_offset = static_cast<std::int16_t>(
            reinterpret_cast<std::intptr_t>(contactPoint) -
            reinterpret_cast<std::intptr_t>(this));
    }
    int get_material_index() const {
        return BML::IVP::ABI::InvokeThis<int>(
            BML::IVP::ABI::Address::SynapseFrictionGetMaterialIndex,
            const_cast<IVP_Synapse_Friction *>(this));
    }
    IVP_SYNAPSE_POLYGON_STATUS get_status() const {
        return static_cast<IVP_SYNAPSE_POLYGON_STATUS>(status);
    }
    const IVP_Compact_Edge *get_edge() const { return edge; }
    void init_synapse_friction(
        IVP_Contact_Point *contactPoint, IVP_Real_Object *object,
        const IVP_Compact_Edge *newEdge,
        IVP_SYNAPSE_POLYGON_STATUS newStatus) {
        l_obj = object;
        set_contact_point(contactPoint);
        next = object ? object->friction_synapses : nullptr;
        prev = nullptr;
        if (next)
            next->prev = this;
        if (object)
            object->friction_synapses = this;
        status = static_cast<std::int8_t>(newStatus);
        edge = newEdge;
    }
    IVP_BOOL is_same_as(const IVP_Synapse_Real *other) const {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::SynapseFrictionIsSameAs,
            const_cast<IVP_Synapse_Friction *>(this), other);
    }
    void remove_friction_synapse_from_object() {
        if (next)
            next->prev = prev;
        if (prev)
            prev->next = next;
        else if (l_obj)
            l_obj->friction_synapses = next;
    }

};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Synapse) == 0x1C);
static_assert(sizeof(IVP_Synapse_Real) == 0x1C);
static_assert(sizeof(IVP_Synapse_Friction) == 0x14);
static_assert(sizeof(IVP_Mindist_Base) == 0x78);
static_assert(sizeof(IVP_Mindist) == 0x88);
static_assert(sizeof(IVP_Mindist_Recursive) == 0x98);
static_assert(sizeof(IVP_Mindist_Manager) == 0x18);
static_assert(offsetof(IVP_Synapse, next) == 0x08);
static_assert(offsetof(IVP_Synapse, l_obj) == 0x10);
static_assert(offsetof(IVP_Synapse, edge) == 0x14);
static_assert(offsetof(IVP_Synapse_Friction, l_obj) == 0x08);
static_assert(offsetof(IVP_Synapse_Friction, edge) == 0x10);
static_assert(offsetof(IVP_Mindist, next) == 0x7C);
static_assert(offsetof(IVP_Mindist, prev) == 0x80);
static_assert(offsetof(IVP_Mindist, last_visited_triangle) == 0x84);
static_assert(offsetof(IVP_Mindist_Recursive, recursive_status) == 0x8C);
static_assert(offsetof(IVP_Mindist_Recursive, mindists) == 0x90);
static_assert(offsetof(IVP_Mindist_Manager, environment) == 0x04);
static_assert(offsetof(IVP_Mindist_Manager, exact_mindists) == 0x08);
static_assert(offsetof(IVP_Mindist_Manager, wheel_look_ahead_mindists) == 0x0C);
static_assert(offsetof(IVP_Mindist_Manager, invalid_mindists) == 0x14);
#endif

#endif // BML_IVP_MINDIST_H
