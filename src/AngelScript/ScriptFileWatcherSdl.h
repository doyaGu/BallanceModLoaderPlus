#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace BML {
// SDL does not expose native file notifications. Poll filesystem metadata on
// background workers and feed the existing debounce/dependency/rollback layer.
class ScriptFileWatcherSdl {
public:
    struct Event {
        std::wstring Root;
        std::wstring Path;
        uint32_t Action = 0;
        bool Overflow = false;
        bool Recursive = true;
    };
    ScriptFileWatcherSdl();
    ~ScriptFileWatcherSdl();
    ScriptFileWatcherSdl(const ScriptFileWatcherSdl &) = delete;
    ScriptFileWatcherSdl &operator=(const ScriptFileWatcherSdl &) = delete;
    bool Watch(const std::wstring &root, bool recursive = true);
    bool Unwatch(const std::wstring &root);
    void StopAll();
    std::vector<Event> DrainEvents();
    uint64_t GetDroppedEventCount() const;
private:
    struct WatchState;
    void WorkerLoop(WatchState &state);
    void PushEvent(Event event);
    static void Stop(std::vector<std::unique_ptr<WatchState>> &states);
    mutable std::mutex m_Mutex;
    std::vector<std::unique_ptr<WatchState>> m_Watches;
    std::vector<Event> m_Events;
    // Separate overflow records ensure a saturated queue cannot lose a rescan.
    std::vector<Event> m_Overflows;
    uint64_t m_DroppedEvents = 0;
};
}
