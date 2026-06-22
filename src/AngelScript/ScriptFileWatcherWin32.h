#ifndef BML_SCRIPTFILEWATCHERWIN32_H
#define BML_SCRIPTFILEWATCHERWIN32_H

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
    };

    ScriptFileWatcherWin32() = default;
    ~ScriptFileWatcherWin32();

    ScriptFileWatcherWin32(const ScriptFileWatcherWin32 &) = delete;
    ScriptFileWatcherWin32 &operator=(const ScriptFileWatcherWin32 &) = delete;

    bool Watch(const std::wstring &root);
    void StopAll();
    std::vector<Event> DrainEvents();

private:
    struct WatchState {
        std::wstring Root;
        HANDLE Directory = INVALID_HANDLE_VALUE;
        HANDLE StopEvent = nullptr;
        std::thread Worker;
    };

    void WorkerLoop(WatchState *state);
    void PushEvent(const Event &event);

    std::mutex m_Mutex;
    std::vector<Event> m_Events;
    std::vector<WatchState *> m_Watches;
};

} // namespace BML

#endif
