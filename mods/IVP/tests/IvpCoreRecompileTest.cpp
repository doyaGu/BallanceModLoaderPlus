#include "IvpTestAdapter.h"

#include "BML/IVP/Environment.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

int g_calculateCount = 0;
int g_ensureSimulationCount = 0;
int g_freeTransactionCount = 0;
int g_recalculateCount = 0;
int g_readMaterialsCount = 0;
int g_virtualMassCount = 0;
int g_fusionCount = 0;
int g_reviveObjectCount = 0;
int g_destroyContactCount = 0;
int g_deleteContactCount = 0;
void *g_expectedFirstSystem = nullptr;
void *g_expectedSecondSystem = nullptr;
IVP_Real_Object *g_expectedRevivedObject = nullptr;
IVP_Contact_Point *g_expectedDestroyedContact = nullptr;

void __fastcall CalculateRedundantValues(IVP_Core *core, void *) {
    ++g_calculateCount;
    core->inv_rot_inertia.set(0.5f, 0.25f, 0.125f);
    core->inv_rot_inertia.hesse_val = 0.1f;
}

void __fastcall EnsureSimulation(IVP_Core *, void *) {
    ++g_ensureSimulationCount;
}

void __fastcall ReviveObject(
    IVP_Real_Object *object, void *) {
    EXPECT_EQ(object, g_expectedRevivedObject);
    ++g_reviveObjectCount;
}

void __fastcall FreeMemoryTransaction(IVP_U_Memory *, void *) {
    ++g_freeTransactionCount;
}

void __fastcall RecalculateContact(IVP_Contact_Point *, void *) {
    ++g_recalculateCount;
}

void __fastcall ReadContactMaterials(
    IVP_Contact_Point *contactPoint, void *, void *temporaryInfo) {
    EXPECT_EQ(temporaryInfo,
              BML::IVP::Detail::ContactPointTemporaryInfo(contactPoint));
    ++g_readMaterialsCount;
}

void __fastcall CalculateContactVirtualMass(
    IVP_Contact_Point *, void *) {
    ++g_virtualMassCount;
}

void __fastcall DestroyContact(
    IVP_Contact_Point *contactPoint, void *) {
    EXPECT_EQ(contactPoint, g_expectedDestroyedContact);
    ++g_destroyContactCount;
}

void __cdecl DeleteContact(void *contactPoint) {
    EXPECT_EQ(contactPoint, g_expectedDestroyedContact);
    ++g_deleteContactCount;
}

void __fastcall FuseFrictionSystems(
    void *firstSystem, void *, void *secondSystem) {
    EXPECT_EQ(firstSystem, g_expectedFirstSystem);
    EXPECT_EQ(secondSystem, g_expectedSecondSystem);
    ++g_fusionCount;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreCalculateRedundantValues, &CalculateRedundantValues),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreEnsureSimulation, &EnsureSimulation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectEnsureSimulation, &ReviveObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MemoryFreeTransaction, &FreeMemoryTransaction),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointRecalculateFriction, &RecalculateContact),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointReadMaterials, &ReadContactMaterials),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointCalculateVirtualMass, &CalculateContactVirtualMass),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointDestruct, &DestroyContact),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::FrictionSystemFusion, &FuseFrictionSystems),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorDelete, &DeleteContact),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


void ResetObservations() {
    g_calculateCount = 0;
    g_ensureSimulationCount = 0;
    g_freeTransactionCount = 0;
    g_recalculateCount = 0;
    g_readMaterialsCount = 0;
    g_virtualMassCount = 0;
    g_fusionCount = 0;
    g_reviveObjectCount = 0;
    g_destroyContactCount = 0;
    g_deleteContactCount = 0;
    g_expectedFirstSystem = nullptr;
    g_expectedSecondSystem = nullptr;
    g_expectedRevivedObject = nullptr;
    g_expectedDestroyedContact = nullptr;
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

TEST(IvpCoreCoordinates,
     ConvertsAWorldSpaceSteeringDirectionIntoTheBallCoreFrame) {
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());

    // A +90 degree world-from-core rotation about Z. A world-space +Y
    // steering direction therefore points along +X in the ball's core frame.
    core->m_world_f_core_last_psi.set_identity();
    core->m_world_f_core_last_psi.set_elem(0, 0, 0.0);
    core->m_world_f_core_last_psi.set_elem(0, 1, -1.0);
    core->m_world_f_core_last_psi.set_elem(1, 0, 1.0);
    core->m_world_f_core_last_psi.set_elem(1, 1, 0.0);

    const IVP_U_Float_Point steeringWorld(0.0f, 1.0f, 0.0f);
    const IVP_Vec_PCore steeringCore(core, &steeringWorld);

    EXPECT_FLOAT_EQ(steeringCore.k[0], 1.0f);
    EXPECT_FLOAT_EQ(steeringCore.k[1], 0.0f);
    EXPECT_FLOAT_EQ(steeringCore.k[2], 0.0f);
}

TEST(IvpCoreRecompile,
     RebuildsMovableCoreAndEveryAttachedContactAfterMaterialChange) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
        environmentStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> otherCoreStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
        otherObjectStorage{};
    alignas(16) std::array<std::byte, 0x80> firstContactStorage{};
    alignas(16) std::array<std::byte, 0x80> secondContactStorage{};
    alignas(16) std::array<std::byte, 0x48> firstSystemStorage{};
    alignas(16) std::array<std::byte, 0x48> secondSystemStorage{};
    alignas(16) std::array<std::byte, 0x14> memoryStorage{};
    std::uint32_t firstTemporaryInfo = 0;
    std::uint32_t secondTemporaryInfo = 0;

    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());
    auto *otherCore = reinterpret_cast<IVP_Core *>(otherCoreStorage.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    auto *otherObject = reinterpret_cast<IVP_Real_Object *>(
        otherObjectStorage.data());
    auto *memory = reinterpret_cast<IVP_U_Memory *>(memoryStorage.data());

    environment->sim_unit_mem = memory;
    core->environment = environment;
    core->movement_state = IVP_MT_MOVING;
    otherCore->environment = environment;
    object->physical_core = core;
    otherObject->physical_core = otherCore;

    std::array<void *, 1> coreObjects{object};
    core->objects.elems = coreObjects.data();
    core->objects.memsize = 1;
    core->objects.n_elems = 1;

    InitializeContact(firstContactStorage.data(), object, otherObject,
                      firstSystemStorage.data(), &firstTemporaryInfo);
    InitializeContact(secondContactStorage.data(), object, otherObject,
                      secondSystemStorage.data(), &secondTemporaryInfo);
    auto *firstSynapse = reinterpret_cast<IVP_Synapse_Friction *>(
        firstContactStorage.data() + 0x08u);
    auto *secondSynapse = reinterpret_cast<IVP_Synapse_Friction *>(
        secondContactStorage.data() + 0x08u);
    firstSynapse->next = secondSynapse;
    secondSynapse->prev = firstSynapse;
    object->friction_synapses = firstSynapse;

    g_expectedFirstSystem = firstSystemStorage.data();
    g_expectedSecondSystem = secondSystemStorage.data();

    object->recompile_material_changed();

    EXPECT_EQ(g_calculateCount, 1);
    EXPECT_FLOAT_EQ(core->inv_rot_inertia.k[0], 0.5f);
    EXPECT_FLOAT_EQ(core->inv_rot_inertia.hesse_val, 0.1f);
    auto *fastStatic = static_cast<IVP_Core_Fast_Static *>(core);
    EXPECT_FLOAT_EQ(fastStatic->get_mass(), core->rot_inertia.hesse_val);
    EXPECT_FLOAT_EQ(fastStatic->get_inv_mass(), 0.1f);
    EXPECT_EQ(fastStatic->get_rot_inertia(), &core->rot_inertia);
    EXPECT_EQ(fastStatic->get_inv_rot_inertia(), &core->inv_rot_inertia);
    EXPECT_EQ(reinterpret_cast<const void *>(fastStatic->get_inv_masses()),
              static_cast<const void *>(&core->inv_rot_inertia));
    EXPECT_EQ(core->movement_state, IVP_MT_MOVING);
    EXPECT_EQ(object->get_movement_state(), IVP_MT_MOVING);
    EXPECT_EQ(g_ensureSimulationCount, 1);
    EXPECT_EQ(g_fusionCount, 1);
    EXPECT_EQ(g_recalculateCount, 2);
    EXPECT_EQ(g_readMaterialsCount, 2);
    EXPECT_EQ(g_virtualMassCount, 2);
    EXPECT_EQ(g_freeTransactionCount, 2);
    EXPECT_EQ(*reinterpret_cast<std::int16_t *>(
                  memoryStorage.data() + 0x10u),
              0);
}

TEST(IvpCoreRecompile,
     RevivesMovableNeighborsWhenStaticContactTopologyChanges) {
    ResetObservations();

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
    auto *memory = reinterpret_cast<IVP_U_Memory *>(memoryStorage.data());

    environment->sim_unit_mem = memory;
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
    g_expectedRevivedObject = movingObject;

    staticCore->revive_adjacent_to_unmoveable();

    EXPECT_EQ(g_reviveObjectCount, 1);
    EXPECT_EQ(*reinterpret_cast<std::uint8_t *>(
                  frictionSystemStorage.data() + 0x44u),
              static_cast<std::uint8_t>(IVP_TRUE));
    EXPECT_EQ(g_recalculateCount, 1);
    EXPECT_EQ(g_readMaterialsCount, 1);
    EXPECT_EQ(g_virtualMassCount, 1);
    EXPECT_EQ(g_freeTransactionCount, 1);
    EXPECT_EQ(*reinterpret_cast<std::int16_t *>(
                  memoryStorage.data() + 0x10u),
              0);
}

TEST(IvpCoreRecompile,
     RemovesStaticToStaticContactsWithoutRevivingEitherCore) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
        environmentStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> firstCoreStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> secondCoreStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
        firstObjectStorage{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
        secondObjectStorage{};
    alignas(16) std::array<std::byte, 0x80> contactStorage{};
    alignas(16) std::array<std::byte, 0x48> frictionSystemStorage{};
    alignas(16) std::array<std::byte, 0x14> memoryStorage{};
    std::uint32_t temporaryInfo = 0;

    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *firstCore = reinterpret_cast<IVP_Core *>(firstCoreStorage.data());
    auto *secondCore = reinterpret_cast<IVP_Core *>(secondCoreStorage.data());
    auto *firstObject = reinterpret_cast<IVP_Real_Object *>(
        firstObjectStorage.data());
    auto *secondObject = reinterpret_cast<IVP_Real_Object *>(
        secondObjectStorage.data());
    environment->sim_unit_mem = reinterpret_cast<IVP_U_Memory *>(
        memoryStorage.data());
    firstCore->environment = environment;
    firstCore->flags = 0x04u;
    secondCore->environment = environment;
    secondCore->flags = 0x04u;
    firstObject->physical_core = firstCore;
    secondObject->physical_core = secondCore;

    std::array<void *, 1> firstObjects{firstObject};
    firstCore->objects.elems = firstObjects.data();
    firstCore->objects.memsize = 1;
    firstCore->objects.n_elems = 1;
    InitializeContact(contactStorage.data(), firstObject, secondObject,
                      frictionSystemStorage.data(), &temporaryInfo);
    firstObject->friction_synapses =
        reinterpret_cast<IVP_Synapse_Friction *>(
            contactStorage.data() + 0x08u);
    g_expectedDestroyedContact =
        reinterpret_cast<IVP_Contact_Point *>(contactStorage.data());

    firstCore->revive_adjacent_to_unmoveable();

    EXPECT_EQ(g_destroyContactCount, 1);
    EXPECT_EQ(g_deleteContactCount, 1);
    EXPECT_EQ(g_reviveObjectCount, 0);
    EXPECT_EQ(g_recalculateCount, 0);
    EXPECT_EQ(g_readMaterialsCount, 0);
    EXPECT_EQ(g_virtualMassCount, 0);
    EXPECT_EQ(g_freeTransactionCount, 1);
    EXPECT_EQ(*reinterpret_cast<std::uint8_t *>(
                  frictionSystemStorage.data() + 0x44u),
              static_cast<std::uint8_t>(IVP_TRUE));
}

} // namespace
