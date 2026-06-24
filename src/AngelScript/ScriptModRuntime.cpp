#include "ScriptModRuntime.h"

#include <utility>

#include "ScriptMod.h"

namespace BML {

namespace {
thread_local ScriptMod *g_CurrentScriptMod = nullptr;
thread_local ScriptMod *g_ConstructingScriptMod = nullptr;
thread_local ScriptModRuntime *g_ConstructingScriptModRuntime = nullptr;
thread_local int g_ScriptObjectConstructionDepth = 0;
thread_local std::string g_ScriptObjectConstructionViolation;
thread_local ScriptMod *g_StateHookScriptMod = nullptr;
thread_local ScriptModRuntime *g_StateHookScriptModRuntime = nullptr;
thread_local ScriptModReloadPhase g_StateHookPhase = ScriptModReloadPhase::None;
thread_local int g_StateHookDepth = 0;
thread_local bool g_InRenderCallback = false;

void ReplaceAll(std::string &value, const std::string &from, const std::string &to) {
    if (from.empty())
        return;

    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

void ReplaceCompilerMessageSection(ScriptDiagnostic &diagnostic,
                                   const std::string &from,
                                   const std::string &to) {
    if (from.empty())
        return;
    for (ScriptCompilerMessage &message : diagnostic.CompilerMessages) {
        if (message.Section == from)
            message.Section = to;
    }
}

CKAS_STATUS __cdecl BMLScriptHostCallFilter(const char *apiName, CKDWORD flags, void *) {
    if ((flags & CKAS_HOSTCALL_MUTATES_HOST_STATE) == 0)
        return CKAS_OK;

    return ScriptModRuntime::RecordConstructionHostCallViolation(apiName) ||
           ScriptModRuntime::RecordStateHookHostCallViolation(apiName)
               ? CKAS_INVALIDSTATE
               : CKAS_OK;
}
} // namespace

ScriptCurrentModScope::ScriptCurrentModScope(ScriptMod *owner)
    : m_Previous(g_CurrentScriptMod) {
    if (owner)
        g_CurrentScriptMod = owner;
}

ScriptCurrentModScope::~ScriptCurrentModScope() {
    g_CurrentScriptMod = m_Previous;
}

ScriptObjectConstructionScope::ScriptObjectConstructionScope(ScriptMod *owner, ScriptModRuntime *runtime)
    : m_Previous(g_ConstructingScriptMod),
      m_PreviousRuntime(g_ConstructingScriptModRuntime),
      m_PreviousDepth(g_ScriptObjectConstructionDepth),
      m_PreviousViolation(g_ScriptObjectConstructionViolation),
      m_Active(owner != nullptr) {
    if (m_Active) {
        if (g_ScriptObjectConstructionDepth == 0)
            g_ScriptObjectConstructionViolation.clear();
        g_ConstructingScriptMod = owner;
        g_ConstructingScriptModRuntime = runtime;
        ++g_ScriptObjectConstructionDepth;
    }
}

ScriptObjectConstructionScope::~ScriptObjectConstructionScope() {
    if (!m_Active)
        return;
    if (g_ScriptObjectConstructionDepth > 0)
        --g_ScriptObjectConstructionDepth;
    g_ConstructingScriptMod = m_Previous;
    g_ConstructingScriptModRuntime = m_PreviousRuntime;
    if (m_PreviousDepth == 0)
        g_ScriptObjectConstructionViolation = m_PreviousViolation;
}

std::string ScriptObjectConstructionScope::GetViolation() const {
    return m_Active ? g_ScriptObjectConstructionViolation : std::string();
}

ScriptStateHookScope::ScriptStateHookScope(ScriptMod *owner, ScriptModRuntime *runtime, ScriptModReloadPhase phase)
    : m_PreviousMod(g_StateHookScriptMod),
      m_PreviousRuntime(g_StateHookScriptModRuntime),
      m_PreviousPhase(g_StateHookPhase),
      m_PreviousDepth(g_StateHookDepth),
      m_Active(owner != nullptr && runtime != nullptr && IsScriptModStateHookPhase(phase)) {
    if (m_Active) {
        g_StateHookScriptMod = owner;
        g_StateHookScriptModRuntime = runtime;
        g_StateHookPhase = phase;
        ++g_StateHookDepth;
    }
}

ScriptStateHookScope::~ScriptStateHookScope() {
    if (!m_Active)
        return;
    if (g_StateHookDepth > 0)
        --g_StateHookDepth;
    g_StateHookScriptMod = m_PreviousMod;
    g_StateHookScriptModRuntime = m_PreviousRuntime;
    g_StateHookPhase = m_PreviousDepth > 0 ? m_PreviousPhase : ScriptModReloadPhase::None;
}

ScriptRenderCallbackScope::ScriptRenderCallbackScope()
    : m_Previous(g_InRenderCallback) {
    g_InRenderCallback = true;
}

ScriptRenderCallbackScope::~ScriptRenderCallbackScope() {
    g_InRenderCallback = m_Previous;
}

class ScriptRuntimeCallScope {
public:
    explicit ScriptRuntimeCallScope(ScriptMod *owner) : m_Owner(owner) {
        if (m_Owner)
            m_Entered = m_Owner->EnterScriptCall();
    }

    ~ScriptRuntimeCallScope() {
        if (m_Owner && m_Entered)
            m_Owner->LeaveScriptCall();
    }

    bool Entered() const { return m_Entered; }

    ScriptRuntimeCallScope(const ScriptRuntimeCallScope &) = delete;
    ScriptRuntimeCallScope &operator=(const ScriptRuntimeCallScope &) = delete;

private:
    ScriptMod *m_Owner = nullptr;
    bool m_Entered = false;
};

ScriptModRuntime::ScriptModRuntime(std::string moduleName)
    : m_ModuleName(std::move(moduleName)) {
}

ScriptModRuntime::ScriptModRuntime(ScriptModRuntime &&other) noexcept
    : m_ModuleName(std::move(other.m_ModuleName)),
      m_Adapter(std::move(other.m_Adapter)),
      m_AngelScript(other.m_AngelScript),
      m_Api(other.m_AngelScript ? &m_Adapter.GetApi() : nullptr),
      m_Object(other.m_Object),
      m_ModuleLoaded(other.m_ModuleLoaded),
      m_Loaded(other.m_Loaded),
      m_Owner(other.m_Owner) {
    other.m_AngelScript = nullptr;
    other.m_Api = nullptr;
    other.m_Object = nullptr;
    other.m_ModuleLoaded = false;
    other.m_Loaded = false;
    other.m_Owner = nullptr;
}

ScriptModRuntime &ScriptModRuntime::operator=(ScriptModRuntime &&other) noexcept {
    if (this == &other)
        return *this;

    m_ModuleName = std::move(other.m_ModuleName);
    m_Adapter = std::move(other.m_Adapter);
    m_AngelScript = other.m_AngelScript;
    m_Api = other.m_AngelScript ? &m_Adapter.GetApi() : nullptr;
    m_Object = other.m_Object;
    m_ModuleLoaded = other.m_ModuleLoaded;
    m_Loaded = other.m_Loaded;
    m_Owner = other.m_Owner;

    other.m_AngelScript = nullptr;
    other.m_Api = nullptr;
    other.m_Object = nullptr;
    other.m_ModuleLoaded = false;
    other.m_Loaded = false;
    other.m_Owner = nullptr;
    return *this;
}

ScriptMod *ScriptModRuntime::GetCurrentScriptMod() {
    return g_CurrentScriptMod;
}

bool ScriptModRuntime::IsConstructingScriptObject() {
    return g_ScriptObjectConstructionDepth > 0 && g_ConstructingScriptMod != nullptr;
}

bool ScriptModRuntime::RecordConstructionHostCallViolation(const char *apiName) {
    if (!IsConstructingScriptObject())
        return false;

    if (g_ScriptObjectConstructionViolation.empty()) {
        g_ScriptObjectConstructionViolation = apiName ? apiName : "BML host API";
        g_ScriptObjectConstructionViolation +=
            " is not available while constructing a script mod object; move host-visible work to OnLoad().";
    }

    ScriptModRuntime *runtime = g_ConstructingScriptModRuntime;
    if (runtime) {
        const ::CKAngelScriptAdapter::Api &api = runtime->m_Adapter.GetApi();
        if (api.InitResult && api.SetActiveContextException) {
            CKAngelScriptResult result = {};
            api.InitResult(&result);
            api.SetActiveContextException(runtime->m_Adapter.GetAngelScript(),
                                          g_ScriptObjectConstructionViolation.c_str(),
                                          &result);
        }
    }
    return true;
}

bool ScriptModRuntime::IsInStateHook() {
    return g_StateHookDepth > 0 && g_StateHookScriptMod != nullptr;
}

ScriptModReloadPhase ScriptModRuntime::GetStateHookPhase() {
    return IsInStateHook() ? g_StateHookPhase : ScriptModReloadPhase::None;
}

bool ScriptModRuntime::RecordStateHookHostCallViolation(const char *apiName) {
    if (!IsInStateHook())
        return false;

    std::string message = apiName ? apiName : "BML host API";
    message += " is not available during hot reload ";
    message += GetScriptModReloadPhaseName(g_StateHookPhase);
    message += "; state hooks may only transfer primitive/string values through BML::StateBag and use read-only queries/logging.";

    ScriptModRuntime *runtime = g_StateHookScriptModRuntime;
    if (runtime) {
        const ::CKAngelScriptAdapter::Api &api = runtime->m_Adapter.GetApi();
        if (api.InitResult && api.SetActiveContextException) {
            CKAngelScriptResult result = {};
            api.InitResult(&result);
            api.SetActiveContextException(runtime->m_Adapter.GetAngelScript(),
                                          message.c_str(),
                                          &result);
        }
    }
    return true;
}

bool ScriptModRuntime::IsInRenderCallback() {
    return g_InRenderCallback;
}

bool ScriptModRuntime::Refresh(CKContext *context, ScriptDiagnostic &diagnostic) {
    if (m_Adapter.Refresh(context)) {
        const ::CKAngelScriptAdapter::Api &api = m_Adapter.GetApi();
        if (api.InitResult && api.SetHostCallFilter) {
            CKAngelScriptResult result = {};
            api.InitResult(&result);
            const CKAS_STATUS status = api.SetHostCallFilter(m_Adapter.GetAngelScript(),
                                                             BMLScriptHostCallFilter,
                                                             nullptr,
                                                             &result);
            if (status != CKAS_OK) {
                diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::CkasHost,
                                                  status,
                                                  result,
                                                  "Failed to install CKAngelScript host-call filter");
                return false;
            }
        }
        return true;
    }
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
        ReplaceAll(diagnostic.RawMessage, m_ModuleName + "(", entryPathUtf8 + "(");
        ReplaceCompilerMessageSection(diagnostic, m_ModuleName, entryPathUtf8);
        return false;
    }

    m_ModuleLoaded = true;
    return true;
}

bool ScriptModRuntime::LoadModuleFromCode(CKContext *context,
                                          const std::string &sourceCode,
                                          const std::string &entryPathUtf8,
                                          ScriptDiagnostic &diagnostic) {
    if (m_ModuleLoaded)
        return true;
    if (!Refresh(context, diagnostic))
        return false;

    const ::CKAngelScriptAdapter::Api &api = m_Adapter.GetApi();
    CKAngelScript *angelScript = m_Adapter.GetAngelScript();

    CKAngelScriptResult result = {};
    api.InitResult(&result);
    CKAngelScriptLoadOptions loadOptions = {};
    api.InitLoadOptions(&loadOptions);
    loadOptions.ModuleName = m_ModuleName.c_str();
    loadOptions.Code = sourceCode.c_str();
    loadOptions.Flags = CKAS_LOAD_REPLACEEXISTING;

    const CKAS_STATUS status = api.LoadModule(angelScript, &loadOptions, &result);
    if (status != CKAS_OK) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Compile, status, result, "Compile failed");
        diagnostic.EntryPath = entryPathUtf8;
        ReplaceAll(diagnostic.RawMessage, m_ModuleName + "(", entryPathUtf8 + "(");
        ReplaceCompilerMessageSection(diagnostic, m_ModuleName, entryPathUtf8);
        return false;
    }

    m_ModuleLoaded = true;
    return true;
}

bool ScriptModRuntime::LoadModuleFromSections(CKContext *context,
                                              const std::vector<ScriptSourceSection> &sections,
                                              const std::string &entryPathUtf8,
                                              ScriptDiagnostic &diagnostic) {
    if (m_ModuleLoaded)
        return true;
    if (sections.empty()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Compile,
                                          "Script source snapshot does not contain an entry section.");
        diagnostic.EntryPath = entryPathUtf8;
        return false;
    }
    if (!Refresh(context, diagnostic))
        return false;

    const ::CKAngelScriptAdapter::Api &api = m_Adapter.GetApi();
    CKAngelScript *angelScript = m_Adapter.GetAngelScript();

    std::vector<CKAngelScriptSourceSection> sourceSections;
    sourceSections.reserve(sections.size());
    for (const ScriptSourceSection &section : sections) {
        CKAngelScriptSourceSection sourceSection = {};
        sourceSection.Size = sizeof(sourceSection);
        sourceSection.SectionName = section.Name.c_str();
        sourceSection.Code = section.Code.c_str();
        sourceSection.CodeSize = section.Code.size();
        sourceSections.push_back(sourceSection);
    }

    CKAngelScriptResult result = {};
    api.InitResult(&result);
    CKAngelScriptLoadOptions loadOptions = {};
    api.InitLoadOptions(&loadOptions);
    loadOptions.ModuleName = m_ModuleName.c_str();
    loadOptions.Sections = sourceSections.data();
    loadOptions.SectionCount = sourceSections.size();
    loadOptions.Flags = CKAS_LOAD_REPLACEEXISTING;

    const CKAS_STATUS status = api.LoadModule(angelScript, &loadOptions, &result);
    if (status != CKAS_OK) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Compile, status, result, "Compile failed");
        diagnostic.EntryPath = entryPathUtf8;
        ReplaceAll(diagnostic.RawMessage, m_ModuleName + "(", entryPathUtf8 + "(");
        ReplaceCompilerMessageSection(diagnostic, m_ModuleName, entryPathUtf8);
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
    ScriptObjectConstructionScope constructionScope(m_Owner, this);
    const CKAS_STATUS status = api.CreateObject(m_Adapter.GetAngelScript(), &objectOptions, &m_Object, &result);
    const std::string constructionViolation = constructionScope.GetViolation();
    if (status == CKAS_OK && !constructionViolation.empty()) {
        if (m_Object) {
            CKAngelScriptResult releaseResult = {};
            api.InitResult(&releaseResult);
            api.ReleaseObject(m_Adapter.GetAngelScript(), m_Object, &releaseResult);
            m_Object = nullptr;
        }
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::CreateObject,
                                          constructionViolation);
        diagnostic.Status = CKAS_INVALIDSTATE;
        return false;
    }

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
        method = nullptr;
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
    method = nullptr;
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
    if (!m_Object) {
        diagnostic = MakeScriptDiagnostic(phase, "Script object is not available.");
        diagnostic.Status = CKAS_INVALIDARGUMENT;
        return false;
    }
    if (!m_Api || m_Api != &m_Adapter.GetApi() || !m_AngelScript) {
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
    ScriptRuntimeCallScope activeCallScope(m_Owner);
    if (m_Owner && !activeCallScope.Entered()) {
        diagnostic = MakeScriptDiagnostic(phase, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }
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
