#include "CallFrameInternal.h"

#include <cstdint>
#include <string>
#include <vector>

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

TEST(InteropCallFrameTest, StoresExtendedArgumentValues) {
    BML_CallFrame frame;

    BML_CallValue *boolArray = BML_EnsureCallFrameArg(&frame, 0);
    ASSERT_NE(boolArray, nullptr);
    boolArray->Type = BML_CallValueType::BoolArray;
    boolArray->IntArrayValue = {1, 0, 1};

    BML_CallValue *floatArray = BML_EnsureCallFrameArg(&frame, 1);
    ASSERT_NE(floatArray, nullptr);
    floatArray->Type = BML_CallValueType::FloatArray;
    floatArray->FloatArrayValue = {1.5f, 2.5f};

    BML_CallValue *stringArray = BML_EnsureCallFrameArg(&frame, 2);
    ASSERT_NE(stringArray, nullptr);
    stringArray->Type = BML_CallValueType::StringArray;
    stringArray->StringArrayValue = {"left", "right"};

    BML_CallValue *buffer = BML_EnsureCallFrameArg(&frame, 4);
    ASSERT_NE(buffer, nullptr);
    buffer->Type = BML_CallValueType::Buffer;
    buffer->BufferValue = {1, 2, 3};

    BML_CallValue *objectId = BML_EnsureCallFrameArg(&frame, 5);
    ASSERT_NE(objectId, nullptr);
    objectId->Type = BML_CallValueType::ObjectId;
    objectId->IntValue = 123;

    const BML_CallValue *value = nullptr;
    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 0, BML_CallValueType::BoolArray, &value), BML_OK);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->IntArrayValue, std::vector<int>({1, 0, 1}));

    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 1, BML_CallValueType::FloatArray, &value), BML_OK);
    ASSERT_NE(value, nullptr);
    ASSERT_EQ(value->FloatArrayValue.size(), 2u);
    EXPECT_FLOAT_EQ(value->FloatArrayValue[0], 1.5f);
    EXPECT_FLOAT_EQ(value->FloatArrayValue[1], 2.5f);

    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 2, BML_CallValueType::StringArray, &value), BML_OK);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->StringArrayValue, std::vector<std::string>({"left", "right"}));

    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 4, BML_CallValueType::Buffer, &value), BML_OK);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->BufferValue, std::vector<std::uint8_t>({1, 2, 3}));

    EXPECT_EQ(BML_GetCallFrameArgChecked(&frame, 5, BML_CallValueType::ObjectId, &value), BML_OK);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->IntValue, 123);
}

TEST(InteropCallFrameTest, DistinguishesMissingAndMismatchedResultTypes) {
    BML_CallFrame frame;

    const BML_CallValue *value = nullptr;
    EXPECT_EQ(BML_GetCallFrameResultChecked(&frame, BML_CallValueType::IntArray, &value),
              BML_ERROR_NOT_FOUND);
    EXPECT_EQ(value, nullptr);

    frame.Result.Type = BML_CallValueType::IntArray;
    frame.Result.IntArrayValue = {2, 4, 6};
    EXPECT_EQ(BML_GetCallFrameResultChecked(&frame, BML_CallValueType::IntArray, &value), BML_OK);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->IntArrayValue, std::vector<int>({2, 4, 6}));

    value = nullptr;
    EXPECT_EQ(BML_GetCallFrameResultChecked(&frame, BML_CallValueType::Buffer, &value),
              BML_ERROR_INTEROP_TYPE_MISMATCH);
    EXPECT_EQ(value, nullptr);

    EXPECT_EQ(BML_GetCallFrameResultChecked(nullptr, BML_CallValueType::IntArray, &value),
              BML_ERROR_INTEROP_BAD_CALL_FRAME);
}
