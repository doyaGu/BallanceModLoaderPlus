#ifndef BML_IVP_SIMULATION_UNIT_H
#define BML_IVP_SIMULATION_UNIT_H

#include "BML/IVP/Controller.h"

#include <cstddef>
#include <cstdint>
#include <new>

class IVP_Vector_of_Cores_2 : public IVP_U_Vector<IVP_Core> {
public:
    IVP_Vector_of_Cores_2()
        : IVP_U_Vector<IVP_Core>(reinterpret_cast<void **>(elem_buffer), 2) {}

private:
    IVP_Core *elem_buffer[2];
};

class IVP_Sim_Unit_Controller_Core_List {
public:
    IVP_Controller *l_controller = nullptr;
    IVP_Vector_of_Cores_2 cores_controlled_by;
};

// Internal in the neighboring tree, but reachable through the public Core and
// Environment interfaces. Ballance retains every stateful operation below
// except the small header-inline helpers, reset_time and core membership test.
class IVP_Simulation_Unit {
private:
    IVP_Movement_Type sim_unit_movement_type : 8;

public:
    IVP_BOOL union_find_needed_for_sim_unit : 2;
    IVP_BOOL sim_unit_has_fast_objects : 2;
    IVP_BOOL sim_unit_just_slowed_down : 2;

    IVP_Simulation_Unit *prev_sim_unit;
    IVP_Simulation_Unit *next_sim_unit;
    // RVA 0x11920 constructs both vectors in place; RVA 0x11890 destroys and
    // clears both. Anonymous unions expose the original field names without
    // making the host compiler repeat either lifetime around those complete
    // retail entry points.
    union {
        IVP_Vector_of_Cores_2 sim_unit_cores;
    };
    union {
        IVP_U_Vector<IVP_Sim_Unit_Controller_Core_List> controller_cores;
    };

    BML_IVP_RETAIL_ALLOCATED_OBJECT;
    IVP_Simulation_Unit() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitConstruct, this);
    }
    ~IVP_Simulation_Unit() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitDestruct, this);
    }

    void do_sim_unit_union_find() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitUnionFind, this);
    }
    IVP_BOOL sim_unit_calc_movement_state(IVP_Environment *environment) {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::SimulationUnitCalculateMovementState,
            this, environment);
    }
    void sim_unit_clear_movement_check_values() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitClearMovementChecks, this);
    }
    void sim_unit_revive_for_simulation(IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitRevive,
            this, environment);
    }
    void sim_unit_ensure_in_simulation() {
        if (get_unit_movement_type() >= IVP_MT_NOT_SIM) {
            if (sim_unit_cores.len() > 0) {
                sim_unit_revive_for_simulation(
                    sim_unit_cores.element_at(0)->environment);
            }
        } else {
            sim_unit_ensure_cores_movement();
        }
    }
    void sim_unit_ensure_cores_movement() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitEnsureCoresMovement, this);
    }
    IVP_Core *sim_unit_union_find_test() {
        return BML::IVP::ABI::InvokeThis<IVP_Core *>(
            BML::IVP::ABI::Address::SimulationUnitUnionFindTest, this);
    }
    void sim_unit_calc_redundants() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitCalculateRedundants, this);
    }
    IVP_BOOL controller_is_known_to_sim_unit(IVP_Controller *controller) {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::SimulationUnitKnowsController,
            this, controller);
    }
    void add_controlled_core_for_controller(
        IVP_Controller *controller, IVP_Core *core) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitAddControlledCore,
            this, controller, core);
    }
    void add_controller_unit_sim(IVP_Controller *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitAddController,
            this, controller);
    }
    void split_sim_unit(IVP_Core *splitFather) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitSplit,
            this, splitFather);
    }
    void perform_test_and_split() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitPerformSplitTest, this);
    }
    void clean_sim_unit() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitClean, this);
    }
    void throw_cores_into_my_sim_unit(IVP_Simulation_Unit *secondUnit) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitThrowCores,
            this, secondUnit);
    }
    void fusion_simulation_unities(IVP_Simulation_Unit *secondUnit) {
        // Retail RVA 0x11770 is called from managed-friction generation when
        // two simulated objects start sharing a contact.
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitFuse,
            this, secondUnit);
    }
    void rem_sim_unit_controller(IVP_Controller *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitRemoveController,
            this, controller);
    }
    void sim_unit_remove_core(IVP_Core *core) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitRemoveCoreAndOwnership,
            this, core);
    }
    void sim_unit_sort_controllers() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitSortControllers, this);
    }
    void add_controller_of_core(IVP_Core *core,
                                IVP_Controller *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitAddControllerForCore,
            this, core, controller);
    }
    void remove_controller_of_core(IVP_Core *core,
                                   IVP_Controller *controller) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitRemoveControllerForCore,
            this, core, controller);
    }
    IVP_Movement_Type get_unit_movement_type() {
        return sim_unit_movement_type;
    }
    void set_unit_movement_type(IVP_Movement_Type movementType) {
        sim_unit_movement_type = movementType;
    }
    void add_sim_unit_core(IVP_Core *core) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitAddCore, this, core);
    }
    void rem_sim_unit_core(IVP_Core *core) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitRemoveCore, this, core);
    }
    static void prefetch0_init_moving_core_for_psi(IVP_Core *) {
        // The neighboring macro expands to an optional platform prefetch only.
    }
    static void init_moving_core_for_psi(
        IVP_Core *core, const IVP_Time &currentTime) {
        const IVP_DOUBLE deltaTime = currentTime - core->time_of_last_psi;
        core->q_world_f_core_next_psi.set_matrix(
            &core->m_world_f_core_last_psi);
        core->m_world_f_core_last_psi.vv.add_multiple(
            &core->pos_world_f_core_last_psi,
            &core->delta_world_f_core_psis, deltaTime);
    }
    int get_pos_of_controller(IVP_Controller *controller) {
        return BML::IVP::ABI::InvokeThis<int>(
            BML::IVP::ABI::Address::SimulationUnitFindController,
            this, controller);
    }
    void sim_unit_exchange_controllers(int first, int second) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitExchangeControllers,
            this, first, second);
    }
    void reset_time(IVP_Time offset) {
        for (int index = controller_cores.len() - 1; index >= 0; --index)
            controller_cores.element_at(index)->l_controller->reset_time(offset);
        for (int index = sim_unit_cores.len() - 1; index >= 0; --index)
            sim_unit_cores.element_at(index)->reset_time(offset);
    }
    void simulate_single_sim_unit_psi(
        IVP_Event_Sim *event, IVP_U_Vector<IVP_Core> *touchedCores) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitSimulatePsi,
            this, event, touchedCores);
    }
    IVP_BOOL sim_unit_core_exists(IVP_Core *core) {
        for (int index = sim_unit_cores.len() - 1; index >= 0; --index) {
            if (sim_unit_cores.element_at(index) == core)
                return IVP_TRUE;
        }
        return IVP_FALSE;
    }
};

class IVP_Sim_Units_Manager {
public:
    static constexpr int SlotCount = 100;

    IVP_Environment *l_environment;
    IVP_Time nb;
    IVP_Time bt;
    IVP_Simulation_Unit *sim_units_slots[SlotCount];
    IVP_Simulation_Unit *still_slot;

    explicit IVP_Sim_Units_Manager(IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitsManagerConstruct,
            this, environment);
    }
    void add_sim_unit_to_manager(IVP_Simulation_Unit *unit) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitsManagerAdd, this, unit);
    }
    void rem_sim_unit_from_manager(IVP_Simulation_Unit *unit) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitsManagerRemove, this, unit);
    }
    void add_unit_to_slot(IVP_Simulation_Unit *unit,
                          IVP_Simulation_Unit **slot) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitsManagerAddToSlot,
            this, unit, slot);
    }
    void rem_unit_from_slot(IVP_Simulation_Unit *unit,
                            IVP_Simulation_Unit **slot) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitsManagerRemoveFromSlot,
            this, unit, slot);
    }
    void simulate_sim_units_psi(
        IVP_Environment *environment,
        IVP_U_Vector<IVP_Core> *touchedCores) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SimulationUnitsManagerSimulatePsi,
            this, environment, touchedCores);
    }
    void reset_time(IVP_Time offset) {
        for (IVP_Simulation_Unit *unit = sim_units_slots[0]; unit;) {
            IVP_Simulation_Unit *next = unit->next_sim_unit;
            unit->reset_time(offset);
            unit = next;
        }
    }
};

inline void IVP_Real_Object::change_unmovable_flag(IVP_BOOL unmovable) {
    IVP_Core *core = get_core();
    if (!core || core->is_physical_unmoveable() == unmovable)
        return;

    for (int index = core->objects.len() - 1; index >= 0; --index) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectUnlinkContactPoints,
            core->objects.element_at(index), IVP_FALSE);
    }

    if (core->is_physical_unmoveable()) {
        IVP_Friction_Hash *hash = core->core_friction_info.l_friction_info_hash;
        if (hash) {
            reinterpret_cast<IVP_VHash_Store *>(hash)->~IVP_VHash_Store();
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::OperatorDelete, hash);
            core->core_friction_info.l_friction_info_hash = nullptr;
        }
    } else {
        core->core_friction_info.moveable_core_friction_info = nullptr;
    }

    if (core->movement_state < IVP_MT_NOT_SIM)
        core->freeze_simulation_core();

    core->flags =
        (core->flags & ~(0x3u << 2u)) |
        ((static_cast<std::uint32_t>(unmovable) & 0x3u) << 2u);

    if (unmovable != IVP_TRUE || !core->sim_unit_of_core)
        return;

    core->sim_unit_of_core->sim_unit_remove_core(core);
    IVP_Simulation_Unit *replacement = new IVP_Simulation_Unit();
    core->sim_unit_of_core = replacement;
    replacement->add_sim_unit_core(core);
    replacement->set_unit_movement_type(IVP_MT_NOT_SIM);

    for (int index = core->controllers_of_core.len() - 1;
         index >= 0; --index) {
        core->controllers_of_core.remove_at(index);
    }

    core->environment->get_sim_units_manager()->add_sim_unit_to_manager(
        replacement);
    core->add_core_controller(
        core->environment->standard_gravity_controller);
}

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Vector_of_Cores_2) == 0x10);
static_assert(sizeof(IVP_Sim_Unit_Controller_Core_List) == 0x14);
static_assert(sizeof(IVP_Simulation_Unit) == 0x24);
static_assert(offsetof(IVP_Simulation_Unit, prev_sim_unit) == 0x04);
static_assert(offsetof(IVP_Simulation_Unit, next_sim_unit) == 0x08);
static_assert(offsetof(IVP_Simulation_Unit, sim_unit_cores) == 0x0C);
static_assert(offsetof(IVP_Simulation_Unit, controller_cores) == 0x1C);
static_assert(sizeof(IVP_Sim_Units_Manager) == 0x1B0);
static_assert(offsetof(IVP_Sim_Units_Manager, nb) == 0x08);
static_assert(offsetof(IVP_Sim_Units_Manager, sim_units_slots) == 0x18);
static_assert(offsetof(IVP_Sim_Units_Manager, still_slot) == 0x1A8);
#endif

#endif // BML_IVP_SIMULATION_UNIT_H
