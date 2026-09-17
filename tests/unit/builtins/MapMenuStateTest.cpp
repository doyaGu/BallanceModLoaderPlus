#include <string>

#include <gtest/gtest.h>

#include "CustomMaps/CustomMapLoad.h"
#include "CustomMaps/MapMenuState.h"

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

    const bool accepted = state.BeginLoad(L"missing.nmo");

    EXPECT_TRUE(called);
    EXPECT_EQ(receivedPath, L"missing.nmo");
    EXPECT_FALSE(accepted);
    EXPECT_EQ(state.GetCurrentMaps(), &directory);
    EXPECT_FALSE(state.TakeMapLoaded());
}

TEST(MapMenuStateTest, AcceptedSelectionWaitsForLoadCompletion) {
    MapMenuState state([](const std::wstring &) { return true; });
    MapEntry directory(nullptr, MAP_ENTRY_DIR);
    state.SetCurrentMaps(&directory);

    const bool accepted = state.BeginLoad(L"example.nmo");

    EXPECT_TRUE(accepted);
    EXPECT_TRUE(state.IsLoading());
    EXPECT_EQ(state.GetCurrentMaps(), &directory);
    EXPECT_FALSE(state.TakeMapLoaded());
    EXPECT_FALSE(state.TakeCloseRequest());
}

TEST(MapMenuStateTest, SuccessfulCompletionResetsDirectoryAndRequestsClose) {
    MapMenuState state([](const std::wstring &) { return true; });
    MapEntry *root = state.GetCurrentMaps();
    MapEntry directory(nullptr, MAP_ENTRY_DIR);
    state.SetCurrentMaps(&directory);

    ASSERT_TRUE(state.BeginLoad(L"example.nmo"));
    EXPECT_TRUE(state.CompleteLoad(true));

    EXPECT_FALSE(state.IsLoading());
    EXPECT_EQ(state.GetCurrentMaps(), root);
    EXPECT_TRUE(state.TakeCloseRequest());
    EXPECT_FALSE(state.TakeCloseRequest());
    EXPECT_TRUE(state.TakeMapLoaded());
    EXPECT_FALSE(state.TakeMapLoaded());
}

TEST(MapMenuStateTest, FailedCompletionKeepsDirectoryAndMenuOpen) {
    MapMenuState state([](const std::wstring &) { return true; });
    MapEntry directory(nullptr, MAP_ENTRY_DIR);
    state.SetCurrentMaps(&directory);

    ASSERT_TRUE(state.BeginLoad(L"example.nmo"));
    EXPECT_TRUE(state.CompleteLoad(false));

    EXPECT_FALSE(state.IsLoading());
    EXPECT_EQ(state.GetCurrentMaps(), &directory);
    EXPECT_FALSE(state.TakeCloseRequest());
    EXPECT_FALSE(state.TakeMapLoaded());
}

TEST(MapMenuStateTest, PendingSelectionCannotStartAnotherLoad) {
    int calls = 0;
    MapMenuState state([&](const std::wstring &) {
        ++calls;
        return true;
    });

    ASSERT_TRUE(state.BeginLoad(L"first.nmo"));
    EXPECT_FALSE(state.BeginLoad(L"second.nmo"));
    EXPECT_EQ(calls, 1);
}

TEST(MapMenuStateTest, ResetAllowsReuseAfterAnInterruptedLoad) {
    int calls = 0;
    MapMenuState state([&](const std::wstring &) {
        ++calls;
        return true;
    });

    ASSERT_TRUE(state.BeginLoad(L"first.nmo"));
    state.ResetLoad();

    EXPECT_FALSE(state.IsLoading());
    EXPECT_TRUE(state.BeginLoad(L"second.nmo"));
    EXPECT_EQ(calls, 2);
}

TEST(MapMenuStateTest, SynchronousCompletionIsNotOverwrittenByRequestReturn) {
    MapMenuState *active = nullptr;
    MapMenuState state([&](const std::wstring &) {
        return active->CompleteLoad(true);
    });
    active = &state;

    EXPECT_TRUE(state.BeginLoad(L"example.nmo"));
    EXPECT_FALSE(state.IsLoading());
    EXPECT_TRUE(state.TakeCloseRequest());
    EXPECT_TRUE(state.TakeMapLoaded());
}

TEST(CustomMapLoadPathTest, DistinguishesFullSourcePathAndAttempt) {
    const std::wstring extension = L".nmo";
    const std::wstring first = CustomMapLoad::MakeTempFileName(
        L"C:\\Maps\\First\\same.nmo", extension, 1);
    const std::wstring second = CustomMapLoad::MakeTempFileName(
        L"C:\\Maps\\Second\\same.nmo", extension, 1);
    const std::wstring repeated = CustomMapLoad::MakeTempFileName(
        L"C:\\Maps\\First\\same.nmo", extension, 2);

    EXPECT_NE(first, second);
    EXPECT_NE(first, repeated);
    EXPECT_EQ(first.substr(first.size() - extension.size()), extension);
    EXPECT_EQ(first.size(), 16u + 1u + 16u + extension.size());
}

} // namespace
