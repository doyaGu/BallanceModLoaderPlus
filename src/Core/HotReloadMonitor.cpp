#include "HotReloadMonitor.h"

#include <atomic>

namespace BML::Core {

HotReloadMonitor::HotReloadMonitor() = default;

HotReloadMonitor::~HotReloadMonitor() {
    Stop();
}

void HotReloadMonitor::SetCallback(ReloadCallback callback) {
    std::lock_guard lock(m_StateMutex);
    m_Callback = std::move(callback);
}

void HotReloadMonitor::SetIntervals(std::chrono::milliseconds poll_interval,
                                    std::chrono::milliseconds debounce_interval) {
    std::lock_guard lock(m_StateMutex);
    if (poll_interval.count() > 0)
        m_PollInterval = poll_interval;
    if (debounce_interval.count() > 0)
        m_Debounce = debounce_interval;
}

void HotReloadMonitor::UpdateWatchList(const std::vector<std::wstring> &paths) {
    std::lock_guard watch_lock(m_WatchMutex);
    m_WatchList.Reset(paths);
    m_HasWatchPaths = !m_WatchList.Empty();
    std::lock_guard state_lock(m_StateMutex);
    m_PendingReload = false;
}

void HotReloadMonitor::Start() {
    std::lock_guard lock(m_StateMutex);
    if (m_Running)
        return;
    m_Running = true;
    m_Worker = std::thread(&HotReloadMonitor::WorkerLoop, this);
}

void HotReloadMonitor::Stop() {
    {
        std::lock_guard lock(m_StateMutex);
        if (!m_Running)
            return;
        m_Running = false;
    }
    m_StateCv.notify_all();
    if (m_Worker.joinable())
        m_Worker.join();
    m_PendingReload = false;
}

void HotReloadMonitor::WorkerLoop() {
    std::unique_lock state_lock(m_StateMutex);
    while (true) {
        if (!m_Running)
            break;

        auto wait_status = m_StateCv.wait_for(state_lock, m_PollInterval, [this] { return !m_Running; });
        if (!m_Running)
            break;

        state_lock.unlock();
        bool changed = false;
        {
            std::lock_guard watch_lock(m_WatchMutex);
            if (m_HasWatchPaths)
                changed = m_WatchList.DetectChanges();
        }

        if (changed) {
            std::lock_guard lock(m_StateMutex);
            m_PendingReload = true;
            m_LastChange = std::chrono::steady_clock::now();
        }

        ReloadCallback callback_to_fire;
        {
            std::lock_guard lock(m_StateMutex);
            if (m_PendingReload) {
                auto now = std::chrono::steady_clock::now();
                if (now - m_LastChange >= m_Debounce) {
                    m_PendingReload = false;
                    callback_to_fire = m_Callback;
                }
            }
        }

        if (callback_to_fire) {
            try {
                callback_to_fire();
            } catch (...) {
                // Callback threw an exception - absorb to keep worker thread alive
                // The caller is responsible for logging/handling callback errors
            }
        }

        state_lock.lock();
    }
}

} // namespace BML::Core
