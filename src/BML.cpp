#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "ModLoader.h"

#include "Hooks.h"

#define BML_HOOK_CODE 0x00424D4C
#define BML_HOOK_VERSION 0x00000001

HMODULE g_DllHandle = nullptr;
HookApi *g_HookApi = nullptr;
CKContext *g_CKContext = nullptr;

CKERROR PostProcess(void *arg) {
    return ModLoader::GetInstance().PostProcess();
}

CKERROR OnCKInit(void *arg) {
    ModLoader::GetInstance().Init(g_CKContext);
    return CK_OK;
}

CKERROR OnCKEnd(void *arg) {
    ModLoader::GetInstance().Shutdown();
    return CK_OK;
}

CKERROR OnCKReset(void *arg) {
    return ModLoader::GetInstance().OnCKReset();
}

CKERROR OnCKPostReset(void *arg) {
    return ModLoader::GetInstance().OnCKPostReset();
}

CKERROR OnPostRender(CKRenderContext *dev, void *arg) {
    return ModLoader::GetInstance().OnPostRender(dev);
}

CKERROR OnPostSpriteRender(CKRenderContext *dev, void *arg) {
    return ModLoader::GetInstance().OnPostSpriteRender(dev);
}

static int OnError(HookModuleErrorCode code, void *data1, void *data2) {
    return HMR_OK;
}

static int OnQuery(HookModuleQueryCode code, void *data1, void *data2) {
    switch (code) {
        case HMQC_ABI:
            *reinterpret_cast<int *>(data2) = HOOKS_ABI_VERSION;
            break;
        case HMQC_CODE:
            *reinterpret_cast<int *>(data2) = BML_HOOK_CODE;
            break;
        case HMQC_VERSION:
            *reinterpret_cast<int *>(data2) = BML_HOOK_VERSION;
            break;
        case HMQC_CK2:
            *reinterpret_cast<int *>(data2) =
                    CKHF_PostProcess |
                    CKHF_OnCKInit |
                    CKHF_OnCKEnd |
                    CKHF_OnCKReset |
                    CKHF_OnCKPostReset |
                    CKHF_OnPostRender |
                    CKHF_OnPostSpriteRender;
            break;
        default:
            return HMR_SKIP;
    }

    return HMR_OK;
}

static int OnPost(HookModulePostCode code, void *data1, void *data2) {
    switch (code) {
        case HMPC_API:
            g_HookApi = (HookApi *) data2;
        case HMPC_CKCONTEXT:
            g_CKContext = (CKContext *) data2;
            break;
        default:
            return HMR_SKIP;
    }
    return HMR_OK;
}

static int OnLoad(size_t code, void * /* handle */) {
    return HMR_OK;
}

static int OnUnload(size_t code, void * /* handle */) {
    return HMR_OK;
}

static int OnSet(size_t code, void **pcb, void **parg) {
    switch (code) {
        case CKHFI_PostProcess:
            *pcb = reinterpret_cast<void*>(PostProcess);
            *parg = nullptr;
            break;
        case CKHFI_OnCKInit:
            *pcb = reinterpret_cast<void*>(OnCKInit);
            *parg = nullptr;
            break;
        case CKHFI_OnCKEnd:
            *pcb = reinterpret_cast<void*>(OnCKEnd);
            *parg = nullptr;
            break;
        case CKHFI_OnCKReset:
            *pcb = reinterpret_cast<void *>(OnCKReset);
            *parg = nullptr;
            break;
        case CKHFI_OnCKPostReset:
            *pcb = reinterpret_cast<void*>(OnCKPostReset);
            *parg = nullptr;
            break;
        case CKHFI_OnPostRender:
            *pcb = reinterpret_cast<void*>(OnPostRender);
            *parg = nullptr;
            break;
        case CKHFI_OnPostSpriteRender:
            *pcb = reinterpret_cast<void*>(OnPostSpriteRender);
            *parg = nullptr;
            break;
        default:
            break;
    }
    return HMR_OK;
}

static int OnUnset(size_t code, void **pcb, void **parg) {
    switch (code) {
        case CKHFI_PostProcess:
            *pcb = reinterpret_cast<void*>(PostProcess);
            *parg = nullptr;
            break;
        case CKHFI_OnCKInit:
            *pcb = reinterpret_cast<void*>(OnCKInit);
            *parg = nullptr;
            break;
        case CKHFI_OnCKEnd:
            *pcb = reinterpret_cast<void*>(OnCKEnd);
            *parg = nullptr;
            break;
        case CKHFI_OnCKReset:
            *pcb = reinterpret_cast<void *>(OnCKReset);
            *parg = nullptr;
            break;
        case CKHFI_OnCKPostReset:
            *pcb = reinterpret_cast<void*>(OnCKPostReset);
            *parg = nullptr;
            break;
        case CKHFI_OnPostRender:
            *pcb = reinterpret_cast<void*>(OnPostRender);
            *parg = nullptr;
            break;
        case CKHFI_OnPostSpriteRender:
            *pcb = reinterpret_cast<void*>(OnPostSpriteRender);
            *parg = nullptr;
            break;
        default:
            break;
    }
    return HMR_OK;
}

HOOKS_EXPORT int BMLHandler(size_t action, size_t code, uintptr_t param1, uintptr_t param2) {
    switch (action) {
        case HMA_ERROR:
            return OnError(static_cast<HookModuleErrorCode>(code), reinterpret_cast<void *>(param1), reinterpret_cast<void *>(param2));
        case HMA_QUERY:
            return OnQuery(static_cast<HookModuleQueryCode>(code), reinterpret_cast<void *>(param1), reinterpret_cast<void *>(param2));
        case HMA_POST:
            return OnPost(static_cast<HookModulePostCode>(code), reinterpret_cast<void *>(param1), reinterpret_cast<void *>(param2));
        case HMA_LOAD:
            return OnLoad(code, reinterpret_cast<void *>(param2));
        case HMA_UNLOAD:
            return OnUnload(code, reinterpret_cast<void *>(param2));
        case HMA_SET:
            return OnSet(code, reinterpret_cast<void **>(param1), reinterpret_cast<void **>(param2));
        case HMA_UNSET:
            return OnUnset(code, reinterpret_cast<void **>(param1), reinterpret_cast<void **>(param2));
        default:
            return HMR_SKIP;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_DllHandle = hModule;
            break;
        case DLL_PROCESS_DETACH:
            g_DllHandle = nullptr;
            break;
        default:
            break;
    }
    return TRUE;
}