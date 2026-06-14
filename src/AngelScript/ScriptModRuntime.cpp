#include "ScriptModRuntime.h"

#include <utility>

namespace BML {

namespace {
thread_local ScriptMod *g_CurrentScriptMod = nullptr;
thread_local bool g_InRenderCallback = false;
} // namespace

ScriptCurrentModScope::ScriptCurrentModScope(ScriptMod *owner)
    : m_Previous(g_CurrentScriptMod) {
    if (owner)
        g_CurrentScriptMod = owner;
}

ScriptCurrentModScope::~ScriptCurrentModScope() {
    g_CurrentScriptMod = m_Previous;
}

ScriptRenderCallbackScope::ScriptRenderCallbackScope()
    : m_Previous(g_InRenderCallback) {
    g_InRenderCallback = true;
}

ScriptRenderCallbackScope::~ScriptRenderCallbackScope() {
    g_InRenderCallback = m_Previous;
}

ScriptModRuntime::ScriptModRuntime(std::string moduleName)
    : m_ModuleName(std::move(moduleName)) {
}

ScriptMod *ScriptModRuntime::GetCurrentScriptMod() {
    return g_CurrentScriptMod;
}

bool ScriptModRuntime::IsInRenderCallback() {
    return g_InRenderCallback;
}

bool ScriptModRuntime::Refresh(CKContext *context, ScriptDiagnostic &diagnostic) {
    if (m_Adapter.Refresh(context))
        return true;
    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::CkasHost, m_Adapter.GetDiagnostic());
    return false;
}

bool ScriptModRuntime::LoadModule(CKContext *context, const std::string &entryPathUtf8, ScriptDiagnostic &diagnostic) {
    if (m_ModuleLoaded)
        return true;
    if (!Refresh(context, diagnostic))
        return false;

    const ::CKAngelScriptAdapter::Api &api = m_Adapter.GetApi();
    CKAngelScript *angelScript = m_Adapter.GetAngelScript();
    const char *filenames[] = {entryPathUtf8.c_str()};

    CKAngelScriptResult result = {};
    api.InitResult(&result);
    CKAngelScriptLoadOptions loadOptions = {};
    api.InitLoadOptions(&loadOptions);
    loadOptions.ModuleName = m_ModuleName.c_str();
    loadOptions.Filenames = filenames;
    loadOptions.FileCount = 1;
    loadOptions.Flags = CKAS_LOAD_REPLACEEXISTING;

    const CKAS_STATUS status = api.LoadModule(angelScript, &loadOptions, &result);
    if (status != CKAS_OK) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Compile, status, result, "Compile failed");
        diagnostic.EntryPath = entryPathUtf8;
        return false;
    }

    m_ModuleLoaded = true;
    return true;
}

bool ScriptModRuntime::EnumerateMetadata(CKContext *context,
                                         CKAngelScriptMetadataCallback callback,
                                         void *userData,
                                         ScriptDiagnostic &diagnostic) {
    if (!Refresh(context, diagnostic))
        return false;

    CKAngelScriptResult result = {};
    m_Adapter.GetApi().InitResult(&result);
    const CKAS_STATUS status = m_Adapter.GetApi().EnumerateMetadata(m_Adapter.GetAngelScript(),
                                                                     m_ModuleName.c_str(),
                                                                     callback,
                                                                     userData,
                                                                     &result);
    if (status == CKAS_OK)
        return true;

    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata,
                                      status,
                                      result,
                                      "Metadata reflection failed");
    return false;
}

bool ScriptModRuntime::CreateObject(CKContext *context,
                                    const std::string &classNamespace,
                                    const std::string &className,
                                    ScriptDiagnostic &diagnostic) {
    if (m_Object)
        return true;
    if (!Refresh(context, diagnostic))
        return false;

    const ::CKAngelScriptAdapter::Api &api = m_Adapter.GetApi();
    CKAngelScriptObjectOptions objectOptions = {};
    api.InitObjectOptions(&objectOptions);
    objectOptions.ModuleName = m_ModuleName.c_str();
    objectOptions.ClassName = className.c_str();
    objectOptions.ClassNamespace = classNamespace.empty() ? nullptr : classNamespace.c_str();

    CKAngelScriptResult result = {};
    api.InitResult(&result);
    const CKAS_STATUS status = api.CreateObject(m_Adapter.GetAngelScript(), &objectOptions, &m_Object, &result);
    if (status == CKAS_OK && m_Object) {
        m_AngelScript = m_Adapter.GetAngelScript();
        m_Api = &m_Adapter.GetApi();
        return true;
    }

    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::CreateObject,
                                      status,
                                      result,
                                      "Object creation failed");
    return false;
}

CKAngelScriptMethod *ScriptModRuntime::FindMethod(CKContext *context,
                                                  const char *methodDecl,
                                                  ScriptDiagnostic &diagnostic,
                                                  bool optional) {
    if (!Refresh(context, diagnostic))
        return nullptr;

    const ::CKAngelScriptAdapter::Api &api = m_Adapter.GetApi();
    CKAngelScriptMethodOptions options = {};
    api.InitMethodOptions(&options);
    options.Object = m_Object;
    options.MethodDecl = methodDecl;

    CKAngelScriptResult result = {};
    api.InitResult(&result);
    CKAngelScriptMethod *method = nullptr;
    const CKAS_STATUS status = api.FindObjectMethod(m_Adapter.GetAngelScript(), &options, &method, &result);
    if (status == CKAS_OK)
        return method;
    if (status == CKAS_NOTFOUND && optional)
        return nullptr;

    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::MethodLookup,
                                      status,
                                      result,
                                      std::string("Method lookup failed for ") + (methodDecl ? methodDecl : ""));
    return nullptr;
}

bool ScriptModRuntime::ReleaseMethod(CKContext *context,
                                     CKAngelScriptMethod *&method,
                                     ScriptDiagnostic *diagnostic) {
    if (!method)
        return true;

    ScriptDiagnostic localDiagnostic;
    if (!Refresh(context, localDiagnostic)) {
        if (diagnostic)
            *diagnostic = localDiagnostic;
        return false;
    }

    CKAngelScriptResult result = {};
    m_Adapter.GetApi().InitResult(&result);
    const CKAS_STATUS status = m_Adapter.GetApi().ReleaseMethod(m_Adapter.GetAngelScript(), method, &result);
    if (status == CKAS_OK) {
        method = nullptr;
        return true;
    }

    if (diagnostic) {
        *diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Unload,
                                           status,
                                           result,
                                           "Method release failed");
    }
    return false;
}

bool ScriptModRuntime::CallMethod(CKContext *context,
                                  CKAngelScriptMethod *method,
                                  CKAngelScriptWriteArgsCallback writeArgs,
                                  CKAngelScriptReadResultCallback readResult,
                                  CKAngelScriptContextCallback configureContext,
                                  CKAngelScriptContextCallback readContextResult,
                                  void *userData,
                                  ScriptDiagnosticPhase phase,
                                  const char *failurePrefix,
                                  ScriptDiagnostic &diagnostic) {
    if (!method)
        return true;
    if (!m_Api || !m_AngelScript) {
        if (!Refresh(context, diagnostic))
            return false;
        m_Api = &m_Adapter.GetApi();
        m_AngelScript = m_Adapter.GetAngelScript();
    }
    if (!m_Api || !m_AngelScript)
        return false;

    CKAngelScriptObjectMethodExecuteOptions options = {};
    m_Api->InitObjectMethodExecuteOptions(&options);
    options.Object = m_Object;
    options.Method = method;
    options.WriteArgs = writeArgs;
    options.ReadResult = readResult;
    options.ConfigureContext = configureContext;
    options.ReadContextResult = readContextResult;
    options.UserData = userData;
    options.Flags = CKAS_CALL_NO_SUSPEND;

    CKAngelScriptResult result = {};
    m_Api->InitResult(&result);
    ScriptCurrentModScope callScope(m_Owner);
    const CKAS_STATUS status = m_Api->CallObjectMethod(m_AngelScript, &options, &result);
    if (status == CKAS_OK)
        return true;

    diagnostic = MakeScriptDiagnostic(phase, status, result, failurePrefix);
    return false;
}

bool ScriptModRuntime::CallMethod(CKContext *context,
                                  const ScriptMethodCall &call,
                                  ScriptDiagnostic &diagnostic) {
    return CallMethod(context,
                      call.Method,
                      call.WriteArgs,
                      call.ReadResult,
                      call.ConfigureContext,
                      call.ReadContextResult,
                      call.UserData,
                      call.Phase,
                      call.FailurePrefix,
                      diagnostic);
}

bool ScriptModRuntime::Release(CKContext *context, ScriptDiagnostic *diagnostic) {
    if (!m_Object && (!m_ModuleLoaded || m_ModuleName.empty())) {
        m_ModuleLoaded = false;
        m_Loaded = false;
        return true;
    }

    ScriptDiagnostic localDiagnostic;
    if (!Refresh(context, localDiagnostic)) {
        if (diagnostic)
            *diagnostic = localDiagnostic;
        return false;
    }

    const ::CKAngelScriptAdapter::Api &api = m_Adapter.GetApi();
    CKAngelScript *angelScript = m_Adapter.GetAngelScript();
    CKAngelScriptResult result = {};
    api.InitResult(&result);
    bool ok = true;

    if (m_Object) {
        const CKAS_STATUS status = api.ReleaseObject(angelScript, m_Object, &result);
        if (status == CKAS_OK) {
            m_Object = nullptr;
        } else {
            ok = false;
            if (diagnostic) {
                *diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Unload,
                                                   status,
                                                   result,
                                                   "Object release failed");
            }
        }
    }
    if (ok && !m_ModuleName.empty() && m_ModuleLoaded) {
        api.InitResult(&result);
        const CKAS_STATUS status = api.UnloadModule(angelScript, m_ModuleName.c_str(), &result);
        if (status != CKAS_OK) {
            ok = false;
            if (diagnostic) {
                *diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Unload,
                                                   status,
                                                   result,
                                                   "Module unload failed");
            }
        }
    }

    if (ok) {
        m_ModuleLoaded = false;
        m_Loaded = false;
        m_AngelScript = nullptr;
        m_Api = nullptr;
    }
    return ok;
}

} // namespace BML
