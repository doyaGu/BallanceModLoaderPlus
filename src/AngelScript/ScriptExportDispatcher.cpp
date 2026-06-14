#include "ScriptExportDispatcher.h"

#include <algorithm>

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
    const ::CKAngelScriptAdapter::Api *Api = nullptr;
    int FailureStatus = BML_OK;
};

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
    case CKAS_UNSUPPORTED:
        return BML_ERROR_INTEROP_TYPE_MISMATCH;
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
        if (m_Exports.find(exportInfo.Name) != m_Exports.end()) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                              "Duplicate script export '" + exportInfo.Name + "'.");
            return false;
        }

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

        m_Order.push_back(binding.Name);
        m_Exports.emplace(binding.Name, binding);
    }
    std::sort(m_Order.begin(), m_Order.end(), [this](const std::string &leftName, const std::string &rightName) {
        const ScriptExportBinding *left = Find(leftName);
        const ScriptExportBinding *right = Find(rightName);
        if (!left || !right)
            return leftName < rightName;
        if (left->Name == right->Name)
            return left->Signature < right->Signature;
        return left->Name < right->Name;
    });
    ExportRegistry::NotifyScriptExportsChanged();
    return true;
}

bool ScriptExportTable::Release(CKContext *context, ScriptModRuntime &runtime, ScriptDiagnostic *diagnostic) {
    bool ok = true;
    ScriptDiagnostic firstFailure;
    for (auto &entry : m_Exports) {
        ScriptDiagnostic releaseDiagnostic;
        if (!runtime.ReleaseMethod(context, entry.second.Method, &releaseDiagnostic)) {
            if (ok)
                firstFailure = releaseDiagnostic;
            ok = false;
        }
    }
    m_Exports.clear();
    m_Order.clear();
    ExportRegistry::NotifyScriptExportsChanged();
    if (!ok && diagnostic)
        *diagnostic = firstFailure;
    return ok;
}

bool ScriptExportTable::HasExport(const std::string &name, const std::string &signature) const {
    return Resolve(name, signature) != nullptr;
}

const ScriptExportBinding *ScriptExportTable::Resolve(const std::string &name, const std::string &signature) const {
    const ScriptExportBinding *binding = Find(name);
    if (!binding || (!signature.empty() && binding->Signature != signature))
        return nullptr;
    return binding;
}

const ScriptExportBinding *ScriptExportTable::Find(const std::string &name) const {
    auto it = m_Exports.find(name);
    return it == m_Exports.end() ? nullptr : &it->second;
}

std::string ScriptExportTable::GetSignature(const std::string &name) const {
    const ScriptExportBinding *binding = Find(name);
    return binding ? binding->Signature : std::string();
}

int ScriptExportTable::GetCount() const {
    return static_cast<int>(m_Exports.size());
}

bool ScriptExportTable::GetInfo(int index, std::string &name, std::string &signature) const {
    if (index < 0 || index >= static_cast<int>(m_Order.size()))
        return false;

    const ScriptExportBinding *binding = Find(m_Order[static_cast<size_t>(index)]);
    if (!binding)
        return false;
    name = binding->Name;
    signature = binding->Signature;
    return true;
}

void ScriptExportTable::Clear() {
    m_Exports.clear();
    m_Order.clear();
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
    callArgs.Api = &runtime.GetApi();

    ScriptMethodCall call;
    call.Method = binding.Method;
    call.WriteArgs = binding.SignatureInfo.ParameterTypes.empty() ? nullptr : WriteFrameExportArgs;
    call.ReadResult = binding.SignatureInfo.ReturnType == ScriptExportValueType::Void ? nullptr : ReadFrameExportResult;
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
