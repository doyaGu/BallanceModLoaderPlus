#include "ScriptFileWatcher.h"

#include <SDL3/SDL_filesystem.h>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include "Platform/FileSystemNative.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {
namespace {
struct Stamp {
    SDL_PathType Type;
    Uint64 Size;
    SDL_Time Modified;
    SDL_Time Created;
    bool operator==(const Stamp &) const = default;
};
using Snapshot = std::unordered_map<std::string, Stamp>;
constexpr size_t MaxEvents = 4096;
constexpr size_t MaxEntries = 100000;

bool Scan(const std::string &root, bool recursive, Snapshot &snapshot) {
    std::vector<std::string> directories{root};
    for (size_t i = 0; i < directories.size(); ++i) {
        // Copy the current directory: the callback may grow the vector.
        const std::string directory = directories[i];
        SDL_PathInfo info{};
        if (!SDL_GetPathInfo(directory.c_str(), &info) || info.type != SDL_PATHTYPE_DIRECTORY)
            return false;
        snapshot[directory] = {info.type, 0, info.modify_time, info.create_time};
        struct Visit {
            Snapshot &Files;
            std::vector<std::string> &Directories;
            bool Recursive;
            bool Complete = true;
        } visit{snapshot, directories, recursive};
        const bool enumerated = SDL_EnumerateDirectory(directory.c_str(),
            [](void *data, const char *parent, const char *name) -> SDL_EnumerationResult {
                auto &visit = *static_cast<Visit *>(data);
                try {
                    const auto path = utils::CombinePathUtf8(parent, name);
                    if (Platform::IsLink(path)) return SDL_ENUM_CONTINUE;
                    SDL_PathInfo info{};
                    if (visit.Files.size() >= MaxEntries || !SDL_GetPathInfo(path.c_str(), &info)) {
                        visit.Complete = false;
                        return SDL_ENUM_FAILURE;
                    }
                    visit.Files[path] = {info.type, info.size, info.modify_time, info.create_time};
                    if (visit.Recursive && info.type == SDL_PATHTYPE_DIRECTORY)
                        visit.Directories.push_back(path);
                    return SDL_ENUM_CONTINUE;
                } catch (...) {
                    visit.Complete = false;
                    return SDL_ENUM_FAILURE;
                }
            }, &visit);
        if (!enumerated || !visit.Complete) return false;
    }
    return true;
}
}

struct ScriptFileWatcherSdl::WatchState {
    std::wstring Root;
    std::string Utf8Root;
    bool Recursive = true;
    std::mutex Mutex;
    std::condition_variable Wake;
    bool Stopped = false;
    Snapshot Files;
    std::thread Worker;
};

ScriptFileWatcherSdl::ScriptFileWatcherSdl() = default;
ScriptFileWatcherSdl::~ScriptFileWatcherSdl() { StopAll(); }

bool ScriptFileWatcherSdl::Watch(const std::wstring &root, bool recursive) {
    if (root.empty()) return false;
    auto state = std::make_unique<WatchState>();
    state->Root = utils::TrimTrailingSeparatorsW(utils::ResolvePathW(root));
    state->Utf8Root = utils::Utf16ToUtf8(state->Root);
    state->Recursive = recursive;
    if (Platform::IsLink(state->Utf8Root) || !Scan(state->Utf8Root, recursive, state->Files)) return false;
    std::lock_guard lock(m_Mutex);
    for (const auto &existing : m_Watches)
        if (existing->Root == state->Root && existing->Recursive == recursive) return true;
    // Allocate registry space before starting the worker so exceptions cannot
    // destroy a joinable thread that has not yet been published.
    m_Watches.reserve(m_Watches.size() + 1);
    auto *workerState = state.get();
    state->Worker = std::thread([this, workerState] { WorkerLoop(*workerState); });
    m_Watches.push_back(std::move(state));
    return true;
}

void ScriptFileWatcherSdl::Stop(std::vector<std::unique_ptr<WatchState>> &states) {
    for (auto &state : states) {
        { std::lock_guard lock(state->Mutex); state->Stopped = true; }
        state->Wake.notify_all();
    }
    for (auto &state : states) if (state->Worker.joinable()) state->Worker.join();
}

bool ScriptFileWatcherSdl::Unwatch(const std::wstring &root) {
    const auto resolved = utils::TrimTrailingSeparatorsW(utils::ResolvePathW(root));
    std::vector<std::unique_ptr<WatchState>> removed;
    {
        std::lock_guard lock(m_Mutex);
        for (auto it = m_Watches.begin(); it != m_Watches.end();) {
            if ((*it)->Root == resolved) {
                removed.push_back(std::move(*it));
                it = m_Watches.erase(it);
            } else ++it;
        }
    }
    Stop(removed);
    return !removed.empty();
}

void ScriptFileWatcherSdl::StopAll() {
    std::vector<std::unique_ptr<WatchState>> stopped;
    { std::lock_guard lock(m_Mutex); stopped.swap(m_Watches); }
    Stop(stopped);
    std::lock_guard lock(m_Mutex);
    m_Events.clear();
    m_Overflows.clear();
}

void ScriptFileWatcherSdl::WorkerLoop(WatchState &state) {
    bool incomplete = false;
    try {
        for (;;) {
            {
                std::unique_lock lock(state.Mutex);
                if (state.Wake.wait_for(lock, std::chrono::milliseconds(100), [&] { return state.Stopped; })) return;
            }
            Snapshot next;
            if (!Scan(state.Utf8Root, state.Recursive, next)) {
                if (!incomplete) PushEvent({state.Root, state.Root, 0, true, state.Recursive});
                incomplete = true;
                continue; // Never manufacture removals from a partial scan.
            }
            if (incomplete) PushEvent({state.Root, state.Root, 0, true, state.Recursive});
            incomplete = false;
            for (const auto &[path, stamp] : next) {
                auto previous = state.Files.find(path);
                const uint32_t action = previous == state.Files.end() ? ScriptFileAdded : ScriptFileModified;
                if (previous == state.Files.end() || !(stamp == previous->second))
                    PushEvent({state.Root, utils::Utf8ToUtf16(path), action, false, state.Recursive});
            }
            for (const auto &[path, stamp] : state.Files)
                if (!next.contains(path)) PushEvent({state.Root, utils::Utf8ToUtf16(path), ScriptFileRemoved, false, state.Recursive});
            state.Files.swap(next);
        }
    } catch (...) {
        // Request a full rescan after an unexpected worker failure.
        try { PushEvent({state.Root, state.Root, 0, true, state.Recursive}); } catch (...) {}
    }
}

void ScriptFileWatcherSdl::PushEvent(Event event) {
    std::lock_guard lock(m_Mutex);
    if (!event.Overflow && m_Events.size() < MaxEvents) {
        m_Events.push_back(std::move(event));
        return;
    }
    if (!event.Overflow) ++m_DroppedEvents;
    for (const auto &existing : m_Overflows)
        if (existing.Root == event.Root && existing.Recursive == event.Recursive) return;
    event.Path = event.Root;
    event.Action = 0;
    event.Overflow = true;
    m_Overflows.push_back(std::move(event));
}

std::vector<ScriptFileWatcherSdl::Event> ScriptFileWatcherSdl::DrainEvents() {
    std::lock_guard lock(m_Mutex);
    std::vector<Event> events;
    events.swap(m_Events);
    events.insert(events.end(), std::make_move_iterator(m_Overflows.begin()), std::make_move_iterator(m_Overflows.end()));
    m_Overflows.clear();
    return events;
}

uint64_t ScriptFileWatcherSdl::GetDroppedEventCount() const {
    std::lock_guard lock(m_Mutex);
    return m_DroppedEvents;
}
}
