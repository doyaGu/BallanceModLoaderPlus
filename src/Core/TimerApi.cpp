#include "ApiRegistrationMacros.h"
#include "TimerManager.h"

#include "Context.h"

#include "bml_timer.h"

namespace BML::Core {
    namespace {
        std::string ResolveCallerModuleId(Context &context, BML_Mod owner) {
            auto *consumer = context.ResolveModHandle(owner);
            return consumer ? consumer->id : std::string{};
        }
    } // namespace

    BML_Result BML_API_TimerScheduleOnce(BML_Mod owner,
                                         uint32_t delay_ms,
                                         BML_TimerCallback callback,
                                         void *user_data,
                                         BML_Timer *out_timer) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        const std::string owner_id = ResolveCallerModuleId(context, owner);
        if (owner_id.empty()) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.timers->ScheduleOnce(
            owner_id, delay_ms, callback, user_data, out_timer);
    }

    BML_Result BML_API_TimerScheduleRepeat(BML_Mod owner,
                                           uint32_t interval_ms,
                                           BML_TimerCallback callback,
                                           void *user_data,
                                           BML_Timer *out_timer) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        const std::string owner_id = ResolveCallerModuleId(context, owner);
        if (owner_id.empty()) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.timers->ScheduleRepeat(
            owner_id, interval_ms, callback, user_data, out_timer);
    }

    BML_Result BML_API_TimerScheduleFrames(BML_Mod owner,
                                           uint32_t frame_count,
                                           BML_TimerCallback callback,
                                           void *user_data,
                                           BML_Timer *out_timer) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        const std::string owner_id = ResolveCallerModuleId(context, owner);
        if (owner_id.empty()) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.timers->ScheduleFrames(
            owner_id, frame_count, callback, user_data, out_timer);
    }

    BML_Result BML_API_TimerCancel(BML_Mod owner, BML_Timer timer) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        const std::string owner_id = ResolveCallerModuleId(context, owner);
        if (owner_id.empty()) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.timers->Cancel(owner_id, timer);
    }

    BML_Result BML_API_TimerIsActive(BML_Mod owner, BML_Timer timer, BML_Bool *out_active) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        const std::string owner_id = ResolveCallerModuleId(context, owner);
        if (owner_id.empty()) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.timers->IsActive(owner_id, timer, out_active);
    }

    BML_Result BML_API_TimerCancelAll(BML_Mod owner) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        const std::string owner_id = ResolveCallerModuleId(context, owner);
        if (owner_id.empty()) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.timers->CancelAllForModule(owner_id);
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
