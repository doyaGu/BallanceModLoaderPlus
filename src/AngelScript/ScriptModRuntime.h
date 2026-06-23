#ifndef BML_SCRIPTMODRUNTIME_H
#define BML_SCRIPTMODRUNTIME_H

#include <string>

#include "CKAngelScriptAdapter.h"
#include "ScriptDiagnostic.h"

namespace BML {

class ScriptMod;

class ScriptCurrentModScope {
public:
    explicit ScriptCurrentModScope(ScriptMod *owner);
    ~ScriptCurrentModScope();

    ScriptCurrentModScope(const ScriptCurrentModScope &) = delete;
    ScriptCurrentModScope &operator=(const ScriptCurrentModScope &) = delete;

private:
    ScriptMod *m_Previous = nullptr;
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
    static ScriptMod *GetCurrentScriptMod();
    static bool IsInRenderCallback();
    bool IsModuleLoaded() const { return m_ModuleLoaded; }
    bool HasObject() const { return m_Object != nullptr; }

    bool Refresh(CKContext *context, ScriptDiagnostic &diagnostic);
    bool LoadModule(CKContext *context, const std::string &entryPathUtf8, ScriptDiagnostic &diagnostic);
    bool LoadModuleFromCode(CKContext *context,
                            const std::string &sourceCode,
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

private:
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
