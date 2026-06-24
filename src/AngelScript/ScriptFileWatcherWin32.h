#ifndef BML_SCRIPTFILEWATCHERWIN32_H
#define BML_SCRIPTFILEWATCHERWIN32_H

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace BML {

class ScriptFileWatcherWin32 {
public:
    struct Event {
        std::wstring Root;
        std::wstring Path;
        DWORD Action = 0;
        bool Overflow = false;
        bool Recursive = true;
    };

    ScriptFileWatcherWin32() = default;
    ~ScriptFileWatcherWin32();

    ScriptFileWatcherWin32(const ScriptFileWatcherWin32 &) = delete;
    ScriptFileWatcherWin32 &operator=(const ScriptFileWatcherWin32 &) = delete;

    bool Watch(const std::wstring &root, bool recursive = true);
    bool Unwatch(const std::wstring &root);
    void StopAll();
    std::vector<Event> DrainEvents();
    uint64_t GetDroppedEventCount() const;

private:
    struct WatchState {
        std::wstring Root;
        bool Recursive = true;
        HANDLE Directory = INVALID_HANDLE_VALUE;
        HANDLE StopEvent = nullptr;
        std::thread Worker;
    };

    static bool SameRoot(const std::wstring &left, const std::wstring &right);
    static void StopWatches(std::vector<WatchState *> &watches);

    void WorkerLoop(WatchState *state);
    void PushEvent(const Event &event);
    void PushOverflowEventLocked(const std::wstring &root, bool recursive);

    mutable std::mutex m_Mutex;
    std::vector<Event> m_Events;
    std::vector<WatchState *> m_Watches;
    uint64_t m_DroppedEvents = 0;
    bool m_OverflowQueued = false;
};

} // namespace BML

#endif
