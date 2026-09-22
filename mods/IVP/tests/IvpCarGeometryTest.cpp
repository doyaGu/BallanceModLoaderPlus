#include "IvpTestAdapter.h"

#include "BML/IVP/ConstraintCar.h"
#include "BML/IVP/RaycastCar.h"
#include "BML/IVP/RealWheelsCar.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

int g_vectorAllocations = 0;
int g_vectorFrees = 0;
int g_announceCount = 0;
int g_ensureSimulationCount = 0;
int g_removeCount = 0;
int g_controllerDestructCount = 0;
int g_actuatorEnsureCount = 0;
int g_realObjectEnsureCount = 0;
int g_createConstraintCount = 0;
IVP_Real_Object *g_constraintReference = nullptr;
IVP_Real_Object *g_constraintAttached = nullptr;
std::array<IVP_CONSTRAINT_AXIS_TYPE, 6> g_constraintAxes{};
IVP_Controller_Dependent *g_announcedController = nullptr;
IVP_Controller_Dependent *g_ensuredController = nullptr;
IVP_Controller_Dependent *g_removedController = nullptr;
IVP_Controller *g_destructedController = nullptr;

void *Allocate(unsigned int size) {
  ++g_vectorAllocations;
  return std::malloc(size);
}

void Free(void *memory) {
  if (memory)
    ++g_vectorFrees;
  std::free(memory);
}

void ResetControllerObservations() {
  g_announceCount = 0;
  g_ensureSimulationCount = 0;
  g_removeCount = 0;
  g_controllerDestructCount = 0;
  g_announcedController = nullptr;
  g_ensuredController = nullptr;
  g_removedController = nullptr;
  g_destructedController = nullptr;
  g_actuatorEnsureCount = 0;
  g_realObjectEnsureCount = 0;
  g_createConstraintCount = 0;
  g_constraintReference = nullptr;
  g_constraintAttached = nullptr;
  g_constraintAxes.fill(IVP_CONSTRAINT_AXIS_FREE);
}

struct RawTemplateAnchor {
  IVP_Real_Object *object;
  IVP_U_Point worldPosition;
};

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

void *OperatorNew(unsigned int size) { return std::malloc(size); }
void OperatorDelete(void *memory) { std::free(memory); }

void __fastcall ConstructTemplateTwoPoint(IVP_Template_Two_Point *value,
                                           void *) {
  std::memset(value, 0, sizeof(*value));
}
void __fastcall ConstructTemplateSpring(IVP_Template_Spring *value, void *) {
  ConstructTemplateTwoPoint(value, nullptr);
  std::memset(value, 0, sizeof(*value));
  value->break_max_len = 1.0e20f;
}
void __fastcall ConstructActuator(IVP_Actuator *actuator, void *,
                                  IVP_Environment *) {
  ::new (static_cast<void *>(&actuator->actuator_controlled_cores))
      IVP_U_Vector<IVP_Core>();
}
void __fastcall DestructActuator(IVP_Actuator *actuator, void *) {
  actuator->actuator_controlled_cores.~IVP_U_Vector<IVP_Core>();
}
void __fastcall InitializeAnchor(IVP_Anchor *anchor, void *,
                                 IVP_Actuator *actuator,
                                 IVP_Template_Anchor *configuration) {
  const auto *raw = reinterpret_cast<const RawTemplateAnchor *>(configuration);
  anchor->anchor_next_in_object = nullptr;
  anchor->anchor_prev_in_object = nullptr;
  anchor->l_anchor_object = raw->object;
  anchor->object_pos.set_to_zero();
  anchor->core_pos.set_to_zero();
  anchor->l_actuator = actuator;
}
void __fastcall DestructAnchor(IVP_Anchor *, void *) {}
void __fastcall EnsureActuatorSimulation(IVP_Actuator *, void *) {
  ++g_actuatorEnsureCount;
}
void __fastcall EnsureRealObjectSimulation(IVP_Real_Object *, void *) {
  ++g_realObjectEnsureCount;
}
void __fastcall ConstructTemplateConstraint(IVP_Template_Constraint *value,
                                            void *) {
  value->flags = IVP_CONSTRAINT_ACTIVATED;
  value->objectR = nullptr;
  value->objectA = nullptr;
  value->m_Ros_f_Rfs = nullptr;
  value->m_Ros_f_Rrs = nullptr;
  value->m_Aos_f_Afs = nullptr;
  value->mm_Ros_f_Rfs.set_identity();
  value->mm_Ros_f_Rrs.set_identity();
  value->mm_Aos_f_Afs.set_identity();
  value->force_factor = 1.0f;
  value->damp_factor = 1.0f;
  value->limited_axis_stiffness = 0.3f;
  for (int axis = 0; axis < 6; ++axis) {
    value->axis_type[axis] = axis < 3 ? IVP_CONSTRAINT_AXIS_FIXED
                                     : IVP_CONSTRAINT_AXIS_FREE;
    value->borderleft_Rfs[axis] = 0.0f;
    value->borderright_Rfs[axis] = 0.0f;
    value->maximpulse_type[axis] = IVP_CFE_NONE;
    value->maximpulse[axis] = 0.0f;
  }
}
void __fastcall ConstructConstraint(IVP_Constraint *value, void *) {
  auto *raw = reinterpret_cast<std::byte *>(value);
  *reinterpret_cast<std::uint16_t *>(raw + 0x08) = 2;
  *reinterpret_cast<std::uint16_t *>(raw + 0x0A) = 0;
  *reinterpret_cast<void ***>(raw + 0x0C) =
      reinterpret_cast<void **>(raw + 0x10);
  auto *flags = reinterpret_cast<std::uint32_t *>(raw + 0x04);
  *flags = (*flags & ~std::uint32_t{3}) | std::uint32_t{1};
}
class TestWheelConstraint final : public IVP_Constraint {
public:
  TestWheelConstraint() { is_enabled = IVP_TRUE; }

  void do_simulation_controller(IVP_Event_Sim *,
                                IVP_U_Vector<IVP_Core> *) override {}
};
IVP_Constraint *__fastcall CreateConstraint(
    IVP_Environment *, void *, const IVP_Template_Constraint *definition) {
  ++g_createConstraintCount;
  g_constraintReference = definition->objectR;
  g_constraintAttached = definition->objectA;
  std::copy(std::begin(definition->axis_type), std::end(definition->axis_type),
            g_constraintAxes.begin());
  return new TestWheelConstraint();
}
void __fastcall SetSpringConstant(IVP_Actuator_Spring *spring, void *,
                                  IVP_DOUBLE value) {
  *reinterpret_cast<IVP_FLOAT *>(reinterpret_cast<std::byte *>(spring) + 0x7C) =
      static_cast<IVP_FLOAT>(value);
  EnsureActuatorSimulation(spring, nullptr);
}
void __fastcall SetSpringDamping(IVP_Actuator_Spring *spring, void *,
                                 IVP_DOUBLE value) {
  *reinterpret_cast<IVP_FLOAT *>(reinterpret_cast<std::byte *>(spring) + 0x80) =
      static_cast<IVP_FLOAT>(value);
  EnsureActuatorSimulation(spring, nullptr);
}
void __fastcall SetSpringLength(IVP_Actuator_Spring *spring, void *,
                                IVP_DOUBLE value) {
  *reinterpret_cast<IVP_FLOAT *>(reinterpret_cast<std::byte *>(spring) + 0x74) =
      static_cast<IVP_FLOAT>(value);
  EnsureActuatorSimulation(spring, nullptr);
}

IVP_DOUBLE __fastcall FloatPointLength(IVP_U_Float_Point *point, void *) {
  return std::sqrt(point->quad_length());
}

void __fastcall CalculateCoreFromObject(IVP_Real_Object *, void *,
                                        IVP_U_Matrix *output) {
  output->set_identity();
}

void __fastcall TransformFloatPoint(IVP_U_Matrix *matrix, void *,
                                    const IVP_U_Float_Point *input,
                                    IVP_U_Float_Point *output) {
  matrix->inline_vmult4(input, output);
}

void __fastcall TransformFloatPointToPoint(IVP_U_Matrix *matrix, void *,
                                           const IVP_U_Float_Point *input,
                                           IVP_U_Point *output) {
  matrix->inline_vmult4(input, output);
}

void __fastcall TransformCachePositionToWorld(
    IVP_Cache_Object *cache, void *, const IVP_U_Float_Point *input,
    IVP_U_Point *output) {
  cache->m_world_f_object.inline_vmult4(input, output);
}

void __fastcall TransformMatrix3FloatPoint(IVP_U_Matrix3 *matrix, void *,
                                           const IVP_U_Float_Point *input,
                                           IVP_U_Float_Point *output) {
  matrix->inline_vmult3(input, output);
}

void __fastcall SetOrthogonalPart(IVP_U_Float_Point *output, void *,
                                  const IVP_U_Float_Point *input,
                                  const IVP_U_Float_Point *normal) {
  output->set(input);
  output->add_multiple(normal, -input->dot_product(normal));
}

void __fastcall CrossFloatPoints(IVP_U_Float_Point *output, void *,
                                 const IVP_U_Float_Point *left,
                                 const IVP_U_Float_Point *right) {
  output->inline_calc_cross_product(left, right);
}

void __fastcall InitializeReactionTranslation(IVP_Solver_Core_Reaction *solver,
                                              void *, IVP_Core *core0,
                                              IVP_Core *, IVP_U_Point *,
                                              IVP_U_Float_Point *direction0,
                                              IVP_U_Float_Point *direction1,
                                              IVP_U_Float_Point *direction2) {
  std::memset(solver, 0, sizeof(*solver));
  solver->direction_ws[0] = direction0;
  solver->direction_ws[1] = direction1;
  solver->direction_ws[2] = direction2;
  if (!direction0)
    return;
  solver->m_velocity_ds_f_impulse_ds.set_elem(0, 0, 1.0);
  if (core0) {
    solver->delta_velocity_ds.k[0] =
        static_cast<IVP_FLOAT>(core0->speed.dot_product(direction0));
  }
}

void __fastcall ConstructGreatMatrix(IVP_Great_Matrix_Many_Zero *matrix,
                                     void *) {
  matrix->MATRIX_EPS = 1.0e-9;
  matrix->columns = 0;
  matrix->aligned_row_len = 0;
  matrix->matrix_values = nullptr;
  matrix->desired_vector = nullptr;
  matrix->result_vector = nullptr;
}

void __fastcall MultiplyGreatMatrix(IVP_Great_Matrix_Many_Zero *matrix,
                                    void *) {
  for (int row = 0; row < matrix->columns; ++row) {
    IVP_DOUBLE sum = 0.0;
    for (int column = 0; column < matrix->columns; ++column) {
      sum += matrix->matrix_values[row * matrix->aligned_row_len + column] *
             matrix->desired_vector[column];
    }
    matrix->result_vector[row] = sum;
  }
}

void CalculateSurfaceSpeed(const IVP_Core *core,
                           const IVP_U_Float_Point *positionCore,
                           const IVP_U_Float_Point *centerSpeedWorld,
                           const IVP_U_Float_Point *rotationSpeedCore,
                           IVP_U_Float_Point *resultWorld) {
  IVP_U_Float_Point positionWorld;
  IVP_U_Float_Point rotationWorld;
  core->m_world_f_core_last_psi.inline_vmult3(positionCore, &positionWorld);
  core->m_world_f_core_last_psi.inline_vmult3(rotationSpeedCore,
                                              &rotationWorld);
  IVP_U_Float_Point rotationalSpeed;
  rotationalSpeed.inline_calc_cross_product(&rotationWorld, &positionWorld);
  resultWorld->add(centerSpeedWorld, &rotationalSpeed);
}

void __fastcall GetSurfaceSpeedOnTest(
    IVP_Core *core, void *, const IVP_U_Float_Point *positionCore,
    const IVP_U_Float_Point *centerSpeedWorld,
    const IVP_U_Float_Point *rotationSpeedCore,
    IVP_U_Float_Point *resultWorld) {
  CalculateSurfaceSpeed(core, positionCore, centerSpeedWorld, rotationSpeedCore,
                        resultWorld);
}

void __fastcall GetSurfaceSpeed(IVP_Core *core, void *,
                                const IVP_U_Float_Point *positionCore,
                                IVP_U_Float_Point *resultWorld) {
  CalculateSurfaceSpeed(core, positionCore, &core->speed, &core->rot_speed,
                        resultWorld);
}

void __fastcall TestCorePush(IVP_Core *core, void *,
                             const IVP_U_Float_Point *positionCore,
                             const IVP_U_Float_Point *impulseCore,
                             const IVP_U_Float_Point *impulseWorld,
                             IVP_U_Float_Point *speedChangeWorld,
                             IVP_U_Float_Point *rotationChangeCore) {
  speedChangeWorld->set_multiple(impulseWorld, core->get_inv_mass());
  IVP_U_Float_Point angularImpulse;
  angularImpulse.inline_calc_cross_product(positionCore, impulseCore);
  rotationChangeCore->set_pairwise_mult(&angularImpulse,
                                        core->get_inv_rot_inertia());
}

void __fastcall TestCoreRotationPush(IVP_Core *core, void *,
                                     const IVP_U_Float_Point *angularImpulse,
                                     IVP_DOUBLE factor,
                                     IVP_U_Float_Point *rotationChangeCore) {
  rotationChangeCore->set_pairwise_mult(angularImpulse,
                                        core->get_inv_rot_inertia());
  rotationChangeCore->mult(factor);
}

void __fastcall AnnounceController(IVP_Controller_Manager *, void *,
                                   IVP_Controller_Dependent *controller) {
  ++g_announceCount;
  g_announcedController = controller;
}

void __fastcall EnsureController(IVP_Controller_Manager *, void *,
                                 IVP_Controller_Dependent *controller) {
  ++g_ensureSimulationCount;
  g_ensuredController = controller;
}

void RemoveController(IVP_Controller_Dependent *controller, IVP_BOOL silently) {
  EXPECT_EQ(silently, IVP_TRUE);
  ++g_removeCount;
  g_removedController = controller;
}

void __fastcall DestructController(IVP_Controller *controller, void *) {
  ++g_controllerDestructCount;
  g_destructedController = controller;
}

// The first raycast-car core lives in the byte immediately following the
// vector base. Growing it must copy out of that inline cell, not realloc stack
// storage. Later growth follows the normal engine vector path.
void __fastcall IncrementVector(IVP_U_Vector_Base *vector, void *) {
  const std::uint16_t capacity =
      vector->memsize == 0 ? 4
                           : static_cast<std::uint16_t>(vector->memsize * 2);
  void **inlineElements = reinterpret_cast<void **>(
      reinterpret_cast<std::byte *>(vector) + sizeof(*vector));
  void **elements = nullptr;
  if (vector->elems == inlineElements) {
    elements = static_cast<void **>(Allocate(capacity * sizeof(void *)));
    ASSERT_NE(elements, nullptr);
    std::memcpy(elements, vector->elems, vector->n_elems * sizeof(void *));
  } else {
    elements = static_cast<void **>(
        std::realloc(vector->elems, capacity * sizeof(void *)));
    ASSERT_NE(elements, nullptr);
  }
  vector->elems = elements;
  vector->memsize = capacity;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::Allocate, &Allocate),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::Free, &Free),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::OperatorNew, &OperatorNew),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::OperatorDelete,
                         &OperatorDelete),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::TemplateTwoPointConstruct,
                         &ConstructTemplateTwoPoint),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::TemplateSpringConstruct,
                         &ConstructTemplateSpring),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ActuatorConstruct,
                         &ConstructActuator),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ActuatorDestruct,
                         &DestructActuator),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::AnchorInitialize,
                         &InitializeAnchor),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::AnchorDestruct,
                         &DestructAnchor),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ActuatorEnsureSimulation,
                         &EnsureActuatorSimulation),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::RealObjectEnsureSimulation,
                         &EnsureRealObjectSimulation),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::TemplateConstraintConstruct,
                         &ConstructTemplateConstraint),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ConstraintConstruct,
                         &ConstructConstraint),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::EnvironmentCreateConstraint,
                         &CreateConstraint),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ActuatorSpringSetConstant,
                         &SetSpringConstant),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ActuatorSpringSetDamping,
                         &SetSpringDamping),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ActuatorSpringSetLength,
                         &SetSpringLength),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::VectorIncrementMemory,
                         &IncrementVector),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::FloatPointRealLength,
                         &FloatPointLength),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::RealObjectCalculateCoreFromObject,
        &CalculateCoreFromObject),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::MatrixTransformFloatPoint,
                         &TransformFloatPoint),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MatrixTransformFloatPointToPoint,
        &TransformFloatPointToPoint),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CacheTransformPositionToWorld,
        &TransformCachePositionToWorld),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::Matrix3TransformFloatPoint,
                         &TransformMatrix3FloatPoint),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::FloatPointOrthogonalPart,
                         &SetOrthogonalPart),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::FloatPointCrossProduct,
                         &CrossFloatPoints),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SolverCoreReactionInitTranslation,
        &InitializeReactionTranslation),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::GreatMatrixConstruct,
                         &ConstructGreatMatrix),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::GreatMatrixMultiplyAligned,
                         &MultiplyGreatMatrix),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::CoreGetSurfaceSpeed,
                         &GetSurfaceSpeed),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::CoreGetSurfaceSpeedOnTest,
                         &GetSurfaceSpeedOnTest),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::CoreTestPush, &TestCorePush),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::CoreTestRotationalPushCore,
                         &TestCoreRotationPush),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ControllerManagerAnnounce,
                         &AnnounceController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerEnsureSimulation,
        &EnsureController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerRemoveFromEnvironment,
        &RemoveController),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ControllerDestruct,
                         &DestructController),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
  return BML::IVP::Test::Resolve(rva, kRetailCalls);
}

class TestRaycastCar final : public IVP_Controller_Raycast_Car {
public:
  TestRaycastCar(IVP_Environment *environment,
                 const IVP_Template_Car_System *definition)
      : IVP_Controller_Raycast_Car(environment, definition) {}

  IVP_Raycast_Car_Wheel &wheel(int index) {
    return *get_wheel(static_cast<IVP_POS_WHEEL>(index));
  }
  IVP_Raycast_Car_Axis &axis(int index) {
    return *get_axis(static_cast<IVP_POS_AXIS>(index));
  }
  IVP_U_Vector<IVP_Core> *controlled_cores() {
    return get_associated_controlled_cores();
  }
  IVP_Controller_Dependent *dependent_controller() {
    return static_cast<IVP_Controller_Dependent *>(this);
  }
  IVP_CONTROLLER_PRIORITY priority() { return get_controller_priority(); }
  IVP_FLOAT body_downforce() const { return down_force; }
  IVP_FLOAT booster_acceleration() const { return booster_force; }
  const IVP_U_Float_Point &normalized_gravity() const {
    return normized_gravity_ws;
  }
  void simulate(IVP_DOUBLE deltaTime) {
    IVP_Event_Sim event(car_body->get_environment(), deltaTime);
    do_simulation_controller(&event, controlled_cores());
  }

  int raycastCount = 0;
  int raycastWheelCount = 0;
  std::array<IVP_Ray_Solver_Template, 4> lastRays{};

protected:
  void do_raycasts(IVP_Event_Sim *, int wheelCount,
                   IVP_Ray_Solver_Template *rays, IVP_Ray_Hit *,
                   IVP_FLOAT *frictions) override {
    ++raycastCount;
    raycastWheelCount = wheelCount;
    for (int wheel = 0; wheel < wheelCount; ++wheel) {
      lastRays[wheel] = rays[wheel];
      frictions[wheel] = 0.8f;
    }
  }
};

class RaycastCarFixture {
public:
  RaycastCarFixture()
      : environment(
            reinterpret_cast<IVP_Environment *>(environmentStorage.data())),
        core(reinterpret_cast<IVP_Core *>(coreStorage.data())),
        body(reinterpret_cast<IVP_Real_Object *>(bodyStorage.data())),
        manager(environment) {
    environment->controller_manager = &manager;
    environment->gravity.set(0.0, -9.81, 0.0);

    core->environment = environment;
    core->rot_inertia.hesse_val = 1000.0f;
    core->inv_rot_inertia.hesse_val = 0.001f;
    core->speed.set_to_zero();
    core->rot_speed.set_to_zero();
    core->m_world_f_core_last_psi.set_identity();

    body->environment = environment;
    body->physical_core = core;
    body->original_core = core;

    definition.car_body = body;
    definition.extra_gravity_force_value = 2.5f;
    definition.body_down_force_vertical_offset = 0.3f;
    const IVP_FLOAT x[4] = {0.8f, -0.8f, 0.8f, -0.8f};
    const IVP_FLOAT z[4] = {1.3f, 1.3f, -1.3f, -1.3f};
    for (int wheel = 0; wheel < 4; ++wheel) {
      definition.wheel_pos_Bos[wheel].set(x[wheel], 0.2f, z[wheel]);
      definition.friction_of_wheel[wheel] = 0.65f;
      definition.wheel_radius[wheel] = 0.25f;
      definition.spring_constant[wheel] = 18000.0f + wheel;
      definition.spring_dampening[wheel] = 1200.0f + wheel;
      definition.spring_dampening_compression[wheel] = 1600.0f + wheel;
      definition.spring_pre_tension[wheel] = -0.4f - wheel * 0.01f;
    }
    definition.stabilizer_constant[0] = 5000.0f;
    definition.stabilizer_constant[1] = 4500.0f;
    definition.wheel_max_rotation_speed[0] = 90.0f;
    definition.wheel_max_rotation_speed[1] = 80.0f;
  }

  alignas(IVP_Environment)
      std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
  alignas(IVP_Real_Object)
      std::array<std::byte, sizeof(IVP_Real_Object)> bodyStorage{};
  IVP_Environment *environment;
  IVP_Core *core;
  IVP_Real_Object *body;
  IVP_Controller_Manager manager;
  IVP_Template_Car_System definition{4, 2};
};

class ConstraintCarFixture {
public:
  ConstraintCarFixture()
      : environment(
            reinterpret_cast<IVP_Environment *>(environmentStorage.data())),
        body(reinterpret_cast<IVP_Real_Object *>(bodyStorage.data())),
        bodyCore(reinterpret_cast<IVP_Core *>(bodyCoreStorage.data())),
        staticObject(
            reinterpret_cast<IVP_Real_Object *>(staticObjectStorage.data())),
        staticCore(reinterpret_cast<IVP_Core *>(staticCoreStorage.data())),
        manager(environment) {
    environment->controller_manager = &manager;
    environment->gravity.set(0.0, -9.81, 0.0);
    InitializeObject(body, bodyCore, 1200.0f, 900.0f, 0.0, 0.0);
    InitializeCache(body, bodyCore, bodyCacheStorage.data());
    InitializeObject(staticObject, staticCore, 1.0f, 1.0f, 0.0, 0.0);
    InitializeCache(staticObject, staticCore, staticCacheStorage.data());
    staticCore->flags = 0x0C;
    staticObject->set_movement_state(IVP_MT_STATIC);
    environment->static_object = staticObject;

    constexpr IVP_FLOAT x[4] = {0.8f, -0.8f, 0.8f, -0.8f};
    constexpr IVP_FLOAT z[4] = {1.3f, 1.3f, -1.3f, -1.3f};
    for (int index = 0; index < 4; ++index) {
      wheelObjects[index] =
          reinterpret_cast<IVP_Real_Object *>(wheelObjectStorage[index].data());
      wheelCores[index] =
          reinterpret_cast<IVP_Core *>(wheelCoreStorage[index].data());
      InitializeObject(wheelObjects[index], wheelCores[index], 24.0f, 1.5f,
                       x[index], z[index]);
      InitializeCache(wheelObjects[index], wheelCores[index],
                      wheelCacheStorage[index].data());
      targetPositions[index].set(x[index], 0.0f, z[index]);
      wheels.add(wheelObjects[index]);
      positions.add(&targetPositions[index]);
    }

    definition.car_body = body;
    definition.body_counter_torque_factor = 0.35f;
    definition.extra_gravity_force_value = 140.0f;
    definition.extra_gravity_height_offset = 0.2f;
    definition.body_down_force_vertical_offset = 0.3f;
    definition.fast_turn_factor = 0.75f;
    for (int index = 0; index < 4; ++index) {
      definition.car_wheel[index] = wheelObjects[index];
      definition.wheel_pos_Bos[index].set(-targetPositions[index].k[0],
                                          targetPositions[index].k[1],
                                          targetPositions[index].k[2]);
      definition.wheel_radius[index] = 0.25f;
      definition.spring_constant[index] = 18000.0f + index;
      definition.spring_dampening[index] = 1200.0f + index;
      definition.spring_dampening_compression[index] = 1600.0f + index;
      definition.max_body_force[index] = 7000.0f + index;
      definition.spring_pre_tension[index] = 0.4f + index * 0.01f;
    }
    definition.stabilizer_constant[0] = 5000.0f;
    definition.stabilizer_constant[1] = 4500.0f;
    definition.wheel_max_rotation_speed[0] = 90.0f;
    definition.wheel_max_rotation_speed[1] = 80.0f;
  }

  void InitializeObject(IVP_Real_Object *object, IVP_Core *core, IVP_FLOAT mass,
                        IVP_FLOAT inertia, IVP_DOUBLE x, IVP_DOUBLE z) {
    object->environment = environment;
    object->physical_core = core;
    object->original_core = core;
    object->friction_core = core;
    object->flags = 0;
    object->set_movement_state(IVP_MT_MOVING);
    object->flags.shift_core_f_object_is_zero = 1;
    object->shift_core_f_object.set_to_zero();

    core->environment = environment;
    core->car_wheel = nullptr;
    core->rot_inertia.set(inertia, inertia, inertia);
    core->rot_inertia.hesse_val = mass;
    core->inv_rot_inertia.set(1.0f / inertia, 1.0f / inertia, 1.0f / inertia);
    core->inv_rot_inertia.hesse_val = 1.0f / mass;
    core->speed.set_to_zero();
    core->rot_speed.set_to_zero();
    core->pos_world_f_core_last_psi.set(x, 0.0, z);
    core->q_world_f_core_last_psi.init();
    core->q_world_f_core_next_psi.init();
    core->m_world_f_core_last_psi.set_identity();
    core->m_world_f_core_last_psi.vv.set(x, 0.0, z);
  }

  void InitializeCache(IVP_Real_Object *object, IVP_Core *core,
                       std::byte *storage) {
    auto *cache = reinterpret_cast<IVP_Cache_Object *>(storage);
    RawCache(cache).validUntilTimeCode = environment->get_current_time_code();
    RawCache(cache).referenceCount = 0;
    RawCache(cache).object = object;
    RawCache(cache).reserved = 0;
    cache->q_world_f_object = core->q_world_f_core_last_psi;
    cache->m_world_f_object = core->m_world_f_core_last_psi;
    cache->core_pos.set(&core->pos_world_f_core_last_psi);
    object->cache_object = cache;
  }

  alignas(IVP_Environment)
      std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
  alignas(IVP_Real_Object)
      std::array<std::byte, sizeof(IVP_Real_Object)> bodyStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> bodyCoreStorage{};
  alignas(IVP_Cache_Object)
      std::array<std::byte, sizeof(IVP_Cache_Object)> bodyCacheStorage{};
  alignas(IVP_Real_Object)
      std::array<std::byte, sizeof(IVP_Real_Object)> staticObjectStorage{};
  alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> staticCoreStorage{};
  alignas(IVP_Cache_Object)
      std::array<std::byte, sizeof(IVP_Cache_Object)> staticCacheStorage{};
  alignas(IVP_Real_Object) std::array<
      std::array<std::byte, sizeof(IVP_Real_Object)>, 4> wheelObjectStorage{};
  alignas(IVP_Core)
      std::array<std::array<std::byte, sizeof(IVP_Core)>, 4> wheelCoreStorage{};
  alignas(IVP_Cache_Object) std::array<
      std::array<std::byte, sizeof(IVP_Cache_Object)>, 4> wheelCacheStorage{};
  IVP_Environment *environment;
  IVP_Real_Object *body;
  IVP_Core *bodyCore;
  IVP_Real_Object *staticObject;
  IVP_Core *staticCore;
  std::array<IVP_Real_Object *, 4> wheelObjects{};
  std::array<IVP_Core *, 4> wheelCores{};
  std::array<IVP_U_Float_Point, 4> targetPositions{};
  IVP_Controller_Manager manager;
  IVP_U_Vector<IVP_Real_Object> wheels;
  IVP_U_Vector<IVP_U_Float_Point> positions;
  IVP_Template_Car_System definition{4, 2};
};

class TestRealWheelsCar final : public IVP_Car_System_Real_Wheels {
public:
  TestRealWheelsCar(IVP_Environment *environment,
                    IVP_Template_Car_System *definition)
      : IVP_Car_System_Real_Wheels(environment, definition) {}

  ~TestRealWheelsCar() override {
    set_booster_acceleration(0.0f);
    delete car_act_down_force;
    delete car_act_extra_gravity;
    for (int index = 0; index < 2; ++index)
      delete car_stabilizer[index];
    for (int index = 0; index < 4; ++index) {
      delete car_spring[index];
      delete car_act_torque[index];
    }
    delete car_act_torque_body;
  }

  IVP_Actuator_Suspension *spring(int index) { return car_spring[index]; }
  IVP_Actuator_Torque *wheel_torque(int index) { return car_act_torque[index]; }
  IVP_Actuator_Torque *body_torque() { return car_act_torque_body; }
  IVP_Constraint_Car_Object *wheel_state(int index) {
    return car_constraint_solver->wheel_objects.element_at(index);
  }
  IVP_Actuator_Force *down_force() { return car_act_down_force; }
  IVP_Constraint_Solver_Car *solver() { return car_constraint_solver; }
  IVP_FLOAT current_fast_turn_factor() const { return fast_turn_factor; }
  IVP_Constraint *fixed_constraint(int index) {
    return fix_wheel_constraint[index];
  }
};

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(std::uint32_t rva) noexcept {
  return ResolveRetailCall(rva);
}

namespace {

TEST(IvpCarGeometry, InitializesAUsableFourWheelConfiguration) {
  IVP_Template_Car_System car(4, 2);

  EXPECT_EQ(car.n_wheels, 4);
  EXPECT_EQ(car.n_axis, 2);
  EXPECT_EQ(car.index_x, IVP_INDEX_X);
  EXPECT_EQ(car.index_y, IVP_INDEX_Y);
  EXPECT_EQ(car.index_z, IVP_INDEX_Z);
  EXPECT_EQ(car.is_left_handed, IVP_FALSE);
  EXPECT_FLOAT_EQ(car.fast_turn_factor, 1.0f);

  for (int wheel = 0; wheel < 4; ++wheel) {
    EXPECT_FLOAT_EQ(car.wheel_reversed_sign[wheel], 1.0f);
    EXPECT_EQ(car.car_wheel[wheel], nullptr);
    EXPECT_FLOAT_EQ(car.friction_of_wheel[wheel], 0.0f);
    EXPECT_FLOAT_EQ(car.spring_constant[wheel], 0.0f);
  }
  for (int wheel = 4; wheel < IVP_CAR_SYSTEM_MAX_WHEELS; ++wheel)
    EXPECT_FLOAT_EQ(car.wheel_reversed_sign[wheel], 0.0f);
}

TEST(IvpCarGeometry, ComputesOuterWheelAckermannAngle) {
  constexpr IVP_FLOAT innerAngle = 0.45f;
  constexpr IVP_FLOAT trackWidth = 1.6f;
  constexpr IVP_FLOAT wheelbase = 2.6f;

  const IVP_FLOAT outerAngle =
      IVP_Car_System::calc_ackerman_angle(innerAngle, trackWidth, wheelbase);
  const IVP_FLOAT mirroredAngle =
      IVP_Car_System::calc_ackerman_angle(-innerAngle, trackWidth, wheelbase);

  EXPECT_GT(outerAngle, 0.0f);
  EXPECT_LT(outerAngle, innerAngle);
  EXPECT_FLOAT_EQ(mirroredAngle, -outerAngle);
  EXPECT_FLOAT_EQ(
      IVP_Car_System::calc_ackerman_angle(0.0005f, trackWidth, wheelbase),
      0.0005f);
}

TEST(IvpCarGeometry, GrowsTheInlineControlledCoreListWithoutLosingOrder) {
  std::byte firstStorage{};
  std::byte secondStorage{};
  auto *first = reinterpret_cast<IVP_Core *>(&firstStorage);
  auto *second = reinterpret_cast<IVP_Core *>(&secondStorage);

  g_vectorAllocations = 0;
  g_vectorFrees = 0;
  {
    IVP_Controller_Raycast_Car_Vector_of_Cores_1 cores;
    void **inlineElements = reinterpret_cast<void **>(
        reinterpret_cast<std::byte *>(&cores) + sizeof(IVP_U_Vector_Base));
    EXPECT_EQ(cores.elems, inlineElements);
    EXPECT_EQ(cores.memsize, 1);

    cores.add(first);
    EXPECT_EQ(cores.elems, inlineElements);
    cores.add(second);

    EXPECT_NE(cores.elems, inlineElements);
    EXPECT_EQ(cores.len(), 2);
    EXPECT_EQ(cores.element_at(0), first);
    EXPECT_EQ(cores.element_at(1), second);
    EXPECT_EQ(g_vectorAllocations, 1);
  }
  EXPECT_EQ(g_vectorFrees, 1);
}

TEST(IvpCarGeometry, InitializesBallanceRaycastVehicleAndControllerLifecycle) {
  ResetControllerObservations();
  RaycastCarFixture fixture;
  IVP_Controller_Dependent *dependent = nullptr;

  {
    TestRaycastCar car(fixture.environment, &fixture.definition);
    dependent = car.dependent_controller();

    EXPECT_EQ(reinterpret_cast<std::byte *>(dependent) -
                  reinterpret_cast<std::byte *>(&car),
              4);
    EXPECT_EQ(g_announceCount, 1);
    EXPECT_EQ(g_announcedController, dependent);
    EXPECT_EQ(g_ensureSimulationCount, 1);
    EXPECT_EQ(g_ensuredController, dependent);
    ASSERT_EQ(car.controlled_cores()->len(), 1);
    EXPECT_EQ(car.controlled_cores()->element_at(0), fixture.core);
    EXPECT_EQ(car.priority(), IVP_CP_CONSTRAINTS_MAX);
    EXPECT_FLOAT_EQ(car.normalized_gravity().k[0], 0.0f);
    EXPECT_FLOAT_EQ(car.normalized_gravity().k[1], -1.0f);
    EXPECT_FLOAT_EQ(car.normalized_gravity().k[2], 0.0f);

    for (int wheel = 0; wheel < 4; ++wheel) {
      const auto &state = car.wheel(wheel);
      EXPECT_FLOAT_EQ(state.hp_cs.k[0],
                      fixture.definition.wheel_pos_Bos[wheel].k[0]);
      EXPECT_FLOAT_EQ(state.hp_cs.k[1], 0.2f);
      EXPECT_FLOAT_EQ(state.hp_cs.k[2],
                      fixture.definition.wheel_pos_Bos[wheel].k[2]);
      EXPECT_FLOAT_EQ(state.spring_len,
                      -fixture.definition.spring_pre_tension[wheel]);
      EXPECT_FLOAT_EQ(state.spring_direction_cs.k[1], -1.0f);
      EXPECT_FLOAT_EQ(state.friction_of_wheel, 1.0f);
      EXPECT_FLOAT_EQ(state.wheel_radius, 0.25f);
      EXPECT_FLOAT_EQ(state.inv_wheel_radius, 4.0f);
      EXPECT_EQ(state.wheel_is_fixed, IVP_FALSE);
      EXPECT_FLOAT_EQ(state.max_rotation_speed,
                      fixture.definition.wheel_max_rotation_speed[wheel >> 1]);
    }
    EXPECT_FLOAT_EQ(car.axis(IVP_FRONT).stabilizer_constant, 5000.0f);
    EXPECT_FLOAT_EQ(car.axis(IVP_REAR).stabilizer_constant, 4500.0f);
    EXPECT_NEAR(car.get_orig_front_wheel_distance(), 1.6, 1.0e-6);
    EXPECT_NEAR(car.get_orig_axles_distance(), 2.6, 1.0e-6);
  }

  EXPECT_EQ(g_removeCount, 1);
  EXPECT_EQ(g_removedController, dependent);
  EXPECT_EQ(g_controllerDestructCount, 1);
  EXPECT_EQ(g_destructedController, dependent);
}

TEST(IvpCarGeometry, AppliesRaycastVehicleSteeringDriveAndDebugControls) {
  ResetControllerObservations();
  RaycastCarFixture fixture;
  fixture.core->speed.set(0.0f, 0.0f, 12.5f);

  TestRaycastCar car(fixture.environment, &fixture.definition);
  EXPECT_DOUBLE_EQ(car.get_body_speed(IVP_INDEX_Z), 12.5);

  const int wakesAfterConstruction = g_ensureSimulationCount;
  car.do_steering(0.35f, true);
  EXPECT_EQ(g_ensureSimulationCount, wakesAfterConstruction + 1);
  EXPECT_NEAR(car.wheel(IVP_FRONT_LEFT).axis_direction_cs.k[0], std::cos(0.35f),
              1.0e-6f);
  EXPECT_NEAR(car.wheel(IVP_FRONT_LEFT).axis_direction_cs.k[2],
              -std::sin(0.35f), 1.0e-6f);
  EXPECT_FLOAT_EQ(car.wheel(IVP_REAR_LEFT).axis_direction_cs.k[0], 1.0f);

  car.change_wheel_torque(IVP_REAR_LEFT, 320.0f);
  EXPECT_FLOAT_EQ(car.wheel(IVP_REAR_LEFT).torque, 320.0f);
  EXPECT_EQ(g_ensureSimulationCount, wakesAfterConstruction + 2);
  car.fix_wheel(IVP_REAR_RIGHT, IVP_TRUE);
  EXPECT_EQ(car.wheel(IVP_REAR_RIGHT).wheel_is_fixed, IVP_TRUE);
  car.change_spring_constant(IVP_FRONT_LEFT, 21000.0f);
  car.change_spring_dampening(IVP_FRONT_LEFT, 1400.0f);
  car.change_spring_dampening_compression(IVP_FRONT_LEFT, 1700.0f);
  car.change_spring_length(IVP_FRONT_LEFT, 0.55f);
  EXPECT_FLOAT_EQ(car.wheel(IVP_FRONT_LEFT).spring_constant, 21000.0f);
  EXPECT_FLOAT_EQ(car.wheel(IVP_FRONT_LEFT).spring_damp_relax, 1400.0f);
  EXPECT_FLOAT_EQ(car.wheel(IVP_FRONT_LEFT).spring_damp_compress, 1700.0f);
  EXPECT_FLOAT_EQ(car.wheel(IVP_FRONT_LEFT).spring_len, 0.55f);
  car.change_spring_pre_tension(IVP_FRONT_LEFT, 0.45f);
  EXPECT_FLOAT_EQ(car.wheel(IVP_FRONT_LEFT).spring_len, 0.45f);
  car.change_stabilizer_constant(IVP_FRONT, 5250.0f);
  EXPECT_FLOAT_EQ(car.axis(IVP_FRONT).stabilizer_constant, 5250.0f);
  EXPECT_DOUBLE_EQ(car.get_wheel_angular_velocity(IVP_FRONT_LEFT), 0.0);
  car.change_body_downforce(150.0f);
  EXPECT_FLOAT_EQ(car.body_downforce(), 150.0f);

  car.set_booster_acceleration(7.0f);
  EXPECT_FLOAT_EQ(car.booster_acceleration(), 7.0f);
  car.set_booster_acceleration(0.0f);

  car.activate_booster(18.0f, 1.5f, 2.0f);
  EXPECT_FLOAT_EQ(car.get_booster_time_to_go(), 1.5f);
  EXPECT_FLOAT_EQ(car.get_booster_delay(), 3.5f);
  car.activate_booster(99.0f, 9.0f, 9.0f);
  EXPECT_FLOAT_EQ(car.get_booster_time_to_go(), 1.5f);
  EXPECT_FLOAT_EQ(car.get_booster_delay(), 3.5f);

  IVP_CarSystemDebugData_t input{};
  IVP_CarSystemDebugData_t output{};
  input.wheelRaycasts[IVP_FRONT_LEFT][0].set(1.0, 2.0, 3.0);
  input.wheelRaycasts[IVP_FRONT_LEFT][1].set(4.0, 5.0, 6.0);
  input.wheelRaycastImpacts[IVP_FRONT_LEFT] = 0.75f;
  output.wheelRotationalTorque[0][0] = 123.0f;
  car.SetCarSystemDebugData(input);
  car.GetCarSystemDebugData(output);
  EXPECT_DOUBLE_EQ(output.wheelRaycasts[IVP_FRONT_LEFT][0].k[1], 2.0);
  EXPECT_DOUBLE_EQ(output.wheelRaycasts[IVP_FRONT_LEFT][1].k[2], 6.0);
  EXPECT_FLOAT_EQ(output.wheelRaycastImpacts[IVP_FRONT_LEFT], 0.75f);
  EXPECT_FLOAT_EQ(output.wheelRotationalTorque[0][0], 123.0f);

  std::array<IVP_Wheel_Skid_Info, 4> skid{};
  car.get_skid_info(skid.data());
  for (const auto &wheel : skid) {
    EXPECT_FLOAT_EQ(wheel.last_skid_value, 0.0f);
    EXPECT_DOUBLE_EQ(wheel.last_skid_time.get_time(), 0.0);
  }
}

TEST(IvpCarGeometry, SimulatesAnAirborneRaycastVehiclePsi) {
  ResetControllerObservations();
  RaycastCarFixture fixture;
  TestRaycastCar car(fixture.environment, &fixture.definition);

  car.activate_booster(18.0f, 1.5f, 2.0f);
  car.simulate(0.02);

  EXPECT_EQ(car.raycastCount, 1);
  EXPECT_EQ(car.raycastWheelCount, 4);
  for (int wheel = 0; wheel < 4; ++wheel) {
    const auto &ray = car.lastRays[wheel];
    EXPECT_NEAR(ray.ray_start_point.k[0],
                fixture.definition.wheel_pos_Bos[wheel].k[0], 1.0e-6);
    EXPECT_NEAR(ray.ray_start_point.k[1], 0.2, 1.0e-6);
    EXPECT_NEAR(ray.ray_start_point.k[2],
                fixture.definition.wheel_pos_Bos[wheel].k[2], 1.0e-6);
    EXPECT_FLOAT_EQ(ray.ray_normized_direction.k[0], 0.0f);
    EXPECT_FLOAT_EQ(ray.ray_normized_direction.k[1], -1.0f);
    EXPECT_FLOAT_EQ(ray.ray_normized_direction.k[2], 0.0f);
    EXPECT_NEAR(ray.ray_length,
                -fixture.definition.spring_pre_tension[wheel] + 0.25f, 1.0e-6f);
    EXPECT_EQ(ray.ray_flags, IVP_RAY_SOLVER_ALL);
    EXPECT_FLOAT_EQ(car.wheel(wheel).pressure, 0.0f);
    EXPECT_NEAR(car.wheel(wheel).raycast_dist, ray.ray_length, 1.0e-6f);
  }

  EXPECT_NEAR(fixture.core->speed.k[0], 0.0f, 1.0e-6f);
  EXPECT_NEAR(fixture.core->speed.k[1], -0.00005f, 1.0e-7f);
  EXPECT_NEAR(fixture.core->speed.k[2], 0.36f, 1.0e-6f);
  EXPECT_NEAR(car.get_booster_time_to_go(), 1.48f, 1.0e-6f);
  EXPECT_NEAR(car.get_booster_delay(), 3.48f, 1.0e-6f);
}

TEST(IvpCarGeometry, SolvesFourRealWheelBodyConstraintsForOnePsi) {
  ResetControllerObservations();
  ConstraintCarFixture fixture;
  const std::array<IVP_U_Float_Point, 4> initialSpeeds{
      IVP_U_Float_Point(2.0f, 0.0f, 4.0f),
      IVP_U_Float_Point(-1.0f, 0.0f, 3.0f),
      IVP_U_Float_Point(0.5f, 0.0f, -2.0f),
      IVP_U_Float_Point(-0.5f, 0.0f, 1.0f),
  };
  for (int index = 0; index < 4; ++index)
    fixture.wheelCores[index]->speed = initialSpeeds[index];

  IVP_U_Float_Point momentumBefore;
  momentumBefore.set_to_zero();
  for (int index = 0; index < 4; ++index) {
    momentumBefore.add_multiple(&fixture.wheelCores[index]->speed, 24.0);
  }

  {
    IVP_Constraint_Solver_Car solver(IVP_INDEX_X, IVP_INDEX_Y, IVP_INDEX_Z,
                                     IVP_FALSE);
    ASSERT_EQ(solver.init_constraint_system(fixture.environment, fixture.body,
                                            fixture.wheels, fixture.positions),
              IVP_OK);
    EXPECT_EQ(g_announceCount, 1);
    EXPECT_EQ(g_announcedController, &solver);
    EXPECT_EQ(solver.get_num_of_appending_terminals(), 4);
    ASSERT_EQ(solver.get_associated_controlled_cores()->len(), 5);
    EXPECT_EQ(solver.co_matrix.columns, 8);
    for (int index = 0; index < 4; ++index)
      EXPECT_NE(fixture.wheelCores[index]->car_wheel, nullptr);

    IVP_Event_Sim event(fixture.environment, 0.01);
    solver.do_simulation_controller(&event,
                                    solver.get_associated_controlled_cores());

    for (int index = 0; index < 4; ++index) {
      IVP_U_Float_Point bodySurface;
      fixture.bodyCore->get_surface_speed(&fixture.targetPositions[index],
                                          &bodySurface);
      EXPECT_NEAR(fixture.wheelCores[index]->speed.k[0], bodySurface.k[0],
                  2.0e-4f)
          << "wheel=" << index;
      EXPECT_NEAR(fixture.wheelCores[index]->speed.k[2], bodySurface.k[2],
                  2.0e-4f)
          << "wheel=" << index;
    }

    IVP_U_Float_Point momentumAfter;
    momentumAfter.set_multiple(&fixture.bodyCore->speed, 1200.0);
    for (int index = 0; index < 4; ++index) {
      momentumAfter.add_multiple(&fixture.wheelCores[index]->speed, 24.0);
    }
    EXPECT_NEAR(momentumAfter.k[0], momentumBefore.k[0], 2.0e-3f);
    EXPECT_NEAR(momentumAfter.k[2], momentumBefore.k[2], 2.0e-3f);
  }

  EXPECT_EQ(g_removeCount, 1);
  for (int index = 0; index < 4; ++index)
    EXPECT_EQ(fixture.wheelCores[index]->car_wheel, nullptr);
}

TEST(IvpCarGeometry, RunsAConstraintBasedFourWheelVehicleControlCycle) {
  ResetControllerObservations();
  ConstraintCarFixture fixture;

  {
    TestRealWheelsCar car(fixture.environment, &fixture.definition);
    fixture.bodyCore->speed.set(0.0f, 0.0f, 12.0f);
    EXPECT_EQ(car.solver()->get_num_of_appending_terminals(), 4);
    EXPECT_NEAR(car.get_orig_front_wheel_distance(), 1.6, 1.0e-6);
    EXPECT_NEAR(car.get_orig_axles_distance(), 2.6, 1.0e-6);
    EXPECT_NEAR(car.get_body_speed(), 12.0, 1.0e-6);

    car.change_spring_constant(IVP_FRONT_LEFT, 21000.0f);
    car.change_spring_dampening(IVP_FRONT_LEFT, 1350.0f);
    const int ensureBeforeSuspensionLimits = g_actuatorEnsureCount;
    car.change_spring_dampening_compression(IVP_FRONT_LEFT, 1725.0f);
    car.change_max_body_force(IVP_FRONT_LEFT, 7600.0f);
    EXPECT_EQ(g_actuatorEnsureCount, ensureBeforeSuspensionLimits + 2);
    car.change_spring_pre_tension(IVP_FRONT_LEFT, 0.6f);
    EXPECT_FLOAT_EQ(car.spring(0)->get_spring_length_zero_force(), 499.4f);
    car.change_spring_length(IVP_FRONT_LEFT, 499.25f);
    EXPECT_FLOAT_EQ(car.spring(0)->get_constant(), 21000.0f);
    EXPECT_FLOAT_EQ(car.spring(0)->get_damp_factor(), 1350.0f);
    EXPECT_FLOAT_EQ(car.spring(0)->get_spring_length_zero_force(), 499.25f);

    car.change_stabilizer_constant(IVP_FRONT, 5400.0f);
    EXPECT_EQ(g_realObjectEnsureCount, 1);
    car.change_fast_turn_factor(0.6f);
    EXPECT_FLOAT_EQ(car.current_fast_turn_factor(), 0.6f);
    car.change_wheel_speed_dampening(IVP_REAR_LEFT, 0.93f);
    EXPECT_FLOAT_EQ(fixture.wheelCores[2]->speed_damp_factor, 0.93f);

    car.change_wheel_torque(IVP_FRONT_LEFT, 200.0f);
    car.change_wheel_torque(IVP_FRONT_RIGHT, 180.0f);
    car.change_wheel_torque(IVP_REAR_LEFT, 220.0f);
    car.change_wheel_torque(IVP_REAR_RIGHT, 210.0f);
    car.update_body_countertorque();
    EXPECT_FLOAT_EQ(car.body_torque()->get_torque(), -283.5f);
    car.update_throttle(0.75f);
    EXPECT_FLOAT_EQ(car.wheel_torque(0)->get_torque(), 200.0f);
    car.wheel_torque(2)->rot_speed_out = -13.5f;
    EXPECT_DOUBLE_EQ(car.get_wheel_angular_velocity(IVP_REAR_LEFT), -13.5);

    fixture.bodyCore->rot_speed.set(2.0f, 4.0f, 6.0f);
    fixture.wheelCores[3]->rot_speed.set(1.0f, 2.0f, 3.0f);
    car.fix_wheel(IVP_REAR_RIGHT, IVP_TRUE);
    ASSERT_NE(car.fixed_constraint(3), nullptr);
    EXPECT_EQ(car.wheel_state(3)->fix_wheel_constraint,
              car.fixed_constraint(3));
    EXPECT_EQ(g_createConstraintCount, 1);
    EXPECT_EQ(g_constraintReference, fixture.wheelObjects[3]);
    EXPECT_EQ(g_constraintAttached, fixture.body);
    EXPECT_EQ(g_constraintAxes[IVP_TR_INDEX_TX], IVP_CONSTRAINT_AXIS_FREE);
    EXPECT_EQ(g_constraintAxes[IVP_TR_INDEX_TY], IVP_CONSTRAINT_AXIS_FREE);
    EXPECT_EQ(g_constraintAxes[IVP_TR_INDEX_TZ], IVP_CONSTRAINT_AXIS_FREE);
    EXPECT_EQ(g_constraintAxes[IVP_TR_INDEX_RX], IVP_CONSTRAINT_AXIS_FIXED);
    EXPECT_EQ(g_constraintAxes[IVP_TR_INDEX_RY], IVP_CONSTRAINT_AXIS_FREE);
    EXPECT_EQ(g_constraintAxes[IVP_TR_INDEX_RZ], IVP_CONSTRAINT_AXIS_FREE);
    EXPECT_FLOAT_EQ(fixture.wheelCores[3]->rot_speed.k[0], 1.5f);
    EXPECT_FLOAT_EQ(fixture.wheelCores[3]->rot_speed.k[1], 3.0f);
    EXPECT_FLOAT_EQ(fixture.wheelCores[3]->rot_speed.k[2], 4.5f);
    car.fix_wheel(IVP_REAR_RIGHT, IVP_TRUE);
    EXPECT_EQ(g_createConstraintCount, 1);
    const int removeBeforeUnlock = g_removeCount;
    car.fix_wheel(IVP_REAR_RIGHT, IVP_FALSE);
    EXPECT_EQ(car.fixed_constraint(3), nullptr);
    EXPECT_EQ(car.wheel_state(3)->fix_wheel_constraint, nullptr);
    EXPECT_EQ(g_removeCount, removeBeforeUnlock + 1);

    const IVP_FLOAT innerAngle = 0.42f;
    car.do_steering(innerAngle);
    const IVP_FLOAT outerAngle =
        IVP_Car_System::calc_ackerman_angle(innerAngle, 1.6f, 2.6f);
    const auto *left = car.wheel_state(0)->target_position_bs.get_position();
    const auto *right = car.wheel_state(1)->target_position_bs.get_position();
    EXPECT_NEAR(left->k[0], -0.8, 1.0e-6);
    EXPECT_NEAR(right->k[0], 0.8, 1.0e-6);
    EXPECT_LT(outerAngle, innerAngle);
    EXPECT_NEAR(fixture.bodyCore->rot_speed_change.k[IVP_INDEX_Y],
                -innerAngle * 12.0f / 2.6f * 0.6f, 1.0e-5f);
    const IVP_U_Point rearTargetBefore =
        *car.wheel_state(2)->target_position_bs.get_position();
    car.update_wheel_positions();
    EXPECT_DOUBLE_EQ(car.wheel_state(2)->target_position_bs.get_position()->k[0],
                     rearTargetBefore.k[0]);
    EXPECT_DOUBLE_EQ(car.wheel_state(2)->target_position_bs.get_position()->k[2],
                     rearTargetBefore.k[2]);

    IVP_CarSystemDebugData_t debugInput;
    IVP_CarSystemDebugData_t debugOutput;
    std::memset(&debugInput, 0x3C, sizeof(debugInput));
    std::memset(&debugOutput, 0xA7, sizeof(debugOutput));
    car.SetCarSystemDebugData(debugInput);
    car.GetCarSystemDebugData(debugOutput);
    const auto *debugBytes = reinterpret_cast<const unsigned char *>(&debugOutput);
    EXPECT_TRUE(std::all_of(debugBytes, debugBytes + sizeof(debugOutput),
                            [](unsigned char value) { return value == 0xA7; }));

    car.wheel_state(2)->last_contact_position_ws.set(0.7f, 0.1f, -1.2f);
    car.wheel_state(2)->last_skid_value = 3.25f;
    car.wheel_state(2)->last_skid_time = IVP_Time(17.5);
    IVP_Wheel_Skid_Info skid[4]{};
    car.get_skid_info(skid);
    EXPECT_FLOAT_EQ(skid[2].last_contact_position_ws.k[0], 0.7f);
    EXPECT_FLOAT_EQ(skid[2].last_skid_value, 3.25f);
    EXPECT_DOUBLE_EQ(skid[2].last_skid_time.get_time(), 17.5);

    car.change_body_downforce(320.0f);
    EXPECT_DOUBLE_EQ(car.down_force()->get_force(), 320.0);
    car.activate_booster(1.5f, 0.8f, 1.2f);
    EXPECT_FLOAT_EQ(car.get_booster_time_to_go(), 0.8f);
    EXPECT_FLOAT_EQ(car.get_booster_delay(), 2.0f);
    car.update_booster(0.3f);
    EXPECT_NEAR(car.get_booster_time_to_go(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(car.get_booster_delay(), 1.7f, 1.0e-6f);
    car.update_booster(0.6f);
    EXPECT_LE(car.get_booster_time_to_go(), 0.0f);

    EXPECT_GE(g_actuatorEnsureCount, 8);
    EXPECT_GE(g_announceCount, 14);
  }
  for (int index = 0; index < 4; ++index)
    EXPECT_EQ(fixture.wheelCores[index]->car_wheel, nullptr);
}

} // namespace
