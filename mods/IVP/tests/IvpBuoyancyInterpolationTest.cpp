#include "IvpTestAdapter.h"

#include "BML/IVP/Interpolation.h"

#include <gtest/gtest.h>

#include <cstdlib>

namespace {

void *Allocate(unsigned int size) { return std::malloc(size); }
void *AllocateZeroed(int count, int size) {
    return std::calloc(static_cast<std::size_t>(count),
                       static_cast<std::size_t>(size));
}
void Free(void *memory) { std::free(memory); }

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::Allocate, &Allocate),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::AllocateZeroed, &AllocateZeroed),
    BML::IVP::Test::Bind(BML::IVP::ABI::Address::Free, &Free),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}

class OwnedVector {
public:
    OwnedVector(int size, IVP_FLOAT first, IVP_FLOAT second)
        : value(IVP_MI_Vector::malloc_mi_vector(size)) {
        EXPECT_NE(value, nullptr);
        value->set(0, first);
        value->set(1, second);
    }

    ~OwnedVector() {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::Free, value);
    }

    IVP_MI_Vector *get() const { return value; }

private:
    IVP_MI_Vector *value;
};

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpBuoyancyInterpolation, ReusesAffineFluidImpulseSolutions) {
    // Model the two inputs used by a buoyancy controller as submerged fraction
    // and downward speed.  The two solution values are lift and damping
    // impulses.  Three prior solver runs determine an affine response plane.
    IVP_Multidimensional_Interpolator interpolator(4, 2, 2);
    OwnedVector dryStillInput(2, 0.0f, 0.0f);
    OwnedVector dryStillSolution(2, 0.0f, 0.0f);
    OwnedVector submergedStillInput(2, 1.0f, 0.0f);
    OwnedVector submergedStillSolution(2, 10.0f, 2.0f);
    OwnedVector dryFallingInput(2, 0.0f, 1.0f);
    OwnedVector dryFallingSolution(2, -3.0f, -4.0f);

    interpolator.add_new_input_solution_combination_conventional(
        dryStillInput.get(), dryStillSolution.get());
    interpolator.add_new_input_solution_combination_conventional(
        submergedStillInput.get(), submergedStillSolution.get());
    interpolator.add_new_input_solution_combination_conventional(
        dryFallingInput.get(), dryFallingSolution.get());

    EXPECT_EQ(interpolator.get_nr_of_vectors(), 4);
    EXPECT_EQ(interpolator.get_nr_occupied(), 3);

    OwnedVector currentInput(2, 0.25f, 0.5f);
    OwnedVector interpolatedImpulse(2, 0.0f, 0.0f);
    EXPECT_EQ(interpolator.check_interpolation(
                  currentInput.get(), 15, 1.0e-4f,
                  interpolatedImpulse.get()),
              IVP_OK);
    EXPECT_NEAR(interpolatedImpulse.get()->element[0], 1.0f, 1.0e-5f);
    EXPECT_NEAR(interpolatedImpulse.get()->element[1], -1.5f, 1.0e-5f);
    EXPECT_EQ(interpolator.nr_of_success[2], 1);

    // An input far beyond the sampled fluid states would require an unsafe
    // extrapolation weight and must fall back to a real buoyancy solve.
    OwnedVector unsafeInput(2, 3.0f, 3.0f);
    EXPECT_EQ(interpolator.check_interpolation(
                  unsafeInput.get(), 15, 1.0e-4f,
                  interpolatedImpulse.get()),
              IVP_FAULT);
    EXPECT_GT(interpolator.nr_int_weight_over_limit[1] +
                  interpolator.nr_int_weight_over_limit[2],
              0);
}

TEST(IvpBuoyancyInterpolation, ReplacesAStaleSolverSample) {
    IVP_Multidimensional_Interpolator interpolator(3, 2, 2);
    OwnedVector input0(2, 0.0f, 0.0f);
    OwnedVector solution0(2, 0.0f, 0.0f);
    OwnedVector input1(2, 1.0f, 0.0f);
    OwnedVector solution1(2, 10.0f, 2.0f);
    OwnedVector input2(2, 0.0f, 1.0f);
    OwnedVector solution2(2, -3.0f, -4.0f);
    interpolator.add_new_input_solution_combination_conventional(
        input0.get(), solution0.get());
    interpolator.add_new_input_solution_combination_conventional(
        input1.get(), solution1.get());
    interpolator.add_new_input_solution_combination_conventional(
        input2.get(), solution2.get());

    OwnedVector output(2, 0.0f, 0.0f);
    EXPECT_EQ(interpolator.check_interpolation(
                  input2.get(), 15, 1.0e-4f, output.get()),
              IVP_OK);
    EXPECT_FLOAT_EQ(output.get()->element[0], -3.0f);
    EXPECT_FLOAT_EQ(output.get()->element[1], -4.0f);
    EXPECT_EQ(interpolator.nr_one_vector_sufficient, 1);

    OwnedVector combinedInput(2, 1.0f, 1.0f);
    // Deliberately depart from the old affine response so this assertion can
    // only pass if the stochastic replacement is actually consumed.
    OwnedVector combinedSolution(2, 70.0f, -20.0f);
    interpolator.add_new_input_solution_combination_stochastic(
        combinedInput.get(), combinedSolution.get());

    EXPECT_EQ(interpolator.check_interpolation(
                  combinedInput.get(), 15, 1.0e-4f, output.get()),
              IVP_OK);
    EXPECT_FLOAT_EQ(output.get()->element[0], 70.0f);
    EXPECT_FLOAT_EQ(output.get()->element[1], -20.0f);
    EXPECT_EQ(interpolator.nr_one_vector_sufficient, 1);
}

} // namespace
