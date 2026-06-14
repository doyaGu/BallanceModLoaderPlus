#include "ScriptExportDispatcher.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "BML/Defines.h"
#include "CallFrameInternal.h"
#include "ExportRegistry.h"

namespace BML {

struct StringExportCallArgs {
    bool HasArgument = false;
    const std::string *Argument = nullptr;
    std::string *Result = nullptr;
    ::CKAngelScriptAdapter::ArgSetStringFn SetString = nullptr;
    ::CKAngelScriptAdapter::ResultGetStringViewFn GetStringView = nullptr;
};

struct FrameExportCallArgs {
    const ScriptExportSignatureInfo *Signature = nullptr;
    BML_CallFrame *Frame = nullptr;
    CKContext *Context = nullptr;
    CKAngelScript *AngelScript = nullptr;
    const ::CKAngelScriptAdapter::Api *Api = nullptr;
    int FailureStatus = BML_OK;
    std::vector<std::string> StringArgs;
    std::vector<void *> ArrayArgs;

    ~FrameExportCallArgs() {
        if (!Api || !Api->ArrayRelease)
            return;
        for (void *array : ArrayArgs) {
            if (array)
                Api->ArrayRelease(array);
        }
    }
};

static bool IsInteropV2Type(ScriptExportValueType type) {
    return type == ScriptExportValueType::BoolArray ||
           type == ScriptExportValueType::IntArray ||
           type == ScriptExportValueType::FloatArray ||
           type == ScriptExportValueType::StringArray ||
           type == ScriptExportValueType::Buffer ||
           type == ScriptExportValueType::ObjectId;
}

static bool UsesInteropV2Types(const ScriptExportSignatureInfo &signature) {
    if (IsInteropV2Type(signature.ReturnType))
        return true;
    for (ScriptExportValueType type : signature.ParameterTypes) {
        if (IsInteropV2Type(type))
            return true;
    }
    return false;
}

static CKAS_STATUS MapFrameStatus(FrameExportCallArgs *args, int status) {
    if (status == BML_ERROR_INTEROP_TYPE_MISMATCH) {
        if (args)
            args->FailureStatus = BML_ERROR_INTEROP_TYPE_MISMATCH;
        return CKAS_TYPEMISMATCH;
    }
    if (status == BML_ERROR_INTEROP_HANDLE_STALE) {
        if (args)
            args->FailureStatus = BML_ERROR_INTEROP_HANDLE_STALE;
        return CKAS_STALEHANDLE;
    }
    if (args)
        args->FailureStatus = BML_ERROR_INTEROP_BAD_CALL_FRAME;
    return CKAS_INVALIDARGUMENT;
}

static const char *ArrayDeclForType(ScriptExportValueType type) {
    switch (type) {
    case ScriptExportValueType::BoolArray:
        return "array<bool>";
    case ScriptExportValueType::IntArray:
        return "array<int>";
    case ScriptExportValueType::FloatArray:
        return "array<float>";
    case ScriptExportValueType::StringArray:
        return "array<string>";
    case ScriptExportValueType::Buffer:
        return "array<uint8>";
    default:
        return nullptr;
    }
}

static CKAS_STATUS CreateScriptArray(FrameExportCallArgs *args,
                                     ScriptExportValueType type,
                                     size_t count,
                                     void **outArray) {
    if (outArray)
        *outArray = nullptr;
    if (!args || !args->Api || !args->Api->CreateArray || !args->AngelScript || !outArray)
        return CKAS_INVALIDARGUMENT;
    const char *decl = ArrayDeclForType(type);
    if (!decl)
        return CKAS_TYPEMISMATCH;
    return args->Api->CreateArray(args->AngelScript, decl, static_cast<CKDWORD>(count), outArray);
}

static CKAS_STATUS SetScriptArrayArg(asIScriptContext *context,
                                     FrameExportCallArgs *args,
                                     CKDWORD index,
                                     ScriptExportValueType type,
                                     const BML_CallValue &slot) {
    if (!context || !args || !args->Api)
        return CKAS_INVALIDARGUMENT;
    void *array = nullptr;
    if (type == ScriptExportValueType::BoolArray || type == ScriptExportValueType::IntArray) {
        CKAS_STATUS status = CreateScriptArray(args, type, slot.IntArrayValue.size(), &array);
        if (status != CKAS_OK)
            return status;
        for (CKDWORD i = 0; i < slot.IntArrayValue.size(); ++i) {
            if (type == ScriptExportValueType::BoolArray) {
                const bool value = slot.IntArrayValue[i] != 0;
                status = args->Api->ArraySetElementValue(array, i, &value);
            } else {
                const int value = slot.IntArrayValue[i];
                status = args->Api->ArraySetElementValue(array, i, &value);
            }
            if (status != CKAS_OK) {
                args->Api->ArrayRelease(array);
                return status;
            }
        }
    } else if (type == ScriptExportValueType::FloatArray) {
        CKAS_STATUS status = CreateScriptArray(args, type, slot.FloatArrayValue.size(), &array);
        if (status != CKAS_OK)
            return status;
        for (CKDWORD i = 0; i < slot.FloatArrayValue.size(); ++i) {
            const float value = slot.FloatArrayValue[i];
            status = args->Api->ArraySetElementValue(array, i, &value);
            if (status != CKAS_OK) {
                args->Api->ArrayRelease(array);
                return status;
            }
        }
    } else if (type == ScriptExportValueType::StringArray) {
        CKAS_STATUS status = CreateScriptArray(args, type, slot.StringArrayValue.size(), &array);
        if (status != CKAS_OK)
            return status;
        for (CKDWORD i = 0; i < slot.StringArrayValue.size(); ++i) {
            const std::string &value = slot.StringArrayValue[i];
            status = args->Api->ArraySetElementValue(array, i, &value);
            if (status != CKAS_OK) {
                args->Api->ArrayRelease(array);
                return status;
            }
        }
    } else if (type == ScriptExportValueType::Buffer) {
        CKAS_STATUS status = CreateScriptArray(args, type, slot.BufferValue.size(), &array);
        if (status != CKAS_OK)
            return status;
        for (CKDWORD i = 0; i < slot.BufferValue.size(); ++i) {
            const std::uint8_t value = slot.BufferValue[i];
            status = args->Api->ArraySetElementValue(array, i, &value);
            if (status != CKAS_OK) {
                args->Api->ArrayRelease(array);
                return status;
            }
        }
    }

    if (!array)
        return CKAS_TYPEMISMATCH;

    const int status = context->SetArgObject(static_cast<asUINT>(index), array);
    if (status < 0) {
        args->Api->ArrayRelease(array);
        return CKAS_TYPEMISMATCH;
    }
    args->ArrayArgs.push_back(array);
    return CKAS_OK;
}

static CKAS_STATUS WriteStringExportArgs(CKAngelScriptArgWriter *writer, void *userData) {
    auto *args = static_cast<StringExportCallArgs *>(userData);
    if (!args || !args->HasArgument || !args->Argument || !args->SetString)
        return CKAS_INVALIDARGUMENT;
    return args->SetString(writer, 0, args->Argument->c_str());
}

static CKAS_STATUS ReadStringExportResult(CKAngelScriptResultReader *reader, void *userData) {
    auto *args = static_cast<StringExportCallArgs *>(userData);
    if (!args || !args->Result || !args->GetStringView)
        return CKAS_INVALIDARGUMENT;

    const char *data = nullptr;
    size_t size = 0;
    const CKAS_STATUS status = args->GetStringView(reader, &data, &size);
    if (status != CKAS_OK)
        return status;
    args->Result->assign(data ? data : "", size);
    return CKAS_OK;
}

static CKAS_STATUS WriteFrameExportArgs(CKAngelScriptArgWriter *writer, void *userData) {
    auto *args = static_cast<FrameExportCallArgs *>(userData);
    if (!args || !args->Signature || !args->Frame || !args->Api)
        return CKAS_INVALIDARGUMENT;

    if (args->Signature->ParameterTypes.empty())
        return CKAS_OK;

    for (size_t i = 0; i < args->Signature->ParameterTypes.size(); ++i) {
        const ScriptExportValueType parameterType = args->Signature->ParameterTypes[i];
        const CKDWORD index = static_cast<CKDWORD>(i);

        CKAS_STATUS status = CKAS_OK;
        if (parameterType == ScriptExportValueType::Bool) {
            int value = 0;
            const int frameStatus = BML_CallFrame_GetBool(args->Frame, i, &value);
            if (frameStatus != BML_OK) {
                args->FailureStatus = frameStatus == BML_ERROR_INTEROP_TYPE_MISMATCH
                                          ? BML_ERROR_INTEROP_TYPE_MISMATCH
                                          : BML_ERROR_INTEROP_BAD_CALL_FRAME;
                return CKAS_INVALIDARGUMENT;
            }
            status = args->Api->ArgSetBool(writer, index, value ? TRUE : FALSE);
        } else if (parameterType == ScriptExportValueType::Int) {
            int value = 0;
            const int frameStatus = BML_CallFrame_GetInt(args->Frame, i, &value);
            if (frameStatus != BML_OK) {
                args->FailureStatus = frameStatus == BML_ERROR_INTEROP_TYPE_MISMATCH
                                          ? BML_ERROR_INTEROP_TYPE_MISMATCH
                                          : BML_ERROR_INTEROP_BAD_CALL_FRAME;
                return CKAS_INVALIDARGUMENT;
            }
            status = args->Api->ArgSetInt(writer, index, value);
        } else if (parameterType == ScriptExportValueType::Float) {
            float value = 0.0f;
            const int frameStatus = BML_CallFrame_GetFloat(args->Frame, i, &value);
            if (frameStatus != BML_OK) {
                args->FailureStatus = frameStatus == BML_ERROR_INTEROP_TYPE_MISMATCH
                                          ? BML_ERROR_INTEROP_TYPE_MISMATCH
                                          : BML_ERROR_INTEROP_BAD_CALL_FRAME;
                return CKAS_INVALIDARGUMENT;
            }
            status = args->Api->ArgSetFloat(writer, index, value);
        } else if (parameterType == ScriptExportValueType::String) {
            const BML_CallValue *slot = nullptr;
            const int frameStatus = BML_GetCallFrameArgChecked(args->Frame, i, BML_CallValueType::String, &slot);
            if (frameStatus != BML_OK) {
                args->FailureStatus = frameStatus == BML_ERROR_INTEROP_TYPE_MISMATCH
                                          ? BML_ERROR_INTEROP_TYPE_MISMATCH
                                          : BML_ERROR_INTEROP_BAD_CALL_FRAME;
                return CKAS_INVALIDARGUMENT;
            }
            status = args->Api->ArgSetString(writer, index, slot->StringValue.c_str());
        } else {
            args->FailureStatus = BML_ERROR_INTEROP_TYPE_MISMATCH;
            return CKAS_TYPEMISMATCH;
        }

        if (status != CKAS_OK) {
            args->FailureStatus = BML_ERROR_INTEROP_BAD_CALL_FRAME;
            return status;
        }
    }

    return CKAS_OK;
}

static CKAS_STATUS ReadFrameExportResult(CKAngelScriptResultReader *reader, void *userData) {
    auto *args = static_cast<FrameExportCallArgs *>(userData);
    if (!args || !args->Signature || !args->Frame || !args->Api)
        return CKAS_INVALIDARGUMENT;

    const ScriptExportValueType returnType = args->Signature->ReturnType;
    if (returnType == ScriptExportValueType::Void)
        return CKAS_OK;

    if (returnType == ScriptExportValueType::Bool) {
        CKBOOL value = FALSE;
        const CKAS_STATUS status = args->Api->ResultGetBool(reader, &value);
        const int frameStatus = status == CKAS_OK ? BML_CallFrame_SetResultBool(args->Frame, value != 0) : BML_OK;
        if (status == CKAS_OK && frameStatus != BML_OK)
            args->FailureStatus = BML_ERROR_INTEROP_BAD_CALL_FRAME;
        return status == CKAS_OK ? (frameStatus == BML_OK ? CKAS_OK : CKAS_INVALIDARGUMENT) : status;
    }
    if (returnType == ScriptExportValueType::Int) {
        int value = 0;
        const CKAS_STATUS status = args->Api->ResultGetInt(reader, &value);
        const int frameStatus = status == CKAS_OK ? BML_CallFrame_SetResultInt(args->Frame, value) : BML_OK;
        if (status == CKAS_OK && frameStatus != BML_OK)
            args->FailureStatus = BML_ERROR_INTEROP_BAD_CALL_FRAME;
        return status == CKAS_OK ? (frameStatus == BML_OK ? CKAS_OK : CKAS_INVALIDARGUMENT) : status;
    }
    if (returnType == ScriptExportValueType::Float) {
        float value = 0.0f;
        const CKAS_STATUS status = args->Api->ResultGetFloat(reader, &value);
        const int frameStatus = status == CKAS_OK ? BML_CallFrame_SetResultFloat(args->Frame, value) : BML_OK;
        if (status == CKAS_OK && frameStatus != BML_OK)
            args->FailureStatus = BML_ERROR_INTEROP_BAD_CALL_FRAME;
        return status == CKAS_OK ? (frameStatus == BML_OK ? CKAS_OK : CKAS_INVALIDARGUMENT) : status;
    }
    if (returnType == ScriptExportValueType::String) {
        const char *data = nullptr;
        size_t size = 0;
        const CKAS_STATUS status = args->Api->ResultGetStringView(reader, &data, &size);
        if (status != CKAS_OK)
            return status;
        if (!args->Frame) {
            args->FailureStatus = BML_ERROR_INTEROP_BAD_CALL_FRAME;
            return CKAS_INVALIDARGUMENT;
        }
        args->Frame->Result = BML_CallValue();
        args->Frame->Result.Type = BML_CallValueType::String;
        args->Frame->Result.StringValue.assign(data ? data : "", size);
        return CKAS_OK;
    }

    args->FailureStatus = BML_ERROR_INTEROP_TYPE_MISMATCH;
    return CKAS_TYPEMISMATCH;
}

static CKAS_STATUS ConfigureFrameExportContext(asIScriptContext *context, void *userData) {
    auto *args = static_cast<FrameExportCallArgs *>(userData);
    if (!context || !args || !args->Signature || !args->Frame)
        return CKAS_INVALIDARGUMENT;

    args->StringArgs.reserve(args->Signature->ParameterTypes.size());
    args->ArrayArgs.reserve(args->Signature->ParameterTypes.size());

    for (size_t i = 0; i < args->Signature->ParameterTypes.size(); ++i) {
        const ScriptExportValueType parameterType = args->Signature->ParameterTypes[i];
        const CKDWORD index = static_cast<CKDWORD>(i);
        int result = 0;

        if (parameterType == ScriptExportValueType::Bool) {
            int value = 0;
            const int status = BML_CallFrame_GetBool(args->Frame, i, &value);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            result = context->SetArgByte(index, value ? 1 : 0);
        } else if (parameterType == ScriptExportValueType::Int) {
            int value = 0;
            const int status = BML_CallFrame_GetInt(args->Frame, i, &value);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            result = context->SetArgDWord(index, static_cast<asDWORD>(value));
        } else if (parameterType == ScriptExportValueType::Float) {
            float value = 0.0f;
            const int status = BML_CallFrame_GetFloat(args->Frame, i, &value);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            result = context->SetArgFloat(index, value);
        } else if (parameterType == ScriptExportValueType::String) {
            const BML_CallValue *slot = nullptr;
            const int status = BML_GetCallFrameArgChecked(args->Frame, i, BML_CallValueType::String, &slot);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            args->StringArgs.push_back(slot->StringValue);
            result = context->SetArgObject(index, &args->StringArgs.back());
        } else if (parameterType == ScriptExportValueType::BoolArray) {
            const BML_CallValue *slot = nullptr;
            const int status = BML_GetCallFrameArgChecked(args->Frame, i, BML_CallValueType::BoolArray, &slot);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            const CKAS_STATUS arrayStatus = SetScriptArrayArg(context, args, index, parameterType, *slot);
            if (arrayStatus != CKAS_OK)
                return arrayStatus;
            continue;
        } else if (parameterType == ScriptExportValueType::IntArray) {
            const BML_CallValue *slot = nullptr;
            const int status = BML_GetCallFrameArgChecked(args->Frame, i, BML_CallValueType::IntArray, &slot);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            const CKAS_STATUS arrayStatus = SetScriptArrayArg(context, args, index, parameterType, *slot);
            if (arrayStatus != CKAS_OK)
                return arrayStatus;
            continue;
        } else if (parameterType == ScriptExportValueType::FloatArray) {
            const BML_CallValue *slot = nullptr;
            const int status = BML_GetCallFrameArgChecked(args->Frame, i, BML_CallValueType::FloatArray, &slot);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            const CKAS_STATUS arrayStatus = SetScriptArrayArg(context, args, index, parameterType, *slot);
            if (arrayStatus != CKAS_OK)
                return arrayStatus;
            continue;
        } else if (parameterType == ScriptExportValueType::StringArray) {
            const BML_CallValue *slot = nullptr;
            const int status = BML_GetCallFrameArgChecked(args->Frame, i, BML_CallValueType::StringArray, &slot);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            const CKAS_STATUS arrayStatus = SetScriptArrayArg(context, args, index, parameterType, *slot);
            if (arrayStatus != CKAS_OK)
                return arrayStatus;
            continue;
        } else if (parameterType == ScriptExportValueType::Buffer) {
            const BML_CallValue *slot = nullptr;
            const int status = BML_GetCallFrameArgChecked(args->Frame, i, BML_CallValueType::Buffer, &slot);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            const CKAS_STATUS arrayStatus = SetScriptArrayArg(context, args, index, parameterType, *slot);
            if (arrayStatus != CKAS_OK)
                return arrayStatus;
            continue;
        } else if (parameterType == ScriptExportValueType::ObjectId) {
            int objectId = 0;
            const int status = BML_CallFrame_GetObjectId(args->Frame, i, &objectId);
            if (status != BML_OK)
                return MapFrameStatus(args, status);
            CKObject *object = nullptr;
            if (objectId != 0) {
                object = args->Context ? args->Context->GetObject(static_cast<CK_ID>(objectId)) : nullptr;
                if (!object)
                    return MapFrameStatus(args, BML_ERROR_INTEROP_HANDLE_STALE);
            }
            result = context->SetArgObject(index, object);
        } else {
            args->FailureStatus = BML_ERROR_INTEROP_TYPE_MISMATCH;
            return CKAS_TYPEMISMATCH;
        }

        if (result < 0) {
            args->FailureStatus = BML_ERROR_INTEROP_BAD_CALL_FRAME;
            return CKAS_TYPEMISMATCH;
        }
    }

    return CKAS_OK;
}

static int SetFrameResultFromScriptArray(FrameExportCallArgs *args,
                                         ScriptExportValueType type,
                                         void *array) {
    if (!args || !args->Frame || !args->Api || !array)
        return BML_ERROR_INTEROP_TYPE_MISMATCH;

    CKDWORD count = 0;
    CKAS_STATUS status = args->Api->ArrayGetSize(array, &count);
    if (status != CKAS_OK)
        return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;

    if (type == ScriptExportValueType::BoolArray) {
        std::vector<int> values(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = args->Api->ArrayGetElementAddress(array, i, &address);
            if (status != CKAS_OK)
                return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;
            values[i] = address && *static_cast<bool *>(address) ? 1 : 0;
        }
        return BML_CallFrame_SetResultBoolArray(args->Frame, values.data(), values.size());
    }
    if (type == ScriptExportValueType::IntArray) {
        std::vector<int> values(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = args->Api->ArrayGetElementAddress(array, i, &address);
            if (status != CKAS_OK)
                return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;
            values[i] = address ? *static_cast<int *>(address) : 0;
        }
        return BML_CallFrame_SetResultIntArray(args->Frame, values.data(), values.size());
    }
    if (type == ScriptExportValueType::FloatArray) {
        std::vector<float> values(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = args->Api->ArrayGetElementAddress(array, i, &address);
            if (status != CKAS_OK)
                return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;
            values[i] = address ? *static_cast<float *>(address) : 0.0f;
        }
        return BML_CallFrame_SetResultFloatArray(args->Frame, values.data(), values.size());
    }
    if (type == ScriptExportValueType::StringArray) {
        std::vector<const char *> values(count);
        std::vector<std::string> storage(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = args->Api->ArrayGetElementAddress(array, i, &address);
            if (status != CKAS_OK)
                return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;
            const auto *value = static_cast<const std::string *>(address);
            storage[i] = value ? *value : "";
            values[i] = storage[i].c_str();
        }
        return BML_CallFrame_SetResultStringArray(args->Frame, values.data(), values.size());
    }
    if (type == ScriptExportValueType::Buffer) {
        std::vector<std::uint8_t> values(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = args->Api->ArrayGetElementAddress(array, i, &address);
            if (status != CKAS_OK)
                return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;
            values[i] = address ? *static_cast<std::uint8_t *>(address) : 0;
        }
        return BML_CallFrame_SetResultBuffer(args->Frame, values.data(), values.size());
    }
    return BML_ERROR_INTEROP_TYPE_MISMATCH;
}

static CKAS_STATUS ReadFrameExportContextResult(asIScriptContext *context, void *userData) {
    auto *args = static_cast<FrameExportCallArgs *>(userData);
    if (!context || !args || !args->Signature || !args->Frame)
        return CKAS_INVALIDARGUMENT;

    const ScriptExportValueType returnType = args->Signature->ReturnType;
    int frameStatus = BML_OK;
    if (returnType == ScriptExportValueType::Void) {
        return CKAS_OK;
    }
    if (returnType == ScriptExportValueType::Bool) {
        frameStatus = BML_CallFrame_SetResultBool(args->Frame, context->GetReturnByte() ? 1 : 0);
    } else if (returnType == ScriptExportValueType::Int) {
        frameStatus = BML_CallFrame_SetResultInt(args->Frame, static_cast<int>(context->GetReturnDWord()));
    } else if (returnType == ScriptExportValueType::Float) {
        frameStatus = BML_CallFrame_SetResultFloat(args->Frame, context->GetReturnFloat());
    } else if (returnType == ScriptExportValueType::String) {
        const auto *value = static_cast<const std::string *>(context->GetReturnObject());
        frameStatus = value ? BML_CallFrame_SetResultString(args->Frame, value->c_str())
                            : BML_ERROR_INTEROP_TYPE_MISMATCH;
    } else if (returnType == ScriptExportValueType::BoolArray ||
               returnType == ScriptExportValueType::IntArray ||
               returnType == ScriptExportValueType::FloatArray ||
               returnType == ScriptExportValueType::StringArray ||
               returnType == ScriptExportValueType::Buffer) {
        void *array = context->GetReturnObject();
        frameStatus = SetFrameResultFromScriptArray(args, returnType, array);
    } else if (returnType == ScriptExportValueType::ObjectId) {
        auto *object = static_cast<CKObject *>(context->GetReturnObject());
        frameStatus = BML_CallFrame_SetResultObjectId(args->Frame, object ? static_cast<int>(object->GetID()) : 0);
    } else {
        frameStatus = BML_ERROR_INTEROP_TYPE_MISMATCH;
    }

    if (frameStatus == BML_OK)
        return CKAS_OK;
    return MapFrameStatus(args, frameStatus);
}

static int MapExportCallStatus(const ScriptDiagnostic &diagnostic) {
    switch (diagnostic.Status) {
    case CKAS_OK:
        return BML_OK;
    case CKAS_NOTFOUND:
    case CKAS_STALEHANDLE:
        return BML_ERROR_INTEROP_HANDLE_STALE;
    case CKAS_INVALIDARGUMENT:
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    case CKAS_TYPEMISMATCH:
        return BML_ERROR_INTEROP_TYPE_MISMATCH;
    case CKAS_UNSUPPORTED:
        return BML_ERROR_INTEROP_UNSUPPORTED;
    case CKAS_BUFFERTOOSMALL:
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    default:
        return BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED;
    }
}

bool ScriptExportTable::Cache(CKContext *context,
                              ScriptModRuntime &runtime,
                              const std::vector<ScriptModExportDefinition> &definitions,
                              ScriptDiagnostic &diagnostic) {
    for (const ScriptModExportDefinition &exportInfo : definitions) {
        ScriptExportBinding binding;
        binding.Name = exportInfo.Name;
        binding.Signature = exportInfo.Signature;
        binding.MethodDecl = ScriptExportSignature::BuildMethodDecl(exportInfo);
        const int signatureStatus = InteropSignature::Validate(binding.Signature,
                                                               binding.Name,
                                                               &binding.SignatureInfo);
        if (signatureStatus != BML_OK) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                              "Unsupported script export signature '" +
                                              binding.Signature + "'.");
            return false;
        }
        binding.Method = runtime.FindMethod(context, binding.MethodDecl.c_str(), diagnostic, false);
        if (!binding.Method) {
            diagnostic.Phase = ScriptDiagnosticPhase::ExportLookup;
            return false;
        }

        const bool duplicate = std::any_of(m_Exports.begin(), m_Exports.end(), [&binding](const ScriptExportBinding &existing) {
            return existing.Name == binding.Name &&
                   InteropSignature::Equivalent(existing.SignatureInfo, binding.SignatureInfo);
        });
        if (duplicate) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                              "Duplicate script export '" + binding.Name +
                                              "' with signature '" + binding.Signature + "'.");
            runtime.ReleaseMethod(context, binding.Method, nullptr);
            return false;
        }

        m_Exports.push_back(binding);
    }
    std::sort(m_Exports.begin(), m_Exports.end(), [](const ScriptExportBinding &left, const ScriptExportBinding &right) {
        if (left.Name == right.Name)
            return left.Signature < right.Signature;
        return left.Name < right.Name;
    });
    ExportRegistry::NotifyScriptExportsChanged();
    return true;
}

bool ScriptExportTable::Release(CKContext *context, ScriptModRuntime &runtime, ScriptDiagnostic *diagnostic) {
    bool ok = true;
    ScriptDiagnostic firstFailure;
    for (auto &entry : m_Exports) {
        ScriptDiagnostic releaseDiagnostic;
        if (!runtime.ReleaseMethod(context, entry.Method, &releaseDiagnostic)) {
            if (ok)
                firstFailure = releaseDiagnostic;
            ok = false;
        }
    }
    m_Exports.clear();
    ExportRegistry::NotifyScriptExportsChanged();
    if (!ok && diagnostic)
        *diagnostic = firstFailure;
    return ok;
}

bool ScriptExportTable::HasExport(const std::string &name, const std::string &signature) const {
    return Resolve(name, signature) != nullptr;
}

const ScriptExportBinding *ScriptExportTable::Resolve(const std::string &name, const std::string &signature) const {
    ScriptExportSignatureInfo requestedSignature;
    if (!signature.empty() &&
        InteropSignature::Validate(signature, name, &requestedSignature) != BML_OK) {
        return nullptr;
    }

    const ScriptExportBinding *match = nullptr;
    for (const ScriptExportBinding &binding : m_Exports) {
        if (binding.Name != name)
            continue;
        if (!signature.empty()) {
            if (InteropSignature::Equivalent(binding.SignatureInfo, requestedSignature))
                return &binding;
            continue;
        }
        if (match)
            return nullptr;
        match = &binding;
    }
    return match;
}

std::string ScriptExportTable::GetSignature(const std::string &name) const {
    std::string signatures;
    for (const ScriptExportBinding &binding : m_Exports) {
        if (binding.Name != name)
            continue;
        if (!signatures.empty())
            signatures += "; ";
        signatures += binding.Signature;
    }
    return signatures;
}

int ScriptExportTable::GetCount() const {
    return static_cast<int>(m_Exports.size());
}

bool ScriptExportTable::GetInfo(int index, std::string &name, std::string &signature) const {
    if (index < 0 || index >= static_cast<int>(m_Exports.size()))
        return false;

    const ScriptExportBinding &binding = m_Exports[static_cast<size_t>(index)];
    name = binding.Name;
    signature = binding.Signature;
    return true;
}

void ScriptExportTable::Clear() {
    m_Exports.clear();
    ExportRegistry::NotifyScriptExportsChanged();
}

int ScriptExportDispatcher::CallVoid(CKContext *context,
                                     ScriptModRuntime &runtime,
                                     const ScriptExportBinding &binding,
                                     ScriptDiagnostic &diagnostic) {
    ScriptMethodCall call;
    call.Method = binding.Method;
    call.Phase = ScriptDiagnosticPhase::ExportCall;
    call.FailurePrefix = "Export call failed";
    if (runtime.CallMethod(context, call, diagnostic)) {
        return BML_OK;
    }
    return MapExportCallStatus(diagnostic);
}

int ScriptExportDispatcher::CallString(CKContext *context,
                                       ScriptModRuntime &runtime,
                                       const ScriptExportBinding &binding,
                                       const std::string &argument,
                                       bool hasArgument,
                                       std::string &out,
                                       ScriptDiagnostic &diagnostic) {
    const ::CKAngelScriptAdapter::Api &api = runtime.GetApi();
    StringExportCallArgs callArgs;
    callArgs.HasArgument = hasArgument;
    callArgs.Argument = &argument;
    callArgs.Result = &out;
    callArgs.SetString = api.ArgSetString;
    callArgs.GetStringView = api.ResultGetStringView;

    ScriptMethodCall call;
    call.Method = binding.Method;
    call.WriteArgs = hasArgument ? WriteStringExportArgs : nullptr;
    call.ReadResult = ReadStringExportResult;
    call.UserData = &callArgs;
    call.Phase = ScriptDiagnosticPhase::ExportCall;
    call.FailurePrefix = "Export call failed";
    if (runtime.CallMethod(context, call, diagnostic)) {
        return BML_OK;
    }
    return MapExportCallStatus(diagnostic);
}

int ScriptExportDispatcher::CallFrame(CKContext *context,
                                      ScriptModRuntime &runtime,
                                      const ScriptExportBinding &binding,
                                      BML_CallFrame *frame,
                                      ScriptDiagnostic &diagnostic) {
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;

    FrameExportCallArgs callArgs;
    callArgs.Signature = &binding.SignatureInfo;
    callArgs.Frame = frame;
    callArgs.Context = context;
    callArgs.AngelScript = runtime.GetAngelScript();
    callArgs.Api = &runtime.GetApi();

    ScriptMethodCall call;
    call.Method = binding.Method;
    if (UsesInteropV2Types(binding.SignatureInfo)) {
        call.ConfigureContext = ConfigureFrameExportContext;
        call.ReadContextResult = binding.SignatureInfo.ReturnType == ScriptExportValueType::Void
                                     ? nullptr
                                     : ReadFrameExportContextResult;
    } else {
        call.WriteArgs = binding.SignatureInfo.ParameterTypes.empty() ? nullptr : WriteFrameExportArgs;
        call.ReadResult = binding.SignatureInfo.ReturnType == ScriptExportValueType::Void ? nullptr : ReadFrameExportResult;
    }
    call.UserData = &callArgs;
    call.Phase = ScriptDiagnosticPhase::ExportCall;
    call.FailurePrefix = "Export call failed";
    if (runtime.CallMethod(context, call, diagnostic)) {
        return BML_OK;
    }
    if (callArgs.FailureStatus != BML_OK)
        return callArgs.FailureStatus;
    return MapExportCallStatus(diagnostic);
}

} // namespace BML
