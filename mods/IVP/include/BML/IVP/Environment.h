#ifndef BML_IVP_ENVIRONMENT_H
#define BML_IVP_ENVIRONMENT_H

#include "BML/IVP/Anomaly.h"
#include "BML/IVP/Listeners.h"
#include "BML/IVP/Memory.h"
#include "BML/IVP/Mindist.h"
#include "BML/IVP/Object.h"
#include "BML/IVP/Performance.h"
#include "BML/IVP/Radar.h"
#include "BML/IVP/Set.h"
#include "BML/IVP/TimeManager.h"
#include "BML/IVP/Universe.h"

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <new>

class IVP_Actuator_Spring;
class IVP_Actuator_Suspension;
class IVP_Actuator_Check_Dist;
class IVP_Actuator_Force;
class IVP_Actuator_Stabilizer;
class IVP_Actuator_Torque;
class IVP_Actuator_Rot_Mot;
class IVP_Anomaly_Limits;
class IVP_Anomaly_Manager;
class IVP_Cache_Object_Manager;
class IVP_Collision_Delegator_Root;
class IVP_Collision_Filter;
class IVP_Controller_Manager;
class IVP_Controller_Motion;
class IVP_Constraint;
class IVP_Debug_Manager;
class IVP_Environment_Manager;
class IVP_Material_Manager;
class IVP_Mindist_Manager;
class IVP_OV_Tree_Manager;
class IVP_PerformanceCounter;
class IVP_Range_Manager;
class IVP_Sim_Units_Manager;
class IVP_Standard_Gravity_Controller;
class IVP_SurfaceManager;
class IVP_Template_Constraint;
class IVP_Template_Controller_Motion;
struct IVP_Template_Ball;
class IVP_Template_Real_Object;
struct IVP_Template_Spring;
struct IVP_Template_Suspension;
struct IVP_Template_Force;
struct IVP_Template_Torque;
struct IVP_Template_Rot_Mot;
class IVP_Time_Manager;
class IVP_Template_Check_Dist;
class IVP_Template_Stabilizer;
class IVP_U_Active_Value_Manager;
namespace BML::IVP::Detail {

// Transaction prologues/epilogues are source-inline throughout physics_RT.
// Route the environment helpers through the reconstructed public type so the
// layout knowledge remains centralized in Memory.h.
inline void StartSimulationMemoryTransaction(IVP_U_Memory *memory) {
    if (!memory)
        return;
    memory->start_memory_transaction();
}

inline void EndSimulationMemoryTransaction(IVP_U_Memory *memory) {
    if (!memory)
        return;
    memory->end_memory_transaction();
}

// Runtime layout of the per-object callback value stored in Ballance's
// IVP_Object_Callback_Table_Hash. The hash type itself remains opaque: the
// retail find/remove functions use its original vtable and comparison rule.
struct ObjectListenerTable {
    IVP_Real_Object *real_object = nullptr;
    IVP_U_Vector<IVP_Listener_Object> listeners;
};

inline int ObjectListenerHashIndex(IVP_Real_Object *object) {
    const std::uint32_t pointerValue = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(object));
    return IVP_VHash::hash_index(
        reinterpret_cast<const char *>(&pointerValue),
        static_cast<int>(sizeof(pointerValue)));
}

// The reset methods of IVP_Simulation_Unit and IVP_Sim_Units_Manager were
// inlined out of Ballance. Their traversed fields are independently visible
// in retained manager/simulation code: first managed unit at +0x18, next unit
// at +0x08, core vector at +0x0c, and controller-entry vector at +0x1c.
inline void ResetSimulationUnitTime(void *simulationUnit, IVP_Time offset) {
    auto *controllerEntries = reinterpret_cast<IVP_U_Vector_Base *>(
        reinterpret_cast<std::byte *>(simulationUnit) + 0x1Cu);
    for (int index = controllerEntries->n_elems - 1; index >= 0; --index) {
        void *entry = controllerEntries->elems[index];
        void *controller = *reinterpret_cast<void **>(entry);
        void **vtable = *reinterpret_cast<void ***>(controller);
        using ResetTime = void (__thiscall *)(void *, IVP_Time);
        auto resetTime = reinterpret_cast<ResetTime>(vtable[3]);
        resetTime(controller, offset);
    }

    auto *cores = reinterpret_cast<IVP_U_Vector_Base *>(
        reinterpret_cast<std::byte *>(simulationUnit) + 0x0Cu);
    for (int index = cores->n_elems - 1; index >= 0; --index)
        static_cast<IVP_Core *>(cores->elems[index])->reset_time(offset);
}

inline void ResetSimulationUnitsTime(
    IVP_Sim_Units_Manager *manager, IVP_Time offset) {
    void *unit = *reinterpret_cast<void **>(
        reinterpret_cast<std::byte *>(manager) + 0x18u);
    while (unit) {
        void *next = *reinterpret_cast<void **>(
            reinterpret_cast<std::byte *>(unit) + 0x08u);
        ResetSimulationUnitTime(unit, offset);
        unit = next;
    }
}

} // namespace BML::IVP::Detail

// Ballance strips the small debug-vector node methods, but its Environment
// keeps the owning list field. The neighboring node layout uses no virtual
// state and is fully supported by the retained retail allocator boundary.
class IVP_Draw_Vector_Debug {
public:
    IVP_Draw_Vector_Debug() = default;
    ~IVP_Draw_Vector_Debug() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::DrawVectorDebugDestruct, this);
    }

    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_Draw_Vector_Debug *next = nullptr;
    IVP_U_Point first_point{};
    IVP_U_Point direction_vec{};
    int color = 0;
    char *debug_text = nullptr;
};

// This manager is internal in the nearby source tree, but IVP_Environment's
// public API exposes its pointer. Its 0x18-byte Ballance layout is needed for
// the four public per-object listener registration methods that the compiler
// inlined. No foreign IVP implementation is instantiated here.
class IVP_Cluster_Manager {
public:
    void add_listener_object(
        IVP_Real_Object *object, IVP_Listener_Object *listener) {
        using Table = BML::IVP::Detail::ObjectListenerTable;
        Table *table = BML::IVP::ABI::InvokeThis<Table *>(
            BML::IVP::ABI::Address::ObjectCallbackTableFind,
            object_callback_hash, object);
        if (table) {
            table->listeners.add(listener);
            return;
        }

        void *storage = BML::IVP::ABI::Invoke<void *>(
            BML::IVP::ABI::Address::OperatorNew,
            static_cast<unsigned int>(sizeof(Table)));
        table = storage ? ::new (storage) Table() : nullptr;
        if (!table)
            return;

        table->real_object = object;
        table->listeners.add(listener);
        object_callback_hash->add_elem(
            table, BML::IVP::Detail::ObjectListenerHashIndex(object));
        object->flags.raw |= 0x00001000u;
    }

    void remove_listener_object(
        IVP_Real_Object *object, IVP_Listener_Object *listener) {
        using Table = BML::IVP::Detail::ObjectListenerTable;
        Table *table = BML::IVP::ABI::InvokeThis<Table *>(
            BML::IVP::ABI::Address::ObjectCallbackTableFind,
            object_callback_hash, object);
        if (!table)
            return;

        table->listeners.remove(listener);
        if (table->listeners.len() != 0)
            return;

        BML::IVP::ABI::InvokeThis<Table *>(
            BML::IVP::ABI::Address::ObjectCallbackTableRemove,
            object_callback_hash, object);
        table->~ObjectListenerTable();
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::OperatorDelete, table);
        object->flags.raw &= ~0x00001000u;
    }

private:
    IVP_Cluster *root_cluster;
    IVP_Environment *environment;
    IVP_VHash *object_callback_hash;
    IVP_VHash *collision_callback_hash;
    IVP_Real_Object *object_to_be_checked;
    std::int32_t number_of_real_objects;
};

class IVP_Vector_of_Hulls_128
    : public IVP_U_Vector<IVP_Hull_Manager_Base> {
public:
    IVP_Vector_of_Hulls_128()
        : IVP_U_Vector<IVP_Hull_Manager_Base>(
              reinterpret_cast<void **>(elem_buffer), 128) {}

private:
    IVP_Hull_Manager_Base *elem_buffer[128];
};

class IVP_Vector_of_Cores_128 : public IVP_U_Vector<IVP_Core> {
public:
    IVP_Vector_of_Cores_128()
        : IVP_U_Vector<IVP_Core>(elem_buffer, 128) {}

private:
    void *elem_buffer[128];
};

struct IVP_Statistic_Manager {
    IVP_Environment *l_environment;
    IVP_Time last_statistic_output;
    IVP_FLOAT max_rescue_speed;
    IVP_FLOAT max_speed_gain;
    std::int32_t impact_sys_num;
    std::int32_t impact_counter;
    std::int32_t impact_sum_sys;
    std::int32_t impact_hard_rescue_counter;
    std::int32_t impact_rescue_after_counter;
    std::int32_t impact_delayed_counter;
    std::int32_t impact_coll_checks;
    std::int32_t impact_unmov;
    IVP_DOUBLE sum_energy_destr;
    std::int32_t sum_of_mindists;
    std::int32_t mindists_generated;
    std::int32_t mindists_deleted;
    std::int32_t range_intra_exceeded;
    std::int32_t range_world_exceeded;
    std::int32_t processed_fmindists;
    std::int32_t global_fmd_counter;

    IVP_Statistic_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::StatisticManagerConstruct, this);
    }

    void clear_statistic() {
        max_rescue_speed = 0.0f;
        max_speed_gain = 0.0f;
        impact_sys_num = 0;
        impact_counter = 0;
        impact_sum_sys = 0;
        impact_hard_rescue_counter = 0;
        impact_rescue_after_counter = 0;
        impact_delayed_counter = 0;
        impact_coll_checks = 0;
        mindists_deleted = 0;
        mindists_generated = 0;
        processed_fmindists = 0;
        range_intra_exceeded = 0;
        range_world_exceeded = 0;
        impact_unmov = 0;
    }

    void output_statistic() {
        std::printf(
            "nr_impacts %d  hard_resc %d  resc_after %d  delayed %d  "
            "sys_strt %d sys_imps %d unmov %d  coll_check %d  mindists "
            "%d  gen_mindists %d ov %d\n",
            impact_counter, impact_hard_rescue_counter,
            impact_rescue_after_counter, impact_delayed_counter,
            impact_sys_num, impact_sum_sys, impact_unmov, impact_coll_checks,
            sum_of_mindists, mindists_generated, range_world_exceeded);
        clear_statistic();
    }
};

struct IVP_Freeze_Manager {
    IVP_FLOAT freeze_check_dtime;

    IVP_Freeze_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::FreezeManagerConstruct, this);
    }

    void init_freeze_manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::FreezeManagerInitialize, this);
    }
};

class IVP_Environment {
private:
    IVP_Environment() = delete;

public:
    IVP_Standard_Gravity_Controller *standard_gravity_controller;
    IVP_Time_Manager *time_manager;
    IVP_Sim_Units_Manager *sim_units_manager;
    IVP_Cluster_Manager *cluster_manager;
    IVP_Mindist_Manager *mindist_manager;
    IVP_OV_Tree_Manager *ov_tree_manager;
    IVP_Collision_Filter *collision_filter;
    IVP_Range_Manager *range_manager;
    IVP_Anomaly_Manager *anomaly_manager;
    IVP_Anomaly_Limits *anomaly_limits;
    IVP_PerformanceCounter *performancecounter;
    IVP_Universe_Manager *universe_manager;
    IVP_Real_Object *static_object;
    // The retail compiler aligns IVP_Statistic_Manager's IVP_Time to eight
    // bytes. IDA's imported 0x58 definition omitted this padding; the actual
    // constructor clears 0x60 bytes starting at environment + 0x38.
    std::uint32_t reserved_34;
    IVP_Statistic_Manager statistic_manager;
    IVP_Freeze_Manager freeze_manager;
    IVP_BetterStatisticsmanager *better_statisticsmanager;
    IVP_Controller_Manager *controller_manager;
    IVP_Cache_Object_Manager *cache_object_manager;
    IVP_U_Active_Value_Manager *l_active_value_manager;
    IVP_Material_Manager *l_material_manager;
    IVP_U_Memory *short_term_mem;
    IVP_U_Memory *sim_unit_mem;
    IVP_DOUBLE time_since_last_blocking;
    IVP_DOUBLE delta_PSI_time;
    IVP_DOUBLE inv_delta_PSI_time;
    IVP_U_Point gravity;
    IVP_FLOAT gravity_scalar;
    IVP_U_Vector<IVP_Listener_Collision> collision_listeners;
    IVP_U_Vector<IVP_Listener_PSI> psi_listeners;
    IVP_U_Vector<IVP_Core> core_revive_list;
    char *auth_costumer_name;
    std::uint32_t auth_costumer_code;
    std::int32_t pw_count;
    IVP_Environment_Manager *environment_manager;
    IVP_Time current_time;
    IVP_Time time_of_next_psi;
    IVP_Time time_of_last_psi;
    IVP_Time_CODE current_time_code;
    IVP_Time_CODE mindist_event_timestamp_reference;
    std::int16_t next_movement_check;
    std::uint16_t reserved_142;
    IVP_ENV_STATE state;
    IVP_DOUBLE integrated_energy_damp;
    IVP_U_Vector<IVP_Listener_Object> global_object_listeners;
    IVP_U_Vector<IVP_Collision_Delegator_Root> collision_delegator_roots;
    IVP_Debug_Manager *debug_information;
    IVP_BOOL delete_debug_information;
    IVP_Draw_Vector_Debug *draw_vectors;
    void *client_data;
    std::int32_t environment_magic_number;
    std::uint32_t reserved_174;

    IVP_Time get_current_time() { return current_time; }
    IVP_Time get_current_time() const { return current_time; }
    void set_current_time(IVP_Time time) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentSetCurrentTime, this, time);
    }
    IVP_Time get_next_PSI_time() { return time_of_next_psi; }
    IVP_Time get_next_PSI_time() const { return time_of_next_psi; }
    IVP_Time get_old_time_of_last_PSI() { return time_of_last_psi; }
    IVP_Time get_old_time_of_last_PSI() const { return time_of_last_psi; }
    IVP_FLOAT get_delta_PSI_time() {
        return static_cast<IVP_FLOAT>(delta_PSI_time);
    }
    IVP_FLOAT get_delta_PSI_time() const {
        return static_cast<IVP_FLOAT>(delta_PSI_time);
    }
    IVP_FLOAT get_inv_delta_PSI_time() {
        return static_cast<IVP_FLOAT>(inv_delta_PSI_time);
    }
    IVP_FLOAT get_inv_delta_PSI_time() const {
        return static_cast<IVP_FLOAT>(inv_delta_PSI_time);
    }
    IVP_Time_CODE get_current_time_code() { return current_time_code; }
    IVP_Time_CODE get_current_time_code() const { return current_time_code; }
    IVP_ENV_STATE get_env_state() { return state; }
    IVP_ENV_STATE get_env_state() const { return state; }
    IVP_DOUBLE get_integrated_energy_damp() const { return integrated_energy_damp; }
    const IVP_U_Point *get_gravity() { return &gravity; }
    const IVP_U_Point *get_gravity() const { return &gravity; }
    IVP_Real_Object *get_static_object() const { return static_object; }
    IVP_Time_Manager *get_time_manager() const { return time_manager; }
    IVP_Controller_Manager *get_controller_manager() const {
        return controller_manager;
    }
    IVP_Mindist_Manager *get_mindist_manager() const { return mindist_manager; }
    IVP_Sim_Units_Manager *get_sim_units_manager() const {
        return sim_units_manager;
    }
    IVP_Cache_Object_Manager *get_cache_object_manager() const {
        return cache_object_manager;
    }
    class IVP_Standard_Gravity_Controller *get_gravity_controller() {
        return standard_gravity_controller;
    }
    IVP_Standard_Gravity_Controller *get_gravity_controller() const {
        return standard_gravity_controller;
    }
    IVP_OV_Tree_Manager *get_ov_tree_manager() const { return ov_tree_manager; }
    IVP_Cluster_Manager *get_cluster_manager() const { return cluster_manager; }
    IVP_Debug_Manager *get_debug_manager() const { return debug_information; }
    IVP_PerformanceCounter *get_performancecounter() {
        return performancecounter;
    }
    IVP_PerformanceCounter *get_performancecounter() const {
        return performancecounter;
    }
    IVP_BetterStatisticsmanager *get_betterstatisticsmanager() {
        return better_statisticsmanager;
    }
    IVP_BetterStatisticsmanager *get_betterstatisticsmanager() const {
        return better_statisticsmanager;
    }
    IVP_U_Active_Value_Manager *get_active_value_manager() const {
        return l_active_value_manager;
    }
    IVP_Statistic_Manager *get_statistic_manager() { return &statistic_manager; }
    const IVP_Statistic_Manager *get_statistic_manager() const {
        return &statistic_manager;
    }
    IVP_Freeze_Manager *get_freeze_manager() { return &freeze_manager; }
    const IVP_Freeze_Manager *get_freeze_manager() const {
        return &freeze_manager;
    }
    IVP_U_Memory *get_memory_manager() const { return short_term_mem; }
    IVP_U_Memory *get_short_term_mem() { return short_term_mem; }
    IVP_U_Memory *get_short_term_mem() const { return short_term_mem; }
    IVP_U_Memory *get_sim_unit_mem() { return sim_unit_mem; }
    IVP_U_Memory *get_sim_unit_mem() const { return sim_unit_mem; }
    IVP_Material_Manager *get_material_manager() const { return l_material_manager; }
    IVP_Collision_Filter *get_collision_filter() const { return collision_filter; }
    IVP_Range_Manager *get_range_manager() const { return range_manager; }
    IVP_Universe_Manager *get_universe_manager() const { return universe_manager; }
    IVP_Anomaly_Manager *get_anomaly_manager() const { return anomaly_manager; }
    IVP_Anomaly_Limits *get_anomaly_limits() const { return anomaly_limits; }

    // Ballance retains the complete list teardown at RVA 0x13940. Adding a
    // node is link-stripped, so only that half is reconstructed locally over
    // the retail-confirmed Environment and node layouts.
    void set_event_function() = delete;
    void add_draw_vector(
        const IVP_U_Point *startWorld,
        const IVP_U_Float_Point *direction,
        const char *debugText, int vectorColor) {
        auto *drawVector = new IVP_Draw_Vector_Debug();
        drawVector->first_point = *startWorld;
        drawVector->direction_vec.set(direction);
        if (debugText) {
            const std::size_t length = std::strlen(debugText) + 1;
            drawVector->debug_text = static_cast<char *>(
                BML::IVP::ABI::Invoke<void *>(
                    BML::IVP::ABI::Address::Allocate,
                    static_cast<unsigned int>(length)));
            std::memcpy(drawVector->debug_text, debugText, length);
        }
        drawVector->color = vectorColor;
        drawVector->next = draw_vectors;
        draw_vectors = drawVector;
    }
    void delete_draw_vector_debug() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentDeleteDrawVectors, this);
    }

    // set_event_function has no defined neighboring behavior. merge_objects
    // depends on the retired merged-core representation, so these remain the
    // narrow unsupported cases rather than receiving guessed implementations.
    void merge_objects(
        IVP_U_Vector<IVP_Real_Object> *objectsToMerge) = delete;

    ~IVP_Environment() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentDestruct, this);
    }
    BML_IVP_RETAIL_DEALLOCATED_OBJECT;

    // Environments are allocated by the retail manager. The class-specific
    // delete keeps the deallocation in the same runtime as physics_RT.
    void destroy() { delete this; }

    void set_delta_PSI_time(IVP_DOUBLE seconds) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentSetFixedStep, this, seconds);
    }
    static void set_global_collision_tolerance(
        IVP_DOUBLE tolerance = 0.01,
        IVP_DOUBLE gravityLength = 0.01) {
        (void) gravityLength; // Not consumed by this Ballance revision.
        using Setter = void (__thiscall *)(void *, IVP_DOUBLE);
        Setter setter = BML::IVP::ABI::Resolve<Setter>(
            BML::IVP::ABI::Address::MindistSettingsSetCollisionTolerance);
        if (!setter)
            return;
        const std::uintptr_t imageBase =
            reinterpret_cast<std::uintptr_t>(setter) -
            static_cast<std::uint32_t>(
                BML::IVP::ABI::Address::MindistSettingsSetCollisionTolerance);
        setter(reinterpret_cast<void *>(
                   imageBase + BML::IVP::ABI::MindistSettingsRva),
               tolerance);
    }
    static IVP_FLOAT get_global_collision_tolerance() {
        using Setter = void (__thiscall *)(void *, IVP_DOUBLE);
        Setter setter = BML::IVP::ABI::Resolve<Setter>(
            BML::IVP::ABI::Address::MindistSettingsSetCollisionTolerance);
        if (!setter)
            return 0.0f;
        const std::uintptr_t imageBase =
            reinterpret_cast<std::uintptr_t>(setter) -
            static_cast<std::uint32_t>(
                BML::IVP::ABI::Address::MindistSettingsSetCollisionTolerance);
        const auto *settings = reinterpret_cast<const IVP_FLOAT *>(
            imageBase + BML::IVP::ABI::MindistSettingsRva);
        return settings[1]; // min_coll_dists at IVP_Mindist_Settings + 0x04.
    }
    void set_gravity(IVP_U_Point *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentSetGravity, this, value);
    }
    IVP_Actuator_Spring *create_spring(IVP_Template_Spring *definition) {
        return BML::IVP::ABI::InvokeThis<IVP_Actuator_Spring *>(
            BML::IVP::ABI::Address::EnvironmentCreateSpring,
            this, definition);
    }
    IVP_Actuator_Suspension *create_suspension(
        IVP_Template_Suspension *definition);
    IVP_Actuator_Check_Dist *create_check_dist(
        IVP_Template_Check_Dist *definition);
    IVP_Actuator_Stabilizer *create_stabilizer(
        IVP_Template_Stabilizer *definition);
    IVP_Actuator_Force *create_force(IVP_Template_Force *definition);
    IVP_Actuator_Torque *create_torque(IVP_Template_Torque *definition);
    IVP_Actuator_Rot_Mot *create_rotmot(IVP_Template_Rot_Mot *definition);
    IVP_Controller_Motion *create_controller_motion(
        IVP_Real_Object *object,
        const class IVP_Template_Controller_Motion *definition);
    IVP_Constraint *create_constraint(
        const IVP_Template_Constraint *definition) {
        return BML::IVP::ABI::InvokeThis<IVP_Constraint *>(
            BML::IVP::ABI::Address::EnvironmentCreateConstraint,
            this, definition);
    }
    IVP_Cluster *get_root_cluster() {
        return BML::IVP::ABI::InvokeThis<IVP_Cluster *>(
            BML::IVP::ABI::Address::EnvironmentGetRootCluster, this);
    }
    void simulate_dtime(IVP_DOUBLE seconds) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentSimulateDelta, this, seconds);
    }
    void simulate_until(IVP_Time untilTime) {
        if (better_statisticsmanager)
            better_statisticsmanager->set_simulation_time(
                untilTime.get_time());
        if (time_manager)
            time_manager->event_loop(this, untilTime);
    }
    void simulate_variable_time_step(IVP_FLOAT deltaTime) {
        if (better_statisticsmanager)
            better_statisticsmanager->set_simulation_time(
                get_current_time().get_time() + deltaTime);
        if (time_manager)
            time_manager->simulate_variable_time_step(this, deltaTime);
    }
    void simulate_time_step(IVP_FLOAT subPsiTime) {
        IVP_Time untilTime = get_old_time_of_last_PSI();
        untilTime += get_delta_PSI_time() * subPsiTime;
        if (time_manager)
            time_manager->event_loop(this, untilTime);
    }
    void reset_time() {
        const IVP_Time offset = get_old_time_of_last_PSI();
        time_manager->reset_time(offset);
        BML::IVP::Detail::ResetSimulationUnitsTime(
            sim_units_manager, offset);
        ++current_time_code;
        current_time = IVP_Time(current_time.get_time() - offset.get_time());
        time_of_last_psi = IVP_Time(0.0);
        time_of_next_psi = IVP_Time(get_delta_PSI_time());
    }
    IVP_Polygon *create_polygon(IVP_SurfaceManager *surface,
                                const IVP_Template_Real_Object *objectTemplate,
                                const IVP_U_Quat *rotation,
                                const IVP_U_Point *position) {
        return BML::IVP::ABI::InvokeThis<IVP_Polygon *>(
            BML::IVP::ABI::Address::EnvironmentCreatePolygon,
            this, surface, objectTemplate, rotation, position);
    }
    IVP_Ball *create_ball(const IVP_Template_Ball *ballTemplate,
                          const IVP_Template_Real_Object *objectTemplate,
                          const IVP_U_Quat *rotation,
                          const IVP_U_Point *position) {
        return BML::IVP::ABI::InvokeThis<IVP_Ball *>(
            BML::IVP::ABI::Address::EnvironmentCreateBall,
            this, ballTemplate, objectTemplate, rotation, position);
    }
    void add_listener_object_global(IVP_Listener_Object *listener) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentAddObjectListener,
            this, listener);
    }
    void add_listener_collision_global(IVP_Listener_Collision *listener) {
        // Retail appends rather than installing uniquely: registering the same
        // listener twice produces two callbacks and requires two removals.
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentAddCollisionListener,
            this, listener);
    }
    void install_listener_object_global(IVP_Listener_Object *listener) {
        global_object_listeners.install(listener);
    }
    void remove_listener_object_global(IVP_Listener_Object *listener) {
        global_object_listeners.remove(listener);
    }
    void remove_listener_collision_global(IVP_Listener_Collision *listener) {
        // IVP_U_Vector::remove assumes the element exists. Call this exactly
        // once for each successful registration still owned by the caller.
        collision_listeners.remove(listener);
    }
    void add_listener_object_private(
        IVP_Real_Object *object, IVP_Listener_Object *listener) {
        cluster_manager->add_listener_object(object, listener);
    }
    void remove_listener_object_private(
        IVP_Real_Object *object, IVP_Listener_Object *listener) {
        cluster_manager->remove_listener_object(object, listener);
    }
    void add_listener_collision_private(
        IVP_Real_Object *object, IVP_Listener_Collision *listener) {
        object->add_listener_collision(listener);
    }
    void remove_listener_collision_private(
        IVP_Real_Object *object, IVP_Listener_Collision *listener) {
        object->remove_listener_collision(listener);
    }
    void add_listener_PSI(IVP_Listener_PSI *listener) {
        psi_listeners.add(listener);
    }
    void remove_listener_PSI(IVP_Listener_PSI *listener) {
        psi_listeners.remove(listener);
    }
    // The nearby source revision stores a fourth vector here. Ballance retail
    // instead stores auth_costumer_name at +0x10C and environment_manager at
    // +0x118, so these APIs cannot be represented without corrupting the
    // object passed across the DLL boundary.
    void add_listener_constraint_global(IVP_Listener_Constraint *) = delete;
    void remove_listener_constraint_global(IVP_Listener_Constraint *) = delete;

    void fire_event_object_created(IVP_Event_Object *event) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::EnvironmentFireObjectCreated, this,
            [this, event] {
                for (int index = global_object_listeners.len() - 1;
                     index >= 0; --index) {
                    global_object_listeners.element_at(index)
                        ->event_object_created(event);
                }
            }, event);
    }
    void fire_event_object_deleted(IVP_Event_Object *event) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::EnvironmentFireObjectDeleted, this,
            [this, event] {
                for (int index = global_object_listeners.len() - 1;
                     index >= 0; --index) {
                    global_object_listeners.element_at(index)
                        ->event_object_deleted(event);
                }
            }, event);
    }
    void fire_event_object_revived(IVP_Event_Object *event) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::EnvironmentFireObjectRevived, this,
            [this, event] {
                for (int index = global_object_listeners.len() - 1;
                     index >= 0; --index) {
                    global_object_listeners.element_at(index)
                        ->event_object_revived(event);
                }
            }, event);
    }
    void fire_event_object_frozen(IVP_Event_Object *event) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::EnvironmentFireObjectFrozen, this,
            [this, event] {
                for (int index = global_object_listeners.len() - 1;
                     index >= 0; --index) {
                    global_object_listeners.element_at(index)
                        ->event_object_frozen(event);
                }
            }, event);
    }
    void fire_event_constraint_broken(IVP_Constraint *) = delete;
    void fire_object_is_removed_from_collision_detection(
        IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentNotifyCollisionRemoval,
            this, object);
    }
    void force_psi_on_next_simulation() {
        IVP_Time_Event_PSI *event = time_manager->psi_event;
        const IVP_FLOAT relativeTime = static_cast<IVP_FLOAT>(
            current_time.get_seconds() -
            time_manager->base_time.get_seconds());
        time_manager->min_hash->remove_minlist_elem(event->index);
        event->index = time_manager->min_hash->add(event, relativeTime);
    }
};

inline void IVP_Core::clip_velocity(
    IVP_U_Float_Point *velocity,
    IVP_U_Float_Point *angularVelocity) {
    IVP_Anomaly_Limits *limits = environment->get_anomaly_limits();

    if (spin_clipping) {
        for (int axis = 0; axis < 3; ++axis) {
            angularVelocity->k[axis] = std::max(
                -spin_clipping->k[axis],
                std::min(angularVelocity->k[axis],
                         spin_clipping->k[axis]));
        }
    } else if (!car_wheel || max_surface_deviation != 0.0f) {
        const IVP_DOUBLE maximum =
            limits->get_max_angular_velocity_per_psi() *
            environment->get_inv_delta_PSI_time();
        if (angularVelocity->quad_length() > maximum * maximum) {
            environment->get_anomaly_manager()->max_angular_velocity_exceeded(
                limits, this, angularVelocity);
        }
    }

    const IVP_DOUBLE maximum = limits->get_max_velocity();
    if (velocity->quad_length() > maximum * maximum) {
        environment->get_anomaly_manager()->max_velocity_exceeded(
            limits, this, velocity);
    }
}

inline void IVP_Core::apply_velocity_limit() {
    IVP_Anomaly_Limits *limits = environment->get_anomaly_limits();
    const IVP_DOUBLE maximumSpeed = limits->get_max_velocity();
    if (maximumSpeed > 0.0) {
        const IVP_DOUBLE speedLength = speed.real_length();
        if (speedLength > maximumSpeed)
            speed.mult(maximumSpeed / speedLength);

        const IVP_DOUBLE changeLength = speed_change.real_length();
        if (changeLength > maximumSpeed)
            speed_change.mult(maximumSpeed / changeLength);
    }

    const IVP_DOUBLE maximumAngularSpeed =
        limits->get_max_angular_velocity_per_psi() *
        environment->get_inv_delta_PSI_time();
    if (limits->get_max_angular_velocity_per_psi() > 0.0f) {
        const IVP_DOUBLE angularLength = rot_speed.real_length();
        if (angularLength > maximumAngularSpeed)
            rot_speed.mult(maximumAngularSpeed / angularLength);

        // This unusual trigger is intentional: it is what the neighboring
        // Ballance-era implementation does. There is no retained retail body
        // or callsite that would justify silently changing its behavior.
        const IVP_DOUBLE angularChangeTrigger = speed_change.real_length();
        if (angularChangeTrigger > maximumAngularSpeed) {
            rot_speed_change.mult(
                maximumAngularSpeed / angularChangeTrigger);
        }
    }
}

inline void IVP_Real_Object::recheck_collision_filter() {
    if (ov_element)
        physical_core->environment->get_mindist_manager()
            ->recheck_ov_element(this);
}

inline void IVP_Real_Object::force_grow_friction_system() {
    const IVP_Movement_Type objectMovement = get_movement_state();
    const IVP_Movement_Type coreMovement =
        static_cast<IVP_Movement_Type>(physical_core->movement_state);

    set_movement_state(IVP_MT_GET_MINDIST);
    physical_core->movement_state = IVP_MT_GET_MINDIST;
    physical_core->environment->get_mindist_manager()
        ->recheck_ov_element(this);
    set_movement_state(objectMovement);
    physical_core->movement_state =
        static_cast<std::uint8_t>(coreMovement);

    physical_core->grow_friction_system();
}

namespace BML::IVP::Detail {

// This layer sees only the IVP_Contact_Point forward declaration; Friction.h
// owns its verified 0x78 definition and the typed IVP_Friction_System view.
// These cycle-breaking helpers use the same Ballance offsets: the two object
// pointers are loaded at +0x10/+0x24, l_friction_system at +0x70, and the
// friction-distance count is tested as a 16-bit value at system +0x3e.
inline IVP_Real_Object *ContactPointObject(
    IVP_Contact_Point *contactPoint, std::size_t index) {
    const std::size_t offset = index == 0 ? 0x10u : 0x24u;
    return *reinterpret_cast<IVP_Real_Object **>(
        reinterpret_cast<std::byte *>(contactPoint) + offset);
}

inline IVP_Friction_System *ContactPointFrictionSystem(
    IVP_Contact_Point *contactPoint) {
    return *reinterpret_cast<IVP_Friction_System **>(
        reinterpret_cast<std::byte *>(contactPoint) + 0x70u);
}

inline void *ContactPointTemporaryInfo(IVP_Contact_Point *contactPoint) {
    return *reinterpret_cast<void **>(
        reinterpret_cast<std::byte *>(contactPoint) + 0x40u);
}

inline void SetFrictionSystemUnionFindNecessary(
    IVP_Friction_System *frictionSystem) {
    // The neighboring declaration places a short before this enum bitfield,
    // but MSVC starts the IVP_BOOL bitfield in an aligned four-byte unit.
    // Retail's constructor clears the low byte at +0x44.
    *reinterpret_cast<std::uint8_t *>(
        reinterpret_cast<std::byte *>(frictionSystem) + 0x44u) =
        static_cast<std::uint8_t>(IVP_TRUE);
}

inline void RefreshContactPoint(IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointRecalculateFriction,
        contactPoint);
    void *temporaryInfo = ContactPointTemporaryInfo(contactPoint);
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointReadMaterials,
        contactPoint, temporaryInfo);
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointCalculateVirtualMass,
        contactPoint);
}

inline void DestroyContactPoint(IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointDestruct, contactPoint);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::OperatorDelete, contactPoint);
}

inline std::int16_t FrictionSystemDistanceCount(
    IVP_Friction_System *frictionSystem) {
    return *reinterpret_cast<std::int16_t *>(
        reinterpret_cast<std::byte *>(frictionSystem) + 0x3Eu);
}

inline void DeleteFrictionDistance(
    IVP_Friction_System *frictionSystem,
    IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemDeleteDistance,
        frictionSystem, contactPoint);
}

inline void DestroyFrictionSystem(IVP_Friction_System *frictionSystem) {
    BML::IVP::ABI::InvokeThis<void *>(
        BML::IVP::ABI::Address::FrictionSystemScalarDeletingDestructor,
        frictionSystem, 1u);
}

} // namespace BML::IVP::Detail

inline void IVP_Core::revive_adjacent_to_unmoveable() {
    IVP_U_Memory *memory = environment->get_sim_unit_mem();
    BML::IVP::Detail::StartSimulationMemoryTransaction(memory);

    for (int objectIndex = objects.len() - 1;
         objectIndex >= 0; --objectIndex) {
        IVP_Synapse_Friction *synapse =
            objects.element_at(objectIndex)->get_first_friction_synapse();
        while (synapse) {
            IVP_Synapse_Friction *next = synapse->get_next();
            IVP_Contact_Point *contactPoint = synapse->get_contact_point();
            IVP_Friction_System *frictionSystem =
                BML::IVP::Detail::ContactPointFrictionSystem(contactPoint);
            BML::IVP::Detail::SetFrictionSystemUnionFindNecessary(
                frictionSystem);

            IVP_Core *otherCore =
                BML::IVP::Detail::ContactPointObject(contactPoint, 0)
                    ->get_core();
            if (otherCore == this) {
                otherCore =
                    BML::IVP::Detail::ContactPointObject(contactPoint, 1)
                        ->get_core();
            }

            if (otherCore->is_physical_unmoveable()) {
                BML::IVP::Detail::DestroyContactPoint(contactPoint);
            } else {
                otherCore->ensure_all_core_objs_in_simulation();
                BML::IVP::Detail::RefreshContactPoint(contactPoint);
            }
            synapse = next;
        }
    }

    BML::IVP::Detail::EndSimulationMemoryTransaction(memory);
}

inline void IVP_Core::values_changed_recalc_redundants() {
    IVP_U_Memory *memory = environment->get_sim_unit_mem();
    BML::IVP::Detail::StartSimulationMemoryTransaction(memory);
    calc_calc();
    BML::IVP::Detail::EndSimulationMemoryTransaction(memory);

    IVP_Movement_Type objectMovement;
    if (is_physical_unmoveable()) {
        objectMovement = IVP_MT_STATIC;
        movement_state = IVP_MT_NOT_SIM;
    } else {
        if (movement_state > IVP_MT_NOT_SIM)
            movement_state = IVP_MT_NOT_SIM;
        objectMovement = static_cast<IVP_Movement_Type>(movement_state);
    }

    for (int objectIndex = objects.len() - 1;
         objectIndex >= 0; --objectIndex) {
        objects.element_at(objectIndex)->set_movement_state(objectMovement);
    }

    if (is_physical_unmoveable()) {
        stop_movement_without_collision_recheck();
        revive_adjacent_to_unmoveable();
        return;
    }

    ensure_core_to_be_in_simulation();
    BML::IVP::Detail::StartSimulationMemoryTransaction(memory);
    for (int objectIndex = objects.len() - 1;
         objectIndex >= 0; --objectIndex) {
        IVP_Synapse_Friction *synapse =
            objects.element_at(objectIndex)->get_first_friction_synapse();
        IVP_Friction_System *frictionSystem = synapse
            ? BML::IVP::Detail::ContactPointFrictionSystem(
                  synapse->get_contact_point())
            : nullptr;

        while (synapse) {
            IVP_Contact_Point *contactPoint = synapse->get_contact_point();
            IVP_Friction_System *nextSystem =
                BML::IVP::Detail::ContactPointFrictionSystem(contactPoint);
            if (nextSystem != frictionSystem) {
                BML::IVP::ABI::InvokeThis<void>(
                    BML::IVP::ABI::Address::FrictionSystemFusion,
                    frictionSystem, nextSystem);
            }
            BML::IVP::Detail::RefreshContactPoint(contactPoint);
            synapse = synapse->get_next();
        }
    }
    BML::IVP::Detail::EndSimulationMemoryTransaction(memory);
}

inline void IVP_Real_Object::delete_and_check_vicinity() {
    IVP_Core *core = get_core();

    // The neighboring implementation first asks the movable simulation unit
    // to wake before performing the common vicinity-revival step. Ballance's
    // retained Core entry contains that inlined simulation-unit operation.
    if (!core->is_physical_unmoveable())
        core->ensure_core_to_be_in_simulation();

    if (core->is_physical_unmoveable()) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectGetAllNearMindists, this);
        core->grow_friction_system();
        core->revive_adjacent_to_unmoveable();
    } else {
        core->ensure_core_to_be_in_simulation();
    }

    // This retained helper null-checks and invokes vtable slot 0 with the
    // MSVC scalar-deleting-destructor flag. No local destructor is substituted.
    destroy();
}

inline void IVP_Real_Object::recalc_exact_mindists_of_object() {
    IVP_Mindist_Manager *manager = environment->get_mindist_manager();
    for (IVP_Synapse_Real *synapse = exact_synapses; synapse;) {
        IVP_Synapse_Real *next = synapse->get_next();
        manager->recalc_exact_mindist(synapse->get_mindist());
        synapse = next;
    }
}

inline void IVP_Real_Object::recalc_invalid_mindists_of_object() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::RealObjectRecalculateInvalidMindists, this);
}

inline void IVP_Calc_Next_PSI_Solver::set_transformation(
    const IVP_U_Quat *rotation, const IVP_U_Point *position,
    IVP_BOOL optimizeForRepeatedCalls) {
    IVP_Environment *environment = core->get_environment();
    const IVP_Time currentTime = environment->get_current_time();

    // This retained Environment entry invalidates every time-based cache even
    // when the numeric time is unchanged, exactly as the neighboring solver
    // requires before rewriting a Core transform.
    environment->set_current_time(currentTime);

    IVP_FLOAT simulationDelta = static_cast<IVP_FLOAT>(
        currentTime - core->time_of_last_psi);
    if (simulationDelta >= environment->get_delta_PSI_time()) {
        core->i_delta_time = 1.0f / simulationDelta;
    } else {
        simulationDelta = environment->get_delta_PSI_time();
        core->i_delta_time = environment->get_inv_delta_PSI_time();
    }

    core->time_of_last_psi = currentTime;
    core->q_world_f_core_next_psi = *rotation;
    IVP_U_Quat coreFromPreviousCore;
    coreFromPreviousCore.set_invert_mult(
        &core->q_world_f_core_last_psi,
        &core->q_world_f_core_next_psi);
    core->q_world_f_core_last_psi = *rotation;
    core->time_of_last_psi = environment->get_old_time_of_last_PSI();

    IVP_U_Point movedDistance;
    movedDistance.subtract(position, &core->pos_world_f_core_last_psi);
    const IVP_FLOAT movedDistanceValue =
        static_cast<IVP_FLOAT>(movedDistance.real_length());
    core->pos_world_f_core_last_psi.set(position);
    core->delta_world_f_core_psis.set_to_zero();
    core->speed.set_to_zero();

    core->m_world_f_core_last_psi.get_position()->set(position);
    rotation->set_matrix(&core->m_world_f_core_last_psi);

    if (optimizeForRepeatedCalls) {
        core->current_speed = movedDistanceValue * core->i_delta_time;
        calc_psi_rotation_axis(&coreFromPreviousCore);
    } else {
        core->current_speed = 0.0f;
        coreFromPreviousCore.init();
        calc_psi_rotation_axis(&coreFromPreviousCore);
    }

    const IVP_DOUBLE hullGrowth =
        core->max_surface_deviation * core->max_surface_rot_speed *
            simulationDelta +
        movedDistanceValue;
    for (int index = core->objects.len() - 1; index >= 0; --index) {
        IVP_Real_Object *object = core->objects.element_at(index);
        IVP_Hull_Manager *hull = object->get_hull_manager();
        hull->jump_add_hull(static_cast<IVP_FLOAT>(hullGrowth),
                            movedDistanceValue);

        if (object->get_movement_state() >= IVP_MT_NOT_SIM) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::CacheManagerInvalidObject, object);
            object->recalc_exact_mindists_of_object();
            object->recalc_invalid_mindists_of_object();
        }

        if (!optimizeForRepeatedCalls)
            environment->get_mindist_manager()->recheck_ov_element(object);

        hull->check_hull_synapses();
        hull->check_for_reset();
    }

    core->current_speed = 0.0f;
    core->abs_omega = 0.0f;
    core->max_surface_rot_speed = 0.0f;
}

inline void IVP_Calc_Next_PSI_Solver::commit_one_hull_manager(
    IVP_Environment *,
    IVP_U_Vector<IVP_Hull_Manager_Base> *activeHullManagers) {
    if (!activeHullManagers || activeHullManagers->len() == 0)
        return;
    auto *hull = reinterpret_cast<IVP_Hull_Manager *>(
        activeHullManagers->element_at(0));
    hull->check_hull_synapses();
    hull->check_for_reset();
}

inline void IVP_Real_Object::beam_object_to_new_position(
    const IVP_U_Quat *rotationWorldFromObject,
    const IVP_U_Point *positionWorldFromObject,
    IVP_BOOL optimizeForRepeatedCalls) {
    IVP_Core *core = get_core();
    IVP_U_Quat rotationWorldFromCore(*rotationWorldFromObject);
    IVP_U_Point positionWorldFromCore(*positionWorldFromObject);

    if (((flags.raw >> 10u) & 0x3u) == 0u) {
        IVP_U_Matrix worldFromObject;
        rotationWorldFromObject->set_matrix(&worldFromObject);
        worldFromObject.get_position()->set(positionWorldFromObject);
        IVP_U_Float_Point inverseShift;
        inverseShift.set_negative(&shift_core_f_object);
        worldFromObject.vmult4(&inverseShift, &positionWorldFromCore);
    }

    if (q_core_f_object) {
        rotationWorldFromCore.set_div_unit_quat(
            rotationWorldFromObject, q_core_f_object);
    }

    IVP_Calc_Next_PSI_Solver solver(core);
    solver.set_transformation(&rotationWorldFromCore,
                              &positionWorldFromCore,
                              optimizeForRepeatedCalls);
}

inline void IVP_Real_Object::unlink_contact_points_for_object(
    IVP_Real_Object *otherObject) {
    IVP_Synapse_Friction *synapse = get_first_friction_synapse();
    while (synapse) {
        IVP_Contact_Point *contactPoint = synapse->get_contact_point();
        IVP_Friction_System *frictionSystem =
            BML::IVP::Detail::ContactPointFrictionSystem(contactPoint);
        synapse = synapse->get_next();

        if (BML::IVP::Detail::ContactPointObject(contactPoint, 0) !=
                otherObject &&
            BML::IVP::Detail::ContactPointObject(contactPoint, 1) !=
                otherObject) {
            continue;
        }

        BML::IVP::Detail::DeleteFrictionDistance(
            frictionSystem, contactPoint);
        if (BML::IVP::Detail::FrictionSystemDistanceCount(
                frictionSystem) == 0) {
            BML::IVP::Detail::DestroyFrictionSystem(frictionSystem);
            return;
        }
    }
}

inline void IVP_Real_Object::do_radar_checking(IVP_Radar *radar) {
    if (!radar)
        return;

    IVP_Radar_Hit hit{};
    IVP_DOUBLE maxRange = radar->max_range;
    for (IVP_Synapse_Real *synapse = exact_synapses;
         synapse; synapse = synapse->get_next()) {
        IVP_Mindist *mindist = synapse->get_mindist();
        const int thisIndex =
            mindist->get_synapse(1)->get_object() == this ? 1 : 0;
        hit.this_object = mindist->get_synapse(thisIndex)->get_object();
        hit.other_object = mindist->get_synapse(1 - thisIndex)->get_object();
        hit.dist = mindist->get_length();
        if (hit.dist <= maxRange)
            radar->radar_hit(&hit);
    }

    IVP_Hull_Manager *manager = get_hull_manager();
    maxRange = radar->max_range + manager->get_current_hull_time();
    IVP_U_Min_List_Enumerator candidates(manager->get_sorted_synapses());
    while (auto *listener = static_cast<IVP_Listener_Hull *>(
               candidates.get_next_element_lt(
                   static_cast<IVP_FLOAT>(maxRange)))) {
        if (listener->get_type() != IVP_HULL_ELEM_POLYGON)
            continue;

        auto *synapse = static_cast<IVP_Synapse_Real *>(listener);
        IVP_Mindist *mindist = synapse->get_mindist();
        if (mindist->recalc_mindist() != IVP_MRC_OK)
            continue;

        const int thisIndex =
            mindist->get_synapse(1)->get_object() == this ? 1 : 0;
        hit.this_object = mindist->get_synapse(thisIndex)->get_object();
        hit.other_object = mindist->get_synapse(1 - thisIndex)->get_object();
        hit.dist = mindist->get_length();
        if (hit.dist <= maxRange)
            radar->radar_hit(&hit);
    }
}

inline IVP_BOOL IVP_Real_Object::disable_simulation() {
    IVP_Core *core = get_core();
    if (((core->flags >> 2u) & 0x3u) != 0u)
        return IVP_TRUE;

    if (((core->flags >> 4u) & 0x3u) != 0u) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EnvironmentRemoveReviveCore,
            core->environment, core);
    }

    if (core->movement_state >= IVP_MT_NOT_SIM)
        return IVP_TRUE;

    for (int index = core->objects.len() - 1; index >= 0; --index) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RealObjectUnlinkContactPoints,
            core->objects.element_at(index), IVP_TRUE);
    }

    core->q_world_f_core_calm_reference[0].set(
        &core->q_world_f_core_next_psi);
    core->position_world_f_core_calm_reference[0].set(
        &core->pos_world_f_core_last_psi);
    core->time_of_calm_reference[0] = IVP_Time(
        core->environment->get_current_time().get_time() - 20.0);

    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::SimulationUnitUnionFind,
        core->sim_unit_of_core);
    return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
        BML::IVP::ABI::Address::SimulationUnitCalculateMovementState,
        core->sim_unit_of_core, core->environment);
}

inline void IVP_Real_Object::add_listener_object(
    IVP_Listener_Object *listener) {
    environment->get_cluster_manager()->add_listener_object(this, listener);
}

inline void IVP_Real_Object::remove_listener_object(
    IVP_Listener_Object *listener) {
    environment->get_cluster_manager()->remove_listener_object(this, listener);
}

class IVP_Application_Environment {
public:
    IVP_Application_Environment() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::ApplicationEnvironmentConstruct, this,
            [this] {
                n_cache_object = 256;
                scratchpad_addr = nullptr;
                scratchpad_size = 0;
                material_manager = nullptr;
                collision_filter = nullptr;
                universe_manager = nullptr;
                performancecounter = nullptr;
                anomaly_manager = nullptr;
                anomaly_limits = nullptr;
                default_collision_delegator_root = nullptr;
                env_active_float_manager = nullptr;
                range_manager = nullptr;
            });
    }

    std::int32_t n_cache_object;
    char *scratchpad_addr;
    std::int32_t scratchpad_size;
    IVP_Material_Manager *material_manager;
    IVP_Collision_Filter *collision_filter;
    IVP_Universe_Manager *universe_manager;
    IVP_PerformanceCounter *performancecounter;
    IVP_Anomaly_Manager *anomaly_manager;
    IVP_Anomaly_Limits *anomaly_limits;
    IVP_Collision_Delegator_Root *default_collision_delegator_root;
    IVP_U_Active_Value_Manager *env_active_float_manager;
    IVP_Range_Manager *range_manager;
};

class IVP_Environment_Manager {
    friend class IVP_Environment;

private:
    // The only instance is physics_RT's process-global singleton. Ballance's
    // retained constructor at RVA 0x136E0 is private; exposing an implicit
    // public default constructor creates an owner the DLL never supports.
    IVP_Environment_Manager();

public:
    ~IVP_Environment_Manager() = default;

    std::int32_t ivp_willamette_optimization;
    IVP_U_Vector<IVP_Environment> environments;

    IVP_Environment *create_environment(
        IVP_Application_Environment *application, const char *customerName,
        unsigned int authorizationCode) {
        return BML::IVP::ABI::InvokeThis<IVP_Environment *>(
            BML::IVP::ABI::Address::EnvironmentManagerCreateEnvironment,
            this, application, customerName, authorizationCode);
    }
    static IVP_Environment_Manager *get_environment_manager() {
        return BML::IVP::ABI::Invoke<IVP_Environment_Manager *>(
            BML::IVP::ABI::Address::EnvironmentManagerGet);
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Statistic_Manager) == 0x60);
static_assert(offsetof(IVP_Statistic_Manager, l_environment) == 0x00);
static_assert(offsetof(IVP_Statistic_Manager, last_statistic_output) == 0x08);
static_assert(offsetof(IVP_Statistic_Manager, max_rescue_speed) == 0x10);
static_assert(offsetof(IVP_Statistic_Manager, impact_sys_num) == 0x18);
static_assert(offsetof(IVP_Statistic_Manager, impact_unmov) == 0x34);
static_assert(offsetof(IVP_Statistic_Manager, sum_energy_destr) == 0x38);
static_assert(offsetof(IVP_Statistic_Manager, sum_of_mindists) == 0x40);
static_assert(offsetof(IVP_Statistic_Manager, global_fmd_counter) == 0x58);
static_assert(sizeof(IVP_Freeze_Manager) == 0x04);
static_assert(offsetof(IVP_Freeze_Manager, freeze_check_dtime) == 0x00);
static_assert(sizeof(IVP_Environment) == 0x178);
static_assert(sizeof(IVP_Application_Environment) == 0x30);
static_assert(offsetof(IVP_Application_Environment, n_cache_object) == 0x00);
static_assert(offsetof(IVP_Application_Environment, material_manager) == 0x0C);
static_assert(offsetof(IVP_Application_Environment, anomaly_manager) == 0x1C);
static_assert(offsetof(IVP_Application_Environment,
                       default_collision_delegator_root) == 0x24);
static_assert(offsetof(IVP_Application_Environment, range_manager) == 0x2C);
static_assert(sizeof(IVP_Environment_Manager) == 0x0C);
static_assert(offsetof(IVP_Environment_Manager, ivp_willamette_optimization) ==
              0x00);
static_assert(offsetof(IVP_Environment_Manager, environments) == 0x04);
static_assert(sizeof(IVP_Cluster_Manager) == 0x18);
static_assert(sizeof(BML::IVP::Detail::ObjectListenerTable) == 0x0C);
static_assert(sizeof(IVP_Vector_of_Hulls_128) == 0x208);
static_assert(sizeof(IVP_Vector_of_Cores_128) == 0x208);
static_assert(offsetof(IVP_Environment, sim_units_manager) == 0x08);
static_assert(offsetof(IVP_Environment, static_object) == 0x30);
static_assert(offsetof(IVP_Environment, statistic_manager) == 0x38);
static_assert(offsetof(IVP_Environment, freeze_manager) == 0x98);
static_assert(offsetof(IVP_Environment, delta_PSI_time) == 0xC0);
static_assert(offsetof(IVP_Environment, gravity) == 0xD0);
static_assert(offsetof(IVP_Environment, collision_listeners) == 0xF4);
static_assert(offsetof(IVP_Environment, psi_listeners) == 0xFC);
static_assert(offsetof(IVP_Environment, core_revive_list) == 0x104);
static_assert(offsetof(IVP_Environment, auth_costumer_name) == 0x10C);
static_assert(offsetof(IVP_Environment, auth_costumer_code) == 0x110);
static_assert(offsetof(IVP_Environment, pw_count) == 0x114);
static_assert(offsetof(IVP_Environment, environment_manager) == 0x118);
static_assert(offsetof(IVP_Environment, current_time) == 0x120);
static_assert(offsetof(IVP_Environment, state) == 0x144);
static_assert(offsetof(IVP_Environment, global_object_listeners) == 0x150);
static_assert(offsetof(IVP_Environment, collision_delegator_roots) == 0x158);
static_assert(offsetof(IVP_Environment, client_data) == 0x16C);
static_assert(offsetof(IVP_Environment, environment_magic_number) == 0x170);
#endif

#endif // BML_IVP_ENVIRONMENT_H
