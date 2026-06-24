#include "ScriptModLifecycle.h"

namespace BML {

const char *GetScriptModReloadPhaseName(ScriptModReloadPhase phase) {
    switch (phase) {
    case ScriptModReloadPhase::None:
        return "none";
    case ScriptModReloadPhase::Unload:
        return "unload";
    case ScriptModReloadPhase::Load:
        return "load";
    case ScriptModReloadPhase::Rollback:
        return "rollback";
    case ScriptModReloadPhase::Recovery:
        return "recovery";
    case ScriptModReloadPhase::Cleanup:
        return "cleanup";
    case ScriptModReloadPhase::SaveState:
        return "save-state";
    case ScriptModReloadPhase::MigrateState:
        return "migrate-state";
    case ScriptModReloadPhase::RestoreState:
        return "restore-state";
    }
    return "unknown";
}

bool IsScriptModStateHookPhase(ScriptModReloadPhase phase) {
    return phase == ScriptModReloadPhase::SaveState ||
           phase == ScriptModReloadPhase::MigrateState ||
           phase == ScriptModReloadPhase::RestoreState;
}

} // namespace BML
