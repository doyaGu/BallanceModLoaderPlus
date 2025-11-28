/**
 * @file bml_virtools.h
 * @brief Virtools Integration API - Provided by ModLoader
 * 
 * This header defines APIs for accessing Virtools engine objects.
 * These APIs are NOT part of BML Core - they are registered by ModLoader
 * as a special extension that bridges BML with the Virtools SDK.
 * 
 * ModLoader sets these via bmlSetUserData() with well-known keys:
 * - "virtools.ckcontext" - CKContext pointer
 * - "virtools.rendercontext" - CKRenderContext pointer
 * - "virtools.inputmanager" - CKInputManager pointer
 * - etc.
 * 
 * @section usage Usage Example
 * @code
 * #include <bml_virtools.h>
 * 
 * // Get CKContext from BML context
 * CKContext* ck = bmlVirtoolsGetCKContext(bmlGetGlobalContext());
 * if (ck) {
 *     // Use CKContext...
 * }
 * 
 * // Or use the helper macros with context user data directly
 * void* ptr = NULL;
 * if (bmlContextGetUserData(ctx, BML_VIRTOOLS_KEY_CKCONTEXT, &ptr) == BML_RESULT_OK && ptr) {
 *     CKContext* ck = (CKContext*)ptr;
 * }
 * @endcode
 * 
 * @note These APIs require ModLoader to be loaded. They will return NULL
 *       if ModLoader hasn't registered the Virtools objects yet.
 */

#ifndef BML_VIRTOOLS_H
#define BML_VIRTOOLS_H

#include "bml_types.h"
#include "bml_core.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Well-known User Data Keys
 * 
 * These keys are used with bmlSetUserData/bmlContextGetUserData
 * to store/retrieve Virtools objects. ModLoader registers these during
 * engine initialization.
 * ======================================================================== */

/** @brief Key for CKContext pointer */
#define BML_VIRTOOLS_KEY_CKCONTEXT      "virtools.ckcontext"

/** @brief Key for CKRenderContext pointer */
#define BML_VIRTOOLS_KEY_RENDERCONTEXT  "virtools.rendercontext"

/** @brief Key for CKInputManager pointer */
#define BML_VIRTOOLS_KEY_INPUTMANAGER   "virtools.inputmanager"

/** @brief Key for CKTimeManager pointer */
#define BML_VIRTOOLS_KEY_TIMEMANAGER    "virtools.timemanager"

/** @brief Key for CKMessageManager pointer */
#define BML_VIRTOOLS_KEY_MESSAGEMANAGER "virtools.messagemanager"

/** @brief Key for CKAttributeManager pointer */
#define BML_VIRTOOLS_KEY_ATTRIBUTEMANAGER "virtools.attributemanager"

/** @brief Key for CKPathManager pointer */
#define BML_VIRTOOLS_KEY_PATHMANAGER    "virtools.pathmanager"

/** @brief Key for CKSoundManager pointer */
#define BML_VIRTOOLS_KEY_SOUNDMANAGER   "virtools.soundmanager"

/** @brief Key for main HWND */
#define BML_VIRTOOLS_KEY_MAINHWND       "virtools.mainhwnd"

/** @brief Key for render HWND */
#define BML_VIRTOOLS_KEY_RENDERHWND     "virtools.renderhwnd"

/* ========================================================================
 * Convenience Functions (inline)
 * 
 * These are thin wrappers around bmlContextGetUserData for convenience.
 * They handle the NULL checks and casting.
 * ======================================================================== */

/**
 * @brief Get CKContext from BML context
 * 
 * @param[in] ctx BML context handle
 * @return CKContext pointer, or NULL if not available
 */
static inline void* bmlVirtoolsGetCKContext(BML_Context ctx) {
    void* result = NULL;
    if (ctx && bmlContextGetUserData) {
        bmlContextGetUserData(ctx, BML_VIRTOOLS_KEY_CKCONTEXT, &result);
    }
    return result;
}

/**
 * @brief Get CKRenderContext from BML context
 */
static inline void* bmlVirtoolsGetRenderContext(BML_Context ctx) {
    void* result = NULL;
    if (ctx && bmlContextGetUserData) {
        bmlContextGetUserData(ctx, BML_VIRTOOLS_KEY_RENDERCONTEXT, &result);
    }
    return result;
}

/**
 * @brief Get CKInputManager from BML context
 */
static inline void* bmlVirtoolsGetInputManager(BML_Context ctx) {
    void* result = NULL;
    if (ctx && bmlContextGetUserData) {
        bmlContextGetUserData(ctx, BML_VIRTOOLS_KEY_INPUTMANAGER, &result);
    }
    return result;
}

/**
 * @brief Get CKTimeManager from BML context
 */
static inline void* bmlVirtoolsGetTimeManager(BML_Context ctx) {
    void* result = NULL;
    if (ctx && bmlContextGetUserData) {
        bmlContextGetUserData(ctx, BML_VIRTOOLS_KEY_TIMEMANAGER, &result);
    }
    return result;
}

/**
 * @brief Get main window HWND from BML context
 */
static inline void* bmlVirtoolsGetMainHWND(BML_Context ctx) {
    void* result = NULL;
    if (ctx && bmlContextGetUserData) {
        bmlContextGetUserData(ctx, BML_VIRTOOLS_KEY_MAINHWND, &result);
    }
    return result;
}

/**
 * @brief Get render window HWND from BML context
 */
static inline void* bmlVirtoolsGetRenderHWND(BML_Context ctx) {
    void* result = NULL;
    if (ctx && bmlContextGetUserData) {
        bmlContextGetUserData(ctx, BML_VIRTOOLS_KEY_RENDERHWND, &result);
    }
    return result;
}

BML_END_CDECLS

#endif /* BML_VIRTOOLS_H */
