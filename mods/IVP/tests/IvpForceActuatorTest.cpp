#include "IvpTestAdapter.h"

#include "BML/IVP/Actuator.h"
#include "BML/IVP/Constraint.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>

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

struct RawTemplateAnchor {
    IVP_Real_Object *object;
    IVP_U_Point worldPosition;
};

struct PushRecord {
    IVP_Core *core;
    IVP_U_Point position;
    IVP_U_Float_Point impulse;
};

std::array<PushRecord, 8> g_pushes{};
int g_pushCount = 0;
int g_announceCount = 0;
int g_removeControllerCount = 0;
int g_addIndependentControllerCount = 0;
int g_removeIndependentControllerCount = 0;
int g_ensureSimulationCount = 0;
IVP_Controller_Independent *g_lastIndependentController = nullptr;
IVP_Core *g_lastIndependentCore = nullptr;
int g_anchorDestructCount = 0;
int g_controllerDestructCount = 0;
int g_actuatorDestructCount = 0;
int g_actuatorAllocations = 0;
int g_actuatorDeallocations = 0;
int g_stiffSpringBreakCount = 0;
int g_hullAddCount = 0;
int g_hullRemoveCount = 0;
int g_templateTwoPointConstructionCount = 0;
int g_templateSpringConstructionCount = 0;
int g_actuatorConstructionCount = 0;
int g_twoPointConstructionCount = 0;
int g_springConstructionCount = 0;
int g_activeSpringConstructionCount = 0;
int g_activeSpringChangedCount = 0;
int g_springBrokenFireCount = 0;
unsigned int g_nextHullIndex = 1;

void ResetObservations() {
    g_pushCount = 0;
    g_announceCount = 0;
    g_removeControllerCount = 0;
    g_addIndependentControllerCount = 0;
    g_removeIndependentControllerCount = 0;
    g_ensureSimulationCount = 0;
    g_lastIndependentController = nullptr;
    g_lastIndependentCore = nullptr;
    g_anchorDestructCount = 0;
    g_controllerDestructCount = 0;
    g_actuatorDestructCount = 0;
    g_actuatorAllocations = 0;
    g_actuatorDeallocations = 0;
    g_stiffSpringBreakCount = 0;
    g_hullAddCount = 0;
    g_hullRemoveCount = 0;
    g_templateTwoPointConstructionCount = 0;
    g_templateSpringConstructionCount = 0;
    g_actuatorConstructionCount = 0;
    g_twoPointConstructionCount = 0;
    g_springConstructionCount = 0;
    g_activeSpringConstructionCount = 0;
    g_activeSpringChangedCount = 0;
    g_springBrokenFireCount = 0;
    g_nextHullIndex = 1;
}

char *DuplicateString(const char *value) {
    const std::size_t size = std::strlen(value) + 1;
    auto *copy = static_cast<char *>(std::malloc(size));
    if (copy)
        std::memcpy(copy, value, size);
    return copy;
}

void *Allocate(unsigned int size) { return std::malloc(size); }
void Free(void *memory) { std::free(memory); }

void *OperatorNew(unsigned int size) {
    ++g_actuatorAllocations;
    return std::malloc(size);
}

void OperatorDelete(void *memory) {
    ++g_actuatorDeallocations;
    std::free(memory);
}

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

void __fastcall ConstructTemplateTwoPoint(
    IVP_Template_Two_Point *value, void *) {
    ++g_templateTwoPointConstructionCount;
    std::memset(value, 0, sizeof(*value));
}

void __fastcall ConstructTemplateSpring(
    IVP_Template_Spring *value, void *) {
    ++g_templateSpringConstructionCount;
    ConstructTemplateTwoPoint(value, nullptr);
    std::memset(value, 0, sizeof(*value));
    value->break_max_len = 1.0e20f;
}

void __fastcall SetAnchorWorldPosition(
    IVP_Template_Anchor *anchor, void *, IVP_Real_Object *object,
    const IVP_U_Point *position) {
    auto *raw = reinterpret_cast<RawTemplateAnchor *>(anchor);
    raw->object = object;
    raw->worldPosition = *position;
}

void __fastcall ConstructActuator(
    IVP_Actuator *actuator, void *, IVP_Environment *) {
    ++g_actuatorConstructionCount;
    ::new (static_cast<void *>(&actuator->actuator_controlled_cores))
        IVP_U_Vector<IVP_Core>();
}

void __fastcall DestructActuator(IVP_Actuator *actuator, void *) {
    ++g_actuatorDestructCount;
    actuator->actuator_controlled_cores.~IVP_U_Vector<IVP_Core>();
}

void __fastcall InitializeAnchor(
    IVP_Anchor *anchor, void *, IVP_Actuator *actuator,
    IVP_Template_Anchor *configuration) {
    const auto *raw = reinterpret_cast<const RawTemplateAnchor *>(configuration);
    anchor->anchor_next_in_object = nullptr;
    anchor->anchor_prev_in_object = nullptr;
    anchor->l_anchor_object = raw->object;
    anchor->l_actuator = actuator;

    const IVP_U_Point *corePosition =
        raw->object->get_core()->get_position_PSI();
    anchor->object_pos.set(
        static_cast<IVP_FLOAT>(
            raw->worldPosition.k[0] - corePosition->k[0]),
        static_cast<IVP_FLOAT>(
            raw->worldPosition.k[1] - corePosition->k[1]),
        static_cast<IVP_FLOAT>(
            raw->worldPosition.k[2] - corePosition->k[2]));
    anchor->core_pos.set(&anchor->object_pos);
}

void __fastcall AnnounceController(
    IVP_Controller_Manager *, void *, IVP_Controller_Dependent *) {
    ++g_announceCount;
}

void __fastcall ConstructTwoPointActuator(
    IVP_Actuator_Two_Point *actuator, void *, IVP_Environment *environment,
    IVP_Template_Two_Point *definition, IVP_ACTUATOR_TYPE) {
    ++g_twoPointConstructionCount;
    ConstructActuator(actuator, nullptr, environment);
    ::new (static_cast<void *>(actuator->get_actuator_anchor(0))) IVP_Anchor();
    ::new (static_cast<void *>(actuator->get_actuator_anchor(1))) IVP_Anchor();

    actuator->client_data = definition->client_data;
    actuator->get_actuator_anchor(0)->init_anchor(
        actuator, definition->anchors[0]);
    actuator->get_actuator_anchor(1)->init_anchor(
        actuator, definition->anchors[1]);

    IVP_Core *firstCore =
        actuator->get_actuator_anchor(0)->anchor_get_real_object()->get_core();
    IVP_Core *secondCore =
        actuator->get_actuator_anchor(1)->anchor_get_real_object()->get_core();
    if ((firstCore->flags & 0x0Cu) == 0u)
        actuator->actuator_controlled_cores.add(firstCore);
    if ((secondCore->flags & 0x0Cu) == 0u && secondCore != firstCore)
        actuator->actuator_controlled_cores.add(secondCore);
    if (firstCore->get_environment()) {
        firstCore->get_environment()->get_controller_manager()
            ->announce_controller_to_environment(actuator);
    }
}

template <typename Value>
void WriteSpringField(
    IVP_Actuator_Spring *spring, std::size_t offset,
    const Value &value) {
    std::memcpy(
        reinterpret_cast<std::byte *>(spring) + offset,
        &value, sizeof(value));
}

template <typename Value>
Value ReadSpringField(
    const IVP_Actuator_Spring *spring, std::size_t offset) {
    Value value{};
    std::memcpy(
        &value, reinterpret_cast<const std::byte *>(spring) + offset,
        sizeof(value));
    return value;
}

void __fastcall ConstructSpringActuator(
    IVP_Actuator_Spring *spring, void *, IVP_Environment *environment,
    IVP_Template_Spring *definition, IVP_ACTUATOR_TYPE actuatorType) {
    ++g_springConstructionCount;
    EXPECT_TRUE(
        actuatorType == IVP_ACTUATOR_TYPE_SPRING ||
        actuatorType == IVP_ACTUATOR_TYPE_SUSPENSION);
    ConstructTwoPointActuator(
        spring, nullptr, environment, definition, actuatorType);
    ::new (reinterpret_cast<std::byte *>(spring) + 0x90)
        IVP_U_Vector<IVP_Listener_Spring>();

    WriteSpringField(spring, 0x70, environment);
    WriteSpringField(spring, 0x74, definition->spring_len);
    IVP_FLOAT factor = 1.0f;
    if (definition->spring_values_are_relative != IVP_FALSE) {
        IVP_Core *firstCore =
            spring->get_actuator_anchor(0)->anchor_get_real_object()->get_core();
        IVP_Core *secondCore =
            spring->get_actuator_anchor(1)->anchor_get_real_object()->get_core();
        const IVP_DOUBLE firstMass = firstCore->calc_virt_mass(
            &spring->get_actuator_anchor(0)->core_pos, nullptr);
        const IVP_DOUBLE secondMass = secondCore->calc_virt_mass(
            &spring->get_actuator_anchor(1)->core_pos, nullptr);
        factor = static_cast<IVP_FLOAT>(
            (firstMass * secondMass) / (firstMass + secondMass));
    }
    WriteSpringField(spring, 0x78, factor);
    WriteSpringField(
        spring, 0x7C, definition->spring_constant * factor);
    WriteSpringField(spring, 0x80, definition->spring_damp * factor);
    WriteSpringField(spring, 0x84, definition->rel_pos_damp * factor);
    WriteSpringField(spring, 0x88, definition->break_max_len);
    WriteSpringField(spring, 0x8C, definition->max_len_exceed_type);
}

void __fastcall FireSpringBroken(
    IVP_Actuator_Spring *spring, void *) {
    ++g_springBrokenFireCount;
    auto *listeners = reinterpret_cast<IVP_U_Vector<IVP_Listener_Spring> *>(
        reinterpret_cast<std::byte *>(spring) + 0x90);
    for (int index = listeners->len() - 1; index >= 0; --index)
        listeners->element_at(index)->event_spring_broken(spring);
}

void __fastcall SetSpringLength(
    IVP_Actuator_Spring *spring, void *, IVP_DOUBLE value) {
    if (ReadSpringField<IVP_FLOAT>(spring, 0x74) == value)
        return;
    WriteSpringField(spring, 0x74, static_cast<IVP_FLOAT>(value));
    spring->ensure_actuator_in_simulation();
}

void SetScaledSpringValue(
    IVP_Actuator_Spring *spring, std::size_t offset,
    IVP_DOUBLE value) {
    const IVP_FLOAT scaled = static_cast<IVP_FLOAT>(
        value * ReadSpringField<IVP_FLOAT>(spring, 0x78));
    WriteSpringField(spring, offset, scaled);
    spring->ensure_actuator_in_simulation();
}

void __fastcall SetSpringConstant(
    IVP_Actuator_Spring *spring, void *, IVP_DOUBLE value) {
    SetScaledSpringValue(spring, 0x7C, value);
}

void __fastcall SetSpringDamping(
    IVP_Actuator_Spring *spring, void *, IVP_DOUBLE value) {
    SetScaledSpringValue(spring, 0x80, value);
}

void __fastcall SetSpringRelativeDamping(
    IVP_Actuator_Spring *spring, void *, IVP_DOUBLE value) {
    SetScaledSpringValue(spring, 0x84, value);
}

void __fastcall ConstructActiveSpringActuator(
    IVP_Actuator_Spring_Active *spring, void *,
    IVP_Environment *environment, IVP_Template_Spring *definition) {
    ++g_activeSpringConstructionCount;
    ConstructSpringActuator(
        spring, nullptr, environment, definition,
        IVP_ACTUATOR_TYPE_SPRING);

    auto *listener = static_cast<IVP_U_Active_Float_Listener *>(spring);
    const std::array<IVP_U_Active_Float *, 4> values = {
        definition->active_float_spring_len,
        definition->active_float_spring_constant,
        definition->active_float_spring_damp,
        definition->active_float_spring_rel_pos_damp,
    };
    constexpr std::array<std::size_t, 4> pointerOffsets = {
        0x9C, 0xA0, 0xA4, 0xA8};
    constexpr std::array<std::size_t, 4> valueOffsets = {
        0x74, 0x7C, 0x80, 0x84};
    for (std::size_t index = 0; index < values.size(); ++index) {
        WriteSpringField(spring, pointerOffsets[index], values[index]);
        if (!values[index])
            continue;
        values[index]->add_dependency(listener);
        const IVP_FLOAT value =
            static_cast<IVP_FLOAT>(values[index]->get_float_value());
        WriteSpringField(spring, valueOffsets[index], value);
    }
}

void __fastcall ActiveSpringFloatChanged(
    IVP_U_Active_Float_Listener *listener, void *,
    IVP_U_Active_Float *value) {
    ++g_activeSpringChangedCount;
    auto *spring = reinterpret_cast<IVP_Actuator_Spring *>(
        reinterpret_cast<std::byte *>(listener) - 0x98);
    const std::array<std::size_t, 4> pointerOffsets = {
        0x9C, 0xA0, 0xA4, 0xA8};
    bool matched = false;
    for (std::size_t index = 0; index < pointerOffsets.size(); ++index) {
        if (ReadSpringField<IVP_U_Active_Float *>(
                spring, pointerOffsets[index]) != value) {
            continue;
        }
        matched = true;
        const IVP_DOUBLE activeValue = value->get_float_value();
        switch (index) {
        case 0: spring->set_len(activeValue); return;
        case 1: spring->set_constant(activeValue); return;
        case 2: spring->set_damp(activeValue); return;
        case 3: spring->set_rel_pos_damp(activeValue); return;
        default: return;
        }
    }
    EXPECT_TRUE(matched);
}

void RemoveController(
    IVP_Controller_Dependent *, IVP_BOOL silently) {
    EXPECT_EQ(silently, IVP_TRUE);
    ++g_removeControllerCount;
}

void AddIndependentController(
    IVP_Controller_Independent *controller, IVP_Core *core) {
    ++g_addIndependentControllerCount;
    g_lastIndependentController = controller;
    g_lastIndependentCore = core;
}

void RemoveIndependentController(
    IVP_Controller_Independent *controller, IVP_Core *core) {
    ++g_removeIndependentControllerCount;
    g_lastIndependentController = controller;
    g_lastIndependentCore = core;
}

void __fastcall EnsureSimulation(IVP_Actuator *, void *) {
    ++g_ensureSimulationCount;
}

void __fastcall EnsureObjectSimulation(IVP_Real_Object *, void *) {
    ++g_ensureSimulationCount;
}

void __fastcall EnsureCoreSimulation(IVP_Core *, void *) {
    ++g_ensureSimulationCount;
}

void __fastcall DestructAnchor(IVP_Anchor *, void *) {
    ++g_anchorDestructCount;
}

void __fastcall DestructController(IVP_Controller *, void *) {
    ++g_controllerDestructCount;
}

void __fastcall SetInverseUnitQuaternion(
    IVP_U_Quat *output, void *, const IVP_U_Quat *input) {
    output->set(-input->x, -input->y, -input->z, input->w);
}

void __fastcall SetMultiplyQuaternion(
    IVP_U_Quat *output, void *,
    const IVP_U_Quat *left, const IVP_U_Quat *right) {
    output->inline_set_mult_quat(left, right);
}

void __fastcall InterpolateQuaternion(
    IVP_U_Quat *output, void *,
    const IVP_U_Quat *from, const IVP_U_Quat *to,
    IVP_DOUBLE factor) {
    output->set_interpolate_linear(from, to, factor);
    const IVP_DOUBLE length = std::sqrt(
        output->x * output->x + output->y * output->y +
        output->z * output->z + output->w * output->w);
    if (length > 1.0e-12) {
        output->x /= length;
        output->y /= length;
        output->z /= length;
        output->w /= length;
    }
}

void __fastcall WriteQuaternionMatrix(
    IVP_U_Quat *quaternion, void *, IVP_U_Matrix3 *matrix) {
    IVP_DOUBLE values[4][4]{};
    quaternion->set_matrix(values);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column)
            matrix->set_elem(row, column, values[row][column]);
    }
}

void __fastcall SetQuaternionFromMatrix(
    IVP_U_Quat *quaternion, void *, const IVP_U_Matrix3 *matrix) {
    IVP_DOUBLE values[4][4]{};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column)
            values[row][column] = matrix->get_elem(row, column);
    }
    values[3][3] = 1.0;
    quaternion->set_quaternion(values);
}

void __fastcall EnsureDependentSimulation(
    IVP_Controller_Manager *, void *, IVP_Controller_Dependent *) {
    ++g_ensureSimulationCount;
}

void __fastcall InitializeReactionTranslation(
    IVP_Solver_Core_Reaction *solver, void *,
    IVP_Core *core0, IVP_Core *core1, IVP_U_Point *,
    IVP_U_Float_Point *direction0,
    IVP_U_Float_Point *direction1,
    IVP_U_Float_Point *direction2) {
    solver->direction_ws[0] = direction0;
    solver->direction_ws[1] = direction1;
    solver->direction_ws[2] = direction2;
    std::memset(&solver->m_velocity_ds_f_impulse_ds, 0,
                sizeof(solver->m_velocity_ds_f_impulse_ds));
    solver->delta_velocity_ds.set_to_zero();
    const IVP_FLOAT inverseMass =
        (core0 ? core0->get_inv_mass() : 0.0f) +
        (core1 ? core1->get_inv_mass() : 0.0f);
    IVP_U_Float_Point *directions[3] = {
        direction0, direction1, direction2};
    for (int axis = 0; axis < 3 && directions[axis]; ++axis) {
        solver->m_velocity_ds_f_impulse_ds.set_elem(
            axis, axis, inverseMass);
        if (core0) {
            solver->delta_velocity_ds.k[axis] += static_cast<IVP_FLOAT>(
                core0->speed.dot_product(directions[axis]));
        }
        if (core1) {
            solver->delta_velocity_ds.k[axis] -= static_cast<IVP_FLOAT>(
                core1->speed.dot_product(directions[axis]));
        }
    }
}

void __fastcall TransformFloatPointToPoint(
    IVP_U_Matrix *matrix, void *, const IVP_U_Float_Point *input,
    IVP_U_Point *output) {
    matrix->inline_vmult4(input, output);
}

void __fastcall TransformFloatPoint(
    IVP_U_Matrix *matrix, void *, const IVP_U_Float_Point *input,
    IVP_U_Float_Point *output) {
    matrix->inline_vmult4(input, output);
}

void __fastcall TransformFloatVector(
    IVP_U_Matrix3 *matrix, void *, const IVP_U_Float_Point *input,
    IVP_U_Float_Point *output) {
    matrix->inline_vmult3(input, output);
}

void __fastcall GetCurrentObjectMatrix(
    const IVP_Real_Object *object, void *, IVP_U_Matrix *output) {
    *output = *object->get_core()->get_m_world_f_core_PSI();
}

void __fastcall ConstructMinList(IVP_U_Min_List *, void *, int) {}
void __fastcall DestructMinList(IVP_U_Min_List *, void *) {}

unsigned int __fastcall AddMinListElement(
    IVP_U_Min_List *, void *, void *, IVP_FLOAT) {
    ++g_hullAddCount;
    return g_nextHullIndex++;
}

void __fastcall RemoveMinListElement(
    IVP_U_Min_List *, void *, unsigned int) {
    ++g_hullRemoveCount;
}

IVP_RETURN_TYPE __fastcall FastNormalize(IVP_U_Point *point, void *) {
    const IVP_DOUBLE length = point->real_length();
    if (length <= 1.0e-12)
        return IVP_FAULT;
    point->mult(1.0 / length);
    return IVP_OK;
}

IVP_DOUBLE __fastcall FloatPointLength(
    IVP_U_Float_Point *point, void *) {
    return std::sqrt(point->quad_length());
}

void __fastcall CalculateCoreFromObject(
    IVP_Real_Object *, void *, IVP_U_Matrix *matrix) {
    matrix->set_identity();
    matrix->vv.set(1.0, 2.0, 3.0);
}

void __fastcall TransformPositionToObject(
    IVP_Cache_Object *cache, void *, const IVP_U_Point *input,
    IVP_U_Point *output) {
    cache->m_world_f_object.inline_vimult4(input, output);
}

void __fastcall InverseTransformPoint(
    IVP_U_Matrix *matrix, void *, const IVP_U_Point *input,
    IVP_U_Point *output) {
    matrix->inline_vimult4(input, output);
}

void __fastcall TransformPositionToWorld(
    IVP_Cache_Object *cache, void *, const IVP_U_Float_Point *input,
    IVP_U_Point *output) {
    cache->m_world_f_object.inline_vmult4(input, output);
}

void __fastcall PushWorld(
    IVP_Core *core, void *, const IVP_U_Point *position,
    const IVP_U_Float_Point *impulse) {
    ASSERT_LT(g_pushCount, static_cast<int>(g_pushes.size()));
    g_pushes[g_pushCount++] = PushRecord{core, *position, *impulse};
}

void __fastcall RotationalPushCore(
    IVP_Core *core, void *, const IVP_U_Float_Point *impulse) {
    IVP_U_Float_Point speedChange;
    speedChange.set_pairwise_mult(impulse, core->get_inv_rot_inertia());
    core->rot_speed.add(&speedChange);
}

IVP_DOUBLE __fastcall CalculateVirtualMass(
    const IVP_Core *core, void *, const IVP_U_Float_Point *,
    const IVP_U_Float_Point *) {
    return 1.0 / core->get_inv_mass();
}

void GetDifferentialSurfaceSpeed(
    const IVP_Core *left, const IVP_Core *right,
    const IVP_U_Float_Point *, const IVP_U_Float_Point *,
    IVP_U_Float_Point *result) {
    result->subtract(left->get_speed(), right->get_speed());
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::DuplicateString, &DuplicateString),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Allocate, &Allocate),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Free, &Free),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorNew, &OperatorNew),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorDelete, &OperatorDelete),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VectorIncrementMemory, &IncrementVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplateTwoPointConstruct, &ConstructTemplateTwoPoint),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplateSpringConstruct, &ConstructTemplateSpring),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplateAnchorSetWorldPosition, &SetAnchorWorldPosition),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorConstruct, &ConstructActuator),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorTwoPointConstruct,
        &ConstructTwoPointActuator),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringFireBroken,
        &FireSpringBroken),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringConstruct,
        &ConstructSpringActuator),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringActiveFloatChanged,
        &ActiveSpringFloatChanged),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringActiveConstruct,
        &ConstructActiveSpringActuator),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringSetLength,
        &SetSpringLength),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringSetConstant,
        &SetSpringConstant),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringSetDamping,
        &SetSpringDamping),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorSpringSetRelativePositionDamping,
        &SetSpringRelativeDamping),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorDestruct, &DestructActuator),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::AnchorInitialize, &InitializeAnchor),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerAnnounce, &AnnounceController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerRemoveFromEnvironment, &RemoveController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerAddToCore, &AddIndependentController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerRemoveFromCore, &RemoveIndependentController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerEnsureSimulation, &EnsureDependentSimulation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActuatorEnsureSimulation, &EnsureSimulation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectEnsureSimulation, &EnsureObjectSimulation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreEnsureSimulation, &EnsureCoreSimulation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::AnchorDestruct, &DestructAnchor),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerDestruct, &DestructController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionSetInverseUnit, &SetInverseUnitQuaternion),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionSetMultiply, &SetMultiplyQuaternion),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionInterpolateSmoothly, &InterpolateQuaternion),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionWriteMatrix, &WriteQuaternionMatrix),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::QuaternionSetFromMatrix, &SetQuaternionFromMatrix),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixTransformFloatPointToPoint, &TransformFloatPointToPoint),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixInverseTransformPoint,
        &InverseTransformPoint),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixTransformFloatPoint,
        &TransformFloatPoint),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Matrix3TransformFloatPoint,
        &TransformFloatVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectGetCurrentMatrix, &GetCurrentObjectMatrix),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MinListConstruct, &ConstructMinList),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MinListDestruct, &DestructMinList),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MinListAdd, &AddMinListElement),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MinListRemove, &RemoveMinListElement),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::PointFastNormalize, &FastNormalize),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::FloatPointRealLength, &FloatPointLength),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectCalculateCoreFromObject,
        &CalculateCoreFromObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CacheTransformPositionToObject,
        &TransformPositionToObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CacheTransformPositionToWorld,
        &TransformPositionToWorld),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreAsyncPushWorld, &PushWorld),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreRotationalPushCore,
        &RotationalPushCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreCalculateVirtualMass, &CalculateVirtualMass),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreGetDifferentialSurfaceSpeed, &GetDifferentialSurfaceSpeed),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SolverCoreReactionInitTranslation, &InitializeReactionTranslation),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


class ForceFixture {
public:
    ForceFixture()
        : environment(reinterpret_cast<IVP_Environment *>(
              environmentStorage.data())),
          firstCore(reinterpret_cast<IVP_Core *>(firstCoreStorage.data())),
          secondCore(reinterpret_cast<IVP_Core *>(secondCoreStorage.data())),
          firstObject(reinterpret_cast<IVP_Real_Object *>(
              firstObjectStorage.data())),
          secondObject(reinterpret_cast<IVP_Real_Object *>(
              secondObjectStorage.data())),
          controllerManager(environment) {
        environment->controller_manager = &controllerManager;
        firstCore->environment = environment;
        secondCore->environment = environment;
        firstCore->movement_state = IVP_MT_MOVING;
        secondCore->movement_state = IVP_MT_MOVING;
        firstCore->m_world_f_core_last_psi.set_identity();
        secondCore->m_world_f_core_last_psi.set_identity();
        firstCore->m_world_f_core_last_psi.vv.set(4.0, 0.0, 0.0);
        secondCore->m_world_f_core_last_psi.vv.set(0.0, 0.0, 0.0);
        firstObject->physical_core = firstCore;
        secondObject->physical_core = secondCore;
        firstObject->original_core = firstCore;
        secondObject->original_core = secondCore;
        firstObject->environment = environment;
        secondObject->environment = environment;
    }

    void ConfigureAnchors(IVP_Template_Force *definition) {
        firstAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 0.0);
        secondAnchor.set_anchor_position_ws(
            secondObject, 0.0, 0.0, 0.0);
        definition->anchors[0] = &firstAnchor;
        definition->anchors[1] = &secondAnchor;
    }

    void ConfigureTorqueAnchors(IVP_Template_Torque *definition) {
        firstAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 0.0);
        secondAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 2.0);
        definition->anchors[0] = &firstAnchor;
        definition->anchors[1] = &secondAnchor;
    }

    void ConfigureMotorAnchors(IVP_Template_Rot_Mot *definition) {
        firstAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 0.0);
        secondAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 2.0);
        definition->anchors[0] = &firstAnchor;
        definition->anchors[1] = &secondAnchor;
    }

    void ConfigureSpringAnchors(IVP_Template_Spring *definition) {
        firstAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 0.0);
        secondAnchor.set_anchor_position_ws(
            secondObject, 0.0, 0.0, 0.0);
        definition->anchors[0] = &firstAnchor;
        definition->anchors[1] = &secondAnchor;
    }

    void ConfigureStiffSpringAnchors(
        IVP_Template_Stiff_Spring *definition) {
        firstAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 0.0);
        secondAnchor.set_anchor_position_ws(
            secondObject, 0.0, 0.0, 0.0);
        definition->anchors[0] = &firstAnchor;
        definition->anchors[1] = &secondAnchor;
    }

    void ConfigureStabilizerAnchors(
        IVP_Template_Stabilizer *definition) {
        firstAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 0.0);
        secondAnchor.set_anchor_position_ws(
            secondObject, 2.0, 0.0, 0.0);
        thirdAnchor.set_anchor_position_ws(
            firstObject, 4.0, 1.0, 0.0);
        fourthAnchor.set_anchor_position_ws(
            secondObject, 0.0, 1.0, 0.0);
        definition->anchors[0] = &firstAnchor;
        definition->anchors[1] = &secondAnchor;
        definition->anchors[2] = &thirdAnchor;
        definition->anchors[3] = &fourthAnchor;
    }

    void ConfigureSuspensionAnchors(
        IVP_Template_Suspension *definition) {
        firstAnchor.set_anchor_position_ws(
            firstObject, 4.0, 0.0, 0.0);
        secondAnchor.set_anchor_position_ws(
            secondObject, 0.0, 0.0, 0.0);
        definition->anchors[0] = &firstAnchor;
        definition->anchors[1] = &secondAnchor;
    }

    alignas(IVP_Environment)
        std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    alignas(IVP_Core)
        std::array<std::byte, sizeof(IVP_Core)> firstCoreStorage{};
    alignas(IVP_Core)
        std::array<std::byte, sizeof(IVP_Core)> secondCoreStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> firstObjectStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> secondObjectStorage{};
    IVP_Environment *environment;
    IVP_Core *firstCore;
    IVP_Core *secondCore;
    IVP_Real_Object *firstObject;
    IVP_Real_Object *secondObject;
    IVP_Controller_Manager controllerManager;
    IVP_Template_Anchor firstAnchor;
    IVP_Template_Anchor secondAnchor;
    IVP_Template_Anchor thirdAnchor;
    IVP_Template_Anchor fourthAnchor;
};

class StiffSpringBreakListener final : public IVP_Listener_Stiff_Spring {
public:
    void event_stiff_spring_broken(
        IVP_Controller_Stiff_Spring *) override {
        ++g_stiffSpringBreakCount;
    }
};

class ActiveSpringHarness final : public IVP_Actuator_Spring_Active {
public:
    ActiveSpringHarness(
        IVP_Environment *environment,
        IVP_Template_Spring *definition)
        : IVP_Actuator_Spring_Active(environment, definition) {}

    void fire_broken_for_test() { fire_event_spring_broken(); }
};

class SpringBreakListener final : public IVP_Listener_Spring {
public:
    void event_spring_broken(IVP_Actuator_Spring *spring) override {
        ++calls;
        lastSpring = spring;
    }

    int calls = 0;
    IVP_Actuator_Spring *lastSpring = nullptr;
};

class CheckDistanceListener final : public IVP_Listener_Check_Dist_Event {
public:
    void check_dist_event(
        IVP_Actuator_Check_Dist *, IVP_BOOL outside) override {
        ASSERT_LT(transitionCount, 2);
        transitions[transitionCount++] = outside;
    }

    void check_dist_is_going_to_be_deleted_event(
        IVP_Actuator_Check_Dist *) override {
        ++deletionCount;
    }

    IVP_BOOL transitions[2]{};
    int transitionCount = 0;
    int deletionCount = 0;
};

class TestGolem final : public IVP_Controller_Golem {
public:
    TestGolem(
        IVP_Real_Object *object,
        const IVP_Template_Controller_Golem *definition)
        : IVP_Controller_Golem(object, definition) {}

    IVP_RETURN_TYPE resolve_for_problem(
        IVP_Event_Sim *, IVP_GOLEM_PROBLEM problem) override {
        if (problemCount < static_cast<int>(problems.size()))
            problems[problemCount++] = problem;
        return IVP_FAULT;
    }

    std::array<IVP_GOLEM_PROBLEM, 4> problems{};
    int problemCount = 0;
};

class TestFloating final : public IVP_Controller_Floating {
public:
    TestFloating(
        IVP_Real_Object *object,
        const IVP_Template_Controller_Floating *definition)
        : IVP_Controller_Floating(object, definition) {}

    IVP_RETURN_TYPE do_ray_casting(IVP_Event_Sim *) override {
        if (!ray_hit)
            return IVP_FAULT;
        set_current_distance(sampled_distance);
        return IVP_OK;
    }

    IVP_BOOL ray_hit = IVP_TRUE;
    IVP_DOUBLE sampled_distance = 0.0;
};

class CoreVectorOne final : public IVP_U_Vector<IVP_Core> {
public:
    CoreVectorOne()
        : IVP_U_Vector<IVP_Core>(
              reinterpret_cast<void **>(&element), 1) {}

    IVP_Core *element = nullptr;
};

class GolemFixture {
public:
    GolemFixture()
        : environment(reinterpret_cast<IVP_Environment *>(
              environmentStorage.data())),
          core(reinterpret_cast<IVP_Core *>(coreStorage.data())),
          object(reinterpret_cast<IVP_Real_Object *>(
              objectStorage.data())) {
        environment->current_time = IVP_Time(0.0);

        core->environment = environment;
        core->movement_state = IVP_MT_MOVING;
        core->rot_inertia.set(1.0f, 1.0f, 1.0f);
        core->rot_inertia.hesse_val = 2.0f;
        core->inv_rot_inertia.set(1.0f, 1.0f, 1.0f);
        core->inv_rot_inertia.hesse_val = 0.5f;
        core->speed.set_to_zero();
        core->rot_speed.set_to_zero();
        core->m_world_f_core_last_psi.set_identity();
        core->q_world_f_core_next_psi.init();

        object->environment = environment;
        object->physical_core = core;
        object->original_core = core;
        object->friction_core = core;
        object->flags = 1u << 10u;

        cores.add(core);
    }

    alignas(IVP_Environment)
        std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    alignas(IVP_Core)
        std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    IVP_Environment *environment;
    IVP_Core *core;
    IVP_Real_Object *object;
    CoreVectorOne cores;
};

class FixedConstraintFixture {
public:
    FixedConstraintFixture()
        : environment(reinterpret_cast<IVP_Environment *>(
              environmentStorage.data())),
          referenceCore(reinterpret_cast<IVP_Core *>(
              referenceCoreStorage.data())),
          attachedCore(reinterpret_cast<IVP_Core *>(
              attachedCoreStorage.data())),
          referenceObject(reinterpret_cast<IVP_Real_Object *>(
              referenceObjectStorage.data())),
          attachedObject(reinterpret_cast<IVP_Real_Object *>(
              attachedObjectStorage.data())),
          controllerManager(environment) {
        environment->controller_manager = &controllerManager;
        environment->current_time = IVP_Time(0.0);
        environment->current_time_code = 0;

        InitializeCore(referenceCore, 5.0);
        InitializeCore(attachedCore, 7.0);
        InitializeObject(referenceObject, referenceCore, referenceCache);
        InitializeObject(attachedObject, attachedCore, attachedCache);
    }

    void InitializeCore(IVP_Core *core, IVP_DOUBLE x) {
        core->environment = environment;
        core->flags = 0;
        core->movement_state = IVP_MT_MOVING;
        core->inv_rot_inertia.set(1.0f, 1.0f, 1.0f);
        core->inv_rot_inertia.hesse_val = 1.0f;
        core->rot_inertia.set(1.0f, 1.0f, 1.0f);
        core->rot_inertia.hesse_val = 1.0f;
        core->speed.set_to_zero();
        core->rot_speed.set_to_zero();
        core->m_world_f_core_last_psi.set_identity();
        core->m_world_f_core_last_psi.vv.set(x, 0.0, 0.0);
        core->pos_world_f_core_last_psi.set(x, 0.0, 0.0);
    }

    void InitializeObject(
        IVP_Real_Object *object, IVP_Core *core,
        IVP_Cache_Object &cache) {
        object->environment = environment;
        object->physical_core = core;
        object->original_core = core;
        object->friction_core = core;
        object->flags = IVP_MT_MOVING;
        object->cache_object = &cache;
        RawCache(&cache).validUntilTimeCode = 1;
        RawCache(&cache).referenceCount = 0;
        RawCache(&cache).object = object;
        cache.m_world_f_object = core->m_world_f_core_last_psi;
        cache.q_world_f_object.init();
    }

    alignas(IVP_Environment)
        std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    alignas(IVP_Core)
        std::array<std::byte, sizeof(IVP_Core)> referenceCoreStorage{};
    alignas(IVP_Core)
        std::array<std::byte, sizeof(IVP_Core)> attachedCoreStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> referenceObjectStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> attachedObjectStorage{};
    IVP_Environment *environment;
    IVP_Core *referenceCore;
    IVP_Core *attachedCore;
    IVP_Real_Object *referenceObject;
    IVP_Real_Object *attachedObject;
    IVP_Controller_Manager controllerManager;
    IVP_Cache_Object referenceCache{};
    IVP_Cache_Object attachedCache{};
};

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpForceActuator, CompleteSpringTemplateOwnsBaseConstruction) {
    ResetObservations();

    IVP_Template_Spring springTemplate;

    EXPECT_EQ(g_templateSpringConstructionCount, 1);
    EXPECT_EQ(g_templateTwoPointConstructionCount, 1);
    EXPECT_EQ(springTemplate.client_data, nullptr);
    EXPECT_EQ(springTemplate.anchors[0], nullptr);
    EXPECT_EQ(springTemplate.anchors[1], nullptr);
    EXPECT_FLOAT_EQ(springTemplate.break_max_len, 1.0e20f);
}

TEST(IvpForceActuator,
     ActiveSpringUsesRetainedConstructionAndTracksAllFourLiveParameters) {
    ResetObservations();
    ForceFixture fixture;
    IVP_U_Active_Terminal_Double restLength("spring_rest_length", 3.0);
    IVP_U_Active_Terminal_Double stiffness("spring_constant", 12.0);
    IVP_U_Active_Terminal_Double damping("spring_damping", 0.4);
    IVP_U_Active_Terminal_Double relativeDamping(
        "spring_relative_damping", 0.15);
    restLength.add_reference();
    stiffness.add_reference();
    damping.add_reference();
    relativeDamping.add_reference();

    IVP_Template_Spring definition;
    fixture.ConfigureSpringAnchors(&definition);
    definition.active_float_spring_len = &restLength;
    definition.active_float_spring_constant = &stiffness;
    definition.active_float_spring_damp = &damping;
    definition.active_float_spring_rel_pos_damp = &relativeDamping;

    auto *spring = new ActiveSpringHarness(fixture.environment, &definition);
    ASSERT_NE(spring, nullptr);
    EXPECT_EQ(g_activeSpringConstructionCount, 1);
    EXPECT_EQ(g_springConstructionCount, 1);
    EXPECT_EQ(g_twoPointConstructionCount, 1);
    EXPECT_EQ(g_actuatorConstructionCount, 1);
    EXPECT_EQ(g_announceCount, 1);
    EXPECT_FLOAT_EQ(spring->get_spring_length_zero_force(), 3.0f);
    EXPECT_FLOAT_EQ(spring->get_constant(), 12.0f);
    EXPECT_FLOAT_EQ(spring->get_damp_factor(), 0.4f);
    EXPECT_FLOAT_EQ(spring->get_rel_pos_damp(), 0.15f);

    SpringBreakListener breakListener;
    spring->add_listener_spring(&breakListener);
    spring->fire_broken_for_test();
    EXPECT_EQ(g_springBrokenFireCount, 1);
    EXPECT_EQ(breakListener.calls, 1);
    EXPECT_EQ(breakListener.lastSpring, spring);

    restLength.set_double(3.5);
    stiffness.set_double(18.0);
    damping.set_double(0.55);
    relativeDamping.set_double(0.2);
    EXPECT_EQ(g_activeSpringChangedCount, 4);
    EXPECT_EQ(g_ensureSimulationCount, 4);
    EXPECT_FLOAT_EQ(spring->get_spring_length_zero_force(), 3.5f);
    EXPECT_FLOAT_EQ(spring->get_constant(), 18.0f);
    EXPECT_FLOAT_EQ(spring->get_damp_factor(), 0.55f);
    EXPECT_FLOAT_EQ(spring->get_rel_pos_damp(), 0.2f);

    delete spring;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_actuatorDestructCount, 1);
    EXPECT_EQ(g_actuatorAllocations, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);

    restLength.set_double(4.0);
    stiffness.set_double(24.0);
    damping.set_double(0.7);
    relativeDamping.set_double(0.3);
    EXPECT_EQ(g_activeSpringChangedCount, 4);
    EXPECT_EQ(g_ensureSimulationCount, 4);
}

TEST(IvpForceActuator,
     MovingAnchorUpdatesObjectAndCoreCoordinatesWithoutChangingOwnership) {
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    alignas(IVP_Cache_Object)
        std::array<std::byte, sizeof(IVP_Cache_Object)> cacheStorage{};
    alignas(IVP_Anchor)
        std::array<std::byte, sizeof(IVP_Anchor)> anchorStorage{};
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    auto *cache = reinterpret_cast<IVP_Cache_Object *>(cacheStorage.data());
    auto *anchor = reinterpret_cast<IVP_Anchor *>(anchorStorage.data());

    cache->m_world_f_object.set_identity();
    cache->m_world_f_object.vv.set(10.0, 20.0, 30.0);
    object->cache_object = cache;
    object->flags = IVP_MT_NOT_SIM;
    anchor->l_anchor_object = object;
    anchor->anchor_next_in_object = reinterpret_cast<IVP_Anchor *>(0x1234);
    anchor->anchor_prev_in_object = reinterpret_cast<IVP_Anchor *>(0x5678);
    IVP_Actuator *const actuator =
        reinterpret_cast<IVP_Actuator *>(0x9ABC);
    anchor->l_actuator = actuator;

    IVP_U_Point worldPosition(13.0, 24.0, 35.0);
    EXPECT_EQ(anchor->move_anchor(&worldPosition), anchor);

    EXPECT_FLOAT_EQ(anchor->object_pos.k[0], 3.0f);
    EXPECT_FLOAT_EQ(anchor->object_pos.k[1], 4.0f);
    EXPECT_FLOAT_EQ(anchor->object_pos.k[2], 5.0f);
    EXPECT_FLOAT_EQ(anchor->core_pos.k[0], 4.0f);
    EXPECT_FLOAT_EQ(anchor->core_pos.k[1], 6.0f);
    EXPECT_FLOAT_EQ(anchor->core_pos.k[2], 8.0f);
    EXPECT_EQ(anchor->anchor_next_in_object,
              reinterpret_cast<IVP_Anchor *>(0x1234));
    EXPECT_EQ(anchor->anchor_prev_in_object,
              reinterpret_cast<IVP_Anchor *>(0x5678));
    EXPECT_EQ(anchor->l_anchor_object, object);
    EXPECT_EQ(anchor->l_actuator, actuator);
}

TEST(IvpForceActuator,
     AppliesEqualAndOppositeWorldImpulsesAndHonorsBodyState) {
    ResetObservations();
    ForceFixture fixture;
    IVP_Template_Force definition;
    fixture.ConfigureAnchors(&definition);
    definition.force = 12.0f;
    definition.push_first_object = IVP_TRUE;
    definition.push_second_object = IVP_TRUE;

    IVP_Actuator_Force *actuator = fixture.environment->create_force(&definition);
    ASSERT_NE(actuator, nullptr);
    ASSERT_EQ(actuator->get_associated_controlled_cores()->len(), 2);
    EXPECT_EQ(
        actuator->get_associated_controlled_cores()->element_at(0),
        fixture.firstCore);
    EXPECT_EQ(
        actuator->get_associated_controlled_cores()->element_at(1),
        fixture.secondCore);
    EXPECT_EQ(g_announceCount, 1);
    EXPECT_EQ(g_twoPointConstructionCount, 1);
    EXPECT_EQ(g_actuatorConstructionCount, 1);
    EXPECT_EQ(g_actuatorAllocations, 1);

    IVP_Event_Sim event(fixture.environment, 0.25);
    actuator->do_simulation_controller(&event, nullptr);

    ASSERT_EQ(g_pushCount, 2);
    EXPECT_EQ(g_pushes[0].core, fixture.firstCore);
    EXPECT_DOUBLE_EQ(g_pushes[0].position.k[0], 4.0);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[0], 3.0f);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[1], 0.0f);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[2], 0.0f);
    EXPECT_EQ(g_pushes[1].core, fixture.secondCore);
    EXPECT_DOUBLE_EQ(g_pushes[1].position.k[0], 0.0);
    EXPECT_FLOAT_EQ(g_pushes[1].impulse.k[0], -3.0f);

    fixture.firstCore->movement_state = IVP_MT_NOT_SIM;
    fixture.secondCore->flags |= 1u << 8u;
    actuator->do_simulation_controller(&event, nullptr);
    EXPECT_EQ(g_pushCount, 2);

    actuator->set_force(12.0);
    EXPECT_EQ(g_ensureSimulationCount, 0);
    actuator->set_force(20.0);
    EXPECT_EQ(g_ensureSimulationCount, 1);
    EXPECT_DOUBLE_EQ(actuator->get_force(), 20.0);

    delete actuator;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpForceActuator,
     TracksActiveForceChangesAndDetachesBeforeDestruction) {
    ResetObservations();
    ForceFixture fixture;
    IVP_U_Active_Terminal_Double commandedForce("commanded_force", 5.0);
    commandedForce.add_reference();

    IVP_Template_Force definition;
    fixture.ConfigureAnchors(&definition);
    definition.force = 5.0f;
    definition.active_float_force = &commandedForce;
    definition.push_first_object = IVP_TRUE;

    IVP_Actuator_Force *actuator = fixture.environment->create_force(&definition);
    ASSERT_NE(actuator, nullptr);
    EXPECT_DOUBLE_EQ(actuator->get_force(), 5.0);
    EXPECT_EQ(g_ensureSimulationCount, 0);

    commandedForce.set_double(8.0);
    EXPECT_DOUBLE_EQ(actuator->get_force(), 8.0);
    EXPECT_EQ(g_ensureSimulationCount, 1);

    IVP_Event_Sim event(fixture.environment, 0.5);
    actuator->do_simulation_controller(&event, nullptr);
    ASSERT_EQ(g_pushCount, 1);
    EXPECT_EQ(g_pushes[0].core, fixture.firstCore);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[0], 4.0f);

    delete actuator;
    commandedForce.set_double(11.0);
    EXPECT_EQ(g_ensureSimulationCount, 1);
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpTorqueActuator,
     AppliesAxisTorqueBelowSpeedLimitAndPublishesMeasuredSpeed) {
    ResetObservations();
    ForceFixture fixture;
    fixture.firstCore->inv_rot_inertia.set(0.5f, 1.0f, 2.0f);
    fixture.firstCore->rot_speed.set(0.0f, 0.0f, 1.5f);

    IVP_U_Active_Terminal_Double measuredSpeed("measured_speed", 0.0);
    measuredSpeed.add_reference();
    IVP_Template_Torque definition;
    fixture.ConfigureTorqueAnchors(&definition);
    definition.torque = 6.0f;
    definition.max_rotation_speed = 2.0f;
    definition.active_float_rotation_speed_out = &measuredSpeed;

    IVP_Actuator_Torque *actuator =
        fixture.environment->create_torque(&definition);
    ASSERT_NE(actuator, nullptr);
    EXPECT_EQ(actuator->get_associated_controlled_cores()->len(), 1);
    EXPECT_EQ(g_announceCount, 1);
    EXPECT_EQ(g_ensureSimulationCount, 1);

    IVP_Event_Sim event(fixture.environment, 0.25);
    static_cast<IVP_Controller *>(actuator)->do_simulation_controller(
        &event, nullptr);
    EXPECT_FLOAT_EQ(actuator->rot_speed_out, 1.5f);
    EXPECT_DOUBLE_EQ(measuredSpeed.give_double_value(), 1.5);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[0], 0.0f);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[1], 0.0f);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 3.0f);

    fixture.firstCore->rot_speed.set(0.0f, 0.0f, 3.0f);
    fixture.firstCore->rot_speed_change.set_to_zero();
    static_cast<IVP_Controller *>(actuator)->do_simulation_controller(
        &event, nullptr);
    EXPECT_FLOAT_EQ(actuator->rot_speed_out, 3.0f);
    EXPECT_DOUBLE_EQ(measuredSpeed.give_double_value(), 3.0);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 0.0f);

    actuator->set_torque(6.0);
    EXPECT_EQ(g_ensureSimulationCount, 1);
    actuator->set_torque(8.0);
    actuator->set_max_rotation_speed(4.0);
    EXPECT_EQ(g_ensureSimulationCount, 3);
    EXPECT_FLOAT_EQ(actuator->get_torque(), 8.0f);

    delete actuator;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpTorqueActuator,
     FollowsActiveTorqueAndSpeedLimitThenDetachesBothInputs) {
    ResetObservations();
    ForceFixture fixture;
    fixture.firstCore->inv_rot_inertia.set(1.0f, 1.0f, 2.0f);
    fixture.firstCore->rot_speed.set(0.0f, 0.0f, 3.0f);

    IVP_U_Active_Terminal_Double commandedTorque("commanded_torque", 5.0);
    IVP_U_Active_Terminal_Double speedLimit("speed_limit", 10.0);
    commandedTorque.add_reference();
    speedLimit.add_reference();

    IVP_Template_Torque definition;
    fixture.ConfigureTorqueAnchors(&definition);
    definition.torque = 5.0f;
    definition.max_rotation_speed = 10.0f;
    definition.active_float_torque = &commandedTorque;
    definition.active_float_max_rotation_speed = &speedLimit;

    IVP_Actuator_Torque *actuator =
        fixture.environment->create_torque(&definition);
    ASSERT_NE(actuator, nullptr);
    EXPECT_EQ(g_ensureSimulationCount, 1);

    commandedTorque.set_double(8.0);
    speedLimit.set_double(2.0);
    EXPECT_FLOAT_EQ(actuator->get_torque(), 8.0f);
    EXPECT_EQ(g_ensureSimulationCount, 3);

    IVP_Event_Sim event(fixture.environment, 0.25);
    static_cast<IVP_Controller *>(actuator)->do_simulation_controller(
        &event, nullptr);
    EXPECT_FLOAT_EQ(actuator->rot_speed_out, 3.0f);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 0.0f);

    speedLimit.set_double(4.0);
    static_cast<IVP_Controller *>(actuator)->do_simulation_controller(
        &event, nullptr);
    EXPECT_EQ(g_ensureSimulationCount, 4);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 4.0f);

    delete actuator;
    commandedTorque.set_double(12.0);
    speedLimit.set_double(20.0);
    EXPECT_EQ(g_ensureSimulationCount, 4);
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpRotationMotor,
     ConvertsPowerToClippedTorqueAndHandlesDirectionAndSpeedLimit) {
    ResetObservations();
    ForceFixture fixture;
    fixture.firstCore->rot_inertia.set(2.0f, 2.0f, 2.0f);
    fixture.firstCore->inv_rot_inertia.set(0.5f, 0.5f, 0.5f);
    fixture.firstCore->rot_speed.set(0.0f, 0.0f, 0.5f);

    IVP_U_Active_Terminal_Double measuredSpeed("motor_speed", 0.0);
    measuredSpeed.add_reference();
    IVP_Template_Rot_Mot definition;
    fixture.ConfigureMotorAnchors(&definition);
    definition.max_rotation_speed = 2.0f;
    definition.power = 20.0f;
    definition.max_torque = 4.0f;
    definition.active_float_rotation_speed_out = &measuredSpeed;

    IVP_Actuator_Rot_Mot *motor =
        fixture.environment->create_rotmot(&definition);
    ASSERT_NE(motor, nullptr);
    EXPECT_EQ(motor->get_associated_controlled_cores()->len(), 1);
    EXPECT_EQ(g_announceCount, 1);
    EXPECT_EQ(g_ensureSimulationCount, 0);

    IVP_Event_Sim event(fixture.environment, 0.25);
    motor->do_simulation_controller(&event, nullptr);
    EXPECT_FLOAT_EQ(motor->rot_speed_out, 0.5f);
    EXPECT_DOUBLE_EQ(measuredSpeed.give_double_value(), 0.5);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 0.5f);

    fixture.firstCore->rot_speed.set(0.0f, 0.0f, -1.0f);
    fixture.firstCore->rot_speed_change.set_to_zero();
    motor->do_simulation_controller(&event, nullptr);
    EXPECT_FLOAT_EQ(motor->rot_speed_out, 0.0f);
    EXPECT_DOUBLE_EQ(measuredSpeed.give_double_value(), 0.0);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 0.5f);

    fixture.firstCore->rot_speed.set(0.0f, 0.0f, 3.0f);
    fixture.firstCore->rot_speed_change.set_to_zero();
    motor->do_simulation_controller(&event, nullptr);
    EXPECT_FLOAT_EQ(motor->rot_speed_out, 3.0f);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 0.0f);

    motor->set_power(0.0);
    EXPECT_EQ(g_ensureSimulationCount, 1);
    motor->do_simulation_controller(&event, nullptr);
    EXPECT_FLOAT_EQ(motor->rot_speed_out, 3.0f);

    delete motor;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpRotationMotor,
     FollowsThreeActiveInputsAndDetachesThemAtDeletion) {
    ResetObservations();
    ForceFixture fixture;
    fixture.firstCore->rot_inertia.set(2.0f, 2.0f, 2.0f);
    fixture.firstCore->inv_rot_inertia.set(0.5f, 0.5f, 0.5f);
    fixture.firstCore->rot_speed.set(0.0f, 0.0f, 1.0f);

    IVP_U_Active_Terminal_Double speedLimit("motor_limit", 10.0);
    IVP_U_Active_Terminal_Double commandedPower("motor_power", 20.0);
    IVP_U_Active_Terminal_Double torqueLimit("motor_torque", 4.0);
    speedLimit.add_reference();
    commandedPower.add_reference();
    torqueLimit.add_reference();

    IVP_Template_Rot_Mot definition;
    fixture.ConfigureMotorAnchors(&definition);
    definition.max_rotation_speed = 10.0f;
    definition.power = 20.0f;
    definition.max_torque = 4.0f;
    definition.active_float_max_rotation_speed = &speedLimit;
    definition.active_float_power = &commandedPower;
    definition.active_float_max_torque = &torqueLimit;

    IVP_Actuator_Rot_Mot *motor =
        fixture.environment->create_rotmot(&definition);
    ASSERT_NE(motor, nullptr);
    EXPECT_EQ(g_ensureSimulationCount, 0);

    speedLimit.set_double(2.0);
    commandedPower.set_double(30.0);
    torqueLimit.set_double(5.0);
    EXPECT_EQ(g_ensureSimulationCount, 3);
    EXPECT_FLOAT_EQ(motor->get_power(), 30.0f);

    IVP_Event_Sim event(fixture.environment, 0.2);
    motor->do_simulation_controller(&event, nullptr);
    EXPECT_FLOAT_EQ(motor->rot_speed_out, 1.0f);
    EXPECT_FLOAT_EQ(fixture.firstCore->rot_speed_change.k[2], 0.5f);

    delete motor;
    speedLimit.set_double(20.0);
    commandedPower.set_double(40.0);
    torqueLimit.set_double(8.0);
    EXPECT_EQ(g_ensureSimulationCount, 3);
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpStiffSpring,
     AppliesMassAdaptedRestoringAndDampingImpulsesAtTheAnchor) {
    ResetObservations();
    ForceFixture fixture;
    fixture.firstCore->inv_rot_inertia.set(1.0f, 1.0f, 1.0f);
    fixture.secondCore->inv_rot_inertia.set(1.0f, 1.0f, 1.0f);
    fixture.firstCore->inv_rot_inertia.hesse_val = 0.5f;
    fixture.secondCore->inv_rot_inertia.hesse_val = 0.25f;

    IVP_Template_Stiff_Spring definition;
    fixture.ConfigureStiffSpringAnchors(&definition);
    definition.spring_len = 6.0f;
    definition.spring_constant = 0.25f;
    definition.spring_damp = 0.5f;

    auto *spring = new IVP_Controller_Stiff_Spring(
        fixture.environment, &definition);
    ASSERT_NE(spring, nullptr);
    EXPECT_EQ(
        static_cast<IVP_Controller *>(spring)->get_controller_priority(),
        IVP_CP_STIFF_SPRINGS);
    EXPECT_EQ(spring->get_associated_controlled_cores()->len(), 2);
    EXPECT_EQ(g_announceCount, 1);

    IVP_Event_Sim event(fixture.environment, 0.5);
    static_cast<IVP_Controller *>(spring)->do_simulation_controller(
        &event, nullptr);
    EXPECT_NEAR(fixture.firstCore->speed.k[0], 2.0 / 3.0, 1.0e-6);
    EXPECT_NEAR(fixture.secondCore->speed.k[0], -1.0 / 3.0, 1.0e-6);

    fixture.firstCore->speed.set(1.0f, 0.0f, 0.0f);
    fixture.secondCore->speed.set(-1.0f, 0.0f, 0.0f);
    static_cast<IVP_Controller *>(spring)->do_simulation_controller(
        &event, nullptr);
    EXPECT_NEAR(fixture.firstCore->speed.k[0], 5.0 / 6.0, 1.0e-6);
    EXPECT_NEAR(fixture.secondCore->speed.k[0], -11.0 / 12.0, 1.0e-6);

    delete spring;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpStiffSpring,
     FiresTheBreakListenerAndRemovesItselfAfterOverextension) {
    ResetObservations();
    ForceFixture fixture;
    IVP_Template_Stiff_Spring definition;
    fixture.ConfigureStiffSpringAnchors(&definition);
    definition.spring_constant = 0.5f;
    definition.break_max_len = 3.0f;
    definition.max_len_exceed_type = IVP_SFE_BREAK;

    auto *spring = new IVP_Controller_Stiff_Spring(
        fixture.environment, &definition);
    StiffSpringBreakListener listener;
    spring->add_listener_stiff_spring(&listener);
    spring->add_listener_stiff_spring(&listener);

    IVP_Event_Sim event(fixture.environment, 0.5);
    static_cast<IVP_Controller *>(spring)->do_simulation_controller(
        &event, nullptr);

    EXPECT_EQ(g_stiffSpringBreakCount, 1);
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpStiffSpring,
     TracksThreeActiveParametersAndDetachesBeforeDestruction) {
    ResetObservations();
    ForceFixture fixture;
    IVP_U_Active_Terminal_Double restLength("stiff_rest_length", 5.0);
    IVP_U_Active_Terminal_Double stiffness("stiff_constant", 0.25);
    IVP_U_Active_Terminal_Double damping("stiff_damping", 0.1);
    restLength.add_reference();
    stiffness.add_reference();
    damping.add_reference();

    IVP_Template_Stiff_Spring_Active definition;
    fixture.ConfigureStiffSpringAnchors(&definition);
    definition.active_float_spring_len = &restLength;
    definition.active_float_spring_constant = &stiffness;
    definition.active_float_spring_damp = &damping;

    auto *spring = new IVP_Controller_Stiff_Spring_Active(
        fixture.environment, &definition);
    ASSERT_NE(spring, nullptr);
    EXPECT_FLOAT_EQ(spring->get_spring_length_zero_force(), 5.0f);
    EXPECT_FLOAT_EQ(spring->get_constant(), 0.25f);
    EXPECT_FLOAT_EQ(spring->get_damp_factor(), 0.1f);
    EXPECT_EQ(g_ensureSimulationCount, 0);

    restLength.set_double(6.0);
    stiffness.set_double(0.4);
    damping.set_double(0.2);
    EXPECT_FLOAT_EQ(spring->get_spring_length_zero_force(), 6.0f);
    EXPECT_FLOAT_EQ(spring->get_constant(), 0.4f);
    EXPECT_FLOAT_EQ(spring->get_damp_factor(), 0.2f);
    EXPECT_EQ(g_ensureSimulationCount, 3);

    delete spring;
    restLength.set_double(7.0);
    stiffness.set_double(0.5);
    damping.set_double(0.3);
    EXPECT_EQ(g_ensureSimulationCount, 3);
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpCheckDistance,
     TracksRangeCrossingsThroughMovingBallanceObjectsAndHullUpdates) {
    ResetObservations();
    ForceFixture fixture;
    IVP_U_Active_Terminal_Int outsideValue("distance_outside", 0);
    outsideValue.add_reference();

    IVP_Template_Check_Dist definition;
    definition.objects[0] = fixture.firstObject;
    definition.objects[1] = fixture.secondObject;
    // Keep a non-zero object-space anchor on the first object. This makes the
    // case depend on the retained MatrixInverseTransformPoint API instead of
    // accidentally passing when an unbound call leaves a zero result behind.
    definition.position_world_space[0].set(5.0, 0.0, 0.0);
    definition.position_world_space[1].set(0.0, 0.0, 0.0);
    definition.range = 4.5f;
    definition.mod_is_outside = &outsideValue;

    IVP_Actuator_Check_Dist *check =
        fixture.environment->create_check_dist(&definition);
    ASSERT_NE(check, nullptr);
    EXPECT_EQ(check->is_outside, IVP_TRUE);
    EXPECT_EQ(outsideValue.give_int_value(), 1);
    EXPECT_EQ(g_hullAddCount, 4);
    EXPECT_EQ(g_hullRemoveCount, 2);

    CheckDistanceListener listener;
    check->add_listener_check_dist_event(&listener);
    check->add_listener_check_dist_event(&listener);

    fixture.firstCore->m_world_f_core_last_psi.vv.set(2.0, 0.0, 0.0);
    check->set_range(4.5);
    ASSERT_EQ(listener.transitionCount, 1);
    EXPECT_EQ(listener.transitions[0], IVP_FALSE);
    EXPECT_EQ(check->is_outside, IVP_FALSE);
    EXPECT_EQ(outsideValue.give_int_value(), 0);

    fixture.firstCore->m_world_f_core_last_psi.vv.set(5.0, 0.0, 0.0);
    check->set_range(4.5);
    ASSERT_EQ(listener.transitionCount, 2);
    EXPECT_EQ(listener.transitions[1], IVP_TRUE);
    EXPECT_EQ(check->is_outside, IVP_TRUE);
    EXPECT_EQ(outsideValue.give_int_value(), 1);

    EXPECT_EQ(g_hullAddCount, 8);
    EXPECT_EQ(g_hullRemoveCount, 6);
    delete check;
    EXPECT_EQ(listener.deletionCount, 1);
    EXPECT_EQ(g_hullRemoveCount, 8);
    EXPECT_EQ(g_actuatorAllocations, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpStabilizer,
     CouplesTwoAnchorPairLengthsAndHonorsPinnedOrSleepingCores) {
    ResetObservations();
    ForceFixture fixture;
    fixture.firstObject->friction_core = fixture.firstCore;
    fixture.secondObject->friction_core = fixture.secondCore;

    IVP_Template_Stabilizer definition;
    fixture.ConfigureStabilizerAnchors(&definition);
    definition.stabi_constant = 10.0f;

    IVP_Actuator_Stabilizer *stabilizer =
        fixture.environment->create_stabilizer(&definition);
    ASSERT_NE(stabilizer, nullptr);
    ASSERT_EQ(stabilizer->get_associated_controlled_cores()->len(), 2);
    EXPECT_EQ(g_announceCount, 1);

    IVP_Event_Sim event(fixture.environment, 0.1);
    static_cast<IVP_Controller *>(stabilizer)->do_simulation_controller(
        &event, nullptr);
    ASSERT_EQ(g_pushCount, 4);
    EXPECT_EQ(g_pushes[0].core, fixture.secondCore);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[0], 2.0f);
    EXPECT_EQ(g_pushes[1].core, fixture.firstCore);
    EXPECT_FLOAT_EQ(g_pushes[1].impulse.k[0], -2.0f);
    EXPECT_EQ(g_pushes[2].core, fixture.secondCore);
    EXPECT_FLOAT_EQ(g_pushes[2].impulse.k[0], -2.0f);
    EXPECT_EQ(g_pushes[3].core, fixture.firstCore);
    EXPECT_FLOAT_EQ(g_pushes[3].impulse.k[0], 2.0f);

    fixture.firstCore->movement_state = IVP_MT_NOT_SIM;
    fixture.secondCore->flags |= 1u << 8u;
    static_cast<IVP_Controller *>(stabilizer)->do_simulation_controller(
        &event, nullptr);
    EXPECT_EQ(g_pushCount, 4);

    stabilizer->set_stabi_constant(10.0);
    EXPECT_EQ(g_ensureSimulationCount, 0);
    stabilizer->set_stabi_constant(12.0);
    EXPECT_EQ(g_ensureSimulationCount, 1);

    delete stabilizer;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 4);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpSuspension,
     UsesSeparateCompressionAndReboundDampingAndCapsOnlyTheBodyForce) {
    ResetObservations();
    ForceFixture fixture;

    IVP_Template_Suspension definition;
    EXPECT_FLOAT_EQ(definition.break_max_len, 1.0e9f);
    fixture.ConfigureSuspensionAnchors(&definition);
    definition.spring_len = 4.0f;
    definition.spring_damp = 2.0f;
    definition.spring_dampening_compression = 6.0f;
    definition.max_body_force = 5.0f;

    IVP_Actuator_Suspension *suspension =
        fixture.environment->create_suspension(&definition);
    ASSERT_NE(suspension, nullptr);
    ASSERT_EQ(suspension->get_associated_controlled_cores()->len(), 2);
    EXPECT_EQ(g_announceCount, 1);

    IVP_Event_Sim event(fixture.environment, 0.5);
    fixture.firstCore->speed.set(-2.0f, 0.0f, 0.0f);
    suspension->do_simulation_controller(&event, nullptr);
    ASSERT_EQ(g_pushCount, 2);
    EXPECT_EQ(g_pushes[0].core, fixture.secondCore);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[0], -6.0f);
    EXPECT_EQ(g_pushes[1].core, fixture.firstCore);
    EXPECT_FLOAT_EQ(g_pushes[1].impulse.k[0], 2.5f);

    g_pushCount = 0;
    fixture.firstCore->speed.set(2.0f, 0.0f, 0.0f);
    suspension->do_simulation_controller(&event, nullptr);
    ASSERT_EQ(g_pushCount, 2);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[0], 2.0f);
    EXPECT_FLOAT_EQ(g_pushes[1].impulse.k[0], -2.0f);

    suspension->set_spring_damp_compression(6.0f);
    suspension->set_max_body_force(5.0f);
    EXPECT_EQ(g_ensureSimulationCount, 0);
    suspension->set_spring_damp_compression(8.0f);
    suspension->set_max_body_force(3.0f);
    EXPECT_EQ(g_ensureSimulationCount, 2);

    g_pushCount = 0;
    fixture.firstCore->speed.set(-2.0f, 0.0f, 0.0f);
    suspension->do_simulation_controller(&event, nullptr);
    ASSERT_EQ(g_pushCount, 2);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[0], -8.0f);
    EXPECT_FLOAT_EQ(g_pushes[1].impulse.k[0], 1.5f);

    delete suspension;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorAllocations, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpSuspension,
     AdaptsSpringAndDampingValuesToTheReducedVirtualMass) {
    ResetObservations();
    ForceFixture fixture;
    fixture.firstCore->inv_rot_inertia.hesse_val = 0.5f;
    fixture.secondCore->inv_rot_inertia.hesse_val = 1.0f / 6.0f;

    IVP_Template_Suspension definition;
    fixture.ConfigureSuspensionAnchors(&definition);
    definition.spring_values_are_relative = IVP_TRUE;
    definition.spring_len = 2.0f;
    definition.spring_constant = 4.0f;
    definition.spring_damp = 2.0f;
    definition.spring_dampening_compression = 3.0f;
    definition.max_body_force = 100.0f;

    IVP_Actuator_Suspension *suspension =
        fixture.environment->create_suspension(&definition);
    ASSERT_NE(suspension, nullptr);
    EXPECT_FLOAT_EQ(suspension->get_constant(), 6.0f);
    EXPECT_FLOAT_EQ(suspension->get_damp_factor(), 3.0f);

    IVP_Event_Sim event(fixture.environment, 0.25);
    suspension->do_simulation_controller(&event, nullptr);
    ASSERT_EQ(g_pushCount, 2);
    EXPECT_EQ(g_pushes[0].core, fixture.secondCore);
    EXPECT_FLOAT_EQ(g_pushes[0].impulse.k[0], 3.0f);
    EXPECT_EQ(g_pushes[1].core, fixture.firstCore);
    EXPECT_FLOAT_EQ(g_pushes[1].impulse.k[0], -3.0f);

    delete suspension;
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_anchorDestructCount, 2);
    EXPECT_EQ(g_controllerDestructCount, 1);
    EXPECT_EQ(g_actuatorDeallocations, 1);
}

TEST(IvpFixedKeyframedConstraint,
     DrivesTheAttachedObjectToATargetInTheReferenceObjectFrame) {
    ResetObservations();
    FixedConstraintFixture fixture;
    IVP_Template_Constraint_Fixed_Keyframed definition;
    definition.force_factor = 0.8f;
    definition.damp_factor = 1.0f;

    {
        IVP_Constraint_Fixed_Keyframed constraint(
            fixture.referenceObject, fixture.attachedObject, &definition);
        ASSERT_EQ(static_cast<IVP_Controller *>(&constraint)
                      ->get_associated_controlled_cores()->len(), 2);
        EXPECT_EQ(constraint.get_environment(), fixture.environment);
        EXPECT_EQ(g_announceCount, 1);

        const IVP_U_Point targetReference(1.0, 0.0, 0.0);
        const IVP_U_Float_Point stopped(0.0f, 0.0f, 0.0f);
        constraint.set_prime_position_Ros(
            &targetReference, &stopped, IVP_Time(0.0));
        EXPECT_EQ(g_ensureSimulationCount, 1);

        IVP_Event_Sim event(fixture.environment, 0.1);
        static_cast<IVP_Controller *>(&constraint)
            ->do_simulation_controller(&event, nullptr);

        // Reference is at x=5 and the target is at reference-space x=1,
        // while the attached body starts at x=7.  Equal masses therefore
        // receive equal and opposite speeds that close the one-unit error.
        EXPECT_NEAR(fixture.attachedCore->speed.k[0], -4.0f, 1.0e-5f);
        EXPECT_NEAR(fixture.referenceCore->speed.k[0], 4.0f, 1.0e-5f);
        EXPECT_NEAR(
            fixture.attachedCore->speed.k[0] +
                fixture.referenceCore->speed.k[0],
            0.0f, 1.0e-6f);
        EXPECT_FLOAT_EQ(fixture.attachedCore->speed.k[1], 0.0f);
        EXPECT_FLOAT_EQ(fixture.attachedCore->speed.k[2], 0.0f);
    }

    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_controllerDestructCount, 1);
}

TEST(IvpFixedKeyframedConstraint,
     CorrectsRelativeOrientationWithBalancedAngularImpulse) {
    ResetObservations();
    FixedConstraintFixture fixture;
    IVP_Template_Constraint_Fixed_Keyframed definition;
    definition.torque_factor = 0.8f;
    definition.angular_damp_factor = 1.0f;

    {
        IVP_Constraint_Fixed_Keyframed constraint(
            fixture.referenceObject, fixture.attachedObject, &definition);
        IVP_U_Quat target;
        const IVP_DOUBLE halfAngle = 0.1;
        target.set(0.0, 0.0, std::sin(halfAngle), std::cos(halfAngle));
        constraint.set_prime_orientation_Ros(
            &target, IVP_Time(0.0));

        IVP_Event_Sim event(fixture.environment, 0.1);
        static_cast<IVP_Controller *>(&constraint)
            ->do_simulation_controller(&event, nullptr);

        // IVP's row-vector quaternion convention makes the attached-body
        // correction negative for this positive target quaternion.
        EXPECT_LT(fixture.attachedCore->rot_speed.k[2], 0.0f);
        EXPECT_GT(fixture.referenceCore->rot_speed.k[2], 0.0f);
        EXPECT_NEAR(
            fixture.attachedCore->rot_speed.k[2] +
                fixture.referenceCore->rot_speed.k[2],
            0.0f, 1.0e-6f);
        EXPECT_FLOAT_EQ(fixture.attachedCore->rot_speed.k[0], 0.0f);
        EXPECT_FLOAT_EQ(fixture.attachedCore->rot_speed.k[1], 0.0f);
    }

    EXPECT_EQ(g_ensureSimulationCount, 1);
    EXPECT_EQ(g_removeControllerCount, 1);
    EXPECT_EQ(g_controllerDestructCount, 1);
}

TEST(IvpMotionController,
     PreservesTemplateLimitsAndOwnsOneCoreThroughTargetTrackingLifecycle) {
    ResetObservations();
    GolemFixture fixture;
    IVP_Template_Controller_Motion definition;
    definition.force_factor = 0.5f;
    definition.damp_factor = 0.25f;
    definition.torque_factor = 0.75f;
    definition.angular_damp_factor = 0.125f;
    definition.max_translation_force.set(2.0f, 3.0f, 4.0f);
    definition.max_torque = 7.0f;

    IVP_Controller_Motion *observedController = nullptr;
    {
        IVP_Controller_Motion controller(fixture.object, &definition);
        observedController = &controller;

        ASSERT_EQ(g_addIndependentControllerCount, 1);
        EXPECT_EQ(g_lastIndependentController, &controller);
        EXPECT_EQ(g_lastIndependentCore, fixture.core);
        EXPECT_FLOAT_EQ(controller.get_force_factor(), 0.5f);
        EXPECT_FLOAT_EQ(controller.get_damp_factor(), 0.25f);
        EXPECT_FLOAT_EQ(controller.get_torque_factor(), 0.75f);
        EXPECT_FLOAT_EQ(controller.get_angular_damp_factor(), 0.125f);
        EXPECT_FLOAT_EQ(controller.get_max_translation_force()->k[0], 2.0f);
        EXPECT_FLOAT_EQ(controller.get_max_translation_force()->k[1], 3.0f);
        EXPECT_FLOAT_EQ(controller.get_max_translation_force()->k[2], 4.0f);
        EXPECT_FLOAT_EQ(controller.get_max_torque()->k[0], 7.0f);
        EXPECT_FLOAT_EQ(controller.get_max_torque()->k[1], 7.0f);
        EXPECT_FLOAT_EQ(controller.get_max_torque()->k[2], 7.0f);
        EXPECT_DOUBLE_EQ(controller.get_target_position_ws()->k[0], 0.0);
        EXPECT_DOUBLE_EQ(controller.get_target_position_ws()->k[1], 0.0);
        EXPECT_DOUBLE_EQ(controller.get_target_position_ws()->k[2], 0.0);
        EXPECT_DOUBLE_EQ(controller.get_target_orientation()->x, 0.0);
        EXPECT_DOUBLE_EQ(controller.get_target_orientation()->y, 0.0);
        EXPECT_DOUBLE_EQ(controller.get_target_orientation()->z, 0.0);
        EXPECT_DOUBLE_EQ(controller.get_target_orientation()->w, 1.0);

        // Ballance marks this object as having no object/core center shift, so
        // the object-space helper must preserve the supplied world target.
        IVP_U_Quat identity;
        identity.init();
        const IVP_U_Point target(0.5, 0.0, 0.0);
        controller.set_target_object_position_ws(
            fixture.object, &identity, &target);
        EXPECT_EQ(g_ensureSimulationCount, 1);
        EXPECT_DOUBLE_EQ(controller.get_target_position_ws()->k[0], 0.5);

        // Repeating an identical target is deliberately wakeup-free.
        controller.set_target_position_ws(&target);
        EXPECT_EQ(g_ensureSimulationCount, 1);

        IVP_Event_Sim event(fixture.environment, 0.1);
        IVP_Controller *base = &controller;
        base->do_simulation_controller(&event, &fixture.cores);
        EXPECT_NEAR(fixture.core->speed.k[0], 0.1f, 1.0e-6f);
        EXPECT_FLOAT_EQ(fixture.core->speed.k[1], 0.0f);
        EXPECT_FLOAT_EQ(fixture.core->speed.k[2], 0.0f);
        EXPECT_EQ(base->get_controller_priority(), IVP_CP_MOTION);

        const IVP_U_Float_Point torqueLimit(8.0f, 9.0f, 10.0f);
        const IVP_U_Float_Point forceLimit(11.0f, 12.0f, 13.0f);
        controller.set_max_torque(&torqueLimit);
        controller.set_max_translation_force(&forceLimit);
        controller.set_force_factor(0.6f);
        controller.set_damp_factor(0.3f);
        controller.set_torque_factor(0.7f);
        controller.set_angular_damp_factor(0.2f);
        EXPECT_FLOAT_EQ(controller.get_max_torque()->k[2], 10.0f);
        EXPECT_FLOAT_EQ(controller.get_max_translation_force()->k[2], 13.0f);
        EXPECT_FLOAT_EQ(controller.get_force_factor(), 0.6f);
        EXPECT_FLOAT_EQ(controller.get_damp_factor(), 0.3f);
        EXPECT_FLOAT_EQ(controller.get_torque_factor(), 0.7f);
        EXPECT_FLOAT_EQ(controller.get_angular_damp_factor(), 0.2f);

        IVP_U_Quat rotated;
        rotated.set(0.0, 0.0, std::sin(0.125), std::cos(0.125));
        controller.set_target_q_world_f_core(&rotated);
        EXPECT_EQ(g_ensureSimulationCount, 2);
        EXPECT_DOUBLE_EQ(controller.get_target_orientation()->z, rotated.z);
        EXPECT_DOUBLE_EQ(controller.get_target_orientation()->w, rotated.w);
        controller.set_target_q_world_f_core(&rotated);
        EXPECT_EQ(g_ensureSimulationCount, 2);
    }

    EXPECT_EQ(g_removeIndependentControllerCount, 1);
    EXPECT_EQ(g_lastIndependentController, observedController);
    EXPECT_EQ(g_lastIndependentCore, fixture.core);
    EXPECT_EQ(g_controllerDestructCount, 1);
}

TEST(IvpFloatingController,
     RayDistanceErrorProducesAClippedAdhesiveImpulseAndBalancedCacheLock) {
    ResetObservations();
    FixedConstraintFixture fixture;
    IVP_Template_Controller_Floating definition;
    definition.max_repulsive_force = 3.0f;
    definition.max_adhesive_force = 2.0f;
    definition.target_distance = 0.25f;
    definition.current_distance = 0.25f;

    const IVP_U_Point forcePositionWs(5.0, 2.0, 0.0);
    definition.set_position_ws(fixture.referenceObject, &forcePositionWs);
    const IVP_U_Point rayDirectionWs(0.0, 1.0, 0.0);
    definition.set_ray_direction_ws(
        fixture.referenceObject, &rayDirectionWs);
    EXPECT_DOUBLE_EQ(definition.position_os.k[0], 0.0);
    EXPECT_DOUBLE_EQ(definition.position_os.k[1], 2.0);

    {
        TestFloating controller(fixture.referenceObject, &definition);
        controller.sampled_distance = 0.75;
        EXPECT_EQ(g_addIndependentControllerCount, 1);
        EXPECT_EQ(g_lastIndependentController, &controller);
        EXPECT_EQ(g_lastIndependentCore, fixture.referenceCore);
        EXPECT_DOUBLE_EQ(controller.get_target_distance(), 0.25);
        EXPECT_DOUBLE_EQ(controller.get_current_distance(), 0.25);
        EXPECT_FLOAT_EQ(controller.get_position_os()->k[1], 2.0f);
        EXPECT_FLOAT_EQ(controller.get_ray_direction_ws()->k[1], 1.0f);

        controller.set_current_distance(0.5);
        controller.set_target_distance(0.25);
        IVP_U_Float_Point updatedRayDirection(0.0f, 1.0f, 0.0f);
        controller.set_ray_direction_ws(&updatedRayDirection);
        EXPECT_DOUBLE_EQ(controller.get_current_distance(), 0.5);
        EXPECT_DOUBLE_EQ(controller.get_target_distance(), 0.25);
        EXPECT_FLOAT_EQ(controller.get_ray_direction_ws()->k[1], 1.0f);

        IVP_Event_Sim event(fixture.environment, 0.1);
        IVP_Controller *base = &controller;
        base->do_simulation_controller(&event, nullptr);

        // Closing a 0.5 m gap in one PSI would require a force of 50. The
        // configured adhesive limit clips it to 2, hence impulse 0.2.
        EXPECT_NEAR(fixture.referenceCore->speed.k[1], 0.2f, 1.0e-6f);
        EXPECT_FLOAT_EQ(fixture.referenceCore->speed.k[0], 0.0f);
        EXPECT_EQ(RawCache(&fixture.referenceCache).referenceCount, 0);
        EXPECT_EQ(base->get_controller_priority(), IVP_CP_FLOATING);

        controller.ray_hit = IVP_FALSE;
        base->do_simulation_controller(&event, nullptr);
        EXPECT_NEAR(fixture.referenceCore->speed.k[1], 0.2f, 1.0e-6f);
        EXPECT_EQ(RawCache(&fixture.referenceCache).referenceCount, 0);
    }

    EXPECT_EQ(g_removeIndependentControllerCount, 1);
    EXPECT_EQ(g_lastIndependentCore, fixture.referenceCore);
    EXPECT_EQ(g_controllerDestructCount, 1);
}

TEST(IvpWorldFrictionController,
     ApproachesMovingPlatformSpeedsWithinPerAxisLinearAndAngularLimits) {
    ResetObservations();
    GolemFixture fixture;
    fixture.core->speed.set(3.0f, -1.0f, 0.1f);
    fixture.core->rot_speed.set(2.0f, -2.0f, 0.2f);

    IVP_Template_Controller_World_Friction definition;
    definition.desired_speed_ws.set(1.0, 0.0, 0.0);
    definition.desired_rot_speed_cs.set_to_zero();
    definition.friction_value_translation.set(4.0, 2.0, 0.5);
    definition.friction_value_rotation.set(1.0, 3.0, 0.4);

    {
        IVP_Controller_World_Friction controller(
            fixture.object, &definition);
        ASSERT_EQ(g_addIndependentControllerCount, 1);
        EXPECT_EQ(g_lastIndependentController, &controller);
        EXPECT_EQ(g_lastIndependentCore, fixture.core);

        IVP_U_Float_Point desiredSpeed(1.0f, 0.0f, 0.0f);
        IVP_U_Float_Point desiredRotation(0.0f, 0.0f, 0.0f);
        IVP_U_Point translationLimit(4.0, 2.0, 0.5);
        IVP_U_Point rotationLimit(1.0, 3.0, 0.4);
        controller.set_desired_speed_ws(&desiredSpeed);
        controller.set_desired_rot_speed_cs(&desiredRotation);
        controller.set_friction_value_translation(translationLimit);
        controller.set_friction_value_rotation(rotationLimit);
        EXPECT_FLOAT_EQ(controller.get_desired_speed_ws()->k[0], 1.0f);
        EXPECT_FLOAT_EQ(controller.get_desired_rot_speed_cs()->k[0], 0.0f);

        IVP_Event_Sim event(fixture.environment, 0.25);
        ASSERT_FLOAT_EQ(fixture.core->get_mass(), 2.0f);
        ASSERT_FLOAT_EQ(fixture.core->get_inv_mass(), 0.5f);
        ASSERT_DOUBLE_EQ(event.delta_time, 0.25);
        IVP_U_Float_Point expectedTranslationLimit;
        expectedTranslationLimit.set_multiple(
            controller.get_friction_value_translation(), event.delta_time);
        ASSERT_FLOAT_EQ(expectedTranslationLimit.k[0], 1.0f);
        ASSERT_FLOAT_EQ(expectedTranslationLimit.k[1], 0.5f);
        IVP_Controller *base = &controller;
        base->do_simulation_controller(&event, nullptr);

        // Per-axis maximum speed corrections are friction * dtime. The
        // identity core transform makes the expected world/core values equal.
        EXPECT_NEAR(fixture.core->speed.k[0], 2.0f, 1.0e-6f);
        EXPECT_NEAR(fixture.core->speed.k[1], -0.5f, 1.0e-6f);
        EXPECT_NEAR(fixture.core->speed.k[2], 0.0f, 1.0e-6f);
        EXPECT_NEAR(fixture.core->rot_speed.k[0], 1.75f, 1.0e-6f);
        EXPECT_NEAR(fixture.core->rot_speed.k[1], -1.25f, 1.0e-6f);
        EXPECT_NEAR(fixture.core->rot_speed.k[2], 0.1f, 1.0e-6f);
        EXPECT_EQ(base->get_controller_priority(), IVP_CP_CONSTRAINTS_MAX);
        EXPECT_FLOAT_EQ(
            controller.get_friction_value_translation()->k[0], 4.0f);
        EXPECT_FLOAT_EQ(
            controller.get_friction_value_rotation()->k[1], 3.0f);
    }

    EXPECT_EQ(g_removeIndependentControllerCount, 1);
    EXPECT_EQ(g_lastIndependentCore, fixture.core);
    EXPECT_EQ(g_controllerDestructCount, 1);
}

TEST(IvpGolemController,
     TracksMovingTargetWhileRespectingPerAxisForceLimit) {
    ResetObservations();
    GolemFixture fixture;
    IVP_Template_Controller_Golem definition;
    definition.max_delta_position = 10.0f;
    definition.max_translation_force.set(2.0f, 1000.0f, 1000.0f);

    {
        TestGolem golem(fixture.object, &definition);
        EXPECT_EQ(g_addIndependentControllerCount, 1);

        const IVP_U_Point target(0.5, 0.0, 0.0);
        const IVP_U_Float_Point velocity(0.2f, 0.0f, 0.0f);
        golem.set_prime_position(&target, &velocity, IVP_Time(0.0));
        EXPECT_EQ(g_ensureSimulationCount, 1);

        fixture.environment->current_time = IVP_Time(0.5);
        IVP_Event_Sim event(fixture.environment, 0.1);
        IVP_Controller *controller = &golem;
        controller->do_simulation_controller(&event, &fixture.cores);

        EXPECT_EQ(golem.problemCount, 0);
        EXPECT_NEAR(fixture.core->speed.k[0], 0.1f, 1.0e-6f);
        EXPECT_FLOAT_EQ(fixture.core->speed.k[1], 0.0f);
        EXPECT_FLOAT_EQ(fixture.core->speed.k[2], 0.0f);
    }

    EXPECT_EQ(g_removeIndependentControllerCount, 1);
    EXPECT_EQ(g_controllerDestructCount, 1);
}

TEST(IvpGolemController,
     RoutesUnsafeDistanceAndOrientationThroughModPolicy) {
    ResetObservations();
    GolemFixture fixture;
    IVP_Template_Controller_Golem definition;
    definition.max_delta_position = 0.25f;
    definition.max_delta_orientation = 0.2f;

    TestGolem golem(fixture.object, &definition);
    IVP_Controller *controller = &golem;
    IVP_Event_Sim event(fixture.environment, 0.1);

    const IVP_U_Point distantTarget(1.0, 0.0, 0.0);
    const IVP_U_Float_Point stopped(0.0f, 0.0f, 0.0f);
    golem.set_prime_position(
        &distantTarget, &stopped, IVP_Time(0.0));
    controller->do_simulation_controller(&event, &fixture.cores);
    ASSERT_EQ(golem.problemCount, 1);
    EXPECT_EQ(golem.problems[0], IVP_GP_FAR_DISTANCE);
    EXPECT_FLOAT_EQ(fixture.core->speed.k[0], 0.0f);

    const IVP_U_Point currentTarget(0.0, 0.0, 0.0);
    golem.set_prime_position(
        &currentTarget, &stopped, IVP_Time(0.0));
    const IVP_DOUBLE halfAngle = 0.25 * std::acos(-1.0);
    IVP_U_Quat rotated;
    rotated.set(
        std::sin(halfAngle), 0.0, 0.0, std::cos(halfAngle));
    golem.set_prime_orientation(
        &rotated, IVP_Time(0.0));
    controller->do_simulation_controller(&event, &fixture.cores);

    ASSERT_EQ(golem.problemCount, 2);
    EXPECT_EQ(golem.problems[1], IVP_GP_BIG_ANGLE);
    EXPECT_FLOAT_EQ(fixture.core->rot_speed.k[0], 0.0f);
}

} // namespace
