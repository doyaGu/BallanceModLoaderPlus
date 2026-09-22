#include "IvpTestAdapter.h"

#include "BML/IVP/Environment.h"
#include "BML/IVP/Friction.h"
#include "BML/IVP/FrictionSolver.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

struct ContactFixture {
  IVP_Synapse_Friction synapse{};
  alignas(IVP_Contact_Point)
      std::array<std::byte, sizeof(IVP_Contact_Point)> contactStorage{};

  IVP_Contact_Point *contact() {
    return reinterpret_cast<IVP_Contact_Point *>(contactStorage.data());
  }
};

std::array<IVP_Contact_Point *, 4> g_deletedContacts{};
int g_deletedContactCount = 0;
void *g_destroyedSystem = nullptr;
int g_constraint2DCalls = 0;
int g_constraint1DCalls = 0;

std::int16_t &DistanceCount(IVP_Friction_System *system) {
  return *reinterpret_cast<std::int16_t *>(
      reinterpret_cast<std::byte *>(system) + 0x3Eu);
}

void SetContactObjects(IVP_Contact_Point *contact, IVP_Real_Object *first,
                       IVP_Real_Object *second) {
  *reinterpret_cast<IVP_Real_Object **>(reinterpret_cast<std::byte *>(contact) +
                                        0x10u) = first;
  *reinterpret_cast<IVP_Real_Object **>(reinterpret_cast<std::byte *>(contact) +
                                        0x24u) = second;
}

void SetContactSystem(IVP_Contact_Point *contact, IVP_Friction_System *system) {
  *reinterpret_cast<IVP_Friction_System **>(
      reinterpret_cast<std::byte *>(contact) + 0x70u) = system;
}

void __fastcall DeleteFrictionDistance(IVP_Friction_System *system, void *,
                                       IVP_Contact_Point *contact) {
  ASSERT_LT(g_deletedContactCount, static_cast<int>(g_deletedContacts.size()));
  g_deletedContacts[g_deletedContactCount++] = contact;
  --DistanceCount(system);
}

void *__fastcall DestroyFrictionSystem(IVP_Friction_System *system, void *,
                                       unsigned int flags) {
  EXPECT_EQ(flags, 1u);
  g_destroyedSystem = system;
  return system;
}

void __fastcall ConstructFrictionCorePair(IVP_Friction_Core_Pair *pair,
                                          void *) {
  auto *bytes = reinterpret_cast<std::byte *>(pair);
  *reinterpret_cast<std::uint16_t *>(bytes + 0x00u) = 0;
  *reinterpret_cast<std::uint16_t *>(bytes + 0x02u) = 0;
  *reinterpret_cast<void ***>(bytes + 0x04u) = nullptr;
  *reinterpret_cast<int *>(bytes + 0x18u) = 1;
  *reinterpret_cast<IVP_Time *>(bytes + 0x20u) = IVP_Time(-1000.0);
  *reinterpret_cast<IVP_FLOAT *>(bytes + 0x28u) = 0.0f;
}

int __fastcall CountFrictionCorePairDistances(IVP_Friction_Core_Pair *pair,
                                              void *) {
  return reinterpret_cast<IVP_U_Vector_Base *>(pair)->n_elems;
}

void __fastcall RemoveFrictionCorePairDistance(IVP_Friction_Core_Pair *pair,
                                               void *,
                                               IVP_Contact_Point *contact) {
  auto *vector = reinterpret_cast<IVP_U_Vector_Base *>(pair);
  int found = -1;
  for (int index = static_cast<int>(vector->n_elems) - 1; index >= 0; --index) {
    if (vector->elems[index] == contact) {
      found = index;
      break;
    }
  }
  ASSERT_GE(found, 0);
  --vector->n_elems;
  for (int index = found; index < vector->n_elems; ++index)
    vector->elems[index] = vector->elems[index + 1];
}

IVP_FLOAT __cdecl InverseSqrtFloat(IVP_FLOAT squaredLength) {
  return 1.0f / std::sqrt(squaredLength);
}

IVP_FLOAT __fastcall FrictionConstraint2D(IVP_Contact_Point *, void *,
                                          const IVP_Event_Sim *) {
  ++g_constraint2DCalls;
  return 0.75f;
}

void __fastcall FrictionConstraint1D(IVP_Contact_Point *, void *,
                                     const IVP_Event_Sim *) {
  ++g_constraint1DCalls;
}

IVP_DOUBLE __cdecl CalculateMutualEnergyPotential(
    IVP_DOUBLE speedPotential, IVP_DOUBLE firstMass, IVP_DOUBLE secondMass,
    IVP_DOUBLE firstInverseMass, IVP_DOUBLE secondInverseMass) {
  const IVP_DOUBLE impulse =
      speedPotential / (firstInverseMass + secondInverseMass);
  const IVP_DOUBLE secondResidual =
      speedPotential - impulse * secondInverseMass;
  const IVP_DOUBLE firstResidual = impulse * firstInverseMass;
  return 0.5 *
         (secondMass * speedPotential * speedPotential -
          firstMass * firstResidual * firstResidual -
          secondMass * secondResidual * secondResidual);
}

IVP_DOUBLE __cdecl GetClosingSpeedCore(
    const IVP_Impact_Solver_Long_Term *information, int coreIndex,
    const IVP_U_Float_Point *rotationalSpeed,
    IVP_U_Float_Point *linearSpeed) {
  return rotationalSpeed->dot_product(
             &information->contact_cross_nomal_cs[coreIndex]) +
         linearSpeed->dot_product(&information->surf_normal);
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::FrictionSystemDeleteDistance,
                         &DeleteFrictionDistance),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::FrictionSystemScalarDeletingDestructor,
        &DestroyFrictionSystem),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::FrictionCorePairConstruct,
                         &ConstructFrictionCorePair),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::FrictionCorePairDistanceCount,
                         &CountFrictionCorePairDistances),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::FrictionCorePairRemoveDistance,
                         &RemoveFrictionCorePairDistance),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::InlineMathInverseSqrtFloat,
                         &InverseSqrtFloat),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointFrictionConstraint2D,
        &FrictionConstraint2D),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ContactPointFrictionConstraint1D,
        &FrictionConstraint1D),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MutualEnergizerCalculatePotential,
        &CalculateMutualEnergyPotential),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::FrictionSolverGetClosingSpeedCore,
        &GetClosingSpeedCore),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
  return BML::IVP::Test::Resolve(rva, kRetailCalls);
}

void ResetObservations() {
  g_deletedContacts.fill(nullptr);
  g_deletedContactCount = 0;
  g_destroyedSystem = nullptr;
  g_constraint2DCalls = 0;
  g_constraint1DCalls = 0;
}

template <typename T>
T &ContactField(IVP_Contact_Point *contact, std::size_t offset) {
  return *reinterpret_cast<T *>(reinterpret_cast<std::byte *>(contact) +
                                offset);
}

struct ContactFrictionFixture {
  ContactFixture contact;
  alignas(8) std::array<std::byte, 0xE0u> longTermInformation{};

  ContactFrictionFixture(IVP_Real_Object *firstObject,
                         const IVP_U_Float_Point &firstSpan,
                         const IVP_U_Float_Point &secondSpan,
                         IVP_FLOAT firstCoordinate,
                         IVP_FLOAT secondCoordinate) {
    contact.contact()->get_synapse(0)->l_obj = firstObject;
    ContactField<std::byte *>(contact.contact(), 0x40u) =
        longTermInformation.data();
    *reinterpret_cast<IVP_U_Float_Point *>(longTermInformation.data() + 0x80u) =
        firstSpan;
    *reinterpret_cast<IVP_U_Float_Point *>(longTermInformation.data() + 0x90u) =
        secondSpan;
    ContactField<IVP_FLOAT>(contact.contact(), 0x38u) = firstCoordinate;
    ContactField<IVP_FLOAT>(contact.contact(), 0x3Cu) = secondCoordinate;
  }
};

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(std::uint32_t rva) noexcept {
  return ResolveRetailCall(rva);
}

namespace {

TEST(IvpContactTopology,
     CollisionCallbackReadsAndResetsTheBallContactTelemetry) {
  ContactFixture fixture;
  IVP_Impact_Solver_Long_Term contactInformation{};
  contactInformation.surf_normal.set(0.0f, 1.0f, 0.0f);
  ContactField<IVP_Impact_Solver_Long_Term *>(fixture.contact(), 0x40u) =
      &contactInformation;
  ContactField<IVP_FLOAT>(fixture.contact(), 0x48u) = 12.5f;
  ContactField<IVP_FLOAT>(fixture.contact(), 0x54u) = 320.0f;

  IVP_U_Float_Point normal;
  IVP_Contact_Point_API::get_surface_normal_ws(fixture.contact(), &normal);

  EXPECT_FLOAT_EQ(
      IVP_Contact_Point_API::get_eliminated_energy(fixture.contact()),
      12.5f);
  EXPECT_FLOAT_EQ(IVP_Contact_Point_API::get_vert_force(fixture.contact()),
                  320.0f);
  EXPECT_FLOAT_EQ(normal.k[0], 0.0f);
  EXPECT_FLOAT_EQ(normal.k[1], 1.0f);
  EXPECT_FLOAT_EQ(normal.k[2], 0.0f);

  IVP_Contact_Point_API::reset_eliminated_energy(fixture.contact());
  EXPECT_FLOAT_EQ(
      IVP_Contact_Point_API::get_eliminated_energy(fixture.contact()), 0.0f);
  EXPECT_FLOAT_EQ(IVP_Contact_Point_API::get_vert_force(fixture.contact()),
                  320.0f);
}

TEST(IvpContactTopology,
     RemovingOneObjectPairPreservesTheUnrelatedFloorContact) {
  ResetObservations();

  std::array<std::byte, sizeof(IVP_Real_Object)> movingStorage{};
  std::array<std::byte, sizeof(IVP_Real_Object)> oldPlatformStorage{};
  std::array<std::byte, sizeof(IVP_Real_Object)> floorStorage{};
  auto *moving = reinterpret_cast<IVP_Real_Object *>(movingStorage.data());
  auto *oldPlatform =
      reinterpret_cast<IVP_Real_Object *>(oldPlatformStorage.data());
  auto *floor = reinterpret_cast<IVP_Real_Object *>(floorStorage.data());
  ContactFixture oldPlatformContact;
  ContactFixture floorContact;
  alignas(IVP_Friction_System)
      std::array<std::byte, sizeof(IVP_Friction_System)>
          sharedSystem{};
  auto *system = reinterpret_cast<IVP_Friction_System *>(sharedSystem.data());

  oldPlatformContact.synapse.next = &floorContact.synapse;
  floorContact.synapse.next = nullptr;
  moving->friction_synapses = &oldPlatformContact.synapse;
  oldPlatformContact.synapse.set_contact_point(oldPlatformContact.contact());
  floorContact.synapse.set_contact_point(floorContact.contact());
  SetContactObjects(oldPlatformContact.contact(), moving, oldPlatform);
  SetContactObjects(floorContact.contact(), moving, floor);
  SetContactSystem(oldPlatformContact.contact(), system);
  SetContactSystem(floorContact.contact(), system);
  DistanceCount(system) = 2;

  moving->unlink_contact_points_for_object(oldPlatform);

  ASSERT_EQ(g_deletedContactCount, 1);
  EXPECT_EQ(g_deletedContacts[0], oldPlatformContact.contact());
  EXPECT_EQ(system->get_fr_dist_number(), 1);
  EXPECT_EQ(g_destroyedSystem, nullptr);
}

TEST(IvpContactTopology, RemovingTheLastPairTearsDownItsEmptyFrictionSystem) {
  ResetObservations();

  std::array<std::byte, sizeof(IVP_Real_Object)> movingStorage{};
  std::array<std::byte, sizeof(IVP_Real_Object)> platformStorage{};
  auto *moving = reinterpret_cast<IVP_Real_Object *>(movingStorage.data());
  auto *platform = reinterpret_cast<IVP_Real_Object *>(platformStorage.data());
  ContactFixture contact;
  alignas(IVP_Friction_System)
      std::array<std::byte, sizeof(IVP_Friction_System)>
          frictionSystem{};
  auto *system = reinterpret_cast<IVP_Friction_System *>(frictionSystem.data());

  contact.synapse.next = nullptr;
  moving->friction_synapses = &contact.synapse;
  contact.synapse.set_contact_point(contact.contact());
  SetContactObjects(contact.contact(), moving, platform);
  SetContactSystem(contact.contact(), system);
  DistanceCount(system) = 1;

  moving->unlink_contact_points_for_object(platform);

  ASSERT_EQ(g_deletedContactCount, 1);
  EXPECT_EQ(g_deletedContacts[0], contact.contact());
  EXPECT_EQ(system->get_fr_dist_number(), 0);
  EXPECT_EQ(g_destroyedSystem, system);
}

TEST(IvpContactTopology,
     RetailContactAndPairStateUseTheConfirmedBallanceLayout) {
  ContactFixture contact;
  auto *bytes = reinterpret_cast<std::byte *>(contact.contact());
  *reinterpret_cast<IVP_FLOAT *>(bytes + 0x38u) = 0.5f;
  *reinterpret_cast<IVP_FLOAT *>(bytes + 0x3Cu) = 0.25f;

  EXPECT_FLOAT_EQ(contact.contact()->get_friction_factor(), 0.125f);
  contact.contact()->set_friction_to_neutral();
  EXPECT_FLOAT_EQ(contact.contact()->get_friction_factor(), 0.0f);

  IVP_Friction_Core_Pair pair;
  EXPECT_EQ(pair.number_of_pair_dists(), 0);
  EXPECT_EQ(pair.next_ease_nr_psi, 1);
  EXPECT_DOUBLE_EQ(pair.last_impact_time_pair.get_seconds(), -1000.0);
  EXPECT_FLOAT_EQ(pair.integrated_anti_energy, 0.0f);

  void *pairContacts[1]{};
  auto *pairVector = pair.get_contact_points();
  pairVector->elems = pairContacts;
  pairVector->memsize = 1;
  pair.add_fr_dist_obj_pairs(contact.contact());
  ASSERT_EQ(pair.number_of_pair_dists(), 1);
  EXPECT_EQ(pair.get_contact_points()->element_at(0), contact.contact());
  pair.del_fr_dist_obj_pairs(contact.contact());
  EXPECT_EQ(pair.number_of_pair_dists(), 0);
  pairVector->elems = nullptr;
  pairVector->memsize = 0;
}

TEST(IvpContactTopology,
     PairMembershipDistinguishesConnectedAndDisconnectedCoresForUnionFind) {
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> firstStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> secondStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> thirdStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> isolatedStorage{};
  auto *first = reinterpret_cast<IVP_Core *>(firstStorage.data());
  auto *second = reinterpret_cast<IVP_Core *>(secondStorage.data());
  auto *third = reinterpret_cast<IVP_Core *>(thirdStorage.data());
  auto *isolated = reinterpret_cast<IVP_Core *>(isolatedStorage.data());

  IVP_Friction_Core_Pair firstPair;
  IVP_Friction_Core_Pair secondPair;
  firstPair.objs[0] = first;
  firstPair.objs[1] = second;
  secondPair.objs[0] = second;
  secondPair.objs[1] = third;
  void *pairPointers[] = {&firstPair, &secondPair};

  alignas(IVP_Friction_System)
      std::array<std::byte, sizeof(IVP_Friction_System)> systemStorage{};
  auto *system = reinterpret_cast<IVP_Friction_System *>(systemStorage.data());
  IVP_U_Vector<IVP_Friction_Core_Pair> *pairs = system->get_friction_pairs();
  pairs->elems = pairPointers;
  pairs->memsize = 2;
  pairs->n_elems = 2;

  EXPECT_EQ(system->core_is_found_in_pairs(first), IVP_TRUE);
  EXPECT_EQ(system->core_is_found_in_pairs(second), IVP_TRUE);
  EXPECT_EQ(system->core_is_found_in_pairs(third), IVP_TRUE);
  EXPECT_EQ(system->core_is_found_in_pairs(isolated), IVP_FALSE);

  IVP_U_Vector<IVP_Core> controlled;
  controlled.n_elems = 1;
  system->get_controlled_cores(&controlled);
  EXPECT_EQ(controlled.n_elems, 1);
}

TEST(IvpContactTopology,
     PairCoordinatesPreserveDirectionAcrossOppositelyOrientedContacts) {
  ResetObservations();

  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> coreAStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> coreBStorage{};
  alignas(IVP_Real_Object) std::array<std::byte, sizeof(IVP_Real_Object)>
      objectAStorage{};
  alignas(IVP_Real_Object) std::array<std::byte, sizeof(IVP_Real_Object)>
      objectBStorage{};
  auto *coreA = reinterpret_cast<IVP_Core *>(coreAStorage.data());
  auto *coreB = reinterpret_cast<IVP_Core *>(coreBStorage.data());
  auto *objectA = reinterpret_cast<IVP_Real_Object *>(objectAStorage.data());
  auto *objectB = reinterpret_cast<IVP_Real_Object *>(objectBStorage.data());
  objectA->physical_core = coreA;
  objectB->physical_core = coreB;

  const IVP_U_Float_Point spanX(1.0, 0.0, 0.0);
  const IVP_U_Float_Point spanY(0.0, 1.0, 0.0);
  ContactFrictionFixture sameOrientation(objectA, spanX, spanY, 2.0f, 4.0f);
  ContactFrictionFixture oppositeOrientation(objectB, spanX, spanY, -6.0f,
                                             -8.0f);

  IVP_Friction_Core_Pair pair;
  pair.objs[0] = coreA;
  pair.objs[1] = coreB;
  void *contacts[] = {
      sameOrientation.contact.contact(),
      oppositeOrientation.contact.contact(),
  };
  auto *vector = pair.get_contact_points();
  vector->elems = contacts;
  vector->memsize = 2;
  vector->n_elems = 2;

  IVP_U_Float_Point average;
  pair.get_average_friction_vector(&average);
  EXPECT_FLOAT_EQ(average.k[0], 4.0f);
  EXPECT_FLOAT_EQ(average.k[1], 6.0f);
  EXPECT_FLOAT_EQ(average.k[2], 0.0f);

  IVP_U_Float_Point replacement(3.0, -2.0, 0.0);
  pair.set_friction_vectors(&replacement);
  EXPECT_FLOAT_EQ(
      ContactField<IVP_FLOAT>(sameOrientation.contact.contact(), 0x38u), 3.0f);
  EXPECT_FLOAT_EQ(
      ContactField<IVP_FLOAT>(sameOrientation.contact.contact(), 0x3Cu), -2.0f);
  EXPECT_FLOAT_EQ(
      ContactField<IVP_FLOAT>(oppositeOrientation.contact.contact(), 0x38u),
      -3.0f);
  EXPECT_FLOAT_EQ(
      ContactField<IVP_FLOAT>(oppositeOrientation.contact.contact(), 0x3Cu),
      2.0f);

  ContactField<IVP_FLOAT>(sameOrientation.contact.contact(), 0x30u) = 0.5f;
  ContactField<IVP_FLOAT>(sameOrientation.contact.contact(), 0x44u) = 0.4f;
  ContactField<IVP_FLOAT>(sameOrientation.contact.contact(), 0x54u) = 10.0f;
  ContactField<IVP_FLOAT>(oppositeOrientation.contact.contact(), 0x30u) = 0.25f;
  ContactField<IVP_FLOAT>(oppositeOrientation.contact.contact(), 0x44u) = 0.8f;
  ContactField<IVP_FLOAT>(oppositeOrientation.contact.contact(), 0x54u) = 20.0f;
  IVP_Event_Sim event(nullptr, 0.5);
  EXPECT_FLOAT_EQ(pair.get_sum_slide_way(&event), 1.5f);

  ContactField<std::uint8_t>(sameOrientation.contact.contact(), 0x34u) = 0;
  ContactField<std::uint8_t>(oppositeOrientation.contact.contact(), 0x34u) = 1;
  pair.pair_calc_friction_forces(&event);
  EXPECT_EQ(g_constraint2DCalls, 1);
  EXPECT_EQ(g_constraint1DCalls, 1);
  EXPECT_FLOAT_EQ(pair.integrated_anti_energy, 0.75f);
  EXPECT_NEAR(
      std::hypot(
          ContactField<IVP_FLOAT>(sameOrientation.contact.contact(), 0x38u),
          ContactField<IVP_FLOAT>(sameOrientation.contact.contact(), 0x3Cu)),
      1.5, 1.0e-5);
  EXPECT_NEAR(std::hypot(ContactField<IVP_FLOAT>(
                             oppositeOrientation.contact.contact(), 0x38u),
                         ContactField<IVP_FLOAT>(
                             oppositeOrientation.contact.contact(), 0x3Cu)),
              1.5, 1.0e-5);

  vector->n_elems = 0;
  vector->elems = nullptr;
  vector->memsize = 0;
}

TEST(IvpContactTopology,
     LongTermImpactClosingSpeedIncludesBothLinearAndAngularMotion) {
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> coreAStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> coreBStorage{};
  auto *coreA = reinterpret_cast<IVP_Core *>(coreAStorage.data());
  auto *coreB = reinterpret_cast<IVP_Core *>(coreBStorage.data());

  coreA->speed.set(3.0f, 0.0f, 0.0f);
  coreA->rot_speed.set(0.0f, 0.0f, 2.0f);
  coreB->speed.set(1.0f, 0.0f, 0.0f);
  coreB->rot_speed.set(0.0f, 4.0f, 0.0f);

  IVP_Impact_Solver_Long_Term impact;
  impact.surf_normal.set(1.0f, 0.0f, 0.0f);
  impact.contact_core[0] = coreA;
  impact.contact_core[1] = coreB;
  impact.contact_cross_nomal_cs[0].set(0.0f, 0.0f, 0.5f);
  impact.contact_cross_nomal_cs[1].set(0.0f, 0.25f, 0.0f);

  // Core A closes at 3 + 2*0.5, core B separates at 1 + 4*0.25.
  EXPECT_DOUBLE_EQ(impact.get_closing_speed(), 2.0);

  impact.contact_core[1] = nullptr;
  EXPECT_DOUBLE_EQ(impact.get_closing_speed(), 4.0);
}

TEST(IvpContactTopology,
     MutualEnergizerReportsOnlyRemovableRelativeMotionEnergy) {
  constexpr IVP_DOUBLE firstMass = 2.0;
  constexpr IVP_DOUBLE secondMass = 3.0;
  constexpr IVP_DOUBLE relativeSpeed = 5.0;

  // In core 0's frame, the removable energy is the initial kinetic energy
  // minus the center-of-mass kinetic energy that a mutual impulse preserves.
  const IVP_DOUBLE commonSpeed =
      secondMass * relativeSpeed / (firstMass + secondMass);
  const IVP_DOUBLE expected =
      0.5 * secondMass * relativeSpeed * relativeSpeed -
      0.5 * (firstMass + secondMass) * commonSpeed * commonSpeed;

  const IVP_DOUBLE actual = IVP_Mutual_Energizer::calc_energy_potential(
      relativeSpeed, firstMass, secondMass, 1.0 / firstMass,
      1.0 / secondMass);
  EXPECT_NEAR(actual, expected, 1.0e-12);
  EXPECT_NEAR(actual, 15.0, 1.0e-12);

  // Relative energy cannot depend on which member of the contact pair was
  // selected as core 0, or on the sign chosen for the relative direction.
  EXPECT_NEAR(IVP_Mutual_Energizer::calc_energy_potential(
                  -relativeSpeed, secondMass, firstMass, 1.0 / secondMass,
                  1.0 / firstMass),
              expected, 1.0e-12);
  EXPECT_DOUBLE_EQ(IVP_Mutual_Energizer::calc_energy_potential(
                       0.0, firstMass, secondMass, 1.0 / firstMass,
                       1.0 / secondMass),
                   0.0);
}

TEST(IvpContactTopology,
     FrictionControllersBracketTheDynamicSolveInRetailPipelineOrder) {
  IVP_Friction_Sys_Static staticController;
  IVP_Friction_Sys_Energy energyController;

  EXPECT_EQ(staticController.get_controller_priority(),
            IVP_CP_STATIC_FRICTION);
  EXPECT_EQ(energyController.get_controller_priority(),
            IVP_CP_ENERGY_FRICTION);
  EXPECT_LT(staticController.get_controller_priority(),
            IVP_CP_DYNAMIC_FRICTION);
  EXPECT_GT(energyController.get_controller_priority(),
            IVP_CP_DYNAMIC_FRICTION);
  EXPECT_DOUBLE_EQ(staticController.get_minimum_simulation_frequency(), 1.0);
  EXPECT_DOUBLE_EQ(energyController.get_mimumum_simulation_frequency(), 1.0);

  alignas(IVP_Friction_System)
      std::array<std::byte, sizeof(IVP_Friction_System)> systemStorage{};
  auto *system = reinterpret_cast<IVP_Friction_System *>(systemStorage.data());
  auto *staticBorrowed = system->get_static_friction_controller();
  auto *energyBorrowed = system->get_energy_friction_controller();
  EXPECT_EQ(reinterpret_cast<std::byte *>(staticBorrowed),
            systemStorage.data() + 0x08u);
  EXPECT_EQ(reinterpret_cast<std::byte *>(energyBorrowed),
            systemStorage.data() + 0x10u);
  staticBorrowed->l_friction_system = system;
  energyBorrowed->l_friction_system = system;
  EXPECT_EQ(staticBorrowed->l_friction_system, system);
  EXPECT_EQ(energyBorrowed->l_friction_system, system);
}

TEST(IvpContactTopology,
     ContactImpulseConservesLinearMomentumAndUpdatesBothAngularVelocities) {
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> firstStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> secondStorage{};
  auto *first = reinterpret_cast<IVP_Core *>(firstStorage.data());
  auto *second = reinterpret_cast<IVP_Core *>(secondStorage.data());

  first->inv_rot_inertia.set_to_zero();
  second->inv_rot_inertia.set_to_zero();
  first->inv_rot_inertia.k[0] = 2.0f;
  first->inv_rot_inertia.k[1] = 3.0f;
  first->inv_rot_inertia.k[2] = 4.0f;
  first->inv_rot_inertia.hesse_val = 0.5f;
  second->inv_rot_inertia.k[0] = 5.0f;
  second->inv_rot_inertia.k[1] = 6.0f;
  second->inv_rot_inertia.k[2] = 7.0f;
  second->inv_rot_inertia.hesse_val = 0.25f;
  first->speed.set_to_zero();
  second->speed.set_to_zero();
  first->rot_speed.set_to_zero();
  second->rot_speed.set_to_zero();
  first->speed_change.set_to_zero();
  second->speed_change.set_to_zero();
  first->rot_speed_change.set_to_zero();
  second->rot_speed_change.set_to_zero();

  IVP_Impact_Solver_Long_Term information;
  information.surf_normal.set_to_zero();
  information.surf_normal.k[0] = 1.0f;
  information.contact_cross_nomal_cs[0].set_to_zero();
  information.contact_cross_nomal_cs[0].k[1] = 1.0f;
  information.contact_cross_nomal_cs[1].set_to_zero();
  information.contact_cross_nomal_cs[1].k[2] = 1.0f;
  information.contact_core[0] = first;
  information.contact_core[1] = second;
  information.inv_virtual_mass = 0.125f;

  IVP_Friction_Solver::apply_impulse(&information, 4.0);
  EXPECT_FLOAT_EQ(first->speed.k[0], -2.0f);
  EXPECT_FLOAT_EQ(second->speed.k[0], 1.0f);
  EXPECT_FLOAT_EQ(2.0f * first->speed.k[0] +
                      4.0f * second->speed.k[0],
                  0.0f);
  EXPECT_FLOAT_EQ(first->rot_speed.k[1], -12.0f);
  EXPECT_FLOAT_EQ(second->rot_speed.k[2], 28.0f);

  IVP_Friction_Solver::async_apply_impulse(&information, 2.0);
  EXPECT_FLOAT_EQ(first->speed_change.k[0], -1.0f);
  EXPECT_FLOAT_EQ(second->speed_change.k[0], 0.5f);
  EXPECT_FLOAT_EQ(first->rot_speed_change.k[1], -6.0f);
  EXPECT_FLOAT_EQ(second->rot_speed_change.k[2], 14.0f);
  EXPECT_DOUBLE_EQ(IVP_Friction_Solver::get_inv_virtual_mass(&information),
                   0.125);
  EXPECT_DOUBLE_EQ(
      IVP_Friction_Solver::calc_desired_gap_speed(2.0, 0.5, 4.0f), 4.0);
  EXPECT_DOUBLE_EQ(
      IVP_Friction_Solver::calc_desired_gap_speed(2.0, -0.5, 4.0f), -38.0);
}

TEST(IvpContactTopology,
     SharedCoreImpulseColumnCouplesContactRowsWithOppositeOrientations) {
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
  auto *sharedCore = reinterpret_cast<IVP_Core *>(coreStorage.data());

  IVP_Impact_Solver_Long_Term firstInformation;
  IVP_Impact_Solver_Long_Term secondInformation;
  firstInformation.index_in_fs = 0;
  secondInformation.index_in_fs = 1;
  firstInformation.contact_core[0] = sharedCore;
  firstInformation.contact_core[1] = nullptr;
  secondInformation.contact_core[0] = nullptr;
  secondInformation.contact_core[1] = sharedCore;
  firstInformation.surf_normal.set_to_zero();
  secondInformation.surf_normal.set_to_zero();
  firstInformation.surf_normal.k[0] = 1.0f;
  secondInformation.surf_normal.k[0] = 1.0f;
  firstInformation.contact_cross_nomal_cs[0].set_to_zero();
  secondInformation.contact_cross_nomal_cs[1].set_to_zero();
  firstInformation.contact_cross_nomal_cs[0].k[1] = 1.0f;
  secondInformation.contact_cross_nomal_cs[1].k[1] = 0.5f;

  ContactFixture firstContact;
  ContactFixture secondContact;
  ContactField<IVP_Impact_Solver_Long_Term *>(firstContact.contact(), 0x40u) =
      &firstInformation;
  ContactField<IVP_Impact_Solver_Long_Term *>(secondContact.contact(), 0x40u) =
      &secondInformation;
  void *contactPointers[] = {firstContact.contact(), secondContact.contact()};
  IVP_Friction_Info_For_Core frictionInfo;
  frictionInfo.friction_springs.memsize = 2;
  frictionInfo.friction_springs.n_elems = 2;
  frictionInfo.friction_springs.elems = contactPointers;

  alignas(IVP_Friction_Solver)
      std::array<std::byte, sizeof(IVP_Friction_Solver)> solverStorage{};
  auto *solver =
      reinterpret_cast<IVP_Friction_Solver *>(solverStorage.data());
  IVP_DOUBLE matrix[4]{};
  solver->dist_change_mat.columns = 2;
  solver->dist_change_mat.aligned_row_len = 2;
  solver->dist_change_mat.matrix_values = matrix;

  IVP_U_Float_Point rotationChange;
  IVP_U_Float_Point translationChange;
  rotationChange.set_to_zero();
  translationChange.set_to_zero();
  rotationChange.k[1] = 2.0f;
  translationChange.k[0] = 3.0f;
  solver->calc_distance_matrix_column(
      0, sharedCore, &frictionInfo, &rotationChange, &translationChange);

  EXPECT_DOUBLE_EQ(matrix[0], -5.0);
  EXPECT_DOUBLE_EQ(matrix[2], 4.0);
  EXPECT_DOUBLE_EQ(matrix[1], 0.0);
  EXPECT_DOUBLE_EQ(matrix[3], 0.0);

  // The fixture borrows stack storage; prevent its vector destructor from
  // passing that storage to the retail allocator.
  frictionInfo.friction_springs.elems = nullptr;
  frictionInfo.friction_springs.memsize = 0;
  frictionInfo.friction_springs.n_elems = 0;
}

} // namespace
