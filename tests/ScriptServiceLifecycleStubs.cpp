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

bool ModContext::GetCommandInfo(int, BML::CommandContext::CommandInfo &) const {
    return false;
}

bool ModContext::FindCommandInfo(
    const char *, BML::CommandContext::CommandInfo &) const {
    return false;
}

const char *CKAngelScriptAdapter::StatusName(CKAS_STATUS) {
    return "CKAS_TEST";
}

namespace BML {

bool CommandContext::RegisterCommand(const void *, ICommand *) {
    return false;
}

bool CommandContext::IsValidCommandAlias(const char *alias) {
    return alias && alias[0] != '\0';
}

bool CommandContext::IsValidCommandName(const char *name) {
    return name && name[0] != '\0';
}

std::string CommandContext::NormalizeCommandName(const char *name) {
    std::string normalized = name ? name : "";
    for (char &ch : normalized) {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }
    return normalized;
}

CommandContext::UnregisterResult CommandContext::UnregisterCommand(const void *, const char *) {
    return UnregisterResult::NotFound;
}

void CommandContext::UnregisterCommands(const void *) {}

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

bool ScriptModRuntime::RecordConstructionHostCallViolation(const char *) {
    return false;
}

bool ScriptModRuntime::RecordStateHookHostCallViolation(const char *) {
    return false;
}

} // namespace BML
