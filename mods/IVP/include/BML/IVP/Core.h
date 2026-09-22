#ifndef BML_IVP_CORE_H
#define BML_IVP_CORE_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstdint>

class IVP_Constraint_Car_Object;
class IVP_Controller;
class IVP_Environment;
class IVP_Event_Sim;
class IVP_Friction_Hash;
class IVP_Friction_Info_For_Core;
class IVP_Friction_System;
class IVP_Core;
class IVP_Core_Merged;
struct IVP_Hull_Manager_Base;
class IVP_Real_Object;
class IVP_Sim_Units_Manager;
class IVP_Simulation_Unit;
class IVP_U_Point_4;

class IVP_Vec_PCore : public IVP_U_Float_Point {
public:
    IVP_Vec_PCore(const IVP_Core *core,
                  const IVP_U_Float_Point *directionWorldSpace);
};

union IVP_Core_Friction_Info {
    struct {
        IVP_Friction_Hash *l_friction_info_hash;
    } for_unmoveables;
    struct {
        IVP_Friction_Info_For_Core *moveable_core_friction_info;
    } for_moveables;

    // Raw aliases keep the short Ballance reconstruction call sites source
    // compatible while the historical public union views remain available.
    IVP_Friction_Hash *l_friction_info_hash;
    IVP_Friction_Info_For_Core *moveable_core_friction_info;
};

class IVP_Old_Sync_Rot_Z {
public:
    IVP_U_Float_Point old_sync_rot_speed;
    IVP_U_Quat old_sync_q_world_f_core_next_psi;
    IVP_BOOL was_pushed_during_i_s;
};

struct IVP_Vector_of_Objects : IVP_U_Vector<IVP_Real_Object> {
    IVP_Vector_of_Objects()
        : IVP_U_Vector<IVP_Real_Object>(
              reinterpret_cast<void **>(elem_buffer), 1) {}

    void reset() {
        elems = reinterpret_cast<void **>(elem_buffer);
        memsize = 1;
    }

    IVP_Real_Object *elem_buffer[1];
};

// Keep the public source hierarchy: retail instructions address these three
// prefixes at exactly +0x00, +0x60 and +0x1B0.  They have no vptr and therefore
// introduce no storage beyond the fields below.
class IVP_Core_Fast_Static {
public:
    union {
        struct {
            IVP_BOOL fast_piling_allowed_flag : 2;
            IVP_BOOL physical_unmoveable : 2;
            IVP_BOOL is_in_wakeup_vec : 2;
            IVP_BOOL rot_inertias_are_equal : 2;
            IVP_BOOL pinned : 2;
        };
        std::uint32_t flags;
    };
    IVP_FLOAT upper_limit_radius;
    IVP_FLOAT max_surface_deviation;
    IVP_Environment *environment;
    IVP_Constraint_Car_Object *car_wheel;
    IVP_U_Float_Hesse rot_inertia;
    IVP_U_Float_Point rot_speed_damp_factor;
    IVP_U_Float_Hesse inv_rot_inertia;
    IVP_FLOAT speed_damp_factor;
    IVP_FLOAT inv_object_diameter;
    IVP_U_Float_Point *spin_clipping;
    IVP_Vector_of_Objects objects;
    IVP_Core_Friction_Info core_friction_info;

    const IVP_U_Point_4 *get_inv_masses() {
        return reinterpret_cast<const IVP_U_Point_4 *>(&inv_rot_inertia);
    }
    IVP_FLOAT get_mass() const { return rot_inertia.hesse_val; }
    const IVP_U_Float_Point *get_rot_inertia() const { return &rot_inertia; }
    const IVP_U_Float_Point *get_inv_rot_inertia() const {
        return &inv_rot_inertia;
    }
    IVP_FLOAT get_inv_mass() const { return inv_rot_inertia.hesse_val; }
};

class IVP_Core_Fast_PSI : public IVP_Core_Fast_Static {
public:
    // VC6 packed these two different enum types into adjacent bytes. Modern
    // MSVC starts a new 32-bit allocation unit when the enum type changes, so
    // spelling them as historical bit-fields would move every later member.
    // Byte storage is the ABI-safe representation of the retail layout.
    std::uint8_t movement_state;
    std::uint8_t temporarily_unmovable;
    std::uint16_t reserved_62;
    std::int16_t impacts_since_last_PSI;
    std::uint16_t reserved_66;
    IVP_Time time_of_last_psi;
    IVP_FLOAT i_delta_time;
    IVP_U_Float_Point rot_speed_change;
    IVP_U_Float_Point speed_change;
    IVP_U_Float_Point rot_speed;
    IVP_U_Float_Point speed;
    std::uint32_t reserved_B4;
    IVP_U_Point pos_world_f_core_last_psi;
    IVP_U_Float_Point delta_world_f_core_psis;
    IVP_U_Quat q_world_f_core_last_psi;
    IVP_U_Quat q_world_f_core_next_psi;
    IVP_U_Matrix m_world_f_core_last_psi;
};

class IVP_Core_Fast : public IVP_Core_Fast_PSI {
public:
    IVP_U_Float_Point rotation_axis_world_space;
    IVP_FLOAT current_speed;
    IVP_FLOAT abs_omega;
    IVP_FLOAT max_surface_rot_speed;
    std::uint32_t reserved_1C4;
};

class IVP_Core : public IVP_Core_Fast {
public:
    // Real-object teardown and core merging delete cores inside physics_RT.
    // A directly constructed public Core therefore has to originate from the
    // same heap even though this class has no virtual table.
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    // The retained five-argument constructor initializes this vector at
    // +0x1c8; the retained initializer used by the reconstructed one-object
    // constructor establishes its valid empty state. The retained complete
    // destructor releases it. Keep only storage here so the host compiler
    // cannot add a second C++ lifetime around those engine-owned paths.
    union {
        IVP_U_Vector<IVP_Controller> controllers_of_core;
    };
    IVP_Core_Merged *merged_core_which_replace_this_core;
    IVP_Simulation_Unit *sim_unit_of_core;
    IVP_Time time_of_calm_reference[2];
    IVP_U_Float_Quat q_world_f_core_calm_reference[2];
    IVP_U_Float_Point position_world_f_core_calm_reference[2];
    union {
        IVP_Core *union_find_father;
    } tmp;
    union {
        IVP_Old_Sync_Rot_Z *old_sync_info;
        std::uint32_t raw;
    } tmp_null;
    std::int32_t mindist_event_already_done;
    std::uint32_t reserved_234;

protected:
    // Retained at RVA 0xD2F0. Ballance link-stripped the small single-object
    // constructor around this helper, but retained every stateful primitive
    // that constructor needs.
    void init(IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreInitialize, this, object);
    }
    explicit IVP_Core(IVP_Real_Object *object) {
        init(object);

        // IVP_Environment is incomplete here because Object.h includes Core.h.
        // Ballance fixes sim_units_manager at +0x08; Environment.h asserts that
        // offset. Reproduce the neighboring constructor's second and final
        // operation through the retained manager entry point (RVA 0x11EF0).
        auto *simUnitsManager =
            *reinterpret_cast<IVP_Sim_Units_Manager **>(
                reinterpret_cast<std::byte *>(environment) + 0x08);
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitsManagerAdd,
            simUnitsManager, sim_unit_of_core);
    }

public:
    IVP_Core(IVP_Real_Object *object,
             const IVP_U_Quat *worldFromObject,
             const IVP_U_Point *position,
             IVP_BOOL physicalUnmoveable,
             IVP_BOOL enablePilingOptimization) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreConstruct, this, object,
            worldFromObject, position, physicalUnmoveable,
            enablePilingOptimization);
    }
    ~IVP_Core() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreDestruct, this);
    }

    IVP_BOOL is_physical_unmoveable() const {
        return static_cast<IVP_BOOL>((flags >> 2u) & 0x3u);
    }
    IVP_Environment *get_environment() { return environment; }
    const IVP_Environment *get_environment() const { return environment; }
    IVP_U_Float_Point *get_speed() { return &speed; }
    const IVP_U_Float_Point *get_speed() const { return &speed; }
    IVP_U_Float_Point *get_rot_speed() { return &rot_speed; }
    const IVP_U_Float_Point *get_rot_speed() const { return &rot_speed; }
    const IVP_U_Matrix *get_m_world_f_core_PSI() {
        return &m_world_f_core_last_psi;
    }
    const IVP_U_Matrix *get_m_world_f_core_PSI() const {
        return &m_world_f_core_last_psi;
    }
    const IVP_U_Point *get_position_PSI() {
        return m_world_f_core_last_psi.get_position();
    }
    const IVP_U_Point *get_position_PSI() const {
        return m_world_f_core_last_psi.get_position();
    }

    // These two methods were inline in the matching IVP source. Their field
    // offsets and arithmetic are independently confirmed by the retail DLL.
    void inline_calc_at_position(IVP_Time time, IVP_U_Point *position) const {
        const IVP_DOUBLE delta = time - time_of_last_psi;
        position->add_multiple(&pos_world_f_core_last_psi,
                               &delta_world_f_core_psis, delta);
    }
    void inline_calc_at_quaternion(IVP_Time time, IVP_U_Quat *rotation) const {
        const IVP_DOUBLE delta = time - time_of_last_psi;
        rotation->set_interpolate_smoothly(&q_world_f_core_last_psi,
                                           &q_world_f_core_next_psi,
                                           delta * i_delta_time);
    }

    IVP_BOOL revive_simulation_core() {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::CoreRevive, this);
    }
    void freeze_simulation_core() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreFreezeSimulation, this);
    }
    IVP_DOUBLE calc_virt_mass(const IVP_U_Float_Point *point,
                              const IVP_U_Float_Point *direction) const {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::CoreCalculateVirtualMass,
            const_cast<IVP_Core *>(this), point, direction);
    }
    void get_surface_speed_on_test(
        const IVP_U_Float_Point *point,
        const IVP_U_Float_Point *centerSpeed,
        const IVP_U_Float_Point *rotationalSpeed,
        IVP_U_Float_Point *result) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreGetSurfaceSpeedOnTest,
            const_cast<IVP_Core *>(this), point, centerSpeed,
            rotationalSpeed, result);
    }
    static void get_diff_surface_speed_of_two_cores(
        const IVP_Core *left, const IVP_Core *right,
        const IVP_U_Float_Point *leftPoint,
        const IVP_U_Float_Point *rightPoint,
        IVP_U_Float_Point *result) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::CoreGetDifferentialSurfaceSpeed,
            left, right, leftPoint, rightPoint, result);
    }
    static void get_diff_surface_speed_of_two_cores_on_test(
        const IVP_Core *left, const IVP_Core *right,
        const IVP_U_Float_Point *leftPoint,
        const IVP_U_Float_Point *rightPoint,
        const IVP_U_Float_Point *leftSpeed,
        const IVP_U_Float_Point *leftRotationalSpeed,
        const IVP_U_Float_Point *rightSpeed,
        const IVP_U_Float_Point *rightRotationalSpeed,
        IVP_U_Float_Point *result) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::CoreGetDifferentialSurfaceSpeedOnTest,
            left, right, leftPoint, rightPoint, leftSpeed,
            leftRotationalSpeed, rightSpeed, rightRotationalSpeed, result);
    }
    IVP_DOUBLE get_energy_on_test(const IVP_U_Float_Point *linearSpeed,
                                  const IVP_U_Float_Point *rotationalSpeed) {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::CoreGetEnergyOnTest,
            this, linearSpeed, rotationalSpeed);
    }
    void set_radius(IVP_FLOAT radius, IVP_FLOAT deviation) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreSetRadius, this, radius, deviation);
    }
    void calc_at_matrix(IVP_Time time, IVP_U_Matrix *matrix) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreCalculateMatrixAt,
            const_cast<IVP_Core *>(this), time, matrix);
    }
    void abort_all_async_pushes() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreAbortAsyncPushes, this);
    }
    void global_damp_core(IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreGlobalDamp, this, factor);
    }
    void damp_object(IVP_DOUBLE virtualDeltaTime,
                     const IVP_U_Float_Point *rotationFactor,
                     IVP_DOUBLE speedFactor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreDampObject,
            this, virtualDeltaTime, rotationFactor, speedFactor);
    }
    void get_surface_speed(const IVP_U_Float_Point *point,
                           IVP_U_Float_Point *result) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreGetSurfaceSpeed,
            const_cast<IVP_Core *>(this), point, result);
    }
    void async_push_core(const IVP_U_Float_Point *point,
                         const IVP_U_Float_Point *impulse,
                         const IVP_U_Float_Point *angularImpulse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreAsyncPush,
            this, point, impulse, angularImpulse);
    }
    void push_core(const IVP_U_Float_Point *point,
                   const IVP_U_Float_Point *impulse,
                   const IVP_U_Float_Point *angularImpulse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CorePush,
            this, point, impulse, angularImpulse);
    }
    void async_push_core_ws(const IVP_U_Point *position,
                            const IVP_U_Float_Point *impulse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreAsyncPushWorld,
            this, position, impulse);
    }
    void test_push_core(const IVP_U_Float_Point *position,
                        const IVP_U_Float_Point *impulseCore,
                        const IVP_U_Float_Point *impulseWorld,
                        IVP_U_Float_Point *speedChange,
                        IVP_U_Float_Point *rotationalChange) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreTestPush,
            const_cast<IVP_Core *>(this), position, impulseCore,
            impulseWorld, speedChange, rotationalChange);
    }
    void test_rot_push_core_multiple_cs(
        const IVP_U_Float_Point *angularImpulse, IVP_DOUBLE factor,
        IVP_U_Float_Point *rotationalChange) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreTestRotationalPushCore,
            this, angularImpulse, factor, rotationalChange);
    }
    void rot_push_core_cs(const IVP_U_Float_Point *angularImpulse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreRotationalPushCore,
            this, angularImpulse);
    }
    void commit_all_async_pushes() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreCommitAsyncPushes, this);
    }
    void calc_next_PSI_matrix_zero_speed(class IVP_Event_Sim *event) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreCalculateNextMatrixZeroSpeed,
            this, event);
    }
    void calc_next_PSI_matrix(IVP_U_Vector<IVP_Core> *pending,
                              class IVP_Event_Sim *) {
        pending->add(this);
    }
    void unlink_obj_from_core_and_maybe_destroy(IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreUnlinkObject, this, object);
    }
    void stop_movement_without_collision_recheck() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreStopMovement, this);
    }
    void stop_physical_movement() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreStopPhysicalMovement, this);
    }
    void reset_freeze_check_values() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreResetFreezeCheckValues, this);
    }
    void init_core_for_simulation() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreInitializeForSimulation, this);
    }
    void synchronize_with_rot_z() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreSynchronizeWithRotation, this);
    }
    void undo_synchronize_rot_z() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreUndoSynchronizeRotation, this);
    }
    void transform_PSI_matrizes_core(const IVP_U_Matrix *matrix) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreTransformPsiMatrices, this, matrix);
    }
    void ensure_core_to_be_in_simulation() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreEnsureSimulation, this);
    }
    void ensure_core_in_simulation_delayed() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentAddReviveCore,
            environment, this);
    }
    void ensure_all_core_objs_in_simulation();
    void ensure_all_core_objs_in_simulation_now();
    void rem_core_controller(IVP_Controller *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreRemoveController, this, controller);
    }
    void add_core_controller(IVP_Controller *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreAddController, this, controller);
    }
    void add_friction_info(IVP_Friction_Info_For_Core *info) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreAddFrictionInfo, this, info);
    }
    void delete_friction_info(IVP_Friction_Info_For_Core *info) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreDeleteFrictionInfo, this, info);
    }
    void unlink_friction_info(IVP_Friction_Info_For_Core *info) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreUnlinkFrictionInfo, this, info);
    }
    IVP_Friction_Info_For_Core *get_friction_info(
        IVP_Friction_System *system) {
        return BML::IVP::ABI::InvokeThis<IVP_Friction_Info_For_Core *>(
            BML::IVP::ABI::Address::CoreGetFrictionInfo, this, system);
    }
    IVP_Friction_Info_For_Core *moveable_core_has_friction_info() {
        return BML::IVP::ABI::InvokeThis<IVP_Friction_Info_For_Core *>(
            BML::IVP::ABI::Address::CoreMoveableHasFrictionInfo, this);
    }
    IVP_BOOL grow_friction_system() {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::CoreGrowFrictionSystem, this);
    }
    IVP_Core *union_find_get_father() {
        return BML::IVP::ABI::InvokeThisOr<IVP_Core *>(
            BML::IVP::ABI::Address::CoreUnionFindFather, this,
            [this]() {
                IVP_Core *object = this;
                IVP_Core *root = object;
                while (object) {
                    root = object;
                    object = object->tmp.union_find_father;
                }
                return root;
            });
    }
    void fire_event_object_frozen() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreFireObjectFrozen, this);
    }
    IVP_Movement_Type calc_movement_state(IVP_Time time) {
        return BML::IVP::ABI::InvokeThis<IVP_Movement_Type>(
            BML::IVP::ABI::Address::CoreCalculateMovementState,
            this, time);
    }
    void update_exact_mindist_events_of_core() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CoreUpdateExactMindistEvents, this);
    }
    void reset_time(IVP_Time offset);

    // The neighboring implementation is intentionally disabled by an
    // unconditional return. Preserve that early-IVP behavior without
    // importing the obsolete merged-core machinery behind it.
    void create_collision_merged_core_with(IVP_Core *otherCore) {
        (void) otherCore;
    }
    void set_matrizes_and_speed(
        IVP_Core_Merged *templateCore, IVP_U_Matrix *coreFromCoreOut) = delete;

    // Selectively reconstructed after the complete Environment/Object/Mindist
    // declarations. Their contact operations dispatch retained retail bodies;
    // only the short traversal and inlined memory transaction remain local.
    void revive_adjacent_to_unmoveable();
    void values_changed_recalc_redundants();

    // Source-defined inline operations. Their definitions follow the complete
    // Environment/Object declarations in those headers.
    void clip_velocity(IVP_U_Float_Point *velocity,
                       IVP_U_Float_Point *angularVelocity);
    void apply_velocity_limit();

    void set_fast_piling_allowed(IVP_BOOL allowed) {
        flags = (flags & ~0x3u) |
                (static_cast<std::uint32_t>(allowed) & 0x3u);
    }
    IVP_BOOL fast_piling_allowed() {
        return static_cast<IVP_BOOL>(flags & 0x3u);
    }
    IVP_DOUBLE calc_virt_mass_worst_case(
        const IVP_U_Float_Point *corePoint) const {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::CoreCalculateVirtualMassWorstCase,
            const_cast<IVP_Core *>(this), corePoint);
    }
    IVP_DOUBLE calc_correct_virt_mass(
        const IVP_U_Float_Point *corePoint,
        const IVP_U_Float_Point *directionCore,
        const IVP_U_Float_Point *directionWorld) const {
        if (((flags >> 8u) & 0x3u) != 0u)
            return 1.0;
        IVP_U_Float_Point centerSpeed;
        IVP_U_Float_Point rotationalSpeed;
        test_push_core(corePoint, directionCore, directionWorld,
                       &centerSpeed, &rotationalSpeed);
        IVP_U_Float_Point worldSpeed;
        get_surface_speed_on_test(corePoint, &centerSpeed,
                                  &rotationalSpeed, &worldSpeed);
        return 1.0 / worldSpeed.real_length();
    }
    void get_surface_speed_ws(const IVP_U_Point *worldPosition,
                              IVP_U_Float_Point *worldSpeed) {
        IVP_U_Float_Point angularWorld;
        m_world_f_core_last_psi.vmult3(&rot_speed, &angularWorld);
        IVP_U_Float_Point relative;
        relative.subtract(worldPosition, get_position_PSI());
        IVP_U_Float_Point cross;
        cross.inline_calc_cross_product(&angularWorld, &relative);
        worldSpeed->add(&speed, &cross);
    }
    void center_push_core_multiple_ws(const IVP_U_Float_Point *impulse,
                                      IVP_DOUBLE factor = 1.0) {
        speed.add_multiple(impulse, factor * get_inv_mass());
    }
    void async_center_push_core_multiple_ws(
        const IVP_U_Float_Point *impulse, IVP_DOUBLE factor = 1.0) {
        speed_change.add_multiple(impulse, factor * get_inv_mass());
    }
    void push_core_ws(const IVP_U_Point *worldPosition,
                      const IVP_U_Float_Point *worldImpulse) {
        IVP_U_Float_Point relative;
        relative.subtract(worldPosition, get_position_PSI());
        IVP_U_Float_Point angularWorld;
        angularWorld.calc_cross_product(&relative, worldImpulse);
        IVP_U_Float_Point angularCore;
        m_world_f_core_last_psi.vimult3(&angularWorld, &angularCore);
        angularCore.set_pairwise_mult(&angularCore, get_inv_rot_inertia());
        rot_speed.add(&angularCore);
        speed.add_multiple(worldImpulse, get_inv_mass());
    }
    void async_rot_push_core_multiple_ws(
        const IVP_U_Float_Point *angularImpulse,
        IVP_DOUBLE factor = 1.0) {
        IVP_U_Float_Point coreImpulse;
        m_world_f_core_last_psi.vimult3(angularImpulse, &coreImpulse);
        IVP_U_Float_Point change;
        change.set_pairwise_mult(&coreImpulse, get_inv_rot_inertia());
        rot_speed_change.add_multiple(&change, factor);
    }
    void rot_push_core_multiple_ws(const IVP_U_Float_Point *angularImpulse,
                                   IVP_DOUBLE factor = 1.0) {
        IVP_U_Float_Point coreImpulse;
        m_world_f_core_last_psi.vimult3(angularImpulse, &coreImpulse);
        IVP_U_Float_Point change;
        change.set_pairwise_mult(&coreImpulse, get_inv_rot_inertia());
        rot_speed.add_multiple(&change, factor);
    }
    void async_rot_push_core_multiple_cs(
        const IVP_U_Float_Point *axis, IVP_DOUBLE impulse = 1.0) {
        IVP_U_Float_Point change;
        change.set_pairwise_mult(axis, get_inv_rot_inertia());
        rot_speed_change.add_multiple(&change, impulse);
    }
    void rot_push_core_multiple_cs(const IVP_U_Float_Point *axis,
                                   IVP_DOUBLE impulse = 1.0) {
        IVP_U_Float_Point change;
        change.set_pairwise_mult(axis, get_inv_rot_inertia());
        rot_speed.add_multiple(&change, impulse);
    }
    IVP_DOUBLE get_rot_inertia_cs(const IVP_U_Float_Point *axis) {
        IVP_U_Float_Point projected;
        projected.set_pairwise_mult(axis, get_rot_inertia());
        return projected.real_length();
    }
    IVP_DOUBLE get_rot_speed_cs(const IVP_U_Float_Point *axis) {
        return rot_speed.dot_product(axis);
    }
    void set_rotation_inertia(const IVP_U_Float_Point *value) {
        constexpr IVP_FLOAT maximum = 3.402823466e+38F;
        rot_inertia.set(std::min(value->k[0], maximum),
                        std::min(value->k[1], maximum),
                        std::min(value->k[2], maximum));
        calc_calc();
    }
    void calc_calc() {
        using Function = void (__thiscall *)(IVP_Core *);
        if (Function function = BML::IVP::ABI::Resolve<Function>(
                BML::IVP::ABI::Address::CoreCalculateRedundantValues)) {
            function(this);
            return;
        }

        inv_rot_inertia.set(1.0f / rot_inertia.k[0],
                            1.0f / rot_inertia.k[1],
                            1.0f / rot_inertia.k[2]);
        inv_rot_inertia.hesse_val = 1.0f / get_mass();
        IVP_U_Float_Point differences(
            inv_rot_inertia.k[1] - inv_rot_inertia.k[2],
            inv_rot_inertia.k[2] - inv_rot_inertia.k[0],
            inv_rot_inertia.k[0] - inv_rot_inertia.k[1]);
        const bool equal = differences.quad_length() <
                           inv_rot_inertia.quad_length() * 0.01;
        flags = (flags & ~(0x3u << 6u)) |
                ((equal ? 1u : 0u) << 6u);
        inv_object_diameter = 0.5f / upper_limit_radius;
    }
    void set_mass(IVP_FLOAT mass) {
        const IVP_DOUBLE factor = mass / get_mass();
        rot_inertia.mult(factor);
        rot_inertia.hesse_val = mass;
        calc_calc();
    }
    void core_add_link_to_obj(IVP_Real_Object *object) {
        objects.add(object);
    }
    void core_plausible_check() {}
    void rot_speed_plausible_check(const IVP_U_Float_Point *) {}
    void debug_vec_movement_state() {}
    void debug_out_movement_vars() {}
    void unmovable_core_debug_friction_hash() {}
};

inline IVP_Vec_PCore::IVP_Vec_PCore(
    const IVP_Core *core,
    const IVP_U_Float_Point *directionWorldSpace) {
    core->m_world_f_core_last_psi.vimult3(directionWorldSpace, this);
}

// The retail IDB and the neighboring internal declaration both describe this
// solver as one Core pointer. Ballance retains calc_next_PSI_matrix and the two
// batch commit entries; set_transformation is selectively reconstructed after
// Environment, Real Object and mindist types are complete.
class IVP_Calc_Next_PSI_Solver {
private:
    IVP_Core *core;

    void calc_psi_rotation_axis(const IVP_U_Quat *coreFromCore) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CalcNextPsiRotationAxis,
            this, coreFromCore);
    }

public:
    explicit IVP_Calc_Next_PSI_Solver(IVP_Core *coreIn) : core(coreIn) {}

    void set_transformation(const IVP_U_Quat *rotation,
                            const IVP_U_Point *position,
                            IVP_BOOL optimizeForRepeatedCalls);

    void calc_next_PSI_matrix(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Hull_Manager_Base> *activeHullManagers) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CalcNextPsiMatrix,
            this, event, activeHullManagers);
    }

    static void commit_all_calc_next_PSI_matrix(
        IVP_Environment *environment,
        IVP_U_Vector<IVP_Core> *cores,
        IVP_U_Vector<IVP_Hull_Manager_Base> *activeHullManagers) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::CalcNextPsiCommitAll,
            environment, cores, activeHullManagers);
    }

    static void commit_one_hull_manager(
        IVP_Environment *environment,
        IVP_U_Vector<IVP_Hull_Manager_Base> *activeHullManagers);

    static void commit_all_hull_managers(
        IVP_Environment *environment,
        IVP_U_Vector<IVP_Hull_Manager_Base> *activeHullManagers) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::CalcNextPsiCommitAllHulls,
            environment, activeHullManagers);
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Vector_of_Objects) == 0x0C);
static_assert(sizeof(IVP_Vec_PCore) == 0x10);
static_assert(sizeof(IVP_Core_Fast_Static) == 0x60);
static_assert(offsetof(IVP_Core_Fast_Static, flags) == 0x00);
static_assert(offsetof(IVP_Core_Fast_Static, rot_inertia) == 0x14);
static_assert(offsetof(IVP_Core_Fast_Static, inv_rot_inertia) == 0x34);
static_assert(offsetof(IVP_Core_Fast_Static, objects) == 0x50);
static_assert(offsetof(IVP_Core_Fast_Static, core_friction_info) == 0x5C);
static_assert(sizeof(IVP_Core_Fast_PSI) == 0x1A8);
static_assert(sizeof(IVP_Core_Fast) == 0x1C8);
static_assert(sizeof(IVP_Core) == 0x238);
static_assert(sizeof(IVP_Calc_Next_PSI_Solver) == 0x04);
static_assert(offsetof(IVP_Core, upper_limit_radius) == 0x04);
static_assert(offsetof(IVP_Core, rot_inertia) == 0x14);
static_assert(offsetof(IVP_Core, speed_damp_factor) == 0x44);
static_assert(offsetof(IVP_Core, movement_state) == 0x60);
static_assert(offsetof(IVP_Core, rot_speed_change) == 0x74);
static_assert(offsetof(IVP_Core, speed_change) == 0x84);
static_assert(offsetof(IVP_Core, rot_speed) == 0x94);
static_assert(offsetof(IVP_Core, speed) == 0xA4);
static_assert(offsetof(IVP_Core, controllers_of_core) == 0x1C8);
static_assert(offsetof(IVP_Core, sim_unit_of_core) == 0x1D4);
static_assert(offsetof(IVP_Core, tmp) == 0x228);
#endif

#endif // BML_IVP_CORE_H
