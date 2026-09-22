#ifndef BML_IVP_OBJECT_ATTACH_H
#define BML_IVP_OBJECT_ATTACH_H

#include "BML/IVP/Cache.h"
#include "BML/IVP/Controller.h"
#include "BML/IVP/Templates.h"

#include <cmath>
#include <cstring>
#include <new>

class IVP_Real_Object;
class IVP_Template_Real_Object;

// Public attachment helper from the Ballance-era IVP headers. The retail DLL
// retains the empty 1-byte type but none of these three static bodies. The
// implementations below are selective reconstructions around retained retail
// Core/Object operations; they intentionally do not import a foreign IVP Core.
class IVP_Object_Attach {
public:
    static void attach_object(
        IVP_Real_Object *parent, IVP_Real_Object *attachedObject,
        IVP_DOUBLE maxDistanceToParentMassCenter = -1.0f) {
        if (!parent || !attachedObject || parent == attachedObject ||
            !parent->get_core() || !attachedObject->get_core() ||
            !parent->get_environment() ||
            parent->get_environment() != attachedObject->get_environment()) {
            return;
        }

        IVP_Core *parentCore = parent->get_core();
        IVP_Core *oldCore = attachedObject->get_core();
        if (parentCore == oldCore)
            return;

        if (maxDistanceToParentMassCenter < 0.0f) {
            const IVP_Time now = parent->get_environment()->get_current_time();
            IVP_U_Point parentCenter;
            IVP_U_Point attachedCenter;
            parentCore->inline_calc_at_position(now, &parentCenter);
            oldCore->inline_calc_at_position(now, &attachedCenter);
            maxDistanceToParentMassCenter =
                std::sqrt(parentCenter.quad_distance_to(&attachedCenter));
        }

        // The original synchronizes both objects into a simulated state before
        // changing ownership. State values below IVP_MT_NOT_SIM are simulated.
        const bool parentSimulated =
            parent->get_movement_state() < IVP_MT_NOT_SIM;
        const bool attachedSimulated =
            attachedObject->get_movement_state() < IVP_MT_NOT_SIM;
        if (parentSimulated && !attachedSimulated)
            attachedObject->revive_object_for_simulation();
        else if (!parentSimulated && attachedSimulated)
            parent->revive_object_for_simulation();

        IVP_Cache_Object *attachedCache =
            attachedObject->get_cache_object_no_lock();
        if (!attachedCache)
            return;
        const IVP_U_Quat worldFromObject = attachedCache->q_world_f_object;
        IVP_U_Point worldObjectPosition;
        worldObjectPosition.set(
            attachedCache->m_world_f_object.get_position());

        const IVP_DOUBLE radius =
            maxDistanceToParentMassCenter + oldCore->upper_limit_radius;
        if (radius > parentCore->upper_limit_radius)
            parentCore->upper_limit_radius = static_cast<IVP_FLOAT>(radius);
        const IVP_DOUBLE deviation =
            maxDistanceToParentMassCenter + oldCore->max_surface_deviation;
        if (deviation > parentCore->max_surface_deviation) {
            parentCore->max_surface_deviation =
                static_cast<IVP_FLOAT>(deviation);
        }

        oldCore->unlink_obj_from_core_and_maybe_destroy(attachedObject);
        attachedObject->physical_core = parentCore;
        attachedObject->friction_core = parentCore;
        attachedObject->original_core = parentCore;
        parentCore->core_add_link_to_obj(attachedObject);
        attachedObject->set_movement_state(parent->get_movement_state());

        reposition_object_Ros(nullptr, attachedObject, &worldFromObject,
                              &worldObjectPosition, IVP_FALSE);
        parentCore->values_changed_recalc_redundants();
    }
    static void detach_object(
        IVP_Real_Object *attachedObject,
        IVP_Template_Real_Object *configuration) {
        if (!attachedObject || !configuration ||
            !attachedObject->get_core() ||
            !attachedObject->get_environment()) {
            return;
        }

        IVP_Cache_Object *cache = attachedObject->get_cache_object_no_lock();
        if (!cache)
            return;

        IVP_Environment *environment = attachedObject->get_environment();
        IVP_Core *oldCore = attachedObject->get_core();
        const bool wasSimulated = oldCore->movement_state < IVP_MT_NOT_SIM;
        const int oldCoreObjectCount = oldCore->objects.len();

        const IVP_U_Quat worldFromObject = cache->q_world_f_object;
        IVP_U_Point worldPosition;
        worldPosition.set(cache->m_world_f_object.get_position());
        IVP_U_Float_Point worldSpeed;
        oldCore->get_surface_speed_ws(&worldPosition, &worldSpeed);
        const IVP_U_Float_Point rotationalSpeed = oldCore->rot_speed;
        const IVP_DOUBLE movedDistance =
            attachedObject->get_shift_core_f_object()->real_length();

        void *coreMemory = BML::IVP::ABI::Invoke<void *>(
            BML::IVP::ABI::Address::OperatorNew,
            static_cast<unsigned int>(sizeof(IVP_Core)));
        if (!coreMemory)
            return;

        attachedObject->unlink_contact_points(IVP_TRUE);
        if (configuration->get_nocoll_group_ident()[0] != '\0') {
            std::strncpy(attachedObject->nocoll_group_ident,
                         configuration->get_nocoll_group_ident(),
                         sizeof(attachedObject->nocoll_group_ident));
            attachedObject->nocoll_group_ident[
                sizeof(attachedObject->nocoll_group_ident) - 1] = '\0';
        }

        oldCore->unlink_obj_from_core_and_maybe_destroy(attachedObject);
        if (oldCoreObjectCount > 1)
            oldCore->values_changed_recalc_redundants();

        attachedObject->set_movement_state(IVP_MT_NOT_SIM);
        // Ballance's retained Real Object constructor passes template +0x10
        // as Core's final piling-policy argument. Ballance omits the nearby
        // revision's following `pinned` field, yielding the 0x70 layout.
        IVP_Core *newCore = new (coreMemory) IVP_Core(
            attachedObject, &worldFromObject, &worldPosition,
            configuration->physical_unmoveable,
            configuration->enable_piling_optimization);
        attachedObject->physical_core = newCore;
        attachedObject->friction_core = newCore;
        attachedObject->original_core = newCore;

        if (configuration->material)
            attachedObject->l_default_material = configuration->material;
        if (configuration->client_data)
            attachedObject->client_data = configuration->client_data;
        attachedObject->extra_radius = configuration->extra_radius;
        attachedObject->init_object_core(environment, configuration);

        if (!wasSimulated)
            return;

        newCore->fire_event_object_frozen();
        attachedObject->ensure_in_simulation_now();
        newCore->speed.set(&worldSpeed);
        newCore->rot_speed.set(&rotationalSpeed);

        if (!newCore->is_physical_unmoveable()) {
            IVP_Hull_Manager *hull = attachedObject->get_hull_manager();
            hull->jump_add_hull(static_cast<IVP_FLOAT>(movedDistance), 0.0f);
            const IVP_DOUBLE untilNextPsi =
                environment->get_next_PSI_time() -
                environment->get_current_time();
            IVP_Event_Sim event(environment, untilNextPsi);
            IVP_Vector_of_Hulls_128 activeHullManagers;
            IVP_Calc_Next_PSI_Solver solver(newCore);
            solver.calc_next_PSI_matrix(&event, &activeHullManagers);
            newCore->tmp_null.raw = 0;
            IVP_Calc_Next_PSI_Solver::commit_all_hull_managers(
                environment, &activeHullManagers);
            attachedObject->recalc_exact_mindists_of_object();
            attachedObject->update_exact_mindist_events_of_object();
        }
    }
    static IVP_RETURN_TYPE reposition_object_Ros(
        IVP_Real_Object *parent, IVP_Real_Object *attachedObject,
        const IVP_U_Quat *referenceFromAttached,
        const IVP_U_Point *referenceFromAttachedShift,
        IVP_BOOL checkBeforeMoving) {
        if (!attachedObject || !referenceFromAttached ||
            !referenceFromAttachedShift || !attachedObject->get_core() ||
            !attachedObject->get_environment()) {
            return IVP_FAULT;
        }

        // The Ballance-era implementation exposes this argument but does not
        // branch on it. Preserve that behavior instead of inventing a sweep.
        (void) checkBeforeMoving;

        IVP_U_Matrix worldFromReference;
        if (parent) {
            IVP_Cache_Object *parentCache =
                parent->get_cache_object_no_lock();
            if (!parentCache)
                return IVP_FAULT;
            worldFromReference = parentCache->m_world_f_object;
        } else {
            worldFromReference.init();
        }

        IVP_U_Matrix referenceFromObject;
        referenceFromAttached->set_matrix(&referenceFromObject);
        referenceFromObject.vv.set(referenceFromAttachedShift);

        IVP_U_Matrix worldFromObject;
        worldFromReference.mmult4(&referenceFromObject, &worldFromObject);

        IVP_Core *core = attachedObject->get_core();
        const IVP_Time now = attachedObject->get_environment()->get_current_time();
        IVP_U_Quat worldFromCoreRotation;
        core->inline_calc_at_quaternion(now, &worldFromCoreRotation);
        IVP_U_Matrix worldFromCore;
        worldFromCoreRotation.set_matrix(&worldFromCore);
        core->inline_calc_at_position(now, &worldFromCore.vv);

        IVP_U_Matrix objectFromCore;
        worldFromObject.mimult4(&worldFromCore, &objectFromCore);
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectSetMatrixCoreFromObject,
            attachedObject, &objectFromCore);

        IVP_Hull_Manager *hull = attachedObject->get_hull_manager();
        if (!hull)
            return IVP_FAULT;
        hull->jump_add_hull(0.01f, 0.0f);
        IVP_Cache_Object_Manager::invalid_cache_object(attachedObject);
        attachedObject->recalc_exact_mindists_of_object();
        attachedObject->recalc_invalid_mindists_of_object();
        hull->check_hull_synapses();
        hull->check_for_reset();
        return IVP_OK;
    }
};

static_assert(sizeof(IVP_Object_Attach) == 0x01);

#endif // BML_IVP_OBJECT_ATTACH_H
