#ifndef BML_IVP_GREAT_MATRIX_H
#define BML_IVP_GREAT_MATRIX_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <vector>

inline constexpr int IVP_MAX_GREAT_MATRIX_SIZE = 2048;

enum IVP_2P_RET : std::int32_t {
    IVP_2P_SIGMA_TRUE,
    IVP_2P_SIGMA_FALSE,
    IVP_2P_ILLEGAL,
};

class IVP_U_Memory;
class IVP_Great_Matrix_Many_Zero;

// Ballance compiles these public row primitives completely inline. Its x86
// build uses one IVP_DOUBLE per vector-FPU unit, so a scalar implementation is
// both ABI-neutral and mathematically identical to the retained call-site
// expansions. addressAligned remains a source-compatible performance hint.
class IVP_VecFPU {
public:
    static void fpu_add_multiple_row(
        IVP_DOUBLE *targetAddress, IVP_DOUBLE *sourceAddress,
        IVP_DOUBLE factor, int size, IVP_BOOL addressAligned) {
        (void)addressAligned;
        for (int index = 0; index < size; ++index)
            targetAddress[index] += sourceAddress[index] * factor;
    }
    static void fpu_multiply_row(
        IVP_DOUBLE *targetAddress, IVP_DOUBLE factor,
        int size, IVP_BOOL addressAligned) {
        (void)addressAligned;
        for (int index = 0; index < size; ++index)
            targetAddress[index] *= factor;
    }
    static void fpu_exchange_rows(
        IVP_DOUBLE *firstAddress, IVP_DOUBLE *secondAddress,
        int size, IVP_BOOL addressAligned) {
        (void)addressAligned;
        for (int index = 0; index < size; ++index) {
            const IVP_DOUBLE temporary = firstAddress[index];
            firstAddress[index] = secondAddress[index];
            secondAddress[index] = temporary;
        }
    }
    static void fpu_copy_rows(
        IVP_DOUBLE *targetAddress, IVP_DOUBLE *sourceAddress,
        int size, IVP_BOOL addressAligned) {
        (void)addressAligned;
        for (int index = 0; index < size; ++index)
            targetAddress[index] = sourceAddress[index];
    }
    static void fpu_set_row_to_zero(
        IVP_DOUBLE *targetAddress, int size, IVP_BOOL addressAligned) {
        (void)addressAligned;
        for (int index = 0; index < size; ++index)
            targetAddress[index] = 0.0;
    }
    static IVP_DOUBLE fpu_large_dot_product(
        IVP_DOUBLE *firstAddress, IVP_DOUBLE *secondAddress,
        int size, IVP_BOOL addressAligned) {
        (void)addressAligned;
        IVP_DOUBLE result = 0.0;
        for (int index = size - 1; index >= 0; --index)
            result += firstAddress[index] * secondAddress[index];
        return result;
    }
};

// Incremental LU state. The public fields and all exposed operations match
// retail field accesses; the implementation remains in physics_RT.dll.
class IVP_Incr_L_U_Matrix {
public:
    IVP_DOUBLE MATRIX_EPS;
    IVP_DOUBLE *L_matrix;
    IVP_DOUBLE *U_matrix;
    int *index_pos_contains;
    int *inv_index_pos_contains;
    IVP_DOUBLE *input_vec;
    IVP_DOUBLE *out_vec;
    IVP_DOUBLE *temp_vec;
    IVP_DOUBLE *mult_vec;
    int aligned_row_len;
    int n_sub;

    // The adjacent public header exposes this older index-based family, but
    // no implementation exists in that tree or in Ballance. Retain the exact
    // declarations so source can be ported without mistaking them for retail
    // entry points.
    void solve_lin_equ_index() = delete;
    void mult_vec_with_L_index() = delete;
    IVP_RETURN_TYPE l_u_decomposition_with_pivoting_index() = delete;
    void pivot_search_l_u_index(int column) = delete;
    void add_neg_row_to_row_l_u_index(
        int pivotRow, int destinationRow, IVP_DOUBLE factor) = delete;
    void exchange_rows_l_u_index(int pivotColumn, int exchange) = delete;

    IVP_RETURN_TYPE increment_l_u() {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::IncrementalLuIncrement, this);
    }
    void solve_lin_equ() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuSolveLinearEquation, this);
    }
    void mult_vec_with_L() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuMultiplyVectorL, this);
    }
    void solve_vec_with_U() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuSolveVectorU, this);
    }
    IVP_RETURN_TYPE normize_row(int row) {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::IncrementalLuNormalizeRow, this, row);
    }
    IVP_RETURN_TYPE l_u_decomposition_with_pivoting() {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::IncrementalLuDecompose, this);
    }
    void pivot_search_l_u(int column) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuPivotSearch, this, column);
    }
    void add_neg_row_to_row_l_u(
        int pivotRow, int destinationRow, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuAddNegativeRow, this,
            pivotRow, destinationRow, factor);
    }
    void add_neg_row_upwards_l_u(
        int lowerRow, int destinationRow, IVP_DOUBLE factor) {
        IVP_DOUBLE *sourceU = &U_matrix[lowerRow * aligned_row_len];
        IVP_DOUBLE *destinationU =
            &U_matrix[destinationRow * aligned_row_len];
        for (int column = lowerRow + 1; column < n_sub; ++column)
            destinationU[column] -= factor * sourceU[column];

        IVP_DOUBLE *sourceL = &L_matrix[lowerRow * aligned_row_len];
        IVP_DOUBLE *destinationL =
            &L_matrix[destinationRow * aligned_row_len];
        for (int column = 0; column < n_sub; ++column)
            destinationL[column] -= factor * sourceL[column];
        destinationL[lowerRow] = 0.0;
    }
    void exchange_rows_l_u(int pivotColumn, int exchange) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuExchangeRows, this,
            pivotColumn, exchange);
    }
    void exchange_columns_l_u(int first, int second) {
        // Ballance retains the two concrete column-exchange bodies. The
        // stripped public convenience method is exactly the operation over
        // both factors; keeping the calls separate also preserves the retail
        // row stride and scalar precision.
        exchange_columns_L(first, second);
        exchange_columns_U(first, second);
    }
    IVP_RETURN_TYPE normize_row_L(int row) {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::IncrementalLuNormalizeRowL, this, row);
    }
    void subtract_row_L(int source, int destination, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuSubtractRowL, this,
            source, destination, factor);
    }
    IVP_RETURN_TYPE decrement_l_u(int deletedIndex) {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::IncrementalLuDecrement,
            this, deletedIndex);
    }
    void delete_row_and_col_l_u(int variable) {
        // This is the old void spelling of the retained variable-removal
        // operation. Ballance's decrement_l_u body performs the complete
        // factor update and reports a status that this signature cannot
        // propagate.
        (void)decrement_l_u(variable);
    }
    void exchange_columns_L(int first, int second) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuExchangeColumnsL,
            this, first, second);
    }
    void exchange_columns_U(int first, int second) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuExchangeColumnsU,
            this, first, second);
    }
    void add_neg_row_L(
        int sourceRow, int destinationRow, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::IncrementalLuAddNegativeRowL,
            this, sourceRow, destinationRow, factor);
    }
    void add_neg_col_L(
        int sourceColumn, int destinationColumn, IVP_DOUBLE factor) {
        for (int row = n_sub - 1; row >= 0; --row) {
            L_matrix[row * aligned_row_len + destinationColumn] -=
                factor * L_matrix[row * aligned_row_len + sourceColumn];
        }
    }
    void debug_print_a();
    void debug_print_l_u() {
        std::printf("  L                                      U\n");
        for (int row = 0; row < n_sub; ++row) {
            for (int column = 0; column < n_sub; ++column) {
                std::printf(
                    "%.5f  ",
                    L_matrix[row * aligned_row_len + column]);
            }
            std::printf("          ");
            for (int column = 0; column < n_sub; ++column) {
                std::printf(
                    "%.5f  ",
                    U_matrix[row * aligned_row_len + column]);
            }
            std::printf("\n");
        }
    }
};

// Sparse square-matrix state. No foreign allocator or implementation is
// linked; callers supply buffers and retail methods operate on them.
class IVP_Great_Matrix_Many_Zero {
public:
    // This historical allocating constructor is present in the neighboring
    // public header, but that same source marks it unusable and its body does
    // not survive in Ballance. Keep the exact declaration without inventing
    // allocator ownership or a destructor contract.
    IVP_Great_Matrix_Many_Zero(int n) = delete;

    IVP_Great_Matrix_Many_Zero() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixConstruct, this);
    }

    void calc_aligned_row_len() {
        // Ballance's x86 build uses one double per vector-FPU unit. The
        // retained align_matrix_values body consequently aligns only the
        // base pointer to 8 bytes and does not round the logical row width.
        aligned_row_len = columns;
    }
    void debug_fill_zero() {
        for (int row = 0; row < columns; ++row) {
            for (int column = 0; column < aligned_row_len; ++column)
                matrix_values[row * aligned_row_len + column] = 0.0;
        }
    }
    void set_value(IVP_DOUBLE value, int column, int row) {
        matrix_values[row * aligned_row_len + column] = value;
    }
    IVP_DOUBLE get_value(int column, int row) const {
        return matrix_values[row * aligned_row_len + column];
    }
    void copy_matrix(IVP_DOUBLE *values, IVP_DOUBLE *desired) {
        for (int row = 0; row < columns; ++row) {
            for (int column = 0; column < columns; ++column) {
                values[row * columns + column] =
                    matrix_values[row * aligned_row_len + column];
            }
            desired[row] = desired_vector[row];
        }
    }
    void copy_matrix(IVP_Great_Matrix_Many_Zero *source) {
        for (int row = 0; row < columns; ++row) {
            for (int column = 0; column < columns; ++column) {
                matrix_values[row * aligned_row_len + column] =
                    source->matrix_values[
                        row * source->aligned_row_len + column];
            }
            desired_vector[row] = source->desired_vector[row];
        }
    }
    void copy_to_sub_matrix(
        IVP_DOUBLE *valuesBigMatrix,
        IVP_Great_Matrix_Many_Zero *subMatrix,
        int *originalPositions) {
        for (int row = 0; row < subMatrix->columns; ++row) {
            for (int column = 0; column < subMatrix->columns; ++column) {
                subMatrix->matrix_values[
                    row * subMatrix->aligned_row_len + column] =
                    valuesBigMatrix[
                        originalPositions[row] * columns +
                        originalPositions[column]];
            }
        }
    }
    void matrix_multiplication(IVP_DOUBLE *first, IVP_DOUBLE *second) {
        for (int row = 0; row < columns; ++row) {
            for (int column = 0; column < columns; ++column) {
                IVP_DOUBLE sum = 0.0;
                for (int index = 0; index < columns; ++index) {
                    sum += first[row * columns + index] *
                           second[index * columns + column];
                }
                matrix_values[row * aligned_row_len + column] = sum;
            }
        }
    }
    IVP_RETURN_TYPE lu_crout(int *indexVector, IVP_DOUBLE *sign) {
        if (!indexVector || !sign || !matrix_values || !desired_vector ||
            columns < 0 || aligned_row_len < columns) {
            return IVP_FAULT;
        }

        *sign = 1.0;
        int zeroPivots = 0;

        // The adjacent implementation uses desired_vector as its row-scale
        // workspace. Ballance's retained matrix methods establish the same
        // row-major layout and double-precision element type.
        for (int row = 0; row < columns; ++row) {
            IVP_DOUBLE biggest = 0.0;
            for (int column = 0; column < columns; ++column) {
                const IVP_DOUBLE value = std::fabs(
                    matrix_values[row * aligned_row_len + column]);
                if (value > biggest)
                    biggest = value;
            }
            desired_vector[row] = biggest == 0.0 ? 0.0 : 1.0 / biggest;
        }

        for (int column = 0; column < columns; ++column) {
            for (int row = 0; row < column; ++row) {
                IVP_DOUBLE sum =
                    matrix_values[row * aligned_row_len + column];
                for (int inner = 0; inner < row; ++inner) {
                    sum -= matrix_values[row * aligned_row_len + inner] *
                           matrix_values[inner * aligned_row_len + column];
                }
                matrix_values[row * aligned_row_len + column] = sum;
            }

            IVP_DOUBLE biggest = 0.0;
            int pivotRow = column;
            for (int row = column; row < columns; ++row) {
                IVP_DOUBLE sum =
                    matrix_values[row * aligned_row_len + column];
                for (int inner = 0; inner < column; ++inner) {
                    sum -= matrix_values[row * aligned_row_len + inner] *
                           matrix_values[inner * aligned_row_len + column];
                }
                matrix_values[row * aligned_row_len + column] = sum;
                const IVP_DOUBLE scaled =
                    desired_vector[row] * std::fabs(sum);
                if (scaled >= biggest) {
                    biggest = scaled;
                    pivotRow = row;
                }
            }

            if (pivotRow != column) {
                for (int inner = 0; inner < columns; ++inner) {
                    const int pivotIndex =
                        pivotRow * aligned_row_len + inner;
                    const int columnIndex =
                        column * aligned_row_len + inner;
                    const IVP_DOUBLE temporary = matrix_values[pivotIndex];
                    matrix_values[pivotIndex] = matrix_values[columnIndex];
                    matrix_values[columnIndex] = temporary;
                }
                *sign = -*sign;
                desired_vector[pivotRow] = desired_vector[column];
            }
            indexVector[column] = pivotRow;

            IVP_DOUBLE &diagonal =
                matrix_values[column * aligned_row_len + column];
            if (std::fabs(diagonal) < MATRIX_EPS) {
                ++zeroPivots;
            } else if (column != columns - 1) {
                const IVP_DOUBLE inverse = 1.0 / diagonal;
                for (int row = column + 1; row < columns; ++row)
                    matrix_values[row * aligned_row_len + column] *= inverse;
            }
        }

        return zeroPivots == 0 ? IVP_OK : IVP_FAULT;
    }
    IVP_RETURN_TYPE lu_solve(int *indexVector) {
        if (!indexVector || !matrix_values || !desired_vector ||
            !result_vector || columns < 0 || aligned_row_len < columns) {
            return IVP_FAULT;
        }

        for (int index = 0; index < columns; ++index) {
            if (std::fabs(
                    matrix_values[index * aligned_row_len + index]) <
                MATRIX_EPS) {
                return IVP_FAULT;
            }
        }

        int firstNonzero = -1;
        for (int row = 0; row < columns; ++row) {
            const int permutedRow = indexVector[row];
            IVP_DOUBLE sum = desired_vector[permutedRow];
            desired_vector[permutedRow] = desired_vector[row];
            if (firstNonzero >= 0) {
                for (int column = firstNonzero; column < row; ++column) {
                    sum -= matrix_values[row * aligned_row_len + column] *
                           desired_vector[column];
                }
            } else if (sum != 0.0) {
                firstNonzero = row;
            }
            desired_vector[row] = sum;
        }

        for (int row = columns - 1; row >= 0; --row) {
            IVP_DOUBLE sum = desired_vector[row];
            for (int column = row + 1; column < columns; ++column) {
                sum -= matrix_values[row * aligned_row_len + column] *
                       desired_vector[column];
            }
            desired_vector[row] =
                sum / matrix_values[row * aligned_row_len + row];
            result_vector[row] = desired_vector[row];
        }
        return IVP_OK;
    }
    IVP_RETURN_TYPE lu_inverse(
        IVP_Great_Matrix_Many_Zero *output, int *indexVector) {
        if (!output || output->columns != columns ||
            output->aligned_row_len < columns || !output->matrix_values) {
            return IVP_FAULT;
        }

        for (int inputColumn = 0; inputColumn < columns; ++inputColumn) {
            for (int row = 0; row < columns; ++row)
                desired_vector[row] = 0.0;
            desired_vector[inputColumn] = 1.0;

            if (lu_solve(indexVector) == IVP_FAULT)
                return IVP_FAULT;
            for (int row = 0; row < columns; ++row) {
                output->matrix_values[
                    row * output->aligned_row_len + inputColumn] =
                    result_vector[row];
            }
        }
        return IVP_OK;
    }
    IVP_RETURN_TYPE invert(IVP_Great_Matrix_Many_Zero *output) {
        if (!output || columns < 0 ||
            columns > IVP_MAX_GREAT_MATRIX_SIZE) {
            return IVP_FAULT;
        }

        int indexVector[IVP_MAX_GREAT_MATRIX_SIZE];
        IVP_DOUBLE sign = 1.0;
        if (lu_crout(indexVector, &sign) == IVP_FAULT)
            return IVP_FAULT;
        return lu_inverse(output, indexVector);
    }
    void mult() { mult_aligned(); }

    void align_matrix_values() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixAlignValues, this);
    }
    IVP_RETURN_TYPE solve_great_matrix_many_zero() {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::GreatMatrixSolve, this);
    }
    IVP_RETURN_TYPE solve_lower_null_matrix() {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::GreatMatrixSolveLowerNull, this);
    }
    void transform_to_lower_null_triangle() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixTransformLowerNull, this);
    }
    void add_multiple_line(int first, int second, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixAddMultipleLine,
            this, first, second, factor);
    }
    void exchange_rows(int first, int second) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixExchangeRows,
            this, first, second);
    }
    void find_pivot_in_column(int column) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixFindPivot, this, column);
    }
    void fill_from_bigger_matrix(
        IVP_Great_Matrix_Many_Zero *matrix,
        int *originalPositions, int columnCount) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixFillFromBigger, this,
            matrix, originalPositions, columnCount);
    }
    IVP_RETURN_TYPE matrix_check_unequation_line(int line) {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::GreatMatrixCheckUnequationLine,
            this, line);
    }
    int test_result(IVP_DOUBLE *oldMatrix, IVP_DOUBLE *oldDesired) {
        for (int row = 0; row < columns; ++row) {
            IVP_DOUBLE leftSide = 0.0;
            for (int column = 0; column < columns; ++column) {
                leftSide += oldMatrix[row * columns + column] *
                            result_vector[column];
                std::printf(
                    "multip %.5f %.5f  ",
                    oldMatrix[row * columns + column],
                    result_vector[column]);
            }
            std::printf(
                "\nwanted_res was %.5f now %.5f\n",
                oldDesired[row], leftSide);
        }
        return 0;
    }
    void matrix_out_before_gauss() {
        for (int row = 0; row < columns; ++row) {
            for (int column = 0; column < columns; ++column) {
                std::printf(
                    "%.5f  ",
                    matrix_values[row * aligned_row_len + column]);
            }
            std::printf("=  %.5f\n", desired_vector[row]);
        }
    }
    void matrix_test_unequation() {
        for (int row = 0; row < columns; ++row) {
            IVP_DOUBLE left = 0.0;
            for (int column = 0; column < columns; ++column) {
                left += result_vector[column] *
                        matrix_values[row * aligned_row_len + column];
            }
            std::printf(
                "unequation_test left %f right %f\n",
                left, desired_vector[row]);
        }
    }
    int print_great_matrix(const char *comment) const {
        std::printf("Matrix: %s\n", comment);
        for (int row = 0; row < columns; ++row) {
            for (int column = 0; column < columns; ++column) {
                const IVP_DOUBLE value = get_value(column, row);
                if (std::fabs(value) < 1.0e-20)
                    std::printf("    0  ");
                else
                    std::printf("%2.6g  ", value);
            }
            std::printf("\n");
        }
        std::printf("desired ");
        for (int row = 0; row < columns; ++row)
            std::printf("%.6f ", desired_vector[row]);
        std::printf("\n");
        return 0;
    }

    // The historical body was linked out, but this read-only rank helper is
    // fully determined by the retained row-major layout and MATRIX_EPS. A
    // zero diagonal alone is not a null equation: inspect the complete row.
    int get_number_null_lines() {
        if (columns <= 0 || aligned_row_len < columns || !matrix_values)
            return 0;

        int nullLines = 0;
        for (int row = 0; row < columns; ++row) {
            bool isNull = true;
            for (int column = 0; column < columns; ++column) {
                if (std::fabs(
                        matrix_values[row * aligned_row_len + column]) >=
                    MATRIX_EPS) {
                    isNull = false;
                    break;
                }
            }
            if (isNull)
                ++nullLines;
        }
        return nullLines;
    }

    // These three solver names are public declarations in the neighboring
    // tree, but neither that tree nor Ballance contains a callable body.
    IVP_RETURN_TYPE solve_lower_null_matrix_unsymmetrical(
        int *columnIsOriginal, int *columnsWentInactive,
        int *numberColumnsWentInactive) = delete;
    IVP_RETURN_TYPE solve_lp_with_complex() = delete;
    IVP_RETURN_TYPE solve_lp_with_own_complex(IVP_U_Memory *memory) = delete;

    void mult_aligned() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::GreatMatrixMultiplyAligned, this);
    }

    IVP_DOUBLE MATRIX_EPS;
    int columns;
    int aligned_row_len;
    IVP_DOUBLE *matrix_values;
    IVP_DOUBLE *desired_vector;
    IVP_DOUBLE *result_vector;
};

inline void IVP_Incr_L_U_Matrix::debug_print_a() {
    if (n_sub < 0 || n_sub > IVP_MAX_GREAT_MATRIX_SIZE ||
        aligned_row_len < n_sub || !L_matrix || !U_matrix) {
        return;
    }

    const std::size_t elementCount =
        static_cast<std::size_t>(n_sub) * static_cast<std::size_t>(n_sub);
    std::vector<IVP_DOUBLE> lowerValues(elementCount);
    std::vector<IVP_DOUBLE> upperValues(elementCount);
    std::vector<IVP_DOUBLE> inverseValues(elementCount);
    std::vector<IVP_DOUBLE> originalValues(elementCount);
    std::vector<IVP_DOUBLE> lowerWorkspace(n_sub);
    std::vector<IVP_DOUBLE> lowerResult(n_sub);
    std::vector<IVP_DOUBLE> inverseWorkspace(n_sub);
    std::vector<IVP_DOUBLE> inverseResult(n_sub);
    std::vector<IVP_DOUBLE> originalWorkspace(n_sub);
    std::vector<IVP_DOUBLE> originalResult(n_sub);

    IVP_Great_Matrix_Many_Zero lower;
    IVP_Great_Matrix_Many_Zero inverse;
    IVP_Great_Matrix_Many_Zero original;
    auto configure = [this](
                         IVP_Great_Matrix_Many_Zero &matrix,
                         IVP_DOUBLE *values, IVP_DOUBLE *workspace,
                         IVP_DOUBLE *result) {
        matrix.MATRIX_EPS = MATRIX_EPS;
        matrix.columns = n_sub;
        matrix.calc_aligned_row_len();
        matrix.matrix_values = values;
        matrix.desired_vector = workspace;
        matrix.result_vector = result;
    };
    configure(
        lower, lowerValues.data(), lowerWorkspace.data(), lowerResult.data());
    configure(
        inverse, inverseValues.data(), inverseWorkspace.data(),
        inverseResult.data());
    configure(
        original, originalValues.data(), originalWorkspace.data(),
        originalResult.data());

    for (int row = 0; row < n_sub; ++row) {
        for (int column = 0; column < n_sub; ++column) {
            lowerValues[row * n_sub + column] =
                L_matrix[row * aligned_row_len + column];
            upperValues[row * n_sub + column] =
                U_matrix[row * aligned_row_len + column];
        }
    }

    if (lower.invert(&inverse) == IVP_OK) {
        original.matrix_multiplication(
            inverse.matrix_values, upperValues.data());
        original.print_great_matrix("orig_A");
    }
}

// Historical tableau used by early unilateral-constraint experiments. The
// complete field layout is present in the neighboring public header, but all
// seven algorithms lack definitions there and are absent from Ballance. They
// are explicitly deleted here: this is a source-layout compatibility type,
// not a claim of retail code.
class IVP_Complex_Simple {
public:
    int memory_column;
    int n_columns;
    int m_rows;
    IVP_INT32 *inactives;
    IVP_INT32 *inactives_copy;
    IVP_INT32 *actives;
    IVP_INT32 *actives_copy;
    IVP_DOUBLE *matrix;
    IVP_DOUBLE *matrix_copy;
    IVP_INT32 *index_is_at;
    IVP_INT32 *index_is_at_copy;
    IVP_DOUBLE *help_column;
    IVP_DOUBLE *help_row;

    int search_for_save_pivots(int *pivotRow, int *pivotColumn) = delete;
    void complex_step(int pivotRow, int pivotColumn) = delete;
    void complex_step_fast(int pivotRow, int pivotColumn) = delete;
    IVP_RETURN_TYPE complex_solving() = delete;
    void debug_print(void *file) = delete;
    void make_copy() = delete;
    void undo_copy() = delete;
};

class alignas(8) IVP_Linear_Constraint_Solver {
    friend struct BML_IvpLinearConstraintSolverLayoutCheck;

public:
    IVP_RETURN_TYPE init_and_solve_lc(
        IVP_DOUBLE *matrix, IVP_DOUBLE *desired,
        IVP_DOUBLE *result, int variableCount,
        int activeCount, IVP_U_Memory *memory) {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::LinearConstraintSolverSolve,
            this, matrix, desired, result, variableCount,
            activeCount, memory);
    }

private:
    IVP_DOUBLE SOLVER_EPS;
    IVP_DOUBLE GAUSS_EPS;
    IVP_DOUBLE TEST_EPS;
    IVP_DOUBLE MAX_STEP_LEN;
    IVP_DOUBLE *full_A;
    IVP_DOUBLE *full_b;
    IVP_DOUBLE *temp;
    IVP_DOUBLE *full_x;
    IVP_DOUBLE *delta_f;
    IVP_DOUBLE *accel;
    IVP_DOUBLE *delta_accel;
    IVP_DOUBLE *reset_x;
    IVP_DOUBLE *reset_accel;
    int *actives_inactives_ignored;
    int *variable_is_found_at;
    int n_variables;
    int aligned_size;
    int r_actives;
    int aligned_sub_size;
    int ignored_pos;
    int debug_lcs;
    int debug_no_lu_count;
    int first_permute_index;
    int second_permute_index;
    int first_permute_ignored;
    int second_permute_ignored;
    int sub_solver_status;
    std::uint32_t alignment_padding_7C;
    IVP_Incr_L_U_Matrix lu_sub_solver;
    IVP_Great_Matrix_Many_Zero sub_solver_mat;
    IVP_Great_Matrix_Many_Zero full_solver_mat;
    IVP_Great_Matrix_Many_Zero debug_mat;
    IVP_Great_Matrix_Many_Zero inv_mat;
};

struct BML_IvpLinearConstraintSolverLayoutCheck {
    static constexpr std::size_t solver_epsilon =
        offsetof(IVP_Linear_Constraint_Solver, SOLVER_EPS);
    static constexpr std::size_t full_matrix =
        offsetof(IVP_Linear_Constraint_Solver, full_A);
    static constexpr std::size_t variables =
        offsetof(IVP_Linear_Constraint_Solver, n_variables);
    static constexpr std::size_t status =
        offsetof(IVP_Linear_Constraint_Solver, sub_solver_status);
    static constexpr std::size_t lu =
        offsetof(IVP_Linear_Constraint_Solver, lu_sub_solver);
    static constexpr std::size_t sub_matrix =
        offsetof(IVP_Linear_Constraint_Solver, sub_solver_mat);
    static constexpr std::size_t inverse_matrix =
        offsetof(IVP_Linear_Constraint_Solver, inv_mat);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_VecFPU) == 0x01);
static_assert(sizeof(IVP_Incr_L_U_Matrix) == 0x30);
static_assert(offsetof(IVP_Incr_L_U_Matrix, L_matrix) == 0x08);
static_assert(offsetof(IVP_Incr_L_U_Matrix, aligned_row_len) == 0x28);
static_assert(sizeof(IVP_Great_Matrix_Many_Zero) == 0x20);
static_assert(offsetof(IVP_Great_Matrix_Many_Zero, columns) == 0x08);
static_assert(offsetof(IVP_Great_Matrix_Many_Zero, matrix_values) == 0x10);
static_assert(sizeof(IVP_Complex_Simple) == 0x34);
static_assert(offsetof(IVP_Complex_Simple, matrix) == 0x1C);
static_assert(offsetof(IVP_Complex_Simple, help_row) == 0x30);
static_assert(sizeof(IVP_Linear_Constraint_Solver) == 0x130);
static_assert(BML_IvpLinearConstraintSolverLayoutCheck::solver_epsilon == 0x00);
static_assert(BML_IvpLinearConstraintSolverLayoutCheck::full_matrix == 0x20);
static_assert(BML_IvpLinearConstraintSolverLayoutCheck::variables == 0x4C);
static_assert(BML_IvpLinearConstraintSolverLayoutCheck::status == 0x78);
static_assert(BML_IvpLinearConstraintSolverLayoutCheck::lu == 0x80);
static_assert(BML_IvpLinearConstraintSolverLayoutCheck::sub_matrix == 0xB0);
static_assert(BML_IvpLinearConstraintSolverLayoutCheck::inverse_matrix ==
              0x110);
#endif

#endif // BML_IVP_GREAT_MATRIX_H
