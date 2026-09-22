#include "IvpTestAdapter.h"

#include "BML/IVP/SimulationUnit.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

enum class Operation {
    UnlinkPrimary,
    UnlinkAttached,
    DestroyStaticFrictionHash,
    DeleteStaticFrictionHash,
    FreezeCore,
    RemoveCoreFromOldUnit,
    AllocateReplacementUnit,
    ConstructReplacementUnit,
    AddCoreToReplacementUnit,
    AddReplacementUnitToManager,
    AddGravityController,
};

std::vector<Operation> g_operations;
IVP_Real_Object *g_primary = nullptr;
IVP_Real_Object *g_attached = nullptr;
IVP_Core *g_core = nullptr;
IVP_Simulation_Unit *g_oldUnit = nullptr;
IVP_Simulation_Unit *g_replacementUnit = nullptr;
IVP_Sim_Units_Manager *g_manager = nullptr;
IVP_Standard_Gravity_Controller *g_gravity = nullptr;
IVP_Friction_Hash *g_frictionHash = nullptr;

void __fastcall UnlinkContactPoints(
    IVP_Real_Object *object, void *, IVP_BOOL silent) {
    EXPECT_EQ(silent, IVP_FALSE);
    if (object == g_primary)
        g_operations.push_back(Operation::UnlinkPrimary);
    else if (object == g_attached)
        g_operations.push_back(Operation::UnlinkAttached);
    else
        ADD_FAILURE() << "unexpected object in compound-core unlink";
}

void __fastcall DestroyFrictionHash(IVP_VHash_Store *hash, void *) {
    EXPECT_EQ(reinterpret_cast<IVP_Friction_Hash *>(hash), g_frictionHash);
    g_operations.push_back(Operation::DestroyStaticFrictionHash);
}

void __cdecl DeleteEngineObject(void *memory) {
    EXPECT_EQ(memory, g_frictionHash);
    g_operations.push_back(Operation::DeleteStaticFrictionHash);
}

void __fastcall FreezeCore(IVP_Core *core, void *) {
    EXPECT_EQ(core, g_core);
    g_operations.push_back(Operation::FreezeCore);
    core->movement_state = IVP_MT_NOT_SIM;
}

void __fastcall RemoveCoreFromOldUnit(
    IVP_Simulation_Unit *unit, void *, IVP_Core *core) {
    EXPECT_EQ(unit, g_oldUnit);
    EXPECT_EQ(core, g_core);
    g_operations.push_back(Operation::RemoveCoreFromOldUnit);
}

void *__cdecl AllocateEngineObject(unsigned int size) {
    EXPECT_EQ(size, 0x24u);
    g_operations.push_back(Operation::AllocateReplacementUnit);
    return g_replacementUnit;
}

void __fastcall ConstructSimulationUnit(IVP_Simulation_Unit *unit, void *) {
    EXPECT_EQ(unit, g_replacementUnit);
    g_operations.push_back(Operation::ConstructReplacementUnit);

    auto *cores = reinterpret_cast<IVP_U_Vector_Base *>(
        reinterpret_cast<std::byte *>(unit) + 0x0C);
    cores->memsize = 2;
    cores->n_elems = 0;
    cores->elems = reinterpret_cast<void **>(
        reinterpret_cast<std::byte *>(unit) + 0x14);
    auto *controllers = reinterpret_cast<IVP_U_Vector_Base *>(
        reinterpret_cast<std::byte *>(unit) + 0x1C);
    controllers->memsize = 0;
    controllers->n_elems = 0;
    controllers->elems = nullptr;
    unit->union_find_needed_for_sim_unit = IVP_FALSE;
    unit->sim_unit_has_fast_objects = IVP_FALSE;
    unit->sim_unit_just_slowed_down = IVP_FALSE;
    unit->prev_sim_unit = nullptr;
    unit->next_sim_unit = nullptr;
    unit->set_unit_movement_type(IVP_MT_NOT_SIM);
}

void __fastcall AddCoreToSimulationUnit(
    IVP_Simulation_Unit *unit, void *, IVP_Core *core) {
    EXPECT_EQ(unit, g_replacementUnit);
    EXPECT_EQ(core, g_core);
    g_operations.push_back(Operation::AddCoreToReplacementUnit);
    unit->sim_unit_cores.add(core);
}

void __fastcall AddSimulationUnitToManager(
    IVP_Sim_Units_Manager *manager, void *, IVP_Simulation_Unit *unit) {
    EXPECT_EQ(manager, g_manager);
    EXPECT_EQ(unit, g_replacementUnit);
    EXPECT_EQ(unit->get_unit_movement_type(), IVP_MT_NOT_SIM);
    g_operations.push_back(Operation::AddReplacementUnitToManager);
}

void __fastcall AddCoreController(
    IVP_Core *core, void *, IVP_Controller *controller) {
    EXPECT_EQ(core, g_core);
    EXPECT_EQ(controller, g_gravity);
    EXPECT_EQ(core->sim_unit_of_core, g_replacementUnit);
    EXPECT_EQ(core->controllers_of_core.len(), 0);
    g_operations.push_back(Operation::AddGravityController);
    core->controllers_of_core.add(controller);
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectUnlinkContactPoints, &UnlinkContactPoints),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashStoreDestruct, &DestroyFrictionHash),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorDelete, &DeleteEngineObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreFreezeSimulation, &FreezeCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitRemoveCoreAndOwnership, &RemoveCoreFromOldUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorNew, &AllocateEngineObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitConstruct, &ConstructSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitAddCore, &AddCoreToSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitsManagerAdd, &AddSimulationUnitToManager),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreAddController, &AddCoreController),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


void ResetObservations() {
    g_operations.clear();
    g_primary = nullptr;
    g_attached = nullptr;
    g_core = nullptr;
    g_oldUnit = nullptr;
    g_replacementUnit = nullptr;
    g_manager = nullptr;
    g_gravity = nullptr;
    g_frictionHash = nullptr;
}

void SetCoreObjects(IVP_Core *core,
                    const std::array<void *, 2> &objects) {
    core->objects.elems = const_cast<void **>(objects.data());
    core->objects.memsize = static_cast<std::uint16_t>(objects.size());
    core->objects.n_elems = static_cast<std::uint16_t>(objects.size());
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpMovabilityTransition,
     FixesMovingCompoundObjectAndRehomesItsSimulationOwnership) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> primaryData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> attachedData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> oldUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> newUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Sim_Units_Manager)> managerData{};

    auto *environment = reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreData.data());
    auto *primary = reinterpret_cast<IVP_Real_Object *>(primaryData.data());
    auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedData.data());
    auto *oldUnit = reinterpret_cast<IVP_Simulation_Unit *>(oldUnitData.data());
    auto *newUnit = reinterpret_cast<IVP_Simulation_Unit *>(newUnitData.data());
    auto *manager = reinterpret_cast<IVP_Sim_Units_Manager *>(managerData.data());
    auto *gravity = reinterpret_cast<IVP_Standard_Gravity_Controller *>(0x12340000u);
    auto *constraint = reinterpret_cast<IVP_Controller *>(0x11110000u);
    auto *forceField = reinterpret_cast<IVP_Controller *>(0x22220000u);
    auto *frictionInfo = reinterpret_cast<IVP_Friction_Info_For_Core *>(0x33330000u);

    environment->sim_units_manager = manager;
    environment->standard_gravity_controller = gravity;
    primary->physical_core = core;
    attached->physical_core = core;
    core->environment = environment;
    core->sim_unit_of_core = oldUnit;
    core->flags = 0x00000101u; // fast piling and pinned; still movable.
    core->movement_state = IVP_MT_MOVING;
    core->core_friction_info.moveable_core_friction_info = frictionInfo;
    std::array<void *, 2> objects{primary, attached};
    SetCoreObjects(core, objects);
    std::array<void *, 2> controllers{constraint, forceField};
    core->controllers_of_core.elems = controllers.data();
    core->controllers_of_core.memsize = 2;
    core->controllers_of_core.n_elems = 2;

    g_primary = primary;
    g_attached = attached;
    g_core = core;
    g_oldUnit = oldUnit;
    g_replacementUnit = newUnit;
    g_manager = manager;
    g_gravity = gravity;

    primary->change_unmovable_flag(IVP_TRUE);

    EXPECT_EQ(core->is_physical_unmoveable(), IVP_TRUE);
    EXPECT_EQ(core->flags & ~(0x3u << 2u), 0x00000101u);
    EXPECT_EQ(core->movement_state, IVP_MT_NOT_SIM);
    EXPECT_EQ(core->core_friction_info.moveable_core_friction_info, nullptr);
    EXPECT_EQ(core->sim_unit_of_core, newUnit);
    ASSERT_EQ(newUnit->sim_unit_cores.len(), 1);
    EXPECT_EQ(newUnit->sim_unit_cores.element_at(0), core);
    EXPECT_EQ(newUnit->get_unit_movement_type(), IVP_MT_NOT_SIM);
    ASSERT_EQ(core->controllers_of_core.len(), 1);
    EXPECT_EQ(core->controllers_of_core.element_at(0), gravity);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::UnlinkAttached,
                  Operation::UnlinkPrimary,
                  Operation::FreezeCore,
                  Operation::RemoveCoreFromOldUnit,
                  Operation::AllocateReplacementUnit,
                  Operation::ConstructReplacementUnit,
                  Operation::AddCoreToReplacementUnit,
                  Operation::AddReplacementUnitToManager,
                  Operation::AddGravityController,
              }));
}

TEST(IvpMovabilityTransition,
     ReleasesStaticFrictionIndexWhenObstacleBecomesMovable) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> objectData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> unitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_VHash_Store)> hashData{};

    auto *core = reinterpret_cast<IVP_Core *>(coreData.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectData.data());
    auto *unit = reinterpret_cast<IVP_Simulation_Unit *>(unitData.data());
    auto *hash = reinterpret_cast<IVP_Friction_Hash *>(hashData.data());
    auto *gravity = reinterpret_cast<IVP_Controller *>(0x44440000u);

    object->physical_core = core;
    core->flags = 0x00000105u; // fast piling, physical static and pinned.
    core->movement_state = IVP_MT_NOT_SIM;
    core->sim_unit_of_core = unit;
    core->core_friction_info.l_friction_info_hash = hash;
    std::array<void *, 2> objects{object, nullptr};
    core->objects.elems = objects.data();
    core->objects.memsize = 2;
    core->objects.n_elems = 1;
    std::array<void *, 1> controllers{gravity};
    core->controllers_of_core.elems = controllers.data();
    core->controllers_of_core.memsize = 1;
    core->controllers_of_core.n_elems = 1;

    g_primary = object;
    g_core = core;
    g_oldUnit = unit;
    g_frictionHash = hash;

    object->change_unmovable_flag(IVP_FALSE);

    EXPECT_EQ(core->is_physical_unmoveable(), IVP_FALSE);
    EXPECT_EQ(core->flags, 0x00000101u);
    EXPECT_EQ(core->core_friction_info.l_friction_info_hash, nullptr);
    EXPECT_EQ(core->sim_unit_of_core, unit);
    ASSERT_EQ(core->controllers_of_core.len(), 1);
    EXPECT_EQ(core->controllers_of_core.element_at(0), gravity);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::UnlinkPrimary,
                  Operation::DestroyStaticFrictionHash,
                  Operation::DeleteStaticFrictionHash,
              }));
}

} // namespace
