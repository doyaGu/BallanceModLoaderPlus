#ifndef HOOKS_H
#define HOOKS_H

#define HOOKS_MAJOR_VER 0
#define HOOKS_MINOR_VER 3
#define HOOKS_PATCH_VER 0
#define HOOKS_VERSION "0.3.0"

#if (defined(_MSC_VER) || defined(__MINGW32__))
#define HOOKS_EXPORT extern "C" __declspec(dllexport)
#else
#define HOOKS_EXPORT extern "C" __attribute__((__visibility__("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define HOOKS_ABI_VERSION 1

typedef enum HookModuleAction {
    HMA_ERROR = 0,
    HMA_QUERY = 1,
    HMA_POST = 2,
    HMA_PROMPT = 3,
    HMA_LOAD = 4,
    HMA_UNLOAD = 5,
    HMA_SET = 6,
    HMA_UNSET = 7,
} HookModuleAction;

typedef enum HookModuleReturn {
    HMR_OK = 0,
    HMR_FAIL = 1,
    HMR_SKIP = 2,
    HMR_FREEZE = 3,
    HMR_REVIVE = 4,
    HMR_TERMINATE = 5,

    HMR_INTERNALERROR = 0xA000,
    HMR_NOTLOADED = 0xA001,
    HMR_DISABLED = 0xA002,
} HookModuleReturn;

typedef enum HookModuleErrorCode {
    HMEC_USER = 0x100,
} HookModuleErrorCode;

typedef enum HookModuleQueryCode {
    HMQC_ABI = 0,
    HMQC_CODE = 1,
    HMQC_VERSION = 2,

    HMQC_CK2 = 0x20,
    HMQC_WINDOW = 0x30,
} HookModuleQueryCode;

typedef enum HookModulePostCode {
    HMPC_API = 0x10,

    HMPC_CKCONTEXT = 0x40,
    HMPC_WINDOW = 0x41,
} HookModulePostCode;

typedef int (*HookHandlerFunction)(size_t action, size_t code, uintptr_t param1, uintptr_t param2);

typedef enum CKHookFunctions {
    CKHF_OnSequenceToBeDeleted = 0x00000001,
    CKHF_OnSequenceDeleted = 0x00000002,
    CKHF_PreProcess = 0x00000004,
    CKHF_PostProcess = 0x00000008,
    CKHF_PreClearAll = 0x00000010,
    CKHF_PostClearAll = 0x00000020,
    CKHF_OnCKInit = 0x00000040,
    CKHF_OnCKEnd = 0x00000080,
    CKHF_OnCKPlay = 0x00000100,
    CKHF_OnCKPause = 0x00000200,
    CKHF_PreLoad = 0x00000400,
    CKHF_PreSave = 0x00000800,
    CKHF_PreLaunchScene = 0x00001000,
    CKHF_PostLaunchScene = 0x00002000,
    CKHF_OnCKReset = 0x00004000,
    CKHF_PostLoad = 0x00008000,
    CKHF_PostSave = 0x00010000,
    CKHF_OnCKPostReset = 0x00020000,
    CKHF_OnSequenceAddedToScene = 0x00040000,
    CKHF_OnSequenceRemovedFromScene = 0x00080000,
    CKHF_OnPreCopy = 0x00100000,
    CKHF_OnPostCopy = 0x00200000,
    CKHF_OnPreRender = 0x00400000,
    CKHF_OnPostRender = 0x00800000,
    CKHF_OnPostSpriteRender = 0x01000000,
} CKHookFunctions;

typedef enum CKHookFunctionIndex {
    CKHFI_OnSequenceToBeDeleted = 0,
    CKHFI_OnSequenceDeleted = 1,
    CKHFI_PreProcess = 2,
    CKHFI_PostProcess = 3,
    CKHFI_PreClearAll = 4,
    CKHFI_PostClearAll = 5,
    CKHFI_OnCKInit = 6,
    CKHFI_OnCKEnd = 7,
    CKHFI_OnCKPlay = 8,
    CKHFI_OnCKPause = 9,
    CKHFI_PreLoad = 10,
    CKHFI_PreSave = 11,
    CKHFI_PreLaunchScene = 12,
    CKHFI_PostLaunchScene = 13,
    CKHFI_OnCKReset = 14,
    CKHFI_PostLoad = 15,
    CKHFI_PostSave = 16,
    CKHFI_OnCKPostReset = 17,
    CKHFI_OnSequenceAddedToScene = 18,
    CKHFI_OnSequenceRemovedFromScene = 19,
    CKHFI_OnPreCopy = 20,
    CKHFI_OnPostCopy = 21,
    CKHFI_OnPreRender = 22,
    CKHFI_OnPostRender = 23,
    CKHFI_OnPostSpriteRender = 24,
} CKHookFunctionIndex;

typedef enum WindowHookFunctions {
    WHF_OnWndProc = 0x00000001,
    WHF_OnWndProcRet = 0x00000002,
} WindowHookFunctions;

typedef enum WindowHookFunctionIndex {
    WHFI_OnWndProc = 32,
    WHFI_OnWndProcRet = 33,
} WindowHookFunctionIndex;

typedef enum HookApiIndex {
    HAI_HOOK = 0
} HookApiIndex;

typedef enum HookApiErrorCode {
    HAEC_OK = 0,
    HAEC_ALREADY_CREATED,
    HAEC_NOT_CREATED,
    HAEC_ENABLED,
    HAEC_DISABLED,
    HAEC_NOT_EXECUTABLE,
    HAEC_UNSUPPORTED_FUNCTION,
    HAEC_MEMORY_ALLOC,
    HAEC_MEMORY_PROTECT,
    HAEC_MODULE_NOT_FOUND,
    HAEC_FUNCTION_NOT_FOUND,
    HAEC_UNKNOWN = -1
} HookApiErrorCode;

typedef struct HookApi {
    HookApiErrorCode (*CreateHook)(void *target, void *detour, void **originalPtr);
    HookApiErrorCode (*CreateHookApi)(const char *modulePath, const char *procName, void *detour, void **originalPtr, void **targetPtr);
    HookApiErrorCode (*RemoveHook)(void *target);

    HookApiErrorCode (*EnableHook)(void *target);
    HookApiErrorCode (*DisableHook)(void *target);

    HookApiErrorCode (*QueueEnableHook)(void *target);
    HookApiErrorCode (*QueueDisableHook)(void *target);
    HookApiErrorCode (*ApplyQueued)();
} HookApi;

#ifdef __cplusplus
}
#endif

#endif /* HOOKS_H */
