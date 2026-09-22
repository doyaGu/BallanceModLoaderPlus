#ifndef BML_IVP_OBJECT_H
#define BML_IVP_OBJECT_H

#include "BML/IVP/Core.h"
#include "BML/IVP/Broadphase.h"
#include "BML/IVP/Material.h"
#include "BML/IVP/MinList.h"
#include "BML/IVP/Surface.h"

#include <cstddef>
#include <cstdint>

class IVP_Anchor;
struct IVP_Ball;
class IVP_Cache_Object;
class IVP_Cluster;
class IVP_Controller_Phantom;
class IVP_Hull_Manager;
class IVP_Listener_Collision;
class IVP_Listener_Object;
class IVP_OV_Element;
class IVP_Object_Attach;
class IVP_Radar;
struct IVP_Polygon;
class IVP_Real_Object;
class IVP_SurfaceManager;
class IVP_Synapse_Friction;
class IVP_Synapse_Real;
class IVP_Template_Cluster;
class IVP_Template_Object;
struct IVP_Template_Phantom;
class IVP_Template_Real_Object;

enum IVP_OBJECT_TYPE : std::int32_t {
    IVP_NONE = 0,
    IVP_CLUSTER = 1,
    IVP_POLYGON = 2,
    IVP_BALL = 3,
    IVP_OBJECT = 4,
};

struct IVP_Hull_Manager_Base_Gradient {
    struct Retail_Construction_Tag {};

    IVP_Hull_Manager_Base_Gradient() : last_vpsi_time(0.0) {}
    ~IVP_Hull_Manager_Base_Gradient() = default;

protected:
    explicit IVP_Hull_Manager_Base_Gradient(
        Retail_Construction_Tag) noexcept {}

public:

    IVP_Time last_vpsi_time;
    IVP_FLOAT gradient;
    IVP_FLOAT center_gradient;
    IVP_FLOAT hull_value_last_vpsi;
    IVP_FLOAT hull_center_value_last_vpsi;
    IVP_FLOAT hull_value_next_psi;
    std::int32_t time_of_next_reset;
};

struct IVP_Hull_Manager_Base : IVP_Hull_Manager_Base_Gradient {
    IVP_Hull_Manager_Base()
        : IVP_Hull_Manager_Base_Gradient(Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HullManagerBaseConstruct, this);
    }
    ~IVP_Hull_Manager_Base() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HullManagerBaseDestruct, this);
    }

    // The RVA-backed functions are complete C++ constructor/destructor bodies:
    // retail constructs and destroys the embedded min-list at +0x20 itself.
    // A union supplies its storage without making the compiler run a second
    // IVP_U_Min_List lifetime around those calls.
    union {
        IVP_U_Min_List sorted_synapses;
    };
    std::uint32_t reserved_34;
};

// IVP_Real_Object embeds the Base storage, while the engine treats that same
// address as this method-bearing type. Ballance adds no fields in the derived
// class; these accessors are the source-inline operations used by radar and
// broadphase clients.
class IVP_Hull_Manager : public IVP_Hull_Manager_Base {
private:
    void reset_times() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HullManagerResetTimes, this);
    }

public:
    IVP_FLOAT get_current_hull_time() const {
        return hull_value_last_vpsi;
    }
    IVP_FLOAT get_current_center_hull_time() const {
        return hull_center_value_last_vpsi;
    }
    IVP_U_Min_List *get_sorted_synapses() { return &sorted_synapses; }

    IVP_FLOAT insert_lazy_synapse(
        IVP_Listener_Hull *listener, IVP_Time,
        IVP_DOUBLE validHullTime) {
        listener->minlist_index = sorted_synapses.add(
            listener,
            static_cast<IVP_FLOAT>(
                hull_value_next_psi + validHullTime));
        return hull_center_value_last_vpsi;
    }
    void update_lazy_synapse(
        IVP_Listener_Hull *listener, IVP_Time,
        IVP_DOUBLE validHullTime) {
        sorted_synapses.remove_minlist_elem(listener->minlist_index);
        listener->minlist_index = sorted_synapses.add(
            listener,
            static_cast<IVP_FLOAT>(
                hull_value_next_psi + validHullTime));
    }
    void remove_synapse(IVP_Listener_Hull *listener) {
        sorted_synapses.remove_minlist_elem(listener->minlist_index);
    }
    void jump_add_hull(IVP_FLOAT delta, IVP_FLOAT centerDelta) {
        hull_value_last_vpsi = hull_value_next_psi + delta;
        hull_value_next_psi = hull_value_last_vpsi;
        hull_center_value_last_vpsi += centerDelta;
        gradient = 0.0f;
        center_gradient = 0.0f;
    }
    void increase_hull_by_x(IVP_Time now, IVP_FLOAT deltaTime,
                            IVP_FLOAT newGradient,
                            IVP_FLOAT newCenterGradient) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HullManagerIncreaseByX,
            this, now, deltaTime, newGradient, newCenterGradient);
    }
    IVP_BOOL are_events_in_hull() {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::HullManagerAreEventsInHull, this);
    }
    void check_hull_synapses() {
        int remaining = 100;
        IVP_FLOAT intrusion = 0.0f;
        while ((intrusion = sorted_synapses.find_min_value() -
                            hull_value_next_psi) < 0.0f) {
            auto *listener = static_cast<IVP_Listener_Hull *>(
                sorted_synapses.find_min_elem());
            listener->hull_limit_exceeded_event(this, intrusion);
            if (remaining-- < 0)
                break;
        }
    }
    void check_for_reset() {
        if (last_vpsi_time.get_seconds() > time_of_next_reset) {
            reset_times();
            time_of_next_reset = static_cast<std::int32_t>(
                last_vpsi_time.get_seconds() + 10.0f);
        }
    }
};

class IVP_Object {
public:
    // Retail slot 0. Borrowed objects dispatch through the DLL's own table;
    // Mod-created derived shells use this exact complete base destructor.
    virtual ~IVP_Object() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ObjectDestruct, this);
    }

    IVP_OBJECT_TYPE object_type;
    IVP_Object *next_in_cluster;
    IVP_Object *prev_in_cluster;
    IVP_Cluster *father_cluster;
    const char *name;
    IVP_Environment *environment;

protected:
    // These are the two retail base construction paths. They establish the
    // Object vptr, cluster links, environment and engine-allocator-owned name.
    IVP_Object(IVP_Cluster *father, const IVP_Template_Object *configuration) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ObjectConstruct,
            this, father, configuration);
    }
    explicit IVP_Object(IVP_Environment *targetEnvironment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ObjectConstructRoot,
            this, targetEnvironment);
    }

public:

    IVP_OBJECT_TYPE get_type() const { return object_type; }
    const char *get_name() const { return name; }
    IVP_Environment *get_environment() const { return environment; }
    IVP_Polygon *to_poly() { return reinterpret_cast<IVP_Polygon *>(this); }
    IVP_Cluster *to_cluster() { return reinterpret_cast<IVP_Cluster *>(this); }
    IVP_Real_Object *to_real() {
        return reinterpret_cast<IVP_Real_Object *>(this);
    }
    IVP_Ball *to_ball() { return reinterpret_cast<IVP_Ball *>(this); }
};

class IVP_Cluster : public IVP_Object {
    friend struct BML_IvpClusterLayoutCheck;

public:
    IVP_Cluster(IVP_Cluster *father, IVP_Template_Cluster *configuration)
        : IVP_Object(father,
                     reinterpret_cast<const IVP_Template_Object *>(
                         configuration)),
          objects(nullptr) {
        object_type = IVP_CLUSTER;
    }

    ~IVP_Cluster() override {
        // Every child destructor unlinks itself from this head, exactly as the
        // retained retail IVP_Cluster destructor at RVA 0x9CB0 expects.
        while (objects)
            delete objects;
    }

    IVP_Object *get_first_object_of_cluster() { return objects; }
    IVP_Object *get_next_object_in_cluster(IVP_Object *object) {
        return object ? object->next_in_cluster : nullptr;
    }

protected:
    explicit IVP_Cluster(IVP_Environment *targetEnvironment)
        : IVP_Object(targetEnvironment), objects(nullptr) {
        object_type = IVP_CLUSTER;
    }

    IVP_Object *objects;
};

struct BML_IvpClusterLayoutCheck {
    static constexpr std::size_t objects = offsetof(IVP_Cluster, objects);
};

class IVP_Real_Object_Fast_Static : public IVP_Object {
public:
    IVP_Controller_Phantom *controller_phantom;
    IVP_Synapse_Real *exact_synapses;
    IVP_Synapse_Real *invalid_synapses;
    IVP_Synapse_Friction *friction_synapses;
    IVP_U_Quat *q_core_f_object;
    IVP_U_Float_Point shift_core_f_object;

    const IVP_U_Float_Point *get_shift_core_f_object() const {
        return &shift_core_f_object;
    }

protected:
    IVP_Real_Object_Fast_Static(
        IVP_Cluster *father, const IVP_Template_Object *configuration)
        : IVP_Object(father, configuration) {}
};

// The historical header exposed a nested anonymous bit-field struct as
// ``object->flags``.  Modern MSVC cannot preserve its VC6 layout when the
// fields retain their mixed enum types, so use one unsigned allocation unit
// while keeping every public member name and a raw view for retained code.
struct IVP_Real_Object_Flags {
    union {
        struct {
            unsigned int object_movement_state : 8;
            unsigned int collision_detection_enabled : 2;
            unsigned int shift_core_f_object_is_zero : 2;
            unsigned int object_listener_exists : 1;
            unsigned int collision_listener_exists : 1;
            unsigned int collision_listener_listens_to_friction : 1;
            unsigned int reserved : 17;
        };
        std::uint32_t raw;
    };

    operator std::uint32_t() const noexcept { return raw; }
    IVP_Real_Object_Flags &operator=(std::uint32_t value) noexcept {
        raw = value;
        return *this;
    }
    IVP_Real_Object_Flags &operator|=(std::uint32_t value) noexcept {
        raw |= value;
        return *this;
    }
    IVP_Real_Object_Flags &operator&=(std::uint32_t value) noexcept {
        raw &= value;
        return *this;
    }
};

class IVP_Real_Object_Fast : public IVP_Real_Object_Fast_Static {
public:
    IVP_Real_Object_Fast(
        IVP_Cluster *father, const IVP_Template_Object *configuration)
        : IVP_Real_Object_Fast_Static(father, configuration) {}

    IVP_Cache_Object *cache_object;
    std::uint32_t reserved_44;
    IVP_Hull_Manager_Base hull_manager;
    IVP_Real_Object_Flags flags;
    std::uint32_t reserved_84;
};

class IVP_Real_Object : public IVP_Real_Object_Fast {
    friend class IVP_Calc_Next_PSI_Solver;
    friend class IVP_Object_Attach;

public:
    IVP_Anchor *anchors;
    IVP_SurfaceManager *surface_manager;
    char nocoll_group_ident[8];
    IVP_Material *l_default_material;
    IVP_OV_Element *ov_element;
    IVP_FLOAT extra_radius;
    IVP_Core *physical_core;
    IVP_Core *friction_core;
    IVP_Core *original_core;
    void *client_data;
    std::uint32_t reserved_B4;

    IVP_Anchor *get_first_anchor() { return anchors; }
    IVP_Anchor *get_first_anchor() const { return anchors; }
    IVP_Synapse_Real *get_first_exact_synapse() { return exact_synapses; }
    IVP_Synapse_Real *get_first_exact_synapse() const { return exact_synapses; }
    IVP_Synapse_Friction *get_first_friction_synapse() {
        return friction_synapses;
    }
    IVP_Synapse_Friction *get_first_friction_synapse() const {
        return friction_synapses;
    }
    IVP_Hull_Manager *get_hull_manager() {
        return reinterpret_cast<IVP_Hull_Manager *>(&hull_manager);
    }
    IVP_SurfaceManager *get_surface_manager() const { return surface_manager; }
    IVP_OV_Element *get_ov_element() { return ov_element; }
    IVP_OV_Element *get_ov_element() const { return ov_element; }
    IVP_FLOAT get_extra_radius() const { return extra_radius; }
    IVP_Core *get_core() const { return physical_core; }
    IVP_Core *get_original_core() const { return original_core; }
    IVP_Controller_Phantom *get_controller_phantom() {
        return controller_phantom;
    }
    IVP_Controller_Phantom *get_controller_phantom() const {
        return controller_phantom;
    }
    IVP_Cache_Object *get_cache_object();
    IVP_Cache_Object *get_cache_object_no_lock();
    IVP_Movement_Type get_movement_state() {
        return static_cast<IVP_Movement_Type>(flags.object_movement_state);
    }
    IVP_Movement_Type get_movement_state() const {
        return static_cast<IVP_Movement_Type>(flags.object_movement_state);
    }
    void set_movement_state(IVP_Movement_Type state) {
        flags.object_movement_state = static_cast<unsigned int>(state);
    }
    IVP_BOOL is_collision_detection_enabled() {
        return flags.collision_detection_enabled != 0u ? IVP_TRUE : IVP_FALSE;
    }
    IVP_BOOL is_collision_detection_enabled() const {
        return flags.collision_detection_enabled != 0u ? IVP_TRUE : IVP_FALSE;
    }

    void enable_collision_detection(IVP_BOOL enabled = IVP_TRUE) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectEnableCollision, this, enabled);
    }

protected:
    // Retail slot 1. The original method builds a matrix and dispatches slot 2,
    // so preserving virtuality is required even for ordinary matrix updates.
    virtual void set_new_quat_object_f_core(
        const IVP_U_Quat *rotation,
        const IVP_U_Point *translation) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectSetQuatCoreFromObject,
            this, rotation, translation);
    }
    ~IVP_Real_Object() override = default;

public:
    // Retail slot 2. This declaration must follow the protected quaternion
    // overload above: MSVC assigns new virtual slots in declaration order.
    virtual void set_new_m_object_f_core(const IVP_U_Matrix *matrix) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectSetMatrixCoreFromObject,
            this, matrix);
    }

    void add_listener_collision(IVP_Listener_Collision *listener) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectAddCollisionListener,
            this, listener);
    }
    void remove_listener_collision(IVP_Listener_Collision *listener) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectRemoveCollisionListener,
            this, listener);
    }
    void add_listener_object(IVP_Listener_Object *listener);
    void remove_listener_object(IVP_Listener_Object *listener);
    void insert_anchor(IVP_Anchor *anchor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectInsertAnchor, this, anchor);
    }
    void remove_anchor(IVP_Anchor *anchor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectRemoveAnchor, this, anchor);
    }
    void revive_object_for_simulation() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectRevive, this);
    }
    IVP_BOOL disable_simulation();
    // Mirrors the exact deleting helper used by physics_RT. Only call this for
    // engine-owned objects whose owning gameplay path authorizes deletion.
    void destroy() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectDestroy, this);
    }
    void calc_m_core_f_object(IVP_U_Matrix *matrix) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectCalculateCoreFromObject,
            this, matrix);
    }
    void get_m_world_f_object_AT(IVP_U_Matrix *matrix) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectGetCurrentMatrix,
            const_cast<IVP_Real_Object *>(this), matrix);
    }
    void calc_at_matrix(IVP_Time time, IVP_U_Matrix *matrix) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectCalculateMatrixAt,
            const_cast<IVP_Real_Object *>(this), time, matrix);
    }
    int get_collision_check_reference_count() {
        return BML::IVP::ABI::InvokeThis<int>(
            BML::IVP::ABI::Address::RealObjectCollisionReferenceCount, this);
    }
    void convert_to_phantom(const IVP_Template_Phantom *configuration) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectConvertToPhantom,
            this, configuration);
    }
    void async_add_speed_object_ws(const IVP_U_Float_Point *speed) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectAddSpeedWorld, this, speed);
    }
    void async_add_rot_speed_object_cs(const IVP_U_Float_Point *speed) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectAddAngularSpeedCore, this, speed);
    }
    void async_push_object_ws(const IVP_U_Point *position,
                              const IVP_U_Float_Point *impulse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectPushWorld,
            this, position, impulse);
    }
    void ensure_in_simulation() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectEnsureSimulation, this);
    }
    void init_object_core(IVP_Environment *targetEnvironment,
                          const IVP_Template_Real_Object *configuration) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectInitializeCore,
            this, targetEnvironment, configuration);
    }
    IVP_Real_Object *to_nonconst() const {
        return const_cast<IVP_Real_Object *>(this);
    }
    void calc_at_quaternion(IVP_Time time, IVP_U_Quat *rotation,
                            IVP_U_Point *position) const {
        IVP_U_Matrix matrix;
        calc_at_matrix(time, &matrix);
        rotation->set_quaternion(&matrix);
        position->set(&matrix.vv);
    }
    void get_quat_world_f_object_AT(IVP_U_Quat *rotation,
                                    IVP_U_Point *position) const {
        IVP_U_Matrix matrix;
        get_m_world_f_object_AT(&matrix);
        rotation->set_quaternion(&matrix);
        position->set(&matrix.vv);
    }
    void get_geom_center_world_space(IVP_U_Point *center) const {
        center->set(physical_core->get_position_PSI());
    }
    IVP_FLOAT get_geom_radius() const {
        return physical_core->upper_limit_radius;
    }
    IVP_FLOAT get_geom_center_speed() const {
        return static_cast<IVP_FLOAT>(physical_core->speed.real_length());
    }
    void get_geom_center_speed_vec(IVP_U_Point *worldSpeed) const {
        worldSpeed->set(&physical_core->speed);
    }
    void change_nocoll_group_ident(const char *identifier) {
        if (!identifier) {
            nocoll_group_ident[0] = '\0';
            return;
        }
        std::strncpy(nocoll_group_ident, identifier,
                     sizeof(nocoll_group_ident));
        nocoll_group_ident[sizeof(nocoll_group_ident) - 1] = '\0';
    }
    void change_fast_piling_allowed(IVP_BOOL allowed) {
        friction_core->set_fast_piling_allowed(allowed);
    }
    void change_mass(IVP_FLOAT mass) { physical_core->set_mass(mass); }
    void ensure_in_simulation_now() {
        if (physical_core->movement_state == IVP_MT_NOT_SIM)
            revive_object_for_simulation();
    }
    void reset_time(IVP_Time offset) {
        hull_manager.last_vpsi_time -= offset;
    }
    void recheck_collision_filter();
    void force_grow_friction_system();
protected:
    void unlink_contact_points(IVP_BOOL silent) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectUnlinkContactPoints,
            this, silent);
    }
    void recalc_exact_mindists_of_object();
    void recalc_invalid_mindists_of_object();
    void update_exact_mindist_events_of_object() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectUpdateExactMindistEvents,
            this);
    }

public:
    // Defined after the complete Environment/mindist and Simulation Unit
    // types. Both operations span multiple engine-owned subsystems.
    void beam_object_to_new_position(
        const IVP_U_Quat *rotation, const IVP_U_Point *position,
        IVP_BOOL optimizeForRepeatedCalls = IVP_FALSE);
    void change_unmovable_flag(IVP_BOOL unmovable);
    void delete_and_check_vicinity();
    void recompile_values_changed() {
        physical_core->values_changed_recalc_redundants();
    }
    void recompile_material_changed() {
        physical_core->values_changed_recalc_redundants();
    }
    void unlink_contact_points_for_object(IVP_Real_Object *otherObject);
    void do_radar_checking(IVP_Radar *radar);
    void set_pinned(IVP_BOOL pinned) {
        physical_core->flags =
            (physical_core->flags & ~(0x3u << 8u)) |
            ((static_cast<std::uint32_t>(pinned) & 0x3u) << 8u);
        physical_core->speed.set_to_zero();
        physical_core->rot_speed.set_to_zero();
        if (pinned == IVP_TRUE) {
            const IVP_BOOL collisionEnabled =
                is_collision_detection_enabled();
            enable_collision_detection(IVP_FALSE);
            physical_core->inv_rot_inertia.set_to_zero();
            physical_core->inv_rot_inertia.hesse_val = 0.0f;
            physical_core->speed_change.set_to_zero();
            physical_core->rot_speed_change.set_to_zero();
            enable_collision_detection(collisionEnabled);
        } else {
            const IVP_U_Float_Point *inertia =
                physical_core->get_rot_inertia();
            physical_core->inv_rot_inertia.set(
                1.0f / inertia->k[0], 1.0f / inertia->k[1],
                1.0f / inertia->k[2]);
            physical_core->inv_rot_inertia.hesse_val =
                1.0f / physical_core->get_mass();
        }
    }
    void recalc_core_radius() {
        IVP_FLOAT radius = 0.0f;
        IVP_FLOAT deviation = 0.0f;
        if (get_type() == IVP_POLYGON) {
            IVP_U_Float_Point center;
            center.set_negative(&shift_core_f_object);
            surface_manager->get_radius_and_radius_dev_to_given_center(
                &center, &radius, &deviation);
        } else if (get_type() == IVP_BALL) {
            radius = deviation = static_cast<IVP_FLOAT>(
                shift_core_f_object.real_length());
        }
        radius += extra_radius;
        physical_core->upper_limit_radius =
            std::max(physical_core->upper_limit_radius, radius);
        physical_core->max_surface_deviation =
            std::max(physical_core->max_surface_deviation, deviation);
    }
    void set_extra_radius(IVP_DOUBLE radius) {
        const IVP_FLOAT oldRadius = extra_radius;
        extra_radius = static_cast<IVP_FLOAT>(radius);
        if (extra_radius > oldRadius) {
            recalc_core_radius();
            return;
        }
        physical_core->upper_limit_radius = 0.0f;
        physical_core->max_surface_deviation = 0.0f;
        for (int index = physical_core->objects.len() - 1;
             index >= 0; --index)
            physical_core->objects.element_at(index)->recalc_core_radius();
    }
    void set_new_surface_manager(IVP_SurfaceManager *newSurfaceManager) {
        const IVP_BOOL collisionEnabled = is_collision_detection_enabled();
        enable_collision_detection(IVP_FALSE);
        surface_manager = newSurfaceManager;
        IVP_FLOAT radius = 0.0f;
        IVP_FLOAT deviation = 0.0f;
        if (get_type() == IVP_POLYGON) {
            IVP_U_Float_Point center;
            center.set_negative(&shift_core_f_object);
            surface_manager->get_radius_and_radius_dev_to_given_center(
                &center, &radius, &deviation);
        }
        radius += extra_radius;
        physical_core->upper_limit_radius =
            std::max(physical_core->upper_limit_radius, radius);
        physical_core->max_surface_deviation =
            std::max(physical_core->max_surface_deviation, deviation);
        enable_collision_detection(collisionEnabled);
    }
    void delete_silently() { destroy(); }
};

struct IVP_Ball : IVP_Real_Object {
    IVP_FLOAT get_radius() const { return get_extra_radius(); }
};
struct IVP_Polygon : IVP_Real_Object {};

inline void IVP_Core::reset_time(IVP_Time offset) {
    time_of_last_psi -= offset;
    for (int index = objects.len() - 1; index >= 0; --index)
        objects.element_at(index)->reset_time(offset);
}

inline void IVP_Core::ensure_all_core_objs_in_simulation() {
    for (int index = objects.len() - 1; index >= 0; --index)
        objects.element_at(index)->ensure_in_simulation();
}

inline void IVP_Core::ensure_all_core_objs_in_simulation_now() {
    for (int index = objects.len() - 1; index >= 0; --index)
        objects.element_at(index)->ensure_in_simulation_now();
}

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_U_Min_List) == 0x14);
static_assert(sizeof(IVP_Hull_Manager_Base_Gradient) == 0x20);
static_assert(sizeof(IVP_Hull_Manager_Base) == 0x38);
static_assert(offsetof(IVP_Hull_Manager_Base, sorted_synapses) == 0x20);
static_assert(sizeof(IVP_Hull_Manager) == 0x38);
static_assert(sizeof(IVP_Object) == 0x1C);
static_assert(offsetof(IVP_Object, object_type) == 0x04);
static_assert(offsetof(IVP_Object, next_in_cluster) == 0x08);
static_assert(offsetof(IVP_Object, father_cluster) == 0x10);
static_assert(offsetof(IVP_Object, name) == 0x14);
static_assert(offsetof(IVP_Object, environment) == 0x18);
static_assert(sizeof(IVP_Cluster) == 0x20);
static_assert(BML_IvpClusterLayoutCheck::objects == 0x1C);
static_assert(sizeof(IVP_Real_Object_Fast_Static) == 0x40);
static_assert(sizeof(IVP_Real_Object_Flags) == 0x04);
static_assert(sizeof(IVP_Real_Object_Fast) == 0x88);
static_assert(sizeof(IVP_Real_Object) == 0xB8);
static_assert(sizeof(IVP_Ball) == 0xB8);
static_assert(sizeof(IVP_Polygon) == 0xB8);
static_assert(offsetof(IVP_Real_Object_Fast_Static,
                       shift_core_f_object) == 0x30);
static_assert(offsetof(IVP_Real_Object_Fast, cache_object) == 0x40);
static_assert(offsetof(IVP_Real_Object_Fast, hull_manager) == 0x48);
static_assert(offsetof(IVP_Real_Object_Fast, flags) == 0x80);
static_assert(offsetof(IVP_Real_Object, cache_object) == 0x40);
static_assert(offsetof(IVP_Real_Object, flags) == 0x80);
static_assert(offsetof(IVP_Real_Object, anchors) == 0x88);
static_assert(offsetof(IVP_Real_Object, surface_manager) == 0x8C);
static_assert(offsetof(IVP_Real_Object, nocoll_group_ident) == 0x90);
static_assert(offsetof(IVP_Real_Object, l_default_material) == 0x98);
static_assert(offsetof(IVP_Real_Object, physical_core) == 0xA4);
static_assert(offsetof(IVP_Real_Object, original_core) == 0xAC);
static_assert(offsetof(IVP_Real_Object, client_data) == 0xB0);
#endif

#endif // BML_IVP_OBJECT_H
