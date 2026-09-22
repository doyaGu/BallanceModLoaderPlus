#include "IvpTestAdapter.h"

#include "BML/IVP/SimulationUnit.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

struct RawMinList {
    std::uint16_t mallocedSize;
    std::uint16_t freeList;
    IVP_U_Min_List_Element *elements;
    IVP_FLOAT minValue;
    std::uint16_t firstLong;
    std::uint16_t firstElement;
    std::uint16_t counter;
    std::uint16_t reserved;
};

static_assert(sizeof(RawMinList) == sizeof(IVP_U_Min_List));

struct ControllerEntry {
    IVP_Controller *controller;
};

class TimeAwareController final : public IVP_Controller_Dependent {
public:
    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        return nullptr;
    }
    void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *) override {}
    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_MOTION;
    }
    void reset_time(IVP_Time offset) override {
        ++resetCount;
        lastOffset = offset;
    }

    int resetCount = 0;
    IVP_Time lastOffset;
};

class RecordingTimeEvent final : public IVP_Time_Event {
public:
    void simulate_time_event(IVP_Environment *environment) override {
        ++callCount;
        observedTime = environment->current_time;
    }

    int callCount = 0;
    IVP_Time observedTime;
};

std::vector<void *> g_nodeAllocations;
std::vector<void *> g_nodeDeallocations;
std::vector<void *> g_textAllocations;
std::vector<void *> g_textDeallocations;

void *AllocateNode(unsigned int size) {
    void *memory = std::malloc(size);
    g_nodeAllocations.push_back(memory);
    return memory;
}

void DeleteNode(void *memory) {
    g_nodeDeallocations.push_back(memory);
    std::free(memory);
}

void *AllocateText(unsigned int size) {
    void *memory = std::malloc(size);
    g_textAllocations.push_back(memory);
    return memory;
}

void DeleteText(void *memory) {
    g_textDeallocations.push_back(memory);
    std::free(memory);
}

void __fastcall DestructDrawVector(
    IVP_Draw_Vector_Debug *drawVector, void *) {
    if (drawVector->debug_text) {
        DeleteText(drawVector->debug_text);
        drawVector->debug_text = nullptr;
    }
}

void __fastcall DeleteDrawVectors(IVP_Environment *environment, void *) {
    IVP_Draw_Vector_Debug *drawVector = environment->draw_vectors;
    while (drawVector) {
        IVP_Draw_Vector_Debug *next = drawVector->next;
        delete drawVector;
        drawVector = next;
    }
    environment->draw_vectors = nullptr;
}

void __fastcall RemoveMinListElement(
    IVP_U_Min_List *list, void *, unsigned int index) {
    auto *raw = reinterpret_cast<RawMinList *>(list);
    ASSERT_LT(index, raw->mallocedSize);
    IVP_U_Min_List_Element &removed = raw->elements[index];
    raw->firstElement = removed.next;
    removed.next = IVP_U_MINLIST_UNUSED;
    removed.prev = IVP_U_MINLIST_UNUSED;
    --raw->counter;
    raw->minValue = raw->firstElement == IVP_U_MINLIST_UNUSED
        ? IVP_U_MINLIST_MAXVALUE
        : raw->elements[raw->firstElement].value;
}

void __fastcall SetEnvironmentTime(
    IVP_Time_Manager *, void *, IVP_Environment *environment,
    IVP_Time time) {
    environment->current_time = time;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorNew, &AllocateNode),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorDelete, &DeleteNode),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Allocate, &AllocateText),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Free, &DeleteText),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::DrawVectorDebugDestruct,
        &DestructDrawVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::EnvironmentDeleteDrawVectors,
        &DeleteDrawVectors),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MinListRemove, &RemoveMinListElement),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TimeManagerSetEnvironmentTime,
        &SetEnvironmentTime),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpEnvironmentTimeRebase,
     DebugVectorsPreserveListOrderAndRetailAllocationOwnership) {
    std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    const IVP_U_Point firstStart(1.0, 2.0, 3.0);
    const IVP_U_Float_Point firstDirection(4.0f, 5.0f, 6.0f);
    const IVP_U_Point secondStart(-1.0, -2.0, -3.0);
    const IVP_U_Float_Point secondDirection(-4.0f, -5.0f, -6.0f);

    g_nodeAllocations.clear();
    g_nodeDeallocations.clear();
    g_textAllocations.clear();
    g_textDeallocations.clear();

    environment->add_draw_vector(
        &firstStart, &firstDirection, "gravity", 0x112233);
    environment->add_draw_vector(
        &secondStart, &secondDirection, "contact normal", 0x445566);

    IVP_Draw_Vector_Debug *head = environment->draw_vectors;
    ASSERT_NE(head, nullptr);
    EXPECT_DOUBLE_EQ(head->first_point.k[0], -1.0);
    EXPECT_DOUBLE_EQ(head->direction_vec.k[2], -6.0);
    EXPECT_STREQ(head->debug_text, "contact normal");
    EXPECT_EQ(head->color, 0x445566);
    ASSERT_NE(head->next, nullptr);
    EXPECT_DOUBLE_EQ(head->next->first_point.k[1], 2.0);
    EXPECT_DOUBLE_EQ(head->next->direction_vec.k[0], 4.0);
    EXPECT_STREQ(head->next->debug_text, "gravity");
    EXPECT_EQ(head->next->color, 0x112233);
    EXPECT_EQ(head->next->next, nullptr);

    environment->delete_draw_vector_debug();

    EXPECT_EQ(environment->draw_vectors, nullptr);
    EXPECT_EQ(g_nodeAllocations.size(), 2u);
    EXPECT_EQ(g_textAllocations.size(), 2u);
    EXPECT_EQ(g_nodeDeallocations.size(), 2u);
    EXPECT_EQ(g_textDeallocations.size(), 2u);
}

TEST(IvpEnvironmentTimeRebase,
     AlternateEventManagerConsumesExactlyOneQueuedEventPerAdvance) {
    std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    std::array<std::byte, sizeof(IVP_Time_Manager)> timeManagerStorage{};
    std::array<std::byte, sizeof(IVP_U_Min_List)> minListStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *timeManager = reinterpret_cast<IVP_Time_Manager *>(
        timeManagerStorage.data());
    auto *minList = reinterpret_cast<IVP_U_Min_List *>(minListStorage.data());

    RecordingTimeEvent firstEvent;
    RecordingTimeEvent secondEvent;
    std::array<IVP_U_Min_List_Element, 2> elements{};
    elements[0].next = 1;
    elements[0].prev = IVP_U_MINLIST_UNUSED;
    elements[0].value = 2.0f;
    elements[0].element = &firstEvent;
    elements[1].next = IVP_U_MINLIST_UNUSED;
    elements[1].prev = 0;
    elements[1].value = 4.0f;
    elements[1].element = &secondEvent;
    firstEvent.index = 0;
    secondEvent.index = 1;

    auto *rawMinList = reinterpret_cast<RawMinList *>(minList);
    rawMinList->mallocedSize = 2;
    rawMinList->elements = elements.data();
    rawMinList->minValue = 2.0f;
    rawMinList->firstElement = 0;
    rawMinList->firstLong = IVP_U_MINLIST_UNUSED;
    rawMinList->counter = 2;

    IVP_Event_Manager_D oneEventPolicy;
    timeManager->event_manager = &oneEventPolicy;
    timeManager->min_hash = minList;
    timeManager->base_time = IVP_Time(10.0);
    timeManager->last_time = 0.0;
    environment->current_time = IVP_Time(10.0);

    IVP_Event_Manager *policy = &oneEventPolicy;
    policy->simulate_time_events(
        timeManager, environment, IVP_Time(15.0));
    EXPECT_EQ(firstEvent.callCount, 1);
    EXPECT_DOUBLE_EQ(firstEvent.observedTime.get_time(), 12.0);
    EXPECT_EQ(secondEvent.callCount, 0);
    EXPECT_EQ(firstEvent.index, IVP_U_MINLIST_UNUSED);
    EXPECT_EQ(rawMinList->counter, 1);
    EXPECT_EQ(rawMinList->firstElement, 1);
    EXPECT_DOUBLE_EQ(timeManager->last_time, 2.0);
    EXPECT_DOUBLE_EQ(environment->current_time.get_time(), 15.0);

    policy->simulate_time_events(
        timeManager, environment, IVP_Time(16.0));
    EXPECT_EQ(secondEvent.callCount, 1);
    EXPECT_DOUBLE_EQ(secondEvent.observedTime.get_time(), 14.0);
    EXPECT_EQ(secondEvent.index, IVP_U_MINLIST_UNUSED);
    EXPECT_EQ(rawMinList->counter, 0);
    EXPECT_EQ(rawMinList->firstElement, IVP_U_MINLIST_UNUSED);
    EXPECT_DOUBLE_EQ(timeManager->last_time, 4.0);
    EXPECT_DOUBLE_EQ(environment->current_time.get_time(), 16.0);
}

TEST(IvpEnvironmentTimeRebase,
     PreservesRelativeEventAndBodyTimesWhileReturningClockNearZero) {
    std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    std::array<std::byte, sizeof(IVP_Time_Manager)> timeManagerStorage{};
    std::array<std::byte, sizeof(IVP_Sim_Units_Manager)> simManagerStorage{};
    std::array<std::byte, sizeof(IVP_Simulation_Unit)> simulationUnitStorage{};
    std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    std::array<std::byte, sizeof(IVP_U_Min_List)> minListStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *timeManager = reinterpret_cast<IVP_Time_Manager *>(
        timeManagerStorage.data());
    auto *simManager = reinterpret_cast<IVP_Sim_Units_Manager *>(
        simManagerStorage.data());
    auto *simulationUnit = reinterpret_cast<IVP_Simulation_Unit *>(
        simulationUnitStorage.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    auto *minList = reinterpret_cast<IVP_U_Min_List *>(minListStorage.data());

    IVP_U_Min_List_Element eventElement{};
    eventElement.next = IVP_U_MINLIST_UNUSED;
    eventElement.value = 17.0f;
    auto *rawMinList = reinterpret_cast<RawMinList *>(minList);
    rawMinList->elements = &eventElement;
    rawMinList->minValue = 17.0f;
    rawMinList->firstElement = 0;
    rawMinList->firstLong = IVP_U_MINLIST_UNUSED;
    rawMinList->counter = 1;

    timeManager->min_hash = minList;
    timeManager->base_time = IVP_Time(90.0);
    timeManager->last_time = 5.0;

    TimeAwareController controller;
    ControllerEntry controllerEntry{&controller};
    std::array<void *, 1> controllerEntries{&controllerEntry};
    std::array<void *, 1> cores{core};
    auto *unitCoreVector = &simulationUnit->sim_unit_cores;
    unitCoreVector->memsize = 1;
    unitCoreVector->n_elems = 1;
    unitCoreVector->elems = cores.data();
    auto *unitControllerVector = &simulationUnit->controller_cores;
    unitControllerVector->memsize = 1;
    unitControllerVector->n_elems = 1;
    unitControllerVector->elems = controllerEntries.data();
    simulationUnit->next_sim_unit = nullptr;
    simManager->sim_units_slots[0] = simulationUnit;

    std::array<void *, 1> objects{object};
    core->objects.memsize = 1;
    core->objects.n_elems = 1;
    core->objects.elems = objects.data();
    core->time_of_last_psi = IVP_Time(115.0);
    object->hull_manager.last_vpsi_time = IVP_Time(118.0);

    environment->time_manager = timeManager;
    environment->sim_units_manager = simManager;
    environment->current_time = IVP_Time(120.0);
    environment->time_of_last_psi = IVP_Time(100.0);
    environment->time_of_next_psi = IVP_Time(100.01);
    environment->delta_PSI_time = 0.01;
    environment->current_time_code = 7;

    environment->reset_time();

    EXPECT_FLOAT_EQ(eventElement.value, 7.0f);
    EXPECT_FLOAT_EQ(rawMinList->minValue, 7.0f);
    EXPECT_DOUBLE_EQ(timeManager->base_time.get_time(), 0.0);
    EXPECT_DOUBLE_EQ(timeManager->last_time, 0.0);
    EXPECT_EQ(controller.resetCount, 1);
    EXPECT_DOUBLE_EQ(controller.lastOffset.get_time(), 100.0);
    EXPECT_DOUBLE_EQ(core->time_of_last_psi.get_time(), 15.0);
    EXPECT_DOUBLE_EQ(object->hull_manager.last_vpsi_time.get_time(), 18.0);
    EXPECT_DOUBLE_EQ(environment->current_time.get_time(), 20.0);
    EXPECT_DOUBLE_EQ(environment->time_of_last_psi.get_time(), 0.0);
    EXPECT_NEAR(environment->time_of_next_psi.get_time(), 0.01, 1.0e-7);
    EXPECT_EQ(environment->current_time_code, 8);
}

} // namespace
