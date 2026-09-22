#include "IvpTestAdapter.h"

#include "BML/IVP/Environment.h"
#include "BML/IVP/ObjectAttach.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace {

struct RawCacheObjectPrefix {
    IVP_Time_CODE validUntilTimeCode;
    std::int32_t referenceCount;
    IVP_Real_Object *object;
    std::uint32_t reserved;
};

static_assert(sizeof(RawCacheObjectPrefix) == 0x10);

RawCacheObjectPrefix &RawCache(IVP_Cache_Object *cache) {
    return *reinterpret_cast<RawCacheObjectPrefix *>(cache);
}

enum class Operation {
    WriteObjectRotation,
    MultiplyReferenceTransform,
    InterpolateCoreRotation,
    TransformObjectShift,
    InvertObjectRotation,
    SetEnvironmentTime,
    CalculateRelativeRotation,
    WriteCoreRotation,
    InverseMultiplyCoreTransform,
    SetObjectCoreTransform,
    CalculateRotationAxis,
    InvalidateObjectCache,
    RecalculateExactMindist,
    RecalculateInvalidMindists,
    RecheckBroadphase,
    ResetHullTimes,
    ReviveAttachedObject,
    UnlinkFromOldCore,
    InitializeWorldReference,
    RecalculateParentCore,
    EnsureParentCoreSimulation,
    AllocateDetachedCore,
    UnlinkAttachedContacts,
    ConstructDetachedCore,
    InitializeDetachedObjectCore,
    FireDetachedObjectFrozen,
    CalculateDetachedNextPsi,
    CommitDetachedHulls,
    UpdateDetachedMindistEvents,
};

std::vector<Operation> g_operations;
IVP_Real_Object *g_expectedObject = nullptr;
IVP_Mindist *g_expectedMindist = nullptr;
IVP_FLOAT g_speedSeenByRotationSolver = 0.0f;
IVP_FLOAT g_hullValueBeforeReset = 0.0f;
IVP_FLOAT g_centerHullValueBeforeReset = 0.0f;
int g_rotationWrites = 0;
std::array<IVP_DOUBLE, 12> g_objectFromCore{};
IVP_Core *g_expectedOldCore = nullptr;
IVP_Core *g_expectedParentCore = nullptr;
IVP_Core *g_detachedCore = nullptr;
IVP_BOOL g_corePolicyArgument = IVP_FALSE;

IVP_Core *SolverCore(IVP_Calc_Next_PSI_Solver *solver) {
    return *reinterpret_cast<IVP_Core **>(solver);
}

void WriteRotationMatrixValues(const IVP_U_Quat *rotation,
                               IVP_U_Matrix3 *matrix) {
    const IVP_DOUBLE x2 = rotation->x + rotation->x;
    const IVP_DOUBLE y2 = rotation->y + rotation->y;
    const IVP_DOUBLE z2 = rotation->z + rotation->z;
    const IVP_DOUBLE xx = rotation->x * x2;
    const IVP_DOUBLE xy = rotation->x * y2;
    const IVP_DOUBLE xz = rotation->x * z2;
    const IVP_DOUBLE yy = rotation->y * y2;
    const IVP_DOUBLE yz = rotation->y * z2;
    const IVP_DOUBLE zz = rotation->z * z2;
    const IVP_DOUBLE wx = rotation->w * x2;
    const IVP_DOUBLE wy = rotation->w * y2;
    const IVP_DOUBLE wz = rotation->w * z2;
    matrix->set_elem(0, 0, 1.0 - (yy + zz));
    matrix->set_elem(0, 1, xy - wz);
    matrix->set_elem(0, 2, xz + wy);
    matrix->set_elem(1, 0, xy + wz);
    matrix->set_elem(1, 1, 1.0 - (xx + zz));
    matrix->set_elem(1, 2, yz - wx);
    matrix->set_elem(2, 0, xz - wy);
    matrix->set_elem(2, 1, yz + wx);
    matrix->set_elem(2, 2, 1.0 - (xx + yy));
}

void __fastcall WriteRotationMatrix(IVP_U_Quat *rotation, void *,
                                    IVP_U_Matrix3 *matrix) {
    g_operations.push_back(g_rotationWrites++ == 0
                               ? Operation::WriteObjectRotation
                               : Operation::WriteCoreRotation);
    WriteRotationMatrixValues(rotation, matrix);
}

void __fastcall MultiplyReferenceTransform(
    IVP_U_Matrix *left, void *, const IVP_U_Matrix *right,
    IVP_U_Matrix *output) {
    g_operations.push_back(Operation::MultiplyReferenceTransform);
    left->inline_mmult4(right, output);
}

void __fastcall InverseMultiplyCoreTransform(
    IVP_U_Matrix *left, void *, const IVP_U_Matrix *right,
    IVP_U_Matrix *output) {
    g_operations.push_back(Operation::InverseMultiplyCoreTransform);
    left->inline_mimult4(right, output);
}

void __fastcall InterpolateCoreRotation(
    IVP_U_Quat *output, void *, const IVP_U_Quat *from,
    const IVP_U_Quat *to, IVP_DOUBLE factor) {
    g_operations.push_back(Operation::InterpolateCoreRotation);
    output->set_interpolate_linear(from, to, factor);
}

void __fastcall SetObjectCoreTransform(
    IVP_Real_Object *object, void *, const IVP_U_Matrix *matrix) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::SetObjectCoreTransform);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column)
            g_objectFromCore[row * 3 + column] =
                matrix->get_elem(column, row);
    }
    g_objectFromCore[9] = matrix->vv.k[0];
    g_objectFromCore[10] = matrix->vv.k[1];
    g_objectFromCore[11] = matrix->vv.k[2];
}

void __fastcall TransformObjectShift(IVP_U_Matrix *matrix, void *,
                                     const IVP_U_Float_Point *input,
                                     IVP_U_Point *output) {
    g_operations.push_back(Operation::TransformObjectShift);
    matrix->inline_vmult4(input, output);
}

void __fastcall InvertUnitQuaternion(IVP_U_Quat *output, void *,
                                     const IVP_U_Quat *input) {
    g_operations.push_back(Operation::InvertObjectRotation);
    output->set(-input->x, -input->y, -input->z, input->w);
}

void __fastcall SetEnvironmentTime(IVP_Environment *environment, void *,
                                   IVP_Time time) {
    g_operations.push_back(Operation::SetEnvironmentTime);
    ++environment->current_time_code;
    environment->current_time = time;
}

void __fastcall CalculateRelativeRotation(IVP_U_Quat *output, void *,
                                          const IVP_U_Quat *previous,
                                          const IVP_U_Quat *next) {
    g_operations.push_back(Operation::CalculateRelativeRotation);
    IVP_U_Quat inverse;
    inverse.set(-previous->x, -previous->y, -previous->z, previous->w);
    output->inline_set_mult_quat(&inverse, next);
}

void __fastcall CalculateRotationAxis(IVP_Calc_Next_PSI_Solver *solver,
                                      void *, const IVP_U_Quat *relative) {
    g_operations.push_back(Operation::CalculateRotationAxis);
    IVP_Core *core = SolverCore(solver);
    g_speedSeenByRotationSolver = core->current_speed;
    const IVP_DOUBLE vectorLength = relative->x * relative->x +
                                    relative->y * relative->y +
                                    relative->z * relative->z;
    core->abs_omega = vectorLength > 1.0e-12 ? 2.0f : 0.0f;
    core->max_surface_rot_speed =
        core->max_surface_deviation * core->abs_omega;
}

void __cdecl InvalidateObjectCache(IVP_Real_Object *object) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::InvalidateObjectCache);
}

void __fastcall RecalculateExactMindist(IVP_Mindist_Manager *, void *,
                                        IVP_Mindist *mindist) {
    EXPECT_EQ(mindist, g_expectedMindist);
    g_operations.push_back(Operation::RecalculateExactMindist);
}

void __fastcall RecalculateInvalidMindists(IVP_Real_Object *object, void *) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::RecalculateInvalidMindists);
}

void __fastcall RecheckBroadphase(IVP_Mindist_Manager *, void *,
                                  IVP_Real_Object *object) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::RecheckBroadphase);
}

void __fastcall ResetHullTimes(IVP_Hull_Manager *hull, void *) {
    g_operations.push_back(Operation::ResetHullTimes);
    g_hullValueBeforeReset = hull->hull_value_next_psi;
    g_centerHullValueBeforeReset = hull->hull_center_value_last_vpsi;
    hull->hull_value_last_vpsi = 0.0f;
    hull->hull_center_value_last_vpsi = 0.0f;
    hull->hull_value_next_psi = 0.0f;
}

void __fastcall ReviveAttachedObject(IVP_Real_Object *object, void *) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::ReviveAttachedObject);
    object->set_movement_state(IVP_MT_MOVING);
}

void __fastcall UnlinkFromOldCore(IVP_Core *core, void *,
                                  IVP_Real_Object *object) {
    EXPECT_EQ(core, g_expectedOldCore);
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::UnlinkFromOldCore);
    ASSERT_EQ(core->objects.n_elems, 1);
    ASSERT_EQ(core->objects.element_at(0), object);
    core->objects.n_elems = 0;
}

void __fastcall InitializeWorldReference(IVP_U_Matrix *matrix, void *) {
    g_operations.push_back(Operation::InitializeWorldReference);
    matrix->set_identity();
}

void __fastcall RecalculateParentCore(IVP_Core *core, void *) {
    EXPECT_EQ(core, g_expectedParentCore);
    g_operations.push_back(Operation::RecalculateParentCore);
}

void __fastcall EnsureParentCoreSimulation(IVP_Core *core, void *) {
    EXPECT_EQ(core, g_expectedParentCore);
    g_operations.push_back(Operation::EnsureParentCoreSimulation);
}

void *__cdecl AllocateDetachedCore(unsigned int size) {
    EXPECT_EQ(size, sizeof(IVP_Core));
    g_operations.push_back(Operation::AllocateDetachedCore);
    return std::calloc(1, size);
}

void __fastcall UnlinkAttachedContacts(IVP_Real_Object *object, void *,
                                       IVP_BOOL silent) {
    EXPECT_EQ(object, g_expectedObject);
    EXPECT_EQ(silent, IVP_TRUE);
    g_operations.push_back(Operation::UnlinkAttachedContacts);
}

void __fastcall ConstructDetachedCore(
    IVP_Core *core, void *, IVP_Real_Object *object,
    const IVP_U_Quat *worldFromObject, const IVP_U_Point *position,
    IVP_BOOL physicalUnmoveable, IVP_BOOL policyArgument) {
    EXPECT_EQ(object, g_expectedObject);
    EXPECT_EQ(physicalUnmoveable, IVP_FALSE);
    g_operations.push_back(Operation::ConstructDetachedCore);
    g_detachedCore = core;
    g_corePolicyArgument = policyArgument;
    core->environment = object->get_environment();
    core->movement_state = IVP_MT_NOT_SIM;
    core->flags = physicalUnmoveable == IVP_TRUE ? (1u << 2u) : 0u;
    core->objects.reset();
    core->objects.n_elems = 0;
    core->objects.add(object);
    core->q_world_f_core_last_psi = *worldFromObject;
    core->q_world_f_core_next_psi = *worldFromObject;
    core->m_world_f_core_last_psi.set_identity();
    core->m_world_f_core_last_psi.vv.set(position);
    core->pos_world_f_core_last_psi.set(position);
}

void __fastcall InitializeDetachedObjectCore(
    IVP_Real_Object *object, void *, IVP_Environment *environment,
    const IVP_Template_Real_Object *) {
    EXPECT_EQ(object, g_expectedObject);
    EXPECT_EQ(environment, object->get_environment());
    g_operations.push_back(Operation::InitializeDetachedObjectCore);
}

void __fastcall FireDetachedObjectFrozen(IVP_Core *core, void *) {
    EXPECT_EQ(core, g_detachedCore);
    g_operations.push_back(Operation::FireDetachedObjectFrozen);
}

void __fastcall CalculateDetachedNextPsi(
    IVP_Calc_Next_PSI_Solver *solver, void *, IVP_Event_Sim *event,
    IVP_U_Vector<IVP_Hull_Manager_Base> *) {
    EXPECT_EQ(SolverCore(solver), g_detachedCore);
    EXPECT_NEAR(event->delta_time, 0.02, 1.0e-12);
    g_operations.push_back(Operation::CalculateDetachedNextPsi);
}

void __cdecl CommitDetachedHulls(
    IVP_Environment *environment,
    IVP_U_Vector<IVP_Hull_Manager_Base> *) {
    EXPECT_EQ(environment, g_expectedObject->get_environment());
    g_operations.push_back(Operation::CommitDetachedHulls);
}

void __fastcall UpdateDetachedMindistEvents(IVP_Real_Object *object, void *) {
    EXPECT_EQ(object, g_expectedObject);
    g_operations.push_back(Operation::UpdateDetachedMindistEvents);
}

IVP_DOUBLE __fastcall FloatPointLength(IVP_U_Float_Point *point, void *) {
    return std::sqrt(point->quad_length());
}

void __fastcall TransformFloatVector(
    IVP_U_Matrix3 *matrix, void *, const IVP_U_Float_Point *input,
    IVP_U_Float_Point *output) {
    matrix->inline_vmult3(input, output);
}

void __fastcall CrossFloatVectors(
    IVP_U_Float_Point *output, void *, const IVP_U_Float_Point *left,
    const IVP_U_Float_Point *right) {
    output->inline_calc_cross_product(left, right);
}

int g_templateObjectConstructionCalls = 0;
int g_templateRealObjectConstructionCalls = 0;
bool g_expectRawTemplateRealObjectStorage = false;
bool g_sawRawTemplateRealObjectStorage = false;

void __fastcall ConstructTemplateObject(
    IVP_Template_Object *object, void *) {
    ++g_templateObjectConstructionCalls;
    *reinterpret_cast<void **>(object) = nullptr;
}

void __fastcall ConstructTemplateRealObject(
    IVP_Template_Real_Object *object, void *) {
    ++g_templateRealObjectConstructionCalls;
    if (g_expectRawTemplateRealObjectStorage) {
        const auto *bytes = reinterpret_cast<const std::byte *>(object);
        g_sawRawTemplateRealObjectStorage = std::all_of(
            bytes, bytes + sizeof(*object), [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
    // The retained complete constructor owns the base call before initializing
    // the complete 0x70-byte transport object.
    ConstructTemplateObject(object, nullptr);
    std::memset(object, 0, sizeof(*object));
    object->mass = 1.0;
    object->rot_inertia_is_factor = IVP_TRUE;
    object->rot_inertia.set(1.0f, 1.0f, 1.0f);
    object->auto_check_rot_inertia = 0.03f;
    object->speed_damp_factor = 0.01;
    object->rot_speed_damp_factor.set(0.01, 0.01, 0.01);
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplateObjectConstruct,
        &ConstructTemplateObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplateRealObjectConstruct,
        &ConstructTemplateRealObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionWriteMatrix, &WriteRotationMatrix),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixMultiply, &MultiplyReferenceTransform),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixInverseMultiply,
        &InverseMultiplyCoreTransform),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionInterpolateSmoothly,
        &InterpolateCoreRotation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectSetMatrixCoreFromObject,
        &SetObjectCoreTransform),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixTransformFloatPointToPoint, &TransformObjectShift),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionSetInverseUnit, &InvertUnitQuaternion),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::EnvironmentSetCurrentTime, &SetEnvironmentTime),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionSetInverseMultiply, &CalculateRelativeRotation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CalcNextPsiRotationAxis, &CalculateRotationAxis),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CacheManagerInvalidObject, &InvalidateObjectCache),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MindistManagerRecalcExact, &RecalculateExactMindist),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectRecalculateInvalidMindists, &RecalculateInvalidMindists),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MindistManagerRecheckOvElement, &RecheckBroadphase),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::HullManagerResetTimes, &ResetHullTimes),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectRevive, &ReviveAttachedObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreUnlinkObject, &UnlinkFromOldCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixInitialize, &InitializeWorldReference),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreCalculateRedundantValues,
        &RecalculateParentCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreEnsureSimulation,
        &EnsureParentCoreSimulation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorNew, &AllocateDetachedCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectUnlinkContactPoints,
        &UnlinkAttachedContacts),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreConstruct, &ConstructDetachedCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectInitializeCore,
        &InitializeDetachedObjectCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreFireObjectFrozen,
        &FireDetachedObjectFrozen),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CalcNextPsiMatrix,
        &CalculateDetachedNextPsi),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CalcNextPsiCommitAllHulls,
        &CommitDetachedHulls),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectUpdateExactMindistEvents,
        &UpdateDetachedMindistEvents),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::FloatPointRealLength, &FloatPointLength),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Matrix3TransformFloatPoint,
        &TransformFloatVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::FloatPointCrossProduct,
        &CrossFloatVectors),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


void InitializeEmptyHull(IVP_Real_Object *object) {
    IVP_Hull_Manager *hull = object->get_hull_manager();
    hull->sorted_synapses.min_value = IVP_U_MINLIST_MAXVALUE;
    hull->sorted_synapses.first_element = IVP_U_MINLIST_UNUSED;
    hull->sorted_synapses.first_long = IVP_U_MINLIST_UNUSED;
    hull->sorted_synapses.counter = 0;
}

void InitializeCoreObjects(IVP_Core *core, IVP_Real_Object *object,
                           std::array<void *, 1> &storage) {
    storage[0] = object;
    core->objects.elems = storage.data();
    core->objects.memsize = 1;
    core->objects.n_elems = 1;
}

void ResetObservations() {
    g_operations.clear();
    g_expectedObject = nullptr;
    g_expectedMindist = nullptr;
    g_speedSeenByRotationSolver = 0.0f;
    g_hullValueBeforeReset = 0.0f;
    g_centerHullValueBeforeReset = 0.0f;
    g_rotationWrites = 0;
    g_objectFromCore.fill(0.0);
    g_expectedOldCore = nullptr;
    g_expectedParentCore = nullptr;
    g_detachedCore = nullptr;
    g_corePolicyArgument = IVP_FALSE;
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpBeamObject, CompleteRealObjectTemplateOwnsBaseConstruction) {
    g_templateObjectConstructionCalls = 0;
    g_templateRealObjectConstructionCalls = 0;
    g_expectRawTemplateRealObjectStorage = true;
    g_sawRawTemplateRealObjectStorage = false;
    alignas(IVP_Template_Real_Object)
        std::array<std::byte, sizeof(IVP_Template_Real_Object)> storage;
    std::fill(storage.begin(), storage.end(), std::byte{0xA5});

    auto *objectTemplate =
        ::new (storage.data()) IVP_Template_Real_Object();

    EXPECT_EQ(g_templateRealObjectConstructionCalls, 1);
    EXPECT_EQ(g_templateObjectConstructionCalls, 1);
    EXPECT_TRUE(g_sawRawTemplateRealObjectStorage);
    EXPECT_EQ(objectTemplate->get_name(), nullptr);
    EXPECT_DOUBLE_EQ(objectTemplate->mass, 1.0);
    EXPECT_FLOAT_EQ(objectTemplate->rot_inertia.k[0], 1.0f);
    EXPECT_FLOAT_EQ(objectTemplate->rot_inertia.k[1], 1.0f);
    EXPECT_FLOAT_EQ(objectTemplate->rot_inertia.k[2], 1.0f);
    objectTemplate->~IVP_Template_Real_Object();
    g_expectRawTemplateRealObjectStorage = false;
}

TEST(IvpBeamObject, TeleportsSleepingCompoundObjectAndRebuildsCollisionState) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Mindist_Manager)> managerData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> objectData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Mindist)> mindistData{};

    auto *environment = reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *manager = reinterpret_cast<IVP_Mindist_Manager *>(managerData.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreData.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectData.data());
    auto *mindist = reinterpret_cast<IVP_Mindist *>(mindistData.data());
    IVP_Synapse_Real *exactSynapse = mindist->get_synapse(0);

    environment->current_time = IVP_Time(4.0);
    environment->time_of_last_psi = IVP_Time(3.9);
    environment->delta_PSI_time = 0.1;
    environment->inv_delta_PSI_time = 10.0;
    environment->current_time_code = 17;
    environment->mindist_manager = manager;

    core->environment = environment;
    core->time_of_last_psi = IVP_Time(3.5);
    core->pos_world_f_core_last_psi.set(1.0, 2.0, 3.0);
    core->q_world_f_core_last_psi.init();
    core->max_surface_deviation = 0.25f;
    core->speed.set(7.0f, 8.0f, 9.0f);
    core->delta_world_f_core_psis.set(1.0f, 1.0f, 1.0f);

    object->environment = environment;
    object->physical_core = core;
    object->flags = IVP_MT_NOT_SIM;
    object->shift_core_f_object.set(1.0f, 2.0f, 3.0f);
    IVP_U_Quat coreFromObject;
    const IVP_DOUBLE halfRoot = std::sqrt(0.5);
    coreFromObject.set(0.0, 0.0, halfRoot, halfRoot);
    object->q_core_f_object = &coreFromObject;
    object->exact_synapses = exactSynapse;
    exactSynapse->next = nullptr;
    exactSynapse->set_synapse_mindist(mindist);
    InitializeEmptyHull(object);
    IVP_Hull_Manager *hull = object->get_hull_manager();
    hull->hull_value_next_psi = 2.0f;
    hull->hull_value_last_vpsi = 1.0f;
    hull->hull_center_value_last_vpsi = 3.0f;
    hull->last_vpsi_time = IVP_Time(12.0);
    hull->time_of_next_reset = 5;

    std::array<void *, 1> objects{};
    InitializeCoreObjects(core, object, objects);
    g_expectedObject = object;
    g_expectedMindist = mindist;

    IVP_U_Quat requestedRotation;
    requestedRotation.init();
    const IVP_U_Point requestedPosition(10.0, 20.0, 30.0);
    object->beam_object_to_new_position(
        &requestedRotation, &requestedPosition, IVP_FALSE);

    const IVP_U_Point expectedCorePosition(9.0, 18.0, 27.0);
    const IVP_DOUBLE movedDistance = std::sqrt(896.0);
    EXPECT_NEAR(core->pos_world_f_core_last_psi.k[0],
                expectedCorePosition.k[0], 1.0e-9);
    EXPECT_NEAR(core->pos_world_f_core_last_psi.k[1],
                expectedCorePosition.k[1], 1.0e-9);
    EXPECT_NEAR(core->pos_world_f_core_last_psi.k[2],
                expectedCorePosition.k[2], 1.0e-9);
    EXPECT_NEAR(core->q_world_f_core_last_psi.x, 0.0, 1.0e-12);
    EXPECT_NEAR(core->q_world_f_core_last_psi.y, 0.0, 1.0e-12);
    EXPECT_NEAR(core->q_world_f_core_last_psi.z, -halfRoot, 1.0e-12);
    EXPECT_NEAR(core->q_world_f_core_last_psi.w, halfRoot, 1.0e-12);
    EXPECT_NEAR(core->q_world_f_core_next_psi.z, -halfRoot, 1.0e-12);
    EXPECT_NEAR(core->q_world_f_core_next_psi.w, halfRoot, 1.0e-12);
    EXPECT_DOUBLE_EQ(core->time_of_last_psi.get_seconds(), 3.9);
    EXPECT_FLOAT_EQ(core->i_delta_time, 2.0f);
    EXPECT_DOUBLE_EQ(core->speed.quad_length(), 0.0);
    EXPECT_DOUBLE_EQ(core->delta_world_f_core_psis.quad_length(), 0.0);
    EXPECT_EQ(environment->current_time_code, 18);
    EXPECT_NEAR(g_hullValueBeforeReset, 2.0 + movedDistance, 1.0e-4);
    EXPECT_NEAR(g_centerHullValueBeforeReset, 3.0 + movedDistance, 1.0e-4);
    EXPECT_EQ(hull->time_of_next_reset, 22);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::WriteObjectRotation,
                  Operation::TransformObjectShift,
                  Operation::InvertObjectRotation,
                  Operation::SetEnvironmentTime,
                  Operation::CalculateRelativeRotation,
                  Operation::WriteCoreRotation,
                  Operation::CalculateRotationAxis,
                  Operation::InvalidateObjectCache,
                  Operation::RecalculateExactMindist,
                  Operation::RecalculateInvalidMindists,
                  Operation::RecheckBroadphase,
                  Operation::ResetHullTimes,
              }));
}

TEST(IvpBeamObject, RepeatedMovingUpdateSkipsBroadphaseRebuild) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> objectData{};

    auto *environment = reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreData.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectData.data());

    environment->current_time = IVP_Time(2.0);
    environment->time_of_last_psi = IVP_Time(1.98);
    environment->delta_PSI_time = 0.02;
    environment->inv_delta_PSI_time = 50.0;
    core->environment = environment;
    core->time_of_last_psi = IVP_Time(1.99);
    core->pos_world_f_core_last_psi.set_to_zero();
    core->q_world_f_core_last_psi.init();
    core->max_surface_deviation = 0.5f;

    object->physical_core = core;
    object->flags = IVP_MT_MOVING | (1u << 10u);
    object->q_core_f_object = nullptr;
    InitializeEmptyHull(object);
    IVP_Hull_Manager *hull = object->get_hull_manager();
    hull->hull_value_next_psi = 1.0f;
    hull->hull_center_value_last_vpsi = 2.0f;
    hull->time_of_next_reset = 100;

    std::array<void *, 1> objects{};
    InitializeCoreObjects(core, object, objects);
    g_expectedObject = object;

    IVP_U_Quat requestedRotation;
    requestedRotation.init();
    const IVP_U_Point requestedPosition(0.1, 0.0, 0.0);
    object->beam_object_to_new_position(
        &requestedRotation, &requestedPosition, IVP_TRUE);

    EXPECT_FLOAT_EQ(core->i_delta_time, 50.0f);
    EXPECT_NEAR(g_speedSeenByRotationSolver, 5.0f, 1.0e-5);
    EXPECT_NEAR(hull->hull_value_next_psi, 1.1f, 1.0e-5);
    EXPECT_NEAR(hull->hull_center_value_last_vpsi, 2.1f, 1.0e-5);
    EXPECT_FLOAT_EQ(core->current_speed, 0.0f);
    EXPECT_FLOAT_EQ(core->abs_omega, 0.0f);
    EXPECT_FLOAT_EQ(core->max_surface_rot_speed, 0.0f);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::SetEnvironmentTime,
                  Operation::CalculateRelativeRotation,
                  Operation::WriteObjectRotation,
                  Operation::CalculateRotationAxis,
              }));
}

TEST(IvpBeamObject, RepositionsAttachedObjectInParentReferenceFrame) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Mindist_Manager)> managerData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> parentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> attachedData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Cache_Object)> parentCacheData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Cache_Object)> attachedCacheData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Mindist)> mindistData{};

    auto *environment = reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *manager = reinterpret_cast<IVP_Mindist_Manager *>(managerData.data());
    auto *core = reinterpret_cast<IVP_Core *>(coreData.data());
    auto *parent = reinterpret_cast<IVP_Real_Object *>(parentData.data());
    auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedData.data());
    auto *parentCache = reinterpret_cast<IVP_Cache_Object *>(parentCacheData.data());
    auto *attachedCache = reinterpret_cast<IVP_Cache_Object *>(attachedCacheData.data());
    auto *mindist = reinterpret_cast<IVP_Mindist *>(mindistData.data());

    environment->current_time = IVP_Time(4.0);
    environment->current_time_code = 10;
    environment->mindist_manager = manager;

    core->environment = environment;
    core->time_of_last_psi = IVP_Time(4.0);
    core->pos_world_f_core_last_psi.set(15.0, 5.0, 4.0);
    core->delta_world_f_core_psis.set_to_zero();
    core->q_world_f_core_last_psi.init();
    core->q_world_f_core_next_psi.init();
    core->i_delta_time = 50.0f;

    parent->environment = environment;
    parent->cache_object = parentCache;
    parentCache->m_world_f_object.set_identity();
    parentCache->m_world_f_object.vv.set(10.0, 0.0, 0.0);

    attached->environment = environment;
    attached->physical_core = core;
    attached->cache_object = attachedCache;
    IVP_Synapse_Real *exactSynapse = mindist->get_synapse(0);
    attached->exact_synapses = exactSynapse;
    exactSynapse->next = nullptr;
    exactSynapse->set_synapse_mindist(mindist);
    InitializeEmptyHull(attached);
    IVP_Hull_Manager *hull = attached->get_hull_manager();
    hull->hull_value_next_psi = 1.0f;
    hull->last_vpsi_time = IVP_Time(0.0);
    hull->time_of_next_reset = 100;

    g_expectedObject = attached;
    g_expectedMindist = mindist;
    IVP_U_Quat referenceRotation;
    referenceRotation.init();
    const IVP_U_Point referenceShift(2.0, 3.0, 4.0);

    EXPECT_EQ(
        IVP_OK,
        IVP_Object_Attach::reposition_object_Ros(
            parent, attached, &referenceRotation, &referenceShift, IVP_TRUE));

    EXPECT_NEAR(g_objectFromCore[9], 3.0, 1.0e-9);
    EXPECT_NEAR(g_objectFromCore[10], 2.0, 1.0e-9);
    EXPECT_NEAR(g_objectFromCore[11], 0.0, 1.0e-9);
    EXPECT_NEAR(g_objectFromCore[0], 1.0, 1.0e-9);
    EXPECT_NEAR(g_objectFromCore[4], 1.0, 1.0e-9);
    EXPECT_NEAR(g_objectFromCore[8], 1.0, 1.0e-9);
    EXPECT_FLOAT_EQ(hull->hull_value_next_psi, 1.01f);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::WriteObjectRotation,
                  Operation::MultiplyReferenceTransform,
                  Operation::InterpolateCoreRotation,
                  Operation::WriteCoreRotation,
                  Operation::InverseMultiplyCoreTransform,
                  Operation::SetObjectCoreTransform,
                  Operation::InvalidateObjectCache,
                  Operation::RecalculateExactMindist,
                  Operation::RecalculateInvalidMindists,
              }));
}

TEST(IvpBeamObject,
     AttachesSleepingObjectToMovingParentWithoutChangingWorldPose) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> parentCoreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> oldCoreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> parentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> attachedData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Cache_Object)> attachedCacheData{};

    auto *environment = reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *parentCore = reinterpret_cast<IVP_Core *>(parentCoreData.data());
    auto *oldCore = reinterpret_cast<IVP_Core *>(oldCoreData.data());
    auto *parent = reinterpret_cast<IVP_Real_Object *>(parentData.data());
    auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedData.data());
    auto *attachedCache =
        reinterpret_cast<IVP_Cache_Object *>(attachedCacheData.data());

    environment->current_time = IVP_Time(6.0);
    environment->current_time_code = 5;

    parentCore->environment = environment;
    parentCore->movement_state = IVP_MT_MOVING;
    parentCore->time_of_last_psi = IVP_Time(6.0);
    parentCore->pos_world_f_core_last_psi.set(10.0, 0.0, 0.0);
    parentCore->delta_world_f_core_psis.set_to_zero();
    parentCore->q_world_f_core_last_psi.init();
    parentCore->q_world_f_core_next_psi.init();
    parentCore->i_delta_time = 50.0f;
    parentCore->upper_limit_radius = 2.0f;
    parentCore->max_surface_deviation = 1.0f;

    oldCore->environment = environment;
    oldCore->time_of_last_psi = IVP_Time(6.0);
    oldCore->pos_world_f_core_last_psi.set(14.0, 2.0, 0.0);
    oldCore->upper_limit_radius = 1.5f;
    oldCore->max_surface_deviation = 0.25f;

    parent->environment = environment;
    parent->physical_core = parentCore;
    parent->friction_core = parentCore;
    parent->original_core = parentCore;
    parent->set_movement_state(IVP_MT_MOVING);

    attached->environment = environment;
    attached->physical_core = oldCore;
    attached->friction_core = oldCore;
    attached->original_core = oldCore;
    attached->set_movement_state(IVP_MT_NOT_SIM);
    attached->cache_object = attachedCache;
    RawCache(attachedCache).validUntilTimeCode = 5;
    attachedCache->q_world_f_object.init();
    attachedCache->m_world_f_object.set_identity();
    attachedCache->m_world_f_object.vv.set(14.0, 2.0, 0.0);
    InitializeEmptyHull(attached);
    attached->get_hull_manager()->hull_value_next_psi = 3.0f;
    attached->get_hull_manager()->time_of_next_reset = 100;

    std::array<void *, 2> parentObjects{parent, nullptr};
    parentCore->objects.elems = parentObjects.data();
    parentCore->objects.memsize = 2;
    parentCore->objects.n_elems = 1;
    std::array<void *, 1> oldObjects{attached};
    InitializeCoreObjects(oldCore, attached, oldObjects);

    g_expectedObject = attached;
    g_expectedOldCore = oldCore;
    g_expectedParentCore = parentCore;
    IVP_Object_Attach::attach_object(parent, attached);

    EXPECT_EQ(attached->physical_core, parentCore);
    EXPECT_EQ(attached->friction_core, parentCore);
    EXPECT_EQ(attached->original_core, parentCore);
    EXPECT_EQ(attached->get_movement_state(), IVP_MT_MOVING);
    ASSERT_EQ(parentCore->objects.n_elems, 2);
    EXPECT_EQ(parentCore->objects.element_at(0), parent);
    EXPECT_EQ(parentCore->objects.element_at(1), attached);
    EXPECT_EQ(oldCore->objects.n_elems, 0);
    EXPECT_NEAR(parentCore->upper_limit_radius,
                std::sqrt(20.0) + 1.5, 1.0e-5);
    EXPECT_NEAR(parentCore->max_surface_deviation,
                std::sqrt(20.0) + 0.25, 1.0e-5);
    // Core space now sits four units left and two units below the preserved
    // object origin, so the stored core-from-object shift is (-4, -2, 0).
    EXPECT_NEAR(g_objectFromCore[9], -4.0, 1.0e-9);
    EXPECT_NEAR(g_objectFromCore[10], -2.0, 1.0e-9);
    EXPECT_NEAR(g_objectFromCore[11], 0.0, 1.0e-9);
    EXPECT_FLOAT_EQ(attached->get_hull_manager()->hull_value_next_psi, 3.01f);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::ReviveAttachedObject,
                  Operation::UnlinkFromOldCore,
                  Operation::InitializeWorldReference,
                  Operation::WriteObjectRotation,
                  Operation::MultiplyReferenceTransform,
                  Operation::InterpolateCoreRotation,
                  Operation::WriteCoreRotation,
                  Operation::InverseMultiplyCoreTransform,
                  Operation::SetObjectCoreTransform,
                  Operation::InvalidateObjectCache,
                  Operation::RecalculateInvalidMindists,
                  Operation::RecalculateParentCore,
                  Operation::EnsureParentCoreSimulation,
              }));
}

TEST(IvpBeamObject,
     DetachesMovingCompoundChildIntoIndependentCoreWithSurfaceVelocity) {
    ResetObservations();

    alignas(16) std::array<std::byte, sizeof(IVP_Environment)> environmentData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Core)> sharedCoreData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)> attachedData{};
    alignas(16) std::array<std::byte, sizeof(IVP_Cache_Object)> cacheData{};
    auto *environment = reinterpret_cast<IVP_Environment *>(environmentData.data());
    auto *sharedCore = reinterpret_cast<IVP_Core *>(sharedCoreData.data());
    auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedData.data());
    auto *cache = reinterpret_cast<IVP_Cache_Object *>(cacheData.data());

    environment->current_time = IVP_Time(7.0);
    environment->time_of_next_psi = IVP_Time(7.02);
    environment->current_time_code = 12;

    sharedCore->environment = environment;
    sharedCore->movement_state = IVP_MT_MOVING;
    sharedCore->m_world_f_core_last_psi.set_identity();
    sharedCore->m_world_f_core_last_psi.vv.set(9.0, 0.0, 0.0);
    sharedCore->pos_world_f_core_last_psi.set(9.0, 0.0, 0.0);
    sharedCore->speed.set(3.0f, 0.0f, 0.0f);
    sharedCore->rot_speed.set(0.0f, 0.0f, 2.0f);

    attached->environment = environment;
    attached->physical_core = sharedCore;
    attached->friction_core = sharedCore;
    attached->original_core = sharedCore;
    attached->set_movement_state(IVP_MT_MOVING);
    attached->shift_core_f_object.set(1.0f, 0.0f, 0.0f);
    attached->cache_object = cache;
    RawCache(cache).validUntilTimeCode = 12;
    cache->q_world_f_object.init();
    cache->m_world_f_object.set_identity();
    cache->m_world_f_object.vv.set(10.0, 0.0, 0.0);
    InitializeEmptyHull(attached);
    attached->get_hull_manager()->hull_value_next_psi = 2.0f;

    std::array<void *, 1> sharedObjects{};
    InitializeCoreObjects(sharedCore, attached, sharedObjects);

    IVP_Template_Real_Object configuration;
    configuration.physical_unmoveable = IVP_FALSE;
    configuration.enable_piling_optimization = IVP_TRUE;
    configuration.extra_radius = 0.4f;
    configuration.material = reinterpret_cast<IVP_Material *>(0x1234);
    configuration.client_data = reinterpret_cast<void *>(0x5678);
    std::memcpy(const_cast<char *>(configuration.get_nocoll_group_ident()),
                "detach", 7);

    g_expectedObject = attached;
    g_expectedOldCore = sharedCore;
    IVP_Object_Attach::detach_object(attached, &configuration);

    ASSERT_NE(g_detachedCore, nullptr);
    EXPECT_EQ(attached->physical_core, g_detachedCore);
    EXPECT_EQ(attached->friction_core, g_detachedCore);
    EXPECT_EQ(attached->original_core, g_detachedCore);
    EXPECT_EQ(g_detachedCore->objects.n_elems, 1);
    EXPECT_EQ(g_detachedCore->objects.element_at(0), attached);
    EXPECT_EQ(sharedCore->objects.n_elems, 0);
    EXPECT_EQ(g_corePolicyArgument, IVP_TRUE);
    EXPECT_STREQ(attached->nocoll_group_ident, "detach");
    EXPECT_EQ(attached->l_default_material, configuration.material);
    EXPECT_EQ(attached->client_data, configuration.client_data);
    EXPECT_FLOAT_EQ(attached->extra_radius, 0.4f);
    EXPECT_FLOAT_EQ(g_detachedCore->speed.k[0], 3.0f);
    EXPECT_FLOAT_EQ(g_detachedCore->speed.k[1], 2.0f);
    EXPECT_FLOAT_EQ(g_detachedCore->speed.k[2], 0.0f);
    EXPECT_FLOAT_EQ(g_detachedCore->rot_speed.k[2], 2.0f);
    EXPECT_FLOAT_EQ(attached->get_hull_manager()->hull_value_next_psi, 3.0f);
    EXPECT_EQ(g_operations,
              (std::vector<Operation>{
                  Operation::AllocateDetachedCore,
                  Operation::UnlinkAttachedContacts,
                  Operation::UnlinkFromOldCore,
                  Operation::ConstructDetachedCore,
                  Operation::InitializeDetachedObjectCore,
                  Operation::FireDetachedObjectFrozen,
                  Operation::ReviveAttachedObject,
                  Operation::CalculateDetachedNextPsi,
                  Operation::CommitDetachedHulls,
                  Operation::UpdateDetachedMindistEvents,
              }));

    std::free(g_detachedCore);
    g_detachedCore = nullptr;
}

} // namespace
