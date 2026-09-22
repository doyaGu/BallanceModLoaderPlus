#ifndef BML_IVP_RAYCAST_CAR_H
#define BML_IVP_RAYCAST_CAR_H

#include "BML/IVP/Car.h"
#include "BML/IVP/Controller.h"
#include "BML/IVP/Ray.h"
#include "BML/IVP/Reaction.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

#ifndef IVP_RAYCAST_CAR_MAX_WHEELS
#define IVP_RAYCAST_CAR_MAX_WHEELS 12
#endif

struct BML_IvpRaycastCarLayoutCheck;

// This class follows the 0x958 Ballance UDT and its imported primary vtable:
// the 28-slot IVP_Car_System table plus do_raycasts. The implementation is a
// selective reconstruction of the link-stripped neighboring algorithm. It is
// intended to be subclassed by a Mod that supplies the actual raycast batch.
class IVP_Controller_Raycast_Car
    : public IVP_Car_System,
      protected IVP_Controller_Dependent {
    friend struct BML_IvpRaycastCarLayoutCheck;

private:
    IVP_Controller_Raycast_Car_Vector_of_Cores_1 vector_of_cores;

protected:
    std::int16_t n_wheels;
    std::int16_t n_axis;
    std::int16_t wheels_per_axis;
    IVP_Raycast_Car_Wheel wheels_of_car[IVP_RAYCAST_CAR_MAX_WHEELS];
    IVP_Raycast_Car_Axis axis_of_car[IVP_RAYCAST_CAR_MAX_WHEELS / 2];
    IVP_Real_Object *car_body;
    IVP_FLOAT gravity_y_direction;
    IVP_U_Float_Point normized_gravity_ws;
    IVP_FLOAT max_speed;
    IVP_COORDINATE_INDEX index_x;
    IVP_COORDINATE_INDEX index_y;
    IVP_COORDINATE_INDEX index_z;
    IVP_BOOL is_left_handed;
    IVP_FLOAT extra_gravity;
    IVP_FLOAT down_force;
    IVP_FLOAT down_force_vertical_offset;
    IVP_FLOAT booster_force;
    IVP_FLOAT booster_seconds_to_go;
    IVP_FLOAT booster_seconds_until_ready;
    IVP_FLOAT steering_angle;
    IVP_CarSystemDebugData_t m_CarSystemDebugData;

    IVP_Raycast_Car_Wheel *get_wheel(IVP_POS_WHEEL position) {
        return &wheels_of_car[static_cast<int>(position)];
    }
    const IVP_Raycast_Car_Wheel *get_wheel(IVP_POS_WHEEL position) const {
        return &wheels_of_car[static_cast<int>(position)];
    }
    IVP_Raycast_Car_Axis *get_axis(IVP_POS_AXIS position) {
        return &axis_of_car[static_cast<int>(position)];
    }

    void core_is_going_to_be_deleted_event(IVP_Core *) override {
        delete this;
    }
    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        return &vector_of_cores;
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        if (!event || !car_body || !car_body->get_core())
            return;

        IVP_Raycast_Car_Wheel_Temp temporary[IVP_RAYCAST_CAR_MAX_WHEELS]{};
        IVP_Ray_Solver_Template rays[IVP_RAYCAST_CAR_MAX_WHEELS]{};
        IVP_Ray_Hit hits[IVP_RAYCAST_CAR_MAX_WHEELS]{};
        IVP_FLOAT frictions[IVP_RAYCAST_CAR_MAX_WHEELS]{};
        IVP_Core *core = car_body->get_core();
        const IVP_U_Matrix *worldFromCore = core->get_m_world_f_core_PSI();

        setup_wheel_raycasts(rays, worldFromCore, temporary);
        do_raycasts(event, n_wheels, rays, hits, frictions);
        if (!prepare_wheel_contacts(
                rays, worldFromCore, temporary, hits, frictions, core))
            return;
        simulate_stabilizers(temporary);
        core->speed.add_multiple(
            &normized_gravity_ws,
            extra_gravity * core->get_inv_mass() * event->delta_time);
        simulate_shocks(temporary, hits, event, core);
        simulate_booster(event, core);
        simulate_steering(temporary, core, event);
    }

    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_CONSTRAINTS_MAX;
    }

    void initialize_environment(
        IVP_Environment *environment,
        const IVP_Template_Car_System *definition) {
        index_x = definition->index_x;
        index_y = definition->index_y;
        index_z = definition->index_z;
        is_left_handed = definition->is_left_handed;
        environment->get_controller_manager()
            ->announce_controller_to_environment(this);
        extra_gravity = definition->extra_gravity_force_value;
        gravity_y_direction =
            environment->get_gravity()->k[index_y] > 0.0 ? 1.0f : -1.0f;
        normized_gravity_ws.set(environment->get_gravity());
        normized_gravity_ws.normize();
    }

    void initialize_body(const IVP_Template_Car_System *definition) {
        n_wheels = static_cast<std::int16_t>(definition->n_wheels);
        n_axis = static_cast<std::int16_t>(definition->n_axis);
        wheels_per_axis = static_cast<std::int16_t>(n_wheels / n_axis);
        car_body = definition->car_body;
        vector_of_cores.add(car_body->get_core());
        booster_force = 0.0f;
        booster_seconds_to_go = 0.0f;
        booster_seconds_until_ready = 0.0f;
        down_force = 0.0f;
        down_force_vertical_offset =
            definition->body_down_force_vertical_offset;
    }

    void initialize_wheels(const IVP_Template_Car_System *definition) {
        IVP_U_Matrix coreFromObject;
        car_body->calc_m_core_f_object(&coreFromObject);
        for (int index = 0; index < n_wheels; ++index) {
            IVP_Raycast_Car_Wheel *wheel = &wheels_of_car[index];
            std::memset(wheel, 0, sizeof(*wheel));
            coreFromObject.vmult4(&definition->wheel_pos_Bos[index],
                                  &wheel->hp_cs);
            wheel->spring_len = -definition->spring_pre_tension[index];
            wheel->spring_direction_cs.set_to_zero();
            wheel->spring_direction_cs.k[index_y] = gravity_y_direction;
            wheel->spring_constant = definition->spring_constant[index];
            wheel->spring_damp_relax = definition->spring_dampening[index];
            wheel->spring_damp_compress =
                definition->spring_dampening_compression[index];
            // The link-stripped implementation hardcodes combined wheel
            // friction to one here; surface friction arrives per raycast.
            wheel->friction_of_wheel = 1.0f;
            wheel->wheel_radius = definition->wheel_radius[index];
            wheel->inv_wheel_radius = 1.0f / wheel->wheel_radius;
            do_steering_wheel(static_cast<IVP_POS_WHEEL>(index), 0.0f);
            wheel->wheel_is_fixed = IVP_FALSE;
            wheel->max_rotation_speed =
                definition->wheel_max_rotation_speed[index >> 1];
        }
    }

    void initialize_axes(const IVP_Template_Car_System *definition) {
        steering_angle = -1.0f;
        do_steering(0.0f);
        for (int index = 0; index < n_axis; ++index) {
            axis_of_car[index].stabilizer_constant =
                definition->stabilizer_constant[index];
        }
    }

    void setup_wheel_raycasts(
        IVP_Ray_Solver_Template *rays,
        const IVP_U_Matrix *worldFromCore,
        IVP_Raycast_Car_Wheel_Temp *temporary) {
        for (int index = 0; index < n_wheels; ++index) {
            IVP_Raycast_Car_Wheel &wheel = wheels_of_car[index];
            worldFromCore->vmult4(&wheel.hp_cs, &rays[index].ray_start_point);
            worldFromCore->vmult3(
                &wheel.spring_direction_cs,
                &temporary[index].spring_direction_ws);
            rays[index].ray_normized_direction.set(
                &temporary[index].spring_direction_ws);
            rays[index].ray_length = wheel.spring_len + wheel.wheel_radius;
            rays[index].ray_flags = IVP_RAY_SOLVER_ALL;
        }
    }

    bool prepare_wheel_contacts(
        IVP_Ray_Solver_Template *rays,
        const IVP_U_Matrix *worldFromCore,
        IVP_Raycast_Car_Wheel_Temp *temporary,
        IVP_Ray_Hit *hits, IVP_FLOAT *frictions, IVP_Core *core) {
        for (int index = 0; index < n_wheels; ++index) {
            IVP_Raycast_Car_Wheel &wheel = wheels_of_car[index];
            IVP_Raycast_Car_Wheel_Temp &state = temporary[index];
            IVP_Ray_Hit &hit = hits[index];
            if (hit.hit_real_object) {
                IVP_Cache_Object *cache =
                    hit.hit_real_object->get_cache_object_no_lock();
                if (cache) {
                    cache->transform_vector_to_world_coords(
                        &hit.hit_surface_direction_os,
                        &state.ground_normal_ws);
                    wheel.raycast_dist = hit.hit_distance;
                    state.inv_normal_dot_dir = static_cast<IVP_FLOAT>(
                        1.1 /
                        (std::fabs(state.spring_direction_ws.dot_product(
                             &state.ground_normal_ws)) +
                         0.1));
                }
            } else {
                wheel.pressure = 0.0f;
                wheel.raycast_dist = wheel.spring_len + wheel.wheel_radius;
                state.inv_normal_dot_dir = 1.0f;
                state.moveable_object_hit_by_ray = nullptr;
                state.ground_normal_ws.set_multiple(
                    &state.spring_direction_ws, -1.0);
            }
            state.friction_value =
                frictions[index] * wheel.friction_of_wheel;
            state.ground_hit_ws.add_multiple(
                &rays[index].ray_start_point,
                &state.spring_direction_ws, wheel.raycast_dist);
            core->get_surface_speed_ws(
                &state.ground_hit_ws, &state.surface_speed_wheel_ws);
            state.projected_surface_speed_wheel_ws.set_orthogonal_part(
                &state.surface_speed_wheel_ws, &state.ground_normal_ws);
            worldFromCore->vmult3(
                &wheel.axis_direction_cs, &state.axis_direction_ws);
            state.projected_axis_direction_ws.set_orthogonal_part(
                &state.axis_direction_ws, &state.ground_normal_ws);
            if (state.projected_axis_direction_ws.normize() == IVP_FAULT)
                return false;
        }
        return true;
    }

    void simulate_stabilizers(IVP_Raycast_Car_Wheel_Temp *temporary) {
        if (wheels_per_axis != 2) {
            for (int index = 0; index < n_wheels; ++index)
                temporary[index].stabilizer_force = 0.0f;
            return;
        }
        for (int axis = 0; axis < n_axis; ++axis) {
            IVP_Raycast_Car_Wheel &left = wheels_of_car[axis * 2];
            IVP_Raycast_Car_Wheel &right = wheels_of_car[axis * 2 + 1];
            const IVP_DOUBLE leftTravel =
                left.raycast_dist - left.spring_len - left.wheel_radius;
            const IVP_DOUBLE rightTravel =
                right.raycast_dist - right.spring_len - right.wheel_radius;
            temporary[axis * 2].stabilizer_force =
                static_cast<IVP_FLOAT>(
                    (rightTravel - leftTravel) *
                    (axis_of_car[axis].stabilizer_constant * 0.5f));
            temporary[axis * 2 + 1].stabilizer_force =
                -temporary[axis * 2].stabilizer_force;
        }
    }

    void simulate_shocks(
        IVP_Raycast_Car_Wheel_Temp *temporary,
        IVP_Ray_Hit *hits, IVP_Event_Sim *event, IVP_Core *core) {
        for (int index = 0; index < n_wheels; ++index) {
            IVP_Raycast_Car_Wheel &wheel = wheels_of_car[index];
            if (!hits[index].hit_real_object)
                continue;
            const IVP_DOUBLE compression =
                wheel.raycast_dist - wheel.spring_len - wheel.wheel_radius;
            if (compression >= 0.0)
                continue;

            IVP_DOUBLE force =
                -compression * wheel.spring_constant +
                temporary[index].stabilizer_force;
            const IVP_FLOAT normalFactor = std::clamp(
                temporary[index].inv_normal_dot_dir, 0.0f, 3.0f);
            force *= normalFactor;
            IVP_U_Float_Point removedNormalSpeed;
            removedNormalSpeed.subtract(
                &temporary[index].projected_surface_speed_wheel_ws,
                &temporary[index].surface_speed_wheel_ws);
            const IVP_DOUBLE speed = removedNormalSpeed.dot_product(
                &temporary[index].spring_direction_ws);
            force -= (speed > 0.0 ? wheel.spring_damp_relax
                                  : wheel.spring_damp_compress) *
                     speed;
            force = std::max(force, 0.0);
            wheel.pressure = static_cast<IVP_FLOAT>(force);
            IVP_U_Float_Point impulse;
            impulse.set_multiple(
                &temporary[index].ground_normal_ws,
                force * event->delta_time);
            core->push_core_ws(&temporary[index].ground_hit_ws, &impulse);
        }
    }

    void simulate_booster(IVP_Event_Sim *event, IVP_Core *core) {
        if (booster_seconds_until_ready > 0.0f)
            booster_seconds_until_ready -=
                static_cast<IVP_FLOAT>(event->delta_time);
        if (booster_seconds_to_go > 0.0f) {
            booster_seconds_to_go -=
                static_cast<IVP_FLOAT>(event->delta_time);
            if (booster_seconds_to_go <= 0.0f)
                booster_force = 0.0f;
        }
        if (booster_force != 0.0f) {
            IVP_U_Float_Point direction;
            core->get_m_world_f_core_PSI()->get_col(index_z, &direction);
            core->speed.add_multiple(
                &direction, booster_force * event->delta_time);
        }
    }

    void simulate_steering(
        IVP_Raycast_Car_Wheel_Temp *temporary,
        IVP_Core *core, IVP_Event_Sim *event) {
        IVP_FLOAT forces[IVP_RAYCAST_CAR_MAX_WHEELS]{};
        calculate_steering_forces(temporary, core, event, forces);
        apply_steering_forces(temporary, core, event, forces);
    }

    void calculate_steering_forces(
        IVP_Raycast_Car_Wheel_Temp *temporary,
        IVP_Core *core, IVP_Event_Sim *event, IVP_FLOAT *forces) {
        IVP_U_Point frontPosition;
        IVP_U_Point rearPosition;
        int rearWheel = 1;
        if (wheels_per_axis == 2) {
            frontPosition.set_interpolate(
                &temporary[IVP_FRONT_LEFT].ground_hit_ws,
                &temporary[IVP_FRONT_RIGHT].ground_hit_ws, 0.5);
            rearPosition.set_interpolate(
                &temporary[IVP_REAR_LEFT].ground_hit_ws,
                &temporary[IVP_REAR_RIGHT].ground_hit_ws, 0.5);
            rearWheel = IVP_REAR_LEFT;
        } else {
            frontPosition.set(&temporary[0].ground_hit_ws);
            rearPosition.set(&temporary[1].ground_hit_ws);
        }

        IVP_Solver_Core_Reaction reaction[2];
        reaction[0].init_reaction_solver_translation_ws(
            core, nullptr, frontPosition,
            &temporary[IVP_FRONT_LEFT].axis_direction_ws,
            nullptr, nullptr);
        reaction[1].init_reaction_solver_translation_ws(
            core, nullptr, rearPosition,
            &temporary[rearWheel].axis_direction_ws, nullptr, nullptr);
        IVP_FLOAT frontRear = static_cast<IVP_FLOAT>(
            reaction[0].cr_mult_inv0[0].dot_product4(
                reaction[1].cross_direction_position_cs0[0]));
        frontRear += static_cast<IVP_FLOAT>(
            core->get_inv_mass() *
            temporary[IVP_FRONT_LEFT].axis_direction_ws.dot_product(
                &temporary[rearWheel].axis_direction_ws));
        const IVP_DOUBLE frontDemand =
            -reaction[0].delta_velocity_ds.k[0] *
            event->i_delta_time * 1.2;
        const IVP_DOUBLE rearDemand =
            -reaction[1].delta_velocity_ds.k[0] *
            event->i_delta_time * 1.2;
        IVP_DOUBLE inverse[4]{};
        if (IVP_Inline_Math::invert_2x2_matrix(
                reaction[0].m_velocity_ds_f_impulse_ds.get_elem(0, 0),
                frontRear, frontRear,
                reaction[1].m_velocity_ds_f_impulse_ds.get_elem(0, 0),
                &inverse[0], &inverse[1], &inverse[2], &inverse[3]) ==
            IVP_OK) {
            forces[0] = static_cast<IVP_FLOAT>(
                inverse[0] * frontDemand + inverse[1] * rearDemand);
            forces[1] = static_cast<IVP_FLOAT>(
                inverse[2] * frontDemand + inverse[3] * rearDemand);
        }
    }

    void apply_steering_forces(
        IVP_Raycast_Car_Wheel_Temp *temporary,
        IVP_Core *core, IVP_Event_Sim *event, IVP_FLOAT *forces) {
        bool hasTorque = false;
        for (int index = 0; index < n_wheels; ++index)
            hasTorque = hasTorque || wheels_of_car[index].torque != 0.0f;
        const bool forceFixed =
            core->speed.fast_real_length() < 0.5 && !hasTorque;

        for (int index = 0; index < n_wheels; ++index) {
            IVP_Raycast_Car_Wheel &wheel = wheels_of_car[index];
            IVP_Raycast_Car_Wheel_Temp &state = temporary[index];
            IVP_FLOAT maximumForce = wheel.pressure * state.friction_value;
            IVP_U_Float_Point forward;
            forward.inline_calc_cross_product_and_normize(
                &state.projected_axis_direction_ws,
                &state.spring_direction_ws);
            const IVP_FLOAT bodySpeed = static_cast<IVP_FLOAT>(
                core->speed.dot_product(&forward));
            IVP_FLOAT driveForce = 0.0f;
            if (wheel.wheel_is_fixed || forceFixed) {
                driveForce = static_cast<IVP_FLOAT>(
                    -0.5 * bodySpeed * core->get_mass() *
                    event->i_delta_time);
                wheel.wheel_angular_velocity = 0.0f;
            } else {
                wheel.wheel_angular_velocity =
                    bodySpeed * wheel.inv_wheel_radius;
                const IVP_FLOAT sign = bodySpeed >= 0.0f ? 1.0f : -1.0f;
                driveForce =
                    -2.5f * state.friction_value *
                        (0.25f * core->get_mass()) * sign +
                    wheel.torque * wheel.inv_wheel_radius;
            }

            const int axis = index / wheels_per_axis;
            IVP_FLOAT lateralForce = forces[axis];
            const IVP_FLOAT squared =
                driveForce * driveForce + lateralForce * lateralForce;
            if (squared > maximumForce * maximumForce && squared > 0.0f) {
                const IVP_FLOAT factor =
                    std::sqrt(maximumForce * maximumForce / squared);
                driveForce *= factor;
                lateralForce *= factor;
            }
            forces[axis] -= lateralForce;
            wheel.angle_wheel -=
                wheel.wheel_angular_velocity *
                static_cast<IVP_FLOAT>(event->delta_time);

            IVP_U_Float_Point impulse;
            impulse.set_multiple(
                &state.projected_axis_direction_ws,
                lateralForce * event->delta_time);
            core->push_core_ws(&state.ground_hit_ws, &impulse);
            impulse.set_multiple(
                &forward, driveForce * event->delta_time);
            core->push_core_ws(&state.ground_hit_ws, &impulse);
        }
    }

    virtual void do_raycasts(
        IVP_Event_Sim *event, int wheelCount,
        IVP_Ray_Solver_Template *rays,
        IVP_Ray_Hit *hits,
        IVP_FLOAT *objectFrictions) = 0;

public:
    // Controller is inherited as a protected secondary base. Keep allocation
    // public on the complete car so the controller-side `delete this` path and
    // the creating Mod use the same retail heap.
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_Controller_Raycast_Car(
        IVP_Environment *environment,
        const IVP_Template_Car_System *definition) {
        initialize_body(definition);
        initialize_environment(environment, definition);
        initialize_wheels(definition);
        initialize_axes(definition);
    }

    ~IVP_Controller_Raycast_Car() override {
        if (car_body && car_body->get_environment()) {
            IVP_Controller_Manager::remove_controller_from_environment(
                this, IVP_TRUE);
        }
    }

    void do_steering_wheel(
        IVP_POS_WHEEL position, IVP_FLOAT angle) override {
        IVP_Raycast_Car_Wheel *wheel = get_wheel(position);
        wheel->axis_direction_cs.set_to_zero();
        wheel->axis_direction_cs.k[index_x] = 1.0f;
        wheel->axis_direction_cs.rotate(index_y, angle);
    }
    void change_spring_constant(
        IVP_POS_WHEEL position, IVP_FLOAT value) override {
        get_wheel(position)->spring_constant = value;
    }
    void change_spring_dampening(
        IVP_POS_WHEEL position, IVP_FLOAT value) override {
        get_wheel(position)->spring_damp_relax = value;
    }
    void change_spring_dampening_compression(
        IVP_POS_WHEEL position, IVP_FLOAT value) override {
        get_wheel(position)->spring_damp_compress = value;
    }
    void change_max_body_force(IVP_POS_WHEEL, IVP_FLOAT) override {}
    void change_spring_pre_tension(
        IVP_POS_WHEEL position, IVP_FLOAT value) override {
        IVP_Raycast_Car_Wheel *wheel = get_wheel(position);
        wheel->spring_len = gravity_y_direction *
                            (wheel->distance_orig_hp_to_hp - value);
    }
    void change_spring_length(
        IVP_POS_WHEEL position, IVP_FLOAT value) override {
        get_wheel(position)->spring_len = value;
    }
    void change_stabilizer_constant(
        IVP_POS_AXIS position, IVP_FLOAT value) override {
        get_axis(position)->stabilizer_constant = value;
    }
    void change_fast_turn_factor(IVP_FLOAT) override {}
    void change_wheel_torque(
        IVP_POS_WHEEL position, IVP_FLOAT value) override {
        get_wheel(position)->torque = value;
        car_body->get_environment()->get_controller_manager()
            ->ensure_controller_in_simulation(this);
    }
    void update_body_countertorque() override {}
    void update_throttle(IVP_FLOAT) override {}
    void change_body_downforce(IVP_FLOAT force) override {
        down_force = force;
    }
    void fix_wheel(
        IVP_POS_WHEEL position, IVP_BOOL fixed) override {
        get_wheel(position)->wheel_is_fixed = fixed;
    }
    IVP_DOUBLE get_body_speed(
        IVP_COORDINATE_INDEX forwardAxis = IVP_INDEX_Z) override {
        IVP_Core *core = car_body->get_core();
        IVP_U_Point direction;
        core->get_m_world_f_core_PSI()->get_col(forwardAxis, &direction);
        return direction.dot_product(&core->speed);
    }
    IVP_DOUBLE get_wheel_angular_velocity(
        IVP_POS_WHEEL position) override {
        return get_wheel(position)->wheel_angular_velocity;
    }
    void update_wheel_positions() override {}
    IVP_DOUBLE get_orig_front_wheel_distance() override {
        return std::fabs(
            static_cast<IVP_DOUBLE>(
                get_wheel(IVP_FRONT_LEFT)->hp_cs.k[index_x]) -
            get_wheel(IVP_FRONT_RIGHT)->hp_cs.k[index_x]);
    }
    IVP_DOUBLE get_orig_axles_distance() override {
        return std::fabs(
            static_cast<IVP_DOUBLE>(
                get_wheel(IVP_FRONT_LEFT)->hp_cs.k[index_z]) -
            get_wheel(IVP_REAR_LEFT)->hp_cs.k[index_z]);
    }
    void get_skid_info(IVP_Wheel_Skid_Info *output) override {
        for (int index = 0; index < n_wheels; ++index) {
            output[index].last_contact_position_ws.set_to_zero();
            output[index].last_skid_value = 0.0f;
            output[index].last_skid_time = IVP_Time(0.0);
        }
    }
    void do_steering(IVP_FLOAT angle) override {
        if (steering_angle == angle)
            return;
        steering_angle = angle;
        car_body->get_environment()->get_controller_manager()
            ->ensure_controller_in_simulation(this);
        for (int index = 0; index < wheels_per_axis; ++index)
            do_steering_wheel(static_cast<IVP_POS_WHEEL>(index), angle);
    }
    void set_booster_acceleration(IVP_FLOAT acceleration) override {
        booster_force = acceleration;
    }
    void activate_booster(
        IVP_FLOAT thrust, IVP_FLOAT duration,
        IVP_FLOAT rechargeDelay) override {
        if (booster_force != 0.0f || booster_seconds_until_ready > 0.0f)
            return;
        booster_force = thrust;
        booster_seconds_to_go = duration;
        booster_seconds_until_ready = duration + rechargeDelay;
    }
    void update_booster(IVP_FLOAT) override {}
    IVP_FLOAT get_booster_delay() override {
        return booster_seconds_until_ready;
    }
    void SetCarSystemDebugData(
        const IVP_CarSystemDebugData_t &input) override {
        for (int index = 0; index < IVP_CAR_SYSTEM_MAX_WHEELS; ++index) {
            m_CarSystemDebugData.wheelRaycasts[index][0] =
                input.wheelRaycasts[index][0];
            m_CarSystemDebugData.wheelRaycasts[index][1] =
                input.wheelRaycasts[index][1];
            m_CarSystemDebugData.wheelRaycastImpacts[index] =
                input.wheelRaycastImpacts[index];
        }
    }
    void GetCarSystemDebugData(
        IVP_CarSystemDebugData_t &output) override {
        for (int index = 0; index < IVP_CAR_SYSTEM_MAX_WHEELS; ++index) {
            output.wheelRaycasts[index][0] =
                m_CarSystemDebugData.wheelRaycasts[index][0];
            output.wheelRaycasts[index][1] =
                m_CarSystemDebugData.wheelRaycasts[index][1];
            output.wheelRaycastImpacts[index] =
                m_CarSystemDebugData.wheelRaycastImpacts[index];
        }
    }

    // Source compatibility for three later-revision methods. They are
    // deliberately non-virtual so the Ballance primary vtable stays at 29
    // slots. The two-argument steering overload ignores the later analog flag.
    void set_powerslide(IVP_FLOAT, IVP_FLOAT) {}
    IVP_FLOAT get_booster_time_to_go() {
        return booster_seconds_to_go;
    }
    void do_steering(IVP_FLOAT angle, bool) { do_steering(angle); }

    // This declaration has no implementation in the neighboring tree and no
    // body or callsite in Ballance. Reject use at compile time rather than
    // inventing which wheel it was meant to return or failing at link time.
    void get_wheel_position(
        IVP_U_Point *positionWs, IVP_U_Quat *directionWs) = delete;
};

struct BML_IvpRaycastCarLayoutCheck {
    static_assert(offsetof(IVP_Controller_Raycast_Car, vector_of_cores) == 0x08);
    static_assert(offsetof(IVP_Controller_Raycast_Car, n_wheels) == 0x14);
    static_assert(offsetof(IVP_Controller_Raycast_Car, wheels_of_car) == 0x1C);
    static_assert(offsetof(IVP_Controller_Raycast_Car, axis_of_car) == 0x5EC);
    static_assert(offsetof(IVP_Controller_Raycast_Car, car_body) == 0x604);
    static_assert(offsetof(IVP_Controller_Raycast_Car, index_x) == 0x620);
    static_assert(offsetof(IVP_Controller_Raycast_Car, booster_force) == 0x63C);
    static_assert(offsetof(IVP_Controller_Raycast_Car, m_CarSystemDebugData) ==
                  0x650);
};

static_assert(sizeof(IVP_Controller_Raycast_Car) == 0x958);

#endif // BML_IVP_RAYCAST_CAR_H
