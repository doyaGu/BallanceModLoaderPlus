#ifndef BML_TIMER_TIMERREGISTRY_H
#define BML_TIMER_TIMERREGISTRY_H

#include "Api/HandleSequence.h"
#include "BML/Timer.h"
#include "Timer/TimerScheduler.h"
#include <cmath>

namespace BML {

// Shared registration lifetime for the C and script adapters. Scheduler ids stay
// private; published handles never recycle, and release waits for active borrows.
class TimerRegistry {
  public:
    using Callback = std::function<int(const BML_TimerEvent &, uint32_t &)>;
    using Release = std::function<void()>;
    ~TimerRegistry() {
        while (!m_Records.empty())
            CleanupOwner(m_Records.begin()->second->Owner);
    }

    int Register(const void *owner, const BML_TimerDefinition *definition, Callback callback,
                 Release release, size_t tick, float time, BML_TimerHandle *out) {
        if (!out)
            return BML_ERROR_INVALID_PARAMETER;
        *out = 0;
        if (!owner || !definition || definition->StructSize < BML_TIMER_DEFINITION_1_0_SIZE ||
            !callback || definition->StartPaused > 1 ||
            definition->TimeBase > BML_TIMER_TIMEBASE_REALTIME ||
            definition->Type > BML_TIMER_THROTTLE)
            return BML_ERROR_INVALID_PARAMETER;
        const auto &d = *definition;
        if (d.Type >= BML_TIMER_DEBOUNCE)
            return BML_ERROR_NOT_IMPLEMENTED;
        if ((d.Type == BML_TIMER_REPEAT && (!d.RepeatCount || d.RepeatCount > INT32_MAX)) ||
            (d.Type != BML_TIMER_REPEAT && d.RepeatCount))
            return BML_ERROR_INVALID_PARAMETER;
        const float seconds = static_cast<float>(d.DelayMs / 1000.0);
        if (!std::isfinite(d.DelayMs) || d.DelayMs < 0 || !std::isfinite(seconds) ||
            (d.TimeBase == BML_TIMER_TIMEBASE_TICK
                 ? (d.DelayMs != 0 || d.DelayTicks > SIZE_MAX - tick)
                 : (d.DelayTicks != 0 || !std::isfinite(time) || !std::isfinite(time + seconds))))
            return BML_ERROR_INVALID_PARAMETER;
        auto record = std::make_shared<Record>();
        record->Owner = owner;
        record->Name = d.Name ? d.Name : "";
        record->Invoke = std::move(callback);
        record->Dispose = std::move(release);
        static std::atomic<uint64_t> next{1};
        record->Handle = Api::NextHandle(next);
        if (!record->Handle)
            return BML_ERROR_FAIL;
        TimerScheduler::Builder builder;
        builder.WithName(record->Name)
            .WithType(static_cast<TimerScheduler::Type>(d.Type))
            .WithPriority(static_cast<int8_t>(std::clamp(d.Priority, -128, 127)));
        if (d.TimeBase == BML_TIMER_TIMEBASE_TICK)
            builder.WithDelayTicks(static_cast<size_t>(d.DelayTicks));
        else
            builder.WithDelaySeconds(seconds).WithTimeBase(
                static_cast<TimerScheduler::TimeBase>(d.TimeBase));
        if (d.Type == BML_TIMER_REPEAT)
            builder.WithRepeatCount(static_cast<int>(d.RepeatCount));
        if (d.StartPaused)
            builder.StartPaused();
        const auto handle = record->Handle;
        if (d.Type == BML_TIMER_ONCE || d.Type == BML_TIMER_REPEAT)
            builder.WithOnceCallback(
                [this, handle](TimerScheduler &timer) { (void)Fire(handle, timer); });
        else
            builder.WithLoopCallback(
                [this, handle](TimerScheduler &timer) { return Fire(handle, timer); });
        m_Records.emplace(handle, record);
        try {
            record->Timer = builder.Build(tick, time);
        } catch (...) {
            m_Records.erase(handle);
            throw;
        }
        record->Committed = true;
        *out = handle;
        return BML_OK;
    }

    std::shared_ptr<TimerScheduler> Find(const void *owner, BML_TimerHandle handle) const {
        const auto it = m_Records.find(handle);
        return it != m_Records.end() && it->second->Owner == owner && !it->second->Terminal
                   ? it->second->Timer
                   : nullptr;
    }
    template <class Visitor>
    int Read(const void *owner, BML_TimerHandle handle, Visitor &&visitor) {
        const auto it = m_Records.find(handle);
        if (it == m_Records.end() || it->second->Owner != owner || it->second->Terminal)
            return BML_ERROR_INVALID_HANDLE;
        const auto record = it->second;
        ++record->Calls;
        struct Borrow {
            TimerRegistry &Registry;
            std::shared_ptr<Record> Value;
            ~Borrow() {
                --Value->Calls;
                Registry.Retire(Value->Handle);
            }
        } borrow{*this, record};
        return visitor(Event(*record));
    }
    int Control(const void *owner, BML_TimerHandle handle, uint32_t operation) {
        const auto it = m_Records.find(handle);
        if (it == m_Records.end() || it->second->Owner != owner || it->second->Terminal)
            return BML_ERROR_INVALID_HANDLE;
        const auto record = it->second;
        if (operation == 0)
            record->Timer->Pause();
        else if (operation == 1)
            record->Timer->Resume();
        else if (operation == 2) {
            record->Terminal = true;
            record->Timer->Cancel();
            Retire(handle);
        } else
            return BML_ERROR_INVALID_PARAMETER;
        return BML_OK;
    }
    void FinishProcess() {
        for (auto it = m_Records.begin(); it != m_Records.end();) {
            const auto record = it->second;
            ++it;
            if (record->Timer->GetState() == TimerScheduler::COMPLETED ||
                record->Timer->GetState() == TimerScheduler::CANCELLED)
                record->Terminal = true;
            if (record->Terminal && !record->Calls) {
                Retire(record->Handle);
                it = m_Records.begin(); // Release may mutate this registry.
            }
        }
    }
    void CleanupOwner(const void *owner) {
        for (auto it = m_Records.begin(); it != m_Records.end();) {
            const auto record = it->second;
            ++it;
            if (record->Owner != owner)
                continue;
            record->Terminal = true;
            record->Timer->Cancel();
            if (!record->Calls) {
                Retire(record->Handle);
                it = m_Records.begin();
            }
        }
    }
    size_t Size() const noexcept { return m_Records.size(); }

  private:
    struct Record {
        const void *Owner = nullptr;
        BML_TimerHandle Handle = 0;
        std::string Name;
        std::shared_ptr<TimerScheduler> Timer;
        Callback Invoke;
        Release Dispose;
        size_t Calls = 0;
        bool Terminal = false, Committed = false;
    };
    static BML_TimerEvent Event(const Record &record) {
        const auto &timer = *record.Timer;
        return {sizeof(BML_TimerEvent),
                record.Handle,
                record.Name.c_str(),
                static_cast<uint32_t>(timer.GetState()),
                static_cast<uint32_t>(timer.GetType()),
                static_cast<uint32_t>(timer.GetTimeBase()),
                static_cast<uint64_t>(timer.GetCompletedIterations()),
                timer.GetRemainingIterations(),
                timer.GetProgress()};
    }
    bool Fire(BML_TimerHandle handle, TimerScheduler &timer) {
        const auto it = m_Records.find(handle);
        if (it == m_Records.end() || it->second->Terminal)
            return false;
        const auto record = it->second;
        ++record->Calls;
        uint32_t keep = 1;
        int code = BML_ERROR_FAIL;
        try {
            code = record->Invoke(Event(*record), keep);
        } catch (...) {
        }
        --record->Calls;
        if (code != BML_OK || keep > 1) {
            timer.Cancel();
            record->Terminal = true;
        }
        if (!keep && timer.GetType() == TimerScheduler::REPEAT)
            timer.Cancel();
        if (!keep || timer.GetType() == TimerScheduler::ONCE || timer.GetRemainingIterations() == 1)
            record->Terminal = true;
        const bool active = !record->Terminal;
        Retire(handle);
        return active;
    }
    void Retire(BML_TimerHandle handle) {
        const auto it = m_Records.find(handle);
        if (it == m_Records.end() || !it->second->Terminal || it->second->Calls)
            return;
        const auto record = it->second;
        m_Records.erase(it);
        if (record->Committed) {
            record->Committed = false;
            if (record->Dispose)
                record->Dispose();
        }
    }
    std::unordered_map<BML_TimerHandle, std::shared_ptr<Record>> m_Records;
};
} // namespace BML
#endif
