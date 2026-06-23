#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <Windows.h>

#include "AngelScript/ScriptFileWatcherWin32.h"

namespace BML {
namespace Test {

namespace {

std::filesystem::path MakeTempWatchRoot() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root = std::filesystem::temp_directory_path() /
                                 ("bml-script-watch-" + std::to_string(now));
    std::filesystem::create_directories(root);
    return root;
}

class TempWatchRoot {
public:
    TempWatchRoot() : m_Path(MakeTempWatchRoot()) {}
    ~TempWatchRoot() { std::filesystem::remove_all(m_Path); }

    const std::filesystem::path &Path() const { return m_Path; }

private:
    std::filesystem::path m_Path;
};

bool EndsWithInsensitive(const std::wstring &value, const std::wstring &suffix) {
    if (value.size() < suffix.size())
        return false;
    return _wcsicmp(value.c_str() + value.size() - suffix.size(), suffix.c_str()) == 0;
}

std::vector<ScriptFileWatcherWin32::Event> DrainUntil(ScriptFileWatcherWin32 &watcher,
                                                      const std::function<bool(const std::vector<ScriptFileWatcherWin32::Event> &)> &predicate) {
    std::vector<ScriptFileWatcherWin32::Event> all;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        std::vector<ScriptFileWatcherWin32::Event> batch = watcher.DrainEvents();
        all.insert(all.end(), batch.begin(), batch.end());
        if (predicate(all))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return all;
}

bool HasPathEvent(const std::vector<ScriptFileWatcherWin32::Event> &events, const std::wstring &suffix) {
    for (const auto &event : events) {
        if (!event.Overflow && EndsWithInsensitive(event.Path, suffix))
            return true;
    }
    return false;
}

void WriteText(const std::filesystem::path &path, const char *text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

} // namespace

TEST(ScriptFileWatcherWin32Test, ReportsCreateWriteRenameAndDelete) {
    const TempWatchRoot root;
    ScriptFileWatcherWin32 watcher;
    ASSERT_TRUE(watcher.Watch(root.Path().wstring()));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const std::filesystem::path entry = root.Path() / "WatcherSmoke.mod.as";
    WriteText(entry, "class WatcherSmoke {}\n");
    std::vector<ScriptFileWatcherWin32::Event> events = DrainUntil(watcher, [](const auto &seen) {
        return HasPathEvent(seen, L"WatcherSmoke.mod.as");
    });
    EXPECT_TRUE(HasPathEvent(events, L"WatcherSmoke.mod.as"));

    WriteText(entry, "class WatcherSmoke { void OnLoad() {} }\n");
    events = DrainUntil(watcher, [](const auto &seen) {
        return HasPathEvent(seen, L"WatcherSmoke.mod.as");
    });
    EXPECT_TRUE(HasPathEvent(events, L"WatcherSmoke.mod.as"));

    const std::filesystem::path temp = root.Path() / "WatcherSmoke.tmp";
    std::filesystem::rename(entry, temp);
    std::filesystem::rename(temp, entry);
    events = DrainUntil(watcher, [](const auto &seen) {
        return HasPathEvent(seen, L"WatcherSmoke.tmp") &&
               HasPathEvent(seen, L"WatcherSmoke.mod.as");
    });
    EXPECT_TRUE(HasPathEvent(events, L"WatcherSmoke.tmp"));
    EXPECT_TRUE(HasPathEvent(events, L"WatcherSmoke.mod.as"));

    std::filesystem::remove(entry);
    events = DrainUntil(watcher, [](const auto &seen) {
        return HasPathEvent(seen, L"WatcherSmoke.mod.as");
    });
    EXPECT_TRUE(HasPathEvent(events, L"WatcherSmoke.mod.as"));

    watcher.StopAll();
}

TEST(ScriptFileWatcherWin32Test, StopAllIsIdempotent) {
    const TempWatchRoot root;
    ScriptFileWatcherWin32 watcher;
    ASSERT_TRUE(watcher.Watch(root.Path().wstring()));
    watcher.StopAll();
    watcher.StopAll();
    EXPECT_TRUE(watcher.DrainEvents().empty());
}

TEST(ScriptFileWatcherWin32Test, StopAllClearsQueuedEvents) {
    const TempWatchRoot root;
    ScriptFileWatcherWin32 watcher;
    ASSERT_TRUE(watcher.Watch(root.Path().wstring()));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const std::filesystem::path entry = root.Path() / "Stale.mod.as";
    WriteText(entry, "class Stale {}\n");
    const std::vector<ScriptFileWatcherWin32::Event> beforeStop = DrainUntil(watcher, [](const auto &seen) {
        return HasPathEvent(seen, L"Stale.mod.as");
    });
    ASSERT_TRUE(HasPathEvent(beforeStop, L"Stale.mod.as"));

    WriteText(entry, "class Stale { void OnLoad() {} }\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    watcher.StopAll();
    EXPECT_TRUE(watcher.DrainEvents().empty());
}

} // namespace Test
} // namespace BML
