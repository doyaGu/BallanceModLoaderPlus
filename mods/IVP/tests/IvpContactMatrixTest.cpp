#include "IvpTestAdapter.h"

#include "BML/IVP/GreatMatrix.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<char> gColumnExchangeOrder;
int gDeletedVariable = -1;

void __fastcall ConstructGreatMatrix(
    IVP_Great_Matrix_Many_Zero *matrix, void *) {
    matrix->MATRIX_EPS = 1.0e-9;
    matrix->columns = 0;
    matrix->aligned_row_len = 0;
    matrix->matrix_values = nullptr;
    matrix->desired_vector = nullptr;
    matrix->result_vector = nullptr;
}

void __fastcall ExchangeColumnsL(
    IVP_Incr_L_U_Matrix *matrix, void *, int first, int second) {
    gColumnExchangeOrder.push_back('L');
    for (int row = 0; row < matrix->n_sub; ++row) {
        std::swap(
            matrix->L_matrix[row * matrix->aligned_row_len + first],
            matrix->L_matrix[row * matrix->aligned_row_len + second]);
    }
}

void __fastcall ExchangeColumnsU(
    IVP_Incr_L_U_Matrix *matrix, void *, int first, int second) {
    gColumnExchangeOrder.push_back('U');
    for (int row = 0; row < matrix->n_sub; ++row) {
        std::swap(
            matrix->U_matrix[row * matrix->aligned_row_len + first],
            matrix->U_matrix[row * matrix->aligned_row_len + second]);
    }
}

IVP_RETURN_TYPE __fastcall DecrementLu(
    IVP_Incr_L_U_Matrix *matrix, void *, int variable) {
    gDeletedVariable = variable;
    --matrix->n_sub;
    return IVP_OK;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::GreatMatrixConstruct,
        &ConstructGreatMatrix),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::IncrementalLuExchangeColumnsL,
        &ExchangeColumnsL),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::IncrementalLuExchangeColumnsU,
        &ExchangeColumnsU),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::IncrementalLuDecrement,
        &DecrementLu),
};

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}

namespace {

template <std::size_t Size>
void ConfigureMatrix(
    IVP_Great_Matrix_Many_Zero &matrix,
    std::array<IVP_DOUBLE, Size * Size> &values,
    std::array<IVP_DOUBLE, Size> &desired,
    std::array<IVP_DOUBLE, Size> &result) {
    matrix.MATRIX_EPS = 1.0e-9;
    matrix.columns = static_cast<int>(Size);
    matrix.calc_aligned_row_len();
    matrix.matrix_values = values.data();
    matrix.desired_vector = desired.data();
    matrix.result_vector = result.data();
}

TEST(IvpContactMatrix, InvertsCoupledThreeContactEffectiveMassMatrix) {
    // Symmetric positive-definite coupling between three contact normals.
    // This is the matrix shape solved for simultaneous Ballance contacts,
    // unlike a diagonal-only arithmetic probe.
    constexpr std::array<IVP_DOUBLE, 9> original{
        2.0, -1.0, 0.25,
        -1.0, 3.0, -0.5,
        0.25, -0.5, 1.5,
    };
    auto factorizedValues = original;
    std::array<IVP_DOUBLE, 3> factorWorkspace{};
    std::array<IVP_DOUBLE, 3> factorResult{};
    IVP_Great_Matrix_Many_Zero factorized;
    ConfigureMatrix(
        factorized, factorizedValues, factorWorkspace, factorResult);

    std::array<IVP_DOUBLE, 9> inverseValues{};
    std::array<IVP_DOUBLE, 3> inverseWorkspace{};
    std::array<IVP_DOUBLE, 3> inverseResult{};
    IVP_Great_Matrix_Many_Zero inverse;
    ConfigureMatrix(inverse, inverseValues, inverseWorkspace, inverseResult);

    ASSERT_EQ(factorized.invert(&inverse), IVP_OK);

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            IVP_DOUBLE product = 0.0;
            for (int inner = 0; inner < 3; ++inner) {
                product += original[row * 3 + inner] *
                           inverseValues[inner * 3 + column];
            }
            EXPECT_NEAR(product, row == column ? 1.0 : 0.0, 1.0e-10)
                << "row=" << row << " column=" << column;
        }
    }
}

TEST(IvpContactMatrix, RejectsDependentContactEquations) {
    std::array<IVP_DOUBLE, 4> singularValues{
        1.0, 2.0,
        2.0, 4.0,
    };
    std::array<IVP_DOUBLE, 2> factorWorkspace{};
    std::array<IVP_DOUBLE, 2> factorResult{};
    IVP_Great_Matrix_Many_Zero singular;
    ConfigureMatrix(
        singular, singularValues, factorWorkspace, factorResult);

    std::array<IVP_DOUBLE, 4> inverseValues{};
    std::array<IVP_DOUBLE, 2> inverseWorkspace{};
    std::array<IVP_DOUBLE, 2> inverseResult{};
    IVP_Great_Matrix_Many_Zero inverse;
    ConfigureMatrix(inverse, inverseValues, inverseWorkspace, inverseResult);

    EXPECT_EQ(singular.invert(&inverse), IVP_FAULT);
}

TEST(IvpContactMatrix, RecoversPrincipalAxesOfBallRigidBodyInertia) {
    // A diagonal inertia tensor models a Ballance sphere expressed in its
    // principal frame.  The unique largest axis and the repeated transverse
    // eigenspace exercise both one- and two-degree-of-freedom results.
    const IVP_U_Point inertiaX(2.0, 0.0, 0.0);
    const IVP_U_Point inertiaY(0.0, 2.0, 0.0);
    const IVP_U_Point inertiaZ(0.0, 0.0, 5.0);
    IVP_U_Matrix3 inertia;
    inertia.init_rows3(&inertiaX, &inertiaY, &inertiaZ);

    EXPECT_DOUBLE_EQ(inertia.get_determinante(), 20.0);

    IVP_U_Point longitudinalAxis;
    ASSERT_EQ(inertia.calc_eigen_vector(5.0, &longitudinalAxis), 1);
    EXPECT_NEAR(longitudinalAxis.k[0], 0.0, 1.0e-12);
    EXPECT_NEAR(longitudinalAxis.k[1], 0.0, 1.0e-12);
    EXPECT_NEAR(std::fabs(longitudinalAxis.k[2]), 1.0, 1.0e-12);

    IVP_U_Point transverseAxis;
    ASSERT_EQ(inertia.calc_eigen_vector(2.0, &transverseAxis), 2);
    EXPECT_NEAR(transverseAxis.k[2], 0.0, 1.0e-12);
    EXPECT_NEAR(transverseAxis.real_length(), 1.0, 1.0e-12);

    IVP_U_Matrix3 inverse = inertia;
    ASSERT_EQ(inverse.real_invert(), IVP_OK);
    IVP_U_Matrix3 identity;
    inertia.inline_mmult3(&inverse, &identity);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_NEAR(identity.get_elem(row, column),
                        row == column ? 1.0 : 0.0, 1.0e-12);
        }
    }
}

TEST(IvpContactMatrix, CountsOnlyFullyDependentContactEquationsAsNullLines) {
    // Row zero has no diagonal pivot but still constrains the second contact;
    // it must not be mistaken for a null equation. Row one contains only
    // elimination residue below MATRIX_EPS and represents the sole dependent
    // contact. Padding is deliberately nonzero and outside the logical matrix.
    std::array<IVP_DOUBLE, 12> values{
        0.0, 1.0, 0.0, 901.0,
        2.0e-10, -4.0e-10, 1.0e-10, 902.0,
        0.0, 0.0, 2.0, 903.0,
    };
    std::array<IVP_DOUBLE, 3> desired{};
    std::array<IVP_DOUBLE, 3> result{};
    IVP_Great_Matrix_Many_Zero matrix;
    matrix.MATRIX_EPS = 1.0e-9;
    matrix.columns = 3;
    matrix.aligned_row_len = 4;
    matrix.matrix_values = values.data();
    matrix.desired_vector = desired.data();
    matrix.result_vector = result.data();

    EXPECT_EQ(matrix.get_number_null_lines(), 1);
}

TEST(IvpContactMatrix, PerformsRetailDoubleRowOperationsUsedBySolver) {
    std::array<IVP_DOUBLE, 3> first{4.0, 5.0, 6.0};
    std::array<IVP_DOUBLE, 3> second{1.0, -2.0, 0.5};

    IVP_VecFPU::fpu_add_multiple_row(
        first.data(), second.data(), 2.0, 3, IVP_FALSE);
    EXPECT_DOUBLE_EQ(first[0], 6.0);
    EXPECT_DOUBLE_EQ(first[1], 1.0);
    EXPECT_DOUBLE_EQ(first[2], 7.0);

    IVP_VecFPU::fpu_multiply_row(
        first.data(), 0.5, 3, IVP_TRUE);
    EXPECT_DOUBLE_EQ(first[0], 3.0);
    EXPECT_DOUBLE_EQ(first[1], 0.5);
    EXPECT_DOUBLE_EQ(first[2], 3.5);
    EXPECT_DOUBLE_EQ(
        IVP_VecFPU::fpu_large_dot_product(
            first.data(), second.data(), 3, IVP_FALSE),
        3.75);

    IVP_VecFPU::fpu_exchange_rows(
        first.data(), second.data(), 3, IVP_FALSE);
    EXPECT_DOUBLE_EQ(first[0], 1.0);
    EXPECT_DOUBLE_EQ(second[2], 3.5);

    IVP_VecFPU::fpu_copy_rows(
        first.data(), second.data(), 3, IVP_TRUE);
    EXPECT_EQ(first, second);
    IVP_VecFPU::fpu_set_row_to_zero(
        first.data(), 3, IVP_FALSE);
    EXPECT_EQ(first, (std::array<IVP_DOUBLE, 3>{0.0, 0.0, 0.0}));
}

TEST(IvpContactMatrix, ReordersBothFactorsWhenContactVariableLeavesActiveSet) {
    // Three coupled contact variables occupy a four-double retail row stride.
    // Reordering variable 0 behind variable 2 must apply to both factors while
    // leaving the aligned padding outside the active system untouched.
    std::array<IVP_DOUBLE, 12> lower{
        1.0, 2.0, 3.0, 101.0,
        4.0, 5.0, 6.0, 102.0,
        7.0, 8.0, 9.0, 103.0,
    };
    std::array<IVP_DOUBLE, 12> upper{
        11.0, 12.0, 13.0, 201.0,
        14.0, 15.0, 16.0, 202.0,
        17.0, 18.0, 19.0, 203.0,
    };

    IVP_Incr_L_U_Matrix factors{};
    factors.L_matrix = lower.data();
    factors.U_matrix = upper.data();
    factors.aligned_row_len = 4;
    factors.n_sub = 3;

    gColumnExchangeOrder.clear();
    factors.exchange_columns_l_u(0, 2);

    EXPECT_EQ(gColumnExchangeOrder, (std::vector<char>{'L', 'U'}));
    EXPECT_EQ(lower, (std::array<IVP_DOUBLE, 12>{
        3.0, 2.0, 1.0, 101.0,
        6.0, 5.0, 4.0, 102.0,
        9.0, 8.0, 7.0, 103.0,
    }));
    EXPECT_EQ(upper, (std::array<IVP_DOUBLE, 12>{
        13.0, 12.0, 11.0, 201.0,
        16.0, 15.0, 14.0, 202.0,
        19.0, 18.0, 17.0, 203.0,
    }));
}

TEST(IvpContactMatrix, LegacyRowColumnDeletionUsesRetailVariableRemoval) {
    IVP_Incr_L_U_Matrix factors{};
    factors.n_sub = 3;
    gDeletedVariable = -1;

    factors.delete_row_and_col_l_u(1);

    EXPECT_EQ(gDeletedVariable, 1);
    EXPECT_EQ(factors.n_sub, 2);
}

TEST(IvpContactMatrix, ReconstructsCoupledContactMatrixFromLuFactorsForDiagnosis) {
    std::array<IVP_DOUBLE, 6> lower{
        1.0, 0.0, 301.0,
        0.5, 1.0, 302.0,
    };
    std::array<IVP_DOUBLE, 6> upper{
        2.0, 1.0, 401.0,
        0.0, 3.0, 402.0,
    };
    IVP_Incr_L_U_Matrix factors{};
    factors.MATRIX_EPS = 1.0e-9;
    factors.L_matrix = lower.data();
    factors.U_matrix = upper.data();
    factors.aligned_row_len = 3;
    factors.n_sub = 2;

    testing::internal::CaptureStdout();
    factors.debug_print_a();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Matrix: orig_A"), std::string::npos) << output;
    EXPECT_NE(output.find(" 2  "), std::string::npos) << output;
    EXPECT_NE(output.find("-1  "), std::string::npos) << output;
    EXPECT_NE(output.find("2.5"), std::string::npos) << output;
}

} // namespace
