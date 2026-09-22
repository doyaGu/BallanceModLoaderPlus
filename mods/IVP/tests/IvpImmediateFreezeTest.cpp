#include "IvpTestAdapter.h"

#include "BML/IVP/Environment.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

class RecordingAnomalyManager final : public IVP_Anomaly_Manager {
public:
    RecordingAnomalyManager() : IVP_Anomaly_Manager(IVP_FALSE) {}

    void max_velocity_exceeded(
        IVP_Anomaly_Limits *limits, IVP_Core *,
        IVP_U_Float_Point *velocity) override {
        ++linearLimitCount;
        const IVP_DOUBLE length = velocity->real_length();
        velocity->mult(limits->get_max_velocity() / length);
    }

    void max_angular_velocity_exceeded(
        IVP_Anomaly_Limits *limits, IVP_Core *core,
        IVP_U_Float_Point *angularVelocity) override {
        ++angularLimitCount;
        const IVP_DOUBLE maximum =
            limits->get_max_angular_velocity_per_psi() *
            core->get_environment()->get_inv_delta_PSI_time();
        angularVelocity->mult(maximum / angularVelocity->real_length());
    }

    int linearLimitCount = 0;
    int angularLimitCount = 0;
};

IVP_Environment *g_expectedEnvironment = nullptr;
IVP_Core *g_expectedCore = nullptr;
IVP_Simulation_Unit *g_expectedSimulationUnit = nullptr;
std::array<IVP_Real_Object *, 4> g_unlinkedObjects{};
int g_unlinkedCount = 0;
int g_removeWakeupCount = 0;
int g_unionFindCount = 0;
int g_calculateMovementCount = 0;

void __fastcall RemoveReviveCore(
    IVP_Environment *environment, void *, IVP_Core *core) {
    EXPECT_EQ(environment, g_expectedEnvironment);
    EXPECT_EQ(core, g_expectedCore);
    ++g_removeWakeupCount;
}

void __fastcall UnlinkContactPoints(
    IVP_Real_Object *object, void *, IVP_BOOL silent) {
    EXPECT_EQ(silent, IVP_TRUE);
    ASSERT_LT(g_unlinkedCount, static_cast<int>(g_unlinkedObjects.size()));
    g_unlinkedObjects[g_unlinkedCount++] = object;
}

void __fastcall DoUnionFind(IVP_Simulation_Unit *unit, void *) {
    EXPECT_EQ(unit, g_expectedSimulationUnit);
    ++g_unionFindCount;
}

IVP_BOOL __fastcall CalculateMovementState(
    IVP_Simulation_Unit *unit, void *, IVP_Environment *environment) {
    EXPECT_EQ(unit, g_expectedSimulationUnit);
    EXPECT_EQ(environment, g_expectedEnvironment);
    ++g_calculateMovementCount;
    return IVP_TRUE;
}

IVP_DOUBLE __fastcall FloatPointLength(
    IVP_U_Float_Point *point, void *) {
    return std::sqrt(point->quad_length());
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::EnvironmentRemoveReviveCore, &RemoveReviveCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectUnlinkContactPoints, &UnlinkContactPoints),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitUnionFind, &DoUnionFind),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitCalculateMovementState, &CalculateMovementState),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::FloatPointRealLength, &FloatPointLength),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


void ResetObservations() {
    g_unlinkedObjects.fill(nullptr);
    g_unlinkedCount = 0;
    g_removeWakeupCount = 0;
    g_unionFindCount = 0;
    g_calculateMovementCount = 0;
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpImmediateFreeze,
     FreezesEveryObjectSharingTheCoreAndSeedsCalmReferences) {
    ResetObservations();

    std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> primaryStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> attachedStorage{};
    std::uint32_t simulationUnitStorage = 0;
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());
    auto *primary = reinterpret_cast<IVP_Real_Object *>(primaryStorage.data());
    auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedStorage.data());
    auto *simulationUnit = reinterpret_cast<IVP_Simulation_Unit *>(
        &simulationUnitStorage);

    std::array<void *, 2> objects{primary, attached};
    core->objects.elems = objects.data();
    core->objects.memsize = static_cast<std::uint16_t>(objects.size());
    core->objects.n_elems = static_cast<std::uint16_t>(objects.size());
    core->environment = environment;
    core->sim_unit_of_core = simulationUnit;
    core->movement_state = IVP_MT_MOVING;
    core->flags = 0x10u; // is_in_wakeup_vec == IVP_TRUE.
    core->q_world_f_core_next_psi.set(0.1, 0.2, 0.3, 0.9);
    core->pos_world_f_core_last_psi.set(10.0, 20.0, 30.0);
    primary->physical_core = core;
    attached->physical_core = core;
    environment->current_time = IVP_Time(100.0);

    g_expectedEnvironment = environment;
    g_expectedCore = core;
    g_expectedSimulationUnit = simulationUnit;

    EXPECT_EQ(primary->disable_simulation(), IVP_TRUE);

    EXPECT_EQ(g_removeWakeupCount, 1);
    ASSERT_EQ(g_unlinkedCount, 2);
    EXPECT_EQ(g_unlinkedObjects[0], attached);
    EXPECT_EQ(g_unlinkedObjects[1], primary);
    EXPECT_EQ(g_unionFindCount, 1);
    EXPECT_EQ(g_calculateMovementCount, 1);
    EXPECT_FLOAT_EQ(core->q_world_f_core_calm_reference[0].x, 0.1f);
    EXPECT_FLOAT_EQ(core->q_world_f_core_calm_reference[0].w, 0.9f);
    EXPECT_FLOAT_EQ(core->position_world_f_core_calm_reference[0].k[0], 10.0f);
    EXPECT_FLOAT_EQ(core->position_world_f_core_calm_reference[0].k[2], 30.0f);
    EXPECT_DOUBLE_EQ(core->time_of_calm_reference[0].get_time(), 80.0);
}

TEST(IvpImmediateFreeze, StaticCoreIsAlreadySuccessfullyDisabled) {
    ResetObservations();

    std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    core->flags = 0x04u; // physical_unmoveable == IVP_TRUE.
    object->physical_core = core;

    EXPECT_EQ(object->disable_simulation(), IVP_TRUE);
    EXPECT_EQ(g_removeWakeupCount, 0);
    EXPECT_EQ(g_unlinkedCount, 0);
    EXPECT_EQ(g_unionFindCount, 0);
    EXPECT_EQ(g_calculateMovementCount, 0);
}

TEST(IvpImmediateFreeze,
     EnforcesLinearAngularAndPerAxisCoreVelocityPolicies) {
    std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());

    IVP_Anomaly_Limits limits(IVP_FALSE);
    limits.max_velocity = 100.0f;
    limits.max_angular_velocity_per_psi = 0.1f;
    RecordingAnomalyManager anomalyManager;
    environment->anomaly_limits = &limits;
    environment->anomaly_manager = &anomalyManager;
    environment->inv_delta_PSI_time = 100.0;
    core->environment = environment;

    IVP_U_Float_Point linearVelocity(300.0f, 400.0f, 0.0f);
    IVP_U_Float_Point angularVelocity(0.0f, 20.0f, 0.0f);
    core->clip_velocity(&linearVelocity, &angularVelocity);
    EXPECT_EQ(anomalyManager.linearLimitCount, 1);
    EXPECT_EQ(anomalyManager.angularLimitCount, 1);
    EXPECT_NEAR(linearVelocity.real_length(), 100.0, 1.0e-4);
    EXPECT_NEAR(angularVelocity.real_length(), 10.0, 1.0e-4);

    IVP_U_Float_Point spinClipping(2.0f, 3.0f, 4.0f);
    core->spin_clipping = &spinClipping;
    linearVelocity.set(1.0f, 2.0f, 3.0f);
    angularVelocity.set(6.0f, -7.0f, 8.0f);
    core->clip_velocity(&linearVelocity, &angularVelocity);
    EXPECT_EQ(anomalyManager.angularLimitCount, 1);
    EXPECT_FLOAT_EQ(angularVelocity.k[0], 2.0f);
    EXPECT_FLOAT_EQ(angularVelocity.k[1], -3.0f);
    EXPECT_FLOAT_EQ(angularVelocity.k[2], 4.0f);

    core->speed.set(300.0f, 400.0f, 0.0f);
    core->speed_change.set(0.0f, 20.0f, 0.0f);
    core->rot_speed.set(0.0f, 20.0f, 0.0f);
    core->rot_speed_change.set(0.0f, 40.0f, 0.0f);
    core->apply_velocity_limit();
    EXPECT_NEAR(core->speed.real_length(), 100.0, 1.0e-4);
    EXPECT_NEAR(core->speed_change.real_length(), 20.0, 1.0e-4);
    EXPECT_NEAR(core->rot_speed.real_length(), 10.0, 1.0e-4);
    // The neighboring implementation deliberately scales this by the pending
    // linear-change length (20), yielding 20 rather than independently
    // normalizing the angular change to 10.
    EXPECT_NEAR(core->rot_speed_change.real_length(), 20.0, 1.0e-4);
}

} // namespace
