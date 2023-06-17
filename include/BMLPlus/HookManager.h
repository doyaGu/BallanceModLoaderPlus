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
    virtual void AddPreClearAllCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePreClearAllCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddPostClearAllCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePostClearAllCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddPreProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePreProcessCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddPostProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePostProcessCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnSequenceAddedToSceneCallBack(CK_SCENECALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnSequenceAddedToSceneCallBack(CK_SCENECALLBACK func, void *arg) = 0;

    virtual void AddOnSequenceRemovedFromSceneCallBack(CK_SCENECALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnSequenceRemovedFromSceneCallBack(CK_SCENECALLBACK func, void *arg) = 0;

    virtual void AddPreLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePreLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg) = 0;

    virtual void AddPostLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePostLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg) = 0;

    virtual void AddOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnSequenceToBeDeletedCallBack(CK_DELETECALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnSequenceToBeDeletedCallBack(CK_DELETECALLBACK func, void *arg) = 0;

    virtual void AddOnSequenceDeletedCallBack(CK_DELETECALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnSequenceDeletedCallBack(CK_DELETECALLBACK func, void *arg) = 0;

    virtual void AddPreLoadCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePreLoadCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddPostLoadCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePostLoadCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddPreSaveCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePreSaveCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddPostSaveCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemovePostSaveCallBack(CK_PROCESSCALLBACK func, void *arg) = 0;

    virtual void AddOnPreCopyCallBack(CK_COPYCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnPreCopyCallBack(CK_COPYCALLBACK func, void *arg) = 0;

    virtual void AddOnPostCopyCallBack(CK_COPYCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnPostCopyCallBack(CK_COPYCALLBACK func, void *arg) = 0;

    virtual void AddOnPreRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnPreRenderCallBack(CK_RENDERCALLBACK func, void *arg) = 0;

    virtual void AddOnPostRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnPostRenderCallBack(CK_RENDERCALLBACK func, void *arg) = 0;

    virtual void AddOnPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE) = 0;
    virtual void RemoveOnPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg) = 0;

    static HookManager *GetManager(CKContext *context) {
        return (HookManager *) context->GetManagerByGuid(HOOKMANAGER_GUID);
    }
};

#endif // BML_HOOKMANAGER_H
