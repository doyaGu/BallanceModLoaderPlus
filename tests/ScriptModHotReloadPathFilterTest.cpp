#include <gtest/gtest.h>

#include "AngelScript/ScriptModHotReloadPathFilter.h"

namespace BML {
namespace Test {

namespace {

ScriptFileWatcherWin32::Event MakeEvent(const wchar_t *root,
                                        const wchar_t *path,
                                        DWORD action,
                                        bool recursive = true) {
    ScriptFileWatcherWin32::Event event;
    event.Root = root ? root : L"";
    event.Path = path ? path : L"";
    event.Action = action;
    event.Recursive = recursive;
    return event;
}

ScriptFileWatcherWin32::Event MakeOverflow(const wchar_t *root, bool recursive = true) {
    ScriptFileWatcherWin32::Event event;
    event.Root = root ? root : L"";
    event.Path = event.Root;
    event.Overflow = true;
    event.Recursive = recursive;
    return event;
}

ScriptModEntry MakeDirectoryEntry() {
    ScriptModEntry entry;
    entry.SourceKind = ScriptModEntrySourceKind::Directory;
    entry.RootDirectory = L"C:\\Mods\\Hello";
    entry.EntryPath = L"C:\\Mods\\Hello\\Hello.mod.as";
    return entry;
}

ScriptModEntry MakeSingleFileEntry() {
    ScriptModEntry entry;
    entry.SourceKind = ScriptModEntrySourceKind::SingleFile;
    entry.SourcePath = L"C:\\Mods\\Hello.mod.as";
    entry.EntryPath = L"C:\\Mods\\Hello.mod.as";
    entry.ResourceRootDirectory = L"C:\\Mods\\Hello";
    return entry;
}

ScriptModEntry MakeZipEntry() {
    ScriptModEntry entry;
    entry.SourceKind = ScriptModEntrySourceKind::ZipPackage;
    entry.SourcePath = L"C:\\Mods\\Hello.zip";
    entry.RootDirectory = L"C:\\Mods\\Hello.reload.1";
    entry.EntryPath = L"C:\\Mods\\Hello.reload.1\\Hello.mod.as";
    return entry;
}

} // namespace

TEST(ScriptModHotReloadPathFilterTest, DirectoryRootLifecycleEventsAreRelevant) {
    const ScriptModEntry entry = MakeDirectoryEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", FILE_ACTION_REMOVED, false),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", FILE_ACTION_RENAMED_OLD_NAME, false),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", FILE_ACTION_RENAMED_NEW_NAME, false),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", FILE_ACTION_MODIFIED, false),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, DirectoryEntriesAndHelpersAreRelevant) {
    const ScriptModEntry entry = MakeDirectoryEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\Hello.mod.as", FILE_ACTION_MODIFIED),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\scripts\\helper.as", FILE_ACTION_MODIFIED),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\readme.txt", FILE_ACTION_MODIFIED),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, SingleFileResourceRootLifecycleEventsAreRelevant) {
    const ScriptModEntry entry = MakeSingleFileEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", FILE_ACTION_REMOVED, false),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\helper.as", FILE_ACTION_MODIFIED),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello.mod.as", FILE_ACTION_REMOVED, false),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\asset.png", FILE_ACTION_MODIFIED),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, ZipPackagesOnlyReactToTheirArchive) {
    const ScriptModEntry entry = MakeZipEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello.zip", FILE_ACTION_MODIFIED, false),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello.reload.1", L"C:\\Mods\\Hello.reload.1\\helper.as", FILE_ACTION_MODIFIED),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, OverflowMatchesWatchedScope) {
    const ScriptModEntry entry = MakeDirectoryEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(MakeOverflow(L"C:\\Mods", false), entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(MakeOverflow(L"C:\\Mods\\Hello", true), entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(MakeOverflow(L"C:\\Other", true), entry));
}

} // namespace Test
} // namespace BML
