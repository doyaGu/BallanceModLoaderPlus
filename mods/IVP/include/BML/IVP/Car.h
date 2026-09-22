#ifndef BML_IVP_CAR_H
#define BML_IVP_CAR_H

#include "BML/IVP/Types.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

class IVP_Core;
class IVP_Real_Object;
struct BML_IvpRaycastCoreVectorLayoutCheck;

enum IVP_POS_WHEEL : std::int32_t {
    IVP_FRONT_LEFT = 0,
    IVP_FRONT_RIGHT = 1,
    IVP_REAR_LEFT = 2,
    IVP_REAR_RIGHT = 3,
    IVP_CAR_SYSTEM_MAX_WHEELS = 10,
};

enum IVP_POS_AXIS : std::int32_t {
    IVP_FRONT = 0,
    IVP_REAR = 1,
    IVP_CAR_SYSTEM_MAX_AXIS = 5,
};

// Configuration shared by the real-wheel and raycast implementations. The
// layout follows Ballance's imported 0x304 UDT. Its constructor is one of the
// small source-inline bodies that does not survive in physics_RT.dll.
class IVP_Template_Car_System {
public:
    IVP_Template_Car_System(int wheelCount, int axisCount) {
        std::memset(this, 0, sizeof(*this));
        n_wheels = wheelCount;
        n_axis = axisCount;
        for (int index = 0; index < n_wheels; ++index) {
            wheel_reversed_sign[index] = 1.0f;
            index_x = IVP_INDEX_X;
            index_y = IVP_INDEX_Y;
            index_z = IVP_INDEX_Z;
            is_left_handed = IVP_FALSE;
        }
        fast_turn_factor = 1.0f;
    }

    int n_wheels;
    int n_axis;
    IVP_COORDINATE_INDEX index_x;
    IVP_COORDINATE_INDEX index_y;
    IVP_COORDINATE_INDEX index_z;
    IVP_BOOL is_left_handed;
    IVP_Real_Object *car_body;
    IVP_Real_Object *car_wheel[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT friction_of_wheel[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT wheel_radius[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT wheel_reversed_sign[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT body_counter_torque_factor;
    IVP_FLOAT extra_gravity_force_value;
    IVP_FLOAT extra_gravity_height_offset;
    IVP_FLOAT body_down_force_vertical_offset;
    IVP_FLOAT fast_turn_factor;
    IVP_U_Float_Point wheel_pos_Bos[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_U_Float_Point trace_pos_Bos[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT spring_constant[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT spring_dampening[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT spring_dampening_compression[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT max_body_force[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT spring_pre_tension[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT raycast_startpoint_height_offset;
    IVP_FLOAT stabilizer_constant[IVP_CAR_SYSTEM_MAX_AXIS];
    IVP_FLOAT wheel_max_rotation_speed[IVP_CAR_SYSTEM_MAX_AXIS];
};

class IVP_Wheel_Skid_Info {
public:
    IVP_U_Float_Point last_contact_position_ws;
    IVP_FLOAT last_skid_value;
    IVP_Time last_skid_time;
};

struct IVP_CarSystemDebugData_t {
    IVP_U_Point wheelRaycasts[IVP_CAR_SYSTEM_MAX_WHEELS][2];
    IVP_FLOAT wheelRaycastImpacts[IVP_CAR_SYSTEM_MAX_WHEELS];
    IVP_FLOAT wheelRotationalTorque[4][3];
    IVP_FLOAT wheelTranslationTorque[4][3];

    // A neighboring, later IVP tree appends four actuator vectors here.
    // Ballance's embedded debug block ends at 0x308, so they are omitted.
};

// Ballance's imported type has 28 virtual slots. In particular, the nearby
// source revision's set_powerslide, get_booster_time_to_go and
// event_object_deleted slots are not present, and do_steering takes one float.
class IVP_Car_System {
public:
    virtual ~IVP_Car_System() = default;
    IVP_Car_System() = default;

    virtual void do_steering_wheel(
        IVP_POS_WHEEL wheelPosition, IVP_FLOAT steeringAngle) = 0;
    virtual void change_spring_constant(
        IVP_POS_WHEEL position, IVP_FLOAT springConstant) = 0;
    virtual void change_spring_dampening(
        IVP_POS_WHEEL position, IVP_FLOAT springDampening) = 0;
    virtual void change_spring_dampening_compression(
        IVP_POS_WHEEL position, IVP_FLOAT springDampening) = 0;
    virtual void change_max_body_force(
        IVP_POS_WHEEL position, IVP_FLOAT maximumForce) = 0;
    virtual void change_spring_pre_tension(
        IVP_POS_WHEEL position, IVP_FLOAT preTensionLength) = 0;
    virtual void change_spring_length(
        IVP_POS_WHEEL position, IVP_FLOAT springLength) = 0;
    virtual void change_stabilizer_constant(
        IVP_POS_AXIS position, IVP_FLOAT stabilizerConstant) = 0;
    virtual void change_fast_turn_factor(IVP_FLOAT fastTurnFactor) = 0;
    virtual void change_wheel_torque(
        IVP_POS_WHEEL position, IVP_FLOAT torque) = 0;
    virtual void update_body_countertorque() = 0;
    virtual void update_throttle(IVP_FLOAT throttle) = 0;
    virtual void change_body_downforce(IVP_FLOAT force) = 0;
    virtual void fix_wheel(
        IVP_POS_WHEEL position, IVP_BOOL stopWheel) = 0;
    virtual IVP_DOUBLE get_body_speed(
        IVP_COORDINATE_INDEX forwardAxis = IVP_INDEX_Z) = 0;
    virtual IVP_DOUBLE get_wheel_angular_velocity(
        IVP_POS_WHEEL position) = 0;
    virtual void update_wheel_positions() = 0;
    virtual IVP_DOUBLE get_orig_front_wheel_distance() = 0;
    virtual IVP_DOUBLE get_orig_axles_distance() = 0;
    virtual void get_skid_info(IVP_Wheel_Skid_Info *skidInfo) = 0;
    virtual void do_steering(IVP_FLOAT steeringAngle) = 0;
    virtual void set_booster_acceleration(IVP_FLOAT acceleration) = 0;
    virtual void activate_booster(
        IVP_FLOAT thrust, IVP_FLOAT duration, IVP_FLOAT rechargeTime) = 0;
    virtual void update_booster(IVP_FLOAT deltaTime) = 0;
    virtual IVP_FLOAT get_booster_delay() = 0;
    virtual void SetCarSystemDebugData(
        const IVP_CarSystemDebugData_t &debugData) = 0;
    virtual void GetCarSystemDebugData(
        IVP_CarSystemDebugData_t &debugData) = 0;

    static IVP_FLOAT calc_ackerman_angle(
        IVP_FLOAT innerAngle, IVP_FLOAT frontWheelDistance,
        IVP_FLOAT axleDistance) {
        const IVP_FLOAT angle = std::fabs(innerAngle);
        if (angle < 0.001f)
            return innerAngle;

        const IVP_DOUBLE tangent = std::tan(angle);
        const IVP_DOUBLE ratio =
            (axleDistance * tangent) /
            (frontWheelDistance * tangent + axleDistance);
        const IVP_DOUBLE sign = innerAngle < 0.0f ? -1.0 : 1.0;
        return static_cast<IVP_FLOAT>(std::atan(ratio) * sign);
    }
};

class IVP_Raycast_Car_Wheel {
public:
    IVP_U_Float_Point hp_cs;
    IVP_U_Float_Point spring_direction_cs;
    IVP_FLOAT distance_orig_hp_to_hp;
    IVP_FLOAT spring_len;
    IVP_FLOAT spring_constant;
    IVP_FLOAT spring_damp_relax;
    IVP_FLOAT spring_damp_compress;
    IVP_FLOAT max_rotation_speed;
    IVP_FLOAT wheel_radius;
    IVP_FLOAT inv_wheel_radius;
    IVP_FLOAT friction_of_wheel;
    IVP_FLOAT torque;
    IVP_BOOL wheel_is_fixed;
    IVP_U_Float_Point axis_direction_cs;
    IVP_FLOAT angle_wheel;
    IVP_FLOAT wheel_angular_velocity;
    IVP_U_Float_Point surface_speed_of_wheel_on_ground_ws;
    IVP_FLOAT pressure;
    IVP_FLOAT raycast_dist;
};

class IVP_Raycast_Car_Wheel_Temp {
public:
    IVP_FLOAT friction_value;
    IVP_FLOAT stabilizer_force;
    IVP_Real_Object *moveable_object_hit_by_ray;
    IVP_U_Float_Point ground_normal_ws;
    IVP_U_Point ground_hit_ws;
    IVP_U_Float_Point spring_direction_ws;
    IVP_U_Float_Point surface_speed_wheel_ws;
    IVP_U_Float_Point projected_surface_speed_wheel_ws;
    IVP_U_Float_Point axis_direction_ws;
    IVP_U_Float_Point projected_axis_direction_ws;
    IVP_FLOAT forces_needed_to_drive_straight;
    IVP_FLOAT inv_normal_dot_dir;
};

class IVP_Raycast_Car_Axis {
public:
    IVP_FLOAT stabilizer_constant;
};

class IVP_Controller_Raycast_Car_Vector_of_Cores_1
    : public IVP_U_Vector<IVP_Core> {
    friend struct BML_IvpRaycastCoreVectorLayoutCheck;

public:
    IVP_Controller_Raycast_Car_Vector_of_Cores_1()
        : IVP_U_Vector<IVP_Core>(&elem_buffer[0], 1) {}

private:
    void *elem_buffer[1];
};

static_assert(sizeof(IVP_Template_Car_System) == 0x304);
static_assert(offsetof(IVP_Template_Car_System, car_body) == 0x18);
static_assert(offsetof(IVP_Template_Car_System, wheel_reversed_sign) == 0x94);
static_assert(offsetof(IVP_Template_Car_System, wheel_pos_Bos) == 0xD0);
static_assert(offsetof(IVP_Template_Car_System,
                       raycast_startpoint_height_offset) == 0x2D8);
static_assert(sizeof(IVP_Wheel_Skid_Info) == 0x20);
static_assert(sizeof(IVP_CarSystemDebugData_t) == 0x308);
static_assert(sizeof(IVP_Car_System) == 0x04);
static_assert(sizeof(IVP_Raycast_Car_Wheel) == 0x7C);
static_assert(offsetof(IVP_Raycast_Car_Wheel, spring_direction_cs) == 0x10);
static_assert(offsetof(IVP_Raycast_Car_Wheel, spring_len) == 0x24);
static_assert(offsetof(IVP_Raycast_Car_Wheel, wheel_radius) == 0x38);
static_assert(offsetof(IVP_Raycast_Car_Wheel, wheel_is_fixed) == 0x48);
static_assert(offsetof(IVP_Raycast_Car_Wheel, axis_direction_cs) == 0x4C);
static_assert(offsetof(IVP_Raycast_Car_Wheel,
                       surface_speed_of_wheel_on_ground_ws) == 0x64);
static_assert(sizeof(IVP_Raycast_Car_Wheel_Temp) == 0x98);
static_assert(offsetof(IVP_Raycast_Car_Wheel_Temp, ground_normal_ws) == 0x0C);
static_assert(offsetof(IVP_Raycast_Car_Wheel_Temp, ground_hit_ws) == 0x20);
static_assert(
    offsetof(IVP_Raycast_Car_Wheel_Temp, spring_direction_ws) == 0x40);
static_assert(
    offsetof(IVP_Raycast_Car_Wheel_Temp, axis_direction_ws) == 0x70);
static_assert(offsetof(IVP_Raycast_Car_Wheel_Temp,
                       forces_needed_to_drive_straight) == 0x90);
static_assert(
    offsetof(IVP_Raycast_Car_Wheel_Temp, inv_normal_dot_dir) == 0x94);
static_assert(sizeof(IVP_Raycast_Car_Axis) == 0x04);
static_assert(offsetof(IVP_Raycast_Car_Axis, stabilizer_constant) == 0x00);
static_assert(sizeof(IVP_Controller_Raycast_Car_Vector_of_Cores_1) == 0x0C);

struct BML_IvpRaycastCoreVectorLayoutCheck {
    static_assert(offsetof(IVP_Controller_Raycast_Car_Vector_of_Cores_1,
                           elem_buffer) == 0x08);
};

#endif // BML_IVP_CAR_H
