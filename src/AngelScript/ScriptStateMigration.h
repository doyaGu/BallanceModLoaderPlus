#ifndef BML_SCRIPTSTATEMIGRATION_H
#define BML_SCRIPTSTATEMIGRATION_H

#include <string>

#include "ScriptDiagnostic.h"
#include "ScriptModLifecycle.h"
#include "ScriptStateBag.h"

class CKContext;

namespace BML {

class ScriptModRuntime;

class ScriptStateMigration {
public:
    static constexpr const char *SaveStateDecl = "void SaveState(BML::StateBag@)";
    static constexpr const char *MigrateStateDecl = "void MigrateState(const string &in, BML::StateBag@)";
    static constexpr const char *RestoreStateDecl = "void RestoreState(BML::StateBag@)";

    static bool Save(CKContext *context,
                     ScriptModRuntime &runtime,
                     ScriptStateBag &state,
                     bool &called,
                     ScriptDiagnostic &diagnostic);

    static bool Migrate(CKContext *context,
                        ScriptModRuntime &runtime,
                        const std::string &fromVersion,
                        ScriptStateBag &state,
                        bool &called,
                        ScriptDiagnostic &diagnostic);

    static bool Restore(CKContext *context,
                        ScriptModRuntime &runtime,
                        ScriptStateBag &state,
                        bool &called,
                        ScriptDiagnostic &diagnostic);

    static bool HasSaveHook(CKContext *context,
                            ScriptModRuntime &runtime,
                            bool &available,
                            ScriptDiagnostic &diagnostic);

    static bool HasRestoreState(CKContext *context,
                                ScriptModRuntime &runtime,
                                bool &available,
                                ScriptDiagnostic &diagnostic);

    static bool HasRestoreHook(CKContext *context,
                               ScriptModRuntime &runtime,
                               bool &available,
                               ScriptDiagnostic &diagnostic);
};

} // namespace BML

#endif
