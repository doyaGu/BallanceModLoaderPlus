#include "CallFrameInternal.h"

#include <algorithm>

static BML_CallValue *GetCallFrameArgSlot(BML_CallFrame *frame, size_t index) {
    if (!frame || index >= frame->ArgCount)
        return nullptr;

    if (index < BML_CallFrame::InlineArgCount)
        return &frame->InlineArgs[index];

    const size_t extraIndex = index - BML_CallFrame::InlineArgCount;
    return extraIndex < frame->ExtraArgs.size() ? &frame->ExtraArgs[extraIndex] : nullptr;
}

static const BML_CallValue *GetCallFrameArgSlot(const BML_CallFrame *frame, size_t index) {
    return GetCallFrameArgSlot(const_cast<BML_CallFrame *>(frame), index);
}

static void TrimCallFrameArgCount(BML_CallFrame *frame) {
    if (!frame)
        return;

    while (frame->ArgCount > 0) {
        const BML_CallValue *slot = GetCallFrameArgSlot(frame, frame->ArgCount - 1);
        if (!slot || slot->Type != BML_CallValueType::Empty)
            break;
        --frame->ArgCount;
    }

    const size_t requiredExtra = frame->ArgCount > BML_CallFrame::InlineArgCount
                                     ? frame->ArgCount - BML_CallFrame::InlineArgCount
                                     : 0;
    if (frame->ExtraArgs.size() > requiredExtra)
        frame->ExtraArgs.resize(requiredExtra);
}

BML_CallValue *BML_EnsureCallFrameArg(BML_CallFrame *frame, size_t index) {
    if (!frame)
        return nullptr;

    const size_t newArgCount = std::max(frame->ArgCount, index + 1);
    if (index < BML_CallFrame::InlineArgCount) {
        frame->ArgCount = newArgCount;
        return &frame->InlineArgs[index];
    }

    const size_t extraIndex = index - BML_CallFrame::InlineArgCount;
    if (frame->ExtraArgs.size() <= extraIndex)
        frame->ExtraArgs.resize(extraIndex + 1);
    frame->ArgCount = newArgCount;
    return &frame->ExtraArgs[extraIndex];
}

const BML_CallValue *BML_GetCallFrameArg(const BML_CallFrame *frame, size_t index, BML_CallValueType type) {
    const BML_CallValue *slot = nullptr;
    return BML_GetCallFrameArgChecked(frame, index, type, &slot) == BML_OK ? slot : nullptr;
}

int BML_GetCallFrameArgChecked(const BML_CallFrame *frame,
                               size_t index,
                               BML_CallValueType type,
                               const BML_CallValue **outValue) {
    if (outValue)
        *outValue = nullptr;
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    if (index >= frame->ArgCount)
        return BML_ERROR_NOT_FOUND;

    const BML_CallValue *slot = GetCallFrameArgSlot(frame, index);
    if (!slot || slot->Type == BML_CallValueType::Empty)
        return BML_ERROR_NOT_FOUND;
    if (slot->Type != type)
        return BML_ERROR_INTEROP_TYPE_MISMATCH;

    if (outValue)
        *outValue = slot;
    return BML_OK;
}

size_t BML_GetCallFrameArgCount(const BML_CallFrame *frame) {
    return frame ? frame->ArgCount : 0;
}

BML_CallValueType BML_GetCallFrameArgType(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = GetCallFrameArgSlot(frame, index);
    return slot ? slot->Type : BML_CallValueType::Empty;
}

int BML_ClearCallFrameArg(BML_CallFrame *frame, size_t index) {
    BML_CallValue *slot = GetCallFrameArgSlot(frame, index);
    if (!slot)
        return BML_ERROR_NOT_FOUND;
    if (slot->Type == BML_CallValueType::Empty)
        return BML_ERROR_NOT_FOUND;

    BML_ResetCallValue(*slot);
    TrimCallFrameArgCount(frame);
    return BML_OK;
}

BML_CallValueType BML_GetCallFrameResultType(const BML_CallFrame *frame) {
    return frame ? frame->Result.Type : BML_CallValueType::Empty;
}

int BML_GetCallFrameResultChecked(const BML_CallFrame *frame,
                                  BML_CallValueType type,
                                  const BML_CallValue **outValue) {
    if (outValue)
        *outValue = nullptr;
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    if (frame->Result.Type == BML_CallValueType::Empty)
        return BML_ERROR_NOT_FOUND;
    if (frame->Result.Type != type)
        return BML_ERROR_INTEROP_TYPE_MISMATCH;

    if (outValue)
        *outValue = &frame->Result;
    return BML_OK;
}

int BML_ClearCallFrameResult(BML_CallFrame *frame) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;

    if (frame->Result.Type == BML_CallValueType::Empty)
        return BML_ERROR_NOT_FOUND;

    BML_ResetCallValue(frame->Result);
    return BML_OK;
}

const char *BML_BorrowCallFrameString(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = BML_GetCallFrameArg(frame, index, BML_CallValueType::String);
    return slot ? slot->StringValue.c_str() : nullptr;
}

const char *BML_BorrowCallFrameResultString(const BML_CallFrame *frame) {
    if (!frame || frame->Result.Type != BML_CallValueType::String)
        return nullptr;
    return frame->Result.StringValue.c_str();
}

void BML_ResetCallValue(BML_CallValue &value) {
    value.Type = BML_CallValueType::Empty;
    value.IntValue = 0;
    value.FloatValue = 0.0f;
    value.StringValue.clear();
    value.IntArrayValue.clear();
    value.FloatArrayValue.clear();
    value.StringArrayValue.clear();
    value.StringArrayPointerCache.clear();
    value.BufferValue.clear();
}

void BML_ClearCallFrame(BML_CallFrame *frame) {
    if (!frame)
        return;

    for (size_t i = 0; i < BML_CallFrame::InlineArgCount; ++i)
        BML_ResetCallValue(frame->InlineArgs[i]);
    frame->ExtraArgs.clear();
    frame->ArgCount = 0;
    BML_ResetCallValue(frame->Result);
}
