#include "BML/Interop.h"

#include <algorithm>
#include <cstdint>
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

static BML_CallValueType ToInternalCallValueType(BML_CALL_VALUE_TYPE type) {
    switch (type) {
    case BML_CALL_VALUE_BOOL:
        return BML_CallValueType::Bool;
    case BML_CALL_VALUE_INT:
        return BML_CallValueType::Int;
    case BML_CALL_VALUE_FLOAT:
        return BML_CallValueType::Float;
    case BML_CALL_VALUE_STRING:
        return BML_CallValueType::String;
    case BML_CALL_VALUE_BOOL_ARRAY:
        return BML_CallValueType::BoolArray;
    case BML_CALL_VALUE_INT_ARRAY:
        return BML_CallValueType::IntArray;
    case BML_CALL_VALUE_FLOAT_ARRAY:
        return BML_CallValueType::FloatArray;
    case BML_CALL_VALUE_STRING_ARRAY:
        return BML_CallValueType::StringArray;
    case BML_CALL_VALUE_BUFFER:
        return BML_CallValueType::Buffer;
    case BML_CALL_VALUE_OBJECT_ID:
        return BML_CallValueType::ObjectId;
    default:
        return BML_CallValueType::Empty;
    }
}

static bool IsContiguousDataType(BML_CALL_VALUE_TYPE type) {
    switch (ToInternalCallValueType(type)) {
    case BML_CallValueType::BoolArray:
    case BML_CallValueType::IntArray:
    case BML_CallValueType::FloatArray:
    case BML_CallValueType::Buffer:
        return true;
    default:
        return false;
    }
}

template <BML_CallValueType Type>
struct CallValueVectorTraits;

template <>
struct CallValueVectorTraits<BML_CallValueType::BoolArray> {
    using Element = int;

    static std::vector<Element> &Values(BML_CallValue &value) {
        return value.IntArrayValue;
    }

    static const std::vector<Element> &Values(const BML_CallValue &value) {
        return value.IntArrayValue;
    }

    static Element Store(Element value) {
        return value ? 1 : 0;
    }
};

template <>
struct CallValueVectorTraits<BML_CallValueType::IntArray> {
    using Element = int;

    static std::vector<Element> &Values(BML_CallValue &value) {
        return value.IntArrayValue;
    }

    static const std::vector<Element> &Values(const BML_CallValue &value) {
        return value.IntArrayValue;
    }

    static Element Store(Element value) {
        return value;
    }
};

template <>
struct CallValueVectorTraits<BML_CallValueType::FloatArray> {
    using Element = float;

    static std::vector<Element> &Values(BML_CallValue &value) {
        return value.FloatArrayValue;
    }

    static const std::vector<Element> &Values(const BML_CallValue &value) {
        return value.FloatArrayValue;
    }

    static Element Store(Element value) {
        return value;
    }
};

template <>
struct CallValueVectorTraits<BML_CallValueType::Buffer> {
    using Element = std::uint8_t;

    static std::vector<Element> &Values(BML_CallValue &value) {
        return value.BufferValue;
    }

    static const std::vector<Element> &Values(const BML_CallValue &value) {
        return value.BufferValue;
    }

    static Element Store(Element value) {
        return value;
    }
};

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
static int SetResultValue(BML_CallFrame *frame, Fill fill);

template <BML_CallValueType Type>
static void AssignVectorValue(BML_CallValue &value,
                              const typename CallValueVectorTraits<Type>::Element *items,
                              size_t count) {
    using Traits = CallValueVectorTraits<Type>;
    value.Type = Type;
    auto &storage = Traits::Values(value);
    storage.reserve(count);
    if (!items || count == 0)
        return;

    if constexpr (Type == BML_CallValueType::BoolArray) {
        for (size_t i = 0; i < count; ++i)
            storage.push_back(Traits::Store(items[i]));
    } else {
        storage.assign(items, items + count);
    }
}

template <BML_CallValueType Type>
static int SetArgVectorValue(BML_CallFrame *frame,
                             size_t index,
                             const typename CallValueVectorTraits<Type>::Element *items,
                             size_t count) {
    if (!items && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetArgValue(frame, index, [&](BML_CallValue &slot) {
        AssignVectorValue<Type>(slot, items, count);
    });
}

template <BML_CallValueType Type>
static int SetResultVectorValue(BML_CallFrame *frame,
                                const typename CallValueVectorTraits<Type>::Element *items,
                                size_t count) {
    if (!items && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    return SetResultValue(frame, [&](BML_CallValue &result) {
        AssignVectorValue<Type>(result, items, count);
    });
}

template <BML_CallValueType Type>
static size_t GetArgVectorCount(const BML_CallFrame *frame, size_t index) {
    const BML_CallValue *slot = nullptr;
    return BML_GetCallFrameArgChecked(frame, index, Type, &slot) == BML_OK
               ? CallValueVectorTraits<Type>::Values(*slot).size()
               : 0;
}

template <BML_CallValueType Type>
static size_t GetResultVectorCount(const BML_CallFrame *frame) {
    const BML_CallValue *result = nullptr;
    return BML_GetCallFrameResultChecked(frame, Type, &result) == BML_OK
               ? CallValueVectorTraits<Type>::Values(*result).size()
               : 0;
}

template <BML_CallValueType Type>
static int CopyArgVectorValue(const BML_CallFrame *frame,
                              size_t index,
                              typename CallValueVectorTraits<Type>::Element *buffer,
                              size_t bufferCount,
                              size_t *outRequiredCount) {
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, Type, &slot);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(CallValueVectorTraits<Type>::Values(*slot), buffer, bufferCount, outRequiredCount);
}

template <BML_CallValueType Type>
static int CopyResultVectorValue(const BML_CallFrame *frame,
                                 typename CallValueVectorTraits<Type>::Element *buffer,
                                 size_t bufferCount,
                                 size_t *outRequiredCount) {
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, Type, &result);
    if (status != BML_OK) {
        if (outRequiredCount)
            *outRequiredCount = 0;
        return status;
    }
    return CopyVectorOut(CallValueVectorTraits<Type>::Values(*result), buffer, bufferCount, outRequiredCount);
}

template <BML_CallValueType Type>
static int BorrowVectorDataOut(const BML_CallValue &value,
                               const void **outData,
                               size_t *outCount,
                               size_t *outElementSize) {
    if (!outData)
        return BML_ERROR_INVALID_PARAMETER;

    const auto &storage = CallValueVectorTraits<Type>::Values(value);
    *outData = storage.empty() ? nullptr : storage.data();
    if (outCount)
        *outCount = storage.size();
    if (outElementSize)
        *outElementSize = sizeof(typename CallValueVectorTraits<Type>::Element);
    return BML_OK;
}

template <BML_CallValueType Type>
static int BorrowArgVectorValue(const BML_CallFrame *frame,
                                size_t index,
                                const typename CallValueVectorTraits<Type>::Element **outValues,
                                size_t *outCount) {
    if (!outValues)
        return BML_ERROR_INVALID_PARAMETER;
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;

    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, Type, &slot);
    if (status != BML_OK)
        return status;

    const void *data = nullptr;
    const int borrowStatus = BorrowVectorDataOut<Type>(*slot, &data, outCount, nullptr);
    if (borrowStatus == BML_OK)
        *outValues = static_cast<const typename CallValueVectorTraits<Type>::Element *>(data);
    return borrowStatus;
}

template <BML_CallValueType Type>
static int BorrowResultVectorValue(const BML_CallFrame *frame,
                                   const typename CallValueVectorTraits<Type>::Element **outValues,
                                   size_t *outCount) {
    if (!outValues)
        return BML_ERROR_INVALID_PARAMETER;
    if (outValues)
        *outValues = nullptr;
    if (outCount)
        *outCount = 0;

    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, Type, &result);
    if (status != BML_OK)
        return status;

    const void *data = nullptr;
    const int borrowStatus = BorrowVectorDataOut<Type>(*result, &data, outCount, nullptr);
    if (borrowStatus == BML_OK)
        *outValues = static_cast<const typename CallValueVectorTraits<Type>::Element *>(data);
    return borrowStatus;
}

static int SetDataValue(BML_CallValue &value, BML_CALL_VALUE_TYPE type, const void *data, size_t count) {
    switch (ToInternalCallValueType(type)) {
    case BML_CallValueType::BoolArray:
        AssignVectorValue<BML_CallValueType::BoolArray>(value, static_cast<const int *>(data), count);
        return BML_OK;
    case BML_CallValueType::IntArray:
        AssignVectorValue<BML_CallValueType::IntArray>(value, static_cast<const int *>(data), count);
        return BML_OK;
    case BML_CallValueType::FloatArray:
        AssignVectorValue<BML_CallValueType::FloatArray>(value, static_cast<const float *>(data), count);
        return BML_OK;
    case BML_CallValueType::Buffer:
        AssignVectorValue<BML_CallValueType::Buffer>(value, static_cast<const std::uint8_t *>(data), count);
        return BML_OK;
    default:
        return BML_ERROR_INTEROP_UNSUPPORTED;
    }
}

static int BorrowDataValue(const BML_CallValue &value,
                           BML_CALL_VALUE_TYPE type,
                           const void **outData,
                           size_t *outCount,
                           size_t *outElementSize) {
    if (!outData)
        return BML_ERROR_INVALID_PARAMETER;

    switch (ToInternalCallValueType(type)) {
    case BML_CallValueType::BoolArray:
        return BorrowVectorDataOut<BML_CallValueType::BoolArray>(value, outData, outCount, outElementSize);
    case BML_CallValueType::IntArray:
        return BorrowVectorDataOut<BML_CallValueType::IntArray>(value, outData, outCount, outElementSize);
    case BML_CallValueType::FloatArray:
        return BorrowVectorDataOut<BML_CallValueType::FloatArray>(value, outData, outCount, outElementSize);
    case BML_CallValueType::Buffer:
        return BorrowVectorDataOut<BML_CallValueType::Buffer>(value, outData, outCount, outElementSize);
    default:
        return BML_ERROR_INTEROP_UNSUPPORTED;
    }
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

BML_EXPORT int BML_CallFrame_SetData(BML_CallFrame *frame,
                                     size_t index,
                                     BML_CALL_VALUE_TYPE type,
                                     const void *data,
                                     size_t count) {
    if (!data && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    if (!IsContiguousDataType(type))
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        return SetDataValue(*slot, type, data, count);
    });
}

BML_EXPORT int BML_CallFrame_BorrowData(const BML_CallFrame *frame,
                                        size_t index,
                                        BML_CALL_VALUE_TYPE type,
                                        const void **outData,
                                        size_t *outCount,
                                        size_t *outElementSize) {
    if (outData)
        *outData = nullptr;
    if (outCount)
        *outCount = 0;
    if (outElementSize)
        *outElementSize = 0;

    if (!IsContiguousDataType(type))
        return BML_ERROR_INTEROP_UNSUPPORTED;

    const BML_CallValueType internalType = ToInternalCallValueType(type);
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, internalType, &slot);
    return status == BML_OK ? BorrowDataValue(*slot, type, outData, outCount, outElementSize) : status;
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
    return SetArgVectorValue<BML_CallValueType::BoolArray>(frame, index, values, count);
}

BML_EXPORT size_t BML_CallFrame_GetBoolArrayCount(const BML_CallFrame *frame, size_t index) {
    return GetArgVectorCount<BML_CallValueType::BoolArray>(frame, index);
}

BML_EXPORT int BML_CallFrame_CopyBoolArray(const BML_CallFrame *frame,
                                           size_t index,
                                           int *buffer,
                                           size_t bufferCount,
                                           size_t *outRequiredCount) {
    return CopyArgVectorValue<BML_CallValueType::BoolArray>(frame, index, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowBoolArray(const BML_CallFrame *frame,
                                             size_t index,
                                             const int **outValues,
                                             size_t *outCount) {
    return BorrowArgVectorValue<BML_CallValueType::BoolArray>(frame, index, outValues, outCount);
}

BML_EXPORT int BML_CallFrame_SetIntArray(BML_CallFrame *frame, size_t index, const int *values, size_t count) {
    return SetArgVectorValue<BML_CallValueType::IntArray>(frame, index, values, count);
}

BML_EXPORT size_t BML_CallFrame_GetIntArrayCount(const BML_CallFrame *frame, size_t index) {
    return GetArgVectorCount<BML_CallValueType::IntArray>(frame, index);
}

BML_EXPORT int BML_CallFrame_CopyIntArray(const BML_CallFrame *frame,
                                          size_t index,
                                          int *buffer,
                                          size_t bufferCount,
                                          size_t *outRequiredCount) {
    return CopyArgVectorValue<BML_CallValueType::IntArray>(frame, index, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowIntArray(const BML_CallFrame *frame,
                                            size_t index,
                                            const int **outValues,
                                            size_t *outCount) {
    return BorrowArgVectorValue<BML_CallValueType::IntArray>(frame, index, outValues, outCount);
}

BML_EXPORT int BML_CallFrame_SetFloatArray(BML_CallFrame *frame, size_t index, const float *values, size_t count) {
    return SetArgVectorValue<BML_CallValueType::FloatArray>(frame, index, values, count);
}

BML_EXPORT size_t BML_CallFrame_GetFloatArrayCount(const BML_CallFrame *frame, size_t index) {
    return GetArgVectorCount<BML_CallValueType::FloatArray>(frame, index);
}

BML_EXPORT int BML_CallFrame_CopyFloatArray(const BML_CallFrame *frame,
                                            size_t index,
                                            float *buffer,
                                            size_t bufferCount,
                                            size_t *outRequiredCount) {
    return CopyArgVectorValue<BML_CallValueType::FloatArray>(frame, index, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowFloatArray(const BML_CallFrame *frame,
                                              size_t index,
                                              const float **outValues,
                                              size_t *outCount) {
    return BorrowArgVectorValue<BML_CallValueType::FloatArray>(frame, index, outValues, outCount);
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
    return SetArgVectorValue<BML_CallValueType::Buffer>(frame, index, data, size);
}

BML_EXPORT size_t BML_CallFrame_GetBufferSize(const BML_CallFrame *frame, size_t index) {
    return GetArgVectorCount<BML_CallValueType::Buffer>(frame, index);
}

BML_EXPORT int BML_CallFrame_CopyBuffer(const BML_CallFrame *frame,
                                        size_t index,
                                        uint8_t *buffer,
                                        size_t bufferSize,
                                        size_t *outRequiredSize) {
    return CopyArgVectorValue<BML_CallValueType::Buffer>(frame, index, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowBuffer(const BML_CallFrame *frame,
                                          size_t index,
                                          const uint8_t **outData,
                                          size_t *outSize) {
    return BorrowArgVectorValue<BML_CallValueType::Buffer>(frame, index, outData, outSize);
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

BML_EXPORT int BML_CallFrame_SetResultData(BML_CallFrame *frame,
                                           BML_CALL_VALUE_TYPE type,
                                           const void *data,
                                           size_t count) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    if (!data && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    if (!IsContiguousDataType(type))
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        return SetDataValue(frame->Result, type, data, count);
    });
}

BML_EXPORT int BML_CallFrame_BorrowResultData(const BML_CallFrame *frame,
                                              BML_CALL_VALUE_TYPE type,
                                              const void **outData,
                                              size_t *outCount,
                                              size_t *outElementSize) {
    if (outData)
        *outData = nullptr;
    if (outCount)
        *outCount = 0;
    if (outElementSize)
        *outElementSize = 0;

    if (!IsContiguousDataType(type))
        return BML_ERROR_INTEROP_UNSUPPORTED;

    const BML_CallValueType internalType = ToInternalCallValueType(type);
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, internalType, &result);
    return status == BML_OK ? BorrowDataValue(*result, type, outData, outCount, outElementSize) : status;
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
    return SetResultVectorValue<BML_CallValueType::BoolArray>(frame, values, count);
}

BML_EXPORT size_t BML_CallFrame_GetResultBoolArrayCount(const BML_CallFrame *frame) {
    return GetResultVectorCount<BML_CallValueType::BoolArray>(frame);
}

BML_EXPORT int BML_CallFrame_CopyResultBoolArray(const BML_CallFrame *frame,
                                                 int *buffer,
                                                 size_t bufferCount,
                                                 size_t *outRequiredCount) {
    return CopyResultVectorValue<BML_CallValueType::BoolArray>(frame, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowResultBoolArray(const BML_CallFrame *frame,
                                                   const int **outValues,
                                                   size_t *outCount) {
    return BorrowResultVectorValue<BML_CallValueType::BoolArray>(frame, outValues, outCount);
}

BML_EXPORT int BML_CallFrame_SetResultIntArray(BML_CallFrame *frame, const int *values, size_t count) {
    return SetResultVectorValue<BML_CallValueType::IntArray>(frame, values, count);
}

BML_EXPORT size_t BML_CallFrame_GetResultIntArrayCount(const BML_CallFrame *frame) {
    return GetResultVectorCount<BML_CallValueType::IntArray>(frame);
}

BML_EXPORT int BML_CallFrame_CopyResultIntArray(const BML_CallFrame *frame,
                                                int *buffer,
                                                size_t bufferCount,
                                                size_t *outRequiredCount) {
    return CopyResultVectorValue<BML_CallValueType::IntArray>(frame, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowResultIntArray(const BML_CallFrame *frame,
                                                  const int **outValues,
                                                  size_t *outCount) {
    return BorrowResultVectorValue<BML_CallValueType::IntArray>(frame, outValues, outCount);
}

BML_EXPORT int BML_CallFrame_SetResultFloatArray(BML_CallFrame *frame, const float *values, size_t count) {
    return SetResultVectorValue<BML_CallValueType::FloatArray>(frame, values, count);
}

BML_EXPORT size_t BML_CallFrame_GetResultFloatArrayCount(const BML_CallFrame *frame) {
    return GetResultVectorCount<BML_CallValueType::FloatArray>(frame);
}

BML_EXPORT int BML_CallFrame_CopyResultFloatArray(const BML_CallFrame *frame,
                                                  float *buffer,
                                                  size_t bufferCount,
                                                  size_t *outRequiredCount) {
    return CopyResultVectorValue<BML_CallValueType::FloatArray>(frame, buffer, bufferCount, outRequiredCount);
}

BML_EXPORT int BML_CallFrame_BorrowResultFloatArray(const BML_CallFrame *frame,
                                                    const float **outValues,
                                                    size_t *outCount) {
    return BorrowResultVectorValue<BML_CallValueType::FloatArray>(frame, outValues, outCount);
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
    return SetResultVectorValue<BML_CallValueType::Buffer>(frame, data, size);
}

BML_EXPORT size_t BML_CallFrame_GetResultBufferSize(const BML_CallFrame *frame) {
    return GetResultVectorCount<BML_CallValueType::Buffer>(frame);
}

BML_EXPORT int BML_CallFrame_CopyResultBuffer(const BML_CallFrame *frame,
                                              uint8_t *buffer,
                                              size_t bufferSize,
                                              size_t *outRequiredSize) {
    return CopyResultVectorValue<BML_CallValueType::Buffer>(frame, buffer, bufferSize, outRequiredSize);
}

BML_EXPORT int BML_CallFrame_BorrowResultBuffer(const BML_CallFrame *frame,
                                                const uint8_t **outData,
                                                size_t *outSize) {
    return BorrowResultVectorValue<BML_CallValueType::Buffer>(frame, outData, outSize);
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
