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
    return InteropSignature::ScriptArrayDecl(type);
}

static bool IsScriptArrayType(ScriptExportValueType type) {
    return InteropSignature::IsArrayLike(type);
}

static BML_CALL_VALUE_TYPE CallValueTypeForScriptType(ScriptExportValueType type) {
    return InteropSignature::ToCallValueType(type);
}

template <typename FrameElement, typename ScriptElement>
struct PrimitiveScriptArrayBridge {
    static CKAS_STATUS Fill(void *array,
                            const FrameElement *source,
                            size_t count,
                            const ::CKAngelScriptAdapter::Api *api,
                            ScriptElement (*convert)(FrameElement)) {
        if (!array || !api || !api->ArraySetElementValue)
            return CKAS_INVALIDARGUMENT;
        for (CKDWORD i = 0; i < count; ++i) {
            const ScriptElement value = source ? convert(source[i]) : ScriptElement();
            const CKAS_STATUS status = api->ArraySetElementValue(array, i, &value);
            if (status != CKAS_OK)
                return status;
        }
        return CKAS_OK;
    }

    static int Copy(void *array,
                    std::vector<FrameElement> &values,
                    const ::CKAngelScriptAdapter::Api *api,
                    FrameElement (*convert)(ScriptElement)) {
        if (!array || !api || !api->ArrayGetSize || !api->ArrayGetElementAddress)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;

        CKDWORD count = 0;
        CKAS_STATUS status = api->ArrayGetSize(array, &count);
        if (status != CKAS_OK)
            return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;

        values.resize(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = api->ArrayGetElementAddress(array, i, &address);
            if (status != CKAS_OK)
                return status == CKAS_TYPEMISMATCH ? BML_ERROR_INTEROP_TYPE_MISMATCH : BML_ERROR_INTEROP_BAD_CALL_FRAME;
            const auto *value = static_cast<const ScriptElement *>(address);
            values[i] = value ? convert(*value) : FrameElement();
        }
        return BML_OK;
    }
};

static bool BoolToScript(int value) {
    return value != 0;
}

static int BoolToFrame(bool value) {
    return value ? 1 : 0;
}

static int IntToScript(int value) {
    return value;
}

static int IntToFrame(int value) {
    return value;
}

static float FloatToScript(float value) {
    return value;
}

static float FloatToFrame(float value) {
    return value;
}

static std::uint8_t ByteToScript(std::uint8_t value) {
    return value;
}

static std::uint8_t ByteToFrame(std::uint8_t value) {
    return value;
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
                                     size_t frameIndex) {
    if (!context || !args || !args->Api)
        return CKAS_INVALIDARGUMENT;

    void *array = nullptr;

    if (type == ScriptExportValueType::StringArray) {
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        const int frameStatus = BML_CallFrame_BorrowValue(args->Frame,
                                                          frameIndex,
                                                          BML_CALL_VALUE_STRING_ARRAY,
                                                          &data,
                                                          &count,
                                                          &elementSize);
        if (frameStatus != BML_OK)
            return MapFrameStatus(args, frameStatus);
        if (elementSize != sizeof(const char *))
            return MapFrameStatus(args, BML_ERROR_INTEROP_BAD_CALL_FRAME);

        CKAS_STATUS status = CreateScriptArray(args, type, count, &array);
        if (status != CKAS_OK)
            return status;
        const auto *strings = static_cast<const char *const *>(data);
        for (CKDWORD i = 0; i < count; ++i) {
            const std::string value(strings && strings[i] ? strings[i] : "");
            status = args->Api->ArraySetElementValue(array, i, &value);
            if (status != CKAS_OK) {
                args->Api->ArrayRelease(array);
                return status;
            }
        }
    } else {
        const BML_CALL_VALUE_TYPE frameType = CallValueTypeForScriptType(type);
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        const int frameStatus = BML_CallFrame_BorrowValue(args->Frame,
                                                          frameIndex,
                                                          frameType,
                                                          &data,
                                                          &count,
                                                          &elementSize);
        if (frameStatus != BML_OK)
            return MapFrameStatus(args, frameStatus);

        CKAS_STATUS status = CreateScriptArray(args, type, count, &array);
        if (status != CKAS_OK)
            return status;

        if (type == ScriptExportValueType::BoolArray && elementSize == sizeof(int)) {
            status = PrimitiveScriptArrayBridge<int, bool>::Fill(array,
                                                                 static_cast<const int *>(data),
                                                                 count,
                                                                 args->Api,
                                                                 BoolToScript);
        } else if (type == ScriptExportValueType::IntArray && elementSize == sizeof(int)) {
            status = PrimitiveScriptArrayBridge<int, int>::Fill(array,
                                                                static_cast<const int *>(data),
                                                                count,
                                                                args->Api,
                                                                IntToScript);
        } else if (type == ScriptExportValueType::FloatArray && elementSize == sizeof(float)) {
            status = PrimitiveScriptArrayBridge<float, float>::Fill(array,
                                                                    static_cast<const float *>(data),
                                                                    count,
                                                                    args->Api,
                                                                    FloatToScript);
        } else if (type == ScriptExportValueType::Buffer && elementSize == sizeof(std::uint8_t)) {
            status = PrimitiveScriptArrayBridge<std::uint8_t, std::uint8_t>::Fill(
                array,
                static_cast<const std::uint8_t *>(data),
                count,
                args->Api,
                ByteToScript);
        } else {
            status = CKAS_INVALIDARGUMENT;
        }
        if (status != CKAS_OK) {
            args->Api->ArrayRelease(array);
            return status;
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
        } else if (IsScriptArrayType(parameterType)) {
            const CKAS_STATUS arrayStatus = SetScriptArrayArg(context, args, index, parameterType, i);
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
        std::vector<int> values;
        const int copyStatus = PrimitiveScriptArrayBridge<int, bool>::Copy(array, values, args->Api, BoolToFrame);
        return copyStatus == BML_OK ? BML_CallFrame_SetResultValue(args->Frame,
                                                                   BML_CALL_VALUE_BOOL_ARRAY,
                                                                   values.data(),
                                                                   values.size())
                                    : copyStatus;
    }
    if (type == ScriptExportValueType::IntArray) {
        std::vector<int> values;
        const int copyStatus = PrimitiveScriptArrayBridge<int, int>::Copy(array, values, args->Api, IntToFrame);
        return copyStatus == BML_OK ? BML_CallFrame_SetResultValue(args->Frame,
                                                                   BML_CALL_VALUE_INT_ARRAY,
                                                                   values.data(),
                                                                   values.size())
                                    : copyStatus;
    }
    if (type == ScriptExportValueType::FloatArray) {
        std::vector<float> values;
        const int copyStatus = PrimitiveScriptArrayBridge<float, float>::Copy(array, values, args->Api, FloatToFrame);
        return copyStatus == BML_OK ? BML_CallFrame_SetResultValue(args->Frame,
                                                                   BML_CALL_VALUE_FLOAT_ARRAY,
                                                                   values.data(),
                                                                   values.size())
                                    : copyStatus;
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
        return BML_CallFrame_SetResultValue(args->Frame, BML_CALL_VALUE_STRING_ARRAY, values.data(), values.size());
    }
    if (type == ScriptExportValueType::Buffer) {
        std::vector<std::uint8_t> values;
        const int copyStatus = PrimitiveScriptArrayBridge<std::uint8_t, std::uint8_t>::Copy(array,
                                                                                           values,
                                                                                           args->Api,
                                                                                           ByteToFrame);
        return copyStatus == BML_OK ? BML_CallFrame_SetResultValue(args->Frame,
                                                                   BML_CALL_VALUE_BUFFER,
                                                                   values.data(),
                                                                   values.size())
                                    : copyStatus;
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
    } else if (IsScriptArrayType(returnType)) {
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
    case CKAS_INUSE:
        return BML_ERROR_INTEROP_TARGET_FAILED;
    case CKAS_BUFFERTOOSMALL:
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    default:
        return BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED;
    }
}

static std::string MakeExportCanonicalKey(const std::string &name, const std::string &canonicalSignature) {
    std::string key;
    key.reserve(name.size() + 1 + canonicalSignature.size());
    key += name;
    key.push_back('\0');
    key += canonicalSignature;
    return key;
}

bool ScriptExportTable::Cache(CKContext *context,
                              ScriptModRuntime &runtime,
                              const std::vector<ScriptModExportDefinition> &definitions,
                              ScriptDiagnostic &diagnostic,
                              bool publishChange) {
    auto rollback = [&]() {
        Release(context, runtime, nullptr, false);
    };

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
            rollback();
            return false;
        }
        binding.Method = runtime.FindMethod(context, binding.MethodDecl.c_str(), diagnostic, false);
        if (!binding.Method) {
            diagnostic.Phase = ScriptDiagnosticPhase::ExportLookup;
            rollback();
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
            rollback();
            return false;
        }

        m_Exports.push_back(binding);
    }
    std::sort(m_Exports.begin(), m_Exports.end(), [](const ScriptExportBinding &left, const ScriptExportBinding &right) {
        if (left.Name == right.Name)
            return left.Signature < right.Signature;
        return left.Name < right.Name;
    });
    RebuildIndex();
    if (publishChange)
        ExportRegistry::NotifyScriptExportsChanged();
    return true;
}

bool ScriptExportTable::Release(CKContext *context,
                                ScriptModRuntime &runtime,
                                ScriptDiagnostic *diagnostic,
                                bool publishChange) {
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
    m_ExportsByName.clear();
    m_ExportsByCanonicalSignature.clear();
    if (publishChange)
        ExportRegistry::NotifyScriptExportsChanged();
    if (!ok && diagnostic)
        *diagnostic = firstFailure;
    return ok;
}

bool ScriptExportTable::HasExport(const std::string &name, const std::string &signature) const {
    return Resolve(name, signature) != nullptr;
}

const ScriptExportBinding *ScriptExportTable::Resolve(const std::string &name, const std::string &signature) const {
    auto byName = m_ExportsByName.find(name);
    if (byName == m_ExportsByName.end())
        return nullptr;

    if (!signature.empty()) {
        ScriptExportSignatureInfo requestedSignature;
        if (InteropSignature::Validate(signature, name, &requestedSignature) != BML_OK)
            return nullptr;

        auto bySignature = m_ExportsByCanonicalSignature.find(
            MakeExportCanonicalKey(name, requestedSignature.CanonicalSignature));
        return bySignature == m_ExportsByCanonicalSignature.end()
                   ? nullptr
                   : &m_Exports[bySignature->second];
    }

    return byName->second.size() == 1 ? &m_Exports[byName->second.front()] : nullptr;
}

std::string ScriptExportTable::GetSignature(const std::string &name) const {
    std::string signatures;
    auto byName = m_ExportsByName.find(name);
    if (byName == m_ExportsByName.end())
        return signatures;

    for (size_t index : byName->second) {
        const ScriptExportBinding &binding = m_Exports[index];
        if (!signatures.empty())
            signatures += "; ";
        signatures += binding.Signature;
    }
    return signatures;
}

void ScriptExportTable::GetSignatures(const std::string &name, std::vector<std::string> &out) const {
    auto byName = m_ExportsByName.find(name);
    if (byName == m_ExportsByName.end())
        return;

    out.reserve(out.size() + byName->second.size());
    for (size_t index : byName->second)
        out.push_back(m_Exports[index].Signature);
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

void ScriptExportTable::Clear(bool publishChange) {
    m_Exports.clear();
    m_ExportsByName.clear();
    m_ExportsByCanonicalSignature.clear();
    if (publishChange)
        ExportRegistry::NotifyScriptExportsChanged();
}

void ScriptExportTable::RebuildIndex() {
    m_ExportsByName.clear();
    m_ExportsByCanonicalSignature.clear();
    m_ExportsByName.reserve(m_Exports.size());
    m_ExportsByCanonicalSignature.reserve(m_Exports.size());

    for (size_t i = 0; i < m_Exports.size(); ++i) {
        const ScriptExportBinding &binding = m_Exports[i];
        m_ExportsByName[binding.Name].push_back(i);
        m_ExportsByCanonicalSignature.emplace(
            MakeExportCanonicalKey(binding.Name, binding.SignatureInfo.CanonicalSignature),
            i);
    }
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
    call.ConfigureContext = ConfigureFrameExportContext;
    call.ReadContextResult = binding.SignatureInfo.ReturnType == ScriptExportValueType::Void
                                 ? nullptr
                                 : ReadFrameExportContextResult;
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
