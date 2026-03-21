#include "TimerManager.h"

#include "KernelServices.h"

#include <cassert>
#include <exception>

#include "Context.h"
#include "CrashDumpWriter.h"
#include "FaultTracker.h"
#include "Logging.h"

namespace BML::Core {
    namespace {
        constexpr char kTimerLogCategory[] = "timer";

        BML_Timer HandleFromId(uint32_t id) {
            return reinterpret_cast<BML_Timer>(static_cast<uintptr_t>(id));
        }

        uint32_t IdFromHandle(BML_Timer timer) {
            return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(timer));
        }

#if defined(_MSC_VER) && !defined(__MINGW32__)
        int FilterModuleException(unsigned long code) {
            switch (code) {
                case EXCEPTION_ACCESS_VIOLATION:
                case EXCEPTION_ILLEGAL_INSTRUCTION:
                case EXCEPTION_STACK_OVERFLOW:
                case EXCEPTION_INT_DIVIDE_BY_ZERO:
                case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                    return EXCEPTION_EXECUTE_HANDLER;
                default:
                    return EXCEPTION_CONTINUE_SEARCH;
            }
        }

        unsigned long InvokeTimerCallbackSEH(BML_TimerCallback callback,
                                             BML_Context ctx,
                                             BML_Timer timer,
                                             void *user_data) {
            __try {
                callback(ctx, timer, user_data);
            } __except (FilterModuleException(GetExceptionCode())) {
                return GetExceptionCode();
            }
            return 0;
        }
#endif
    } // namespace

    TimerManager &TimerManager::Instance() {
        auto *k = GetKernelOrNull();
        assert(k && k->timers);
        return *k->timers;
    }

    TimerManager::TimerEntry *TimerManager::FindEntry(BML_Timer timer) {
        uint32_t id = IdFromHandle(timer);
        for (auto &entry : m_Timers) {
            if (entry.id == id)
                return &entry;
        }
        return nullptr;
    }

    BML_Result TimerManager::ScheduleOnce(const std::string &owner_id,
                                          uint32_t delay_ms,
                                          BML_TimerCallback callback,
                                          void *user_data,
                                          BML_Timer *out_timer) {
        if (!callback || !out_timer)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(m_Mutex);

        TimerEntry entry;
        entry.id = m_NextId++;
        entry.kind = TimerKind::OnceMs;
        entry.active = true;
        entry.fire_at = Clock::now() + std::chrono::milliseconds(delay_ms);
        entry.callback = callback;
        entry.user_data = user_data;
        entry.owner_id = owner_id;

        *out_timer = HandleFromId(entry.id);
        m_Timers.push_back(std::move(entry));
        return BML_RESULT_OK;
    }

    BML_Result TimerManager::ScheduleRepeat(const std::string &owner_id,
                                            uint32_t interval_ms,
                                            BML_TimerCallback callback,
                                            void *user_data,
                                            BML_Timer *out_timer) {
        if (!callback || !out_timer)
            return BML_RESULT_INVALID_ARGUMENT;
        if (interval_ms == 0)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(m_Mutex);

        TimerEntry entry;
        entry.id = m_NextId++;
        entry.kind = TimerKind::RepeatMs;
        entry.active = true;
        entry.fire_at = Clock::now() + std::chrono::milliseconds(interval_ms);
        entry.interval_ms = interval_ms;
        entry.callback = callback;
        entry.user_data = user_data;
        entry.owner_id = owner_id;

        *out_timer = HandleFromId(entry.id);
        m_Timers.push_back(std::move(entry));
        return BML_RESULT_OK;
    }

    BML_Result TimerManager::ScheduleFrames(const std::string &owner_id,
                                            uint32_t frame_count,
                                            BML_TimerCallback callback,
                                            void *user_data,
                                            BML_Timer *out_timer) {
        if (!callback || !out_timer)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(m_Mutex);

        TimerEntry entry;
        entry.id = m_NextId++;
        entry.kind = TimerKind::OnceFrames;
        entry.active = true;
        entry.frames_remaining = frame_count;
        entry.callback = callback;
        entry.user_data = user_data;
        entry.owner_id = owner_id;

        *out_timer = HandleFromId(entry.id);
        m_Timers.push_back(std::move(entry));
        return BML_RESULT_OK;
    }

    BML_Result TimerManager::Cancel(BML_Timer timer) {
        if (!timer)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(m_Mutex);
        auto *entry = FindEntry(timer);
        if (!entry)
            return BML_RESULT_OK;  // already gone
        entry->active = false;
        return BML_RESULT_OK;
    }

    BML_Result TimerManager::IsActive(BML_Timer timer, BML_Bool *out_active) {
        if (!timer || !out_active)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(m_Mutex);
        auto *entry = FindEntry(timer);
        *out_active = (entry && entry->active) ? BML_TRUE : BML_FALSE;
        return BML_RESULT_OK;
    }

    BML_Result TimerManager::CancelAllForModule(const std::string &owner_id) {
        std::lock_guard lock(m_Mutex);
        for (auto &entry : m_Timers) {
            if (entry.active && entry.owner_id == owner_id) {
                entry.active = false;
            }
        }
        return BML_RESULT_OK;
    }

    void TimerManager::Tick() {
        // Collect timers to fire while holding the lock, then fire them
        // outside the lock to avoid deadlock if callbacks schedule new timers.

        struct PendingFire {
            BML_Timer handle;
            BML_TimerCallback callback;
            void *user_data;
            std::string owner_id;
        };

        // Static thread-local to avoid per-frame heap allocation in the common
        // case (Tick is main-thread-only so TLS is fine).
        thread_local std::vector<PendingFire> to_fire;
        to_fire.clear();

        {
            std::lock_guard lock(m_Mutex);
            auto now = Clock::now();

            for (auto &entry : m_Timers) {
                if (!entry.active)
                    continue;

                bool should_fire = false;

                switch (entry.kind) {
                case TimerKind::OnceMs:
                    if (now >= entry.fire_at) {
                        should_fire = true;
                        entry.active = false;
                    }
                    break;

                case TimerKind::RepeatMs:
                    if (now >= entry.fire_at) {
                        should_fire = true;
                        // Advance fire_at by interval. If multiple intervals
                        // elapsed (e.g. long frame), skip to next future time.
                        auto interval = std::chrono::milliseconds(entry.interval_ms);
                        entry.fire_at += interval;
                        if (entry.fire_at <= now) {
                            entry.fire_at = now + interval;
                        }
                    }
                    break;

                case TimerKind::OnceFrames:
                    if (entry.frames_remaining == 0) {
                        should_fire = true;
                        entry.active = false;
                    } else {
                        --entry.frames_remaining;
                    }
                    break;
                }

                if (should_fire) {
                    to_fire.push_back({
                        HandleFromId(entry.id),
                        entry.callback,
                        entry.user_data,
                        entry.owner_id
                    });
                }
            }

            // Compact: remove inactive entries
            m_Timers.erase(
                std::remove_if(m_Timers.begin(), m_Timers.end(),
                               [](const TimerEntry &e) { return !e.active; }),
                m_Timers.end());
        }

        // Fire callbacks without holding the lock
        BML_Context ctx = GetKernelOrNull()->context->GetHandle();
        for (auto &pending : to_fire) {
            try {
#if defined(_MSC_VER) && !defined(__MINGW32__)
                unsigned long seh_code = InvokeTimerCallbackSEH(
                    pending.callback, ctx, pending.handle, pending.user_data);
                if (seh_code != 0) {
                    CoreLog(BML_LOG_ERROR, kTimerLogCategory,
                            "Timer callback crashed (code 0x%08lX) for module '%s'; cancelling timer",
                            seh_code,
                            pending.owner_id.empty() ? "unknown" : pending.owner_id.c_str());
                    GetKernelOrNull()->crash_dump->WriteDumpOnce(pending.owner_id, seh_code);
                    if (!pending.owner_id.empty()) {
                        GetKernelOrNull()->fault_tracker->RecordFault(pending.owner_id, seh_code);
                    }
                    Cancel(pending.handle);
                }
#else
                pending.callback(ctx, pending.handle, pending.user_data);
#endif
            } catch (const std::exception &ex) {
                CoreLog(BML_LOG_ERROR, kTimerLogCategory,
                        "Timer callback threw C++ exception for module '%s': %s; cancelling timer",
                        pending.owner_id.empty() ? "unknown" : pending.owner_id.c_str(),
                        ex.what());
                Cancel(pending.handle);
            } catch (...) {
                CoreLog(BML_LOG_ERROR, kTimerLogCategory,
                        "Timer callback threw unknown exception for module '%s'; cancelling timer",
                        pending.owner_id.empty() ? "unknown" : pending.owner_id.c_str());
                Cancel(pending.handle);
            }
        }
    }

    void TimerManager::Shutdown() {
        std::lock_guard lock(m_Mutex);
        m_Timers.clear();
        // Do not reset m_NextId -- handles from previous session stay invalid
    }
} // namespace BML::Core
