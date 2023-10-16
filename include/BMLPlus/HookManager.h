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
    virtual void AddPreClearAllCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePreClearAllCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostClearAllCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePostClearAllCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPreProcessCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePreProcessCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostProcessCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePostProcessCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnSequenceAddedToSceneCallBack(CK_SCENECALLBACK func, void *arg);
    virtual void RemoveOnSequenceAddedToSceneCallBack(CK_SCENECALLBACK func, void *arg);

    virtual void AddOnSequenceRemovedFromSceneCallBack(CK_SCENECALLBACK func, void *arg);
    virtual void RemoveOnSequenceRemovedFromSceneCallBack(CK_SCENECALLBACK func, void *arg);

    virtual void AddPreLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg);
    virtual void RemovePreLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg);

    virtual void AddPostLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg);
    virtual void RemovePostLaunchSceneCallBack(CK_LAUNCHSCENECALLBACK func, void *arg);

    virtual void AddOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemoveOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemoveOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemoveOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemoveOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemoveOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemoveOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnSequenceToBeDeletedCallBack(CK_DELETECALLBACK func, void *arg);
    virtual void RemoveOnSequenceToBeDeletedCallBack(CK_DELETECALLBACK func, void *arg);

    virtual void AddOnSequenceDeletedCallBack(CK_DELETECALLBACK func, void *arg);
    virtual void RemoveOnSequenceDeletedCallBack(CK_DELETECALLBACK func, void *arg);

    virtual void AddPreLoadCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePreLoadCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostLoadCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePostLoadCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPreSaveCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePreSaveCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostSaveCallBack(CK_PROCESSCALLBACK func, void *arg);
    virtual void RemovePostSaveCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddOnPreCopyCallBack(CK_COPYCALLBACK func, void *arg);
    virtual void RemoveOnPreCopyCallBack(CK_COPYCALLBACK func, void *arg);

    virtual void AddOnPostCopyCallBack(CK_COPYCALLBACK func, void *arg);
    virtual void RemoveOnPostCopyCallBack(CK_COPYCALLBACK func, void *arg);

    virtual void AddOnPreRenderCallBack(CK_RENDERCALLBACK func, void *arg);
    virtual void RemoveOnPreRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    virtual void AddOnPostRenderCallBack(CK_RENDERCALLBACK func, void *arg);
    virtual void RemoveOnPostRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    virtual void AddOnPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg);
    virtual void RemoveOnPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    static HookManager *GetManager(CKContext *context) {
        return (HookManager *) context->GetManagerByGuid(HOOKMANAGER_GUID);
    }
};

#endif // BML_HOOKMANAGER_H
