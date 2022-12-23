#ifndef BML_HOOKMANAGER_H
#define BML_HOOKMANAGER_H

#include "CKContext.h"
#include "CKBaseManager.h"

#define BML_HOOKMANAGER_GUID CKGUID(0x32a40332, 0x3bf12a51)

typedef void (*CK_PROCESSCALLBACK)(CKContext *context, void *Argument);

class HookManager : public CKBaseManager {
public:
    explicit HookManager(CKContext *ctx);

    ~HookManager() override;

    CKERROR PreProcess() override;
    CKERROR PostProcess() override;

    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;
    CKERROR OnCKReset() override;
    CKERROR OnCKPostReset() override;
    CKERROR OnCKPause() override;
    CKERROR OnCKPlay() override;

    CKERROR OnPreRender(CKRenderContext *dev) override;
    CKERROR OnPostRender(CKRenderContext *dev) override;
    CKERROR OnPostSpriteRender(CKRenderContext *dev) override;

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_PreProcess |
               CKMANAGER_FUNC_PostProcess |
               CKMANAGER_FUNC_OnCKInit |
               CKMANAGER_FUNC_OnCKEnd |
               CKMANAGER_FUNC_OnCKPlay |
               CKMANAGER_FUNC_OnCKPause |
               CKMANAGER_FUNC_OnCKReset |
               CKMANAGER_FUNC_OnCKPostReset |
               CKMANAGER_FUNC_OnPreRender |
               CKMANAGER_FUNC_OnPostRender |
               CKMANAGER_FUNC_OnPostSpriteRender;
    }

    virtual void AddPreProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePreProcessCallBack(CK_PROCESSCALLBACK func, void *arg);

    virtual void AddPostProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostProcessCallBack(CK_PROCESSCALLBACK func, void *arg);

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

    virtual void AddPreRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePreRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    virtual void AddPostRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    virtual void AddPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp = FALSE);
    virtual void RemovePostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg);

    static HookManager *GetManager(CKContext *context) {
        return (HookManager *) context->GetManagerByGuid(BML_HOOKMANAGER_GUID);
    }

private:
    struct Callback {
        void *callback;
        void *argument;
        CKBOOL temp;

        bool operator==(const Callback &rhs) const {
            return callback == rhs.callback && argument == rhs.argument;
        }

        bool operator!=(const Callback &rhs) const {
            return !(rhs == *this);
        }
    };

#define BML_NEW_CALLBACKS(Name, ...) \
    XArray<Callback> m_##Name##Callbacks

    BML_NEW_CALLBACKS(PreProcess);
    BML_NEW_CALLBACKS(PostProcess);
    BML_NEW_CALLBACKS(OnCKInit);
    BML_NEW_CALLBACKS(OnCKEnd);
    BML_NEW_CALLBACKS(OnCKReset);
    BML_NEW_CALLBACKS(OnCKPostReset);
    BML_NEW_CALLBACKS(OnCKPause);
    BML_NEW_CALLBACKS(OnCKPlay);
    BML_NEW_CALLBACKS(OnPreRender, CKRenderContext *);
    BML_NEW_CALLBACKS(OnPostRender, CKRenderContext *);
    BML_NEW_CALLBACKS(OnPostSpriteRender, CKRenderContext *);

#undef BML_NEW_CALLBACKS
};

#endif // BML_HOOKMANAGER_H
