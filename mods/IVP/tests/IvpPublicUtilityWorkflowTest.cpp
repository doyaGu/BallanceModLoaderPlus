#include "IvpTestAdapter.h"

#include "BML/IVP/Types.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

char *DuplicateString(const char *source) {
    if (!source)
        return nullptr;
    const std::size_t bytes = std::strlen(source) + 1;
    auto *result = static_cast<char *>(std::malloc(bytes));
    if (result)
        std::memcpy(result, source, bytes);
    return result;
}

void FreeRetailAllocation(void *memory) {
    std::free(memory);
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::DuplicateString, &DuplicateString),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorDelete, &FreeRetailAllocation),
};

struct ListNode {
    ListNode *next = nullptr;
    ListNode *prev = nullptr;
    int value = 0;
};

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}

TEST(IvpPublicUtilityWorkflow,
     ParsesBallanceStyleParametersAndOwnsFormattedDiagnostics) {
    FILE *parameters = std::tmpfile();
    ASSERT_NE(parameters, nullptr);
    std::fputs("# controller tuning\n", parameters);
    std::fputs("BallSpeedGovernor, 12.5; 42\r\n", parameters);
    std::rewind(parameters);

    EXPECT_STREQ(p_read_first_token(parameters), "BallSpeedGovernor");
    EXPECT_DOUBLE_EQ(p_get_float(), 12.5);
    EXPECT_EQ(p_get_num(), 42);
    EXPECT_EQ(p_get_next_token(), nullptr);
    std::fclose(parameters);

    char description[] = "surface:wooden ramp with grip\n";
    EXPECT_STREQ(p_str_tok(description, ":"), "surface");
    char *remainder = p_get_string();
    ASSERT_NE(remainder, nullptr);
    EXPECT_STREQ(remainder, "wooden ramp with grip");
    p_free(remainder);

    char *status = p_make_string(
        "%s speed %.2f, material %d", "Ball", 12.5, 42);
    ASSERT_NE(status, nullptr);
    EXPECT_STREQ(status, "Ball speed 12.50, material 42");
    p_free(status);

    EXPECT_STREQ(
        p_export_error("invalid friction %.2f", -0.25),
        "ERROR: invalid friction -0.25");
    EXPECT_STREQ(p_get_error(), "ERROR: invalid friction -0.25");

    ushort shortValue = 0x1234u;
    uint longValue = 0x12345678u;
    ivp_byte_swap2(shortValue);
    ivp_byte_swap4(longValue);
    EXPECT_EQ(shortValue, 0x3412u);
    EXPECT_EQ(longValue, 0x78563412u);

    P_List<ListNode> activeControllers;
    ListNode speedController{nullptr, nullptr, 1};
    ListNode surfaceController{nullptr, nullptr, 2};
    activeControllers.insert(&speedController);
    activeControllers.insert(&surfaceController);
    ASSERT_EQ(activeControllers.len, 2);
    EXPECT_EQ(activeControllers.first, &surfaceController);
    EXPECT_EQ(surfaceController.next, &speedController);
    EXPECT_EQ(speedController.prev, &surfaceController);
    activeControllers.remove(&surfaceController);
    EXPECT_EQ(activeControllers.first, &speedController);
    EXPECT_EQ(speedController.prev, nullptr);
    EXPECT_EQ(activeControllers.len, 1);

    char materialName[] = "wooden ramp";
    P_String::uppercase(materialName);
    EXPECT_STREQ(materialName, "WOODEN RAMP");
    EXPECT_STREQ(
        P_String::find_string(
            "Ballance wooden ramp", "WOOD??", 3),
        "wooden ramp");
    EXPECT_EQ(
        P_String::string_cmp(
            "BallWoodRamp", "ball*R?mp", IVP_TRUE),
        0);
}
