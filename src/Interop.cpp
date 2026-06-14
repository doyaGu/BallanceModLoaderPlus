#include "BML/Interop.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "BML/IMod.h"
#include "CallFrameInternal.h"
#include "ExportRegistry.h"
#include "ModContext.h"
#include "StringUtils.h"

#if BML_ENABLE_ANGELSCRIPT
#include "ScriptMod.h"
#endif

static bool IsValidKeyPart(const char *value) {
    return value && value[0] != '\0';
}

static int CopyStringOut(const std::string &value, char *buffer, size_t bufferSize, size_t *outRequiredSize) {
    return utils::CopyStringToBuffer(value, buffer, bufferSize, outRequiredSize)
               ? BML_OK
               : BML_ERROR_INVALID_PARAMETER;
}

static int BorrowStringOut(const std::string &value, const char **outValue, size_t *outSize) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = value.c_str();
    if (outSize)
        *outSize = value.size();
    return BML_OK;
}

template <typename Function>
static int GuardInteropMutation(Function function) {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

static BML_CALL_VALUE_TYPE ToPublicCallValueType(BML_CallValueType type) {
    return static_cast<BML_CALL_VALUE_TYPE>(type);
}

template <typename T>
static int CopyVectorOut(const std::vector<T> &values, T *buffer, size_t bufferCount, size_t *outRequiredCount) {
    if (outRequiredCount)
        *outRequiredCount = values.size();
    if (!buffer || bufferCount == 0)
        return BML_OK;
    if (bufferCount < values.size())
        return BML_ERROR_INVALID_PARAMETER;
    if (!values.empty())
        std::copy(values.begin(), values.end(), buffer);
    return BML_OK;
}

template <typename T>
static int BorrowVectorOut(const std::vector<T> &values, const T **outValues, size_t *outCount) {
    if (!outValues)
        return BML_ERROR_INVALID_PARAMETER;
    *outValues = values.empty() ? nullptr : values.data();
    if (outCount)
        *outCount = values.size();
    return BML_OK;
}

static int CopyBufferOut(const std::vector<std::uint8_t> &values,
                         std::uint8_t *buffer,
                         size_t bufferSize,
                         size_t *outRequiredSize) {
    if (outRequiredSize)
        *outRequiredSize = values.size();
    if (!buffer || bufferSize == 0)
        return BML_OK;
    if (bufferSize < values.size())
        return BML_ERROR_INVALID_PARAMETER;
    if (!values.empty())
        std::memcpy(buffer, values.data(), values.size());
    return BML_OK;
}

static int BorrowBufferOut(const std::vector<std::uint8_t> &values,
                           const std::uint8_t **outData,
                           size_t *outSize) {
    if (!outData)
        return BML_ERROR_INVALID_PARAMETER;
    *outData = values.empty() ? nullptr : values.data();
    if (outSize)
        *outSize = values.size();
    return BML_OK;
}

template <typename Fill>
static int SetArgValue(BML_CallFrame *frame, size_t index, Fill fill) {
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        fill(*slot);
        return BML_OK;
    });
}

template <typename Fill>
static int SetResultValue(BML_CallFrame *frame, Fill fill) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        fill(frame->Result);
        return BML_OK;
    });
}

static BML_MOD_KIND GetModKind(const IMod *mod) {
    if (!mod)
        return BML_MOD_KIND_UNKNOWN;
#if BML_ENABLE_ANGELSCRIPT
    if (dynamic_cast<const BML::ScriptMod *>(mod))
        return BML_MOD_KIND_SCRIPT;
#endif
    return BML_MOD_KIND_NATIVE;
}

static BML_MOD_STATE GetModState(const IMod *mod) {
    if (!mod)
        return BML_MOD_STATE_NOT_FOUND;
#if BML_ENABLE_ANGELSCRIPT
    if (auto *scriptMod = dynamic_cast<const BML::ScriptMod *>(mod)) {
        if (scriptMod->IsFailed())
            return BML_MOD_STATE_FAILED;
        return scriptMod->IsLoaded() ? BML_MOD_STATE_LOADED : BML_MOD_STATE_REGISTERED;
    }
#endif
    return BML_MOD_STATE_LOADED;
}

static std::string GetModDiagnostic(const IMod *mod) {
#if BML_ENABLE_ANGELSCRIPT
    if (auto *scriptMod = dynamic_cast<const BML::ScriptMod *>(mod))
        return scriptMod->GetLastDiagnostic();
#endif
    return "";
}

BML_BEGIN_CDECLS

BML_EXPORT BML_MOD_KIND BML_GetModKind(const char *id) {
    if (!IsValidKeyPart(id))
        return BML_MOD_KIND_UNKNOWN;
    ModContext *context = BML_GetModContext();
    return context ? GetModKind(context->FindMod(id)) : BML_MOD_KIND_UNKNOWN;
}

BML_EXPORT BML_MOD_STATE BML_GetModState(const char *id) {
    if (!IsValidKeyPart(id))
        return BML_MOD_STATE_NOT_FOUND;
    ModContext *context = BML_GetModContext();
    return context ? GetModState(context->FindMod(id)) : BML_MOD_STATE_NOT_FOUND;
}

BML_EXPORT int BML_GetModDiagnostic(const char *id,
                                    char *buffer,
                                    size_t bufferSize,
                                    size_t *outRequiredSize) {
    if (!IsValidKeyPart(id)) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_INVALID_PARAMETER;
    }

    ModContext *context = BML_GetModContext();
    IMod *mod = context ? context->FindMod(id) : nullptr;
    if (!mod) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_NOT_FOUND;
    }

    return GuardInteropMutation([&]() {
        return CopyStringOut(GetModDiagnostic(mod), buffer, bufferSize, outRequiredSize);
    });
}

BML_EXPORT BML_CallFrame *BML_CreateCallFrame(void) {
    return new (std::nothrow) BML_CallFrame();
}

BML_EXPORT void BML_DestroyCallFrame(BML_CallFrame *frame) {
    delete frame;
}

BML_EXPORT void BML_CallFrame_Clear(BML_CallFrame *frame) {
    BML_ClearCallFrame(frame);
}

BML_EXPORT size_t BML_CallFrame_GetArgCount(const BML_CallFrame *frame) {
    return BML_GetCallFrameArgCount(frame);
}

BML_EXPORT BML_CALL_VALUE_TYPE BML_CallFrame_GetArgType(const BML_CallFrame *frame, size_t index) {
    return ToPublicCallValueType(BML_GetCallFrameArgType(frame, index));
}

BML_EXPORT int BML_CallFrame_ClearArg(BML_CallFrame *frame, size_t index) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    return BML_ClearCallFrameArg(frame, index);
}

BML_EXPORT int BML_CallFrame_SetBool(BML_CallFrame *frame, size_t index, int value) {
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        slot->Type = BML_CallValueType::Bool;
        slot->IntValue = value != 0 ? 1 : 0;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetBool(const BML_CallFrame *frame, size_t index, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::Bool, &slot);
    if (status != BML_OK)
        return status;
    *outValue = slot->IntValue != 0 ? 1 : 0;
    return BML_OK;
}

BML_EXPORT int BML_CallFrame_SetInt(BML_CallFrame *frame, size_t index, int value) {
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        slot->Type = BML_CallValueType::Int;
        slot->IntValue = value;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetInt(const BML_CallFrame *frame, size_t index, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::Int, &slot);
    if (status != BML_OK)
        return status;
    *outValue = slot->IntValue;
    return BML_OK;
}

BML_EXPORT int BML_CallFrame_SetFloat(BML_CallFrame *frame, size_t index, float value) {
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        slot->Type = BML_CallValueType::Float;
        slot->FloatValue = value;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetFloat(const BML_CallFrame *frame, size_t index, float *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::Float, &slot);
    if (status != BML_OK)
        return status;
    *outValue = slot->FloatValue;
    return BML_OK;
}

BML_EXPORT int BML_CallFrame_SetString(BML_CallFrame *frame, size_t index, const char *value) {
    if (!frame || !value)
        return BML_ERROR_INVALID_PARAMETER;
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        slot->Type = BML_CallValueType::String;
        slot->StringValue = value;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetString(const BML_CallFrame *frame,
                                       size_t index,
                                             char *buffer,
                                             size_t bufferSize,
                                             size_t *outRequiredSize) {
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::String, &slot);
    if (status != BML_OK) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return status;
    }
    return CopyStringOut(slot->StringValue, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowString(const BML_CallFrame *frame,
                                          size_t index,
                                          const char **outValue,
                                          size_t *outSize) {
    if (outValue)
        *outValue = nullptr;
    if (outSize)
        *outSize = 0;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::String, &slot);
    return status == BML_OK ? BorrowStringOut(slot->StringValue, outValue, outSize) : status;
}

BML_EXPORT int BML_CallFrame_SetBoolArray(BML_CallFrame *frame, size_t index, const int *values, size_t count) {
    if ((!values && count > 0))
        return BML_ERROR_INVALID_PARAMETER;
    return SetArgValue(frame, index, [&](BML_CallValue &slot) {
        slot.Type = BML_CallValueType::BoolArray;
        slot.IntArrayValue.reserve(count);
        for (size_t i = 0; i < count; ++i)
            slot.IntArrayValue.push_back(values[i] ? 1 : 0);
    });
}

BML_EXPORT size_t BML_CallFrame_GetBoolArrayCount(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = nullptr;
    return BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::BoolArray, &slot) == BML_OK
               ? slot->IntArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyBoolArray(const BML_CallFrame *frame,
                                           size_t index,
                                           int *buffer,
                                           size_t bufferCount,
                                           size_t *outRequiredCount) {
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::BoolArray, &slot);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(slot->IntArrayValue, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowBoolArray(const BML_CallFrame *frame,
                                             size_t index,
                                             const int **outValues,
                                             size_t *outCount) {
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::BoolArray, &slot);
    return status == BML_OK ? BorrowVectorOut(slot->IntArrayValue, outValues, outCount) : status;
}

BML_EXPORT int BML_CallFrame_SetIntArray(BML_CallFrame *frame, size_t index, const int *values, size_t count) {
    if (!values && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetArgValue(frame, index, [&](BML_CallValue &slot) {
        slot.Type = BML_CallValueType::IntArray;
        if (count > 0)
            slot.IntArrayValue.assign(values, values + count);
    });
}

BML_EXPORT size_t BML_CallFrame_GetIntArrayCount(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = nullptr;
    return BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::IntArray, &slot) == BML_OK
               ? slot->IntArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyIntArray(const BML_CallFrame *frame,
                                          size_t index,
                                          int *buffer,
                                          size_t bufferCount,
                                          size_t *outRequiredCount) {
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::IntArray, &slot);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(slot->IntArrayValue, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowIntArray(const BML_CallFrame *frame,
                                            size_t index,
                                            const int **outValues,
                                            size_t *outCount) {
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::IntArray, &slot);
    return status == BML_OK ? BorrowVectorOut(slot->IntArrayValue, outValues, outCount) : status;
}

BML_EXPORT int BML_CallFrame_SetFloatArray(BML_CallFrame *frame, size_t index, const float *values, size_t count) {
    if (!values && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetArgValue(frame, index, [&](BML_CallValue &slot) {
        slot.Type = BML_CallValueType::FloatArray;
        if (count > 0)
            slot.FloatArrayValue.assign(values, values + count);
    });
}

BML_EXPORT size_t BML_CallFrame_GetFloatArrayCount(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = nullptr;
    return BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::FloatArray, &slot) == BML_OK
               ? slot->FloatArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyFloatArray(const BML_CallFrame *frame,
                                            size_t index,
                                            float *buffer,
                                            size_t bufferCount,
                                            size_t *outRequiredCount) {
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::FloatArray, &slot);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(slot->FloatArrayValue, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowFloatArray(const BML_CallFrame *frame,
                                              size_t index,
                                              const float **outValues,
                                              size_t *outCount) {
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::FloatArray, &slot);
    return status == BML_OK ? BorrowVectorOut(slot->FloatArrayValue, outValues, outCount) : status;
}

BML_EXPORT int BML_CallFrame_SetStringArray(BML_CallFrame *frame,
                                            size_t index,
                                            const char *const *values,
                                            size_t count) {
    if (!values && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetArgValue(frame, index, [&](BML_CallValue &slot) {
        slot.Type = BML_CallValueType::StringArray;
        slot.StringArrayValue.reserve(count);
        for (size_t i = 0; i < count; ++i)
            slot.StringArrayValue.emplace_back(values[i] ? values[i] : "");
    });
}

BML_EXPORT size_t BML_CallFrame_GetStringArrayCount(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = nullptr;
    return BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::StringArray, &slot) == BML_OK
               ? slot->StringArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_GetStringArrayItem(const BML_CallFrame *frame,
                                                size_t index,
                                                size_t itemIndex,
                                                char *buffer,
                                                size_t bufferSize,
                                                size_t *outRequiredSize) {
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::StringArray, &slot);
    if (status != BML_OK) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return status;
    }
    if (itemIndex >= slot->StringArrayValue.size()) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_NOT_FOUND;
    }
    return CopyStringOut(slot->StringArrayValue[itemIndex], buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowStringArrayItem(const BML_CallFrame *frame,
                                                   size_t index,
                                                   size_t itemIndex,
                                                   const char **outValue,
                                                   size_t *outSize) {
    if (outValue)
        *outValue = nullptr;
    if (outSize)
        *outSize = 0;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::StringArray, &slot);
    if (status != BML_OK)
        return status;
    if (itemIndex >= slot->StringArrayValue.size())
        return BML_ERROR_NOT_FOUND;
    return BorrowStringOut(slot->StringArrayValue[itemIndex], outValue, outSize);
}

BML_EXPORT int BML_CallFrame_SetBuffer(BML_CallFrame *frame, size_t index, const uint8_t *data, size_t size) {
    if (!data && size > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetArgValue(frame, index, [&](BML_CallValue &slot) {
        slot.Type = BML_CallValueType::Buffer;
        if (size > 0)
            slot.BufferValue.assign(data, data + size);
    });
}

BML_EXPORT size_t BML_CallFrame_GetBufferSize(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = nullptr;
    return BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::Buffer, &slot) == BML_OK
               ? slot->BufferValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyBuffer(const BML_CallFrame *frame,
                                        size_t index,
                                        uint8_t *buffer,
                                        size_t bufferSize,
                                        size_t *outRequiredSize) {
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::Buffer, &slot);
    if (status != BML_OK) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return status;
    }
    return CopyBufferOut(slot->BufferValue, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowBuffer(const BML_CallFrame *frame,
                                          size_t index,
                                          const uint8_t **outData,
                                          size_t *outSize) {
    if (outData)
        *outData = nullptr;
    if (outSize)
        *outSize = 0;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::Buffer, &slot);
    return status == BML_OK ? BorrowBufferOut(slot->BufferValue, outData, outSize) : status;
}

BML_EXPORT int BML_CallFrame_SetObjectId(BML_CallFrame *frame, size_t index, int objectId) {
    return SetArgValue(frame, index, [&](BML_CallValue &slot) {
        slot.Type = BML_CallValueType::ObjectId;
        slot.IntValue = objectId;
    });
}

BML_EXPORT int BML_CallFrame_GetObjectId(const BML_CallFrame *frame, size_t index, int *outObjectId) {
    if (!outObjectId)
        return BML_ERROR_INVALID_PARAMETER;
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, BML_CallValueType::ObjectId, &slot);
    if (status != BML_OK)
        return status;
    *outObjectId = slot->IntValue;
    return BML_OK;
}

BML_EXPORT int BML_CallFrame_SetResultBool(BML_CallFrame *frame, int value) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        frame->Result.Type = BML_CallValueType::Bool;
        frame->Result.IntValue = value != 0 ? 1 : 0;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetResultBool(const BML_CallFrame *frame, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    if (frame->Result.Type == BML_CallValueType::Empty)
        return BML_ERROR_NOT_FOUND;
    if (frame->Result.Type != BML_CallValueType::Bool)
        return BML_ERROR_INTEROP_TYPE_MISMATCH;
    *outValue = frame->Result.IntValue != 0 ? 1 : 0;
    return BML_OK;
}

BML_EXPORT BML_CALL_VALUE_TYPE BML_CallFrame_GetResultType(const BML_CallFrame *frame) {
    return ToPublicCallValueType(BML_GetCallFrameResultType(frame));
}

BML_EXPORT int BML_CallFrame_ClearResult(BML_CallFrame *frame) {
    return BML_ClearCallFrameResult(frame);
}

BML_EXPORT int BML_CallFrame_SetResultInt(BML_CallFrame *frame, int value) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        frame->Result.Type = BML_CallValueType::Int;
        frame->Result.IntValue = value;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetResultInt(const BML_CallFrame *frame, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    if (frame->Result.Type == BML_CallValueType::Empty)
        return BML_ERROR_NOT_FOUND;
    if (frame->Result.Type != BML_CallValueType::Int)
        return BML_ERROR_INTEROP_TYPE_MISMATCH;
    *outValue = frame->Result.IntValue;
    return BML_OK;
}

BML_EXPORT int BML_CallFrame_SetResultFloat(BML_CallFrame *frame, float value) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        frame->Result.Type = BML_CallValueType::Float;
        frame->Result.FloatValue = value;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetResultFloat(const BML_CallFrame *frame, float *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    if (frame->Result.Type == BML_CallValueType::Empty)
        return BML_ERROR_NOT_FOUND;
    if (frame->Result.Type != BML_CallValueType::Float)
        return BML_ERROR_INTEROP_TYPE_MISMATCH;
    *outValue = frame->Result.FloatValue;
    return BML_OK;
}

BML_EXPORT int BML_CallFrame_SetResultString(BML_CallFrame *frame, const char *value) {
    if (!frame || !value)
        return BML_ERROR_INVALID_PARAMETER;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        frame->Result.Type = BML_CallValueType::String;
        frame->Result.StringValue = value;
        return BML_OK;
    });
}

BML_EXPORT int BML_CallFrame_GetResultString(const BML_CallFrame *frame,
                                             char *buffer,
                                             size_t bufferSize,
                                             size_t *outRequiredSize) {
    if (!frame) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }
    if (frame->Result.Type == BML_CallValueType::Empty) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_NOT_FOUND;
    }
    if (frame->Result.Type != BML_CallValueType::String) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_INTEROP_TYPE_MISMATCH;
    }
    return CopyStringOut(frame->Result.StringValue, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowResultString(const BML_CallFrame *frame,
                                                const char **outValue,
                                                size_t *outSize) {
    if (outValue)
        *outValue = nullptr;
    if (outSize)
        *outSize = 0;
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::String, &result);
    return status == BML_OK ? BorrowStringOut(result->StringValue, outValue, outSize) : status;
}

BML_EXPORT int BML_CallFrame_SetResultBoolArray(BML_CallFrame *frame, const int *values, size_t count) {
    if (!values && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetResultValue(frame, [&](BML_CallValue &result) {
        result.Type = BML_CallValueType::BoolArray;
        result.IntArrayValue.reserve(count);
        for (size_t i = 0; i < count; ++i)
            result.IntArrayValue.push_back(values[i] ? 1 : 0);
    });
}

BML_EXPORT size_t BML_CallFrame_GetResultBoolArrayCount(const BML_CallFrame *frame) {
    const BML_CallValue *result = nullptr;
    return BML_GetCallFrameResultChecked(frame, BML_CallValueType::BoolArray, &result) == BML_OK
               ? result->IntArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyResultBoolArray(const BML_CallFrame *frame,
                                                 int *buffer,
                                                 size_t bufferCount,
                                                 size_t *outRequiredCount) {
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::BoolArray, &result);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(result->IntArrayValue, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowResultBoolArray(const BML_CallFrame *frame,
                                                   const int **outValues,
                                                   size_t *outCount) {
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::BoolArray, &result);
    return status == BML_OK ? BorrowVectorOut(result->IntArrayValue, outValues, outCount) : status;
}

BML_EXPORT int BML_CallFrame_SetResultIntArray(BML_CallFrame *frame, const int *values, size_t count) {
    if (!values && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetResultValue(frame, [&](BML_CallValue &result) {
        result.Type = BML_CallValueType::IntArray;
        if (count > 0)
            result.IntArrayValue.assign(values, values + count);
    });
}

BML_EXPORT size_t BML_CallFrame_GetResultIntArrayCount(const BML_CallFrame *frame) {
    const BML_CallValue *result = nullptr;
    return BML_GetCallFrameResultChecked(frame, BML_CallValueType::IntArray, &result) == BML_OK
               ? result->IntArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyResultIntArray(const BML_CallFrame *frame,
                                                int *buffer,
                                                size_t bufferCount,
                                                size_t *outRequiredCount) {
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::IntArray, &result);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(result->IntArrayValue, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowResultIntArray(const BML_CallFrame *frame,
                                                  const int **outValues,
                                                  size_t *outCount) {
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::IntArray, &result);
    return status == BML_OK ? BorrowVectorOut(result->IntArrayValue, outValues, outCount) : status;
}

BML_EXPORT int BML_CallFrame_SetResultFloatArray(BML_CallFrame *frame, const float *values, size_t count) {
    if (!values && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetResultValue(frame, [&](BML_CallValue &result) {
        result.Type = BML_CallValueType::FloatArray;
        if (count > 0)
            result.FloatArrayValue.assign(values, values + count);
    });
}

BML_EXPORT size_t BML_CallFrame_GetResultFloatArrayCount(const BML_CallFrame *frame) {
    const BML_CallValue *result = nullptr;
    return BML_GetCallFrameResultChecked(frame, BML_CallValueType::FloatArray, &result) == BML_OK
               ? result->FloatArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyResultFloatArray(const BML_CallFrame *frame,
                                                  float *buffer,
                                                  size_t bufferCount,
                                                  size_t *outRequiredCount) {
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::FloatArray, &result);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(result->FloatArrayValue, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowResultFloatArray(const BML_CallFrame *frame,
                                                    const float **outValues,
                                                    size_t *outCount) {
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::FloatArray, &result);
    return status == BML_OK ? BorrowVectorOut(result->FloatArrayValue, outValues, outCount) : status;
}

BML_EXPORT int BML_CallFrame_SetResultStringArray(BML_CallFrame *frame,
                                                  const char *const *values,
                                                  size_t count) {
    if (!values && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetResultValue(frame, [&](BML_CallValue &result) {
        result.Type = BML_CallValueType::StringArray;
        result.StringArrayValue.reserve(count);
        for (size_t i = 0; i < count; ++i)
            result.StringArrayValue.emplace_back(values[i] ? values[i] : "");
    });
}

BML_EXPORT size_t BML_CallFrame_GetResultStringArrayCount(const BML_CallFrame *frame) {
    const BML_CallValue *result = nullptr;
    return BML_GetCallFrameResultChecked(frame, BML_CallValueType::StringArray, &result) == BML_OK
               ? result->StringArrayValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_GetResultStringArrayItem(const BML_CallFrame *frame,
                                                      size_t itemIndex,
                                                      char *buffer,
                                                      size_t bufferSize,
                                                      size_t *outRequiredSize) {
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::StringArray, &result);
    if (status != BML_OK) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return status;
    }
    if (itemIndex >= result->StringArrayValue.size()) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_NOT_FOUND;
    }
    return CopyStringOut(result->StringArrayValue[itemIndex], buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowResultStringArrayItem(const BML_CallFrame *frame,
                                                         size_t itemIndex,
                                                         const char **outValue,
                                                         size_t *outSize) {
    if (outValue)
        *outValue = nullptr;
    if (outSize)
        *outSize = 0;
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::StringArray, &result);
    if (status != BML_OK)
        return status;
    if (itemIndex >= result->StringArrayValue.size())
        return BML_ERROR_NOT_FOUND;
    return BorrowStringOut(result->StringArrayValue[itemIndex], outValue, outSize);
}

BML_EXPORT int BML_CallFrame_SetResultBuffer(BML_CallFrame *frame, const uint8_t *data, size_t size) {
    if (!data && size > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetResultValue(frame, [&](BML_CallValue &result) {
        result.Type = BML_CallValueType::Buffer;
        if (size > 0)
            result.BufferValue.assign(data, data + size);
    });
}

BML_EXPORT size_t BML_CallFrame_GetResultBufferSize(const BML_CallFrame *frame) {
    const BML_CallValue *result = nullptr;
    return BML_GetCallFrameResultChecked(frame, BML_CallValueType::Buffer, &result) == BML_OK
               ? result->BufferValue.size()
               : 0;
}

BML_EXPORT int BML_CallFrame_CopyResultBuffer(const BML_CallFrame *frame,
                                              uint8_t *buffer,
                                              size_t bufferSize,
                                              size_t *outRequiredSize) {
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::Buffer, &result);
    if (status != BML_OK) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return status;
    }
    return CopyBufferOut(result->BufferValue, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowResultBuffer(const BML_CallFrame *frame,
                                                const uint8_t **outData,
                                                size_t *outSize) {
    if (outData)
        *outData = nullptr;
    if (outSize)
        *outSize = 0;
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::Buffer, &result);
    return status == BML_OK ? BorrowBufferOut(result->BufferValue, outData, outSize) : status;
}

BML_EXPORT int BML_CallFrame_SetResultObjectId(BML_CallFrame *frame, int objectId) {
    return SetResultValue(frame, [&](BML_CallValue &result) {
        result.Type = BML_CallValueType::ObjectId;
        result.IntValue = objectId;
    });
}

BML_EXPORT int BML_CallFrame_GetResultObjectId(const BML_CallFrame *frame, int *outObjectId) {
    if (!outObjectId)
        return BML_ERROR_INVALID_PARAMETER;
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, BML_CallValueType::ObjectId, &result);
    if (status != BML_OK)
        return status;
    *outObjectId = result->IntValue;
    return BML_OK;
}

BML_EXPORT int BML_RegisterNativeModExport(const char *modId,
                                           const char *name,
                                           const char *signature,
                                           BML_NativeExportCallback callback,
                                           void *userdata) {
    return GuardInteropMutation([&]() {
        return BML::ExportRegistry::RegisterNative(modId, name, signature, callback, userdata);
    });
}

BML_EXPORT int BML_UnregisterNativeModExport(const char *modId,
                                             const char *name,
                                             const char *signature) {
    return GuardInteropMutation([&]() {
        return BML::ExportRegistry::UnregisterNative(modId, name, signature);
    });
}

BML_EXPORT int BML_UnregisterNativeModExports(const char *modId) {
    return GuardInteropMutation([&]() {
        return BML::ExportRegistry::UnregisterNativeForMod(modId);
    });
}

BML_EXPORT int BML_GetModExportCount(const char *modId) {
    if (!IsValidKeyPart(modId))
        return BML_ERROR_INVALID_PARAMETER;

    return GuardInteropMutation([&]() {
        return BML::ExportRegistry::GetCount(modId);
    });
}

BML_EXPORT int BML_GetModExportInfo(const char *modId,
                                    size_t index,
                                    char *nameBuffer,
                                    size_t nameBufferSize,
                                    size_t *outNameRequiredSize,
                                    char *signatureBuffer,
                                    size_t signatureBufferSize,
                                    size_t *outSignatureRequiredSize) {
    if (!IsValidKeyPart(modId))
        return BML_ERROR_INVALID_PARAMETER;

    return GuardInteropMutation([&]() {
        BML::ExportInfo info;
        const int status = BML::ExportRegistry::GetInfo(modId, index, info);
        if (status != BML_OK) {
            if (outNameRequiredSize)
                *outNameRequiredSize = 0;
            if (outSignatureRequiredSize)
                *outSignatureRequiredSize = 0;
            return status;
        }

        const int nameStatus = CopyStringOut(info.Name, nameBuffer, nameBufferSize, outNameRequiredSize);
        const int signatureStatus = CopyStringOut(info.Signature,
                                                  signatureBuffer,
                                                  signatureBufferSize,
                                                  outSignatureRequiredSize);
        return nameStatus != BML_OK ? nameStatus : signatureStatus;
    });
}

BML_EXPORT BML_ModExport *BML_FindModExport(const char *modId,
                                            const char *name,
                                            const char *signature) {
    try {
        BML_ModExport *handle = nullptr;
        return BML::ExportRegistry::FindEx(modId, name, signature, &handle) == BML_OK ? handle : nullptr;
    } catch (...) {
        return nullptr;
    }
}

BML_EXPORT int BML_FindModExportEx(const char *modId,
                                   const char *name,
                                   const char *signature,
                                   BML_ModExport **outHandle) {
    return GuardInteropMutation([&]() {
        return BML::ExportRegistry::FindEx(modId, name, signature, outHandle);
    });
}

BML_EXPORT int BML_IsModExportValid(BML_ModExport *handle) {
    try {
        return BML::ExportRegistry::IsValid(handle) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT int BML_GetModExportModId(const BML_ModExport *handle,
                                     char *buffer,
                                     size_t bufferSize,
                                     size_t *outRequiredSize) {
    if (!handle) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_INVALID_PARAMETER;
    }
    return CopyStringOut(handle->Key.ModId, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_GetModExportName(const BML_ModExport *handle,
                                    char *buffer,
                                    size_t bufferSize,
                                    size_t *outRequiredSize) {
    if (!handle) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_INVALID_PARAMETER;
    }
    return CopyStringOut(handle->Key.Name, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_GetModExportSignature(const BML_ModExport *handle,
                                         char *buffer,
                                         size_t bufferSize,
                                         size_t *outRequiredSize) {
    if (!handle) {
        if (outRequiredSize)
            *outRequiredSize = 0;
        return BML_ERROR_INVALID_PARAMETER;
    }
    return CopyStringOut(handle->Key.Signature, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT uint32_t BML_AddRefModExport(BML_ModExport *handle) {
    if (!handle)
        return 0;
    return ++handle->RefCount;
}

BML_EXPORT uint32_t BML_ReleaseModExport(BML_ModExport *handle) {
    if (!handle)
        return 0;
    const uint32_t count = --handle->RefCount;
    if (count == 0)
        delete handle;
    return count;
}

BML_EXPORT int BML_CallModExport(BML_ModExport *handle, BML_CallFrame *frame) {
    return GuardInteropMutation([&]() {
        return BML::ExportRegistry::Call(handle, frame);
    });
}

BML_END_CDECLS
