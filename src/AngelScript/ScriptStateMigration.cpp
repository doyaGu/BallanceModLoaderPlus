#include "ScriptStateMigration.h"

#include "ScriptModRuntime.h"

namespace BML {

namespace {

struct StateOnlyArgs {
    const CKAngelScriptAdapter::Api *Api = nullptr;
    ScriptStateBag *State = nullptr;
};

struct MigrateStateArgs {
    const CKAngelScriptAdapter::Api *Api = nullptr;
    const std::string *FromVersion = nullptr;
    ScriptStateBag *State = nullptr;
};

static CKAS_STATUS WriteStateOnlyArgs(CKAngelScriptArgWriter *writer, void *userData) {
    auto *args = static_cast<StateOnlyArgs *>(userData);
    if (!args || !args->Api || !args->Api->ArgSetObjectHandle || !args->State)
        return CKAS_INVALIDARGUMENT;
    return args->Api->ArgSetObjectHandle(writer, 0, args->State);
}

static CKAS_STATUS WriteMigrateStateArgs(CKAngelScriptArgWriter *writer, void *userData) {
    auto *args = static_cast<MigrateStateArgs *>(userData);
    if (!args || !args->Api || !args->Api->ArgSetString || !args->Api->ArgSetObjectHandle ||
        !args->FromVersion || !args->State) {
        return CKAS_INVALIDARGUMENT;
    }

    CKAS_STATUS status = args->Api->ArgSetString(writer, 0, args->FromVersion->c_str());
    if (status != CKAS_OK)
        return status;
    return args->Api->ArgSetObjectHandle(writer, 1, args->State);
}

static bool CallOptionalStateMethod(CKContext *context,
                                    ScriptModRuntime &runtime,
                                    const char *decl,
                                    CKAngelScriptWriteArgsCallback writeArgs,
                                    void *userData,
                                    const char *failurePrefix,
                                    bool &called,
                                    ScriptDiagnostic &diagnostic) {
    called = false;
    CKAngelScriptMethod *method = runtime.FindMethod(context, decl, diagnostic);
    if (!method)
        return diagnostic.Status == CKAS_OK || diagnostic.Status == CKAS_NOTFOUND || diagnostic.Message.empty();

    ScriptMethodCall call;
    call.Method = method;
    call.WriteArgs = writeArgs;
    call.UserData = userData;
    call.Phase = ScriptDiagnosticPhase::Runtime;
    call.FailurePrefix = failurePrefix;
    const bool ok = runtime.CallMethod(context, call, diagnostic);
    called = ok;

    ScriptDiagnostic releaseDiagnostic;
    runtime.ReleaseMethod(context, method, &releaseDiagnostic);
    if (!ok)
        return false;
    if (!releaseDiagnostic.Message.empty()) {
        diagnostic = releaseDiagnostic;
        return false;
    }
    return true;
}

static bool HasStateMethod(CKContext *context,
                           ScriptModRuntime &runtime,
                           const char *decl,
                           bool &available,
                           ScriptDiagnostic &diagnostic) {
    CKAngelScriptMethod *method = runtime.FindMethod(context, decl, diagnostic);
    if (!method)
        return diagnostic.Status == CKAS_OK || diagnostic.Status == CKAS_NOTFOUND || diagnostic.Message.empty();

    available = true;
    ScriptDiagnostic releaseDiagnostic;
    runtime.ReleaseMethod(context, method, &releaseDiagnostic);
    if (!releaseDiagnostic.Message.empty()) {
        diagnostic = releaseDiagnostic;
        return false;
    }
    return true;
}

} // namespace

bool ScriptStateMigration::Save(CKContext *context,
                                ScriptModRuntime &runtime,
                                ScriptStateBag &state,
                                bool &called,
                                ScriptDiagnostic &diagnostic) {
    StateOnlyArgs args = {&runtime.GetApi(), &state};
    return CallOptionalStateMethod(context,
                                   runtime,
                                   SaveStateDecl,
                                   WriteStateOnlyArgs,
                                   &args,
                                   "SaveState failed",
                                   called,
                                   diagnostic);
}

bool ScriptStateMigration::Restore(CKContext *context,
                                   ScriptModRuntime &runtime,
                                   const std::string &fromVersion,
                                   ScriptStateBag &state,
                                   bool &called,
                                   ScriptDiagnostic &diagnostic) {
    called = false;

    bool migrated = false;
    MigrateStateArgs migrateArgs = {&runtime.GetApi(), &fromVersion, &state};
    if (!CallOptionalStateMethod(context,
                                 runtime,
                                 MigrateStateDecl,
                                 WriteMigrateStateArgs,
                                 &migrateArgs,
                                 "MigrateState failed",
                                 migrated,
                                 diagnostic)) {
        return false;
    }

    bool restored = false;
    StateOnlyArgs restoreArgs = {&runtime.GetApi(), &state};
    if (!CallOptionalStateMethod(context,
                                 runtime,
                                 RestoreStateDecl,
                                 WriteStateOnlyArgs,
                                 &restoreArgs,
                                 "RestoreState failed",
                                 restored,
                                 diagnostic)) {
        return false;
    }

    called = migrated || restored;
    return true;
}

bool ScriptStateMigration::HasRestoreHook(CKContext *context,
                                          ScriptModRuntime &runtime,
                                          bool &available,
                                          ScriptDiagnostic &diagnostic) {
    available = false;
    bool hasMigrate = false;
    if (!HasStateMethod(context, runtime, MigrateStateDecl, hasMigrate, diagnostic))
        return false;

    bool hasRestore = false;
    if (!HasStateMethod(context, runtime, RestoreStateDecl, hasRestore, diagnostic))
        return false;

    available = hasMigrate || hasRestore;
    return true;
}

} // namespace BML
