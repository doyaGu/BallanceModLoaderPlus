#include "IvpTestAdapter.h"

#include "BML/IVP/SimulationUnit.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>
#include <vector>

namespace {

enum class Operation {
    AllocateUnit,
    ConstructUnit,
    DestructUnit,
    AddUnitToManager,
    RemoveUnitFromManager,
    AddCoreToUnit,
    KnowsController,
    AddControllerUnit,
    AddControlledCore,
    CleanUnit,
    UnionFindTest,
};

class TestController final : public IVP_Controller_Independent {
public:
    explicit TestController(IVP_CONTROLLER_PRIORITY priority)
        : priority(priority) {}

    void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *) override {}
    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return priority;
    }

    IVP_CONTROLLER_PRIORITY priority;
};

std::vector<Operation> g_operations;
std::vector<IVP_Simulation_Unit *> g_allocatedUnits;
std::vector<IVP_Simulation_Unit *> g_constructedUnits;
std::vector<IVP_Simulation_Unit *> g_registeredUnits;
std::vector<IVP_Simulation_Unit *> g_unregisteredUnits;
IVP_Sim_Units_Manager *g_manager = nullptr;
IVP_Core *g_unionFindResult = nullptr;
int g_knownFalseCount = 0;
bool g_expectRawVectorStorage = false;

void InitVector(IVP_U_Vector_Base *vector, void **storage, int capacity) {
    vector->memsize = static_cast<std::uint16_t>(capacity);
    vector->n_elems = 0;
    vector->elems = storage;
}

void InitSimulationUnit(
    IVP_Simulation_Unit *unit, void **coreStorage, int coreCapacity,
    void **controllerStorage, int controllerCapacity) {
    std::memset(unit, 0, sizeof(*unit));
    unit->set_unit_movement_type(IVP_MT_NOT_SIM);
    InitVector(
        reinterpret_cast<IVP_U_Vector_Base *>(&unit->sim_unit_cores),
        coreStorage, coreCapacity);
    InitVector(
        reinterpret_cast<IVP_U_Vector_Base *>(&unit->controller_cores),
        controllerStorage, controllerCapacity);
}

void InitCore(IVP_Core *core, IVP_Environment *environment,
              void **controllerStorage, int controllerCapacity) {
    std::memset(core, 0, sizeof(*core));
    core->environment = environment;
    InitVector(
        reinterpret_cast<IVP_U_Vector_Base *>(&core->controllers_of_core),
        controllerStorage, controllerCapacity);
}

void *__cdecl AllocateEngineObject(unsigned int size) {
    EXPECT_EQ(size, 0x24u);
    g_operations.push_back(Operation::AllocateUnit);
    EXPECT_LT(g_constructedUnits.size(), g_allocatedUnits.size());
    return g_allocatedUnits[g_constructedUnits.size()];
}

void __fastcall ConstructSimulationUnit(IVP_Simulation_Unit *unit, void *) {
    g_operations.push_back(Operation::ConstructUnit);
    g_constructedUnits.push_back(unit);
    if (g_expectRawVectorStorage) {
        const auto *bytes = reinterpret_cast<const unsigned char *>(unit);
        for (std::size_t offset = 0x0C; offset < sizeof(*unit); ++offset)
            EXPECT_EQ(bytes[offset], 0xA5u) << "offset 0x" << std::hex << offset;
    }
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
    unit->set_unit_movement_type(IVP_MT_NOT_SIM);
}

void __fastcall DestructSimulationUnit(IVP_Simulation_Unit *unit, void *) {
    g_operations.push_back(Operation::DestructUnit);
    unit->sim_unit_cores.elems = nullptr;
    unit->sim_unit_cores.memsize = 0;
    unit->sim_unit_cores.n_elems = 0;
    unit->controller_cores.elems = nullptr;
    unit->controller_cores.memsize = 0;
    unit->controller_cores.n_elems = 0;
}

void __fastcall AddSimulationUnitToManager(
    IVP_Sim_Units_Manager *manager, void *, IVP_Simulation_Unit *unit) {
    EXPECT_EQ(manager, g_manager);
    EXPECT_EQ(unit->get_unit_movement_type(), IVP_MT_MOVING);
    g_operations.push_back(Operation::AddUnitToManager);
    g_registeredUnits.push_back(unit);
}

void __fastcall RemoveSimulationUnitFromManager(
    IVP_Sim_Units_Manager *manager, void *, IVP_Simulation_Unit *unit) {
    EXPECT_EQ(manager, g_manager);
    g_operations.push_back(Operation::RemoveUnitFromManager);
    g_unregisteredUnits.push_back(unit);
}

void __fastcall AddCoreToSimulationUnit(
    IVP_Simulation_Unit *unit, void *, IVP_Core *core) {
    g_operations.push_back(Operation::AddCoreToUnit);
    unit->sim_unit_cores.add(core);
}

IVP_BOOL __fastcall ControllerIsKnown(
    IVP_Simulation_Unit *unit, void *, IVP_Controller *controller) {
    g_operations.push_back(Operation::KnowsController);
    for (int index = unit->controller_cores.len() - 1; index >= 0; --index) {
        if (unit->controller_cores.element_at(index)->l_controller ==
            controller) {
            return IVP_TRUE;
        }
    }
    ++g_knownFalseCount;
    return IVP_FALSE;
}

void __fastcall AddControllerUnit(
    IVP_Simulation_Unit *unit, void *, IVP_Controller *controller) {
    g_operations.push_back(Operation::AddControllerUnit);
    auto *list = new IVP_Sim_Unit_Controller_Core_List();
    list->l_controller = controller;
    unit->controller_cores.add(list);
}

void __fastcall AddControlledCore(
    IVP_Simulation_Unit *unit, void *, IVP_Controller *controller,
    IVP_Core *core) {
    g_operations.push_back(Operation::AddControlledCore);
    for (int index = unit->controller_cores.len() - 1; index >= 0; --index) {
        IVP_Sim_Unit_Controller_Core_List *list =
            unit->controller_cores.element_at(index);
        if (list->l_controller == controller) {
            list->cores_controlled_by.add(core);
            return;
        }
    }
    ADD_FAILURE() << "controlled core added before its controller list";
}

void __fastcall CleanSimulationUnit(IVP_Simulation_Unit *unit, void *) {
    g_operations.push_back(Operation::CleanUnit);
    unit->controller_cores.n_elems = 0;
}

IVP_Core *__fastcall UnionFindTest(IVP_Simulation_Unit *, void *) {
    g_operations.push_back(Operation::UnionFindTest);
    return g_unionFindResult;
}

void __fastcall ExchangeControllers(
    IVP_Simulation_Unit *unit, void *, int first, int second) {
    unit->controller_cores.swap_elems(first, second);
}

void __fastcall SortControllers(IVP_Simulation_Unit *unit, void *) {
    const int controllerCount = unit->controller_cores.len();
    int first = 0;
    while (true) {
        const int second = first + 1;
        if (second >= controllerCount)
            break;
        const IVP_CONTROLLER_PRIORITY secondPriority =
            unit->controller_cores.element_at(second)
                ->l_controller->get_controller_priority();
        if (unit->controller_cores.element_at(first)
                ->l_controller->get_controller_priority() > secondPriority) {
            unit->sim_unit_exchange_controllers(first, second);
            int testPosition = first;
            while (testPosition > 0 &&
                   unit->controller_cores.element_at(testPosition - 1)
                           ->l_controller->get_controller_priority() >
                       secondPriority) {
                unit->sim_unit_exchange_controllers(
                    testPosition - 1, testPosition);
                --testPosition;
            }
        }
        first = second;
    }
}

void __fastcall CalculateRedundants(IVP_Simulation_Unit *unit, void *) {
    for (int index = unit->sim_unit_cores.len() - 1; index >= 0; --index) {
        IVP_Core *core = unit->sim_unit_cores.element_at(index);
        for (int controllerIndex = core->controllers_of_core.len() - 1;
             controllerIndex >= 0; --controllerIndex) {
            IVP_Controller *controller =
                core->controllers_of_core.element_at(controllerIndex);
            if (unit->controller_is_known_to_sim_unit(controller) == IVP_FALSE)
                unit->add_controller_unit_sim(controller);
            unit->add_controlled_core_for_controller(controller, core);
        }
    }
    unit->sim_unit_sort_controllers();
}

void __fastcall SplitSimulationUnit(
    IVP_Simulation_Unit *unit, void *, IVP_Core *splitFather) {
    IVP_Core *nextSplitFather = nullptr;
    IVP_BOOL nextSplitNecessary = IVP_FALSE;
    do {
        nextSplitFather = nullptr;
        nextSplitNecessary = IVP_FALSE;
        IVP_Simulation_Unit *splitNewUnit = new IVP_Simulation_Unit();
        splitNewUnit->set_unit_movement_type(IVP_MT_MOVING);
        splitFather->environment->get_sim_units_manager()
            ->add_sim_unit_to_manager(splitNewUnit);

        for (int index = 0; index < unit->sim_unit_cores.len(); ++index) {
            IVP_Core *core = unit->sim_unit_cores.element_at(index);
            IVP_Core *father = core->union_find_get_father();
            if (father != splitFather) {
                if (nextSplitFather) {
                    if (father != nextSplitFather)
                        nextSplitNecessary = IVP_TRUE;
                } else {
                    nextSplitFather = father;
                }
                continue;
            }
            unit->sim_unit_cores.remove_at(index);
            --index;
            splitNewUnit->sim_unit_cores.add(core);
            core->sim_unit_of_core = splitNewUnit;
        }

        splitNewUnit->sim_unit_calc_redundants();
        splitFather = nextSplitFather;
    } while (nextSplitNecessary == IVP_TRUE);
}

void __fastcall ThrowCoresIntoUnit(
    IVP_Simulation_Unit *unit, void *, IVP_Simulation_Unit *secondUnit) {
    IVP_Environment *environment = nullptr;
    for (int index = 0; index < secondUnit->sim_unit_cores.len(); ++index) {
        IVP_Core *core = secondUnit->sim_unit_cores.element_at(index);
        unit->add_sim_unit_core(core);
        environment = core->environment;
        core->sim_unit_of_core = unit;
    }
    environment->get_sim_units_manager()->rem_sim_unit_from_manager(
        secondUnit);
}

void __fastcall FuseSimulationUnits(
    IVP_Simulation_Unit *unit, void *, IVP_Simulation_Unit *secondUnit) {
    unit->clean_sim_unit();
    unit->throw_cores_into_my_sim_unit(secondUnit);
    unit->sim_unit_calc_redundants();
}

void __fastcall AddControllerOfCore(
    IVP_Simulation_Unit *unit, void *, IVP_Core *core,
    IVP_Controller *controller) {
    if (unit->controller_is_known_to_sim_unit(controller) == IVP_FALSE)
        unit->add_controller_unit_sim(controller);
    unit->add_controlled_core_for_controller(controller, core);
    unit->sim_unit_sort_controllers();
}

void __fastcall PerformTestAndSplit(IVP_Simulation_Unit *unit, void *) {
    IVP_Core *father = unit->sim_unit_union_find_test();
    if (!father)
        return;
    unit->clean_sim_unit();
    unit->split_sim_unit(father);
    unit->sim_unit_calc_redundants();
}

void __fastcall DestroyController(IVP_Controller *, void *) {}

void __fastcall IncrementVector(IVP_U_Vector_Base *vector, void *) {
    const std::uint16_t capacity = vector->memsize == 0
        ? 4
        : static_cast<std::uint16_t>(vector->memsize * 2);
    void **elements = static_cast<void **>(
        std::realloc(vector->elems, capacity * sizeof(void *)));
    if (!elements)
        throw std::bad_alloc();
    vector->elems = elements;
    vector->memsize = capacity;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorNew, &AllocateEngineObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitConstruct, &ConstructSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitDestruct, &DestructSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitsManagerAdd, &AddSimulationUnitToManager),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitsManagerRemove, &RemoveSimulationUnitFromManager),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitAddCore, &AddCoreToSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitKnowsController, &ControllerIsKnown),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitAddController, &AddControllerUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitAddControlledCore, &AddControlledCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitClean, &CleanSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitUnionFindTest, &UnionFindTest),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitCalculateRedundants, &CalculateRedundants),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitSplit, &SplitSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitPerformSplitTest, &PerformTestAndSplit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitThrowCores, &ThrowCoresIntoUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitFuse, &FuseSimulationUnits),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitAddControllerForCore, &AddControllerOfCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitSortControllers, &SortControllers),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitExchangeControllers, &ExchangeControllers),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerDestruct, &DestroyController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VectorIncrementMemory, &IncrementVector),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


void ResetObservations() {
    g_operations.clear();
    g_allocatedUnits.clear();
    g_constructedUnits.clear();
    g_registeredUnits.clear();
    g_unregisteredUnits.clear();
    g_manager = nullptr;
    g_unionFindResult = nullptr;
    g_knownFalseCount = 0;
    g_expectRawVectorStorage = false;
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpSimulationUnitSplitFuse,
     CompleteRetailLifecycleOwnsBothEmbeddedVectorLifetimes) {
    ResetObservations();
    alignas(IVP_Simulation_Unit)
        std::array<unsigned char, sizeof(IVP_Simulation_Unit)> storage{};
    storage.fill(0xA5u);
    g_expectRawVectorStorage = true;

    auto *unit = ::new (storage.data()) IVP_Simulation_Unit();

    EXPECT_EQ(unit->sim_unit_cores.memsize, 2);
    EXPECT_EQ(unit->sim_unit_cores.n_elems, 0);
    EXPECT_EQ(unit->sim_unit_cores.elems,
              reinterpret_cast<void **>(storage.data() + 0x14));
    EXPECT_EQ(unit->controller_cores.memsize, 0);
    EXPECT_EQ(unit->controller_cores.n_elems, 0);
    EXPECT_EQ(unit->controller_cores.elems, nullptr);
    unit->~IVP_Simulation_Unit();

    EXPECT_EQ(g_operations,
              (std::vector<Operation>{Operation::ConstructUnit,
                                      Operation::DestructUnit}));
}

TEST(IvpSimulationUnitSplitFuse,
     UnionFindFatherWalksTheRetailTmpPointerAtOffset228) {
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> firstData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> secondData{};
    auto *first = reinterpret_cast<IVP_Core *>(firstData.data());
    auto *second = reinterpret_cast<IVP_Core *>(secondData.data());
    first->tmp.union_find_father = second;
    second->tmp.union_find_father = nullptr;
    EXPECT_EQ(first->union_find_get_father(), second);
    EXPECT_EQ(second->union_find_get_father(), second);
}

TEST(IvpSimulationUnitSplitFuse,
     SplitsDisconnectedCoresIntoANewMovingUnit) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Sim_Units_Manager)> managerData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> oldUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> newUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> firstCoreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> secondCoreData{};
    std::array<void *, 4> oldCores{};
    std::array<void *, 2> oldControllers{};
    std::array<void *, 2> firstControllers{};
    std::array<void *, 2> secondControllers{};

    auto *environment =
        reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *manager =
        reinterpret_cast<IVP_Sim_Units_Manager *>(managerData.data());
    auto *oldUnit =
        reinterpret_cast<IVP_Simulation_Unit *>(oldUnitData.data());
    auto *newUnit =
        reinterpret_cast<IVP_Simulation_Unit *>(newUnitData.data());
    auto *firstCore = reinterpret_cast<IVP_Core *>(firstCoreData.data());
    auto *secondCore = reinterpret_cast<IVP_Core *>(secondCoreData.data());

    environment->sim_units_manager = manager;
    InitSimulationUnit(
        oldUnit, oldCores.data(), static_cast<int>(oldCores.size()),
        oldControllers.data(), static_cast<int>(oldControllers.size()));
    InitCore(firstCore, environment, firstControllers.data(),
             static_cast<int>(firstControllers.size()));
    InitCore(secondCore, environment, secondControllers.data(),
             static_cast<int>(secondControllers.size()));
    oldUnit->add_sim_unit_core(firstCore);
    oldUnit->add_sim_unit_core(secondCore);
    firstCore->sim_unit_of_core = oldUnit;
    secondCore->sim_unit_of_core = oldUnit;
    firstCore->tmp.union_find_father = nullptr;
    secondCore->tmp.union_find_father = nullptr;

    g_manager = manager;
    g_allocatedUnits.push_back(newUnit);

    oldUnit->split_sim_unit(firstCore);

    ASSERT_EQ(oldUnit->sim_unit_cores.len(), 1);
    EXPECT_EQ(oldUnit->sim_unit_cores.element_at(0), secondCore);
    EXPECT_EQ(secondCore->sim_unit_of_core, oldUnit);
    ASSERT_EQ(newUnit->sim_unit_cores.len(), 1);
    EXPECT_EQ(newUnit->sim_unit_cores.element_at(0), firstCore);
    EXPECT_EQ(firstCore->sim_unit_of_core, newUnit);
    EXPECT_EQ(newUnit->get_unit_movement_type(), IVP_MT_MOVING);
    ASSERT_EQ(g_registeredUnits.size(), 1u);
    EXPECT_EQ(g_registeredUnits[0], newUnit);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::AddCoreToUnit,
                  Operation::AddCoreToUnit,
                  Operation::AllocateUnit,
                  Operation::ConstructUnit,
                  Operation::AddUnitToManager,
              }));
}

TEST(IvpSimulationUnitSplitFuse,
     TailLoopsToIsolateThreeUnionFindComponents) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Sim_Units_Manager)> managerData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> oldUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> firstNewData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> secondNewData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coresData[3]{};
    std::array<void *, 4> oldCores{};
    std::array<void *, 2> oldControllers{};
    std::array<std::array<void *, 2>, 3> coreControllers{};

    auto *environment =
        reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *manager =
        reinterpret_cast<IVP_Sim_Units_Manager *>(managerData.data());
    auto *oldUnit =
        reinterpret_cast<IVP_Simulation_Unit *>(oldUnitData.data());
    auto *firstNew =
        reinterpret_cast<IVP_Simulation_Unit *>(firstNewData.data());
    auto *secondNew =
        reinterpret_cast<IVP_Simulation_Unit *>(secondNewData.data());
    IVP_Core *cores[3] = {
        reinterpret_cast<IVP_Core *>(coresData[0].data()),
        reinterpret_cast<IVP_Core *>(coresData[1].data()),
        reinterpret_cast<IVP_Core *>(coresData[2].data()),
    };

    environment->sim_units_manager = manager;
    InitSimulationUnit(
        oldUnit, oldCores.data(), static_cast<int>(oldCores.size()),
        oldControllers.data(), static_cast<int>(oldControllers.size()));
    for (int index = 0; index < 3; ++index) {
        InitCore(cores[index], environment, coreControllers[index].data(),
                 static_cast<int>(coreControllers[index].size()));
        oldUnit->add_sim_unit_core(cores[index]);
        cores[index]->sim_unit_of_core = oldUnit;
        cores[index]->tmp.union_find_father = nullptr;
    }

    g_manager = manager;
    g_allocatedUnits.push_back(firstNew);
    g_allocatedUnits.push_back(secondNew);

    oldUnit->split_sim_unit(cores[0]);

    EXPECT_EQ(oldUnit->sim_unit_cores.len(), 1);
    EXPECT_EQ(oldUnit->sim_unit_cores.element_at(0), cores[2]);
    EXPECT_EQ(cores[2]->sim_unit_of_core, oldUnit);
    EXPECT_EQ(firstNew->sim_unit_cores.len(), 1);
    EXPECT_EQ(firstNew->sim_unit_cores.element_at(0), cores[0]);
    EXPECT_EQ(cores[0]->sim_unit_of_core, firstNew);
    EXPECT_EQ(secondNew->sim_unit_cores.len(), 1);
    EXPECT_EQ(secondNew->sim_unit_cores.element_at(0), cores[1]);
    EXPECT_EQ(cores[1]->sim_unit_of_core, secondNew);
    EXPECT_EQ(firstNew->get_unit_movement_type(), IVP_MT_MOVING);
    EXPECT_EQ(secondNew->get_unit_movement_type(), IVP_MT_MOVING);
    ASSERT_EQ(g_registeredUnits.size(), 2u);
    EXPECT_EQ(g_registeredUnits[0], firstNew);
    EXPECT_EQ(g_registeredUnits[1], secondNew);
}

TEST(IvpSimulationUnitSplitFuse,
     FusesSecondUnitCoresAndUnregistersTheDonor) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Sim_Units_Manager)> managerData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> firstUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> secondUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> firstCoreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> secondCoreData{};
    std::array<void *, 4> firstCores{};
    std::array<void *, 2> secondCores{};
    std::array<void *, 2> firstControllers{};
    std::array<void *, 2> secondControllers{};
    std::array<void *, 2> firstCoreControllers{};
    std::array<void *, 2> secondCoreControllers{};

    auto *environment =
        reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *manager =
        reinterpret_cast<IVP_Sim_Units_Manager *>(managerData.data());
    auto *firstUnit =
        reinterpret_cast<IVP_Simulation_Unit *>(firstUnitData.data());
    auto *secondUnit =
        reinterpret_cast<IVP_Simulation_Unit *>(secondUnitData.data());
    auto *firstCore = reinterpret_cast<IVP_Core *>(firstCoreData.data());
    auto *secondCore = reinterpret_cast<IVP_Core *>(secondCoreData.data());

    environment->sim_units_manager = manager;
    InitSimulationUnit(
        firstUnit, firstCores.data(), static_cast<int>(firstCores.size()),
        firstControllers.data(), static_cast<int>(firstControllers.size()));
    InitSimulationUnit(
        secondUnit, secondCores.data(), static_cast<int>(secondCores.size()),
        secondControllers.data(), static_cast<int>(secondControllers.size()));
    InitCore(firstCore, environment, firstCoreControllers.data(),
             static_cast<int>(firstCoreControllers.size()));
    InitCore(secondCore, environment, secondCoreControllers.data(),
             static_cast<int>(secondCoreControllers.size()));
    firstUnit->add_sim_unit_core(firstCore);
    secondUnit->add_sim_unit_core(secondCore);
    firstCore->sim_unit_of_core = firstUnit;
    secondCore->sim_unit_of_core = secondUnit;

    g_manager = manager;
    firstUnit->fusion_simulation_unities(secondUnit);

    ASSERT_EQ(firstUnit->sim_unit_cores.len(), 2);
    EXPECT_EQ(firstUnit->sim_unit_cores.element_at(0), firstCore);
    EXPECT_EQ(firstUnit->sim_unit_cores.element_at(1), secondCore);
    EXPECT_EQ(firstCore->sim_unit_of_core, firstUnit);
    EXPECT_EQ(secondCore->sim_unit_of_core, firstUnit);
    ASSERT_EQ(g_unregisteredUnits.size(), 1u);
    EXPECT_EQ(g_unregisteredUnits[0], secondUnit);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::AddCoreToUnit,
                  Operation::AddCoreToUnit,
                  Operation::CleanUnit,
                  Operation::AddCoreToUnit,
                  Operation::RemoveUnitFromManager,
              }));
}

TEST(IvpSimulationUnitSplitFuse,
     RebuildsSharedControllersOnceThenSortsByPriority) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> unitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> firstCoreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> secondCoreData{};
    std::array<void *, 4> unitCores{};
    std::array<void *, 4> unitControllers{};
    std::array<void *, 4> firstControllers{};
    std::array<void *, 4> secondControllers{};

    auto *unit = reinterpret_cast<IVP_Simulation_Unit *>(unitData.data());
    auto *firstCore = reinterpret_cast<IVP_Core *>(firstCoreData.data());
    auto *secondCore = reinterpret_cast<IVP_Core *>(secondCoreData.data());
    TestController gravity(IVP_CP_GRAVITY);
    TestController constraint(IVP_CP_CONSTRAINTS);

    InitSimulationUnit(
        unit, unitCores.data(), static_cast<int>(unitCores.size()),
        unitControllers.data(), static_cast<int>(unitControllers.size()));
    InitCore(firstCore, nullptr, firstControllers.data(),
             static_cast<int>(firstControllers.size()));
    InitCore(secondCore, nullptr, secondControllers.data(),
             static_cast<int>(secondControllers.size()));
    unit->sim_unit_cores.add(firstCore);
    unit->sim_unit_cores.add(secondCore);
    firstCore->controllers_of_core.add(&gravity);
    secondCore->controllers_of_core.add(&gravity);
    secondCore->controllers_of_core.add(&constraint);

    unit->sim_unit_calc_redundants();

    ASSERT_EQ(unit->controller_cores.len(), 2);
    EXPECT_EQ(unit->controller_cores.element_at(0)->l_controller, &constraint);
    EXPECT_EQ(unit->controller_cores.element_at(1)->l_controller, &gravity);
    EXPECT_EQ(unit->controller_cores.element_at(0)->cores_controlled_by.len(),
              1);
    EXPECT_EQ(unit->controller_cores.element_at(0)
                  ->cores_controlled_by.element_at(0),
              secondCore);
    EXPECT_EQ(unit->controller_cores.element_at(1)->cores_controlled_by.len(),
              2);
    EXPECT_EQ(g_knownFalseCount, 2);

    for (int index = unit->controller_cores.len() - 1; index >= 0; --index)
        delete unit->controller_cores.element_at(index);
}

TEST(IvpSimulationUnitSplitFuse,
     PerformTestCleansThenSplitsWhenASecondFatherExists) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Sim_Units_Manager)> managerData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> oldUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Simulation_Unit)> newUnitData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> firstCoreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> secondCoreData{};
    std::array<void *, 4> oldCores{};
    std::array<void *, 2> oldControllers{};
    std::array<void *, 2> firstControllers{};
    std::array<void *, 2> secondControllers{};

    auto *environment =
        reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *manager =
        reinterpret_cast<IVP_Sim_Units_Manager *>(managerData.data());
    auto *oldUnit =
        reinterpret_cast<IVP_Simulation_Unit *>(oldUnitData.data());
    auto *newUnit =
        reinterpret_cast<IVP_Simulation_Unit *>(newUnitData.data());
    auto *firstCore = reinterpret_cast<IVP_Core *>(firstCoreData.data());
    auto *secondCore = reinterpret_cast<IVP_Core *>(secondCoreData.data());
    IVP_Sim_Unit_Controller_Core_List staleList;
    staleList.l_controller = reinterpret_cast<IVP_Controller *>(0x11110000u);

    environment->sim_units_manager = manager;
    InitSimulationUnit(
        oldUnit, oldCores.data(), static_cast<int>(oldCores.size()),
        oldControllers.data(), static_cast<int>(oldControllers.size()));
    InitCore(firstCore, environment, firstControllers.data(),
             static_cast<int>(firstControllers.size()));
    InitCore(secondCore, environment, secondControllers.data(),
             static_cast<int>(secondControllers.size()));
    oldUnit->add_sim_unit_core(firstCore);
    oldUnit->add_sim_unit_core(secondCore);
    oldUnit->controller_cores.add(&staleList);
    firstCore->sim_unit_of_core = oldUnit;
    secondCore->sim_unit_of_core = oldUnit;
    firstCore->tmp.union_find_father = nullptr;
    secondCore->tmp.union_find_father = nullptr;

    g_manager = manager;
    g_unionFindResult = firstCore;
    g_allocatedUnits.push_back(newUnit);

    oldUnit->perform_test_and_split();

    EXPECT_EQ(oldUnit->controller_cores.len(), 0);
    EXPECT_EQ(oldUnit->sim_unit_cores.element_at(0), secondCore);
    EXPECT_EQ(firstCore->sim_unit_of_core, newUnit);
    EXPECT_EQ(g_operations.front(), Operation::AddCoreToUnit);
    EXPECT_NE(std::find(g_operations.begin(), g_operations.end(),
                        Operation::UnionFindTest),
              g_operations.end());
    EXPECT_NE(std::find(g_operations.begin(), g_operations.end(),
                        Operation::CleanUnit),
              g_operations.end());
}

} // namespace
