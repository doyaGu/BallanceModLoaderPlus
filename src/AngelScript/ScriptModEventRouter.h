#ifndef BML_SCRIPTMODEVENTROUTER_H
#define BML_SCRIPTMODEVENTROUTER_H

#include <cstddef>
#include <string>
#include <vector>

#include "BML/ICommand.h"
#include "BML/IMod.h"
#include "ScriptCallbackDispatcher.h"
#include "ScriptCallbackEvents.h"
#include "ScriptDiagnostic.h"

namespace BML {

class ScriptModContextView;
class ScriptModRuntime;

class ScriptModEventRouter {
public:
    void Bind(CKContext *context, ScriptModRuntime *runtime, ScriptModContextView *contextView);
    bool Cache(ScriptDiagnostic &diagnostic);
    bool Release(ScriptDiagnostic *diagnostic = nullptr);

    bool HasCallback(ScriptCallbackId id) const;
    bool HasOnProcess() const { return HasCallback(ScriptCallbackOnProcess); }
    bool HasOnRender() const { return HasCallback(ScriptCallbackOnRender); }
    bool HasGameEvent() const { return HasCallback(ScriptCallbackOnGameEvent); }
    void GetCallbackNames(std::vector<std::string> &out) const;

    bool CallOnLoad(ScriptDiagnostic &diagnostic);
    bool CallOnUnload(ScriptDiagnostic &diagnostic);
    bool CallOnProcess(ScriptDiagnostic &diagnostic);
    bool CallGameEvent(size_t eventIndex, ScriptDiagnostic &diagnostic);
    bool CallRender(CK_RENDER_FLAGS flags, ScriptDiagnostic &diagnostic);
    bool CallLoadObject(const char *filename,
                        CKBOOL isMap,
                        const char *masterName,
                        CK_CLASSID filterClass,
                        CKBOOL addToScene,
                        CKBOOL reuseMeshes,
                        CKBOOL reuseMaterials,
                        CKBOOL dynamic,
                        XObjectArray *objectArray,
                        CKObject *masterObject,
                        ScriptDiagnostic &diagnostic);
    bool CallLoadScript(const char *filename, CKBehavior *script, ScriptDiagnostic &diagnostic);
    bool CallCheatEnabled(bool enable, ScriptDiagnostic &diagnostic);
    bool CallCommandEvent(bool beforeCommand,
                          ICommand *command,
                          const std::vector<std::string> &args,
                          ScriptDiagnostic &diagnostic);
    bool CallModifyConfig(const char *modId,
                          const char *category,
                          const char *key,
                          IProperty *property,
                          ScriptDiagnostic &diagnostic);
    bool CallPhysicalize(CK3dEntity *target,
                         CKBOOL fixed,
                         float friction,
                         float elasticity,
                         float mass,
                         const char *collGroup,
                         CKBOOL startFrozen,
                         CKBOOL enableColl,
                         CKBOOL calcMassCenter,
                         float linearDamp,
                         float rotDamp,
                         const char *collSurface,
                         VxVector massCenter,
                         int convexCnt,
                         CKMesh **convexMesh,
                         int ballCnt,
                         VxVector *ballCenter,
                         float *ballRadius,
                         int concaveCnt,
                         CKMesh **concaveMesh,
                         ScriptDiagnostic &diagnostic);
    bool CallUnphysicalize(CK3dEntity *target, ScriptDiagnostic &diagnostic);

private:
    bool IsBound() const;
    bool RequireBound(ScriptDiagnostic &diagnostic) const;

    CKContext *m_Context = nullptr;
    ScriptModRuntime *m_Runtime = nullptr;
    ScriptModContextView *m_ContextView = nullptr;
    ScriptCallbackDispatcher m_Dispatcher;
};

} // namespace BML

#endif
