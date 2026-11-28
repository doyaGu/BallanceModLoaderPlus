#include "AngelScript/ScriptFileWatcher.h"
#include "Utils/PathUtils.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace BML;

static void Require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}

static void WaitFor(ScriptFileWatcher &watcher, const std::wstring &path, uint32_t action) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        for (const auto &event : watcher.DrainEvents())
            if (event.Path == path && !event.Overflow &&
                (event.Action == action ||
                 (event.Action == ScriptFileRenamedNew &&
                  (action == ScriptFileAdded || action == ScriptFileModified))))
                return;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    throw std::runtime_error("expected file notification was not delivered");
}

int main() {
    std::wstring root;
    try {
        const auto marker = utils::CreateTempFileW(L"bml-watcher-");
        Require(!marker.empty(), "temporary marker");
        root = marker + L".dir";
        Require(utils::CreateFileTreeW(root), "temporary directory");
        utils::DeleteFileW(marker);
        const auto nested = utils::CombinePathW(root, L"中文-🎮");
        Require(utils::CreateFileTreeW(nested), "Unicode directory");
        ScriptFileWatcher watcher;
        Require(watcher.Watch(root), "start recursive watch");
        Require(watcher.Watch(root), "duplicate watch is idempotent");
        const auto source = utils::CombinePathW(nested, L"main.as");
        uint32_t error = 0;
        Require(utils::WriteFileBytesAtomicW(source, "void Main() {}", error), "create script");
        WaitFor(watcher, source, ScriptFileAdded);
        Require(utils::WriteFileBytesAtomicW(source, "void Main() { int x = 1; }", error), "replace script");
        WaitFor(watcher, source, ScriptFileModified);
        const auto renamed = utils::CombinePathW(nested, L"Main.as");
        // Rename to an unrelated filename works on both case-sensitive and
        // case-insensitive volumes; Linux also observes distinct case names.
        const auto destination = utils::CombinePathW(nested, L"other.as");
        Require(utils::MoveFileW(source, destination), "rename script");
        WaitFor(watcher, destination, ScriptFileAdded);
        Require(utils::DeleteFileW(destination), "delete script");
        WaitFor(watcher, destination, ScriptFileRemoved);
#if !BML_RUNTIME_SDL
        Require(watcher.InterruptReadForTest(root), "interrupt active overlapped read");
        const auto recoveryDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        bool rescans = false;
        while (!rescans && std::chrono::steady_clock::now() < recoveryDeadline) {
            for (const auto &event : watcher.DrainEvents())
                rescans = rescans || (event.Overflow && event.Root == root && event.Recursive);
            if (!rescans)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        Require(rescans, "reattached watch requests rescan");
        Require(utils::WriteFileBytesAtomicW(source, "void Main() {}", error), "edit after watch recovery");
        WaitFor(watcher, source, ScriptFileAdded);
#endif
        Require(watcher.Unwatch(root), "stop watch");
        Require(!watcher.Unwatch(root), "watch removed exactly once");
        Require(watcher.Watch(root, false), "restart shallow watch");
        Require(utils::WriteFileBytesAtomicW(renamed, "void Main() {}", error), "nested file under shallow watch");
        const auto direct = utils::CombinePathW(root, L"direct.as");
        Require(utils::WriteFileBytesAtomicW(direct, "void Main() {}", error), "direct script");
        WaitFor(watcher, direct, ScriptFileAdded);
        for (const auto &event : watcher.DrainEvents())
            Require(event.Path != renamed, "shallow watch must not recurse");
        const auto start = std::chrono::steady_clock::now();
        watcher.StopAll();
        Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1), "prompt worker shutdown");
        Require(watcher.DrainEvents().empty(), "shutdown clears queued events");
        Require(utils::DeleteDirectoryW(root), "remove fixture");
        std::cout << "SDL script watcher passed\n";
        return 0;
    } catch (const std::exception &error) {
        if (!root.empty()) utils::DeleteDirectoryW(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
