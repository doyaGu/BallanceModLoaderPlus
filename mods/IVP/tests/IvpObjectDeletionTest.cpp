#include "IvpTestAdapter.h"

#include "BML/IVP/Environment.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

enum class Operation {
    EnsureCore,
    DiscoverNearMindists,
    GrowFrictionSystem,
    ReviveNeighborObject,
    RecalculateContact,
    ReadContactMaterials,
    CalculateContactVirtualMass,
    FreeMemoryTransaction,
    DestroyObject,
};

std::vector<Operation> g_operations;
IVP_Real_Object *g_expectedObject = nullptr;
IVP_Real_Object *g_expectedNeighbor = nullptr;

void __fastcall EnsureCore(IVP_Core *, void *) {
    g_operations.push_back(Operation::EnsureCore);
}

void __fastcall DiscoverNearMindists(IVP_Real_Object *object, void *) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::DiscoverNearMindists);
}

IVP_BOOL __fastcall GrowFrictionSystem(IVP_Core *, void *) {
    g_operations.push_back(Operation::GrowFrictionSystem);
    return IVP_TRUE;
}

void __fastcall ReviveNeighborObject(IVP_Real_Object *object, void *) {
    EXPECT_EQ(object, g_expectedNeighbor);
    g_operations.push_back(Operation::ReviveNeighborObject);
}

void __fastcall RecalculateContact(IVP_Contact_Point *, void *) {
    g_operations.push_back(Operation::RecalculateContact);
}

void __fastcall ReadContactMaterials(
    IVP_Contact_Point *contactPoint, void *, void *temporaryInfo) {
    EXPECT_EQ(temporaryInfo,
              BML::IVP::Detail::ContactPointTemporaryInfo(contactPoint));
    g_operations.push_back(Operation::ReadContactMaterials);
}

void __fastcall CalculateContactVirtualMass(
    IVP_Contact_Point *, void *) {
    g_operations.push_back(Operation::CalculateContactVirtualMass);
}

void __fastcall FreeMemoryTransaction(IVP_U_Memory *, void *) {
    g_operations.push_back(Operation::FreeMemoryTransaction);
}

void __fastcall DestroyObject(IVP_Real_Object *object, void *) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::DestroyObject);
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreEnsureSimulation, &EnsureCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectGetAllNearMindists, &DiscoverNearMindists),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreGrowFrictionSystem, &GrowFrictionSystem),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectEnsureSimulation, &ReviveNeighborObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointRecalculateFriction, &RecalculateContact),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointReadMaterials, &ReadContactMaterials),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointCalculateVirtualMass, &CalculateContactVirtualMass),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MemoryFreeTransaction, &FreeMemoryTransaction),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectDestroy, &DestroyObject),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


void InitializeContact(
    std::byte *storage, IVP_Real_Object *firstObject,
    IVP_Real_Object *secondObject, void *frictionSystem,
    void *temporaryInfo) {
    auto *contactPoint = reinterpret_cast<IVP_Contact_Point *>(storage);
    auto *firstSynapse = reinterpret_cast<IVP_Synapse_Friction *>(
        storage + 0x08u);
    auto *secondSynapse = reinterpret_cast<IVP_Synapse_Friction *>(
        storage + 0x1Cu);
    firstSynapse->next = nullptr;
    firstSynapse->prev = nullptr;
    firstSynapse->l_obj = firstObject;
    firstSynapse->set_contact_point(contactPoint);
    secondSynapse->next = nullptr;
    secondSynapse->prev = nullptr;
    secondSynapse->l_obj = secondObject;
    secondSynapse->set_contact_point(contactPoint);
    *reinterpret_cast<void **>(storage + 0x40u) = temporaryInfo;
    *reinterpret_cast<void **>(storage + 0x70u) = frictionSystem;
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpObjectDeletion,
     WakesMovableSimulationUnitBeforeVirtualDestruction) {
    g_operations.clear();

    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
        objectStorage{};
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(
        objectStorage.data());
    object->physical_core = core;
    g_expectedObject = object;
    g_expectedNeighbor = nullptr;

    object->delete_and_check_vicinity();

    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::EnsureCore,
                  Operation::EnsureCore,
                  Operation::DestroyObject,
              }));
}

TEST(IvpObjectDeletion,
     RebuildsStaticVicinityBeforeVirtualDestruction) {
    g_operations.clear();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
        environmentStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> staticCoreStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> movingCoreStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
        staticObjectStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
        movingObjectStorage{};
    alignas(16) std::array<std::byte, 0x80> contactStorage{};
    alignas(16) std::array<std::byte, 0x48> frictionSystemStorage{};
    alignas(16) std::array<std::byte, 0x14> memoryStorage{};
    std::uint32_t temporaryInfo = 0;

    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *staticCore = reinterpret_cast<IVP_Core *>(
        staticCoreStorage.data());
    auto *movingCore = reinterpret_cast<IVP_Core *>(
        movingCoreStorage.data());
    auto *staticObject = reinterpret_cast<IVP_Real_Object *>(
        staticObjectStorage.data());
    auto *movingObject = reinterpret_cast<IVP_Real_Object *>(
        movingObjectStorage.data());

    environment->sim_unit_mem = reinterpret_cast<IVP_U_Memory *>(
        memoryStorage.data());
    staticCore->environment = environment;
    staticCore->flags = 0x04u;
    movingCore->environment = environment;
    staticObject->physical_core = staticCore;
    movingObject->physical_core = movingCore;

    std::array<void *, 1> staticObjects{staticObject};
    staticCore->objects.elems = staticObjects.data();
    staticCore->objects.memsize = 1;
    staticCore->objects.n_elems = 1;
    std::array<void *, 1> movingObjects{movingObject};
    movingCore->objects.elems = movingObjects.data();
    movingCore->objects.memsize = 1;
    movingCore->objects.n_elems = 1;

    InitializeContact(contactStorage.data(), staticObject, movingObject,
                      frictionSystemStorage.data(), &temporaryInfo);
    staticObject->friction_synapses =
        reinterpret_cast<IVP_Synapse_Friction *>(
            contactStorage.data() + 0x08u);
    g_expectedObject = staticObject;
    g_expectedNeighbor = movingObject;

    staticObject->delete_and_check_vicinity();

    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::DiscoverNearMindists,
                  Operation::GrowFrictionSystem,
                  Operation::ReviveNeighborObject,
                  Operation::RecalculateContact,
                  Operation::ReadContactMaterials,
                  Operation::CalculateContactVirtualMass,
                  Operation::FreeMemoryTransaction,
                  Operation::DestroyObject,
              }));
    EXPECT_EQ(*reinterpret_cast<std::int16_t *>(
                  memoryStorage.data() + 0x10u),
              0);
    EXPECT_EQ(*reinterpret_cast<std::uint8_t *>(
                  frictionSystemStorage.data() + 0x44u),
              static_cast<std::uint8_t>(IVP_TRUE));
}

} // namespace
