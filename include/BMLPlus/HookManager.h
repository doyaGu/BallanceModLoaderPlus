#ifndef BML_HOOKMANAGER_H
#define BML_HOOKMANAGER_H

#include "CKBaseManager.h"
#include "CKContext.h"

#define HOOKMANAGER_GUID CKGUID(0x32a40332, 0x3bf12a51)

typedef void (*CK_PROCESSCALLBACK)(void *);
typedef void (*CK_SCENECALLBACK)(CKScene *, CK_ID *, int, void *);
typedef void (*CK_LAUNCHSCENECALLBACK)(CKScene *, CKScene *, void *);
typedef void (*CK_DELETECALLBACK)(CK_ID *, int, void *);
typedef void (*CK_COPYCALLBACK)(CKDependenciesContext *, void *);

class HookManager : public CKBaseManager {
public:
    explicit HookManager(CKContext *context);

    ~HookManager() override;

    CKERROR PreClearAll() override;
    CKERROR PostClearAll() override;

    CKERROR PreProcess() override;
    CKERROR PostProcess() override;

    CKERROR SequenceAddedToScene(CKScene *scn, CK_ID *objids, int count) override;
    CKERROR SequenceRemovedFromScene(CKScene *scn, CK_ID *objids, int count) override;

    CKERROR PreLaunchScene(CKScene *OldScene, CKScene *NewScene) override;
    CKERROR PostLaunchScene(CKScene *OldScene, CKScene *NewScene) override;

    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;

    CKERROR OnCKReset() override;
    CKERROR OnCKPostReset() override;

    CKERROR OnCKPause() override;
    CKERROR OnCKPlay() override;

    CKERROR SequenceToBeDeleted(CK_ID *objids, int count) override;
    CKERROR SequenceDeleted(CK_ID *objids, int count) override;

    CKERROR PreLoad() override;
    CKERROR PostLoad() override;

    CKERROR PreSave() override;
    CKERROR PostSave() override;

    CKERROR OnPreCopy(CKDependenciesContext &context) override;
    CKERROR OnPostCopy(CKDependenciesContext &context) override;

    CKERROR OnPreRender(CKRenderContext *dev) override;
    CKERROR OnPostRender(CKRenderContext *dev) override;
    CKERROR OnPostSpriteRender(CKRenderContext *dev) override;

    CKDWORD GetValidFunctionsMask() override;

    virtual void AddPreClearAllCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePreClearAllCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostClearAllCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostClearAllCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPreProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePreProcessCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostProcessCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnSequenceAddedToSceneCallBack(CK_SCENECALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnSequenceAddedToSceneCallBack(CK_SCENECALLBACK func, void *arg);

    virtual void AddOnSequenceRemovedFromSceneCallBack(CK_SCENECALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnSequenceRemovedFromSceneCallBack(CK_SCENECALLBACK func, void *arg);

    virtual void AddPreLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePreLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg);

    virtual void AddPostLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg);

    virtual void AddOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnSequenceToBeDeletedCallBack(CK_DELETECALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnSequenceToBeDeletedCallBack(CK_DELETECALLBACK func, void *arg);

    virtual void AddOnSequenceDeletedCallBack(CK_DELETECALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnSequenceDeletedCallBack(CK_DELETECALLBACK func, void *arg);

    virtual void AddPreLoadCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePreLoadCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostLoadCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostLoadCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPreSaveCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePreSaveCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostSaveCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostSaveCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnPreCopyCallBack(CK_COPYCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnPreCopyCallBack(CK_COPYCALLBACK func, void *arg);

    virtual void AddOnPostCopyCallBack(CK_COPYCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnPostCopyCallBack(CK_COPYCALLBACK func, void *arg);

    virtual void AddOnPreRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnPreRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    virtual void AddOnPostRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnPostRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    virtual void AddOnPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemoveOnPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    static HookManager *GetManager(CKContext *context) {
        return (HookManager *) context->GetManagerByGuid(HOOKMANAGER_GUID);
    }
};

#endif // BML_HOOKMANAGER_H
