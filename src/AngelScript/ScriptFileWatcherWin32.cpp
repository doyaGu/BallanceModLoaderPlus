#include "ScriptFileWatcherWin32.h"

#include <algorithm>
#include <iterator>
#include <memory>

namespace BML {

namespace {

constexpr size_t kMaxQueuedEvents = 4096;

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle = nullptr) : m_Handle(handle) {}
    ~ScopedHandle() {
        if (m_Handle && m_Handle != INVALID_HANDLE_VALUE)
            ::CloseHandle(m_Handle);
    }

    ScopedHandle(const ScopedHandle &) = delete;
    ScopedHandle &operator=(const ScopedHandle &) = delete;

    HANDLE Get() const { return m_Handle; }

private:
    HANDLE m_Handle = nullptr;
};

std::wstring JoinWatchedPath(const std::wstring &root, const wchar_t *relativeName, DWORD lengthBytes) {
    std::wstring path = root;
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
        path.push_back(L'\\');
    path.append(relativeName, relativeName + (lengthBytes / sizeof(wchar_t)));
    return path;
}

void CancelAndDrainOverlapped(HANDLE directory, OVERLAPPED &overlapped) {
    if (directory == INVALID_HANDLE_VALUE)
        return;
    ::CancelIoEx(directory, &overlapped);
    DWORD ignored = 0;
    ::GetOverlappedResult(directory, &overlapped, &ignored, TRUE);
}

} // namespace

ScriptFileWatcherWin32::~ScriptFileWatcherWin32() {
    StopAll();
}

bool ScriptFileWatcherWin32::SameRoot(const std::wstring &left, const std::wstring &right) {
    if (left.empty() || right.empty())
        return false;
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

void ScriptFileWatcherWin32::StopWatches(std::vector<WatchState *> &watches) {
    for (WatchState *state : watches) {
        if (!state)
            continue;
        if (state->StopEvent)
            ::SetEvent(state->StopEvent);
    }

    for (WatchState *state : watches) {
        if (!state)
            continue;
        if (state->Worker.joinable())
            state->Worker.join();
#ifdef BML_TEST
        if (state->InterruptEvent)
            ::CloseHandle(state->InterruptEvent);
#endif
        if (state->StopEvent)
            ::CloseHandle(state->StopEvent);
        delete state;
    }
    watches.clear();
}

bool ScriptFileWatcherWin32::Watch(const std::wstring &root, bool recursive) {
    if (root.empty())
        return false;

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const WatchState *existing : m_Watches) {
            if (existing && SameRoot(existing->Root, root) && existing->Recursive == recursive)
                return true;
        }
    }

    auto *state = new (std::nothrow) WatchState();
    if (!state)
        return false;

    state->Root = root;
    state->Recursive = recursive;
    state->StopEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
#ifdef BML_TEST
    state->InterruptEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!state->StopEvent || !state->InterruptEvent) {
        std::vector<WatchState *> failed{state};
        StopWatches(failed);
        return false;
    }
#else
    if (!state->StopEvent) {
        delete state;
        return false;
    }
#endif

    bool duplicate = false;
    bool registered = false;
    std::promise<bool> ready;
    auto started = ready.get_future();
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const WatchState *existing : m_Watches) {
            if (existing && SameRoot(existing->Root, root) && existing->Recursive == recursive) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            try {
                m_Watches.reserve(m_Watches.size() + 1);
                state->Worker = std::thread([this, state, ready = std::move(ready)]() mutable {
                    WorkerLoop(state, std::move(ready));
                });
                m_Watches.push_back(state);
                registered = true;
            } catch (...) {
                // The unpublished state is stopped and released after dropping m_Mutex.
            }
        }
    }

    if (!registered) {
        std::vector<WatchState *> failed;
        failed.push_back(state);
        StopWatches(failed);
        return duplicate;
    }
    try {
        if (started.get())
            return true;
    } catch (const std::future_error &) {
        // The worker exited before it could start the first read.
    }
    Unwatch(root);
    return false;
}

bool ScriptFileWatcherWin32::Unwatch(const std::wstring &root) {
    if (root.empty())
        return false;

    std::vector<WatchState *> removed;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = std::remove_if(m_Watches.begin(), m_Watches.end(), [&](WatchState *state) {
            if (!state || !SameRoot(state->Root, root))
                return false;
            removed.push_back(state);
            return true;
        });
        m_Watches.erase(it, m_Watches.end());
    }

    if (removed.empty())
        return false;
    StopWatches(removed);
    return true;
}

void ScriptFileWatcherWin32::StopAll() {
    std::vector<WatchState *> watches;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        watches.swap(m_Watches);
    }

    StopWatches(watches);

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Events.clear();
        m_OverflowQueued.clear();
    }
}

std::vector<ScriptFileWatcherWin32::Event> ScriptFileWatcherWin32::DrainEvents() {
    std::vector<Event> events;
    std::lock_guard<std::mutex> lock(m_Mutex);
    events.swap(m_Events);
    m_OverflowQueued.clear();
    return events;
}

uint64_t ScriptFileWatcherWin32::GetDroppedEventCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_DroppedEvents;
}

void ScriptFileWatcherWin32::WorkerLoop(WatchState *state, std::promise<bool> ready) {
    if (!state)
        return;

    try {
        std::vector<unsigned char> buffer(64 * 1024);
        bool started = false;
        while (::WaitForSingleObject(state->StopEvent, 0) == WAIT_TIMEOUT) {
            ScopedHandle directory(::CreateFileW(
                state->Root.c_str(), FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr));
            bool rescan = started;
            while (directory.Get() != INVALID_HANDLE_VALUE) {
                OVERLAPPED overlapped{};
                ScopedHandle event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
                overlapped.hEvent = event.Get();
                constexpr DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                         FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
                if (!overlapped.hEvent ||
                    !::ReadDirectoryChangesW(directory.Get(), buffer.data(),
                                             static_cast<DWORD>(buffer.size()),
                                             state->Recursive, filter, nullptr, &overlapped, nullptr))
                    break;

                try {
                    if (!started) {
                        started = true;
                        ready.set_value(true);
                    } else if (rescan) {
                        rescan = false;
                        PushEvent(Event{state->Root, state->Root, 0, true, state->Recursive});
                    }
                } catch (...) {
                    CancelAndDrainOverlapped(directory.Get(), overlapped);
                    throw;
                }

                const HANDLE handles[] = {
                    state->StopEvent, overlapped.hEvent,
#ifdef BML_TEST
                    state->InterruptEvent,
#endif
                };
                const DWORD wait = ::WaitForMultipleObjects(
                    static_cast<DWORD>(std::size(handles)), handles, FALSE, INFINITE);
                if (wait != WAIT_OBJECT_0 + 1) {
                    CancelAndDrainOverlapped(directory.Get(), overlapped);
                    if (wait == WAIT_OBJECT_0)
                        return;
                    break;
                }

                DWORD bytes = 0;
                if (!::GetOverlappedResult(directory.Get(), &overlapped, &bytes, FALSE))
                    break;
                if (!bytes) {
                    PushEvent(Event{state->Root, state->Root, 0, true, state->Recursive});
                    continue;
                }

                size_t offset = 0;
                while (offset < bytes) {
                    const auto *info = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(buffer.data() + offset);
                    Event changed;
                    changed.Root = state->Root;
                    changed.Path = JoinWatchedPath(state->Root, info->FileName, info->FileNameLength);
                    changed.Action = info->Action;
                    changed.Recursive = state->Recursive;
                    PushEvent(changed);
                    if (info->NextEntryOffset == 0)
                        break;
                    offset += info->NextEntryOffset;
                }
            }
            if (!started) {
                ready.set_value(false);
                return;
            }
            if (::WaitForSingleObject(state->StopEvent, 100) != WAIT_TIMEOUT)
                return;
        }
    } catch (...) {
        // A background watcher must not terminate the host process.
    }
}

#ifdef BML_TEST
bool ScriptFileWatcherWin32::InterruptReadForTest(const std::wstring &root) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto *state : m_Watches)
        if (SameRoot(state->Root, root))
            return ::SetEvent(state->InterruptEvent) != FALSE;
    return false;
}
#endif

void ScriptFileWatcherWin32::PushEvent(const Event &event) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (event.Overflow) {
        PushOverflowEventLocked(event.Root, event.Recursive);
        return;
    }
    if (m_Events.size() >= kMaxQueuedEvents) {
        ++m_DroppedEvents;
        PushOverflowEventLocked(event.Root, event.Recursive);
        return;
    }
    m_Events.push_back(event);
}

bool ScriptFileWatcherWin32::HasOverflowEventLocked(const std::wstring &root, bool recursive) const {
    for (const OverflowKey &key : m_OverflowQueued) {
        if (key.Recursive == recursive && SameRoot(key.Root, root))
            return true;
    }
    return false;
}

void ScriptFileWatcherWin32::ForgetOverflowEventLocked(const Event &event) {
    if (!event.Overflow)
        return;

    auto it = std::remove_if(m_OverflowQueued.begin(), m_OverflowQueued.end(), [&](const OverflowKey &key) {
        return key.Recursive == event.Recursive && SameRoot(key.Root, event.Root);
    });
    m_OverflowQueued.erase(it, m_OverflowQueued.end());
}

void ScriptFileWatcherWin32::PushOverflowEventLocked(const std::wstring &root, bool recursive) {
    if (HasOverflowEventLocked(root, recursive))
        return;
    if (m_Events.size() >= kMaxQueuedEvents && !m_Events.empty()) {
        ForgetOverflowEventLocked(m_Events.front());
        m_Events.erase(m_Events.begin());
        ++m_DroppedEvents;
    }

    Event overflow;
    overflow.Root = root;
    overflow.Path = root;
    overflow.Overflow = true;
    overflow.Recursive = recursive;
    m_Events.push_back(overflow);
    OverflowKey key;
    key.Root = root;
    key.Recursive = recursive;
    m_OverflowQueued.push_back(key);
}

#ifdef BML_TEST
void ScriptFileWatcherWin32::PushOverflowEventForTest(const std::wstring &root, bool recursive) {
    Event overflow;
    overflow.Root = root;
    overflow.Path = root;
    overflow.Overflow = true;
    overflow.Recursive = recursive;
    PushEvent(overflow);
}
#endif

} // namespace BML
