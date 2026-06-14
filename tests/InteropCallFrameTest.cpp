#include "CallFrameInternal.h"

#include <gtest/gtest.h>

TEST(InteropCallFrameTest, TracksArgumentTypesAndTrimsClearedTailSlots) {
    BML_CallFrame frame;

    BML_CallValue *first = BML_EnsureCallFrameArg(&frame, 0);
    ASSERT_NE(first, nullptr);
    first->Type = BML_CallValueType::Int;
    first->IntValue = 42;

    BML_CallValue *third = BML_EnsureCallFrameArg(&frame, 2);
    ASSERT_NE(third, nullptr);
    third->Type = BML_CallValueType::String;
    third->StringValue = "tail";

    EXPECT_EQ(BML_GetCallFrameArgCount(&frame), 3u);
    EXPECT_EQ(BML_GetCallFrameArgType(&frame, 0), BML_CallValueType::Int);
    EXPECT_EQ(BML_GetCallFrameArgType(&frame, 1), BML_CallValueType::Empty);
    EXPECT_EQ(BML_GetCallFrameArgType(&frame, 2), BML_CallValueType::String);

    EXPECT_EQ(BML_ClearCallFrameArg(&frame, 2), BML_OK);
    EXPECT_EQ(BML_GetCallFrameArgCount(&frame), 1u);
    EXPECT_EQ(BML_GetCallFrameArgType(&frame, 2), BML_CallValueType::Empty);

    EXPECT_EQ(BML_ClearCallFrameArg(&frame, 2), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(BML_ClearCallFrameArg(nullptr, 0), BML_ERROR_NOT_FOUND);
}

TEST(InteropCallFrameTest, DistinguishesMissingAndMismatchedArgumentTypes) {
    BML_CallFrame frame;

    BML_CallValue *first = BML_EnsureCallFrameArg(&frame, 0);
    ASSERT_NE(first, nullptr);
    first->Type = BML_CallValueType::Int;
    first->IntValue = 7;

    const BML_CallValue *value = nullptr;
    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 0, BML_CallValueType::Int, &value), BML_OK);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->IntValue, 7);

    value = nullptr;
    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 0, BML_CallValueType::Bool, &value),
              BML_ERROR_INTEROP_TYPE_MISMATCH);
    EXPECT_EQ(value, nullptr);

    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 1, BML_CallValueType::Int, &value),
              BML_ERROR_NOT_FOUND);
    EXPECT_EQ(BML_GetCallFrameArgChecked(nullptr, 0, BML_CallValueType::Int, &value),
              BML_ERROR_INTEROP_BAD_CALL_FRAME);
}

TEST(InteropCallFrameTest, TracksAndClearsResultType) {
    BML_CallFrame frame;
    EXPECT_EQ(BML_GetCallFrameResultType(&frame), BML_CallValueType::Empty);

    frame.Result.Type = BML_CallValueType::Bool;
    frame.Result.IntValue = 1;
    EXPECT_EQ(BML_GetCallFrameResultType(&frame), BML_CallValueType::Bool);
    EXPECT_EQ(BML_ClearCallFrameResult(&frame), BML_OK);
    EXPECT_EQ(BML_GetCallFrameResultType(&frame), BML_CallValueType::Empty);
    EXPECT_EQ(BML_ClearCallFrameResult(&frame), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(BML_ClearCallFrameResult(nullptr), BML_ERROR_INVALID_PARAMETER);
}
