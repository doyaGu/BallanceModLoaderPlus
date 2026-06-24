#include "AngelScript/AngelScriptImGuiBindings.h"
#include "AngelScript/ScriptDiagnostic.h"
#include "AngelScript/ScriptMod.h"

IMod::~IMod() = default;

ILogger *IMod::GetLogger() {
    return nullptr;
}

IConfig *IMod::GetConfig() {
    return nullptr;
}

CKBOOL BMLImGuiASBeginCallbackRecovery(BMLImGuiASCallbackRecoveryScope *) {
    return FALSE;
}

void BMLImGuiASEndCallbackRecovery(BMLImGuiASCallbackRecoveryScope *, const char *, const char *) {}

namespace BML {

const char *ScriptMod::GetID() {
    return "test.script";
}

void ScriptMod::SetLoadFailure(const ScriptDiagnostic &) {}

void ScriptMod::RecordScriptDiagnostic(const ScriptDiagnostic &) {}

bool ScriptMod::EnterScriptCall() const {
    return true;
}

void ScriptMod::LeaveScriptCall() const {}

} // namespace BML
