#include "IvpTestAdapter.h"

#include "BML/IVP/Constraint.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace {

struct ConstraintCall {
  enum class Space { None, ReferenceObject, World } space = Space::None;
  IVP_Real_Object *reference = nullptr;
  const IVP_U_Point *anchor = nullptr;
  const IVP_U_Point *axis = nullptr;
  unsigned int fixedTranslationDimensions = 0;
  unsigned int fixedRotationDimensions = 0;
  IVP_Real_Object *attached = nullptr;
  bool hasAttachedDisplacement = false;
  IVP_U_Matrix attachedDisplacement{};
};

ConstraintCall g_call;
int g_completeConstraintBaseConstructs = 0;
int g_localInitializations = 0;
int g_localCompleteConstructs = 0;
int g_localAnchorConstructs = 0;
int g_activations = 0;
int g_controllerRemovals = 0;
bool g_sawConstructedSubobjects = false;
void *g_localConstraintVtable = nullptr;

void InitializeTemplate(IVP_Template_Constraint *value) {
  value->flags = IVP_CONSTRAINT_ACTIVATED;
  value->objectR = nullptr;
  value->objectA = nullptr;
  value->mm_Ros_f_Rfs.set_identity();
  value->m_Ros_f_Rfs = nullptr;
  value->mm_Ros_f_Rrs.set_identity();
  value->m_Ros_f_Rrs = nullptr;
  value->mm_Aos_f_Afs.set_identity();
  value->m_Aos_f_Afs = nullptr;
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

void __fastcall ConstructTemplate(IVP_Template_Constraint *value, void *) {
  InitializeTemplate(value);
}

void __fastcall InitializeMatrix3(IVP_U_Matrix3 *value, void *) {
  value->set_identity();
}

void *__cdecl AllocateObject(unsigned int size) { return std::malloc(size); }

void __fastcall CompleteConstraintConstruct(IVP_Constraint *value, void *) {
  ++g_completeConstraintBaseConstructs;
  auto *raw = reinterpret_cast<std::byte *>(value);
  *reinterpret_cast<std::uint16_t *>(raw + 0x08) = 2;
  *reinterpret_cast<std::uint16_t *>(raw + 0x0A) = 0;
  *reinterpret_cast<void ***>(raw + 0x0C) =
      reinterpret_cast<void **>(raw + 0x10);
  auto *flags = reinterpret_cast<std::uint32_t *>(raw + 0x04);
  *flags = (*flags & ~std::uint32_t{3}) | std::uint32_t{1};
}

void __fastcall CompleteLocalAnchorConstruct(
    IVP_Constraint_Local_Anchor *value, void *) {
  ++g_localAnchorConstructs;
  auto *raw = reinterpret_cast<std::byte *>(value);
  *reinterpret_cast<IVP_U_Matrix3 **>(raw + 0x84) = nullptr;
}

void __fastcall InitializeLocalConstraint(
    IVP_Constraint_Local *value, void *,
    const IVP_Template_Constraint *definition) {
  ++g_localInitializations;
  auto *raw = reinterpret_cast<std::byte *>(value);
  const auto capacity = *reinterpret_cast<std::uint16_t *>(raw + 0x08);
  const auto count = *reinterpret_cast<std::uint16_t *>(raw + 0x0A);
  const auto elements = *reinterpret_cast<void ***>(raw + 0x0C);
  const auto referenceRotation =
      *reinterpret_cast<IVP_U_Matrix3 **>(raw + 0xF4);
  const auto attachedRotation =
      *reinterpret_cast<IVP_U_Matrix3 **>(raw + 0x17C);
  g_sawConstructedSubobjects =
      capacity == 2 && count == 0 &&
      elements == reinterpret_cast<void **>(raw + 0x10) &&
      referenceRotation == nullptr && attachedRotation == nullptr &&
      static_cast<unsigned char>(raw[0x180]) == 0 &&
      static_cast<unsigned char>(raw[0x181]) == 1 &&
      static_cast<unsigned char>(raw[0x182]) == 2 &&
      static_cast<unsigned char>(raw[0x183]) == 0 &&
      static_cast<unsigned char>(raw[0x184]) == 1 &&
      static_cast<unsigned char>(raw[0x185]) == 2;
  *reinterpret_cast<IVP_Real_Object **>(raw + 0xF0) = definition->objectR;
  *reinterpret_cast<IVP_Real_Object **>(raw + 0x178) = definition->objectA;
  *reinterpret_cast<void **>(raw + 0x6C) = nullptr;
}

void __fastcall ActivateConstraint(IVP_Constraint *value, void *) {
  ++g_activations;
  auto *flags = reinterpret_cast<std::uint32_t *>(
      reinterpret_cast<std::byte *>(value) + 0x04);
  *flags = (*flags & ~std::uint32_t{3}) | std::uint32_t{1};
}

void RemoveConstraintController(IVP_Controller_Dependent *, IVP_BOOL silently) {
  EXPECT_EQ(silently, IVP_TRUE);
  ++g_controllerRemovals;
}

void __fastcall DestructController(IVP_Controller *, void *) {}

void __fastcall CompleteLocalConstraintConstruct(
    IVP_Constraint_Local *value, void *,
    const IVP_Template_Constraint *definition) {
  ++g_localCompleteConstructs;
  auto *raw = reinterpret_cast<std::byte *>(value);
  void *incomingVtable = *reinterpret_cast<void **>(raw);
  if (!g_localConstraintVtable)
    g_localConstraintVtable = incomingVtable;
  CompleteConstraintConstruct(value, nullptr);
  CompleteLocalAnchorConstruct(
      reinterpret_cast<IVP_Constraint_Local_Anchor *>(raw + 0x70), nullptr);
  CompleteLocalAnchorConstruct(
      reinterpret_cast<IVP_Constraint_Local_Anchor *>(raw + 0xF8), nullptr);
  raw[0x180] = std::byte{0};
  raw[0x181] = std::byte{1};
  raw[0x182] = std::byte{2};
  raw[0x183] = std::byte{0};
  raw[0x184] = std::byte{1};
  raw[0x185] = std::byte{2};
  *reinterpret_cast<void **>(raw) = g_localConstraintVtable;
  InitializeLocalConstraint(value, nullptr, definition);
  ActivateConstraint(value, nullptr);
}

void RecordConstraintCall(
    ConstraintCall::Space space, IVP_Real_Object *reference,
    const IVP_U_Point *anchor, const IVP_U_Point *axis,
    unsigned int fixedTranslationDimensions,
    unsigned int fixedRotationDimensions, IVP_Real_Object *attached,
    const IVP_U_Matrix *attachedDisplacement) {
  g_call = {};
  g_call.space = space;
  g_call.reference = reference;
  g_call.anchor = anchor;
  g_call.axis = axis;
  g_call.fixedTranslationDimensions = fixedTranslationDimensions;
  g_call.fixedRotationDimensions = fixedRotationDimensions;
  g_call.attached = attached;
  g_call.hasAttachedDisplacement = attachedDisplacement != nullptr;
  if (attachedDisplacement)
    g_call.attachedDisplacement = *attachedDisplacement;
}

void __fastcall SetConstraintRos(
    IVP_Template_Constraint *, void *, IVP_Real_Object *reference,
    const IVP_U_Point *anchor, const IVP_U_Point *axis,
    unsigned int fixedTranslationDimensions,
    unsigned int fixedRotationDimensions, IVP_Real_Object *attached,
    const IVP_U_Matrix *attachedDisplacement) {
  RecordConstraintCall(
      ConstraintCall::Space::ReferenceObject, reference, anchor, axis,
      fixedTranslationDimensions, fixedRotationDimensions, attached,
      attachedDisplacement);
}

void __fastcall SetConstraintWorld(
    IVP_Template_Constraint *, void *, IVP_Real_Object *reference,
    const IVP_U_Point *anchor, const IVP_U_Point *axis,
    unsigned int fixedTranslationDimensions,
    unsigned int fixedRotationDimensions, IVP_Real_Object *attached,
    const IVP_U_Matrix *attachedDisplacement) {
  RecordConstraintCall(
      ConstraintCall::Space::World, reference, anchor, axis,
      fixedTranslationDimensions, fixedRotationDimensions, attached,
      attachedDisplacement);
}

void __fastcall LimitTranslation(IVP_Template_Constraint *value, void *,
                                 IVP_COORDINATE_INDEX axis, IVP_FLOAT left,
                                 IVP_FLOAT right) {
  value->axis_type[axis] = IVP_CONSTRAINT_AXIS_LIMITED;
  value->borderleft_Rfs[axis] = left;
  value->borderright_Rfs[axis] = right;
}

void __fastcall LimitRotation(IVP_Template_Constraint *value, void *,
                              IVP_COORDINATE_INDEX axis, IVP_FLOAT left,
                              IVP_FLOAT right) {
  const int rotationAxis = axis + 3;
  value->axis_type[rotationAxis] = IVP_CONSTRAINT_AXIS_LIMITED;
  value->borderleft_Rfs[rotationAxis] = left;
  value->borderright_Rfs[rotationAxis] = right;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::OperatorNew,
                         &AllocateObject),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ConstraintConstruct,
                         &CompleteConstraintConstruct),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ConstraintActivate,
                         &ActivateConstraint),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ConstraintLocalConstruct,
                         &CompleteLocalConstraintConstruct),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ConstraintLocalAnchorConstruct,
        &CompleteLocalAnchorConstruct),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ConstraintLocalInitialize,
                         &InitializeLocalConstraint),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerRemoveFromEnvironment,
        &RemoveConstraintController),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::ControllerDestruct,
                         &DestructController),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::Matrix3Initialize,
                         &InitializeMatrix3),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::TemplateConstraintConstruct,
                         &ConstructTemplate),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::TemplateConstraintSetRos,
                         &SetConstraintRos),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::TemplateConstraintSetWorld,
                         &SetConstraintWorld),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplateConstraintLimitTranslation,
        &LimitTranslation),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplateConstraintLimitRotation,
        &LimitRotation),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
  return BML::IVP::Test::Resolve(rva, kRetailCalls);
}

void ExpectCall(ConstraintCall::Space space, IVP_Real_Object *reference,
                const IVP_U_Point *anchor, const IVP_U_Point *axis,
                unsigned int translationDimensions,
                unsigned int rotationDimensions, IVP_Real_Object *attached) {
  EXPECT_EQ(g_call.space, space);
  EXPECT_EQ(g_call.reference, reference);
  EXPECT_EQ(g_call.anchor, anchor);
  EXPECT_EQ(g_call.axis, axis);
  EXPECT_EQ(g_call.fixedTranslationDimensions, translationDimensions);
  EXPECT_EQ(g_call.fixedRotationDimensions, rotationDimensions);
  EXPECT_EQ(g_call.attached, attached);
}

TEST(IvpConstraintConfiguration,
     PreservesBallanceAxisFramesLimitsAndBreakThresholds) {
  IVP_Template_Constraint definition;
  EXPECT_EQ(definition.flags, IVP_CONSTRAINT_ACTIVATED);
  EXPECT_FLOAT_EQ(definition.limited_axis_stiffness, 0.3f);

  alignas(void *) std::array<std::byte, sizeof(void *)> referenceStorage{};
  alignas(void *) std::array<std::byte, sizeof(void *)> attachedStorage{};
  auto *reference = reinterpret_cast<IVP_Real_Object *>(referenceStorage.data());
  auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedStorage.data());
  definition.set_reference_object(reference);
  definition.set_attached_object(attached);
  EXPECT_EQ(definition.objectR, reference);
  EXPECT_EQ(definition.objectA, attached);

  IVP_U_Point referenceAnchor(1.0, -2.0, 3.5);
  IVP_U_Point attachedAnchor(-4.0, 5.0, 0.25);
  definition.set_fixing_point_Ros(&referenceAnchor);
  definition.set_attached_fixing_point_Aos(&attachedAnchor);
  ASSERT_EQ(definition.m_Ros_f_Rfs, &definition.mm_Ros_f_Rfs);
  ASSERT_EQ(definition.m_Aos_f_Afs, &definition.mm_Aos_f_Afs);
  EXPECT_DOUBLE_EQ(definition.m_Ros_f_Rfs->vv.k[0], 1.0);
  EXPECT_DOUBLE_EQ(definition.m_Ros_f_Rfs->vv.k[1], -2.0);
  EXPECT_DOUBLE_EQ(definition.m_Aos_f_Afs->vv.k[2], 0.25);

  IVP_U_Matrix3 translationAxes;
  translationAxes.init_rotated3(IVP_INDEX_Z, 0.4f);
  IVP_U_Matrix3 rotationAxes;
  rotationAxes.init_rotated3(IVP_INDEX_X, -0.25f);
  definition.set_translation_axes_Ros(&translationAxes);
  definition.set_rotation_axes_Ros(&rotationAxes);
  definition.set_attached_translation_axes_Aos(&translationAxes);
  EXPECT_DOUBLE_EQ(definition.mm_Ros_f_Rfs.get_elem(0, 0),
                   translationAxes.get_elem(0, 0));
  EXPECT_DOUBLE_EQ(definition.mm_Ros_f_Rrs.get_elem(1, 2),
                   rotationAxes.get_elem(1, 2));
  EXPECT_DOUBLE_EQ(definition.mm_Aos_f_Afs.get_elem(1, 0),
                   translationAxes.get_elem(1, 0));

  definition.set_translation_axes_as_object_space();
  definition.set_rotation_axes_as_translation_axes();
  definition.set_constraint_is_relaxed();
  EXPECT_EQ(definition.m_Ros_f_Rfs, nullptr);
  EXPECT_EQ(definition.m_Ros_f_Rrs, nullptr);
  EXPECT_EQ(definition.m_Aos_f_Afs, nullptr);

  definition.free_translation_axis(IVP_INDEX_X);
  definition.fix_translation_axis(IVP_INDEX_Y);
  definition.limit_translation_axis(IVP_INDEX_Z, -0.20f, 0.35f);
  definition.fix_rotation_axis(IVP_INDEX_X);
  definition.free_rotation_axis(IVP_INDEX_Y);
  definition.limit_rotation_axis(IVP_INDEX_Z, -0.5f, 0.75f);
  EXPECT_EQ(definition.axis_type[0], IVP_CONSTRAINT_AXIS_FREE);
  EXPECT_EQ(definition.axis_type[1], IVP_CONSTRAINT_AXIS_FIXED);
  EXPECT_EQ(definition.axis_type[2], IVP_CONSTRAINT_AXIS_LIMITED);
  EXPECT_EQ(definition.axis_type[3], IVP_CONSTRAINT_AXIS_FIXED);
  EXPECT_EQ(definition.axis_type[4], IVP_CONSTRAINT_AXIS_FREE);
  EXPECT_EQ(definition.axis_type[5], IVP_CONSTRAINT_AXIS_LIMITED);
  EXPECT_FLOAT_EQ(definition.borderleft_Rfs[2], -0.20f);
  EXPECT_FLOAT_EQ(definition.borderright_Rfs[2], 0.35f);
  EXPECT_FLOAT_EQ(definition.borderleft_Rfs[5], -0.5f);
  EXPECT_FLOAT_EQ(definition.borderright_Rfs[5], 0.75f);

  definition.set_stiffness_for_limited_axis(0.65f);
  definition.set_max_translation_impulse(IVP_CFE_BREAK, 80.0f);
  definition.set_max_translation_impulse(IVP_INDEX_Y, IVP_CFE_CLIP, 12.0f);
  definition.set_max_rotation_impulse(IVP_CFE_BEND, 30.0f);
  definition.set_max_rotation_impulse(IVP_INDEX_Z, IVP_CFE_BREAK, 7.5f);
  EXPECT_FLOAT_EQ(definition.limited_axis_stiffness, 0.65f);
  EXPECT_EQ(definition.maximpulse_type[0], IVP_CFE_BREAK);
  EXPECT_EQ(definition.maximpulse_type[1], IVP_CFE_CLIP);
  EXPECT_FLOAT_EQ(definition.maximpulse[1], 12.0f);
  EXPECT_EQ(definition.maximpulse_type[3], IVP_CFE_BEND);
  EXPECT_EQ(definition.maximpulse_type[5], IVP_CFE_BREAK);
  EXPECT_FLOAT_EQ(definition.maximpulse[5], 7.5f);
}

TEST(IvpConstraintConfiguration,
     MapsBallSocketCardanHingeAndFixedJointsToRetailDimensions) {
  IVP_Template_Constraint definition;
  alignas(void *) std::array<std::byte, sizeof(void *)> referenceStorage{};
  alignas(void *) std::array<std::byte, sizeof(void *)> attachedStorage{};
  auto *reference = reinterpret_cast<IVP_Real_Object *>(referenceStorage.data());
  auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedStorage.data());
  IVP_U_Point anchor(2.0, 3.0, 4.0);
  IVP_U_Point axis(0.0, 1.0, 0.0);

  definition.set_orientation(reference, attached);
  ExpectCall(ConstraintCall::Space::World, reference, nullptr, nullptr, 0, 3,
             attached);
  definition.set_ballsocket_ws(reference, &anchor, attached);
  ExpectCall(ConstraintCall::Space::World, reference, &anchor, nullptr, 3, 0,
             attached);
  definition.set_ballsocket_Ros(reference, &anchor, attached);
  ExpectCall(ConstraintCall::Space::ReferenceObject, reference, &anchor,
             nullptr, 3, 0, attached);

  IVP_U_Point tenseDistance(-0.5, 1.25, 2.0);
  definition.set_ballsocket_tense_Ros(reference, &anchor, attached,
                                      &tenseDistance);
  ExpectCall(ConstraintCall::Space::ReferenceObject, reference, &anchor,
             nullptr, 3, 0, attached);
  ASSERT_TRUE(g_call.hasAttachedDisplacement);
  EXPECT_DOUBLE_EQ(g_call.attachedDisplacement.vv.k[0], -0.5);
  EXPECT_DOUBLE_EQ(g_call.attachedDisplacement.vv.k[1], 1.25);
  EXPECT_DOUBLE_EQ(g_call.attachedDisplacement.vv.k[2], 2.0);
  EXPECT_DOUBLE_EQ(g_call.attachedDisplacement.get_elem(0, 0), 1.0);

  definition.set_cardanjoint_ws(reference, &anchor, &axis, attached);
  ExpectCall(ConstraintCall::Space::World, reference, &anchor, &axis, 3, 1,
             attached);
  definition.set_cardanjoint_Ros(reference, &anchor, &axis, attached);
  ExpectCall(ConstraintCall::Space::ReferenceObject, reference, &anchor, &axis,
             3, 1, attached);
  definition.set_hinge_ws(reference, &anchor, &axis, attached);
  ExpectCall(ConstraintCall::Space::World, reference, &anchor, &axis, 3, 2,
             attached);
  definition.set_hinge_Ros(reference, &anchor, &axis, attached);
  ExpectCall(ConstraintCall::Space::ReferenceObject, reference, &anchor, &axis,
             3, 2, attached);
  definition.set_hinge_Ros(reference, &anchor, &axis, attached, -0.4f, 0.6f);
  ExpectCall(ConstraintCall::Space::ReferenceObject, reference, &anchor, &axis,
             3, 2, attached);
  EXPECT_EQ(definition.axis_type[5], IVP_CONSTRAINT_AXIS_LIMITED);
  EXPECT_FLOAT_EQ(definition.borderleft_Rfs[5], -0.4f);
  EXPECT_FLOAT_EQ(definition.borderright_Rfs[5], 0.6f);
  definition.set_fixed(reference, attached);
  ExpectCall(ConstraintCall::Space::World, reference, nullptr, nullptr, 3, 3,
             attached);

  IVP_U_Matrix displacement;
  displacement.set_identity();
  definition.set_constraint_ws(reference, &anchor, &axis, 2, 1, attached,
                               &displacement);
  ExpectCall(ConstraintCall::Space::World, reference, &anchor, &axis, 2, 1,
             attached);
  definition.set_constraint_Ros(reference, &anchor, &axis, 1, 2, attached,
                                &displacement);
  ExpectCall(ConstraintCall::Space::ReferenceObject, reference, &anchor, &axis,
             1, 2, attached);
}

TEST(IvpConstraintConfiguration,
     ConstructsLocalConstraintSubobjectsOnceAndKeepsRawFactoryRoute) {
  g_completeConstraintBaseConstructs = 0;
  g_localInitializations = 0;
  g_localCompleteConstructs = 0;
  g_localAnchorConstructs = 0;
  g_activations = 0;
  g_controllerRemovals = 0;
  g_sawConstructedSubobjects = false;
  g_localConstraintVtable = nullptr;

  alignas(void *) std::array<std::byte, sizeof(void *)> referenceStorage{};
  alignas(void *) std::array<std::byte, sizeof(void *)> attachedStorage{};
  auto *reference = reinterpret_cast<IVP_Real_Object *>(referenceStorage.data());
  auto *attached = reinterpret_cast<IVP_Real_Object *>(attachedStorage.data());
  IVP_Template_Constraint definition;
  definition.set_reference_object(reference);
  definition.set_attached_object(attached);

  {
    IVP_Constraint_Local local(definition);
    EXPECT_EQ(local.get_objectR(), reference);
    EXPECT_EQ(local.get_objectA(), attached);
    EXPECT_EQ(g_localInitializations, 1);
    EXPECT_EQ(g_localCompleteConstructs, 1);
    EXPECT_EQ(g_completeConstraintBaseConstructs, 1);
    EXPECT_EQ(g_localAnchorConstructs, 2);
    EXPECT_EQ(g_activations, 1);
    EXPECT_TRUE(g_sawConstructedSubobjects);
  }
  EXPECT_EQ(g_controllerRemovals, 1);

  IVP_Constraint *factoryConstraint =
      IVP_Constraint::create_constraint_any_solver(&definition);
  ASSERT_NE(factoryConstraint, nullptr);
  EXPECT_EQ(g_localCompleteConstructs, 2);
  EXPECT_EQ(g_localInitializations, 2);
  EXPECT_EQ(g_completeConstraintBaseConstructs, 2);
  EXPECT_EQ(g_localAnchorConstructs, 4);
  EXPECT_EQ(g_activations, 2);
  auto *factoryLocal = static_cast<IVP_Constraint_Local *>(factoryConstraint);
  EXPECT_EQ(factoryLocal->get_objectR(), reference);
  EXPECT_EQ(factoryLocal->get_objectA(), attached);
  delete factoryConstraint;
  EXPECT_EQ(g_controllerRemovals, 2);

  IVP_Template_Constraint empty;
  EXPECT_EQ(IVP_Constraint::create_constraint_any_solver(nullptr), nullptr);
  EXPECT_EQ(IVP_Constraint::create_constraint_any_solver(&empty), nullptr);
  EXPECT_EQ(g_localCompleteConstructs, 2);
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(std::uint32_t rva) noexcept {
  return ResolveRetailCall(rva);
}
