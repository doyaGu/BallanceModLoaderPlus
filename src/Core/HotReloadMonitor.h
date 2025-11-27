#ifndef BML_CORE_HOT_RELOAD_MONITOR_H
#define BML_CORE_HOT_RELOAD_MONITOR_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "HotReloadWatchList.h"

namespace BML::Core {
    class HotReloadMonitor {
    public:
        using ReloadCallback = std::function<void()>;

        HotReloadMonitor();
        ~HotReloadMonitor();

        HotReloadMonitor(const HotReloadMonitor &) = delete;
        HotReloadMonitor &operator=(const HotReloadMonitor &) = delete;

        void SetCallback(ReloadCallback callback);
        void SetIntervals(std::chrono::milliseconds poll_interval,
                          std::chrono::milliseconds debounce_interval);
        void UpdateWatchList(const std::vector<std::wstring> &paths);

        void Start();
        void Stop();

    private:
        void WorkerLoop();

        std::mutex m_StateMutex;
        std::condition_variable m_StateCv;
        bool m_Running{false};
        bool m_PendingReload{false};
        std::chrono::steady_clock::time_point m_LastChange;
        std::thread m_Worker;

        std::mutex m_WatchMutex;
        HotReloadWatchList m_WatchList;
        bool m_HasWatchPaths{false};

        ReloadCallback m_Callback;
        std::chrono::milliseconds m_PollInterval{std::chrono::milliseconds(750)};
        std::chrono::milliseconds m_Debounce{std::chrono::milliseconds(500)};
    };
} // namespace BML::Core

#endif // BML_CORE_HOT_RELOAD_MONITOR_H
