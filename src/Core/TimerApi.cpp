#include "ApiRegistrationMacros.h"
#include "TimerManager.h"

#include "Context.h"

#include "bml_timer.h"

namespace BML::Core {
    namespace {
        std::string GetCallerModuleId() {
            auto *consumer = Context::Instance().ResolveCurrentConsumer();
            return consumer ? consumer->id : std::string{};
        }
    } // namespace

    BML_Result BML_API_TimerScheduleOnce(uint32_t delay_ms,
                                         BML_TimerCallback callback,
                                         void *user_data,
                                         BML_Timer *out_timer) {
        return TimerManager::Instance().ScheduleOnce(
            GetCallerModuleId(), delay_ms, callback, user_data, out_timer);
    }

    BML_Result BML_API_TimerScheduleRepeat(uint32_t interval_ms,
                                           BML_TimerCallback callback,
                                           void *user_data,
                                           BML_Timer *out_timer) {
        return TimerManager::Instance().ScheduleRepeat(
            GetCallerModuleId(), interval_ms, callback, user_data, out_timer);
    }

    BML_Result BML_API_TimerScheduleFrames(uint32_t frame_count,
                                           BML_TimerCallback callback,
                                           void *user_data,
                                           BML_Timer *out_timer) {
        return TimerManager::Instance().ScheduleFrames(
            GetCallerModuleId(), frame_count, callback, user_data, out_timer);
    }

    BML_Result BML_API_TimerCancel(BML_Timer timer) {
        return TimerManager::Instance().Cancel(timer);
    }

    BML_Result BML_API_TimerIsActive(BML_Timer timer, BML_Bool *out_active) {
        return TimerManager::Instance().IsActive(timer, out_active);
    }

    BML_Result BML_API_TimerCancelAll(void) {
        return TimerManager::Instance().CancelAllForModule(GetCallerModuleId());
    }

    void RegisterTimerApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API_GUARDED(bmlTimerScheduleOnce, "timer", BML_API_TimerScheduleOnce);
        BML_REGISTER_API_GUARDED(bmlTimerScheduleRepeat, "timer", BML_API_TimerScheduleRepeat);
        BML_REGISTER_API_GUARDED(bmlTimerScheduleFrames, "timer", BML_API_TimerScheduleFrames);
        BML_REGISTER_API_GUARDED(bmlTimerCancel, "timer", BML_API_TimerCancel);
        BML_REGISTER_API_GUARDED(bmlTimerIsActive, "timer", BML_API_TimerIsActive);
        BML_REGISTER_API_GUARDED(bmlTimerCancelAll, "timer", BML_API_TimerCancelAll);
    }
} // namespace BML::Core
