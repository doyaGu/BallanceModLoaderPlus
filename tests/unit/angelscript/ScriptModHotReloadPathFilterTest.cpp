#include <gtest/gtest.h>

#include "AngelScript/ScriptModHotReloadPathFilter.h"

namespace BML {
namespace Test {

namespace {

ScriptFileWatcher::Event MakeEvent(const wchar_t *root,
                                        const wchar_t *path,
                                        uint32_t action,
                                        bool recursive = true) {
    ScriptFileWatcher::Event event;
    event.Root = root ? root : L"";
    event.Path = path ? path : L"";
    event.Action = action;
    event.Recursive = recursive;
    return event;
}

ScriptFileWatcher::Event MakeOverflow(const wchar_t *root, bool recursive = true) {
    ScriptFileWatcher::Event event;
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

ScriptLibraryUse MakeLibraryUse() {
    ScriptLibraryUse library;
    library.Id = "com.example.score";
    library.Version = "1.2.0";
    library.RootDirectory = L"C:\\ModLoader\\ScriptLibs\\com.example.score\\1.2.0";
    library.VirtualRoot = "/bml/libs/com.example.score@1.2.0/";
    return library;
}

} // namespace

TEST(ScriptModHotReloadPathFilterTest, DirectoryRootLifecycleEventsAreRelevant) {
    const ScriptModEntry entry = MakeDirectoryEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", ScriptFileRemoved, false),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", ScriptFileRenamedOld, false),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", ScriptFileRenamedNew, false),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", ScriptFileModified, false),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, DirectoryEntriesAndHelpersAreRelevant) {
    const ScriptModEntry entry = MakeDirectoryEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\Hello.mod.as", ScriptFileModified),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\scripts\\helper.as", ScriptFileModified),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\readme.txt", ScriptFileModified),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, SingleFileResourceRootLifecycleEventsAreRelevant) {
    const ScriptModEntry entry = MakeSingleFileEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello", ScriptFileRemoved, false),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\helper.as", ScriptFileModified),
        entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello.mod.as", ScriptFileRemoved, false),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello", L"C:\\Mods\\Hello\\asset.png", ScriptFileModified),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, ZipPackagesOnlyReactToTheirArchive) {
    const ScriptModEntry entry = MakeZipEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods", L"C:\\Mods\\Hello.zip", ScriptFileModified, false),
        entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(
        MakeEvent(L"C:\\Mods\\Hello.reload.1", L"C:\\Mods\\Hello.reload.1\\helper.as", ScriptFileModified),
        entry));
}

TEST(ScriptModHotReloadPathFilterTest, OverflowMatchesWatchedScope) {
    const ScriptModEntry entry = MakeDirectoryEntry();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(MakeOverflow(L"C:\\Mods", false), entry));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevant(MakeOverflow(L"C:\\Mods\\Hello", true), entry));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevant(MakeOverflow(L"C:\\Other", true), entry));
}

TEST(ScriptModHotReloadPathFilterTest, LibraryUseSourceFilesAreRelevant) {
    const ScriptLibraryUse library = MakeLibraryUse();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevantToLibraryUse(
        MakeEvent(L"C:\\ModLoader\\ScriptLibs", L"C:\\ModLoader\\ScriptLibs\\com.example.score\\1.2.0\\api.as", ScriptFileModified),
        library));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevantToLibraryUse(
        MakeEvent(L"C:\\ModLoader\\ScriptLibs", L"C:\\ModLoader\\ScriptLibs\\com.example.score\\1.2.0", ScriptFileRemoved),
        library));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevantToLibraryUse(
        MakeEvent(L"C:\\ModLoader\\ScriptLibs", L"C:\\ModLoader\\ScriptLibs\\com.example.score\\1.2.0\\readme.md", ScriptFileModified),
        library));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevantToLibraryUse(
        MakeEvent(L"C:\\ModLoader\\ScriptLibs", L"C:\\ModLoader\\ScriptLibs\\com.example.other\\1.2.0\\api.as", ScriptFileModified),
        library));
}

TEST(ScriptModHotReloadPathFilterTest, LibraryUseOverflowMatchesPackageScope) {
    const ScriptLibraryUse library = MakeLibraryUse();

    EXPECT_TRUE(ScriptHotReloadEventLooksRelevantToLibraryUse(MakeOverflow(L"C:\\ModLoader\\ScriptLibs", true), library));
    EXPECT_TRUE(ScriptHotReloadEventLooksRelevantToLibraryUse(MakeOverflow(L"C:\\ModLoader\\ScriptLibs\\com.example.score\\1.2.0", true), library));
    EXPECT_FALSE(ScriptHotReloadEventLooksRelevantToLibraryUse(MakeOverflow(L"C:\\Other", true), library));
}

} // namespace Test
} // namespace BML
