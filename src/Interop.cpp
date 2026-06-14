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

static void InvalidateStringArrayPointerCache(BML_CallValue &value) {
    value.StringArrayPointerCache.clear();
    value.StringArrayPointerCacheValid = false;
}

static const std::vector<const char *> &EnsureStringArrayPointerCache(const BML_CallValue &value) {
    if (value.StringArrayPointerCacheValid &&
        value.StringArrayPointerCache.size() == value.StringArrayValue.size()) {
        return value.StringArrayPointerCache;
    }
    value.StringArrayPointerCache.clear();
    value.StringArrayPointerCache.reserve(value.StringArrayValue.size());
    for (const std::string &item : value.StringArrayValue)
        value.StringArrayPointerCache.push_back(item.c_str());
    value.StringArrayPointerCacheValid = true;
    return value.StringArrayPointerCache;
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

static int SetValueData(BML_CallValue &value, BML_CALL_VALUE_TYPE type, const void *data, size_t count) {
    switch (ToInternalCallValueType(type)) {
    case BML_CallValueType::Bool:
        value.Type = BML_CallValueType::Bool;
        value.IntValue = data ? (*static_cast<const int *>(data) != 0 ? 1 : 0) : 0;
        return BML_OK;
    case BML_CallValueType::Int:
        value.Type = BML_CallValueType::Int;
        value.IntValue = data ? *static_cast<const int *>(data) : 0;
        return BML_OK;
    case BML_CallValueType::Float:
        value.Type = BML_CallValueType::Float;
        value.FloatValue = data ? *static_cast<const float *>(data) : 0.0f;
        return BML_OK;
    case BML_CallValueType::String:
        value.Type = BML_CallValueType::String;
        value.StringValue.assign(data ? static_cast<const char *>(data) : "", data ? count : 0);
        return BML_OK;
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
    case BML_CallValueType::StringArray: {
        value.Type = BML_CallValueType::StringArray;
        value.StringArrayValue.reserve(count);
        const auto *strings = static_cast<const char *const *>(data);
        for (size_t i = 0; i < count; ++i)
            value.StringArrayValue.emplace_back(strings && strings[i] ? strings[i] : "");
        InvalidateStringArrayPointerCache(value);
        return BML_OK;
    }
    case BML_CallValueType::ObjectId:
        value.Type = BML_CallValueType::ObjectId;
        value.IntValue = data ? *static_cast<const int *>(data) : 0;
        return BML_OK;
    default:
        return BML_ERROR_INTEROP_UNSUPPORTED;
    }
}

static int BorrowValueData(const BML_CallValue &value,
                           BML_CALL_VALUE_TYPE type,
                           const void **outData,
                           size_t *outCount,
                           size_t *outElementSize) {
    if (!outData)
        return BML_ERROR_INVALID_PARAMETER;

    switch (ToInternalCallValueType(type)) {
    case BML_CallValueType::Bool:
    case BML_CallValueType::Int:
    case BML_CallValueType::ObjectId:
        *outData = &value.IntValue;
        if (outCount)
            *outCount = 1;
        if (outElementSize)
            *outElementSize = sizeof(int);
        return BML_OK;
    case BML_CallValueType::Float:
        *outData = &value.FloatValue;
        if (outCount)
            *outCount = 1;
        if (outElementSize)
            *outElementSize = sizeof(float);
        return BML_OK;
    case BML_CallValueType::String:
        *outData = value.StringValue.empty() ? nullptr : value.StringValue.data();
        if (outCount)
            *outCount = value.StringValue.size();
        if (outElementSize)
            *outElementSize = sizeof(char);
        return BML_OK;
    case BML_CallValueType::BoolArray:
        return BorrowVectorDataOut<BML_CallValueType::BoolArray>(value, outData, outCount, outElementSize);
    case BML_CallValueType::IntArray:
        return BorrowVectorDataOut<BML_CallValueType::IntArray>(value, outData, outCount, outElementSize);
    case BML_CallValueType::FloatArray:
        return BorrowVectorDataOut<BML_CallValueType::FloatArray>(value, outData, outCount, outElementSize);
    case BML_CallValueType::Buffer:
        return BorrowVectorDataOut<BML_CallValueType::Buffer>(value, outData, outCount, outElementSize);
    case BML_CallValueType::StringArray: {
        const auto &cache = EnsureStringArrayPointerCache(value);
        *outData = cache.empty() ? nullptr : cache.data();
        if (outCount)
            *outCount = cache.size();
        if (outElementSize)
            *outElementSize = sizeof(const char *);
        return BML_OK;
    }
    default:
        return BML_ERROR_INTEROP_UNSUPPORTED;
    }
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

BML_EXPORT int BML_CallFrame_SetValue(BML_CallFrame *frame,
                                      size_t index,
                                      BML_CALL_VALUE_TYPE type,
                                      const void *data,
                                      size_t count) {
    if (!data && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    if (ToInternalCallValueType(type) == BML_CallValueType::Empty)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        return SetValueData(*slot, type, data, count);
    });
}

BML_EXPORT int BML_CallFrame_BorrowValue(const BML_CallFrame *frame,
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

    const BML_CallValueType internalType = ToInternalCallValueType(type);
    const BML_CallValue *slot = nullptr;
    const int status = BML_GetCallFrameArgChecked(frame, index, internalType, &slot);
    return status == BML_OK ? BorrowValueData(*slot, type, outData, outCount, outElementSize) : status;
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

BML_EXPORT int BML_CallFrame_SetObjectId(BML_CallFrame *frame, size_t index, int objectId) {
    return GuardInteropMutation([&]() {
        BML_CallValue *slot = BML_EnsureCallFrameArg(frame, index);
        if (!slot)
            return BML_ERROR_INVALID_PARAMETER;
        BML_ResetCallValue(*slot);
        slot->Type = BML_CallValueType::ObjectId;
        slot->IntValue = objectId;
        return BML_OK;
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

BML_EXPORT int BML_CallFrame_SetResultValue(BML_CallFrame *frame,
                                            BML_CALL_VALUE_TYPE type,
                                            const void *data,
                                            size_t count) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    if (!data && count > 0)
        return BML_ERROR_INVALID_PARAMETER;
    if (ToInternalCallValueType(type) == BML_CallValueType::Empty)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        return SetValueData(frame->Result, type, data, count);
    });
}

BML_EXPORT int BML_CallFrame_BorrowResultValue(const BML_CallFrame *frame,
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

    const BML_CallValueType internalType = ToInternalCallValueType(type);
    const BML_CallValue *result = nullptr;
    const int status = BML_GetCallFrameResultChecked(frame, internalType, &result);
    return status == BML_OK ? BorrowValueData(*result, type, outData, outCount, outElementSize) : status;
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

BML_EXPORT int BML_CallFrame_SetResultObjectId(BML_CallFrame *frame, int objectId) {
    if (!frame)
        return BML_ERROR_INVALID_PARAMETER;
    return GuardInteropMutation([&]() {
        BML_ResetCallValue(frame->Result);
        frame->Result.Type = BML_CallValueType::ObjectId;
        frame->Result.IntValue = objectId;
        return BML_OK;
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
