#include "CKAngelScriptAdapter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <sstream>

static constexpr const char *kModuleName = "AngelScript.dll";
static CKAngelScriptAdapter::GetStatusNameFn g_GetStatusName = nullptr;

static constexpr CKAS_FEATURE kRequiredFeatures[] = {
    CKAS_FEATURE_MODULE_LIFECYCLE,
    CKAS_FEATURE_RAW_ANGELSCRIPT_ACCESS,
    CKAS_FEATURE_OBJECT_HANDLE,
    CKAS_FEATURE_SYNC_OBJECT_METHOD_CALL,
    CKAS_FEATURE_TYPED_ARG_READER_WRITER,
    CKAS_FEATURE_STACK_TRACE,
    CKAS_FEATURE_ENGINE_EXTENSION,
    CKAS_FEATURE_PUBLIC_STRUCT_INITIALIZERS,
    CKAS_FEATURE_STATUS_TEXT,
    CKAS_FEATURE_METADATA_REFLECTION,
    CKAS_FEATURE_OBJECT_TYPE_NAMESPACE,
    CKAS_FEATURE_OBJECT_METHOD_CONTEXT_ACCESS,
    CKAS_FEATURE_SCRIPT_ARRAY_ACCESS,
    CKAS_FEATURE_ACTIVE_CONTEXT_EXCEPTION,
    CKAS_FEATURE_SOURCE_SECTIONS,
    CKAS_FEATURE_OBJECT_HANDLE_ARGS,
    CKAS_FEATURE_HOST_CALL_FILTER,
};

template <typename T>
static bool Resolve(HMODULE module, const char *name, T &out, std::string &missingExport) {
    out = reinterpret_cast<T>(::GetProcAddress(module, name));
    if (out)
        return true;

    missingExport = name;
    return false;
}

static std::string MakeMissingFeatureDiagnostic(CKAS_FEATURE feature) {
    std::string message = std::string("CKAngelScript is missing required BML script feature ") +
        CKAngelScriptAdapter::FeatureName(feature) + ".";
    if (feature == CKAS_FEATURE_OBJECT_TYPE_NAMESPACE) {
        message += " The installed CKAngelScript build is too old for namespaced script mod classes; "
                   "install a CKAS v3 build that supports CKAngelScriptObjectOptions::ClassNamespace.";
    } else if (feature == CKAS_FEATURE_OBJECT_METHOD_CONTEXT_ACCESS) {
        message += " BML requires generic object-method context access for script-side value marshalling.";
    } else if (feature == CKAS_FEATURE_SCRIPT_ARRAY_ACCESS) {
        message += " BML requires generic CKAngelScript array object access for script-side value marshalling.";
    } else if (feature == CKAS_FEATURE_ACTIVE_CONTEXT_EXCEPTION) {
        message += " BML requires CKAngelScript to set host API exceptions on the current script context.";
    } else if (feature == CKAS_FEATURE_SOURCE_SECTIONS) {
        message += " BML requires CKAngelScript source-section loading for consistent script hot reload snapshots.";
    } else if (feature == CKAS_FEATURE_OBJECT_HANDLE_ARGS) {
        message += " BML requires CKAngelScript object-handle argument writing for script hot reload state migration.";
    } else if (feature == CKAS_FEATURE_HOST_CALL_FILTER) {
        message += " BML requires CKAngelScript host-call filtering to block world-mutating CKAS APIs during script hot reload state hooks.";
    }
    return message;
}

bool CKAngelScriptAdapter::Refresh(CKContext *context) {
    m_Context = context;
    m_AngelScript = nullptr;

    HMODULE module = ::GetModuleHandleA(kModuleName);
    if (!module) {
        m_Module = nullptr;
        m_Api = Api();
        m_ApiVersion = 0;
        g_GetStatusName = nullptr;
        for (bool &feature : m_Features)
            feature = false;
        SetUnavailable(State::MissingModule, "AngelScript.dll is not loaded; script mods are unavailable.");
        return false;
    }

    const bool needsValidation = m_State != State::Available || m_Module != module || !m_Api.GetAngelScript;
    if (needsValidation) {
        m_Module = module;
        m_Api = Api();
        m_ApiVersion = 0;
        g_GetStatusName = nullptr;
        for (bool &feature : m_Features)
            feature = false;

        if (!ResolveRequiredExports(module))
            return false;
        g_GetStatusName = m_Api.GetStatusName;

        m_ApiVersion = m_Api.GetApiVersion();
        if (m_ApiVersion < CKAS_API_VERSION) {
            std::ostringstream message;
            message << "CKAngelScript API version " << m_ApiVersion
                    << " is too old; BML requires version " << CKAS_API_VERSION << ".";
            SetUnavailable(State::VersionTooOld, message.str());
            return false;
        }
    }

    m_AngelScript = m_Api.GetAngelScript(context);
    if (!m_AngelScript) {
        SetUnavailable(State::MissingRuntime, "CKAngelScript manager is not available from this CK context.");
        return false;
    }

    if (needsValidation && !ValidateFeatures())
        return false;

    m_State = State::Available;
    m_Diagnostic.clear();
    return true;
}

bool CKAngelScriptAdapter::IsAvailable() const {
    return m_State == State::Available && m_AngelScript != nullptr;
}

bool CKAngelScriptAdapter::HasFeature(CKAS_FEATURE feature) const {
    const int index = static_cast<int>(feature);
    return index >= 0 && index <= CKAS_FEATURE_HOST_CALL_FILTER && m_Features[index];
}

CKAngelScriptAdapter::State CKAngelScriptAdapter::GetState() const {
    return m_State;
}

CKDWORD CKAngelScriptAdapter::GetApiVersion() const {
    return m_ApiVersion;
}

CKAngelScript *CKAngelScriptAdapter::GetAngelScript() const {
    return m_AngelScript;
}

const CKAngelScriptAdapter::Api &CKAngelScriptAdapter::GetApi() const {
    return m_Api;
}

const std::string &CKAngelScriptAdapter::GetDiagnostic() const {
    return m_Diagnostic;
}

const char *CKAngelScriptAdapter::StatusName(CKAS_STATUS status) {
    if (g_GetStatusName) {
        const char *name = g_GetStatusName(status);
        if (name && name[0])
            return name;
    }

    switch (status) {
    case CKAS_OK:
        return "CKAS_OK";
    case CKAS_INVALIDARGUMENT:
        return "CKAS_INVALIDARGUMENT";
    case CKAS_NOTINITIALIZED:
        return "CKAS_NOTINITIALIZED";
    case CKAS_NOTFOUND:
        return "CKAS_NOTFOUND";
    case CKAS_COMPILEERROR:
        return "CKAS_COMPILEERROR";
    case CKAS_EXECUTIONFAILED:
        return "CKAS_EXECUTIONFAILED";
    case CKAS_SUSPENDED:
        return "CKAS_SUSPENDED";
    case CKAS_CANCELLED:
        return "CKAS_CANCELLED";
    case CKAS_STALEHANDLE:
        return "CKAS_STALEHANDLE";
    case CKAS_UNSUPPORTED:
        return "CKAS_UNSUPPORTED";
    case CKAS_TYPEMISMATCH:
        return "CKAS_TYPEMISMATCH";
    case CKAS_BUFFERTOOSMALL:
        return "CKAS_BUFFERTOOSMALL";
    case CKAS_INVALIDSTATE:
        return "CKAS_INVALIDSTATE";
    case CKAS_INUSE:
        return "CKAS_INUSE";
    case CKAS_ALREADYEXISTS:
        return "CKAS_ALREADYEXISTS";
    case CKAS_AMBIGUOUS:
        return "CKAS_AMBIGUOUS";
    case CKAS_FOREIGNHANDLE:
        return "CKAS_FOREIGNHANDLE";
    default:
        return "CKAS_UNKNOWN";
    }
}

const char *CKAngelScriptAdapter::FeatureName(CKAS_FEATURE feature) {
    switch (feature) {
    case CKAS_FEATURE_MODULE_LIFECYCLE:
        return "CKAS_FEATURE_MODULE_LIFECYCLE";
    case CKAS_FEATURE_RAW_ANGELSCRIPT_ACCESS:
        return "CKAS_FEATURE_RAW_ANGELSCRIPT_ACCESS";
    case CKAS_FEATURE_FUNCTION_HANDLE:
        return "CKAS_FEATURE_FUNCTION_HANDLE";
    case CKAS_FEATURE_FUNCTION_EXECUTION:
        return "CKAS_FEATURE_FUNCTION_EXECUTION";
    case CKAS_FEATURE_FUNCTION_EXECUTION_RESUME:
        return "CKAS_FEATURE_FUNCTION_EXECUTION_RESUME";
    case CKAS_FEATURE_OBJECT_HANDLE:
        return "CKAS_FEATURE_OBJECT_HANDLE";
    case CKAS_FEATURE_SYNC_OBJECT_METHOD_CALL:
        return "CKAS_FEATURE_SYNC_OBJECT_METHOD_CALL";
    case CKAS_FEATURE_TYPED_ARG_READER_WRITER:
        return "CKAS_FEATURE_TYPED_ARG_READER_WRITER";
    case CKAS_FEATURE_STACK_TRACE:
        return "CKAS_FEATURE_STACK_TRACE";
    case CKAS_FEATURE_ENGINE_EXTENSION:
        return "CKAS_FEATURE_ENGINE_EXTENSION";
    case CKAS_FEATURE_PUBLIC_STRUCT_INITIALIZERS:
        return "CKAS_FEATURE_PUBLIC_STRUCT_INITIALIZERS";
    case CKAS_FEATURE_STATUS_TEXT:
        return "CKAS_FEATURE_STATUS_TEXT";
    case CKAS_FEATURE_METADATA_REFLECTION:
        return "CKAS_FEATURE_METADATA_REFLECTION";
    case CKAS_FEATURE_OBJECT_TYPE_NAMESPACE:
        return "CKAS_FEATURE_OBJECT_TYPE_NAMESPACE";
    case CKAS_FEATURE_OBJECT_METHOD_CONTEXT_ACCESS:
        return "CKAS_FEATURE_OBJECT_METHOD_CONTEXT_ACCESS";
    case CKAS_FEATURE_SCRIPT_ARRAY_ACCESS:
        return "CKAS_FEATURE_SCRIPT_ARRAY_ACCESS";
    case CKAS_FEATURE_ACTIVE_CONTEXT_EXCEPTION:
        return "CKAS_FEATURE_ACTIVE_CONTEXT_EXCEPTION";
    case CKAS_FEATURE_SOURCE_SECTIONS:
        return "CKAS_FEATURE_SOURCE_SECTIONS";
    case CKAS_FEATURE_OBJECT_HANDLE_ARGS:
        return "CKAS_FEATURE_OBJECT_HANDLE_ARGS";
    case CKAS_FEATURE_HOST_CALL_FILTER:
        return "CKAS_FEATURE_HOST_CALL_FILTER";
    default:
        return "CKAS_FEATURE_UNKNOWN";
    }
}

std::string CKAngelScriptAdapter::FormatResult(CKAS_STATUS status, const CKAngelScriptResult &result) {
    std::ostringstream message;
    message << StatusName(status);
    if (result.AngelScriptCode != 0)
        message << " code=" << result.AngelScriptCode;
    if (result.ErrorMessage && result.ErrorMessage[0])
        message << " " << result.ErrorMessage;
    if (result.StackTrace && result.StackTrace[0])
        message << "\n" << result.StackTrace;
    return message.str();
}

bool CKAngelScriptAdapter::ResolveRequiredExports(void *moduleHandle) {
    HMODULE module = static_cast<HMODULE>(moduleHandle);
    std::string missing;

    const bool ok =
        Resolve(module, "CKGetAngelScript", m_Api.GetAngelScript, missing) &&
        Resolve(module, "CKAngelScriptGetApiVersion", m_Api.GetApiVersion, missing) &&
        Resolve(module, "CKAngelScriptHasFeature", m_Api.HasFeature, missing) &&
        Resolve(module, "CKAngelScriptInitResult", m_Api.InitResult, missing) &&
        Resolve(module, "CKAngelScriptInitLoadOptions", m_Api.InitLoadOptions, missing) &&
        Resolve(module, "CKAngelScriptInitObjectOptions", m_Api.InitObjectOptions, missing) &&
        Resolve(module, "CKAngelScriptInitMethodOptions", m_Api.InitMethodOptions, missing) &&
        Resolve(module, "CKAngelScriptInitObjectMethodExecuteOptions", m_Api.InitObjectMethodExecuteOptions, missing) &&
        Resolve(module, "CKAngelScriptInitEngineExtension", m_Api.InitEngineExtension, missing) &&
        Resolve(module, "CKAngelScriptGetStatusName", m_Api.GetStatusName, missing) &&
        Resolve(module, "CKAngelScriptGetStatusDescription", m_Api.GetStatusDescription, missing) &&
        Resolve(module, "CKAngelScriptLoadModule", m_Api.LoadModule, missing) &&
        Resolve(module, "CKAngelScriptCompileModule", m_Api.CompileModule, missing) &&
        Resolve(module, "CKAngelScriptUnloadModule", m_Api.UnloadModule, missing) &&
        Resolve(module, "CKAngelScriptHasModule", m_Api.HasModule, missing) &&
        Resolve(module, "CKAngelScriptGetModuleGeneration", m_Api.GetModuleGeneration, missing) &&
        Resolve(module, "CKAngelScriptEnumerateMetadata", m_Api.EnumerateMetadata, missing) &&
        Resolve(module, "CKAngelScriptCreateObject", m_Api.CreateObject, missing) &&
        Resolve(module, "CKAngelScriptReleaseObject", m_Api.ReleaseObject, missing) &&
        Resolve(module, "CKAngelScriptFindObjectMethod", m_Api.FindObjectMethod, missing) &&
        Resolve(module, "CKAngelScriptReleaseMethod", m_Api.ReleaseMethod, missing) &&
        Resolve(module, "CKAngelScriptBorrowActiveContext", m_Api.BorrowActiveContext, missing) &&
        Resolve(module, "CKAngelScriptSetActiveContextException", m_Api.SetActiveContextException, missing) &&
        Resolve(module, "CKAngelScriptSetHostCallFilter", m_Api.SetHostCallFilter, missing) &&
        Resolve(module, "CKAngelScriptAssignObjectHandle", m_Api.AssignObjectHandle, missing) &&
        Resolve(module, "CKAngelScriptArgSetBool", m_Api.ArgSetBool, missing) &&
        Resolve(module, "CKAngelScriptArgSetInt", m_Api.ArgSetInt, missing) &&
        Resolve(module, "CKAngelScriptArgSetFloat", m_Api.ArgSetFloat, missing) &&
        Resolve(module, "CKAngelScriptArgSetString", m_Api.ArgSetString, missing) &&
        Resolve(module, "CKAngelScriptArgSetBorrowedObject", m_Api.ArgSetBorrowedObject, missing) &&
        Resolve(module, "CKAngelScriptArgSetObjectHandle", m_Api.ArgSetObjectHandle, missing) &&
        Resolve(module, "CKAngelScriptResultGetBool", m_Api.ResultGetBool, missing) &&
        Resolve(module, "CKAngelScriptResultGetInt", m_Api.ResultGetInt, missing) &&
        Resolve(module, "CKAngelScriptResultGetFloat", m_Api.ResultGetFloat, missing) &&
        Resolve(module, "CKAngelScriptResultGetString", m_Api.ResultGetString, missing) &&
        Resolve(module, "CKAngelScriptResultGetStringView", m_Api.ResultGetStringView, missing) &&
        Resolve(module, "CKAngelScriptCallObjectMethod", m_Api.CallObjectMethod, missing) &&
        Resolve(module, "CKAngelScriptRegisterEngineExtension", m_Api.RegisterEngineExtension, missing) &&
        Resolve(module, "CKAngelScriptUnregisterEngineExtension", m_Api.UnregisterEngineExtension, missing) &&
        Resolve(module, "CKAngelScriptCreateArray", m_Api.CreateArray, missing) &&
        Resolve(module, "CKAngelScriptCreateArrayByType", m_Api.CreateArrayByType, missing) &&
        Resolve(module, "CKAngelScriptArrayAddRef", m_Api.ArrayAddRef, missing) &&
        Resolve(module, "CKAngelScriptArrayRelease", m_Api.ArrayRelease, missing) &&
        Resolve(module, "CKAngelScriptArrayGetRefCount", m_Api.ArrayGetRefCount, missing) &&
        Resolve(module, "CKAngelScriptArrayGetArrayType", m_Api.ArrayGetArrayType, missing) &&
        Resolve(module, "CKAngelScriptArrayGetArrayTypeId", m_Api.ArrayGetArrayTypeId, missing) &&
        Resolve(module, "CKAngelScriptArrayGetSize", m_Api.ArrayGetSize, missing) &&
        Resolve(module, "CKAngelScriptArrayResize", m_Api.ArrayResize, missing) &&
        Resolve(module, "CKAngelScriptArrayReserve", m_Api.ArrayReserve, missing) &&
        Resolve(module, "CKAngelScriptArrayGetElementTypeId", m_Api.ArrayGetElementTypeId, missing) &&
        Resolve(module, "CKAngelScriptArrayGetElementAddress", m_Api.ArrayGetElementAddress, missing) &&
        Resolve(module, "CKAngelScriptArrayGetConstElementAddress", m_Api.ArrayGetConstElementAddress, missing) &&
        Resolve(module, "CKAngelScriptArraySetElementValue", m_Api.ArraySetElementValue, missing) &&
        Resolve(module, "CKAngelScriptArrayInsertAt", m_Api.ArrayInsertAt, missing) &&
        Resolve(module, "CKAngelScriptArrayInsertLast", m_Api.ArrayInsertLast, missing) &&
        Resolve(module, "CKAngelScriptArrayRemoveAt", m_Api.ArrayRemoveAt, missing) &&
        Resolve(module, "CKAngelScriptArrayRemoveLast", m_Api.ArrayRemoveLast, missing) &&
        Resolve(module, "CKAngelScriptArrayClear", m_Api.ArrayClear, missing);

    if (ok)
        return true;

    SetUnavailable(State::MissingExport, std::string("AngelScript.dll is missing required export ") + missing + ".");
    return false;
}

bool CKAngelScriptAdapter::ValidateFeatures() {
    for (CKAS_FEATURE feature : kRequiredFeatures) {
        const int index = static_cast<int>(feature);
        const bool supported = m_Api.HasFeature(feature) != 0;
        if (index >= 0 && index <= CKAS_FEATURE_HOST_CALL_FILTER)
            m_Features[index] = supported;

        if (!supported) {
            SetUnavailable(State::MissingFeature, MakeMissingFeatureDiagnostic(feature));
            return false;
        }
    }

    return true;
}

void CKAngelScriptAdapter::SetUnavailable(State state, const std::string &diagnostic) {
    m_State = state;
    m_Diagnostic = diagnostic;
}
