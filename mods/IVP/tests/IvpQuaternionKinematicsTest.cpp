#include "IvpTestAdapter.h"

#include "BML/IVP/Types.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>

namespace {

void __fastcall SetInverseUnit(IVP_U_Quat *output, void *,
                               const IVP_U_Quat *input) {
  output->set(-input->x, -input->y, -input->z, input->w);
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::QuaternionSetInverseUnit,
                         &SetInverseUnit),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
  return BML::IVP::Test::Resolve(rva, kRetailCalls);
}

IVP_DOUBLE Norm(const IVP_U_Quat &value) {
  return std::sqrt(value.x * value.x + value.y * value.y +
                   value.z * value.z + value.w * value.w);
}

template <class T>
void ExpectRetailDefaultConstructionPreservesScratchBytes() {
  alignas(T) std::array<std::byte, sizeof(T)> storage;
  std::fill(storage.begin(), storage.end(), std::byte{0xA5});

  T *value = ::new (static_cast<void *>(storage.data())) T;
  (void)value;

  EXPECT_TRUE(std::all_of(storage.begin(), storage.end(), [](std::byte byte) {
    return byte == std::byte{0xA5};
  }));
}

TEST(IvpQuaternionKinematics,
     PreservesRetailScratchStorageUntilTheCallerInitializesIt) {
  // Ballance-era IVP uses these POD-like values as stack and embedded scratch
  // space. Their default constructors are intentional no-ops; initialization
  // is performed by set/init methods at the point where a value becomes live.
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_Time>();
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_U_Float_Point3>();
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_U_Float_Point>();
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_U_Point>();
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_U_Matrix3>();
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_U_Matrix>();
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_U_Quat>();
  ExpectRetailDefaultConstructionPreservesScratchBytes<IVP_U_Float_Quat>();
}

TEST(IvpQuaternionKinematics, PreservesBallanceElapsedTimeDoublePrecision) {
  const IVP_Time later(1000000.123456789);
  const IVP_Time earlier(999999.0);
  const double fullPrecision = 1000000.123456789 - 999999.0;

  EXPECT_DOUBLE_EQ(later - earlier, fullPrecision);
  EXPECT_NE(later - earlier,
            static_cast<double>(static_cast<IVP_FLOAT>(fullPrecision)));
}

TEST(IvpQuaternionKinematics,
     PreservesBallanceOrientationCompositionAndRoundTrips) {
  constexpr IVP_DOUBLE halfPi = 1.57079632679489661923;

  // Aim a Ballance object's local +Z direction at world +X, then verify the
  // generated rotation through the public matrix representation.
  IVP_U_Quat quarterTurn;
  quarterTurn.set_from_rotation_vectors(0.0, 0.0, 1.0, 1.0, 0.0, 0.0);
  EXPECT_NEAR(Norm(quarterTurn), 1.0, 1.0e-12);
  IVP_DOUBLE worldFromObject[4][4]{};
  quarterTurn.set_matrix(worldFromObject);
  EXPECT_NEAR(worldFromObject[0][2], 1.0, 1.0e-12);
  EXPECT_NEAR(worldFromObject[1][2], 0.0, 1.0e-12);
  EXPECT_NEAR(worldFromObject[2][2], 0.0, 1.0e-12);
  EXPECT_DOUBLE_EQ(worldFromObject[3][3], 1.0);

  IVP_U_Quat roundTrip;
  roundTrip.set_quaternion(worldFromObject);
  IVP_U_Quat inverse = quarterTurn;
  inverse.invert_quat();
  // This overload is the historical OpenGL column-major transport API.  A C
  // array written by set_matrix and read without transposition therefore
  // represents the conjugate orientation, unlike IVP_U_Matrix3.
  EXPECT_NEAR(std::fabs(inverse.acos_quat(&roundTrip)), 1.0, 1.0e-12);

  IVP_U_Quat identity;
  identity.inline_set_mult_quat(&quarterTurn, &inverse);
  EXPECT_NEAR(identity.x, 0.0, 1.0e-12);
  EXPECT_NEAR(identity.y, 0.0, 1.0e-12);
  EXPECT_NEAR(identity.z, 0.0, 1.0e-12);
  EXPECT_NEAR(identity.w, 1.0, 1.0e-12);

  IVP_U_Quat relative;
  relative.set_div_unit_quat(&quarterTurn, &quarterTurn);
  EXPECT_NEAR(relative.x, 0.0, 1.0e-12);
  EXPECT_NEAR(relative.y, 0.0, 1.0e-12);
  EXPECT_NEAR(relative.z, 0.0, 1.0e-12);
  EXPECT_NEAR(relative.w, 1.0, 1.0e-12);

  // The early API stores three independent half-angle sines. Exercise both
  // setters and the matching diagnostic angle extraction used by controllers.
  IVP_U_Quat diagnostic;
  diagnostic.set(0.20, -0.30, 0.40);
  IVP_U_Float_Point angles;
  diagnostic.get_angles(&angles);
  EXPECT_NEAR(angles.k[0], 0.20f, 1.0e-6f);
  EXPECT_NEAR(angles.k[1], -0.30f, 1.0e-6f);
  EXPECT_NEAR(angles.k[2], 0.40f, 1.0e-6f);

  IVP_U_Point turnVector(0.0, halfPi, 0.0);
  diagnostic.set_fast_multiple(&turnVector, 0.5);
  diagnostic.get_angles(&angles);
  EXPECT_NEAR(angles.k[0], 0.0f, 1.0e-6f);
  EXPECT_NEAR(angles.k[1], static_cast<IVP_FLOAT>(halfPi * 0.5), 1.0e-6f);
  EXPECT_NEAR(angles.k[2], 0.0f, 1.0e-6f);

  IVP_U_Quat start;
  start.init();
  IVP_U_Quat halfway;
  halfway.set_interpolate_linear(&start, &quarterTurn, 0.5);
  EXPECT_NEAR(halfway.x, quarterTurn.x * 0.5, 1.0e-12);
  EXPECT_NEAR(halfway.y, quarterTurn.y * 0.5, 1.0e-12);
  EXPECT_NEAR(halfway.z, quarterTurn.z * 0.5, 1.0e-12);
  EXPECT_NEAR(halfway.w, (1.0 + quarterTurn.w) * 0.5, 1.0e-12);

  IVP_U_Float_Quat floatTarget{};
  floatTarget.set(&quarterTurn);
  const IVP_DOUBLE expectedDifference =
      (1.0 - std::pow(halfway.x * floatTarget.x +
                          halfway.y * floatTarget.y +
                          halfway.z * floatTarget.z +
                          halfway.w * floatTarget.w,
                      2.0)) *
      2.0;
  EXPECT_NEAR(halfway.inline_estimate_q_diff_to(&floatTarget),
              expectedDifference, 1.0e-12);
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(std::uint32_t rva) noexcept {
  return ResolveRetailCall(rva);
}
