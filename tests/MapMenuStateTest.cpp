#include <string>

#include <gtest/gtest.h>

#include "MapMenu.h"

namespace {

TEST(MapMenuStateTest, FailedSelectionKeepsDirectoryAndMenuOpen) {
    bool called = false;
    std::wstring receivedPath;
    MapMenuState state([&](const std::wstring &path) {
        called = true;
        receivedPath = path;
        return false;
    });
    MapEntry directory(nullptr, MAP_ENTRY_DIR);
    state.SetCurrentMaps(&directory);

    const Bui::PageAction action = state.SelectMap(L"missing.nmo");

    EXPECT_TRUE(called);
    EXPECT_EQ(receivedPath, L"missing.nmo");
    EXPECT_TRUE(action.IsNone());
    EXPECT_EQ(state.GetCurrentMaps(), &directory);
    EXPECT_FALSE(state.TakeMapLoaded());
}

TEST(MapMenuStateTest, SuccessfulSelectionResetsDirectoryAndRequestsClose) {
    MapMenuState state([](const std::wstring &) { return true; });
    MapEntry *root = state.GetCurrentMaps();
    MapEntry directory(nullptr, MAP_ENTRY_DIR);
    state.SetCurrentMaps(&directory);

    const Bui::PageAction action = state.SelectMap(L"example.nmo");

    EXPECT_FALSE(action.IsNone());
    EXPECT_EQ(state.GetCurrentMaps(), root);
    EXPECT_TRUE(state.TakeMapLoaded());
    EXPECT_FALSE(state.TakeMapLoaded());
}

} // namespace
