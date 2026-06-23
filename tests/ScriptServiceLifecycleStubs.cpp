#include "AngelScript/CKAngelScriptAdapter.h"
#include "AngelScript/ScriptCallbackEvents.h"
#include "AngelScript/ScriptMod.h"
#include "AngelScript/ScriptModRuntime.h"
#include "CommandContext.h"
#include "ModContext.h"

IMod::~IMod() = default;

ILogger *IMod::GetLogger() {
    return nullptr;
}

IConfig *IMod::GetConfig() {
    return nullptr;
}

ICommand *ModContext::FindCommand(const char *) const {
    return nullptr;
}

const char *CKAngelScriptAdapter::StatusName(CKAS_STATUS) {
    return "CKAS_TEST";
}

namespace BML {

bool CommandContext::RegisterCommand(ICommand *) {
    return false;
}

bool CommandContext::UnregisterCommand(const char *) {
    return false;
}

ScriptCommandEventView::ScriptCommandEventView(ScriptCommandEventPhase,
                                               ICommand *,
                                               const std::vector<std::string> *) {}

ScriptCurrentModScope::ScriptCurrentModScope(ScriptMod *) {}

ScriptCurrentModScope::~ScriptCurrentModScope() = default;

const char *ScriptMod::GetID() {
    return "test.script";
}

void ScriptMod::SetLoadFailure(const ScriptDiagnostic &) {}

void ScriptMod::RecordScriptDiagnostic(const ScriptDiagnostic &) {}

bool ScriptMod::UnregisterScriptCommand(const std::string &) {
    return false;
}

bool ScriptMod::EnterScriptCall() const {
    return true;
}

bool ScriptMod::CanDispatchScriptServiceCallback() {
    return true;
}

void ScriptMod::LeaveScriptCall() const {}

} // namespace BML
