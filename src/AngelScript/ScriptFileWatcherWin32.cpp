#include "ScriptFileWatcherWin32.h"

#include <algorithm>
#include <memory>

namespace BML {

namespace {

constexpr size_t kMaxQueuedEvents = 4096;

std::wstring JoinWatchedPath(const std::wstring &root, const wchar_t *relativeName, DWORD lengthBytes) {
    std::wstring path = root;
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
        path.push_back(L'\\');
    path.append(relativeName, relativeName + (lengthBytes / sizeof(wchar_t)));
    return path;
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
        if (state->Directory != INVALID_HANDLE_VALUE)
            ::CancelIoEx(state->Directory, nullptr);
    }

    for (WatchState *state : watches) {
        if (!state)
            continue;
        if (state->Worker.joinable())
            state->Worker.join();
        if (state->Directory != INVALID_HANDLE_VALUE)
            ::CloseHandle(state->Directory);
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
    state->Directory = ::CreateFileW(root.c_str(),
                                     FILE_LIST_DIRECTORY,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr,
                                     OPEN_EXISTING,
                                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                     nullptr);
    if (!state->StopEvent || state->Directory == INVALID_HANDLE_VALUE) {
        if (state->Directory != INVALID_HANDLE_VALUE)
            ::CloseHandle(state->Directory);
        if (state->StopEvent)
            ::CloseHandle(state->StopEvent);
        delete state;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Watches.push_back(state);
    }
    state->Worker = std::thread([this, state] { WorkerLoop(state); });
    return true;
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
        m_OverflowQueued = false;
    }
}

std::vector<ScriptFileWatcherWin32::Event> ScriptFileWatcherWin32::DrainEvents() {
    std::vector<Event> events;
    std::lock_guard<std::mutex> lock(m_Mutex);
    events.swap(m_Events);
    m_OverflowQueued = false;
    return events;
}

uint64_t ScriptFileWatcherWin32::GetDroppedEventCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_DroppedEvents;
}

void ScriptFileWatcherWin32::WorkerLoop(WatchState *state) {
    if (!state)
        return;

    std::vector<unsigned char> buffer(64 * 1024);
    while (::WaitForSingleObject(state->StopEvent, 0) == WAIT_TIMEOUT) {
        OVERLAPPED overlapped = {};
        overlapped.hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent)
            break;

        const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME |
                             FILE_NOTIFY_CHANGE_DIR_NAME |
                             FILE_NOTIFY_CHANGE_LAST_WRITE |
                             FILE_NOTIFY_CHANGE_SIZE;
        const BOOL started = ::ReadDirectoryChangesW(state->Directory,
                                                     buffer.data(),
                                                     static_cast<DWORD>(buffer.size()),
                                                     state->Recursive ? TRUE : FALSE,
                                                     filter,
                                                     nullptr,
                                                     &overlapped,
                                                     nullptr);
        if (!started) {
            ::CloseHandle(overlapped.hEvent);
            break;
        }

        HANDLE handles[2] = {state->StopEvent, overlapped.hEvent};
        const DWORD wait = ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            ::CancelIoEx(state->Directory, &overlapped);
            DWORD ignored = 0;
            ::GetOverlappedResult(state->Directory, &overlapped, &ignored, TRUE);
            ::CloseHandle(overlapped.hEvent);
            break;
        }

        DWORD bytes = 0;
        if (::GetOverlappedResult(state->Directory, &overlapped, &bytes, FALSE)) {
            if (bytes == 0) {
                Event overflow;
                overflow.Root = state->Root;
                overflow.Path = state->Root;
                overflow.Overflow = true;
                overflow.Recursive = state->Recursive;
                PushEvent(overflow);
                ::CloseHandle(overlapped.hEvent);
                continue;
            }

            size_t offset = 0;
            while (offset < bytes) {
                auto *info = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer.data() + offset);
                Event event;
                event.Root = state->Root;
                event.Path = JoinWatchedPath(state->Root, info->FileName, info->FileNameLength);
                event.Action = info->Action;
                event.Recursive = state->Recursive;
                PushEvent(event);
                if (info->NextEntryOffset == 0)
                    break;
                offset += info->NextEntryOffset;
            }
        }
        ::CloseHandle(overlapped.hEvent);
    }
}

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

void ScriptFileWatcherWin32::PushOverflowEventLocked(const std::wstring &root, bool recursive) {
    if (m_OverflowQueued)
        return;
    if (m_Events.size() >= kMaxQueuedEvents && !m_Events.empty()) {
        m_Events.erase(m_Events.begin());
        ++m_DroppedEvents;
    }

    Event overflow;
    overflow.Root = root;
    overflow.Path = root;
    overflow.Overflow = true;
    overflow.Recursive = recursive;
    m_Events.push_back(overflow);
    m_OverflowQueued = true;
}

} // namespace BML
