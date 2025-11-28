/**
 * @file bml_engine_events.h
 * @brief Payload definitions for BML engine lifecycle IMC topics.
 */

#ifndef BML_ENGINE_EVENTS_H
#define BML_ENGINE_EVENTS_H

#include "bml_types.h"

BML_BEGIN_CDECLS

struct CKContext;
struct CKRenderContext;

/**
 * @brief Payload published on the "BML/Engine/Init" topic.
 */
typedef struct BML_EngineInitEvent {
    uint32_t struct_size;       /**< Size of this struct for forward compatibility. */
    CKContext *context;         /**< CKContext pointer, valid for the rest of the run. */
    void *main_window;          /**< HWND for the main application window (may be NULL). */
    void *reserved0;            /**< Reserved for future expansion (must be NULL). */
} BML_EngineInitEvent;

/**
 * @brief Payload published on the "BML/Engine/Play" topic.
 */
typedef struct BML_EnginePlayEvent {
    uint32_t struct_size;       /**< Size of this struct for forward compatibility. */
    CKContext *context;         /**< CKContext pointer. */
    CKRenderContext *render_context; /**< Active CKRenderContext pointer. */
    void *render_window;        /**< HWND for the render window (may be NULL). */
    BML_Bool is_resume;         /**< True if this play event follows a pause/resume. */
    uint32_t reserved0;         /**< Reserved for future expansion (must be zero). */
} BML_EnginePlayEvent;

BML_END_CDECLS

#endif /* BML_ENGINE_EVENTS_H */
