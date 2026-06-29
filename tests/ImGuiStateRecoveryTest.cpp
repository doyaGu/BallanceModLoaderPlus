#include <gtest/gtest.h>

#include "ImGuiStateRecovery.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace {

class ScopedImGuiContext {
public:
    ScopedImGuiContext() : m_Previous(ImGui::GetCurrentContext()) {
        m_Context = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_Context);
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800.0f, 600.0f);
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.Fonts->AddFontDefault();

        unsigned char *pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    ~ScopedImGuiContext() {
        ImGui::SetCurrentContext(m_Context);
        ImGui::DestroyContext(m_Context);
        ImGui::SetCurrentContext(m_Previous);
    }

private:
    ImGuiContext *m_Previous = nullptr;
    ImGuiContext *m_Context = nullptr;
};

void BeginScriptWindow(const void *owner, const char *name) {
    BML::ScriptImGuiCallState call;
    BML::BeginScriptImGuiCall(call, owner);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 120.0f));
    ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoSavedSettings);
    BML::EndScriptImGuiCall(call);
}

void StartScriptWindowMove(const void *owner) {
    BML::ScriptImGuiCallState call;
    BML::BeginScriptImGuiCall(call, owner);
    ImGui::StartMouseMovingWindow(ImGui::GetCurrentWindow());
    BML::EndScriptImGuiCall(call);
}

void EndScriptWindow(const void *owner) {
    BML::ScriptImGuiCallState call;
    BML::BeginScriptImGuiCall(call, owner);
    ImGui::End();
    BML::EndScriptImGuiCall(call);
}

} // namespace

TEST(ImGuiStateRecoveryTest, DoesNotReleaseUnownedMouseCapture) {
    ScopedImGuiContext context;
    int scriptOwner = 0;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 120.0f));
    ImGui::Begin("NativeWindow", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::StartMouseMovingWindow(ImGui::GetCurrentWindow());
    ImGuiIO &io = ImGui::GetIO();
    io.MouseDownOwned[0] = true;
    io.MouseDownOwnedUnlessPopupClose[0] = true;
    io.WantCaptureMouse = true;
    io.WantCaptureMouseUnlessPopupClose = true;

    ImGuiContext &g = *ImGui::GetCurrentContext();
    ASSERT_NE(g.ActiveId, 0u);

    EXPECT_FALSE(BML::ReleaseStaleImGuiMouseCapture(&scriptOwner));
    EXPECT_NE(g.ActiveId, 0u);
    EXPECT_TRUE(io.MouseDownOwned[0]);
    EXPECT_TRUE(io.WantCaptureMouse);

    ImGui::ClearActiveID();
    ImGui::StopMouseMovingWindow();
    ImGui::End();
    ImGui::Render();
}

TEST(ImGuiStateRecoveryTest, ReleasesOnlyMatchingScriptOwnerMouseCapture) {
    ScopedImGuiContext context;
    int scriptOwner = 0;
    int otherOwner = 0;

    ImGui::NewFrame();
    BeginScriptWindow(&scriptOwner, "ScriptWindow");
    StartScriptWindowMove(&scriptOwner);

    ImGuiIO &io = ImGui::GetIO();
    io.MouseDown[0] = true;
    io.MouseDownOwned[0] = true;
    io.MouseDownOwnedUnlessPopupClose[0] = true;
    io.WantCaptureMouse = true;
    io.WantCaptureMouseUnlessPopupClose = true;

    ImGuiContext &g = *ImGui::GetCurrentContext();
    ASSERT_NE(g.ActiveId, 0u);
    ASSERT_NE(g.MovingWindow, nullptr);

    EXPECT_FALSE(BML::ReleaseStaleImGuiMouseCapture(&otherOwner));
    EXPECT_NE(g.ActiveId, 0u);
    EXPECT_NE(g.MovingWindow, nullptr);
    EXPECT_TRUE(io.MouseDownOwned[0]);

    EXPECT_TRUE(BML::ReleaseStaleImGuiMouseCapture(&scriptOwner));
    EXPECT_EQ(g.ActiveId, 0u);
    EXPECT_EQ(g.MovingWindow, nullptr);
    EXPECT_FALSE(io.MouseDownOwned[0]);
    EXPECT_FALSE(io.MouseDownOwnedUnlessPopupClose[0]);
    EXPECT_FALSE(io.WantCaptureMouse);
    EXPECT_FALSE(io.WantCaptureMouseUnlessPopupClose);

    ImGui::End();
    ImGui::Render();
}

TEST(ImGuiStateRecoveryTest, ScriptEndDoesNotMakeParentWindowOwned) {
    ScopedImGuiContext context;
    int scriptOwner = 0;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 120.0f));
    ImGui::Begin("NativeParentWindow", nullptr, ImGuiWindowFlags_NoSavedSettings);
    BeginScriptWindow(&scriptOwner, "ScriptChildWindow");
    EndScriptWindow(&scriptOwner);

    ImGui::StartMouseMovingWindow(ImGui::GetCurrentWindow());
    ImGuiIO &io = ImGui::GetIO();
    io.MouseDownOwned[0] = true;
    io.WantCaptureMouse = true;

    ImGuiContext &g = *ImGui::GetCurrentContext();
    ASSERT_NE(g.ActiveId, 0u);
    ASSERT_NE(g.MovingWindow, nullptr);

    EXPECT_FALSE(BML::ReleaseStaleImGuiMouseCapture(&scriptOwner));
    EXPECT_NE(g.ActiveId, 0u);
    EXPECT_NE(g.MovingWindow, nullptr);
    EXPECT_TRUE(io.MouseDownOwned[0]);
    EXPECT_TRUE(io.WantCaptureMouse);

    ImGui::ClearActiveID();
    ImGui::StopMouseMovingWindow();
    ImGui::End();
    ImGui::Render();
}

TEST(ImGuiStateRecoveryTest, ReleasesMatchingScriptNextFrameMouseCaptureOverride) {
    ScopedImGuiContext context;
    int scriptOwner = 0;
    int otherOwner = 0;

    ImGui::NewFrame();
    BML::ScriptImGuiCallState call;
    BML::BeginScriptImGuiCall(call, &scriptOwner);
    ImGui::SetNextFrameWantCaptureMouse(true);
    BML::EndScriptImGuiCall(call);

    ImGuiContext &g = *ImGui::GetCurrentContext();
    ASSERT_EQ(g.WantCaptureMouseNextFrame, 1);

    EXPECT_FALSE(BML::ReleaseStaleImGuiMouseCapture(&otherOwner));
    EXPECT_EQ(g.WantCaptureMouseNextFrame, 1);

    EXPECT_TRUE(BML::ReleaseStaleImGuiMouseCapture(&scriptOwner));
    EXPECT_EQ(g.WantCaptureMouseNextFrame, -1);

    ImGui::Render();
}

TEST(ImGuiStateRecoveryTest, ReleasesMatchingScriptHoveredWindowCapture) {
    ScopedImGuiContext context;
    int scriptOwner = 0;
    int otherOwner = 0;

    ImGui::NewFrame();
    BeginScriptWindow(&scriptOwner, "ScriptHoverWindow");

    ImGuiContext &g = *ImGui::GetCurrentContext();
    ImGuiWindow *scriptWindow = ImGui::GetCurrentWindow();
    ASSERT_NE(scriptWindow, nullptr);

    g.HoveredWindow = scriptWindow;
    g.HoveredWindowUnderMovingWindow = scriptWindow;
    g.HoveredWindowBeforeClear = scriptWindow;
    ImGuiIO &io = ImGui::GetIO();
    io.WantCaptureMouse = true;
    io.WantCaptureMouseUnlessPopupClose = true;

    EXPECT_FALSE(BML::ReleaseStaleImGuiMouseCapture(&otherOwner));
    EXPECT_EQ(g.HoveredWindow, scriptWindow);
    EXPECT_TRUE(io.WantCaptureMouse);

    EXPECT_TRUE(BML::ReleaseStaleImGuiMouseCapture(&scriptOwner));
    EXPECT_EQ(g.HoveredWindow, nullptr);
    EXPECT_EQ(g.HoveredWindowUnderMovingWindow, nullptr);
    EXPECT_EQ(g.HoveredWindowBeforeClear, nullptr);
    EXPECT_FALSE(io.WantCaptureMouse);
    EXPECT_FALSE(io.WantCaptureMouseUnlessPopupClose);

    ImGui::End();
    ImGui::Render();
}

TEST(ImGuiStateRecoveryTest, PreservesPopupAndNonMouseCaptureState) {
    ScopedImGuiContext context;
    int scriptOwner = 0;

    ImGui::NewFrame();
    BeginScriptWindow(&scriptOwner, "ScriptWindowWithPopup");
    ImGui::OpenPopup("ScriptTransientPopup");

    ImGuiContext &g = *ImGui::GetCurrentContext();
    ASSERT_GT(g.OpenPopupStack.Size, 0);
    g.WantCaptureKeyboardNextFrame = 1;
    g.WantTextInputNextFrame = 1;
    ImGuiIO &io = ImGui::GetIO();
    io.WantCaptureMouse = true;
    io.WantCaptureMouseUnlessPopupClose = true;
    io.WantCaptureKeyboard = true;
    io.WantTextInput = true;

    EXPECT_FALSE(BML::ReleaseStaleImGuiMouseCapture(&scriptOwner));
    EXPECT_GT(g.OpenPopupStack.Size, 0);
    EXPECT_EQ(g.BeginPopupStack.Size, 0);
    EXPECT_EQ(g.WantCaptureKeyboardNextFrame, 1);
    EXPECT_EQ(g.WantTextInputNextFrame, 1);
    EXPECT_TRUE(io.WantCaptureMouse);
    EXPECT_TRUE(io.WantCaptureMouseUnlessPopupClose);
    EXPECT_TRUE(io.WantCaptureKeyboard);
    EXPECT_TRUE(io.WantTextInput);

    ImGui::End();
    ImGui::Render();
}
