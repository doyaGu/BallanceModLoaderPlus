#ifndef BML_IVP_CONTROLLER_H
#define BML_IVP_CONTROLLER_H

#include "BML/IVP/Environment.h"
#include "BML/IVP/Cache.h"
#include "BML/IVP/Reaction.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

enum IVP_CONTROLLER_PRIORITY : std::int32_t {
    IVP_CP_NONE = -1,
    IVP_CP_STATIC_FRICTION = 0,
    IVP_CP_CONSTRAINTS_MIN = 400,
    IVP_CP_CONSTRAINTS = 405,
    IVP_CP_CONSTRAINTS_MAX = 410,
    IVP_CP_STIFF_SPRINGS = 460,
    IVP_CP_FLOATING = 470,
    IVP_CP_MOTION = 500,
    IVP_CP_DYNAMIC_FRICTION = 600,
    IVP_CP_GRAVITY = 1000,
    IVP_CP_SPRING = 1400,
    IVP_CP_ACTUATOR = 1500,
    IVP_CP_FORCEFIELDS = 1600,
    IVP_CP_ENERGY_FRICTION = 2000,
};

class IVP_Event_Sim {
public:
    IVP_DOUBLE delta_time;
    IVP_DOUBLE i_delta_time;
    IVP_Environment *environment;
    IVP_Simulation_Unit *sim_unit;

    IVP_Event_Sim(IVP_Environment *env, IVP_DOUBLE time)
        : delta_time(time),
          i_delta_time(time > 1.0e-10 ? 1.0 / time : 1.0e10),
          environment(env), sim_unit(nullptr) {}
    explicit IVP_Event_Sim(IVP_Environment *env)
        : delta_time(env ? env->get_delta_PSI_time() : 0.0),
          i_delta_time(env ? env->get_inv_delta_PSI_time() : 1.0e10),
          environment(env), sim_unit(nullptr) {}
};

// This is the seven-slot controller interface used by the retail DLL. A newer
// source tree's get_controller_name virtual would shift the destructor slot.
class IVP_Controller {
public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    virtual void core_is_going_to_be_deleted_event(IVP_Core *) {}
    virtual IVP_DOUBLE get_minimum_simulation_frequency() { return 1.0; }
    virtual IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() = 0;
    virtual void reset_time(IVP_Time offset) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ControllerResetTime, this,
            [] {}, offset);
    }
    virtual void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *coreList) = 0;
    virtual IVP_CONTROLLER_PRIORITY get_controller_priority() = 0;
    // The nearby revision made this virtual. Ballance's seven-slot table has
    // no name slot, so retain the source-facing query as a non-virtual method.
    const char *get_controller_name() { return "sys:unknown"; }
    virtual ~IVP_Controller() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ControllerDestruct, this);
    }
};

class IVP_Controller_Dependent : public IVP_Controller {};

class IVP_Controller_Independent : public IVP_Controller {
public:
    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        static IVP_U_Vector<IVP_Core> empty;
        return &empty;
    }
};

class IVP_Template_Controller_Floating {
public:
    IVP_Template_Controller_Floating()
        : max_repulsive_force(std::numeric_limits<IVP_FLOAT>::max()),
          max_adhesive_force(std::numeric_limits<IVP_FLOAT>::max()),
          target_distance(0.0f), current_distance(0.0f) {
        position_os.set_to_zero();
        ray_direction_ws.set(0.0, 1.0, 0.0);
    }

    void set_position_ws(
        IVP_Real_Object *object, const IVP_U_Point *position_ws) {
        IVP_Cache_Object *cache = object->get_cache_object_no_lock();
        cache->transform_position_to_object_coords(position_ws, &position_os);
    }
    void set_ray_direction_ws(
        IVP_Real_Object *, const IVP_U_Point *direction_ws) {
        ray_direction_ws.set(direction_ws);
    }

    IVP_FLOAT max_repulsive_force;
    IVP_FLOAT max_adhesive_force;
    IVP_U_Point position_os;
    IVP_U_Point ray_direction_ws;
    IVP_FLOAT target_distance;
    IVP_FLOAT current_distance;
};

class IVP_Controller_Floating : public IVP_Controller_Independent {
public:
    virtual IVP_RETURN_TYPE do_ray_casting(IVP_Event_Sim *) = 0;

    IVP_Controller_Floating(
        IVP_Real_Object *controlled_object,
        const IVP_Template_Controller_Floating *definition)
        : object(controlled_object),
          max_repulsive_force(definition->max_repulsive_force),
          max_adhesive_force(definition->max_adhesive_force),
          target_distance(definition->target_distance),
          current_distance(definition->current_distance) {
        position_os.set(&definition->position_os);
        ray_direction_ws.set(&definition->ray_direction_ws);
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::ControllerManagerAddToCore,
            this, object->get_core());
    }

    ~IVP_Controller_Floating() override {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::ControllerManagerRemoveFromCore,
            this, object->get_core());
    }

    void set_current_distance(IVP_DOUBLE value) {
        current_distance = value;
    }
    IVP_DOUBLE get_current_distance() { return current_distance; }
    const IVP_U_Float_Point *get_position_os() const {
        return &position_os;
    }
    void set_target_distance(IVP_DOUBLE value) {
        target_distance = value;
    }
    IVP_DOUBLE get_target_distance() { return target_distance; }
    void set_ray_direction_ws(const IVP_U_Float_Point *direction) {
        ray_direction_ws.set(direction);
    }
    const IVP_U_Float_Point *get_ray_direction_ws() {
        return &ray_direction_ws;
    }

protected:
    void do_simulation_controller(
        IVP_Event_Sim *event, IVP_U_Vector<IVP_Core> *) override {
        if (do_ray_casting(event) == IVP_FAULT) {
            return;
        }

        IVP_Cache_Object *cache = object->get_cache_object();
        IVP_U_Point position_ws;
        cache->transform_position_to_world_coords(
            &position_os, &position_ws);

        IVP_Solver_Core_Reaction reaction;
        reaction.init_reaction_solver_translation_ws(
            object->get_core(), nullptr, position_ws,
            &ray_direction_ws, nullptr, nullptr);
        const IVP_DOUBLE response =
            reaction.get_m_velocity_ds_f_impulse_ds()->get_elem(0, 0);
        IVP_DOUBLE impulse =
            ((current_distance - target_distance) * event->i_delta_time -
             reaction.delta_velocity_ds.k[0]) /
            response;
        if (impulse * event->i_delta_time > max_adhesive_force)
            impulse = max_adhesive_force * event->delta_time;
        if (-impulse * event->i_delta_time > max_repulsive_force)
            impulse = -max_repulsive_force * event->delta_time;
        cache->remove_reference();

        IVP_U_Float_Point impulse_ds(impulse, 0.0, 0.0);
        reaction.exert_impulse_dim1(
            object->get_core(), nullptr, impulse_ds);
    }

    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_FLOATING;
    }
    void core_is_going_to_be_deleted_event(IVP_Core *core) override {
        (void)core;
        delete this;
    }

    friend class IVP_Environment;

    IVP_Real_Object *object;
    IVP_FLOAT max_repulsive_force;
    IVP_FLOAT max_adhesive_force;
    IVP_U_Float_Point position_os;
    IVP_U_Float_Point ray_direction_ws;
    IVP_DOUBLE target_distance;
    IVP_DOUBLE current_distance;
};

class IVP_Template_Controller_World_Friction {
public:
    IVP_Template_Controller_World_Friction() {
        desired_speed_ws.set_to_zero();
        desired_rot_speed_cs.set_to_zero();
        const IVP_FLOAT friction = 0.4f * 9.81f;
        friction_value_translation.set(friction, friction, friction);
        friction_value_rotation.set(
            friction * 0.3f, friction * 0.3f, friction * 0.3f);
    }

    IVP_U_Point desired_speed_ws;
    IVP_U_Point desired_rot_speed_cs;
    IVP_U_Point friction_value_translation;
    IVP_U_Point friction_value_rotation;
};

class IVP_Controller_World_Friction
    : public IVP_Controller_Independent {
public:
    IVP_Controller_World_Friction(
        IVP_Real_Object *object,
        const IVP_Template_Controller_World_Friction *definition)
        : real_obj(object) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::ControllerManagerAddToCore,
            this, real_obj->get_core());
        desired_speed_ws.set(&definition->desired_speed_ws);
        desired_rot_speed_cs.set(&definition->desired_rot_speed_cs);
        friction_value_translation.set(
            &definition->friction_value_translation);
        friction_value_rotation.set(&definition->friction_value_rotation);
    }

    ~IVP_Controller_World_Friction() override {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::ControllerManagerRemoveFromCore,
            this, real_obj->get_core());
    }

    void set_desired_speed_ws(IVP_U_Float_Point *speed_ws) {
        desired_speed_ws.set(speed_ws);
    }
    void set_desired_rot_speed_cs(IVP_U_Float_Point *rot_speed_cs) {
        desired_rot_speed_cs.set(rot_speed_cs);
    }
    const IVP_U_Float_Point *get_desired_speed_ws() const {
        return &desired_speed_ws;
    }
    const IVP_U_Float_Point *get_desired_rot_speed_cs() const {
        return &desired_rot_speed_cs;
    }
    void set_friction_value_translation(IVP_U_Point value) {
        friction_value_translation.set(&value);
    }
    void set_friction_value_rotation(IVP_U_Point value) {
        friction_value_rotation.set(&value);
    }
    const IVP_U_Float_Point *get_friction_value_rotation() {
        return &friction_value_rotation;
    }
    const IVP_U_Float_Point *get_friction_value_translation() {
        return &friction_value_translation;
    }

protected:
    void do_simulation_controller(
        IVP_Event_Sim *event, IVP_U_Vector<IVP_Core> *) override {
        IVP_Core *core = real_obj->get_core();

        IVP_U_Float_Point correction_speed_ws;
        correction_speed_ws.subtract(&desired_speed_ws, &core->speed);
        IVP_U_Float_Point correction_speed_cs;
        core->get_m_world_f_core_PSI()->vimult3(
            &correction_speed_ws, &correction_speed_cs);
        IVP_U_Float_Point max_translation;
        max_translation.set_multiple(
            &friction_value_translation, event->delta_time);
        clip_axes(&correction_speed_cs, &max_translation);
        core->get_m_world_f_core_PSI()->vmult3(
            &correction_speed_cs, &correction_speed_ws);
        core->center_push_core_multiple_ws(
            &correction_speed_ws, core->get_mass());

        IVP_U_Float_Point correction_rot_speed_cs;
        correction_rot_speed_cs.subtract(
            &desired_rot_speed_cs, &core->rot_speed);
        IVP_U_Float_Point max_rotation;
        max_rotation.set_multiple(
            &friction_value_rotation, event->delta_time);
        clip_axes(&correction_rot_speed_cs, &max_rotation);
        IVP_U_Float_Point correction_rot_impulse;
        correction_rot_impulse.set_pairwise_mult(
            &correction_rot_speed_cs, core->get_rot_inertia());
        core->rot_push_core_cs(&correction_rot_impulse);
    }

    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_CONSTRAINTS_MAX;
    }
    void core_is_going_to_be_deleted_event(IVP_Core *core) override {
        (void)core;
        delete this;
    }

    friend class IVP_Environment;

    static void clip_axes(
        IVP_U_Float_Point *correction,
        const IVP_U_Float_Point *maximum) {
        for (int axis = 0; axis < 3; ++axis) {
            if (std::fabs(correction->k[axis]) > maximum->k[axis]) {
                correction->k[axis] = correction->k[axis] < 0.0f
                    ? -maximum->k[axis]
                    : maximum->k[axis];
            }
        }
    }

    IVP_Real_Object *real_obj;
    IVP_U_Float_Point desired_speed_ws;
    IVP_U_Float_Point desired_rot_speed_cs;
    IVP_U_Float_Point friction_value_translation;
    IVP_U_Float_Point friction_value_rotation;
    IVP_BOOL clip_manhattan;
};

class IVP_Standard_Gravity_Controller
    : public IVP_Controller_Independent {
public:
    IVP_Standard_Gravity_Controller() = default;

    void set_standard_gravity(IVP_U_Point *newGravity) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::StandardGravitySet,
            this, newGravity);
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *coreList) override {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::StandardGravitySimulate,
            this, [this, event, coreList] {
                if (!event || !coreList) return;
                // Ballance RVA 0x12010 visits every listed Core. A nearby
                // revision skips pinned cores here, but that branch is absent
                // from the retail instruction stream.
                for (int index = coreList->len() - 1; index >= 0; --index) {
                    IVP_Core *core = coreList->element_at(index);
                    if (!core)
                        continue;
                    core->global_damp_core(event->delta_time);
                    core->commit_all_async_pushes();
                    core->speed.add_multiple(&grav_vec, event->delta_time);
                }
            }, event, coreList);
    }

    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return BML::IVP::ABI::InvokeThisOr<IVP_CONTROLLER_PRIORITY>(
            BML::IVP::ABI::Address::StandardGravityGetPriority, this,
            [] { return IVP_CP_GRAVITY; });
    }

    void core_is_going_to_be_deleted_event(IVP_Core *core) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::StandardGravityCoreDeleted,
            this, core);
    }

    ~IVP_Standard_Gravity_Controller() override = default;

    IVP_U_Float_Point grav_vec;
};

class IVP_Template_Controller_Motion {
public:
    IVP_Template_Controller_Motion()
        : force_factor(0.8f), damp_factor(1.0f),
          torque_factor(0.8f), angular_damp_factor(1.0f),
          max_translation_force(1.0e6f, 1.0e6f, 1.0e6f),
          max_torque(1.0e6f) {}

    IVP_FLOAT force_factor;
    IVP_FLOAT damp_factor;
    IVP_FLOAT torque_factor;
    IVP_FLOAT angular_damp_factor;
    IVP_U_Float_Point max_translation_force;
    IVP_FLOAT max_torque;
};

enum IVP_GOLEM_PROBLEM : std::int32_t {
    IVP_GP_FAR_DISTANCE = 0,
    IVP_GP_BIG_ANGLE = 1,
    IVP_GP_TIME_BY_DISTANCE_TOO_LONG = 2,
};

class IVP_Template_Controller_Golem
    : public IVP_Template_Controller_Motion {
public:
    IVP_Template_Controller_Golem()
        : max_delta_position(10.0f),
          max_delta_orientation(1.57079632679489661923f),
          max_integrated_delta_position(10.0f),
          max_golem_force(std::numeric_limits<IVP_FLOAT>::max()),
          filter_dtime(5.0f) {}

    IVP_FLOAT max_delta_position;
    IVP_FLOAT max_delta_orientation;
    IVP_FLOAT max_integrated_delta_position;
    IVP_FLOAT max_golem_force;
    IVP_FLOAT filter_dtime;
};

class IVP_Controller_Motion : public IVP_Controller_Independent {
public:
    IVP_Controller_Motion(
        IVP_Real_Object *object,
        const IVP_Template_Controller_Motion *definition)
        : force_factor(definition->force_factor),
          damp_factor(definition->damp_factor),
          torque_factor(definition->torque_factor),
          angular_damp_factor(definition->angular_damp_factor),
          l_environment(object->get_environment()), real_object(object),
          core(object->get_core()) {
        target_pos_ws.set_to_zero();
        target_q_world_f_core.init();
        max_translation_force.set(&definition->max_translation_force);
        max_torque.set(definition->max_torque, definition->max_torque,
                       definition->max_torque);
        if (core) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::ControllerManagerAddToCore,
                this, core);
        }
    }

    ~IVP_Controller_Motion() override {
        if (core) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::ControllerManagerRemoveFromCore,
                this, core);
        }
    }

    const IVP_U_Float_Point *get_max_torque() const {
        return &max_torque;
    }
    const IVP_U_Float_Point *get_max_translation_force() const {
        return &max_translation_force;
    }
    IVP_FLOAT get_force_factor() const { return force_factor; }
    IVP_FLOAT get_damp_factor() const { return damp_factor; }
    IVP_FLOAT get_torque_factor() const { return torque_factor; }
    IVP_FLOAT get_angular_damp_factor() const {
        return angular_damp_factor;
    }
    const IVP_U_Point *get_target_position_ws() const {
        return &target_pos_ws;
    }
    const IVP_U_Quat *get_target_orientation() const {
        return &target_q_world_f_core;
    }

    void set_max_torque(const IVP_U_Float_Point *value) {
        max_torque = *value;
    }
    void set_max_translation_force(const IVP_U_Float_Point *value) {
        max_translation_force = *value;
    }
    void set_force_factor(IVP_FLOAT value) { force_factor = value; }
    void set_damp_factor(IVP_FLOAT value) { damp_factor = value; }
    void set_torque_factor(IVP_FLOAT value) { torque_factor = value; }
    void set_angular_damp_factor(IVP_FLOAT value) {
        angular_damp_factor = value;
    }

    void set_target_position_ws(const IVP_U_Point *position) {
        if (position->quad_distance_to(&target_pos_ws) == 0.0)
            return;
        target_pos_ws = *position;
        if (core)
            core->ensure_core_to_be_in_simulation();
    }

    void set_target_object_position_ws(
        IVP_Real_Object *object, const IVP_U_Quat *orientation,
        const IVP_U_Point *objectCenterWs) {
        const bool coreShiftIsZero =
            ((real_object->flags >> 10u) & 0x3u) != 0u;
        if (coreShiftIsZero) {
            set_target_position_ws(objectCenterWs);
            return;
        }

        IVP_U_Matrix3 worldFromObject;
        orientation->set_matrix(&worldFromObject);
        IVP_U_Float_Point shiftWorld;
        worldFromObject.vmult3(
            object->get_shift_core_f_object(), &shiftWorld);
        IVP_U_Point shiftedCenter(shiftWorld);
        IVP_U_Point corePosition;
        corePosition.subtract(objectCenterWs, &shiftedCenter);
        set_target_position_ws(&corePosition);
    }

    void set_target_q_world_f_core(const IVP_U_Quat *orientation) {
        if (std::fabs(
                orientation->acos_quat(&target_q_world_f_core) - 1.0) <
            1.0e-10) {
            return;
        }
        target_q_world_f_core = *orientation;
        if (core)
            core->ensure_core_to_be_in_simulation();
    }

protected:
    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        if (!event || !core)
            return;

        const IVP_U_Point *currentPosition =
            core->get_m_world_f_core_PSI()->get_position();
        IVP_U_Float_Point deltaPosition;
        deltaPosition.inline_subtract_and_mult(
            &target_pos_ws, currentPosition, force_factor);
        deltaPosition.add_multiple(
            &core->speed, -event->delta_time * damp_factor);

        const IVP_DOUBLE inverseDelta = event->i_delta_time;
        const IVP_DOUBLE inverseDeltaSquared =
            inverseDelta * inverseDelta;
        for (int axis = 2; axis >= 0; --axis) {
            const IVP_FLOAT axisForce = static_cast<IVP_FLOAT>(
                deltaPosition.k[axis] * core->get_mass() *
                inverseDeltaSquared);
            if (std::fabs(axisForce) < max_translation_force.k[axis])
                continue;
            const IVP_FLOAT clippedDelta =
                max_translation_force.k[axis] * core->get_inv_mass() *
                static_cast<IVP_FLOAT>(event->delta_time * event->delta_time);
            deltaPosition.k[axis] =
                deltaPosition.k[axis] < 0.0f ? -clippedDelta : clippedDelta;
        }
        core->speed.add_multiple(&deltaPosition, inverseDelta);

        IVP_U_Quat targetCoreFromWorld;
        targetCoreFromWorld.set_invert_unit_quat(&target_q_world_f_core);
        IVP_U_Quat deltaCore;
        deltaCore.set_mult_quat(
            &targetCoreFromWorld, &core->q_world_f_core_next_psi);
        IVP_U_Float_Point deltaAngles;
        deltaCore.get_angles(&deltaAngles);
        const IVP_DOUBLE direction =
            target_q_world_f_core.acos_quat(
                &core->q_world_f_core_next_psi) < 0.0
                ? torque_factor * inverseDelta
                : -torque_factor * inverseDelta;
        deltaAngles.mult(direction);

        IVP_U_Float_Point rotationChange;
        rotationChange.add_multiple(
            &deltaAngles, &core->rot_speed, -angular_damp_factor);
        for (int axis = 2; axis >= 0; --axis) {
            const IVP_FLOAT torque = static_cast<IVP_FLOAT>(
                rotationChange.k[axis] * inverseDelta *
                core->get_rot_inertia()->k[axis]);
            if (std::fabs(torque) < max_torque.k[axis])
                continue;
            const IVP_FLOAT clippedChange =
                max_torque.k[axis] * core->get_inv_rot_inertia()->k[axis] *
                static_cast<IVP_FLOAT>(event->delta_time);
            rotationChange.k[axis] =
                torque < 0.0f ? -clippedChange : clippedChange;
        }
        core->rot_speed.add(&rotationChange);
    }

    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_MOTION;
    }

    void core_is_going_to_be_deleted_event(IVP_Core *deletedCore) override {
        if (core == deletedCore) {
            core = nullptr;
            delete this;
        }
    }

    IVP_U_Point target_pos_ws;
    IVP_U_Quat target_q_world_f_core;
    IVP_U_Float_Point max_translation_force;
    IVP_U_Float_Point max_torque;
    IVP_FLOAT force_factor;
    IVP_FLOAT damp_factor;
    IVP_FLOAT torque_factor;
    IVP_FLOAT angular_damp_factor;
    IVP_Environment *l_environment;
    IVP_Real_Object *real_object;
    IVP_Core *core;
};

// Ballance imports this public type with a 0x130-byte x86 layout, but links
// out every Golem-specific function body.  Keep it as the original abstract
// policy controller and reconstruct only the source algorithm over the
// independently verified IVP_Controller_Motion/Core ABI.  A Mod supplies the
// recovery policy through resolve_for_problem().
class IVP_Controller_Golem : public IVP_Controller_Motion {
protected:
    void reset_time(IVP_Time offset) override {
        time_of_prime_position -= offset;
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *coreList) override {
        if (!event || !event->environment || !core || !real_object ||
            !coreList || coreList->len() != 1) {
            return;
        }

        const IVP_Time currentTime = event->environment->get_current_time();
        const IVP_FLOAT positionDeltaTime = static_cast<IVP_FLOAT>(
            currentTime - time_of_prime_position);
        target_pos_ws.add_multiple(
            &prime_position_ws, &velocity_ws, positionDeltaTime);

        if (angular_velocity_set == IVP_FALSE) {
            target_q_world_f_core = prime_orientation_0;
        } else {
            const IVP_FLOAT orientationDeltaTime = static_cast<IVP_FLOAT>(
                currentTime - time_of_prime_orientation_0);
            target_q_world_f_core.set_interpolate_smoothly(
                &prime_orientation_0, &prime_orientation_1,
                orientationDeltaTime * i_delta_prime_orientation_time);
        }

        const IVP_U_Matrix *worldFromCore =
            core->get_m_world_f_core_PSI();
        const IVP_U_Point *currentPosition = worldFromCore->get_position();
        IVP_U_Float_Point deltaPosition(
            static_cast<IVP_FLOAT>(
                target_pos_ws.k[0] - currentPosition->k[0]),
            static_cast<IVP_FLOAT>(
                target_pos_ws.k[1] - currentPosition->k[1]),
            static_cast<IVP_FLOAT>(
                target_pos_ws.k[2] - currentPosition->k[2]));

        const bool coreShiftIsZero =
            ((real_object->flags >> 10u) & 0x3u) != 0u;
        if (!coreShiftIsZero) {
            IVP_U_Float_Point shiftWorld;
            worldFromCore->vmult3(
                real_object->get_shift_core_f_object(), &shiftWorld);
            deltaPosition.subtract(&shiftWorld);
        }

        const IVP_DOUBLE squaredDistance = deltaPosition.quad_length();
        integrated_delta_position = static_cast<IVP_FLOAT>(
            integrated_delta_position * 0.9 + squaredDistance);
        if (squaredDistance >
            static_cast<IVP_DOUBLE>(max_delta_position) *
                max_delta_position) {
            (void)resolve_for_problem(event, IVP_GP_FAR_DISTANCE);
            return;
        }

        deltaPosition.mult(force_factor);
        deltaPosition.add_multiple(
            &velocity_ws, event->delta_time * damp_factor);
        deltaPosition.add_multiple(
            &core->speed, -event->delta_time * damp_factor);

        const IVP_DOUBLE inverseDeltaTime = event->i_delta_time;
        const IVP_DOUBLE inverseDeltaTimeSquared =
            inverseDeltaTime * inverseDeltaTime;
        for (int axis = 2; axis >= 0; --axis) {
            const IVP_FLOAT force = static_cast<IVP_FLOAT>(
                deltaPosition.k[axis] * core->get_mass() *
                inverseDeltaTimeSquared);
            if (std::fabs(force) < max_translation_force.k[axis])
                continue;
            const IVP_FLOAT clippedDelta = static_cast<IVP_FLOAT>(
                max_translation_force.k[axis] * core->get_inv_mass() *
                event->delta_time * event->delta_time);
            deltaPosition.k[axis] = force < 0.0f
                ? -clippedDelta
                : clippedDelta;
        }
        core->speed.add_multiple(&deltaPosition, inverseDeltaTime);

        IVP_U_Quat targetCoreFromWorld;
        targetCoreFromWorld.set_invert_unit_quat(
            &target_q_world_f_core);
        IVP_U_Quat deltaCore;
        deltaCore.set_mult_quat(
            &targetCoreFromWorld, &core->q_world_f_core_next_psi);
        IVP_U_Float_Point deltaAngles;
        deltaCore.get_angles(&deltaAngles);
        if (deltaAngles.quad_length() >
            static_cast<IVP_DOUBLE>(max_delta_orientation) *
                max_delta_orientation) {
            (void)resolve_for_problem(event, IVP_GP_BIG_ANGLE);
            return;
        }

        const IVP_DOUBLE rotationDirection =
            target_q_world_f_core.acos_quat(
                &core->q_world_f_core_next_psi) < 0.0
                ? torque_factor * inverseDeltaTime
                : -torque_factor * inverseDeltaTime;
        deltaAngles.mult(rotationDirection);

        IVP_U_Float_Point rotationChange;
        // The nearby Golem implementation intentionally uses damp_factor here
        // (not angular_damp_factor); preserve that version-specific behavior.
        rotationChange.add_multiple(
            &deltaAngles, &core->rot_speed, -damp_factor);
        for (int axis = 2; axis >= 0; --axis) {
            const IVP_FLOAT torque = static_cast<IVP_FLOAT>(
                rotationChange.k[axis] * inverseDeltaTime *
                core->get_rot_inertia()->k[axis]);
            if (std::fabs(torque) < max_torque.k[axis])
                continue;
            const IVP_FLOAT clippedChange = static_cast<IVP_FLOAT>(
                max_torque.k[axis] *
                core->get_inv_rot_inertia()->k[axis] *
                event->delta_time);
            rotationChange.k[axis] = torque < 0.0f
                ? -clippedChange
                : clippedChange;
        }
        core->rot_speed.add(&rotationChange);
    }

    IVP_FLOAT integrated_delta_position;
    IVP_Time time_of_prime_position;
    IVP_Time time_of_prime_orientation_0;
    IVP_FLOAT i_delta_prime_orientation_time;
    IVP_BOOL angular_velocity_set;
    IVP_U_Quat prime_orientation_0;
    IVP_U_Quat prime_orientation_1;
    IVP_U_Point prime_position_ws;
    IVP_U_Float_Point velocity_ws;
    IVP_FLOAT max_delta_position;
    IVP_FLOAT max_delta_orientation;
    IVP_FLOAT max_integrated_delta_position;
    IVP_FLOAT max_golem_force;
    IVP_FLOAT filter_dtime;

public:
    virtual IVP_RETURN_TYPE resolve_for_problem(
        IVP_Event_Sim *event, IVP_GOLEM_PROBLEM problem) = 0;

    void set_prime_position(
        const IVP_U_Point *position,
        const IVP_U_Float_Point *velocity,
        const IVP_Time &time) {
        time_of_prime_position = time;
        prime_position_ws.set(position);
        velocity_ws.set(velocity);
        if (core)
            core->ensure_core_to_be_in_simulation();
    }

    void set_prime_orientation(
        const IVP_U_Quat *orientation0, const IVP_Time &time0,
        const IVP_U_Quat *orientation1 = nullptr,
        IVP_FLOAT deltaTime = 0.0f) {
        prime_orientation_0 = *orientation0;
        if (orientation1) {
            prime_orientation_1 = *orientation1;
            angular_velocity_set = IVP_TRUE;
            i_delta_prime_orientation_time = 1.0f / deltaTime;
        } else {
            angular_velocity_set = IVP_FALSE;
        }
        time_of_prime_orientation_0 = time0;
        if (core)
            core->ensure_core_to_be_in_simulation();
    }

    IVP_Controller_Golem(
        IVP_Real_Object *object,
        const IVP_Template_Controller_Golem *definition)
        : IVP_Controller_Motion(object, definition),
          integrated_delta_position(0.0f),
          time_of_prime_position(0.0),
          time_of_prime_orientation_0(0.0),
          i_delta_prime_orientation_time(1.0f),
          angular_velocity_set(IVP_FALSE),
          max_delta_position(definition->max_delta_position),
          max_delta_orientation(definition->max_delta_orientation),
          max_integrated_delta_position(
              definition->max_integrated_delta_position),
          max_golem_force(definition->max_golem_force),
          filter_dtime(definition->filter_dtime) {
        prime_orientation_0.init();
        prime_orientation_1.init();
        prime_position_ws.set_to_zero();
        velocity_ws.set_to_zero();
    }

    ~IVP_Controller_Golem() override = default;
};

class IVP_Controller_Manager {
public:
    explicit IVP_Controller_Manager(IVP_Environment *environment)
        : l_environment(environment) {}

    void announce_controller_to_environment(
        IVP_Controller_Dependent *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ControllerManagerAnnounce,
            this, controller);
    }
    static void add_controller_to_core(
        IVP_Controller_Independent *controller, IVP_Core *core) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::ControllerManagerAddToCore,
            controller, core);
    }
    static void remove_controller_from_environment(
        IVP_Controller_Dependent *controller,
        IVP_BOOL silently = IVP_FALSE) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::ControllerManagerRemoveFromEnvironment,
            controller, silently);
    }
    static void remove_controller_from_core(
        IVP_Controller_Independent *controller, IVP_Core *core) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::ControllerManagerRemoveFromCore,
            controller, core);
    }
    void ensure_controller_in_simulation(
        IVP_Controller_Dependent *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ControllerManagerEnsureSimulation,
            this, controller);
    }
    void ensure_core_in_simulation(IVP_Core *core) {
        if (core)
            core->ensure_core_to_be_in_simulation();
    }

    IVP_Environment *l_environment;
};

inline IVP_Controller_Motion *IVP_Environment::create_controller_motion(
    IVP_Real_Object *object,
    const class IVP_Template_Controller_Motion *definition) {
    return object && definition
        ? new IVP_Controller_Motion(object, definition)
        : nullptr;
}

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Event_Sim) == 0x18);
static_assert(alignof(IVP_Event_Sim) == 0x08);
static_assert(offsetof(IVP_Event_Sim, delta_time) == 0x00);
static_assert(offsetof(IVP_Event_Sim, i_delta_time) == 0x08);
static_assert(offsetof(IVP_Event_Sim, environment) == 0x10);
static_assert(offsetof(IVP_Event_Sim, sim_unit) == 0x14);
static_assert(sizeof(IVP_Controller) == 0x04);
static_assert(sizeof(IVP_Controller_Dependent) == 0x04);
static_assert(sizeof(IVP_Controller_Independent) == 0x04);
static_assert(sizeof(IVP_Template_Controller_Floating) == 0x50);
static_assert(offsetof(IVP_Template_Controller_Floating, position_os) == 0x08);
static_assert(offsetof(IVP_Template_Controller_Floating, ray_direction_ws) == 0x28);
static_assert(offsetof(IVP_Template_Controller_Floating, target_distance) == 0x48);
static_assert(sizeof(IVP_Controller_Floating) == 0x40);
static_assert(sizeof(IVP_Template_Controller_World_Friction) == 0x80);
static_assert(offsetof(IVP_Template_Controller_World_Friction,
                       desired_rot_speed_cs) == 0x20);
static_assert(offsetof(IVP_Template_Controller_World_Friction,
                       friction_value_translation) == 0x40);
static_assert(offsetof(IVP_Template_Controller_World_Friction,
                       friction_value_rotation) == 0x60);
static_assert(sizeof(IVP_Controller_World_Friction) == 0x4C);
static_assert(sizeof(IVP_Standard_Gravity_Controller) == 0x14);
static_assert(offsetof(IVP_Standard_Gravity_Controller, grav_vec) == 0x04);
static_assert(sizeof(IVP_Template_Controller_Motion) == 0x24);
static_assert(sizeof(IVP_Template_Controller_Golem) == 0x38);
static_assert(offsetof(IVP_Template_Controller_Golem,
                       max_delta_position) == 0x24);
static_assert(offsetof(IVP_Template_Controller_Golem,
                       max_delta_orientation) == 0x28);
static_assert(offsetof(IVP_Template_Controller_Golem,
                       max_integrated_delta_position) == 0x2C);
static_assert(offsetof(IVP_Template_Controller_Golem,
                       max_golem_force) == 0x30);
static_assert(offsetof(IVP_Template_Controller_Golem, filter_dtime) == 0x34);
static_assert(sizeof(IVP_Controller_Motion) == 0x88);
static_assert(sizeof(IVP_Controller_Golem) == 0x130);
static_assert(sizeof(IVP_Controller_Manager) == 0x04);
static_assert(offsetof(IVP_Controller_Manager, l_environment) == 0x00);
#endif

#endif // BML_IVP_CONTROLLER_H
