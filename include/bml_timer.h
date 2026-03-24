#ifndef BML_TIMER_H
#define BML_TIMER_H

/**
 * @file bml_timer.h
 * @brief Timer and deferred execution API
 *
 * Timer APIs are runtime services. Module code should use the injected
 * `bml.core.timer` runtime interface. Host code may resolve the raw exports
 * explicitly if it owns the runtime.
 *
 * All timers fire on the main thread during `bmlRuntimeUpdate(runtime)`, so callbacks
 * can safely access CK2 APIs without synchronization.
 *
 * Timers owned by a module are automatically cancelled when the module
 * is detached (hot-reload or shutdown).
 */

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_interface.h"

BML_BEGIN_CDECLS

#define BML_CORE_TIMER_INTERFACE_ID "bml.core.timer"

/** @brief Opaque timer handle */
typedef struct BML_Timer_T *BML_Timer;

/**
 * @brief Timer callback function.
 *
 * @param ctx       BML context
 * @param timer     Timer handle that fired
 * @param user_data User-provided context from schedule call
 *
 * @note Called on the main thread during the host runtime update pump.
 */
typedef void (*BML_TimerCallback)(BML_Context ctx,
                                  BML_Timer timer,
                                  void *user_data);

/**
 * @brief Schedule a one-shot timer.
 *
 * The callback fires once after at least `delay_ms` milliseconds,
 * then the timer is automatically cancelled.
 *
 * @param delay_ms   Delay in milliseconds (0 = fire on next update)
 * @param callback   Function to call when timer fires (required)
 * @param user_data  Opaque pointer passed to callback
 * @param out_timer  Receives the timer handle (required)
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TimerScheduleOnce)(
    BML_Mod owner,
    uint32_t delay_ms,
    BML_TimerCallback callback,
    void *user_data,
    BML_Timer *out_timer);

/**
 * @brief Schedule a repeating timer.
 *
 * The callback fires every `interval_ms` milliseconds until cancelled.
 * The first firing occurs after `interval_ms` from the schedule call.
 *
 * @param interval_ms Interval in milliseconds (must be > 0)
 * @param callback    Function to call on each tick (required)
 * @param user_data   Opaque pointer passed to callback
 * @param out_timer   Receives the timer handle (required)
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TimerScheduleRepeat)(
    BML_Mod owner,
    uint32_t interval_ms,
    BML_TimerCallback callback,
    void *user_data,
    BML_Timer *out_timer);

/**
 * @brief Schedule a frame-counted timer.
 *
 * The callback fires once after `frame_count` update ticks,
 * then the timer is automatically cancelled.
 *
 * @param frame_count Number of update frames to wait (0 = fire on next update)
 * @param callback    Function to call when timer fires (required)
 * @param user_data   Opaque pointer passed to callback
 * @param out_timer   Receives the timer handle (required)
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TimerScheduleFrames)(
    BML_Mod owner,
    uint32_t frame_count,
    BML_TimerCallback callback,
    void *user_data,
    BML_Timer *out_timer);

/**
 * @brief Cancel a pending timer.
 *
 * Safe to call on an already-cancelled or already-fired one-shot timer.
 * After this call, the callback will not be invoked again.
 *
 * @param timer Timer handle to cancel
 * @return BML_RESULT_OK on success, BML_RESULT_INVALID_ARGUMENT if NULL
 */
typedef BML_Result (*PFN_BML_TimerCancel)(BML_Mod owner, BML_Timer timer);

/**
 * @brief Query whether a timer is still active.
 *
 * @param timer      Timer handle to query
 * @param out_active Receives BML_TRUE if the timer is pending, BML_FALSE otherwise
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TimerIsActive)(BML_Mod owner,
                                            BML_Timer timer,
                                            BML_Bool *out_active);

/**
 * @brief Cancel all timers owned by the calling module.
 *
 * Called automatically during module detach. Modules may also call
 * this explicitly to reset their timer state.
 *
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TimerCancelAll)(BML_Mod owner);

/**
 * @brief Reserved options struct for future timer scheduling extensions.
 *
 * Not yet consumed by any API. Defined here so that future ScheduleOnceEx()
 * or ScheduleRepeatEx() functions can accept it without breaking ABI.
 */
typedef struct BML_TimerOptions {
    size_t struct_size;     /**< sizeof(BML_TimerOptions), must be first */
    uint32_t priority;      /**< Reserved (0 = default) */
    uint32_t flags;         /**< Reserved (0) */
} BML_TimerOptions;

#define BML_TIMER_OPTIONS_INIT { sizeof(BML_TimerOptions), 0, 0 }

typedef struct BML_CoreTimerInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_TimerScheduleOnce ScheduleOnce;
    PFN_BML_TimerScheduleRepeat ScheduleRepeat;
    PFN_BML_TimerScheduleFrames ScheduleFrames;
    PFN_BML_TimerCancel Cancel;
    PFN_BML_TimerIsActive IsActive;
    PFN_BML_TimerCancelAll CancelAll;
} BML_CoreTimerInterface;

BML_END_CDECLS

/* Compile-time assertions */
#ifdef __cplusplus
#include <cstddef>
static_assert(offsetof(BML_TimerOptions, struct_size) == 0, "BML_TimerOptions.struct_size must be at offset 0");
#endif

#endif /* BML_TIMER_H */
