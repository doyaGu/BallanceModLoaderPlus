#ifndef BML_CALLFRAMEINTERNAL_H
#define BML_CALLFRAMEINTERNAL_H

#include "BML/Interop.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class BML_CallValueType {
    Empty = BML_CALL_VALUE_EMPTY,
    Bool = BML_CALL_VALUE_BOOL,
    Int = BML_CALL_VALUE_INT,
    Float = BML_CALL_VALUE_FLOAT,
    String = BML_CALL_VALUE_STRING,
    BoolArray = BML_CALL_VALUE_BOOL_ARRAY,
    IntArray = BML_CALL_VALUE_INT_ARRAY,
    FloatArray = BML_CALL_VALUE_FLOAT_ARRAY,
    StringArray = BML_CALL_VALUE_STRING_ARRAY,
    Buffer = BML_CALL_VALUE_BUFFER,
    ObjectId = BML_CALL_VALUE_OBJECT_ID,
};

struct BML_CallValue {
    BML_CallValueType Type = BML_CallValueType::Empty;
    int IntValue = 0;
    float FloatValue = 0.0f;
    std::string StringValue;
    std::vector<int> IntArrayValue;
    std::vector<float> FloatArrayValue;
    std::vector<std::string> StringArrayValue;
    std::vector<std::uint8_t> BufferValue;
};

struct BML_CallFrame {
    static const size_t InlineArgCount = 4;

    std::array<BML_CallValue, InlineArgCount> InlineArgs;
    std::vector<BML_CallValue> ExtraArgs;
    size_t ArgCount = 0;
    BML_CallValue Result;
};

BML_CallValue *BML_EnsureCallFrameArg(BML_CallFrame *frame, size_t index);
const BML_CallValue *BML_GetCallFrameArg(const BML_CallFrame *frame, size_t index, BML_CallValueType type);
int BML_GetCallFrameArgChecked(const BML_CallFrame *frame,
                               size_t index,
                               BML_CallValueType type,
                               const BML_CallValue **outValue);
size_t BML_GetCallFrameArgCount(const BML_CallFrame *frame);
BML_CallValueType BML_GetCallFrameArgType(const BML_CallFrame *frame, size_t index);
int BML_ClearCallFrameArg(BML_CallFrame *frame, size_t index);
BML_CallValueType BML_GetCallFrameResultType(const BML_CallFrame *frame);
int BML_GetCallFrameResultChecked(const BML_CallFrame *frame,
                                  BML_CallValueType type,
                                  const BML_CallValue **outValue);
int BML_ClearCallFrameResult(BML_CallFrame *frame);
const char *BML_BorrowCallFrameString(const BML_CallFrame *frame, size_t index);
const char *BML_BorrowCallFrameResultString(const BML_CallFrame *frame);
void BML_ResetCallValue(BML_CallValue &value);
void BML_ClearCallFrame(BML_CallFrame *frame);

#endif
