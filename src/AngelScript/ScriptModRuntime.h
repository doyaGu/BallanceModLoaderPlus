#ifndef BML_SCRIPTMODRUNTIME_H
#define BML_SCRIPTMODRUNTIME_H

#include <string>
#include <vector>

#include "CKAngelScriptAdapter.h"
#include "ScriptDiagnostic.h"
#include "ScriptModLifecycle.h"

namespace BML {

class ScriptMod;
class ScriptModRuntime;

class ScriptCurrentModScope {
public:
    explicit ScriptCurrentModScope(ScriptMod *owner);
    ~ScriptCurrentModScope();

    ScriptCurrentModScope(const ScriptCurrentModScope &) = delete;
    ScriptCurrentModScope &operator=(const ScriptCurrentModScope &) = delete;

private:
    ScriptMod *m_Previous = nullptr;
};

class ScriptObjectConstructionScope {
public:
    explicit ScriptObjectConstructionScope(ScriptMod *owner, ScriptModRuntime *runtime = nullptr);
    ~ScriptObjectConstructionScope();

    ScriptObjectConstructionScope(const ScriptObjectConstructionScope &) = delete;
    ScriptObjectConstructionScope &operator=(const ScriptObjectConstructionScope &) = delete;

    std::string GetViolation() const;

private:
    ScriptMod *m_Previous = nullptr;
    ScriptModRuntime *m_PreviousRuntime = nullptr;
    int m_PreviousDepth = 0;
    std::string m_PreviousViolation;
    bool m_Active = false;
};

class ScriptStateHookScope {
public:
    ScriptStateHookScope(ScriptMod *owner, ScriptModRuntime *runtime, ScriptModReloadPhase phase);
    ~ScriptStateHookScope();

    ScriptStateHookScope(const ScriptStateHookScope &) = delete;
    ScriptStateHookScope &operator=(const ScriptStateHookScope &) = delete;

private:
    ScriptMod *m_PreviousMod = nullptr;
    ScriptModRuntime *m_PreviousRuntime = nullptr;
    ScriptModReloadPhase m_PreviousPhase = ScriptModReloadPhase::None;
    int m_PreviousDepth = 0;
    bool m_Active = false;
};

class ScriptRenderCallbackScope {
public:
    ScriptRenderCallbackScope();
    ~ScriptRenderCallbackScope();

    ScriptRenderCallbackScope(const ScriptRenderCallbackScope &) = delete;
    ScriptRenderCallbackScope &operator=(const ScriptRenderCallbackScope &) = delete;

private:
    bool m_Previous = false;
};

struct ScriptMethodCall {
    CKAngelScriptMethod *Method = nullptr;
    CKAngelScriptWriteArgsCallback WriteArgs = nullptr;
    CKAngelScriptReadResultCallback ReadResult = nullptr;
    CKAngelScriptContextCallback ConfigureContext = nullptr;
    CKAngelScriptContextCallback ReadContextResult = nullptr;
    void *UserData = nullptr;
    ScriptDiagnosticPhase Phase = ScriptDiagnosticPhase::Callback;
    const char *FailurePrefix = nullptr;
};

struct ScriptSourceSection {
    std::string Name;
    std::string Code;
};

class ScriptModRuntime {
public:
    ScriptModRuntime() = default;
    explicit ScriptModRuntime(std::string moduleName);
    ScriptModRuntime(ScriptModRuntime &&other) noexcept;
    ScriptModRuntime &operator=(ScriptModRuntime &&other) noexcept;

    ScriptModRuntime(const ScriptModRuntime &) = delete;
    ScriptModRuntime &operator=(const ScriptModRuntime &) = delete;

    const std::string &GetModuleName() const { return m_ModuleName; }
    CKAngelScriptObject *GetObject() const { return m_Object; }
    const ::CKAngelScriptAdapter::Api &GetApi() const { return m_Adapter.GetApi(); }
    CKAngelScript *GetAngelScript() const { return m_Adapter.GetAngelScript(); }
    void SetLoaded(bool loaded) { m_Loaded = loaded; }
    void SetOwner(ScriptMod *owner) { m_Owner = owner; }
    ScriptMod *GetOwner() const { return m_Owner; }
    static ScriptMod *GetCurrentScriptMod();
    static bool IsConstructingScriptObject();
    static bool RecordConstructionHostCallViolation(const char *apiName);
    static bool IsInStateHook();
    static ScriptModReloadPhase GetStateHookPhase();
    static bool RecordStateHookHostCallViolation(const char *apiName);
    static bool IsInRenderCallback();
    bool IsModuleLoaded() const { return m_ModuleLoaded; }
    bool HasObject() const { return m_Object != nullptr; }

    bool Refresh(CKContext *context, ScriptDiagnostic &diagnostic);
    bool LoadModule(CKContext *context, const std::string &entryPathUtf8, ScriptDiagnostic &diagnostic);
    bool LoadModuleFromCode(CKContext *context,
                            const std::string &sourceCode,
                            const std::string &entryPathUtf8,
                            ScriptDiagnostic &diagnostic);
    bool LoadModuleFromSections(CKContext *context,
                                const std::vector<ScriptSourceSection> &sections,
                                const std::string &entryPathUtf8,
                                ScriptDiagnostic &diagnostic);
    bool EnumerateMetadata(CKContext *context,
                           CKAngelScriptMetadataCallback callback,
                           void *userData,
                           ScriptDiagnostic &diagnostic);
    bool CreateObject(CKContext *context,
                      const std::string &classNamespace,
                      const std::string &className,
                      ScriptDiagnostic &diagnostic);
    CKAngelScriptMethod *FindMethod(CKContext *context,
                                    const char *methodDecl,
                                    ScriptDiagnostic &diagnostic,
                                    bool optional = true);
    bool ReleaseMethod(CKContext *context,
                       CKAngelScriptMethod *&method,
                       ScriptDiagnostic *diagnostic = nullptr);
    bool CallMethod(CKContext *context,
                    CKAngelScriptMethod *method,
                    CKAngelScriptWriteArgsCallback writeArgs,
                    CKAngelScriptReadResultCallback readResult,
                    CKAngelScriptContextCallback configureContext,
                    CKAngelScriptContextCallback readContextResult,
                    void *userData,
                    ScriptDiagnosticPhase phase,
                    const char *failurePrefix,
                    ScriptDiagnostic &diagnostic);
    bool CallMethod(CKContext *context,
                    const ScriptMethodCall &call,
                    ScriptDiagnostic &diagnostic);
    bool Release(CKContext *context, ScriptDiagnostic *diagnostic = nullptr);

#ifdef BML_SCRIPT_RUNTIME_TEST_ACCESS
    void TestSetActiveCachedApi() {
        m_AngelScript = reinterpret_cast<CKAngelScript *>(1);
        m_Api = &m_Adapter.GetApi();
    }
    const ::CKAngelScriptAdapter::Api *TestCachedApi() const { return m_Api; }
    const ::CKAngelScriptAdapter::Api *TestAdapterApi() const { return &m_Adapter.GetApi(); }
    CKAngelScript *TestAngelScript() const { return m_AngelScript; }
#endif

private:
    void RebindCachedPointersAfterMove() noexcept;
    void ResetMovedFrom() noexcept;

    std::string m_ModuleName;
    ::CKAngelScriptAdapter m_Adapter;
    CKAngelScript *m_AngelScript = nullptr;
    const ::CKAngelScriptAdapter::Api *m_Api = nullptr;
    CKAngelScriptObject *m_Object = nullptr;
    bool m_ModuleLoaded = false;
    bool m_Loaded = false;
    ScriptMod *m_Owner = nullptr;
};

} // namespace BML

#endif
