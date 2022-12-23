#include "HookManager.h"

#define BML_TRIGGER_CALLBACK(Name, Type, ...) \
    for (Callback *cb = m_##Name##Callbacks.Begin(); cb != m_##Name##Callbacks.End(); ++cb) { \
        ((Type)cb->callback)(__VA_ARGS__, cb->argument);                            \
        if (cb->temp) m_##Name##Callbacks.Remove(cb);                               \
    }

HookManager::HookManager(CKContext *ctx) : CKBaseManager(ctx, BML_HOOKMANAGER_GUID, "Hook Manager") {
    ctx->RegisterNewManager(this);
}

HookManager::~HookManager() = default;

CKERROR HookManager::PreProcess() {
    BML_TRIGGER_CALLBACK(PreProcess, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::PostProcess() {
    BML_TRIGGER_CALLBACK(PostProcess, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::OnCKInit() {
    BML_TRIGGER_CALLBACK(OnCKInit, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::OnCKEnd() {
    BML_TRIGGER_CALLBACK(OnCKEnd, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::OnCKReset() {
    BML_TRIGGER_CALLBACK(OnCKReset, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::OnCKPostReset() {
    BML_TRIGGER_CALLBACK(OnCKPostReset, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::OnCKPause() {
    BML_TRIGGER_CALLBACK(OnCKPause, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::OnCKPlay() {
    BML_TRIGGER_CALLBACK(OnCKPlay, CK_PROCESSCALLBACK, m_Context)
    return CK_OK;
}

CKERROR HookManager::OnPreRender(CKRenderContext *dev) {
    BML_TRIGGER_CALLBACK(OnPreRender, CK_RENDERCALLBACK, dev)
    return CK_OK;
}

CKERROR HookManager::OnPostRender(CKRenderContext *dev) {
    BML_TRIGGER_CALLBACK(OnPostRender, CK_RENDERCALLBACK, dev)
    return CK_OK;
}

CKERROR HookManager::OnPostSpriteRender(CKRenderContext *dev) {
    BML_TRIGGER_CALLBACK(OnPostSpriteRender, CK_RENDERCALLBACK, dev)
    return CK_OK;
}

int HookManager::GetFunctionPriority(CKMANAGER_FUNCTIONS Function) {
    switch (Function) {
        case CKMANAGER_FUNC_PreProcess:
            return 1000;
        case CKMANAGER_FUNC_PostProcess:
            return -1000;
        default:
            break;
    }
    return 0;
}

CKDWORD HookManager::GetValidFunctionsMask() {
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

#undef BML_TRIGGER_CALLBACK

#define BML_ADD_CALLBACK(Name, Func, Arg, Temp) \
    Callback cb = {(void *)Func, Arg, Temp};    \
    m_##Name##Callbacks.PushBack(cb)

#define BML_REMOVE_CALLBACK(Name, Func, Arg) \
    Callback cb = {(void *)Func, Arg, FALSE};\
    m_##Name##Callbacks.Remove(cb)

void HookManager::AddPreProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(PreProcess, func, arg, temp);
}

void HookManager::RemovePreProcessCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(PreProcess, func, arg);
}

void HookManager::AddPostProcessCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(PostProcess, func, arg, temp);
}

void HookManager::RemovePostProcessCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(PostProcess, func, arg);
}

void HookManager::AddOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnCKInit, func, arg, temp);
}

void HookManager::RemoveOnCKInitCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnCKInit, func, arg);
}

void HookManager::AddOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnCKEnd, func, arg, temp);
}

void HookManager::RemoveOnCKEndCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnCKEnd, func, arg);
}

void HookManager::AddOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnCKReset, func, arg, temp);
}

void HookManager::RemoveOnCKResetCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnCKReset, func, arg);
}

void HookManager::AddOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnCKPostReset, func, arg, temp);
}

void HookManager::RemoveOnCKPostResetCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnCKPostReset, func, arg);
}

void HookManager::AddOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnCKPause, func, arg, temp);
}

void HookManager::RemoveOnCKPauseCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnCKPause, func, arg);
}

void HookManager::AddOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnCKPlay, func, arg, temp);
}

void HookManager::RemoveOnCKPlayCallBack(CK_PROCESSCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnCKPlay, func, arg);
}

void HookManager::AddPreRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnPreRender, func, arg, temp);
}

void HookManager::RemovePreRenderCallBack(CK_RENDERCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnPreRender, func, arg);
}

void HookManager::AddPostRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnPostRender, func, arg, temp);
}

void HookManager::RemovePostRenderCallBack(CK_RENDERCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnPostRender, func, arg);
}

void HookManager::AddPostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg, CKBOOL temp) {
    BML_ADD_CALLBACK(OnPostSpriteRender, func, arg, temp);
}

void HookManager::RemovePostSpriteRenderCallBack(CK_RENDERCALLBACK func, void *arg) {
    BML_REMOVE_CALLBACK(OnPostSpriteRender, func, arg);
}

#undef BML_ADD_CALLBACK
#undef BML_REMOVE_CALLBACK