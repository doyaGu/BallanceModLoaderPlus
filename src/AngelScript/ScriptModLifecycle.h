#ifndef BML_SCRIPTMODLIFECYCLE_H
#define BML_SCRIPTMODLIFECYCLE_H

namespace BML {

enum class ScriptModReloadPhase : int {
    None = 0,
    Unload = 1,
    Load = 2,
    Rollback = 3,
    Recovery = 4,
    Cleanup = 5,
    SaveState = 6,
    MigrateState = 7,
    RestoreState = 8,
};

const char *GetScriptModReloadPhaseName(ScriptModReloadPhase phase);
bool IsScriptModStateHookPhase(ScriptModReloadPhase phase);

} // namespace BML

#endif
