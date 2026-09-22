#ifndef BML_IVP_BUOYANCY_H
#define BML_IVP_BUOYANCY_H

#include "BML/IVP/Attacher.h"
#include "BML/IVP/Object.h"
#include "BML/IVP/Set.h"
#include "BML/IVP/Interpolation.h"

#include <cstddef>
#include <cstdint>

class IVP_Listener_Phantom;
class IVP_Mindist;
class IVP_Mindist_Base;
class IVP_Mindist_Manager;
class IVP_Controller_Buoyancy;
class IVP_VHash_Store;

// Ballance instantiates this public template with a private controller type.
// Keep that instantiation complete without publishing a guessed controller
// body: the three retained callbacks are dispatched to the retail code that
// owns IVP_Controller_Buoyancy construction and destruction.
template <>
class IVP_Attacher_To_Cores<IVP_Controller_Buoyancy>
    : protected IVP_Listener_Set_Active<IVP_Core> {
    friend struct
        BML_IvpAttacherToCoresLayoutCheck<IVP_Controller_Buoyancy>;

protected:
    struct Retail_Construction_Tag {};

    // Storage-only path for the complete retail buoyancy constructor.  Its
    // body constructs both fields itself; touching either one here would run
    // a different IVP constructor before physics_RT receives the object.
    explicit IVP_Attacher_To_Cores(Retail_Construction_Tag) {}

public:
    void attachment_is_going_to_be_deleted(
        IVP_Controller_Buoyancy *, IVP_Core *attachedCore) {
        core_to_attachment_hash.remove_elem(attachedCore);
    }

    union {
        IVP_VHash_Store core_to_attachment_hash;
    };

protected:
    // The concrete derived destructor owns the complete retail teardown.
    // This layer must not unregister or destroy the hash a second time.
    virtual ~IVP_Attacher_To_Cores() {}

    void element_added(
        IVP_U_Set_Active<IVP_Core> *cores, IVP_Core *core) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::BuoyancyAttacherElementAdded,
            this, cores, core);
    }

    void element_removed(
        IVP_U_Set_Active<IVP_Core> *cores, IVP_Core *core) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::BuoyancyAttacherElementRemoved,
            this, cores, core);
    }

    void pset_is_going_to_be_deleted(
        IVP_U_Set_Active<IVP_Core> *cores) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::BuoyancyAttacherSetDeleted,
            this, cores);
    }

    IVP_U_Set_Active<IVP_Core> *set_of_cores;
};

class IVP_Template_Buoyancy {
public:
    IVP_Template_Buoyancy() = default;

    struct MI_Weights {
        IVP_FLOAT weight_current_speed = 1.0f;
        IVP_FLOAT weight_surface = 1.0f;
        IVP_FLOAT weight_rot_speed = 1.0f;
    };

    IVP_BOOL simulate_wing_behavior = IVP_FALSE;
    IVP_FLOAT medium_density = 0.3f;
    IVP_FLOAT pressure_damp_factor = 0.1f;
    IVP_FLOAT friction_damp_factor = 0.05f;
    IVP_FLOAT torque_factor = 1.0f;
    IVP_FLOAT buoyancy_eps = 1.0e-10f;
    IVP_FLOAT ball_rot_dampening_factor = 0.01f;
    IVP_FLOAT viscosity_factor = 0.01f;
    IVP_FLOAT viscosity_input_factor = 2.0f;
    IVP_BOOL use_interpolation = IVP_FALSE;
    int max_interpolation_tries = 10;
    int max_tries_nr_of_vectors_involved = 15;
    IVP_BOOL use_stochastic_insertion = IVP_TRUE;
    IVP_BOOL insert_extrapol_only = IVP_TRUE;
    MI_Weights mi_weights;
    IVP_FLOAT max_res = 0.01f;
    int nr_future_psi_for_extrapolation = 1;
};

class IVP_Buoyancy_Input : public IVP_MI_Vector_Base {
public:
    IVP_Buoyancy_Input() {
        nr_of_elements = 12;
        weight_statistic = 0.0f;
        rel_speed_of_current_os.hesse_val = 0.0f;
        rot_speed.hesse_val = 0.0f;
    }

    IVP_U_Float_Hesse surface_os;
    IVP_U_Float_Point rel_speed_of_current_os;
    IVP_U_Float_Point rot_speed;
};

class IVP_Buoyancy_Output : public IVP_MI_Vector_Base {
public:
    IVP_Buoyancy_Output() {
        nr_of_elements = 18;
        weight_statistic = 0.0f;
        volume_under = 0.0f;
        object_visible_surface_content_under = 0.0f;
    }

    IVP_U_Float_Point volume_center_under;
    IVP_U_Float_Point sum_impulse;
    IVP_U_Float_Point sum_impulse_x_point;
    IVP_U_Float_Point sum_impulse_x_movevector;
    IVP_FLOAT volume_under;
    IVP_FLOAT object_visible_surface_content_under;
};

class IVP_Liquid_Surface_Descriptor {
public:
    virtual void calc_liquid_surface(
        IVP_Environment *environment, IVP_Core *core,
        IVP_U_Float_Hesse *surfaceNormalOut,
        IVP_U_Float_Point *absoluteCurrentSpeedOut) = 0;
};

class IVP_Liquid_Surface_Descriptor_Simple
    : public IVP_Liquid_Surface_Descriptor {
public:
    IVP_Liquid_Surface_Descriptor_Simple(
        const IVP_U_Float_Hesse *surfaceIn,
        const IVP_U_Float_Point *absoluteCurrentSpeedIn) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::LiquidSurfaceSimpleConstruct,
            this, surfaceIn, absoluteCurrentSpeedIn);
    }

    void calc_liquid_surface(
        IVP_Environment *environment, IVP_Core *core,
        IVP_U_Float_Hesse *surfaceNormalOut,
        IVP_U_Float_Point *absoluteCurrentSpeedOut) override {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::LiquidSurfaceSimpleCalculate,
            this, [this, surfaceNormalOut, absoluteCurrentSpeedOut] {
                if (surfaceNormalOut) surfaceNormalOut->set4(&surface);
                if (absoluteCurrentSpeedOut)
                    absoluteCurrentSpeedOut->set(&abs_speed_of_current);
            }, environment, core, surfaceNormalOut,
            absoluteCurrentSpeedOut);
    }

    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_U_Float_Hesse surface;
    IVP_U_Float_Point abs_speed_of_current;
};

// Layout exposed so Mods can obtain the active core set after converting a
// real object to a phantom. The nested mindist set stays intentionally opaque.
class IVP_Controller_Phantom {
    friend class IVP_Real_Object;
    friend class IVP_Mindist_Manager;

protected:
    IVP_Controller_Phantom(
        IVP_Real_Object *targetObject,
        const IVP_Template_Phantom *configuration) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ControllerPhantomConstruct,
            this, targetObject, configuration);
    }

    void mindist_entered_volume(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ControllerPhantomMindistEnteredVolume,
            this, mindist);
    }
    void mindist_left_volume(IVP_Mindist *mindist) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ControllerPhantomMindistLeftVolume,
            this, mindist);
    }

public:
    IVP_U_Set_Active<IVP_Real_Object> *get_intruding_objects() const {
        return set_of_objects;
    }
    IVP_U_Set_Active<IVP_Core> *get_intruding_cores() const {
        return set_of_cores;
    }
    IVP_U_Set_Active<IVP_Mindist_Base> *get_intruding_mindists() {
        return &set_of_mindists;
    }
    IVP_Real_Object *get_object() const { return object; }

    void wake_all_sleeping_objects() {
        if (object)
            object->ensure_in_simulation();
    }
    void add_listener_phantom(IVP_Listener_Phantom *listener) {
        listeners.add(listener);
    }
    void remove_listener_phantom(IVP_Listener_Phantom *listener) {
        listeners.remove(listener);
    }

    ~IVP_Controller_Phantom() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ControllerPhantomDestruct, this);
    }

    BML_IVP_RETAIL_DEALLOCATED_OBJECT;

    IVP_U_Set_Active<IVP_Core> *get_set_of_cores() const {
        return set_of_cores;
    }

    IVP_Real_Object *object;
    IVP_FLOAT exit_policy_extra_radius;
    // The retained complete constructor/destructor own both embedded
    // containers. Anonymous unions preserve their exact public field types
    // and offsets without adding a second compiler-generated lifetime around
    // the retail calls.
    union {
        IVP_U_Vector<IVP_Listener_Phantom> listeners;
    };
    union {
        IVP_U_Set_Active<IVP_Mindist_Base> set_of_mindists;
    };
    IVP_U_Set_Active<IVP_Real_Object> *set_of_objects;
    IVP_VHash_Store *mindist_object_counter;
    IVP_VHash_Store *mindist_core_counter;
    IVP_U_Set_Active<IVP_Core> *set_of_cores;
    IVP_Time time_of_last_set_transformation;
};

// The retail object registers itself as a listener of the supplied active set
// and is normally lifetime-managed by that set. create() uses the DLL's own
// allocator so its deleting destructor can safely release the object later.
class IVP_Attacher_To_Cores_Buoyancy
    : public IVP_Attacher_To_Cores<IVP_Controller_Buoyancy> {
    friend struct BML_IvpAttacherToCoresBuoyancyLayoutCheck;

public:
    // The complete retail constructor replaces the temporary base vptr and
    // the active-set callback eventually deletes this object through that
    // retail table. The public constructor must therefore use the DLL heap.
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_Attacher_To_Cores_Buoyancy(
        IVP_Template_Buoyancy &definition,
        IVP_U_Set_Active<IVP_Core> *cores,
        IVP_Liquid_Surface_Descriptor *surfaceDescriptor)
        : IVP_Attacher_To_Cores<IVP_Controller_Buoyancy>(
              Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::BuoyancyAttacherConstruct,
            this, &definition, cores, surfaceDescriptor);
    }

    ~IVP_Attacher_To_Cores_Buoyancy() override {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::BuoyancyAttacherDestruct, this,
            [this] {
                if (set_of_cores)
                    set_of_cores->remove_listener_set_active(this);
                core_to_attachment_hash.~IVP_VHash_Store();
            });
    }

    static IVP_Attacher_To_Cores_Buoyancy *create(
        IVP_Template_Buoyancy &definition,
        IVP_U_Set_Active<IVP_Core> *cores,
        IVP_Liquid_Surface_Descriptor *surfaceDescriptor) {
        void *memory = BML::IVP::ABI::Invoke<void *>(
            BML::IVP::ABI::Address::OperatorNew, 0x70u);
        if (!memory)
            return nullptr;
        auto *result = static_cast<IVP_Attacher_To_Cores_Buoyancy *>(memory);
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::BuoyancyAttacherConstruct,
            result, &definition, cores, surfaceDescriptor);
        return result;
    }

    virtual IVP_Template_Buoyancy *get_parameters_per_core(IVP_Core *core) {
        return BML::IVP::ABI::InvokeThisOr<IVP_Template_Buoyancy *>(
            BML::IVP::ABI::Address::BuoyancyAttacherGetParameters, this,
            [this] { return &template_buoyancy; }, core);
    }
    virtual IVP_SurfaceManager *get_buoyancy_surface(
        IVP_Real_Object *object) {
        return BML::IVP::ABI::InvokeThisOr<IVP_SurfaceManager *>(
            BML::IVP::ABI::Address::BuoyancyAttacherGetSurface, this,
            [object] {
                return object ? object->get_surface_manager() : nullptr;
            }, object);
    }

private:
    // RVA 0x104C0 copy-constructs this transport block after constructing the
    // retail base.  Keep it inactive until that complete body runs.
    union {
        IVP_Template_Buoyancy template_buoyancy;
    };
    IVP_U_Set_Active<IVP_Core> *set_of_cores;
    IVP_Liquid_Surface_Descriptor *liquid_surface_descriptor;
};

struct BML_IvpAttacherToCoresBuoyancyLayoutCheck {
    static constexpr std::size_t definition =
        offsetof(IVP_Attacher_To_Cores_Buoyancy, template_buoyancy);
    static constexpr std::size_t cores =
        offsetof(IVP_Attacher_To_Cores_Buoyancy, set_of_cores);
    static constexpr std::size_t surface =
        offsetof(IVP_Attacher_To_Cores_Buoyancy,
                 liquid_surface_descriptor);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Template_Buoyancy) == 0x4C);
static_assert(offsetof(IVP_Template_Buoyancy, mi_weights) == 0x38);
static_assert(sizeof(IVP_Buoyancy_Input) == 0x40);
static_assert(sizeof(IVP_Buoyancy_Output) == 0x58);
static_assert(sizeof(IVP_Liquid_Surface_Descriptor) == 0x04);
static_assert(sizeof(IVP_Liquid_Surface_Descriptor_Simple) == 0x24);
static_assert(offsetof(IVP_Liquid_Surface_Descriptor_Simple, surface) == 0x04);
static_assert(sizeof(IVP_Controller_Phantom) == 0x40);
static_assert(offsetof(IVP_Controller_Phantom, listeners) == 0x08);
static_assert(offsetof(IVP_Controller_Phantom, set_of_mindists) == 0x10);
static_assert(offsetof(IVP_Controller_Phantom, set_of_objects) == 0x28);
static_assert(offsetof(IVP_Controller_Phantom, mindist_object_counter) == 0x2C);
static_assert(offsetof(IVP_Controller_Phantom, mindist_core_counter) == 0x30);
static_assert(offsetof(IVP_Controller_Phantom, set_of_cores) == 0x34);
static_assert(offsetof(IVP_Controller_Phantom,
                       time_of_last_set_transformation) == 0x38);
static_assert(sizeof(IVP_Attacher_To_Cores<IVP_Controller_Buoyancy>) == 0x1C);
static_assert(
    BML_IvpAttacherToCoresLayoutCheck<IVP_Controller_Buoyancy>::attachments ==
    0x04);
static_assert(
    BML_IvpAttacherToCoresLayoutCheck<IVP_Controller_Buoyancy>::cores == 0x18);
static_assert(sizeof(IVP_Attacher_To_Cores_Buoyancy) == 0x70);
static_assert(BML_IvpAttacherToCoresBuoyancyLayoutCheck::definition == 0x1C);
static_assert(BML_IvpAttacherToCoresBuoyancyLayoutCheck::cores == 0x68);
static_assert(BML_IvpAttacherToCoresBuoyancyLayoutCheck::surface == 0x6C);
#endif

#endif // BML_IVP_BUOYANCY_H
