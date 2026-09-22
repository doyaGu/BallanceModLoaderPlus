#ifndef BML_IVP_REAL_WHEELS_CAR_H
#define BML_IVP_REAL_WHEELS_CAR_H

#include "BML/IVP/Actuator.h"
#include "BML/IVP/Car.h"
#include "BML/IVP/ConstraintCar.h"

#include <cstddef>
#include <cstring>

// Constraint-based vehicle used by the early IVP branch embedded in Ballance.
// The 0x450 layout and the public 28-slot IVP_Car_System contract come from the
// Ballance type information. Implementations are selectively reconstructed from
// the neighboring source only where all accessed fields have matching layouts.
class IVP_Car_System_Real_Wheels : public IVP_Car_System {
  friend struct BML_IvpRealWheelsCarLayoutCheck;

private:
  IVP_Environment *environment;
  int n_wheels;
  int n_axis;

protected:
  IVP_Real_Object *car_body;
  IVP_Real_Object *car_wheel[IVP_CAR_SYSTEM_MAX_WHEELS];
  IVP_Constraint_Solver_Car *car_constraint_solver;
  IVP_Actuator_Torque *car_act_torque_body;
  IVP_Actuator_Torque *car_act_torque[IVP_CAR_SYSTEM_MAX_WHEELS];
  IVP_Actuator_Suspension *car_spring[IVP_CAR_SYSTEM_MAX_WHEELS];
  IVP_Actuator_Stabilizer *car_stabilizer[IVP_CAR_SYSTEM_MAX_AXIS];
  IVP_Actuator_Force *car_act_down_force;
  IVP_Actuator_Force *car_act_extra_gravity;
  IVP_Constraint *fix_wheel_constraint[IVP_CAR_SYSTEM_MAX_WHEELS];
  IVP_FLOAT wheel_reversed_sign[IVP_CAR_SYSTEM_MAX_WHEELS];
  IVP_FLOAT wheel_radius[IVP_CAR_SYSTEM_MAX_WHEELS];
  IVP_FLOAT body_counter_torque_factor;
  IVP_FLOAT max_speed;
  IVP_FLOAT fast_turn_factor;
  IVP_Actuator_Force *booster_actuator[2];
  IVP_FLOAT booster_seconds_to_go;
  IVP_FLOAT booster_seconds_until_ready;
  IVP_FLOAT steering_angle;
  IVP_CarSystemDebugData_t m_CarSystemDebugData;

  virtual void environment_will_be_deleted(IVP_Environment *) { delete this; }

public:
  IVP_Car_System_Real_Wheels(IVP_Environment *environment,
                             IVP_Template_Car_System *definition);
  ~IVP_Car_System_Real_Wheels() override { delete car_constraint_solver; }

  void do_steering_wheel(IVP_POS_WHEEL wheelPosition,
                         IVP_FLOAT steeringAngle) override;

  void change_spring_constant(IVP_POS_WHEEL position,
                              IVP_FLOAT value) override {
    car_spring[static_cast<int>(position)]->set_constant(value);
  }
  void change_spring_dampening(IVP_POS_WHEEL position,
                               IVP_FLOAT value) override {
    car_spring[static_cast<int>(position)]->set_damp(value);
  }
  void change_spring_dampening_compression(IVP_POS_WHEEL position,
                                           IVP_FLOAT value) override {
    car_spring[static_cast<int>(position)]->set_spring_damp_compression(value);
  }
  void change_max_body_force(IVP_POS_WHEEL position, IVP_FLOAT value) override {
    car_spring[static_cast<int>(position)]->set_max_body_force(value);
  }
  void change_spring_pre_tension(IVP_POS_WHEEL position,
                                 IVP_FLOAT value) override {
    car_spring[static_cast<int>(position)]->set_len(500.0f - value);
  }
  void change_spring_length(IVP_POS_WHEEL position, IVP_FLOAT value) override {
    car_spring[static_cast<int>(position)]->set_len(value);
  }
  void change_stabilizer_constant(IVP_POS_AXIS position,
                                  IVP_FLOAT value) override {
    car_stabilizer[static_cast<int>(position)]->set_stabi_constant(value);
  }
  void change_fast_turn_factor(IVP_FLOAT value) override {
    fast_turn_factor = value;
  }
  void change_wheel_torque(IVP_POS_WHEEL position, IVP_FLOAT value) override {
    car_act_torque[static_cast<int>(position)]->set_torque(value);
  }
  void change_wheel_speed_dampening(IVP_POS_WHEEL position, IVP_FLOAT value) {
    car_wheel[static_cast<int>(position)]->get_core()->speed_damp_factor =
        value;
  }
  void update_body_countertorque() override {
    IVP_FLOAT counterTorque = 0.0f;
    for (int wheel = 0; wheel < n_wheels; ++wheel)
      counterTorque -= car_act_torque[wheel]->get_torque();
    car_act_torque_body->set_torque(counterTorque * body_counter_torque_factor);
  }
  void update_throttle(IVP_FLOAT) override {}
  void change_body_downforce(IVP_FLOAT value) override {
    car_act_down_force->set_force(value);
  }
  void fix_wheel(IVP_POS_WHEEL position, IVP_BOOL stopWheel) override;

  IVP_DOUBLE
  get_body_speed(IVP_COORDINATE_INDEX forwardAxis = IVP_INDEX_Z) override {
    IVP_Core *core = car_body->get_core();
    IVP_U_Point orientation;
    core->get_m_world_f_core_PSI()->get_col(forwardAxis, &orientation);
    return static_cast<IVP_FLOAT>(orientation.dot_product(&core->speed));
  }
  IVP_DOUBLE get_wheel_angular_velocity(IVP_POS_WHEEL position) override {
    return car_act_torque[static_cast<int>(position)]->rot_speed_out;
  }
  void update_wheel_positions() override {}
  IVP_DOUBLE get_orig_front_wheel_distance() override;
  IVP_DOUBLE get_orig_axles_distance() override;
  void get_skid_info(IVP_Wheel_Skid_Info *output) override;
  void do_steering(IVP_FLOAT steeringAngle) override;
  void do_steering(IVP_FLOAT steeringAngle, bool) {
    do_steering(steeringAngle);
  }
  void set_booster_acceleration(IVP_FLOAT acceleration) override;
  void activate_booster(IVP_FLOAT thrust, IVP_FLOAT duration,
                        IVP_FLOAT rechargeTime) override;
  void update_booster(IVP_FLOAT deltaTime) override;
  IVP_FLOAT get_booster_delay() override { return booster_seconds_until_ready; }
  // Present as an inline convenience in the neighboring revision, but not a
  // virtual slot in Ballance's shorter IVP_Car_System ABI.
  IVP_FLOAT get_booster_time_to_go() { return booster_seconds_to_go; }
  void SetCarSystemDebugData(const IVP_CarSystemDebugData_t &) override {}
  void GetCarSystemDebugData(IVP_CarSystemDebugData_t &) override {}
};

inline void
IVP_Car_System_Real_Wheels::do_steering_wheel(IVP_POS_WHEEL wheelPosition,
                                              IVP_FLOAT angle) {
  const int wheel = static_cast<int>(wheelPosition);
  IVP_U_Matrix *target =
      &car_constraint_solver->wheel_objects.element_at(wheel)
           ->target_position_bs;
  IVP_U_Point translation;
  translation.set(&target->vv);

  IVP_U_Point rotation;
  rotation.set_to_zero();
  rotation.k[car_constraint_solver->y_idx] = angle;
  if (wheel_reversed_sign[wheel] < 0.0f)
    rotation.k[car_constraint_solver->y_idx] +=
        static_cast<IVP_FLOAT>(3.14159265358979323846);
  target->init_rot_multiple(&rotation, 1.0f);
  target->vv.set(&translation);
}

inline void IVP_Car_System_Real_Wheels::do_steering(IVP_FLOAT angle) {
  if (steering_angle == angle)
    return;

  const IVP_DOUBLE delta = angle - steering_angle;
  IVP_FLOAT trackWidth = 1.0f;
  IVP_FLOAT axleDistance = 1.0f;
  if (n_wheels >= 4) {
    trackWidth = static_cast<IVP_FLOAT>(get_orig_front_wheel_distance());
    axleDistance = static_cast<IVP_FLOAT>(get_orig_axles_distance());
  }

  const IVP_DOUBLE angularSpin =
      delta *
      get_body_speed(
          static_cast<IVP_COORDINATE_INDEX>(car_constraint_solver->z_idx)) /
      axleDistance;
  car_body->get_core()->rot_speed_change.k[car_constraint_solver->y_idx] -=
      static_cast<IVP_FLOAT>(angularSpin * fast_turn_factor);
  steering_angle = angle;
  environment->get_controller_manager()->ensure_controller_in_simulation(
      car_constraint_solver);

  const int wheelsPerAxis = n_wheels / n_axis;
  for (int wheel = 0; wheel < wheelsPerAxis; ++wheel) {
    IVP_FLOAT wheelAngle = angle;
    if ((wheel & 1) != (angle > 0.0f)) {
      wheelAngle = n_wheels >= 4
                       ? calc_ackerman_angle(angle, trackWidth, axleDistance)
                       : angle;
    }
    do_steering_wheel(static_cast<IVP_POS_WHEEL>(wheel), wheelAngle);
  }
  if (n_wheels > 4) {
    do_steering_wheel(static_cast<IVP_POS_WHEEL>(4), angle * 0.5f);
    do_steering_wheel(static_cast<IVP_POS_WHEEL>(5), angle * 0.5f);
  }
}

inline IVP_DOUBLE IVP_Car_System_Real_Wheels::get_orig_front_wheel_distance() {
  const IVP_U_Matrix &left =
      car_constraint_solver->wheel_objects.element_at(0)->target_position_bs;
  const IVP_U_Matrix &right =
      car_constraint_solver->wheel_objects.element_at(1)->target_position_bs;
  return -(left.get_position()->k[car_constraint_solver->x_idx] -
           right.get_position()->k[car_constraint_solver->x_idx]);
}

inline IVP_DOUBLE IVP_Car_System_Real_Wheels::get_orig_axles_distance() {
  const IVP_U_Matrix &front =
      car_constraint_solver->wheel_objects.element_at(0)->target_position_bs;
  const IVP_U_Matrix &rear =
      car_constraint_solver->wheel_objects.element_at(2)->target_position_bs;
  return std::fabs(front.get_position()->k[car_constraint_solver->z_idx] -
                   rear.get_position()->k[car_constraint_solver->z_idx]);
}

inline void
IVP_Car_System_Real_Wheels::get_skid_info(IVP_Wheel_Skid_Info *output) {
  for (int wheel = 0; wheel < n_wheels; ++wheel) {
    const IVP_Constraint_Car_Object *state =
        car_constraint_solver->wheel_objects.element_at(wheel);
    output[wheel].last_contact_position_ws = state->last_contact_position_ws;
    output[wheel].last_skid_value = state->last_skid_value;
    output[wheel].last_skid_time = state->last_skid_time;
  }
}

inline void IVP_Car_System_Real_Wheels::fix_wheel(IVP_POS_WHEEL position,
                                                  IVP_BOOL stopWheel) {
  const int wheelIndex = static_cast<int>(position);
  IVP_Constraint_Car_Object *wheelState =
      car_constraint_solver->wheel_objects.element_at(wheelIndex);
  if (!stopWheel) {
    wheelState->fix_wheel_constraint = nullptr;
    delete fix_wheel_constraint[wheelIndex];
    fix_wheel_constraint[wheelIndex] = nullptr;
    return;
  }
  if (fix_wheel_constraint[wheelIndex])
    return;

  IVP_Real_Object *wheel = car_wheel[wheelIndex];
  IVP_Template_Constraint definition;
  definition.set_reference_object(wheel);
  definition.set_attached_object(car_body);
  definition.fix_rotation_axis(IVP_INDEX_X);
  definition.free_rotation_axis(IVP_INDEX_Y);
  definition.free_rotation_axis(IVP_INDEX_Z);
  definition.free_translation_axis(IVP_INDEX_X);
  definition.free_translation_axis(IVP_INDEX_Y);
  definition.free_translation_axis(IVP_INDEX_Z);
  wheel->get_core()->rot_speed.add_multiple(&car_body->get_core()->rot_speed,
                                            0.25f);
  fix_wheel_constraint[wheelIndex] =
      environment->create_constraint(&definition);
  wheelState->fix_wheel_constraint = fix_wheel_constraint[wheelIndex];
}

inline void
IVP_Car_System_Real_Wheels::set_booster_acceleration(IVP_FLOAT acceleration) {
  if (acceleration == 0.0f) {
    delete booster_actuator[0];
    delete booster_actuator[1];
    booster_actuator[0] = nullptr;
    booster_actuator[1] = nullptr;
    return;
  }

  const IVP_FLOAT mass = car_body->get_core()->get_mass();
  if (booster_actuator[0]) {
    booster_actuator[0]->set_force(acceleration * mass);
    return;
  }

  IVP_Template_Anchor forwardAnchors[2];
  IVP_Template_Anchor upAnchors[2];
  IVP_Template_Force forwardForce;
  IVP_Template_Force upForce;
  IVP_U_Float_Point front;
  IVP_U_Float_Point back;
  front.set_to_zero();
  back.set_to_zero();
  front.k[car_constraint_solver->z_idx] = 1.0f;
  back.k[car_constraint_solver->z_idx] = -1.0f;
  forwardAnchors[0].set_anchor_position_cs(car_body, &front);
  forwardAnchors[1].set_anchor_position_cs(car_body, &back);

  IVP_U_Float_Point center;
  IVP_U_Float_Point down;
  center.set_to_zero();
  down.set_to_zero();
  down.k[car_constraint_solver->y_idx] = -1.0e8f;
  upAnchors[0].set_anchor_position_cs(car_body, &center);
  upAnchors[1].set_anchor_position_os(environment->get_static_object(), &down);

  forwardForce.anchors[0] = &forwardAnchors[0];
  forwardForce.anchors[1] = &forwardAnchors[1];
  forwardForce.active_float_force = nullptr;
  forwardForce.push_first_object = IVP_TRUE;
  forwardForce.push_second_object = IVP_FALSE;
  forwardForce.force = acceleration * mass;

  upForce.anchors[0] = &upAnchors[0];
  upForce.anchors[1] = &upAnchors[1];
  upForce.active_float_force = nullptr;
  upForce.push_first_object = IVP_TRUE;
  upForce.push_second_object = IVP_FALSE;
  upForce.force = static_cast<IVP_FLOAT>(
      -mass * environment->get_gravity()->k[car_constraint_solver->y_idx]);

  booster_actuator[0] = environment->create_force(&forwardForce);
  booster_actuator[1] = environment->create_force(&upForce);
}

inline void IVP_Car_System_Real_Wheels::activate_booster(IVP_FLOAT thrust,
                                                         IVP_FLOAT duration,
                                                         IVP_FLOAT delay) {
  if (booster_actuator[0] || booster_seconds_until_ready > 0.0f)
    return;
  set_booster_acceleration(
      thrust *
      static_cast<IVP_FLOAT>(environment->get_gravity()->real_length()));
  booster_seconds_to_go = duration;
  booster_seconds_until_ready = duration + delay;
}

inline void IVP_Car_System_Real_Wheels::update_booster(IVP_FLOAT deltaTime) {
  if (booster_seconds_until_ready > 0.0f)
    booster_seconds_until_ready -= deltaTime;
  if (booster_seconds_to_go > 0.0f) {
    booster_seconds_to_go -= deltaTime;
    if (booster_seconds_to_go <= 0.0f)
      set_booster_acceleration(0.0f);
  }
}

inline IVP_Car_System_Real_Wheels::IVP_Car_System_Real_Wheels(
    IVP_Environment *env, IVP_Template_Car_System *definition)
    : environment(env), n_wheels(definition->n_wheels),
      n_axis(definition->n_axis), car_body(definition->car_body),
      car_constraint_solver(nullptr), car_act_torque_body(nullptr),
      car_act_down_force(nullptr), car_act_extra_gravity(nullptr),
      body_counter_torque_factor(definition->body_counter_torque_factor),
      max_speed(0.0f), fast_turn_factor(definition->fast_turn_factor),
      booster_actuator{nullptr, nullptr}, booster_seconds_to_go(0.0f),
      booster_seconds_until_ready(0.0f), steering_angle(-1.0f),
      m_CarSystemDebugData{} {
  std::memset(car_wheel, 0, sizeof(car_wheel));
  std::memset(car_act_torque, 0, sizeof(car_act_torque));
  std::memset(car_spring, 0, sizeof(car_spring));
  std::memset(car_stabilizer, 0, sizeof(car_stabilizer));
  std::memset(fix_wheel_constraint, 0, sizeof(fix_wheel_constraint));
  std::memset(wheel_reversed_sign, 0, sizeof(wheel_reversed_sign));
  std::memset(wheel_radius, 0, sizeof(wheel_radius));

  for (int wheel = 0; wheel < n_wheels; ++wheel) {
    car_wheel[wheel] = definition->car_wheel[wheel];
    wheel_reversed_sign[wheel] = definition->wheel_reversed_sign[wheel];
    wheel_radius[wheel] = definition->wheel_radius[wheel];
  }

  car_constraint_solver = new IVP_Constraint_Solver_Car(
      definition->index_x, definition->index_y, definition->index_z,
      definition->is_left_handed);
  IVP_U_Vector<IVP_Real_Object> wheels;
  IVP_U_Vector<IVP_U_Float_Point> hardPoints;
  for (int wheel = 0; wheel < n_wheels; ++wheel) {
    wheels.add(car_wheel[wheel]);
    hardPoints.add(&definition->wheel_pos_Bos[wheel]);
  }
  car_constraint_solver->init_constraint_system(environment, car_body, wheels,
                                                hardPoints);

  IVP_Template_Anchor bodyAnchors[IVP_CAR_SYSTEM_MAX_WHEELS];
  IVP_Template_Anchor wheelAnchors[IVP_CAR_SYSTEM_MAX_WHEELS];
  for (int wheel = 0; wheel < n_wheels; ++wheel) {
    IVP_Template_Suspension spring;
    spring.spring_values_are_relative = IVP_FALSE;
    spring.spring_constant = definition->spring_constant[wheel];
    spring.spring_damp = definition->spring_dampening[wheel];
    spring.spring_dampening_compression =
        definition->spring_dampening_compression[wheel];
    spring.max_body_force = definition->max_body_force[wheel];
    spring.rel_pos_damp = 0.0f;
    spring.spring_len = 500.0f;

    IVP_U_Float_Point hardPoint(&definition->wheel_pos_Bos[wheel]);
    hardPoint.k[car_constraint_solver->y_idx] -=
        static_cast<IVP_FLOAT>(spring.spring_len);
    bodyAnchors[wheel].set_anchor_position_os(car_body, &hardPoint);
    wheelAnchors[wheel].set_anchor_position_os(car_wheel[wheel], 0.0f, 0.0f,
                                               0.0f);
    spring.anchors[0] = &bodyAnchors[wheel];
    spring.anchors[1] = &wheelAnchors[wheel];
    spring.spring_len -= definition->spring_pre_tension[wheel];
    car_spring[wheel] = environment->create_suspension(&spring);

    IVP_Template_Anchor torqueAnchors[2];
    IVP_Template_Torque torque;
    torque.max_rotation_speed =
        definition->wheel_max_rotation_speed[(wheel & 2) ? 1 : 0];
    IVP_U_Float_Point axis;
    axis.set_to_zero();
    axis.k[car_constraint_solver->x_idx] =
        definition->wheel_reversed_sign[wheel];
    torqueAnchors[0].set_anchor_position_os(car_wheel[wheel], &axis);
    axis.k[car_constraint_solver->x_idx] =
        -definition->wheel_reversed_sign[wheel];
    torqueAnchors[1].set_anchor_position_os(car_wheel[wheel], &axis);
    torque.anchors[0] = &torqueAnchors[0];
    torque.anchors[1] = &torqueAnchors[1];
    torque.torque = 0.0f;
    car_act_torque[wheel] = environment->create_torque(&torque);
  }

  IVP_Template_Anchor counterAnchors[2];
  IVP_Template_Torque counterTorque;
  counterTorque.max_rotation_speed =
      static_cast<IVP_FLOAT>(3.14159265358979323846 * 100.0);
  IVP_U_Float_Point axis;
  axis.set_to_zero();
  axis.k[car_constraint_solver->x_idx] = 1.0f;
  counterAnchors[0].set_anchor_position_os(car_body, &axis);
  axis.k[car_constraint_solver->x_idx] = -1.0f;
  counterAnchors[1].set_anchor_position_os(car_body, &axis);
  counterTorque.anchors[0] = &counterAnchors[0];
  counterTorque.anchors[1] = &counterAnchors[1];
  counterTorque.torque = 0.0f;
  car_act_torque_body = environment->create_torque(&counterTorque);

  if (n_wheels != n_axis) {
    for (int axisIndex = 0; axisIndex < 2; ++axisIndex) {
      IVP_Template_Stabilizer stabilizer;
      stabilizer.stabi_constant = definition->stabilizer_constant[axisIndex];
      stabilizer.anchors[0] = &bodyAnchors[axisIndex * 2];
      stabilizer.anchors[1] = &wheelAnchors[axisIndex * 2];
      stabilizer.anchors[2] = &bodyAnchors[axisIndex * 2 + 1];
      stabilizer.anchors[3] = &wheelAnchors[axisIndex * 2 + 1];
      car_stabilizer[axisIndex] = environment->create_stabilizer(&stabilizer);
    }
  }

  IVP_Real_Object *staticObject = environment->get_static_object();
  IVP_Template_Anchor forceAnchors[2];
  IVP_Template_Force force;
  IVP_U_Float_Point point;
  point.set_to_zero();
  point.k[car_constraint_solver->y_idx] =
      definition->body_down_force_vertical_offset;
  forceAnchors[0].set_anchor_position_cs(car_body, &point);
  point.k[car_constraint_solver->y_idx] =
      environment->get_gravity()->k[car_constraint_solver->y_idx] > 0.0f
          ? -1.0e8f
          : 1.0e8f;
  forceAnchors[1].set_anchor_position_os(staticObject, &point);
  force.anchors[0] = &forceAnchors[0];
  force.anchors[1] = &forceAnchors[1];
  force.force = 0.0f;
  force.push_first_object = IVP_TRUE;
  force.push_second_object = IVP_FALSE;
  car_act_down_force = environment->create_force(&force);

  IVP_U_Float_Point center;
  center.set_to_zero();
  center.k[car_constraint_solver->y_idx] =
      definition->extra_gravity_height_offset;
  forceAnchors[0].set_anchor_position_cs(car_body, &center);
  point.set_to_zero();
  point.k[car_constraint_solver->y_idx] =
      environment->get_gravity()->k[car_constraint_solver->y_idx] > 0.0f
          ? -1.0e8f
          : 1.0e8f;
  forceAnchors[1].set_anchor_position_os(staticObject, &point);
  force.force = definition->extra_gravity_force_value;
  car_act_extra_gravity = environment->create_force(&force);

  for (int wheel = 0; wheel < n_wheels; ++wheel)
    do_steering_wheel(static_cast<IVP_POS_WHEEL>(wheel), 0.0f);
  do_steering(0.0f);
}

struct BML_IvpRealWheelsCarLayoutCheck {
  static_assert(offsetof(IVP_Car_System_Real_Wheels, car_body) == 0x10);
  static_assert(offsetof(IVP_Car_System_Real_Wheels, car_constraint_solver) ==
                0x3C);
  static_assert(offsetof(IVP_Car_System_Real_Wheels, car_spring) == 0x6C);
  static_assert(offsetof(IVP_Car_System_Real_Wheels, wheel_reversed_sign) ==
                0xD8);
  static_assert(offsetof(IVP_Car_System_Real_Wheels, booster_actuator) ==
                0x134);
  static_assert(offsetof(IVP_Car_System_Real_Wheels, m_CarSystemDebugData) ==
                0x148);
};

static_assert(sizeof(IVP_Car_System_Real_Wheels) == 0x450);

#endif // BML_IVP_REAL_WHEELS_CAR_H
