#ifndef BML_TIMER_H
#define BML_TIMER_H

/**
 * @file bml_timer.h
 * @brief Timer and deferred execution API
 *
 * Timer APIs are runtime services. Resolve these function pointers through
 * `bmlGetProcAddress(...)` or acquire `bml.core.timer` in module code.
 *
 * All timers fire on the main thread during `bmlUpdate()`, so callbacks
 * can safely access CK2 APIs without synchronization.
 *
 * Timers owned by a module are automatically cancelled when the module
 * is detached (hot-reload or shutdown).
 */

#include "bml_types.h"
#include "bml_errors.h"

BML_BEGIN_CDECLS

/** @brief Opaque timer handle */
typedef struct BML_Timer_T *BML_Timer;

/**
 * @brief Timer callback function.
 *
 * @param ctx       BML context
 * @param timer     Timer handle that fired
 * @param user_data User-provided context from schedule call
 *
 * @note Called on the main thread during bmlUpdate().
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
    uint32_t delay_ms,
    BML_TimerCallback callback,
    void *user_data,
    BML_Timer *out_timer);
typedef BML_Result (*PFN_BML_TimerScheduleOnceOwned)(
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
    uint32_t interval_ms,
    BML_TimerCallback callback,
    void *user_data,
    BML_Timer *out_timer);
typedef BML_Result (*PFN_BML_TimerScheduleRepeatOwned)(
    BML_Mod owner,
    uint32_t interval_ms,
    BML_TimerCallback callback,
    void *user_data,
    BML_Timer *out_timer);

/**
 * @brief Schedule a frame-counted timer.
 *
 * The callback fires once after `frame_count` calls to bmlUpdate(),
 * then the timer is automatically cancelled.
 *
 * @param frame_count Number of update frames to wait (0 = fire on next update)
 * @param callback    Function to call when timer fires (required)
 * @param user_data   Opaque pointer passed to callback
 * @param out_timer   Receives the timer handle (required)
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TimerScheduleFrames)(
    uint32_t frame_count,
    BML_TimerCallback callback,
    void *user_data,
    BML_Timer *out_timer);
typedef BML_Result (*PFN_BML_TimerScheduleFramesOwned)(
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
typedef BML_Result (*PFN_BML_TimerCancel)(BML_Timer timer);
typedef BML_Result (*PFN_BML_TimerCancelOwned)(BML_Mod owner, BML_Timer timer);

/**
 * @brief Query whether a timer is still active.
 *
 * @param timer      Timer handle to query
 * @param out_active Receives BML_TRUE if the timer is pending, BML_FALSE otherwise
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TimerIsActive)(BML_Timer timer,
                                            BML_Bool *out_active);
typedef BML_Result (*PFN_BML_TimerIsActiveOwned)(BML_Mod owner,
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
typedef BML_Result (*PFN_BML_TimerCancelAll)(void);
typedef BML_Result (*PFN_BML_TimerCancelAllOwned)(BML_Mod owner);

BML_END_CDECLS

#endif /* BML_TIMER_H */
