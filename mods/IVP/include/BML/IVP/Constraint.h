#ifndef BML_IVP_CONSTRAINT_H
#define BML_IVP_CONSTRAINT_H

#include "BML/IVP/Cache.h"
#include "BML/IVP/SimulationUnit.h"
#include "BML/IVP/Reaction.h"

#include <cstddef>
#include <cstdint>

enum IVP_CONSTRAINT_AXIS_TYPE : std::int32_t {
    IVP_CONSTRAINT_AXIS_FREE = 0,
    IVP_CONSTRAINT_AXIS_FIXED = 1,
    IVP_CONSTRAINT_AXIS_LIMITED = 2,
    IVP_CONSTRAINT_AXIS_DUMMY = -1,
};

enum IVP_CONSTRAINT_FORCE_EXCEED : std::int32_t {
    IVP_CFE_NONE = 0,
    IVP_CFE_CLIP = 1,
    IVP_CFE_BREAK = 2,
    IVP_CFE_BEND = 3,
};

enum IVP_CONSTRAINT_FLAGS : std::int32_t {
    IVP_CONSTRAINT_FAST = 0x00,
    IVP_CONSTRAINT_GLOBAL = 0x0F,
    IVP_CONSTRAINT_ACTIVATED = 0x10,
    IVP_CONSTRAINT_SECURE = 0x10,
};

enum IVP_TRANSROT_INDEX : std::int32_t {
    IVP_TR_INDEX_TX = 0,
    IVP_TR_INDEX_TY = 1,
    IVP_TR_INDEX_TZ = 2,
    IVP_TR_INDEX_RX = 3,
    IVP_TR_INDEX_RY = 4,
    IVP_TR_INDEX_RZ = 5,
    IVP_TR_INDEX_MAX = 6,
};

enum IVP_NORM : std::int32_t {
    IVP_NORM_MINIMUM = 0,
    IVP_NORM_EUCLIDIC = 1,
    IVP_NORM_MAXIMUM = 2,
};

class IVP_Template_Constraint_Fixed_Keyframed
    : public IVP_Template_Controller_Motion {
public:
    IVP_Template_Constraint_Fixed_Keyframed() = default;
};

class IVP_Template_Constraint {
public:
    IVP_Template_Constraint() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateConstraintConstruct, this);
    }

    void set_constraint_Ros(
        IVP_Real_Object *referenceObject, const IVP_U_Point *anchorRos,
        const IVP_U_Point *knownAxisRos,
        const unsigned int fixedTranslationDimensions,
        const unsigned int fixedRotationDimensions,
        IVP_Real_Object *attachedObject,
        const IVP_U_Matrix *attachedDisplacement) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateConstraintSetRos, this,
            referenceObject, anchorRos, knownAxisRos,
            fixedTranslationDimensions, fixedRotationDimensions,
            attachedObject, attachedDisplacement);
    }

    void set_constraint_ws(
        IVP_Real_Object *referenceObject, const IVP_U_Point *anchorWs,
        const IVP_U_Point *knownAxisWs,
        const unsigned int fixedTranslationDimensions,
        const unsigned int fixedRotationDimensions,
        IVP_Real_Object *attachedObject,
        const IVP_U_Matrix *attachedDisplacement) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateConstraintSetWorld, this,
            referenceObject, anchorWs, knownAxisWs,
            fixedTranslationDimensions, fixedRotationDimensions,
            attachedObject, attachedDisplacement);
    }

    void fix_translation_axis(IVP_COORDINATE_INDEX axis) {
        axis_type[axis] = IVP_CONSTRAINT_AXIS_FIXED;
    }
    void free_translation_axis(IVP_COORDINATE_INDEX axis) {
        axis_type[axis] = IVP_CONSTRAINT_AXIS_FREE;
    }
    void limit_translation_axis(IVP_COORDINATE_INDEX axis,
                                IVP_FLOAT left, IVP_FLOAT right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateConstraintLimitTranslation,
            this, axis, left, right);
    }
    void fix_rotation_axis(IVP_COORDINATE_INDEX axis) {
        axis_type[axis + 3] = IVP_CONSTRAINT_AXIS_FIXED;
    }
    void free_rotation_axis(IVP_COORDINATE_INDEX axis) {
        axis_type[axis + 3] = IVP_CONSTRAINT_AXIS_FREE;
    }
    void limit_rotation_axis(IVP_COORDINATE_INDEX axis,
                             IVP_FLOAT left, IVP_FLOAT right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateConstraintLimitRotation,
            this, axis, left, right);
    }

    void set_stiffness_for_limited_axis(IVP_FLOAT stiffness) {
        limited_axis_stiffness = stiffness;
    }
    void set_reference_object(IVP_Real_Object *object) { objectR = object; }
    void set_attached_object(IVP_Real_Object *object) { objectA = object; }
    void set_translation_axes_as_object_space() { m_Ros_f_Rfs = nullptr; }
    void set_rotation_axes_as_translation_axes() { m_Ros_f_Rrs = nullptr; }
    void set_constraint_is_relaxed() { m_Aos_f_Afs = nullptr; }

    void set_fixing_point_Ros(const IVP_U_Point *anchor) {
        m_Ros_f_Rfs = &mm_Ros_f_Rfs;
        m_Ros_f_Rfs->vv.set(anchor);
    }
    void set_translation_axes_Ros(const IVP_U_Matrix3 *axes) {
        m_Ros_f_Rfs = &mm_Ros_f_Rfs;
        static_cast<IVP_U_Matrix3 &>(mm_Ros_f_Rfs) = *axes;
    }
    void set_rotation_axes_Ros(const IVP_U_Matrix3 *axes) {
        m_Ros_f_Rrs = &mm_Ros_f_Rrs;
        mm_Ros_f_Rrs = *axes;
    }
    void set_attached_fixing_point_Aos(const IVP_U_Point *anchor) {
        m_Aos_f_Afs = &mm_Aos_f_Afs;
        m_Aos_f_Afs->vv.set(anchor);
    }
    void set_attached_translation_axes_Aos(const IVP_U_Matrix3 *axes) {
        m_Aos_f_Afs = &mm_Aos_f_Afs;
        static_cast<IVP_U_Matrix3 &>(mm_Aos_f_Afs) = *axes;
    }
    void set_max_translation_impulse(
        IVP_CONSTRAINT_FORCE_EXCEED exceedType, IVP_FLOAT impulse) {
        for (int axis = 0; axis < 3; ++axis) {
            maximpulse_type[axis] = exceedType;
            maximpulse[axis] = impulse;
        }
    }
    void set_max_translation_impulse(
        IVP_COORDINATE_INDEX axis,
        IVP_CONSTRAINT_FORCE_EXCEED exceedType, IVP_FLOAT impulse) {
        maximpulse_type[axis] = exceedType;
        maximpulse[axis] = impulse;
    }
    void set_max_rotation_impulse(
        IVP_CONSTRAINT_FORCE_EXCEED exceedType, IVP_FLOAT impulse) {
        for (int axis = 3; axis < 6; ++axis) {
            maximpulse_type[axis] = exceedType;
            maximpulse[axis] = impulse;
        }
    }
    void set_max_rotation_impulse(
        IVP_COORDINATE_INDEX axis,
        IVP_CONSTRAINT_FORCE_EXCEED exceedType, IVP_FLOAT impulse) {
        maximpulse_type[axis + 3] = exceedType;
        maximpulse[axis + 3] = impulse;
    }

    void set_orientation(IVP_Real_Object *referenceObject,
                         IVP_Real_Object *attachedObject) {
        set_constraint_ws(referenceObject, nullptr, nullptr, 0, 3,
                          attachedObject, nullptr);
    }
    void set_ballsocket_ws(IVP_Real_Object *referenceObject,
                           const IVP_U_Point *anchor,
                           IVP_Real_Object *attachedObject) {
        set_constraint_ws(referenceObject, anchor, nullptr, 3, 0,
                          attachedObject, nullptr);
    }
    void set_ballsocket_Ros(IVP_Real_Object *referenceObject,
                            const IVP_U_Point *anchor,
                            IVP_Real_Object *attachedObject) {
        set_constraint_Ros(referenceObject, anchor, nullptr, 3, 0,
                           attachedObject, nullptr);
    }
    void set_ballsocket_tense_Ros(
        IVP_Real_Object *referenceObject, const IVP_U_Point *anchor,
        IVP_Real_Object *attachedObject,
        const IVP_U_Point *attachedDistanceRos) {
        IVP_U_Matrix displacement;
        displacement.init3();
        displacement.vv.set(attachedDistanceRos);
        set_constraint_Ros(referenceObject, anchor, nullptr, 3, 0,
                           attachedObject, &displacement);
    }
    void set_cardanjoint_ws(IVP_Real_Object *referenceObject,
                            const IVP_U_Point *anchor,
                            const IVP_U_Point *fixedAxis,
                            IVP_Real_Object *attachedObject) {
        set_constraint_ws(referenceObject, anchor, fixedAxis, 3, 1,
                          attachedObject, nullptr);
    }
    void set_cardanjoint_Ros(IVP_Real_Object *referenceObject,
                             const IVP_U_Point *anchor,
                             const IVP_U_Point *fixedAxis,
                             IVP_Real_Object *attachedObject) {
        set_constraint_Ros(referenceObject, anchor, fixedAxis, 3, 1,
                           attachedObject, nullptr);
    }
    void set_hinge_ws(IVP_Real_Object *referenceObject,
                      const IVP_U_Point *anchor,
                      const IVP_U_Point *freeAxis,
                      IVP_Real_Object *attachedObject) {
        set_constraint_ws(referenceObject, anchor, freeAxis, 3, 2,
                          attachedObject, nullptr);
    }
    void set_hinge_Ros(IVP_Real_Object *referenceObject,
                       const IVP_U_Point *anchor,
                       const IVP_U_Point *freeAxis,
                       IVP_Real_Object *attachedObject) {
        set_constraint_Ros(referenceObject, anchor, freeAxis, 3, 2,
                           attachedObject, nullptr);
    }
    void set_hinge_Ros(IVP_Real_Object *referenceObject,
                       const IVP_U_Point *anchor,
                       const IVP_U_Point *freeAxis,
                       IVP_Real_Object *attachedObject,
                       IVP_FLOAT borderLeft, IVP_FLOAT borderRight) {
        set_constraint_Ros(referenceObject, anchor, freeAxis, 3, 2,
                           attachedObject, nullptr);
        limit_rotation_axis(IVP_INDEX_Z, borderLeft, borderRight);
    }
    void set_fixed(IVP_Real_Object *referenceObject,
                   IVP_Real_Object *attachedObject) {
        set_constraint_ws(referenceObject, nullptr, nullptr, 3, 3,
                          attachedObject, nullptr);
    }

    IVP_CONSTRAINT_FLAGS flags;
    IVP_Real_Object *objectR;
    IVP_U_Matrix mm_Ros_f_Rfs;
    IVP_U_Matrix *m_Ros_f_Rfs;
    IVP_U_Matrix3 mm_Ros_f_Rrs;
    IVP_U_Matrix3 *m_Ros_f_Rrs;
    IVP_Real_Object *objectA;
    IVP_U_Matrix mm_Aos_f_Afs;
    IVP_U_Matrix *m_Aos_f_Afs;
    IVP_FLOAT force_factor;
    IVP_FLOAT damp_factor;
    IVP_FLOAT limited_axis_stiffness;
    IVP_CONSTRAINT_AXIS_TYPE axis_type[6];
    IVP_FLOAT borderleft_Rfs[6];
    IVP_FLOAT borderright_Rfs[6];
    IVP_CONSTRAINT_FORCE_EXCEED maximpulse_type[6];
    IVP_FLOAT maximpulse[6];
};

// Exact 23-slot public interface: seven inherited controller slots followed
// by sixteen constraint mutation slots.
class IVP_Constraint : public IVP_Controller_Dependent {
    friend struct BML_IvpConstraintLayoutCheck;

protected:
    struct Retail_Construction_Tag {};

    explicit IVP_Constraint(Retail_Construction_Tag) noexcept {}

    void initialize_reconstructed() {
        ::new (static_cast<void *>(&cores_of_constraint_system))
            IVP_Vector_of_Cores_2();
        is_enabled = IVP_TRUE;
    }

public:
    IVP_Constraint() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ConstraintConstruct, this,
            [this] { initialize_reconstructed(); });
    }

    void deactivate() {
        if (!is_enabled)
            return;
        is_enabled = IVP_FALSE;
        IVP_Controller_Manager::remove_controller_from_environment(
            this, IVP_FALSE);
    }
    void activate() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintActivate, this);
    }

    virtual ~IVP_Constraint() {
        if (is_enabled) {
            IVP_Controller_Manager::remove_controller_from_environment(
                this, IVP_TRUE);
        }
        cores_of_constraint_system.~IVP_Vector_of_Cores_2();
    }
    virtual void change_fixing_point_Ros(const IVP_U_Point *anchor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeFixingPoint,
            this, anchor);
    }
    virtual void change_target_fixing_point_Ros(const IVP_U_Point *anchor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeFixingPoint,
            this, anchor);
    }
    virtual void change_translation_axes_Ros(const IVP_U_Matrix3 *axes) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeTranslationAxes,
            this, axes);
    }
    virtual void change_target_translation_axes_Ros(
        const IVP_U_Matrix3 *axes) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeTranslationAxes,
            this, axes);
    }
    virtual void fix_translation_axis(IVP_COORDINATE_INDEX axis) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintFixTranslationAxis,
            this, axis);
    }
    virtual void free_translation_axis(IVP_COORDINATE_INDEX axis) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintFreeTranslationAxis,
            this, axis);
    }
    virtual void limit_translation_axis(
        IVP_COORDINATE_INDEX axis, IVP_FLOAT left, IVP_FLOAT right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLimitTranslationAxis,
            this, axis, left, right);
    }
    virtual void change_max_translation_impulse(
        IVP_CONSTRAINT_FORCE_EXCEED exceedType, IVP_FLOAT impulse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeMaxTranslationImpulse,
            this, exceedType, impulse);
    }
    virtual void change_rotation_axes_Ros(const IVP_U_Matrix3 *axes) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeRotationAxes,
            this, axes);
    }
    virtual void change_target_rotation_axes_Ros(
        const IVP_U_Matrix3 *axes) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeRotationAxes,
            this, axes);
    }
    virtual void fix_rotation_axis(IVP_COORDINATE_INDEX axis) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintFixRotationAxis,
            this, axis);
    }
    virtual void free_rotation_axis(IVP_COORDINATE_INDEX axis) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintFreeRotationAxis,
            this, axis);
    }
    virtual void limit_rotation_axis(
        IVP_COORDINATE_INDEX axis, IVP_FLOAT left, IVP_FLOAT right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLimitRotationAxis,
            this, axis, left, right);
    }
    virtual void change_max_rotation_impulse(
        IVP_CONSTRAINT_FORCE_EXCEED exceedType, IVP_FLOAT impulse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintChangeMaxRotationImpulse,
            this, exceedType, impulse);
    }
    virtual void change_Aos_to_relaxe_constraint() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintRelax, this);
    }
    virtual void change_Ros_to_relaxe_constraint() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintRelax, this);
    }

    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        return BML::IVP::ABI::InvokeThisOr<IVP_U_Vector<IVP_Core> *>(
            BML::IVP::ABI::Address::ConstraintGetControlledCores, this,
            [this] { return &cores_of_constraint_system; });
    }

    static IVP_Constraint *create_constraint_any_solver(
        IVP_Template_Constraint *constraintTemplate);

protected:
    void core_is_going_to_be_deleted_event(IVP_Core *) override {
        delete this;
    }
    IVP_DOUBLE get_minimum_simulation_frequency() override {
        return 1.0;
    }
    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_CONSTRAINTS;
    }
    IVP_Environment *get_environment() {
        IVP_Core *core = cores_of_constraint_system.len() > 0
            ? cores_of_constraint_system.element_at(0)
            : nullptr;
        return core ? core->get_environment() : nullptr;
    }

    std::uint32_t is_enabled : 2;
    // Complete retail constructors initialize this inline two-core vector.
    union {
        IVP_Vector_of_Cores_2 cores_of_constraint_system;
    };
};

// Keeps one object's transform keyframed relative to another object.  All
// class-specific retail bodies were link-stripped, but the imported Ballance
// UDT preserves this exact 0xE0 layout.  The implementation below is rebuilt
// from the neighboring algorithm using only Ballance-confirmed core, cache,
// controller-manager and reaction-solver ABI.
class IVP_Constraint_Fixed_Keyframed : public IVP_Controller_Dependent {
protected:
    IVP_U_Float_Point max_translation_force;
    IVP_U_Float_Point max_torque;
    IVP_FLOAT force_factor;
    IVP_FLOAT damp_factor;
    IVP_FLOAT torque_factor;
    IVP_FLOAT angular_damp_factor;
    IVP_Environment *l_environment;
    IVP_Time time_of_prime_position;
    IVP_Time time_of_prime_orientation_0;
    IVP_FLOAT i_delta_prime_orientation_time;
    IVP_U_Quat prime_orientation_0;
    IVP_U_Quat prime_orientation_1;
    IVP_BOOL angular_velocity_set;
    IVP_U_Point prime_position_Ros;
    IVP_U_Float_Point velocity_Ros;
    IVP_Real_Object *reference_obj;
    IVP_Real_Object *attached_obj;
    IVP_Vector_of_Cores_2 cores_of_constraint_system;

    void core_is_going_to_be_deleted_event(IVP_Core *) override {
        delete this;
    }
    IVP_DOUBLE get_minimum_simulation_frequency() override {
        return 1.0;
    }
    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        return &cores_of_constraint_system;
    }
    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_CONSTRAINTS;
    }
    void ensure_in_simulation() {
        if (l_environment && l_environment->get_controller_manager()) {
            l_environment->get_controller_manager()
                ->ensure_controller_in_simulation(this);
        }
    }
    void do_simulation_controller(
        IVP_Event_Sim *event, IVP_U_Vector<IVP_Core> *) override {
        if (!event || !event->environment ||
            !reference_obj || !attached_obj) {
            return;
        }

        IVP_Cache_Object *referenceCache =
            reference_obj->get_cache_object();
        IVP_Cache_Object *attachedCache =
            attached_obj->get_cache_object();
        if (!referenceCache || !attachedCache) {
            if (referenceCache)
                referenceCache->remove_reference();
            if (attachedCache)
                attachedCache->remove_reference();
            return;
        }

        const IVP_U_Matrix *worldFromReference =
            &referenceCache->m_world_f_object;
        const IVP_U_Matrix *worldFromAttached =
            &attachedCache->m_world_f_object;
        IVP_U_Float_Point axisX(1.0f, 0.0f, 0.0f);
        IVP_U_Float_Point axisY(0.0f, 1.0f, 0.0f);
        IVP_U_Float_Point axisZ(0.0f, 0.0f, 1.0f);

        IVP_Core *referenceCore = reference_obj->get_core();
        IVP_Core *attachedCore = attached_obj->get_core();
        IVP_Core *movableReference =
            referenceCore && !referenceCore->is_physical_unmoveable()
                ? referenceCore
                : nullptr;
        IVP_Core *movableAttached =
            attachedCore && !attachedCore->is_physical_unmoveable()
                ? attachedCore
                : nullptr;

        const IVP_Time currentTime = event->environment->get_current_time();
        const IVP_FLOAT orientationTime = static_cast<IVP_FLOAT>(
            currentTime - time_of_prime_orientation_0);
        IVP_U_Quat targetOrientation;
        if (angular_velocity_set == IVP_TRUE) {
            targetOrientation.set_interpolate_smoothly(
                &prime_orientation_0, &prime_orientation_1,
                (orientationTime + event->delta_time) *
                    i_delta_prime_orientation_time);
        } else {
            targetOrientation = prime_orientation_0;
        }

        IVP_U_Matrix3 referenceFromTarget;
        targetOrientation.set_matrix(&referenceFromTarget);
        IVP_U_Matrix3 worldFromTarget;
        worldFromReference->inline_mmult3(
            &referenceFromTarget, &worldFromTarget);
        IVP_U_Matrix3 attachedFromTarget;
        // mi2mult3 computes this * inverse(right).  Spell the three row
        // transforms out here so the link-stripped controller remains usable
        // without introducing another reconstructed matrix ABI entry.
        for (int row = 0; row < 3; ++row) {
            worldFromTarget.inline_vmult3(
                &worldFromAttached->rows[row],
                &attachedFromTarget.rows[row]);
        }
        IVP_U_Quat orientationError;
        orientationError.set_quaternion(&attachedFromTarget);
        IVP_U_Float_Point deltaAngles(
            2.0f * IVP_Inline_Math::fast_asin(
                static_cast<IVP_FLOAT>(orientationError.x)),
            2.0f * IVP_Inline_Math::fast_asin(
                static_cast<IVP_FLOAT>(orientationError.y)),
            2.0f * IVP_Inline_Math::fast_asin(
                static_cast<IVP_FLOAT>(orientationError.z)));
        if (orientationError.w > 0.0)
            deltaAngles.mult(-1.0);

        IVP_Solver_Core_Reaction reaction;
        reaction.init_reaction_solver_rotation_ws(
            movableAttached, movableReference, &axisX, &axisY, &axisZ);
        if (reaction.invert_3x3_matrix() == IVP_FAULT) {
            referenceCache->remove_reference();
            attachedCache->remove_reference();
            return;
        }
        IVP_U_Float_Point desiredAngularVelocity;
        desiredAngularVelocity.set_multiple(
            &deltaAngles, event->i_delta_time * torque_factor);
        desiredAngularVelocity.add_multiple(
            &reaction.delta_velocity_ds, -angular_damp_factor);
        IVP_U_Float_Point angularImpulse;
        reaction.m_velocity_ds_f_impulse_ds.inline_vmult3(
            &desiredAngularVelocity, &angularImpulse);
        reaction.exert_angular_impulse_dim3(
            movableAttached, movableReference, angularImpulse);

        const IVP_FLOAT positionTime = static_cast<IVP_FLOAT>(
            currentTime - time_of_prime_position);
        IVP_U_Point targetPositionReference;
        targetPositionReference.add_multiple(
            &prime_position_Ros, &velocity_Ros,
            positionTime + event->delta_time);
        IVP_U_Point targetPositionWorld;
        worldFromReference->inline_vmult4(
            &targetPositionReference, &targetPositionWorld);
        IVP_U_Float_Point desiredVelocity;
        desiredVelocity.inline_subtract_and_mult(
            &targetPositionWorld, worldFromAttached->get_position(),
            force_factor * event->i_delta_time);
        if (referenceCore)
            desiredVelocity.add_multiple(&referenceCore->speed, damp_factor);
        if (attachedCore)
            desiredVelocity.add_multiple(&attachedCore->speed, -damp_factor);

        referenceCache->remove_reference();
        attachedCache->remove_reference();

        reaction.init_reaction_solver_translation_ws(
            movableAttached, movableReference, targetPositionWorld,
            &axisX, &axisY, &axisZ);
        if (reaction.invert_3x3_matrix() == IVP_FAULT)
            return;
        IVP_U_Float_Point impulseWorld;
        reaction.m_velocity_ds_f_impulse_ds.inline_vmult3(
            &desiredVelocity, &impulseWorld);
        reaction.exert_impulse_dim3(
            movableAttached, movableReference, impulseWorld);
    }

public:
    IVP_Environment *get_environment() { return l_environment; }

    void set_prime_position_Ros(
        const IVP_U_Point *positionRos,
        const IVP_U_Float_Point *velocityRos,
        const IVP_Time &currentTime) {
        time_of_prime_position = currentTime;
        prime_position_Ros.set(positionRos);
        velocity_Ros.set(velocityRos);
        ensure_in_simulation();
    }

    void set_prime_orientation_Ros(
        const IVP_U_Quat *orientation0Ros, const IVP_Time &time0,
        const IVP_U_Quat *orientation1Ros = nullptr,
        IVP_FLOAT deltaTime = 1.0f) {
        prime_orientation_0 = *orientation0Ros;
        if (orientation1Ros) {
            prime_orientation_1 = *orientation1Ros;
            i_delta_prime_orientation_time = 1.0f / deltaTime;
            angular_velocity_set = IVP_TRUE;
        } else {
            angular_velocity_set = IVP_FALSE;
        }
        time_of_prime_orientation_0 = time0;
        ensure_in_simulation();
    }

    IVP_Constraint_Fixed_Keyframed(
        IVP_Real_Object *referenceObject,
        IVP_Real_Object *attachedObject,
        const IVP_Template_Constraint_Fixed_Keyframed *definition)
        : force_factor(definition->force_factor),
          damp_factor(definition->damp_factor),
          torque_factor(definition->torque_factor),
          angular_damp_factor(definition->angular_damp_factor),
          l_environment(referenceObject->get_environment()),
          time_of_prime_position(0.0),
          time_of_prime_orientation_0(0.0),
          i_delta_prime_orientation_time(1.0f),
          angular_velocity_set(IVP_FALSE),
          reference_obj(referenceObject),
          attached_obj(attachedObject) {
        max_translation_force.set(&definition->max_translation_force);
        max_torque.set(definition->max_torque, definition->max_torque,
                       definition->max_torque);
        velocity_Ros.set_to_zero();

        IVP_Core *referenceCore = referenceObject->get_core();
        IVP_Core *attachedCore = attachedObject->get_core();
        if (referenceCore && !referenceCore->is_physical_unmoveable())
            cores_of_constraint_system.add(referenceCore);
        if (attachedCore && !attachedCore->is_physical_unmoveable())
            cores_of_constraint_system.add(attachedCore);

        IVP_Cache_Object *attachedCache = attachedObject->get_cache_object();
        IVP_Cache_Object *referenceCache = referenceObject->get_cache_object();
        if (attachedCache && referenceCache) {
            IVP_U_Matrix referenceFromAttached;
            referenceCache->m_world_f_object.inline_mimult4(
                &attachedCache->m_world_f_object, &referenceFromAttached);
            prime_position_Ros.set(referenceFromAttached.get_position());
            prime_orientation_0.set_quaternion(&referenceFromAttached);
        } else {
            prime_position_Ros.set_to_zero();
            prime_orientation_0.init();
        }
        if (attachedCache)
            attachedCache->remove_reference();
        if (referenceCache)
            referenceCache->remove_reference();

        if (l_environment && l_environment->get_controller_manager()) {
            l_environment->get_controller_manager()
                ->announce_controller_to_environment(this);
        }
    }

    ~IVP_Constraint_Fixed_Keyframed() override {
        IVP_Controller_Manager::remove_controller_from_environment(
            this, IVP_TRUE);
    }
};

class IVP_Constraint_Local_Anchor : public IVP_U_Matrix {
public:
    // Ballance RVA 0x28210 initializes only rot. The enclosing constraint
    // assigns object after both embedded anchor layers have been constructed.
    IVP_Constraint_Local_Anchor() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ConstraintLocalAnchorConstruct, this,
            [this] { rot = nullptr; });
    }
    ~IVP_Constraint_Local_Anchor() = default;

    IVP_Real_Object *object;
    IVP_U_Matrix3 *rot;
};

class IVP_Constraint_Local_MaxImpulse {
    friend class IVP_Constraint_Local;

    IVP_FLOAT halfimpulse[IVP_TR_INDEX_MAX];
    IVP_CONSTRAINT_FORCE_EXCEED type[IVP_TR_INDEX_MAX];
};

class IVP_Constraint_Local : public IVP_Constraint {
    friend struct BML_IvpConstraintLocalLayoutCheck;

public:
    explicit IVP_Constraint_Local(
        const IVP_Template_Constraint &constraintTemplate)
        : IVP_Constraint(IVP_Constraint::Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ConstraintLocalConstruct, this,
            [this, &constraintTemplate] {
                IVP_Constraint::initialize_reconstructed();
                ::new (static_cast<void *>(&m_Rfs_f_Rcs))
                    IVP_Constraint_Local_Anchor();
                ::new (static_cast<void *>(&m_Afs_f_Acs))
                    IVP_Constraint_Local_Anchor();
                ::new (static_cast<void *>(&mapping_uRfs_f_Rfs))
                    IVP_U_Mapping();
                ::new (static_cast<void *>(&mapping_uRrs_f_Rrs))
                    IVP_U_Mapping();
                BML::IVP::ABI::InvokeThis<void>(
                    BML::IVP::ABI::Address::ConstraintLocalInitialize,
                    this, &constraintTemplate);
                activate();
            },
            &constraintTemplate);
    }

    ~IVP_Constraint_Local() override {
        FreeOwned(maxforce);
        maxforce = nullptr;
        FreeOwned(m_Rfs_f_Rcs.rot);
        m_Rfs_f_Rcs.rot = nullptr;
        FreeOwned(m_Afs_f_Acs.rot);
        m_Afs_f_Acs.rot = nullptr;
    }

    IVP_Real_Object *get_objectR() { return m_Rfs_f_Rcs.object; }
    IVP_Real_Object *get_objectA() { return m_Afs_f_Acs.object; }

    void change_fixing_point_Ros(const IVP_U_Point *anchor) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeFixingPoint,
            this, anchor);
    }
    void change_target_fixing_point_Ros(const IVP_U_Point *anchor) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeTargetFixingPoint,
            this, anchor);
    }
    void change_translation_axes_Ros(const IVP_U_Matrix3 *axes) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeTranslationAxes,
            this, axes);
    }
    void change_target_translation_axes_Ros(
        const IVP_U_Matrix3 *axes) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeTargetTranslationAxes,
            this, axes);
    }
    void fix_translation_axis(IVP_COORDINATE_INDEX axis) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalFixTranslationAxis,
            this, axis);
    }
    void free_translation_axis(IVP_COORDINATE_INDEX axis) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalFreeTranslationAxis,
            this, axis);
    }
    void limit_translation_axis(
        IVP_COORDINATE_INDEX axis, IVP_FLOAT left,
        IVP_FLOAT right) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalLimitTranslationAxis,
            this, axis, left, right);
    }
    void change_max_translation_impulse(
        IVP_CONSTRAINT_FORCE_EXCEED exceedType,
        IVP_FLOAT impulse) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeMaxTranslationImpulse,
            this, exceedType, impulse);
    }
    void change_rotation_axes_Ros(const IVP_U_Matrix3 *axes) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeRotationAxes,
            this, axes);
    }
    void change_target_rotation_axes_Ros(
        const IVP_U_Matrix3 *axes) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeTargetRotationAxes,
            this, axes);
    }
    void fix_rotation_axis(IVP_COORDINATE_INDEX axis) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalFixRotationAxis,
            this, axis);
    }
    void free_rotation_axis(IVP_COORDINATE_INDEX axis) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalFreeRotationAxis,
            this, axis);
    }
    void limit_rotation_axis(
        IVP_COORDINATE_INDEX axis, IVP_FLOAT left,
        IVP_FLOAT right) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalLimitRotationAxis,
            this, axis, left, right);
    }
    void change_max_rotation_impulse(
        IVP_CONSTRAINT_FORCE_EXCEED exceedType,
        IVP_FLOAT impulse) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalChangeMaxRotationImpulse,
            this, exceedType, impulse);
    }
    void change_Aos_to_relaxe_constraint() override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalRelaxAttached, this);
    }
    void change_Ros_to_relaxe_constraint() override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalRelaxReference, this);
    }

protected:
    void core_is_going_to_be_deleted_event(IVP_Core *core) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalCoreDeleted,
            this, core);
    }
    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *coreList) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::ConstraintLocalSimulate,
            this, event, coreList);
    }

private:
    template <typename T>
    static void FreeOwned(T *pointer) {
        if (pointer) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::OperatorDelete, pointer);
        }
    }

    IVP_FLOAT force_factor;
    IVP_FLOAT damp_factor_div_force;
    IVP_CONSTRAINT_AXIS_TYPE fixed[IVP_TR_INDEX_MAX];
    IVP_FLOAT borderleft_Rfs[IVP_TR_INDEX_MAX];
    IVP_FLOAT borderright_Rfs[IVP_TR_INDEX_MAX];
    IVP_FLOAT limited_axis_stiffness;
    IVP_Constraint_Local_MaxImpulse *maxforce;
    union {
        IVP_Constraint_Local_Anchor m_Rfs_f_Rcs;
    };
    union {
        IVP_Constraint_Local_Anchor m_Afs_f_Acs;
    };
    union {
        IVP_U_Mapping mapping_uRfs_f_Rfs;
    };
    union {
        IVP_U_Mapping mapping_uRrs_f_Rrs;
    };
    unsigned char fixedtrans_dim;
    unsigned char fixedrot_dim;
    unsigned char limitedtrans_dim;
    unsigned char limitedrot_dim;
    unsigned char matrix_size;
    IVP_NORM norm : 8;
};

struct BML_IvpConstraintLayoutCheck {
    static constexpr std::size_t cores =
        offsetof(IVP_Constraint, cores_of_constraint_system);
};

struct BML_IvpConstraintLocalLayoutCheck {
    static constexpr std::size_t force_factor =
        offsetof(IVP_Constraint_Local, force_factor);
    static constexpr std::size_t fixed =
        offsetof(IVP_Constraint_Local, fixed);
    static constexpr std::size_t maxforce =
        offsetof(IVP_Constraint_Local, maxforce);
    static constexpr std::size_t reference_anchor =
        offsetof(IVP_Constraint_Local, m_Rfs_f_Rcs);
    static constexpr std::size_t attached_anchor =
        offsetof(IVP_Constraint_Local, m_Afs_f_Acs);
    static constexpr std::size_t reference_mapping =
        offsetof(IVP_Constraint_Local, mapping_uRfs_f_Rfs);
    static constexpr std::size_t attached_mapping =
        offsetof(IVP_Constraint_Local, mapping_uRrs_f_Rrs);
    static constexpr std::size_t fixed_translation_dimensions =
        offsetof(IVP_Constraint_Local, fixedtrans_dim);
};

inline IVP_Constraint *IVP_Constraint::create_constraint_any_solver(
    IVP_Template_Constraint *constraintTemplate) {
    if (!constraintTemplate ||
        (!constraintTemplate->objectR && !constraintTemplate->objectA)) {
        return nullptr;
    }
    void *memory = BML::IVP::ABI::Invoke<void *>(
        BML::IVP::ABI::Address::OperatorNew,
        static_cast<unsigned int>(sizeof(IVP_Constraint_Local)));
    if (!memory)
        return nullptr;
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ConstraintLocalConstruct,
        memory, constraintTemplate);
    return static_cast<IVP_Constraint_Local *>(memory);
}

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Template_Constraint_Fixed_Keyframed) == 0x24);
static_assert(sizeof(IVP_Template_Constraint) == 0x200);
static_assert(offsetof(IVP_Template_Constraint, objectR) == 0x04);
static_assert(offsetof(IVP_Template_Constraint, m_Ros_f_Rfs) == 0x88);
static_assert(offsetof(IVP_Template_Constraint, objectA) == 0xF4);
static_assert(offsetof(IVP_Template_Constraint, force_factor) == 0x17C);
static_assert(offsetof(IVP_Template_Constraint, axis_type) == 0x188);
static_assert(offsetof(IVP_Template_Constraint, maximpulse) == 0x1E8);
static_assert(sizeof(IVP_Vector_of_Cores_2) == 0x10);
static_assert(sizeof(IVP_Constraint) == 0x18);
static_assert(BML_IvpConstraintLayoutCheck::cores == 0x08);
static_assert(sizeof(IVP_Constraint_Fixed_Keyframed) == 0xE0);
static_assert(sizeof(IVP_Constraint_Local_Anchor) == 0x88);
static_assert(offsetof(IVP_Constraint_Local_Anchor, object) == 0x80);
static_assert(offsetof(IVP_Constraint_Local_Anchor, rot) == 0x84);
static_assert(sizeof(IVP_Constraint_Local_MaxImpulse) == 0x30);
static_assert(sizeof(IVP_U_Mapping) == 0x03);
static_assert(sizeof(IVP_Constraint_Local) == 0x190);
static_assert(BML_IvpConstraintLocalLayoutCheck::force_factor == 0x18);
static_assert(BML_IvpConstraintLocalLayoutCheck::fixed == 0x20);
static_assert(BML_IvpConstraintLocalLayoutCheck::maxforce == 0x6C);
static_assert(BML_IvpConstraintLocalLayoutCheck::reference_anchor == 0x70);
static_assert(BML_IvpConstraintLocalLayoutCheck::attached_anchor == 0xF8);
static_assert(BML_IvpConstraintLocalLayoutCheck::reference_mapping == 0x180);
static_assert(BML_IvpConstraintLocalLayoutCheck::attached_mapping == 0x183);
static_assert(
    BML_IvpConstraintLocalLayoutCheck::fixed_translation_dimensions == 0x186);
#endif

#endif // BML_IVP_CONSTRAINT_H
