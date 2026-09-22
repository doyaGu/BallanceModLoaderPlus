#ifndef BML_IVP_ACTUATOR_H
#define BML_IVP_ACTUATOR_H

#include "BML/IVP/Cache.h"
#include "BML/IVP/Controller.h"
#include "BML/IVP/ActiveValue.h"

#include <cstddef>
#include <new>

class IVP_Actuator;
class IVP_Actuator_Check_Dist;
class IVP_Actuator_Spring;
class IVP_Actuator_Suspension;
class IVP_Controller_Stiff_Spring;
class IVP_Listener_Spring;
class IVP_U_Active_Float;

enum IVP_ACTUATOR_TYPE : std::int32_t {
    IVP_ACTUATOR_TYPE_NONE = 0,
    IVP_ACTUATOR_TYPE_SPRING = 1,
    IVP_ACTUATOR_TYPE_STIFF_SPRING = 2,
    IVP_ACTUATOR_TYPE_SUSPENSION = 3,
    IVP_ACTUATOR_TYPE_FORCE = 4,
    IVP_ACTUATOR_TYPE_ROT_MOT = 5,
    IVP_ACTUATOR_TYPE_STABILIZER = 6,
    IVP_ACTUATOR_TYPE_TORQUE = 7,
    IVP_ACTUATOR_TYPE_ETC = 8,
};

enum IVP_SPRING_FORCE_EXCEED : std::int32_t {
    IVP_SFE_NONE = 0,
    IVP_SFE_BREAK = 1,
};

class IVP_Template_Anchor {
    friend struct BML_IvpTemplateAnchorLayoutCheck;

public:
    IVP_Real_Object *get_object() { return object; }
    IVP_Real_Object *get_object() const { return object; }
    IVP_U_Point *get_anchor_point_ws() { return &coords_world; }
    const IVP_U_Point *get_anchor_position_ws() const { return &coords_world; }

    void set_anchor_position_ws(IVP_Real_Object *value,
                                const IVP_U_Point *position) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateAnchorSetWorldPosition,
            this, value, position);
    }
    void set_anchor_position_ws(IVP_Real_Object *value, const IVP_DOUBLE x,
                                const IVP_DOUBLE y, const IVP_DOUBLE z) {
        IVP_U_Point position(x, y, z);
        set_anchor_position_ws(value, &position);
    }
    void set_anchor_position_os(IVP_Real_Object *value,
                                const IVP_U_Float_Point *position) {
        object = value;
        IVP_Cache_Object *cache = value->get_cache_object_no_lock();
        if (cache)
            cache->transform_position_to_world_coords(position, &coords_world);
    }
    void set_anchor_position_os(IVP_Real_Object *value, const IVP_DOUBLE x,
                                const IVP_DOUBLE y, const IVP_DOUBLE z) {
        const IVP_U_Float_Point position(x, y, z);
        set_anchor_position_os(value, &position);
    }
    void set_anchor_position_cs(IVP_Real_Object *value,
                                const IVP_U_Float_Point *position) {
        IVP_U_Matrix coreFromObject;
        value->calc_m_core_f_object(&coreFromObject);
        IVP_U_Float_Point objectPosition;
        coreFromObject.vimult4(position, &objectPosition);
        set_anchor_position_os(value, &objectPosition);
    }
    void set_anchor_position_cs(IVP_Real_Object *value, const IVP_DOUBLE x,
                                const IVP_DOUBLE y, const IVP_DOUBLE z) {
        const IVP_U_Float_Point position(x, y, z);
        set_anchor_position_cs(value, &position);
    }

private:
    IVP_Real_Object *object = nullptr;
    IVP_U_Point coords_world;
};

struct BML_IvpTemplateAnchorLayoutCheck {
    static constexpr std::size_t object = offsetof(IVP_Template_Anchor, object);
    static constexpr std::size_t position =
        offsetof(IVP_Template_Anchor, coords_world);
};

struct IVP_Template_Two_Point {
    struct Retail_Construction_Tag {};

    IVP_Template_Two_Point() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::TemplateTwoPointConstruct, this,
            [this] {
                client_data = nullptr;
                anchors[0] = nullptr;
                anchors[1] = nullptr;
            });
    }

protected:
    explicit IVP_Template_Two_Point(Retail_Construction_Tag) noexcept {}

public:
    void *client_data;
    IVP_Template_Anchor *anchors[2];
};

struct IVP_Template_Four_Point {
    IVP_Template_Four_Point() = default;

    void *client_data = nullptr;
    IVP_Template_Anchor *anchors[4]{};
};

// Ballance's template ends at 0x38. In particular, it does not contain the
// spring_force_only_on_stretch member present in the nearby source revision.
struct IVP_Template_Spring : IVP_Template_Two_Point {
    IVP_Template_Spring()
        : IVP_Template_Two_Point(Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::TemplateSpringConstruct, this,
            [this] {
                client_data = nullptr;
                anchors[0] = nullptr;
                anchors[1] = nullptr;
                spring_len = 0.0f;
                spring_values_are_relative = IVP_FALSE;
                spring_constant = 0.0f;
                spring_damp = 0.0f;
                rel_pos_damp = 0.0f;
                max_len_exceed_type = IVP_SFE_NONE;
                break_max_len = 1.0e20f;
                active_float_spring_len = nullptr;
                active_float_spring_constant = nullptr;
                active_float_spring_damp = nullptr;
                active_float_spring_rel_pos_damp = nullptr;
            });
    }

    IVP_FLOAT spring_len;
    IVP_BOOL spring_values_are_relative;
    IVP_FLOAT spring_constant;
    IVP_FLOAT spring_damp;
    IVP_FLOAT rel_pos_damp;
    IVP_SPRING_FORCE_EXCEED max_len_exceed_type;
    IVP_FLOAT break_max_len;
    IVP_U_Active_Float *active_float_spring_len;
    IVP_U_Active_Float *active_float_spring_constant;
    IVP_U_Active_Float *active_float_spring_damp;
    IVP_U_Active_Float *active_float_spring_rel_pos_damp;
};

// The Suspension constructor was discarded by the Ballance link, while its
// imported UDT remains complete.  The nearby implementation clears the full
// derived template after the Spring base constructor and then selects a
// smaller break threshold of 10e8f.
struct IVP_Template_Suspension : IVP_Template_Spring {
    IVP_Template_Suspension() {
        client_data = nullptr;
        anchors[0] = nullptr;
        anchors[1] = nullptr;
        spring_len = 0.0f;
        spring_values_are_relative = IVP_FALSE;
        spring_constant = 0.0f;
        spring_damp = 0.0f;
        rel_pos_damp = 0.0f;
        max_len_exceed_type = IVP_SFE_NONE;
        break_max_len = 1.0e9f;
        active_float_spring_len = nullptr;
        active_float_spring_constant = nullptr;
        active_float_spring_damp = nullptr;
        active_float_spring_rel_pos_damp = nullptr;
        spring_dampening_compression = 0.0f;
        max_body_force = 0.0f;
    }

    IVP_FLOAT spring_dampening_compression;
    IVP_FLOAT max_body_force;
};

// This is the separate, constraint-like stiff spring from IVP's public API;
// it is not an alias for Ballance's IVP_Actuator_Spring.  The retail type
// information preserves these exact fields even though the linker removed
// every Stiff_Spring function body from physics_RT.dll.
struct IVP_Template_Stiff_Spring : IVP_Template_Two_Point {
    IVP_Template_Stiff_Spring()
        : spring_len(0.0f), spring_constant(0.0f), spring_damp(0.0f),
          max_len_exceed_type(IVP_SFE_NONE),
          break_max_len(3.402823466e+38F) {}

    IVP_FLOAT spring_len;
    IVP_FLOAT spring_constant;
    IVP_FLOAT spring_damp;
    IVP_SPRING_FORCE_EXCEED max_len_exceed_type;
    IVP_FLOAT break_max_len;
};

struct IVP_Template_Stiff_Spring_Active : IVP_Template_Stiff_Spring {
    IVP_Template_Stiff_Spring_Active()
        : active_float_spring_len(nullptr),
          active_float_spring_constant(nullptr),
          active_float_spring_damp(nullptr) {}

    IVP_U_Active_Float *active_float_spring_len;
    IVP_U_Active_Float *active_float_spring_constant;
    IVP_U_Active_Float *active_float_spring_damp;
};

class IVP_Template_Check_Dist {
public:
    IVP_Template_Check_Dist()
        : client_data(nullptr), objects{}, range(0.0f),
          mod_is_outside(nullptr) {
        for (IVP_U_Point &position : position_world_space) {
            position.set_to_zero();
            position.hesse_val = 0.0;
        }
    }

    void *client_data;
    IVP_Real_Object *objects[2];
    IVP_U_Point position_world_space[2];
    IVP_FLOAT range;
    IVP_U_Active_Terminal_Int *mod_is_outside;
};

class IVP_Template_Stabilizer : public IVP_Template_Four_Point {
public:
    IVP_Template_Stabilizer()
        : stabi_constant(0.0f), active_float_stabi_constant(nullptr) {}

    IVP_FLOAT stabi_constant;
    IVP_U_Active_Float *active_float_stabi_constant;
};

// Ballance does not retain this constructor or the Force implementation in
// physics_RT.dll.  The layout is nevertheless part of the public IVP ABI and
// is corroborated by the imported retail type: unlike the runtime object's
// flags, these two template booleans are full 32-bit IVP_BOOL values.
struct IVP_Template_Force : IVP_Template_Two_Point {
    IVP_Template_Force()
        : force(0.0f), active_float_force(nullptr),
          push_first_object(IVP_TRUE), push_second_object(IVP_FALSE) {}

    IVP_FLOAT force;
    IVP_U_Active_Float *active_float_force;
    IVP_BOOL push_first_object;
    IVP_BOOL push_second_object;
};

struct IVP_Template_Torque : IVP_Template_Two_Point {
    IVP_Template_Torque()
        : torque(0.0f), active_float_torque(nullptr),
          max_rotation_speed(0.0f),
          active_float_max_rotation_speed(nullptr),
          active_float_rotation_speed_out(nullptr) {}

    IVP_FLOAT torque;
    IVP_U_Active_Float *active_float_torque;
    IVP_FLOAT max_rotation_speed;
    IVP_U_Active_Float *active_float_max_rotation_speed;
    IVP_U_Active_Terminal_Double *active_float_rotation_speed_out;
};

struct IVP_Template_Rot_Mot : IVP_Template_Two_Point {
    IVP_Template_Rot_Mot()
        : max_rotation_speed(0.0f), power(0.0f), max_torque(0.0f),
          active_float_max_rotation_speed(nullptr),
          active_float_power(nullptr), active_float_max_torque(nullptr),
          active_float_rotation_speed_out(nullptr) {}

    IVP_FLOAT max_rotation_speed;
    IVP_FLOAT power;
    IVP_FLOAT max_torque;
    IVP_U_Active_Float *active_float_max_rotation_speed;
    IVP_U_Active_Float *active_float_power;
    IVP_U_Active_Float *active_float_max_torque;
    IVP_U_Active_Terminal_Double *active_float_rotation_speed_out;
};

// Demo-oriented in the neighboring tree, but the exact Ballance IDB retains
// complete 0x40/0x50 layouts for both data records.  No runtime Extra actuator
// body or vtable is exposed; these types only restore the supported public
// configuration surface.
struct IVP_Extra_Info {
    IVP_Extra_Info()
        : client_data(nullptr), is_float_cam(IVP_FALSE),
          active_float_bomb(nullptr), range(0.0), mod_fc_height(nullptr),
          mod_fc_target_height(nullptr), mod_fc_dist(nullptr),
          mod_fc_speed(nullptr), active_float_force(nullptr),
          is_physic_cam(0), is_puck_force(0), mod_pf_forward(nullptr),
          mod_pf_sideward(nullptr) {}

    void *client_data;
    IVP_BOOL is_float_cam;
    IVP_U_Active_Float *active_float_bomb;
    IVP_DOUBLE range;
    IVP_U_Active_Float *mod_fc_height;
    IVP_U_Active_Float *mod_fc_target_height;
    IVP_U_Active_Float *mod_fc_dist;
    IVP_U_Active_Float *mod_fc_speed;
    IVP_U_Active_Float *active_float_force;
    int is_physic_cam;
    int is_puck_force;
    IVP_U_Active_Float *mod_pf_forward;
    IVP_U_Active_Float *mod_pf_sideward;
};

struct IVP_Template_Extra : IVP_Template_Two_Point {
    IVP_Extra_Info info;
};

class IVP_Anchor {
    friend class IVP_Actuator_Two_Point;
    friend class IVP_Actuator_Four_Point;

public:
    IVP_Anchor() = default;

    IVP_Anchor *anchor_next_in_object;
    IVP_Anchor *anchor_prev_in_object;
    IVP_Real_Object *l_anchor_object;
    IVP_U_Float_Point object_pos;
    IVP_U_Float_Point core_pos;
    IVP_Actuator *l_actuator;

    IVP_Anchor *get_next_anchor() { return anchor_next_in_object; }
    IVP_Anchor *get_next_anchor() const { return anchor_next_in_object; }
    IVP_Anchor *get_prev_anchor() { return anchor_prev_in_object; }
    IVP_Anchor *get_prev_anchor() const { return anchor_prev_in_object; }
    IVP_Real_Object *anchor_get_real_object() { return l_anchor_object; }
    IVP_Real_Object *anchor_get_real_object() const { return l_anchor_object; }

    void init_anchor(IVP_Actuator *actuator,
                     IVP_Template_Anchor *configuration) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnchorInitialize,
            this, actuator, configuration);
    }

    // The top-level body is link-stripped, but Ballance retains the complete
    // anchor layout and both transforms used by init_anchor. Moving an anchor
    // does not change its object/list ownership; it only refreshes the two
    // coordinate-space representations of the same point.
    IVP_Anchor *move_anchor(IVP_U_Point *worldCoordinates) {
        IVP_Cache_Object *cache =
            l_anchor_object->get_cache_object_no_lock();
        IVP_U_Point objectPosition;
        cache->transform_position_to_object_coords(
            worldCoordinates, &objectPosition);
        object_pos.set(&objectPosition);

        IVP_U_Matrix coreFromObject;
        l_anchor_object->calc_m_core_f_object(&coreFromObject);
        coreFromObject.vmult4(&object_pos, &core_pos);
        return this;
    }

    void object_is_going_to_be_deleted_event(IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnchorObjectDeleted, this, object);
    }

private:
    ~IVP_Anchor() {
        if (l_anchor_object) {
            BML::IVP::ABI::InvokeThis<void>(
                BML::IVP::ABI::Address::AnchorDestruct, this);
        }
    }
};

class IVP_Actuator : public IVP_Controller_Dependent {
protected:
    struct Retail_Construction_Tag {};

    explicit IVP_Actuator(Retail_Construction_Tag) noexcept {}

    void initialize_reconstructed(IVP_Environment *) {
        ::new (static_cast<void *>(&actuator_controlled_cores))
            IVP_U_Vector<IVP_Core>();
    }

public:
    explicit IVP_Actuator(IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActuatorConstruct,
            this,
            [this, environment] { initialize_reconstructed(environment); },
            environment);
    }

    // The retained complete destructor owns this vector. Keep it in a union so
    // the host compiler does not destroy the same retail-owned storage again.
    union {
        IVP_U_Vector<IVP_Core> actuator_controlled_cores;
    };

    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        return BML::IVP::ABI::InvokeThisOr<IVP_U_Vector<IVP_Core> *>(
            BML::IVP::ABI::Address::ActuatorGetControlledCores, this,
            [this] { return &actuator_controlled_cores; });
    }
    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return BML::IVP::ABI::InvokeThisOr<IVP_CONTROLLER_PRIORITY>(
            BML::IVP::ABI::Address::ActuatorGetPriority, this,
            [] { return IVP_CP_ACTUATOR; });
    }
    virtual void anchor_will_be_deleted_event(IVP_Anchor *) {
        delete this;
    }
    void core_is_going_to_be_deleted_event(IVP_Core *) override {
        delete this;
    }
    ~IVP_Actuator() override {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActuatorDestruct, this,
            [this] { actuator_controlled_cores.~IVP_U_Vector<IVP_Core>(); });
    }

    BML_IVP_RETAIL_ALLOCATED_OBJECT;
};

class IVP_Actuator_Two_Point : public IVP_Actuator {
protected:
    struct Retail_Construction_Tag {};

    // Storage-only path for a more-derived retained complete constructor.
    // The DLL body will construct the Actuator vector and both anchors.
    explicit IVP_Actuator_Two_Point(Retail_Construction_Tag) noexcept
        : IVP_Actuator(IVP_Actuator::Retail_Construction_Tag{}) {}

    // RVA 0x14020 owns the complete base/vector/anchor construction chain.
    // Union storage prevents the host compiler from constructing the anchors
    // before that retained body enters its vector-constructor iterator.
    union {
        IVP_Anchor anchors[2];
    };

    void initialize_reconstructed(
        IVP_Environment *environment,
        IVP_Template_Two_Point *definition) {
        IVP_Actuator::initialize_reconstructed(environment);
        ::new (static_cast<void *>(&anchors[0])) IVP_Anchor();
        ::new (static_cast<void *>(&anchors[1])) IVP_Anchor();

        client_data = definition->client_data;
        anchors[0].init_anchor(this, definition->anchors[0]);
        anchors[1].init_anchor(this, definition->anchors[1]);

        IVP_Core *firstCore = anchors[0].l_anchor_object->get_core();
        IVP_Core *secondCore = anchors[1].l_anchor_object->get_core();
        if ((firstCore->flags & 0x0Cu) == 0u)
            actuator_controlled_cores.add(firstCore);
        if ((secondCore->flags & 0x0Cu) == 0u && secondCore != firstCore)
            actuator_controlled_cores.add(secondCore);
        if (firstCore->get_environment()) {
            firstCore->get_environment()->get_controller_manager()
                ->announce_controller_to_environment(this);
        }
    }

public:
    IVP_Actuator_Two_Point(
        IVP_Environment *environment,
        IVP_Template_Two_Point *definition,
        IVP_ACTUATOR_TYPE actuatorType)
        : IVP_Actuator(IVP_Actuator::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActuatorTwoPointConstruct, this,
            [this, environment, definition] {
                initialize_reconstructed(environment, definition);
            },
            environment, definition, actuatorType);
    }

    ~IVP_Actuator_Two_Point() override {
        IVP_Real_Object *object = anchors[0].l_anchor_object;
        IVP_Core *core = object ? object->get_core() : nullptr;
        if (core && core->get_environment()) {
            IVP_Controller_Manager::remove_controller_from_environment(
                this, IVP_TRUE);
        }
        anchors[1].~IVP_Anchor();
        anchors[0].~IVP_Anchor();
    }

    void *client_data;

    IVP_Anchor *get_actuator_anchor(int index) { return &anchors[index]; }
    const IVP_Anchor *get_actuator_anchor(int index) const {
        return &anchors[index];
    }
    void ensure_actuator_in_simulation() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActuatorEnsureSimulation, this);
    }
    IVP_DOUBLE calc_len() {
        IVP_U_Point positions[2];
        for (int index = 0; index < 2; ++index) {
            IVP_Core *core = anchors[index].l_anchor_object->get_core();
            core->get_m_world_f_core_PSI()->vmult4(
                &anchors[index].core_pos, &positions[index]);
        }
        return std::sqrt(positions[0].quad_distance_to(&positions[1]));
    }
};

class IVP_Actuator_Four_Point : public IVP_Actuator {
protected:
    IVP_Anchor anchors[4];

public:
    void *client_data;

    IVP_Actuator_Four_Point(
        IVP_Environment *environment,
        IVP_Template_Four_Point *definition,
        IVP_ACTUATOR_TYPE)
        : IVP_Actuator(environment), client_data(definition->client_data) {
        IVP_Core *firstCore = nullptr;
        for (int index = 0; index < 4; ++index) {
            anchors[index].init_anchor(this, definition->anchors[index]);
            IVP_Core *core = anchors[index].l_anchor_object->get_core();
            if (!firstCore)
                firstCore = core;
            // The nearby source accidentally tests physical_unmoveable
            // without negation. Controllers enumerate movable cores, as the
            // verified Two Point constructor does.
            if ((core->flags & 0x0Cu) == 0u)
                actuator_controlled_cores.install(core);
        }
        // The nearby Four Point body omitted registration even though its
        // destructor always removes the controller.
        if (firstCore && firstCore->get_environment()) {
            firstCore->get_environment()->get_controller_manager()
                ->announce_controller_to_environment(this);
        }
    }

    ~IVP_Actuator_Four_Point() override {
        IVP_Real_Object *object = anchors[0].l_anchor_object;
        IVP_Core *core = object ? object->get_core() : nullptr;
        if (core && core->get_environment()) {
            IVP_Controller_Manager::remove_controller_from_environment(
                this, IVP_TRUE);
        }
    }

    IVP_Anchor *get_actuator_anchor(int index) { return &anchors[index]; }
    const IVP_Anchor *get_actuator_anchor(int index) const {
        return &anchors[index];
    }
};

class IVP_Listener_Spring {
public:
    virtual void event_spring_broken(IVP_Actuator_Spring *) = 0;
};

// Callback-only interfaces retained by the nearby public headers. Do not add
// virtual destructors: their ABI consists only of the slots declared here.
class IVP_Listener_Check_Dist_Event {
public:
    virtual void check_dist_event(
        IVP_Actuator_Check_Dist *, IVP_BOOL distanceShorterThanRange) = 0;
    virtual void check_dist_is_going_to_be_deleted_event(
        IVP_Actuator_Check_Dist *) = 0;
};

class IVP_Listener_Stiff_Spring {
public:
    virtual void event_stiff_spring_broken(
        IVP_Controller_Stiff_Spring *) = 0;
};

class IVP_Anchor_Check_Dist : public IVP_Listener_Hull {
public:
    IVP_Real_Object *real_object;
    IVP_Actuator_Check_Dist *l_actuator_check_dist;
    IVP_U_Float_Point object_pos;

    IVP_Anchor_Check_Dist() = default;
    virtual ~IVP_Anchor_Check_Dist() = default;

    IVP_HULL_ELEM_TYPE get_type() override {
        return IVP_HULL_ELEM_ANCHOR;
    }
    void hull_limit_exceeded_event(
        IVP_Hull_Manager *, IVP_HTIME) override;
    void hull_manager_is_going_to_be_deleted_event(
        IVP_Hull_Manager *) override;
    void init_anchor_check_dist(
        IVP_Real_Object *object, IVP_U_Point *worldPosition,
        IVP_Actuator_Check_Dist *actuator);
};

class IVP_Actuator_Check_Dist {
    friend class IVP_Anchor_Check_Dist;
    friend class IVP_Environment;

private:
    IVP_Anchor_Check_Dist anchors[2];
    IVP_FLOAT range;
    IVP_U_Vector<IVP_Listener_Check_Dist_Event>
        listeners_check_dist_event;
    IVP_U_Active_Terminal_Int *mod_is_outside;

    void fire_check_dist_event(IVP_BOOL outside) {
        for (int index = listeners_check_dist_event.len() - 1;
             index >= 0; --index) {
            listeners_check_dist_event.element_at(index)
                ->check_dist_event(this, outside);
        }
    }
    void fire_check_dist_is_going_to_be_deleted_event() {
        for (int index = listeners_check_dist_event.len() - 1;
             index >= 0; --index) {
            listeners_check_dist_event.element_at(index)
                ->check_dist_is_going_to_be_deleted_event(this);
        }
    }
    void hull_limit_exceeded_event() {
        IVP_U_Point positions[2];
        for (int index = 0; index < 2; ++index) {
            IVP_U_Matrix worldFromObject;
            anchors[index].real_object->get_m_world_f_object_AT(
                &worldFromObject);
            worldFromObject.vmult4(
                &anchors[index].object_pos, &positions[index]);
        }

        const IVP_DOUBLE length = std::sqrt(
            positions[0].quad_distance_to(&positions[1]));
        const IVP_BOOL outside =
            length < range ? IVP_FALSE : IVP_TRUE;
        if (outside != is_outside) {
            is_outside = outside;
            // The nearby implementation passes the new outside state.  Its
            // public parameter name (distance_shorter_than_range) is stale.
            fire_check_dist_event(outside);
        }

        if (mod_is_outside)
            mod_is_outside->set_int(is_outside, IVP_TRUE);

        const IVP_DOUBLE validHullTime =
            std::fabs(length - range) * 0.5;
        const IVP_Time now =
            anchors[0].real_object->get_environment()->get_current_time();
        for (IVP_Anchor_Check_Dist &anchor : anchors) {
            anchor.real_object->get_hull_manager()->update_lazy_synapse(
                &anchor, now, validHullTime);
        }
    }

protected:
    IVP_Actuator_Check_Dist(
        IVP_Environment *, IVP_Template_Check_Dist *definition)
        : range(definition->range),
          mod_is_outside(definition->mod_is_outside),
          client_data(definition->client_data), is_outside(IVP_FALSE) {
        for (int index = 0; index < 2; ++index) {
            anchors[index].init_anchor_check_dist(
                definition->objects[index],
                &definition->position_world_space[index], this);
            anchors[index].real_object->get_hull_manager()
                ->insert_lazy_synapse(&anchors[index], IVP_Time(0.0), 0.0);
        }
        hull_limit_exceeded_event();
    }

public:
    void *client_data;
    IVP_BOOL is_outside;

    void set_range(IVP_DOUBLE value) {
        range = static_cast<IVP_FLOAT>(value);
        hull_limit_exceeded_event();
    }
    void add_listener_check_dist_event(
        IVP_Listener_Check_Dist_Event *listener) {
        listeners_check_dist_event.install(listener);
    }
    void remove_listener_check_dist_event(
        IVP_Listener_Check_Dist_Event *listener) {
        listeners_check_dist_event.remove(listener);
    }

    ~IVP_Actuator_Check_Dist() {
        fire_check_dist_is_going_to_be_deleted_event();
        for (IVP_Anchor_Check_Dist &anchor : anchors) {
            anchor.real_object->get_hull_manager()->remove_synapse(&anchor);
        }
    }

    BML_IVP_RETAIL_ALLOCATED_OBJECT;
};

inline void IVP_Anchor_Check_Dist::init_anchor_check_dist(
    IVP_Real_Object *object, IVP_U_Point *worldPosition,
    IVP_Actuator_Check_Dist *actuator) {
    l_actuator_check_dist = actuator;
    real_object = object;
    IVP_U_Matrix worldFromObject;
    object->get_m_world_f_object_AT(&worldFromObject);
    IVP_U_Point objectPosition;
    worldFromObject.vimult4(worldPosition, &objectPosition);
    object_pos.set(&objectPosition);
}

inline void IVP_Anchor_Check_Dist::hull_limit_exceeded_event(
    IVP_Hull_Manager *, IVP_HTIME) {
    l_actuator_check_dist->hull_limit_exceeded_event();
}

inline void IVP_Anchor_Check_Dist::
hull_manager_is_going_to_be_deleted_event(IVP_Hull_Manager *) {
    delete l_actuator_check_dist;
}

class IVP_Actuator_Stabilizer : public IVP_Actuator_Four_Point {
    friend class IVP_Environment;

private:
    IVP_Environment *l_environment;

protected:
    IVP_FLOAT stabi_constant;

    IVP_Actuator_Stabilizer(
        IVP_Environment *environment,
        IVP_Template_Stabilizer *definition)
        : IVP_Actuator_Four_Point(
              environment, definition, IVP_ACTUATOR_TYPE_STABILIZER),
          l_environment(environment),
          stabi_constant(definition->stabi_constant) {}

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        IVP_Core *cores[4];
        for (int index = 0; index < 4; ++index)
            cores[index] = anchors[index].l_anchor_object->friction_core;

        IVP_U_Float_Point directions[2];
        IVP_U_Point firstPositions[2];
        IVP_U_Point secondPositions[2];
        IVP_DOUBLE distances[2];
        for (int pair = 0; pair < 2; ++pair) {
            IVP_Anchor *first = &anchors[pair * 2];
            IVP_Anchor *second = &anchors[pair * 2 + 1];
            IVP_Core *firstCore = first->l_anchor_object->friction_core;
            IVP_Core *secondCore = second->l_anchor_object->friction_core;
            if (!firstCore || !secondCore)
                return;

            firstCore->get_m_world_f_core_PSI()->vmult4(
                &first->core_pos, &firstPositions[pair]);
            secondCore->get_m_world_f_core_PSI()->vmult4(
                &second->core_pos, &secondPositions[pair]);
            directions[pair].subtract(
                &firstPositions[pair], &secondPositions[pair]);
            distances[pair] =
                directions[pair].real_length_plus_normize();
        }

        IVP_DOUBLE force =
            (distances[1] - distances[0]) * stabi_constant *
            event->delta_time;
        for (int pair = 0; pair < 2; ++pair) {
            IVP_U_Float_Point impulse;
            impulse.set_multiple(&directions[pair], force);

            IVP_Core *firstCore = cores[pair * 2];
            IVP_Core *secondCore = cores[pair * 2 + 1];
            if (secondCore->movement_state < IVP_MT_NOT_SIM &&
                ((secondCore->flags >> 8u) & 0x3u) == 0u) {
                secondCore->async_push_core_ws(
                    &secondPositions[pair], &impulse);
            }
            if (firstCore->movement_state < IVP_MT_NOT_SIM &&
                ((firstCore->flags >> 8u) & 0x3u) == 0u) {
                impulse.mult(-1.0);
                firstCore->async_push_core_ws(
                    &firstPositions[pair], &impulse);
            }
            force = -force;
        }
    }

public:
    void set_stabi_constant(IVP_DOUBLE value) {
        if (value == stabi_constant)
            return;
        stabi_constant = static_cast<IVP_FLOAT>(value);
        anchors[0].l_anchor_object->ensure_in_simulation();
    }

    ~IVP_Actuator_Stabilizer() override = default;
};

class IVP_Actuator_Force : public IVP_Actuator_Two_Point {
    friend class IVP_Environment;

private:
    IVP_FLOAT force;
    IVP_BOOL push_first_object : 1;
    IVP_BOOL push_second_object : 1;

protected:
    IVP_Actuator_Force(
        IVP_Environment *environment, IVP_Template_Force *definition)
        : IVP_Actuator_Two_Point(
              environment, definition, IVP_ACTUATOR_TYPE_FORCE),
          force(definition->force),
          push_first_object(definition->push_first_object),
          push_second_object(definition->push_second_object) {}

public:
    ~IVP_Actuator_Force() override = default;

    IVP_DOUBLE get_force() const { return force; }
    void set_force(IVP_DOUBLE value) {
        if (value == force)
            return;
        force = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        const IVP_DOUBLE forceValue = force;
        if (forceValue == 0.0)
            return;

        IVP_Real_Object *firstObject = anchors[0].l_anchor_object;
        IVP_Real_Object *secondObject = anchors[1].l_anchor_object;
        IVP_Core *firstCore = firstObject->get_core();
        IVP_Core *secondCore = secondObject->get_core();

        IVP_U_Point firstPosition;
        IVP_U_Point secondPosition;
        firstCore->get_m_world_f_core_PSI()->vmult4(
            &anchors[0].core_pos, &firstPosition);
        secondCore->get_m_world_f_core_PSI()->vmult4(
            &anchors[1].core_pos, &secondPosition);

        IVP_U_Point direction;
        direction.subtract(&firstPosition, &secondPosition);
        direction.fast_normize();

        IVP_U_Float_Point impulse;
        impulse.set(&direction);
        impulse.mult(forceValue * event->delta_time);

        if (push_first_object != IVP_FALSE &&
            firstCore->movement_state < IVP_MT_NOT_SIM &&
            ((firstCore->flags >> 8u) & 0x3u) == 0u) {
            firstCore->async_push_core_ws(&firstPosition, &impulse);
        }

        if (push_second_object != IVP_FALSE &&
            secondCore->movement_state < IVP_MT_NOT_SIM &&
            ((secondCore->flags >> 8u) & 0x3u) == 0u) {
            IVP_U_Float_Point reverseImpulse;
            reverseImpulse.set_multiple(&impulse, -1.0);
            secondCore->async_push_core_ws(&secondPosition, &reverseImpulse);
        }
    }
};

class IVP_Actuator_Force_Active
    : public IVP_Actuator_Force,
      public IVP_U_Active_Float_Listener {
    friend class IVP_Environment;

private:
    IVP_U_Active_Float *active_float_force;

protected:
    IVP_Actuator_Force_Active(
        IVP_Environment *environment, IVP_Template_Force *definition)
        : IVP_Actuator_Force(environment, definition),
          active_float_force(definition->active_float_force) {
        if (active_float_force) {
            active_float_force->add_dependency(this);
            active_float_changed(active_float_force);
        }
    }

    void active_float_changed(IVP_U_Active_Float *value) override {
        if (value == active_float_force)
            set_force(value->get_float_value());
    }

public:
    ~IVP_Actuator_Force_Active() override {
        if (active_float_force)
            active_float_force->remove_dependency(this);
    }
};

class IVP_Actuator_Torque : public IVP_Actuator_Two_Point {
    friend class IVP_Environment;

private:
    IVP_FLOAT max_rotation_speed;
    IVP_FLOAT torque;
    IVP_U_Float_Point axis_in_core_coord_system;
    IVP_DOUBLE rot_inertia;
    IVP_U_Active_Terminal_Double *active_float_rotation_speed_out;

protected:
    IVP_Actuator_Torque(
        IVP_Environment *environment, IVP_Template_Torque *definition)
        : IVP_Actuator_Two_Point(
              environment, definition, IVP_ACTUATOR_TYPE_TORQUE),
          max_rotation_speed(definition->max_rotation_speed),
          torque(definition->torque), rot_inertia(0.0),
          active_float_rotation_speed_out(
              definition->active_float_rotation_speed_out),
          rot_speed_out(0.0f) {
        if (active_float_rotation_speed_out)
            active_float_rotation_speed_out->add_reference();

        axis_in_core_coord_system.subtract(
            &anchors[1].core_pos, &anchors[0].core_pos);
        axis_in_core_coord_system.normize();

        if (torque != 0.0f)
            ensure_actuator_in_simulation();
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        rot_speed_out = 0.0f;
        if (torque == 0.0f)
            return;

        IVP_Real_Object *object = anchors[0].l_anchor_object;
        IVP_Core *core = object->get_core();
        if (core->movement_state >= IVP_MT_NOT_SIM ||
            ((core->flags >> 8u) & 0x3u) != 0u) {
            return;
        }

        IVP_U_Float_Point mergedAxis;
        IVP_U_Float_Point *axis = &axis_in_core_coord_system;
        if (core != object->get_original_core()) {
            mergedAxis.subtract(&anchors[1].core_pos, &anchors[0].core_pos);
            mergedAxis.normize();
            axis = &mergedAxis;
        }

        const IVP_DOUBLE rotationSpeed = core->get_rot_speed_cs(axis);
        rot_speed_out = static_cast<IVP_FLOAT>(rotationSpeed);
        if (active_float_rotation_speed_out)
            active_float_rotation_speed_out->set_double(rot_speed_out);

        if (std::fabs(rotationSpeed) > max_rotation_speed)
            return;
        core->async_rot_push_core_multiple_cs(
            axis, torque * event->delta_time);
    }

public:
    IVP_FLOAT rot_speed_out;

    void set_max_rotation_speed(IVP_DOUBLE value) {
        if (value == max_rotation_speed)
            return;
        max_rotation_speed = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    void set_torque(IVP_DOUBLE value) {
        if (value == torque)
            return;
        torque = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    IVP_FLOAT get_torque() { return torque; }

    ~IVP_Actuator_Torque() override {
        if (active_float_rotation_speed_out)
            active_float_rotation_speed_out->remove_reference();
    }
};

class IVP_Actuator_Torque_Active
    : public IVP_Actuator_Torque,
      public IVP_U_Active_Float_Listener {
    friend class IVP_Environment;

private:
    IVP_U_Active_Float *active_float_max_rotation_speed;
    IVP_U_Active_Float *active_float_torque;

    void active_float_changed(IVP_U_Active_Float *value) override {
        if (value == active_float_max_rotation_speed)
            set_max_rotation_speed(value->get_float_value());
        if (value == active_float_torque)
            set_torque(value->get_float_value());
    }

public:
    IVP_Actuator_Torque_Active(
        IVP_Environment *environment, IVP_Template_Torque *definition)
        : IVP_Actuator_Torque(environment, definition),
          active_float_max_rotation_speed(
              definition->active_float_max_rotation_speed),
          active_float_torque(definition->active_float_torque) {
        if (active_float_max_rotation_speed) {
            active_float_max_rotation_speed->add_dependency(this);
            set_max_rotation_speed(
                active_float_max_rotation_speed->get_float_value());
        }
        if (active_float_torque) {
            active_float_torque->add_dependency(this);
            set_torque(active_float_torque->get_float_value());
        }
    }

    ~IVP_Actuator_Torque_Active() override {
        if (active_float_max_rotation_speed)
            active_float_max_rotation_speed->remove_dependency(this);
        if (active_float_torque)
            active_float_torque->remove_dependency(this);
    }
};

class IVP_Actuator_Rot_Mot : public IVP_Actuator_Two_Point {
    friend class IVP_Environment;

private:
    IVP_FLOAT max_rotation_speed;
    IVP_FLOAT power;
    IVP_FLOAT max_torque;
    IVP_U_Float_Point axis_in_core_coord_system;
    IVP_DOUBLE rot_inertia;
    IVP_U_Active_Terminal_Double *active_float_rotation_speed_out;

protected:
    IVP_Actuator_Rot_Mot(
        IVP_Environment *environment, IVP_Template_Rot_Mot *definition)
        : IVP_Actuator_Two_Point(
              environment, definition, IVP_ACTUATOR_TYPE_ROT_MOT),
          max_rotation_speed(definition->max_rotation_speed),
          power(definition->power), max_torque(definition->max_torque),
          rot_inertia(0.0),
          active_float_rotation_speed_out(
              definition->active_float_rotation_speed_out),
          rot_speed_out(0.0f) {
        if (active_float_rotation_speed_out)
            active_float_rotation_speed_out->add_reference();

        axis_in_core_coord_system.subtract(
            &anchors[1].core_pos, &anchors[0].core_pos);
        axis_in_core_coord_system.normize();
    }

public:
    IVP_FLOAT rot_speed_out;

    void set_max_rotation_speed(IVP_DOUBLE value) {
        if (value == max_rotation_speed)
            return;
        max_rotation_speed = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    void set_power(IVP_DOUBLE value) {
        if (value == power)
            return;
        power = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    IVP_FLOAT get_power() { return power; }
    void set_max_torque(IVP_DOUBLE value) {
        if (value == max_torque)
            return;
        max_torque = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        if (power == 0.0f)
            return;

        IVP_Real_Object *object = anchors[0].l_anchor_object;
        IVP_Core *core = object->get_core();
        IVP_Core *originalCore = object->get_original_core();

        IVP_U_Float_Point mergedAxis;
        IVP_U_Float_Point *axis = &axis_in_core_coord_system;
        if (core != originalCore) {
            const IVP_U_Matrix *firstMatrix =
                anchors[0].l_anchor_object->get_core()
                    ->get_m_world_f_core_PSI();
            const IVP_U_Matrix *secondMatrix =
                anchors[1].l_anchor_object->get_core()
                    ->get_m_world_f_core_PSI();
            IVP_U_Point secondPositionWorld;
            secondMatrix->vmult4(
                &anchors[1].core_pos, &secondPositionWorld);
            IVP_U_Float_Point secondPositionFirstCore;
            firstMatrix->vimult4(
                &secondPositionWorld, &secondPositionFirstCore);
            mergedAxis.subtract(
                &secondPositionFirstCore, &anchors[0].core_pos);
            mergedAxis.normize();
            axis = &mergedAxis;
        }

        IVP_DOUBLE rotationSpeed = core->get_rot_speed_cs(axis);
        if (power * rotationSpeed < 0.0)
            rotationSpeed = 0.0;
        rot_speed_out = static_cast<IVP_FLOAT>(rotationSpeed);
        if (active_float_rotation_speed_out)
            active_float_rotation_speed_out->set_double(rot_speed_out);

        if (rotationSpeed < 0.0)
            rotationSpeed = -rotationSpeed;
        if (rotationSpeed > max_rotation_speed)
            return;
        if (rotationSpeed < 0.1)
            rotationSpeed = 0.1;

        const IVP_DOUBLE rotationInertia =
            core->get_rot_inertia_cs(axis);
        IVP_DOUBLE rotationForce =
            power / (rotationSpeed * rotationInertia);
        if (max_torque != 0.0f) {
            if (rotationForce > max_torque)
                rotationForce = max_torque;
            else if (rotationForce < -max_torque)
                rotationForce = -max_torque;
        }
        core->async_rot_push_core_multiple_cs(
            axis, rotationForce * event->delta_time);
    }

    ~IVP_Actuator_Rot_Mot() override {
        if (active_float_rotation_speed_out)
            active_float_rotation_speed_out->remove_reference();
    }
};

class IVP_Actuator_Rot_Mot_Active
    : public IVP_Actuator_Rot_Mot,
      public IVP_U_Active_Float_Listener {
    friend class IVP_Environment;

private:
    IVP_U_Active_Float *active_float_max_rotation_speed;
    IVP_U_Active_Float *active_float_power;
    IVP_U_Active_Float *active_float_max_torque;

    void active_float_changed(IVP_U_Active_Float *value) override {
        if (value == active_float_max_rotation_speed)
            set_max_rotation_speed(value->get_float_value());
        if (value == active_float_power)
            set_power(value->get_float_value());
        if (value == active_float_max_torque)
            set_max_torque(value->get_float_value());
    }

public:
    IVP_Actuator_Rot_Mot_Active(
        IVP_Environment *environment, IVP_Template_Rot_Mot *definition)
        : IVP_Actuator_Rot_Mot(environment, definition),
          active_float_max_rotation_speed(
              definition->active_float_max_rotation_speed),
          active_float_power(definition->active_float_power),
          active_float_max_torque(definition->active_float_max_torque) {
        if (active_float_max_rotation_speed) {
            active_float_max_rotation_speed->add_dependency(this);
            set_max_rotation_speed(
                active_float_max_rotation_speed->get_float_value());
        }
        if (active_float_power) {
            active_float_power->add_dependency(this);
            set_power(active_float_power->get_float_value());
        }
        if (active_float_max_torque) {
            active_float_max_torque->add_dependency(this);
            set_max_torque(active_float_max_torque->get_float_value());
        }
    }

    ~IVP_Actuator_Rot_Mot_Active() override {
        if (active_float_max_rotation_speed)
            active_float_max_rotation_speed->remove_dependency(this);
        if (active_float_power)
            active_float_power->remove_dependency(this);
        if (active_float_max_torque)
            active_float_max_torque->remove_dependency(this);
    }
};

class IVP_Controller_Stiff_Spring : public IVP_Actuator_Two_Point {
private:
    IVP_Environment *l_environment;

protected:
    IVP_FLOAT spring_len;
    IVP_FLOAT spring_constant;
    IVP_FLOAT spring_damp;
    IVP_FLOAT break_max_len;
    IVP_SPRING_FORCE_EXCEED max_len_exceed_type;
    IVP_U_Vector<IVP_Listener_Stiff_Spring> listeners_spring_event;

    void fire_event_spring_broken() {
        for (int index = listeners_spring_event.len() - 1;
             index >= 0; --index) {
            listeners_spring_event.element_at(index)
                ->event_stiff_spring_broken(this);
        }
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        IVP_U_Point firstPosition;
        IVP_U_Point secondPosition;
        IVP_Anchor *firstAnchor = get_actuator_anchor(0);
        IVP_Anchor *secondAnchor = get_actuator_anchor(1);
        IVP_Core *firstCore = firstAnchor->l_anchor_object->get_core();
        IVP_Core *secondCore = secondAnchor->l_anchor_object->get_core();

        firstCore->get_m_world_f_core_PSI()->vmult4(
            &firstAnchor->core_pos, &firstPosition);
        secondCore->get_m_world_f_core_PSI()->vmult4(
            &secondAnchor->core_pos, &secondPosition);

        IVP_U_Point direction;
        direction.subtract(&firstPosition, &secondPosition);
        const IVP_DOUBLE currentLength =
            direction.real_length_plus_normize();
        if (currentLength < 1.0e-10)
            return;

        if (currentLength > break_max_len &&
            max_len_exceed_type == IVP_SFE_BREAK) {
            fire_event_spring_broken();
            delete this;
            return;
        }

        if (firstCore->movement_state >= IVP_MT_NOT_SIM)
            firstCore = nullptr;
        if (secondCore->movement_state >= IVP_MT_NOT_SIM)
            secondCore = nullptr;
        if (!firstCore && !secondCore)
            return;

        IVP_U_Float_Point directionFloat(&direction);
        IVP_DOUBLE inverseEffectiveMass = 0.0;
        IVP_DOUBLE relativeSpeed = 0.0;

        const auto accumulateResponse = [&] (
            IVP_Core *core, IVP_DOUBLE sign) {
            if (!core)
                return;

            IVP_U_Float_Point lever;
            lever.subtract(&firstPosition, core->get_position_PSI());
            IVP_U_Float_Point angularAxisWorld;
            angularAxisWorld.inline_calc_cross_product(
                &lever, &directionFloat);
            IVP_U_Float_Point angularAxisCore;
            core->get_m_world_f_core_PSI()->vimult3(
                &angularAxisWorld, &angularAxisCore);
            IVP_U_Float_Point angularResponse;
            angularResponse.set_pairwise_mult(
                &angularAxisCore, core->get_inv_rot_inertia());

            inverseEffectiveMass += core->get_inv_mass() +
                angularAxisCore.dot_product(&angularResponse);
            relativeSpeed += sign * (
                core->get_speed()->dot_product(&directionFloat) +
                core->get_rot_speed()->dot_product(&angularAxisCore));
        };

        accumulateResponse(firstCore, 1.0);
        accumulateResponse(secondCore, -1.0);
        if (inverseEffectiveMass <= 1.0e-20)
            return;

        const IVP_DOUBLE damping =
            spring_constant + spring_damp * (1.0 - spring_constant);
        const IVP_DOUBLE impulseMagnitude =
            ((spring_len - currentLength) * event->i_delta_time *
                 spring_constant -
             relativeSpeed * damping) /
            inverseEffectiveMass;
        IVP_U_Float_Point impulse;
        impulse.set_multiple(&directionFloat, impulseMagnitude);

        if (firstCore)
            firstCore->push_core_ws(&firstPosition, &impulse);
        if (secondCore) {
            IVP_U_Float_Point reverseImpulse;
            reverseImpulse.set_multiple(&impulse, -1.0);
            secondCore->push_core_ws(&firstPosition, &reverseImpulse);
        }
    }

    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_STIFF_SPRINGS;
    }

public:
    IVP_Controller_Stiff_Spring(
        IVP_Environment *environment,
        IVP_Template_Stiff_Spring *definition)
        : IVP_Actuator_Two_Point(
              environment, definition, IVP_ACTUATOR_TYPE_STIFF_SPRING),
          l_environment(environment), spring_len(definition->spring_len),
          spring_constant(definition->spring_constant),
          spring_damp(definition->spring_damp),
          break_max_len(definition->break_max_len),
          max_len_exceed_type(definition->max_len_exceed_type) {}

    ~IVP_Controller_Stiff_Spring() override = default;

    void set_constant(IVP_DOUBLE value) {
        spring_constant = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    IVP_FLOAT get_constant() { return spring_constant; }

    void set_damp(IVP_DOUBLE value) {
        spring_damp = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    IVP_FLOAT get_damp_factor() { return spring_damp; }

    void set_len(IVP_DOUBLE value) {
        if (value == spring_len)
            return;
        spring_len = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    void set_break_max_len(IVP_DOUBLE value) {
        if (value == break_max_len)
            return;
        break_max_len = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    IVP_FLOAT get_spring_length_zero_force() { return spring_len; }

    void add_listener_stiff_spring(
        IVP_Listener_Stiff_Spring *listener) {
        listeners_spring_event.install(listener);
    }
    void remove_listener_stiff_spring(
        IVP_Listener_Stiff_Spring *listener) {
        listeners_spring_event.remove(listener);
    }
};

class IVP_Controller_Stiff_Spring_Active
    : public IVP_Controller_Stiff_Spring,
      public IVP_U_Active_Float_Listener {
protected:
    IVP_U_Active_Float *active_float_spring_len;
    IVP_U_Active_Float *active_float_spring_constant;
    IVP_U_Active_Float *active_float_spring_damp;

    void active_float_changed(IVP_U_Active_Float *value) override {
        if (value == active_float_spring_len)
            set_len(value->get_float_value());
        if (value == active_float_spring_constant)
            set_constant(value->get_float_value());
        if (value == active_float_spring_damp)
            set_damp(value->get_float_value());
    }

public:
    IVP_Controller_Stiff_Spring_Active(
        IVP_Environment *environment,
        IVP_Template_Stiff_Spring_Active *definition)
        : IVP_Controller_Stiff_Spring(environment, definition),
          active_float_spring_len(definition->active_float_spring_len),
          active_float_spring_constant(
              definition->active_float_spring_constant),
          active_float_spring_damp(definition->active_float_spring_damp) {
        if (active_float_spring_len) {
            active_float_spring_len->add_dependency(this);
            spring_len = active_float_spring_len->get_float_value();
        }
        if (active_float_spring_constant) {
            active_float_spring_constant->add_dependency(this);
            spring_constant =
                active_float_spring_constant->get_float_value();
        }
        if (active_float_spring_damp) {
            active_float_spring_damp->add_dependency(this);
            spring_damp = active_float_spring_damp->get_float_value();
        }
    }

    ~IVP_Controller_Stiff_Spring_Active() override {
        if (active_float_spring_len)
            active_float_spring_len->remove_dependency(this);
        if (active_float_spring_constant)
            active_float_spring_constant->remove_dependency(this);
        if (active_float_spring_damp)
            active_float_spring_damp->remove_dependency(this);
    }
};

class IVP_Actuator_Spring : public IVP_Actuator_Two_Point {
    friend struct BML_IvpActuatorSpringLayoutCheck;

protected:
    struct Retail_Construction_Tag {};

    // Storage-only path used by the retained Spring Active constructor.
    explicit IVP_Actuator_Spring(Retail_Construction_Tag) noexcept
        : IVP_Actuator_Two_Point(
              IVP_Actuator_Two_Point::Retail_Construction_Tag{}) {}

    IVP_Environment *l_environment;
    IVP_FLOAT spring_len;
    IVP_FLOAT spring_values_factor;
    IVP_FLOAT spring_constant;
    IVP_FLOAT spring_damp;
    IVP_FLOAT rel_pos_damp;
    IVP_FLOAT break_max_len;
    IVP_SPRING_FORCE_EXCEED max_len_exceed_type;
    // The retained complete constructor at RVA 0x14250 owns this vector's
    // construction. Keep only inactive storage until that body runs.
    union {
        IVP_U_Vector<IVP_Listener_Spring> listeners_spring;
    };

    void initialize_reconstructed(
        IVP_Environment *environment,
        IVP_Template_Spring *definition,
        IVP_ACTUATOR_TYPE actuatorType) {
        IVP_Actuator_Two_Point::initialize_reconstructed(
            environment, definition);
        ::new (static_cast<void *>(&listeners_spring))
            IVP_U_Vector<IVP_Listener_Spring>();

        l_environment = environment;
        spring_len = definition->spring_len;
        spring_values_factor = 1.0f;
        break_max_len = definition->break_max_len;
        max_len_exceed_type = definition->max_len_exceed_type;

        if (definition->spring_values_are_relative != IVP_FALSE) {
            IVP_Core *firstCore = anchors[0].l_anchor_object->get_core();
            IVP_Core *secondCore = anchors[1].l_anchor_object->get_core();
            const IVP_DOUBLE firstMass =
                firstCore->calc_virt_mass(&anchors[0].core_pos, nullptr);
            const IVP_DOUBLE secondMass =
                secondCore->calc_virt_mass(&anchors[1].core_pos, nullptr);
            spring_values_factor = static_cast<IVP_FLOAT>(
                (firstMass * secondMass) / (firstMass + secondMass));
        }
        spring_constant =
            definition->spring_constant * spring_values_factor;
        spring_damp = definition->spring_damp * spring_values_factor;
        rel_pos_damp = definition->rel_pos_damp * spring_values_factor;
        (void)actuatorType;
    }

    void fire_event_spring_broken() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActuatorSpringFireBroken, this,
            [this] {
                for (int index = listeners_spring.len() - 1;
                     index >= 0; --index) {
                    listeners_spring.element_at(index)
                        ->event_spring_broken(this);
                }
            });
    }

    IVP_Actuator_Spring(
        IVP_Environment *environment,
        IVP_Template_Spring *definition,
        IVP_ACTUATOR_TYPE actuatorType)
        : IVP_Actuator_Two_Point(
              IVP_Actuator_Two_Point::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActuatorSpringConstruct, this,
            [this, environment, definition, actuatorType] {
                initialize_reconstructed(
                    environment, definition, actuatorType);
            },
            environment, definition, actuatorType);
    }

public:
    ~IVP_Actuator_Spring() override {
        listeners_spring.~IVP_U_Vector<IVP_Listener_Spring>();
    }

    void set_constant(IVP_DOUBLE value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActuatorSpringSetConstant, this, value);
    }
    IVP_FLOAT get_constant() { return spring_constant; }
    IVP_FLOAT get_constant() const { return spring_constant; }
    void set_damp(IVP_DOUBLE value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActuatorSpringSetDamping, this, value);
    }
    IVP_FLOAT get_damp_factor() { return spring_damp; }
    IVP_FLOAT get_damp_factor() const { return spring_damp; }
    void set_rel_pos_damp(IVP_DOUBLE value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActuatorSpringSetRelativePositionDamping,
            this, value);
    }
    IVP_FLOAT get_rel_pos_damp() { return rel_pos_damp; }
    IVP_FLOAT get_rel_pos_damp() const { return rel_pos_damp; }
    void set_len(IVP_DOUBLE value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActuatorSpringSetLength, this, value);
    }
    void set_break_max_len(IVP_DOUBLE value) {
        if (break_max_len == value)
            return;
        break_max_len = static_cast<IVP_FLOAT>(value);
        ensure_actuator_in_simulation();
    }
    IVP_FLOAT get_spring_length_zero_force() { return spring_len; }
    IVP_FLOAT get_spring_length_zero_force() const { return spring_len; }
    IVP_BOOL get_only_stretch() {
        // Ballance predates this optional spring mode: neither the template,
        // the 0x98-byte runtime object nor the retail simulation body carries
        // the later revision's flag.
        return IVP_FALSE;
    }
    void add_listener_spring(IVP_Listener_Spring *listener) {
        listeners_spring.add(listener);
    }
    void remove_listener_spring(IVP_Listener_Spring *listener) {
        listeners_spring.remove(listener);
    }
    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *controlledCores) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ActuatorSpringDoSimulation,
            this, event, controlledCores);
    }
};

struct BML_IvpActuatorSpringLayoutCheck {
    static constexpr std::size_t environment =
        offsetof(IVP_Actuator_Spring, l_environment);
    static constexpr std::size_t length =
        offsetof(IVP_Actuator_Spring, spring_len);
    static constexpr std::size_t constant =
        offsetof(IVP_Actuator_Spring, spring_constant);
    static constexpr std::size_t damping =
        offsetof(IVP_Actuator_Spring, spring_damp);
    static constexpr std::size_t break_length =
        offsetof(IVP_Actuator_Spring, break_max_len);
    static constexpr std::size_t listeners =
        offsetof(IVP_Actuator_Spring, listeners_spring);
};

class IVP_Actuator_Spring_Active
    : public IVP_Actuator_Spring,
      public IVP_U_Active_Float_Listener {
    friend struct BML_IvpActuatorSpringActiveLayoutCheck;

protected:
    IVP_U_Active_Float *active_float_spring_len;
    IVP_U_Active_Float *active_float_spring_constant;
    IVP_U_Active_Float *active_float_spring_damp;
    IVP_U_Active_Float *active_float_spring_rel_pos_damp;

    void active_float_changed(IVP_U_Active_Float *value) override {
        auto *listenerSelf =
            static_cast<IVP_U_Active_Float_Listener *>(this);
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActuatorSpringActiveFloatChanged,
            listenerSelf,
            [this, value] {
                if (value == active_float_spring_len) {
                    set_len(value->get_float_value());
                    return;
                }
                if (value == active_float_spring_constant) {
                    set_constant(value->get_float_value());
                    return;
                }
                if (value == active_float_spring_damp) {
                    set_damp(value->get_float_value());
                    return;
                }
                if (value == active_float_spring_rel_pos_damp)
                    set_rel_pos_damp(value->get_float_value());
            },
            value);
    }

    friend class IVP_Environment;

    IVP_Actuator_Spring_Active(
        IVP_Environment *environment,
        IVP_Template_Spring *definition)
        : IVP_Actuator_Spring(
              IVP_Actuator_Spring::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ActuatorSpringActiveConstruct, this,
            [this, environment, definition] {
                initialize_reconstructed(
                    environment, definition, IVP_ACTUATOR_TYPE_SPRING);
                active_float_spring_len =
                    definition->active_float_spring_len;
                active_float_spring_constant =
                    definition->active_float_spring_constant;
                active_float_spring_damp =
                    definition->active_float_spring_damp;
                active_float_spring_rel_pos_damp =
                    definition->active_float_spring_rel_pos_damp;

                if (active_float_spring_len) {
                    active_float_spring_len->add_dependency(this);
                    spring_len = static_cast<IVP_FLOAT>(
                        active_float_spring_len->get_float_value());
                }
                if (active_float_spring_constant) {
                    active_float_spring_constant->add_dependency(this);
                    spring_constant = static_cast<IVP_FLOAT>(
                        active_float_spring_constant->get_float_value());
                }
                if (active_float_spring_damp) {
                    active_float_spring_damp->add_dependency(this);
                    spring_damp = static_cast<IVP_FLOAT>(
                        active_float_spring_damp->get_float_value());
                }
                if (active_float_spring_rel_pos_damp) {
                    active_float_spring_rel_pos_damp->add_dependency(this);
                    rel_pos_damp = static_cast<IVP_FLOAT>(
                        active_float_spring_rel_pos_damp->get_float_value());
                }
            },
            environment, definition);
    }

public:
    ~IVP_Actuator_Spring_Active() override {
        if (active_float_spring_len)
            active_float_spring_len->remove_dependency(this);
        if (active_float_spring_constant)
            active_float_spring_constant->remove_dependency(this);
        if (active_float_spring_damp)
            active_float_spring_damp->remove_dependency(this);
        if (active_float_spring_rel_pos_damp)
            active_float_spring_rel_pos_damp->remove_dependency(this);
    }
};

struct BML_IvpActuatorSpringActiveLayoutCheck {
    static constexpr std::size_t spring_length =
        offsetof(IVP_Actuator_Spring_Active, active_float_spring_len);
    static constexpr std::size_t spring_constant =
        offsetof(IVP_Actuator_Spring_Active, active_float_spring_constant);
    static constexpr std::size_t spring_damping =
        offsetof(IVP_Actuator_Spring_Active, active_float_spring_damp);
    static constexpr std::size_t relative_position_damping =
        offsetof(
            IVP_Actuator_Spring_Active,
            active_float_spring_rel_pos_damp);
};

class IVP_Actuator_Suspension : public IVP_Actuator_Spring {
protected:
    IVP_FLOAT spring_dampening_compression;
    IVP_FLOAT max_body_force;

public:
    IVP_Actuator_Suspension(
        IVP_Environment *environment,
        IVP_Template_Suspension *definition)
        : IVP_Actuator_Spring(
              environment, definition, IVP_ACTUATOR_TYPE_SUSPENSION),
          spring_dampening_compression(
              definition->spring_dampening_compression *
              spring_values_factor),
          max_body_force(definition->max_body_force) {}

    void set_spring_damp_compression(IVP_FLOAT value) {
        if (spring_dampening_compression == value)
            return;
        spring_dampening_compression = value * spring_values_factor;
        ensure_actuator_in_simulation();
    }

    void set_max_body_force(IVP_FLOAT value) {
        if (max_body_force == value)
            return;
        max_body_force = value;
        ensure_actuator_in_simulation();
    }

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *) override {
        IVP_Anchor *firstAnchor = get_actuator_anchor(0);
        IVP_Anchor *secondAnchor = get_actuator_anchor(1);
        IVP_Core *firstCore = firstAnchor->l_anchor_object->get_core();
        IVP_Core *secondCore = secondAnchor->l_anchor_object->get_core();

        IVP_U_Point firstPosition;
        IVP_U_Point secondPosition;
        firstCore->get_m_world_f_core_PSI()->vmult4(
            &firstAnchor->core_pos, &firstPosition);
        secondCore->get_m_world_f_core_PSI()->vmult4(
            &secondAnchor->core_pos, &secondPosition);

        IVP_U_Float_Point direction;
        direction.subtract(&firstPosition, &secondPosition);
        const IVP_FLOAT currentLength = static_cast<IVP_FLOAT>(
            direction.real_length_plus_normize());
        if (currentLength < 1.0e-10f)
            return;

        const IVP_DOUBLE deltaLength = currentLength - spring_len;
        IVP_DOUBLE force = deltaLength * spring_constant;

        IVP_U_Float_Point relativeWorldSpeed;
        IVP_Core::get_diff_surface_speed_of_two_cores(
            secondCore, firstCore,
            &secondAnchor->core_pos, &firstAnchor->core_pos,
            &relativeWorldSpeed);
        const IVP_DOUBLE dampingSpeed =
            relativeWorldSpeed.dot_product(&direction);
        const IVP_DOUBLE damping = dampingSpeed < 0.0
            ? spring_damp
            : spring_dampening_compression;
        force -= damping * dampingSpeed;

        IVP_DOUBLE clippedBodyForce = -force;
        if (clippedBodyForce < -max_body_force)
            clippedBodyForce = -max_body_force;
        else if (clippedBodyForce > max_body_force)
            clippedBodyForce = max_body_force;

        IVP_U_Float_Point impulse;
        impulse.set_multiple(&direction, force * event->delta_time);
        secondCore->async_push_core_ws(&secondPosition, &impulse);

        impulse.set_multiple(
            &direction, clippedBodyForce * event->delta_time);
        firstCore->async_push_core_ws(&firstPosition, &impulse);
    }

    ~IVP_Actuator_Suspension() override = default;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Template_Anchor) == 0x28);
static_assert(BML_IvpTemplateAnchorLayoutCheck::object == 0x00);
static_assert(BML_IvpTemplateAnchorLayoutCheck::position == 0x08);
static_assert(sizeof(IVP_Template_Two_Point) == 0x0C);
static_assert(offsetof(IVP_Template_Two_Point, client_data) == 0x00);
static_assert(offsetof(IVP_Template_Two_Point, anchors) == 0x04);
static_assert(sizeof(IVP_Template_Four_Point) == 0x14);
static_assert(offsetof(IVP_Template_Four_Point, client_data) == 0x00);
static_assert(offsetof(IVP_Template_Four_Point, anchors) == 0x04);
static_assert(sizeof(IVP_Template_Spring) == 0x38);
static_assert(sizeof(IVP_Extra_Info) == 0x40);
static_assert(offsetof(IVP_Extra_Info, range) == 0x10);
static_assert(offsetof(IVP_Extra_Info, mod_pf_sideward) == 0x38);
static_assert(sizeof(IVP_Template_Extra) == 0x50);
static_assert(offsetof(IVP_Template_Extra, info) == 0x10);
static_assert(sizeof(IVP_Template_Suspension) == 0x40);
static_assert(
    offsetof(IVP_Template_Suspension, spring_dampening_compression) == 0x38);
static_assert(offsetof(IVP_Template_Suspension, max_body_force) == 0x3C);
static_assert(sizeof(IVP_Template_Stiff_Spring) == 0x20);
static_assert(offsetof(IVP_Template_Stiff_Spring, spring_len) == 0x0C);
static_assert(offsetof(IVP_Template_Stiff_Spring, spring_constant) == 0x10);
static_assert(offsetof(IVP_Template_Stiff_Spring, spring_damp) == 0x14);
static_assert(
    offsetof(IVP_Template_Stiff_Spring, max_len_exceed_type) == 0x18);
static_assert(offsetof(IVP_Template_Stiff_Spring, break_max_len) == 0x1C);
static_assert(sizeof(IVP_Template_Stiff_Spring_Active) == 0x2C);
static_assert(
    offsetof(IVP_Template_Stiff_Spring_Active, active_float_spring_len) ==
    0x20);
static_assert(
    offsetof(IVP_Template_Stiff_Spring_Active,
             active_float_spring_constant) == 0x24);
static_assert(
    offsetof(IVP_Template_Stiff_Spring_Active, active_float_spring_damp) ==
    0x28);
static_assert(sizeof(IVP_Template_Check_Dist) == 0x58);
static_assert(offsetof(IVP_Template_Check_Dist, objects) == 0x04);
static_assert(
    offsetof(IVP_Template_Check_Dist, position_world_space) == 0x10);
static_assert(offsetof(IVP_Template_Check_Dist, range) == 0x50);
static_assert(offsetof(IVP_Template_Check_Dist, mod_is_outside) == 0x54);
static_assert(sizeof(IVP_Template_Stabilizer) == 0x1C);
static_assert(offsetof(IVP_Template_Stabilizer, stabi_constant) == 0x14);
static_assert(
    offsetof(IVP_Template_Stabilizer, active_float_stabi_constant) == 0x18);
static_assert(sizeof(IVP_Template_Force) == 0x1C);
static_assert(offsetof(IVP_Template_Force, force) == 0x0C);
static_assert(offsetof(IVP_Template_Force, active_float_force) == 0x10);
static_assert(offsetof(IVP_Template_Force, push_first_object) == 0x14);
static_assert(offsetof(IVP_Template_Force, push_second_object) == 0x18);
static_assert(sizeof(IVP_Template_Torque) == 0x20);
static_assert(offsetof(IVP_Template_Torque, torque) == 0x0C);
static_assert(offsetof(IVP_Template_Torque, max_rotation_speed) == 0x14);
static_assert(
    offsetof(IVP_Template_Torque, active_float_rotation_speed_out) == 0x1C);
static_assert(sizeof(IVP_Template_Rot_Mot) == 0x28);
static_assert(offsetof(IVP_Template_Rot_Mot, max_rotation_speed) == 0x0C);
static_assert(offsetof(IVP_Template_Rot_Mot, max_torque) == 0x14);
static_assert(
    offsetof(IVP_Template_Rot_Mot, active_float_rotation_speed_out) == 0x24);
static_assert(offsetof(IVP_Template_Spring, spring_constant) == 0x14);
static_assert(offsetof(IVP_Template_Spring, break_max_len) == 0x24);
static_assert(sizeof(IVP_Anchor) == 0x30);
static_assert(sizeof(IVP_Anchor_Check_Dist) == 0x20);
static_assert(offsetof(IVP_Anchor_Check_Dist, real_object) == 0x08);
static_assert(
    offsetof(IVP_Anchor_Check_Dist, l_actuator_check_dist) == 0x0C);
static_assert(offsetof(IVP_Anchor_Check_Dist, object_pos) == 0x10);
static_assert(sizeof(IVP_Listener_Spring) == 0x04);
static_assert(sizeof(IVP_Listener_Check_Dist_Event) == 0x04);
static_assert(sizeof(IVP_Listener_Stiff_Spring) == 0x04);
static_assert(offsetof(IVP_Anchor, object_pos) == 0x0C);
static_assert(offsetof(IVP_Anchor, core_pos) == 0x1C);
static_assert(offsetof(IVP_Anchor, l_actuator) == 0x2C);
static_assert(sizeof(IVP_Actuator) == 0x0C);
static_assert(offsetof(IVP_Actuator, actuator_controlled_cores) == 0x04);
static_assert(sizeof(IVP_Actuator_Two_Point) == 0x70);
static_assert(sizeof(IVP_Actuator_Four_Point) == 0xD0);
static_assert(offsetof(IVP_Actuator_Four_Point, client_data) == 0xCC);
static_assert(offsetof(IVP_Actuator_Two_Point, client_data) == 0x6C);
static_assert(sizeof(IVP_Actuator_Force) == 0x78);
static_assert(sizeof(IVP_Actuator_Force_Active) == 0x80);
static_assert(sizeof(IVP_Actuator_Torque) == 0x98);
static_assert(sizeof(IVP_Actuator_Torque_Active) == 0xA8);
static_assert(sizeof(IVP_Actuator_Rot_Mot) == 0xA0);
static_assert(sizeof(IVP_Actuator_Rot_Mot_Active) == 0xB0);
static_assert(sizeof(IVP_Controller_Stiff_Spring) == 0x90);
static_assert(sizeof(IVP_Controller_Stiff_Spring_Active) == 0xA0);
static_assert(sizeof(IVP_Actuator_Check_Dist) == 0x58);
static_assert(offsetof(IVP_Actuator_Check_Dist, client_data) == 0x50);
static_assert(offsetof(IVP_Actuator_Check_Dist, is_outside) == 0x54);
static_assert(sizeof(IVP_Actuator_Stabilizer) == 0xD8);
static_assert(sizeof(IVP_Actuator_Spring) == 0x98);
static_assert(BML_IvpActuatorSpringLayoutCheck::environment == 0x70);
static_assert(BML_IvpActuatorSpringLayoutCheck::length == 0x74);
static_assert(BML_IvpActuatorSpringLayoutCheck::constant == 0x7C);
static_assert(BML_IvpActuatorSpringLayoutCheck::damping == 0x80);
static_assert(BML_IvpActuatorSpringLayoutCheck::break_length == 0x88);
static_assert(BML_IvpActuatorSpringLayoutCheck::listeners == 0x90);
static_assert(sizeof(IVP_Actuator_Spring_Active) == 0xAC);
static_assert(BML_IvpActuatorSpringActiveLayoutCheck::spring_length == 0x9C);
static_assert(
    BML_IvpActuatorSpringActiveLayoutCheck::spring_constant == 0xA0);
static_assert(BML_IvpActuatorSpringActiveLayoutCheck::spring_damping == 0xA4);
static_assert(
    BML_IvpActuatorSpringActiveLayoutCheck::relative_position_damping ==
    0xA8);
static_assert(sizeof(IVP_Actuator_Suspension) == 0xA0);
#endif

inline IVP_Actuator_Check_Dist *IVP_Environment::create_check_dist(
    IVP_Template_Check_Dist *definition) {
    return new IVP_Actuator_Check_Dist(this, definition);
}

inline IVP_Actuator_Stabilizer *IVP_Environment::create_stabilizer(
    IVP_Template_Stabilizer *definition) {
    return new IVP_Actuator_Stabilizer(this, definition);
}

inline IVP_Actuator_Suspension *IVP_Environment::create_suspension(
    IVP_Template_Suspension *definition) {
    return new IVP_Actuator_Suspension(this, definition);
}

inline IVP_Actuator_Force *IVP_Environment::create_force(
    IVP_Template_Force *definition) {
    if (definition->active_float_force)
        return new IVP_Actuator_Force_Active(this, definition);
    return new IVP_Actuator_Force(this, definition);
}

inline IVP_Actuator_Torque *IVP_Environment::create_torque(
    IVP_Template_Torque *definition) {
    if (definition->active_float_torque ||
        definition->active_float_max_rotation_speed) {
        return new IVP_Actuator_Torque_Active(this, definition);
    }
    return new IVP_Actuator_Torque(this, definition);
}

inline IVP_Actuator_Rot_Mot *IVP_Environment::create_rotmot(
    IVP_Template_Rot_Mot *definition) {
    if (definition->active_float_max_torque ||
        definition->active_float_max_rotation_speed ||
        definition->active_float_power) {
        return new IVP_Actuator_Rot_Mot_Active(this, definition);
    }
    return new IVP_Actuator_Rot_Mot(this, definition);
}

#endif // BML_IVP_ACTUATOR_H
