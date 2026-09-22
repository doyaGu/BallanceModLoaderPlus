#ifndef BML_IVP_INTERPOLATION_H
#define BML_IVP_INTERPOLATION_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <vector>

class IVP_MI_Vector_Base {
public:
    int nr_of_elements = 0;
    IVP_FLOAT weight_statistic = 0.0f;
    IVP_Time time_stamp;
};

// Variable-length float vector used by IVP's multidimensional interpolation
// and buoyancy code. Release instances returned here through physics_RT Free.
class IVP_MI_Vector : public IVP_MI_Vector_Base {
private:
    IVP_MI_Vector() = default;

public:
    static IVP_MI_Vector *malloc_mi_vector(int size) {
        if (size <= 0)
            return nullptr;
        const std::size_t bytes =
            sizeof(IVP_MI_Vector) +
            sizeof(IVP_FLOAT) * static_cast<std::size_t>(size - 1);
        auto *result = static_cast<IVP_MI_Vector *>(
            BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::Allocate,
                static_cast<unsigned int>(bytes)));
        if (!result)
            return nullptr;
        result->nr_of_elements = size;
        result->weight_statistic = 0.0f;
        result->time_stamp = IVP_Time();
        for (int index = 0; index < size; ++index)
            result->element[index] = 0.0f;
        return result;
    }

    static IVP_MI_Vector *malloc_and_set_mi_vector(
        int size, IVP_FLOAT *values) {
        IVP_MI_Vector *result = malloc_mi_vector(size);
        if (!result)
            return nullptr;
        if (values) {
            for (int index = 0; index < size; ++index)
                result->element[index] = values[index];
        }
        return result;
    }

    void print() const {
        for (int index = 0; index < nr_of_elements; ++index)
            std::printf("v[%d]=%1.3e ", index, element[index]);
        std::printf("\n");
    }

    void set(const IVP_MI_Vector *value) {
        if (!value || nr_of_elements < value->nr_of_elements)
            return;
        nr_of_elements = value->nr_of_elements;
        weight_statistic = value->weight_statistic;
        for (int index = 0; index < nr_of_elements; ++index)
            element[index] = value->element[index];
    }

    void set(const int position, IVP_FLOAT value) {
        if (position >= 0 && position < nr_of_elements)
            element[position] = value;
    }

    void subtract(const IVP_MI_Vector *value) {
        if (!same_size(value))
            return;
        for (int index = 0; index < nr_of_elements; ++index)
            element[index] -= value->element[index];
    }

    void add(const IVP_MI_Vector *value) {
        if (!same_size(value))
            return;
        for (int index = 0; index < nr_of_elements; ++index)
            element[index] += value->element[index];
    }

    void add_multiple(
        const IVP_MI_Vector *value, const IVP_FLOAT factor) {
        if (!same_size(value))
            return;
        for (int index = 0; index < nr_of_elements; ++index)
            element[index] += value->element[index] * factor;
    }

    void mult(const IVP_FLOAT factor) {
        for (int index = 0; index < nr_of_elements; ++index)
            element[index] *= factor;
    }

    IVP_FLOAT length() const {
        IVP_FLOAT squaredLength = 0.0f;
        for (int index = 0; index < nr_of_elements; ++index)
            squaredLength += element[index] * element[index];
        return static_cast<IVP_FLOAT>(std::sqrt(squaredLength));
    }

    void set_time_stamp(IVP_Time currentTime) {
        time_stamp = currentTime;
    }

    IVP_FLOAT element[1];

private:
    bool same_size(const IVP_MI_Vector *value) const {
        return value && nr_of_elements == value->nr_of_elements;
    }
};

struct BML_IvpMultidimensionalInterpolatorLayoutCheck;

// Ballance imports the 0x40-byte debug-layout variant of this utility even
// though all of its out-of-line bodies were link-stripped.  It is used by the
// neighboring buoyancy controller to reuse force/torque solutions between PSI
// steps.  The implementation below retains that layout and the original
// interpolation/replacement policy while using physics_RT's allocator.
class IVP_Multidimensional_Interpolator {
    friend struct BML_IvpMultidimensionalInterpolatorLayoutCheck;

private:
    IVP_MI_Vector **previous_inputs;
    IVP_MI_Vector **previous_solutions;

    int nr_of_vectors : 8;
    int nr_of_elements_input : 8;
    int nr_of_elements_solution : 8;

    IVP_FLOAT MI_eps;
    IVP_FLOAT influence_of_old_weight;
    int nr_occupied;
    int scratch_area_index;
    int nr_of_vectors_involved;
    int counter_tries_nr_of_vectors_involved;
    int initial_value_vector_replacement;
    int counter_vector_replacement;

public:
    // These counters are present in the Ballance IDB's imported 0x40 layout.
    // Keeping them unconditional prevents Debug/Release ABI drift.
    int *nr_res_over_limit;
    int *nr_int_weight_over_limit;
    int *nr_of_linfit_failure;
    int *nr_of_success;
    int nr_one_vector_sufficient;

    IVP_Multidimensional_Interpolator(
        int vectorCount, int inputElementCount, int solutionElementCount)
        : previous_inputs(nullptr), previous_solutions(nullptr),
          nr_of_vectors(vectorCount), nr_of_elements_input(inputElementCount),
          nr_of_elements_solution(solutionElementCount), MI_eps(1.0e-14f),
          influence_of_old_weight(0.8f), nr_occupied(0),
          scratch_area_index(vectorCount), nr_of_vectors_involved(2),
          counter_tries_nr_of_vectors_involved(0),
          initial_value_vector_replacement(50),
          counter_vector_replacement(50), nr_res_over_limit(nullptr),
          nr_int_weight_over_limit(nullptr), nr_of_linfit_failure(nullptr),
          nr_of_success(nullptr), nr_one_vector_sufficient(0) {
        previous_inputs = allocate_array<IVP_MI_Vector *>(nr_of_vectors);
        previous_solutions =
            allocate_array<IVP_MI_Vector *>(nr_of_vectors + 1);
        for (int index = 0; index < nr_of_vectors; ++index)
            previous_inputs[index] =
                IVP_MI_Vector::malloc_mi_vector(nr_of_elements_input);
        for (int index = 0; index <= nr_of_vectors; ++index)
            previous_solutions[index] =
                IVP_MI_Vector::malloc_mi_vector(nr_of_elements_solution);

        nr_res_over_limit = allocate_array<int>(nr_of_vectors, true);
        nr_int_weight_over_limit = allocate_array<int>(nr_of_vectors, true);
        nr_of_linfit_failure = allocate_array<int>(nr_of_vectors, true);
        nr_of_success = allocate_array<int>(nr_of_vectors, true);
    }

    ~IVP_Multidimensional_Interpolator() {
        if (previous_inputs) {
            for (int index = 0; index < nr_of_vectors; ++index)
                release(previous_inputs[index]);
        }
        if (previous_solutions) {
            for (int index = 0; index <= nr_of_vectors; ++index)
                release(previous_solutions[index]);
        }
        release(previous_inputs);
        release(previous_solutions);
        release(nr_res_over_limit);
        release(nr_int_weight_over_limit);
        release(nr_of_linfit_failure);
        release(nr_of_success);
    }

    int get_nr_of_vectors() { return nr_of_vectors; }
    int get_nr_occupied() { return nr_occupied; }

    IVP_RETURN_TYPE check_interpolation(
        const IVP_MI_Vector *newInput,
        const int maxTriesNrOfVectorsInvolved,
        const IVP_FLOAT maxResidual, IVP_MI_Vector *output) {
        if (!newInput || !output || nr_occupied < 2 ||
            newInput->nr_of_elements != nr_of_elements_input ||
            output->nr_of_elements < nr_of_elements_solution)
            return IVP_FAULT;

        std::vector<IVP_FLOAT> inputDifference(nr_of_elements_input);
        IVP_FLOAT differenceLengthSquared = 0.0f;
        for (int row = 0; row < nr_of_elements_input; ++row) {
            inputDifference[row] =
                newInput->element[row] - previous_inputs[0]->element[row];
            differenceLengthSquared += inputDifference[row] * inputDifference[row];
        }

        if (counter_tries_nr_of_vectors_involved++ >=
            maxTriesNrOfVectorsInvolved) {
            nr_of_vectors_involved = 2;
            counter_tries_nr_of_vectors_involved = 0;
        }

        IVP_RETURN_TYPE success = IVP_FAULT;
        IVP_FLOAT firstWeight = 0.0f;
        std::vector<IVP_FLOAT> weights(nr_occupied, 0.0f);
        if (std::sqrt(differenceLengthSquared) < MI_eps) {
            nr_of_vectors_involved = 1;
            ++nr_one_vector_sufficient;
            firstWeight = 1.0f;
            success = IVP_OK;
        } else {
            if (nr_of_vectors_involved < 2 ||
                nr_of_vectors_involved > nr_occupied)
                nr_of_vectors_involved = 2;

            while (nr_of_vectors_involved <= nr_occupied) {
                const int columns = nr_of_vectors_involved - 1;
                std::vector<std::vector<IVP_FLOAT>> matrix(
                    columns + 1,
                    std::vector<IVP_FLOAT>(nr_of_elements_input, 0.0f));
                for (int column = 0; column < columns; ++column) {
                    for (int row = 0; row < nr_of_elements_input; ++row) {
                        matrix[column][row] =
                            previous_inputs[column + 1]->element[row] -
                            previous_inputs[0]->element[row];
                    }
                }
                matrix[columns] = inputDifference;

                IVP_FLOAT residual = 0.0f;
                success = linfit(matrix, columns, weights.data(), residual);
                firstWeight = 1.0f;
                if (success == IVP_OK) {
                    if (residual > maxResidual) {
                        success = IVP_FAULT;
                        ++nr_res_over_limit[nr_of_vectors_involved - 1];
                    } else {
                        for (int index = 0; index < columns; ++index) {
                            if (std::fabs(weights[index] - 0.5f) > 0.7f) {
                                success = IVP_FAULT;
                                ++nr_int_weight_over_limit[
                                    nr_of_vectors_involved - 1];
                                break;
                            }
                            firstWeight -= weights[index];
                        }
                        if (success == IVP_OK &&
                            std::fabs(firstWeight - 0.5f) > 0.7f) {
                            success = IVP_FAULT;
                            ++nr_int_weight_over_limit[
                                nr_of_vectors_involved - 1];
                        }
                    }
                } else {
                    ++nr_of_linfit_failure[nr_of_vectors_involved - 1];
                }
                if (success == IVP_OK) {
                    ++nr_of_success[nr_of_vectors_involved - 1];
                    break;
                }
                ++nr_of_vectors_involved;
            }
        }

        if (success == IVP_FAULT)
            return IVP_FAULT;

        IVP_MI_Vector *scratch = previous_solutions[scratch_area_index];
        for (int element = 0; element < nr_of_elements_solution; ++element)
            scratch->element[element] =
                firstWeight * previous_solutions[0]->element[element];
        previous_inputs[0]->weight_statistic =
            previous_inputs[0]->weight_statistic * influence_of_old_weight +
            firstWeight;
        for (int index = 1; index < nr_of_vectors_involved; ++index) {
            previous_inputs[index]->weight_statistic =
                previous_inputs[index]->weight_statistic *
                    influence_of_old_weight +
                weights[index - 1];
            for (int element = 0; element < nr_of_elements_solution; ++element)
                scratch->element[element] +=
                    weights[index - 1] *
                    previous_solutions[index]->element[element];
        }
        output->set(scratch);
        sort_vectors(nr_occupied);
        return IVP_OK;
    }

    void add_new_input_solution_combination_conventional(
        const IVP_MI_Vector *newInput, const IVP_MI_Vector *newSolution) {
        IVP_MI_Vector *input = previous_inputs[nr_of_vectors - 1];
        IVP_MI_Vector *solution = previous_solutions[nr_of_vectors - 1];
        for (int index = nr_of_vectors - 2; index >= 0; --index) {
            previous_inputs[index + 1] = previous_inputs[index];
            previous_solutions[index + 1] = previous_solutions[index];
        }
        previous_inputs[0] = input;
        previous_solutions[0] = solution;
        input->set(newInput);
        solution->set(newSolution);
        input->set_time_stamp(newInput->time_stamp);
        solution->set_time_stamp(newSolution->time_stamp);
        input->weight_statistic = 1.0f / (1.0f - influence_of_old_weight);
        if (nr_occupied < nr_of_vectors)
            ++nr_occupied;
    }

    void add_new_input_solution_combination_stochastic(
        const IVP_MI_Vector *newInput, const IVP_MI_Vector *newSolution) {
        const int slot = (counter_vector_replacement-- * 101) % nr_of_vectors;
        previous_inputs[slot]->set(newInput);
        previous_solutions[slot]->set(newSolution);
        previous_inputs[slot]->set_time_stamp(newInput->time_stamp);
        previous_solutions[slot]->set_time_stamp(newSolution->time_stamp);
        previous_inputs[slot]->weight_statistic =
            1.0f / (1.0f - influence_of_old_weight);
        sort_vectors(nr_occupied);
        if (counter_vector_replacement < 0)
            counter_vector_replacement = initial_value_vector_replacement;
    }

private:
    template <class T>
    static T *allocate_array(int count, bool zeroed = false) {
        if (count <= 0)
            return nullptr;
        const unsigned int bytes =
            static_cast<unsigned int>(sizeof(T) * count);
        if (zeroed) {
            return static_cast<T *>(BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::AllocateZeroed, count,
                static_cast<int>(sizeof(T))));
        }
        return static_cast<T *>(BML::IVP::ABI::Invoke<void *>(
            BML::IVP::ABI::Address::Allocate, bytes));
    }

    template <class T>
    static void release(T *&memory) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::Free, memory);
        memory = nullptr;
    }

    void sort_vectors(int count) {
        for (int index = 1; index < count; ++index) {
            int position = index;
            while (position > 0 &&
                   previous_inputs[position - 1]->weight_statistic <
                       previous_inputs[position]->weight_statistic) {
                IVP_MI_Vector *input = previous_inputs[position];
                previous_inputs[position] = previous_inputs[position - 1];
                previous_inputs[position - 1] = input;
                IVP_MI_Vector *solution = previous_solutions[position];
                previous_solutions[position] = previous_solutions[position - 1];
                previous_solutions[position - 1] = solution;
                --position;
            }
        }
    }

    IVP_RETURN_TYPE linfit(
        std::vector<std::vector<IVP_FLOAT>> &matrix, int columns,
        IVP_FLOAT *weights, IVP_FLOAT &residual) const {
        IVP_FLOAT cancellation = 0.0f;
        for (int column = 0; column < columns; ++column) {
            IVP_FLOAT squared = 0.0f;
            for (int row = 0; row < nr_of_elements_input; ++row)
                squared += matrix[column][row] * matrix[column][row];
            if (squared > cancellation)
                cancellation = squared;
        }
        cancellation = std::sqrt(cancellation) * MI_eps;

        for (int pivot = 0; pivot < columns; ++pivot) {
            for (int row = pivot + 1; row < nr_of_elements_input; ++row) {
                if (std::fabs(matrix[pivot][row]) <= MI_eps)
                    continue;
                const IVP_FLOAT q = std::sqrt(
                    matrix[pivot][row] * matrix[pivot][row] +
                    matrix[pivot][pivot] * matrix[pivot][pivot]);
                if (q == 0.0f)
                    return IVP_FAULT;
                const IVP_FLOAT cosine = matrix[pivot][pivot] / q;
                const IVP_FLOAT sine = -matrix[pivot][row] / q;
                matrix[pivot][pivot] = q;
                for (int column = pivot + 1; column <= columns; ++column) {
                    const IVP_FLOAT value = sine * matrix[column][pivot];
                    matrix[column][pivot] =
                        cosine * matrix[column][pivot] -
                        sine * matrix[column][row];
                    matrix[column][row] =
                        cosine * matrix[column][row] + value;
                }
            }
            if (std::fabs(matrix[pivot][pivot]) <= cancellation)
                return IVP_FAULT;
        }

        weights[columns - 1] =
            matrix[columns][columns - 1] /
            matrix[columns - 1][columns - 1];
        for (int row = columns - 2; row >= 0; --row) {
            weights[row] = matrix[columns][row] -
                weights[columns - 1] * matrix[columns - 1][row];
            for (int column = row + 1; column <= columns - 2; ++column)
                weights[row] -= matrix[column][row] * weights[column];
            weights[row] /= matrix[row][row];
        }

        IVP_FLOAT residualSquared = 0.0f;
        for (int row = columns; row < nr_of_elements_input; ++row)
            residualSquared += matrix[columns][row] * matrix[columns][row];
        residual = std::sqrt(residualSquared);
        return IVP_OK;
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_MI_Vector_Base) == 0x10);
static_assert(sizeof(IVP_MI_Vector) == 0x18);
static_assert(offsetof(IVP_MI_Vector, element) == 0x10);
static_assert(sizeof(IVP_Multidimensional_Interpolator) == 0x40);

struct BML_IvpMultidimensionalInterpolatorLayoutCheck {
    static_assert(
        offsetof(IVP_Multidimensional_Interpolator, previous_inputs) == 0x00);
    static_assert(offsetof(IVP_Multidimensional_Interpolator,
                           previous_solutions) == 0x04);
    // The three signed 8-bit dimensions occupy +0x08..+0x0A. C++ forbids
    // offsetof on a bit-field, so the following member pins their storage end.
    static_assert(offsetof(IVP_Multidimensional_Interpolator, MI_eps) == 0x0C);
    static_assert(offsetof(IVP_Multidimensional_Interpolator,
                           influence_of_old_weight) == 0x10);
    static_assert(
        offsetof(IVP_Multidimensional_Interpolator, nr_occupied) == 0x14);
    static_assert(offsetof(IVP_Multidimensional_Interpolator,
                           counter_vector_replacement) == 0x28);
    static_assert(offsetof(IVP_Multidimensional_Interpolator,
                           nr_res_over_limit) == 0x2C);
    static_assert(offsetof(IVP_Multidimensional_Interpolator,
                           nr_of_success) == 0x38);
    static_assert(offsetof(IVP_Multidimensional_Interpolator,
                           nr_one_vector_sufficient) == 0x3C);
};
#endif

#endif // BML_IVP_INTERPOLATION_H
