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

BMLImGuiASCallScope::~BMLImGuiASCallScope() = default;

bool BMLImGuiASCallScope::Begin() {
    return true;
}

void BMLImGuiASCallScope::End() {}

BMLImGuiASCallbackRecoveryScope::~BMLImGuiASCallbackRecoveryScope() = default;

bool BMLImGuiASCallbackRecoveryScope::Begin() {
    return false;
}

void BMLImGuiASCallbackRecoveryScope::End(const char *, const char *) {}

bool BMLImGuiASBeginCall(BMLImGuiASCallScope *scope) {
    return scope ? scope->Begin() : false;
}

void BMLImGuiASEndCall(BMLImGuiASCallScope *scope) {
    if (scope)
        scope->End();
}

bool BMLImGuiASBeginCallbackRecovery(BMLImGuiASCallbackRecoveryScope *) {
    return false;
}

void BMLImGuiASEndCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope, const char *modId, const char *phase) {
    if (scope)
        scope->End(modId, phase);
}

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
