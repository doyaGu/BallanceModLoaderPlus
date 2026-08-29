#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "BML/Bui.h"

namespace Bui {

bool NavLeft(float, float) { return false; }
bool NavRight(float, float) { return false; }
bool NavBack(float, float) { return false; }
void Title(const char *, float, float, ImU32) {}

} // namespace Bui

namespace {

class ScopedImGuiContext {
public:
    ScopedImGuiContext() : m_Previous(ImGui::GetCurrentContext()) {
        m_Context = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_Context);
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800.0f, 600.0f);
        io.IniFilename = nullptr;
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

    void BeginFrame() const { ImGui::NewFrame(); }
    void EndFrame() const { ImGui::Render(); }

private:
    ImGuiContext *m_Previous = nullptr;
    ImGuiContext *m_Context = nullptr;
};

class TestMenu {
public:
    TestMenu()
        : m_Routes(
              [count = &OpenCount, hook = &OpenHook]() {
                  ++*count;
                  if (*hook)
                      (*hook)();
              },
              [count = &CloseCount, hook = &CloseHook]() {
                  ++*count;
                  if (*hook)
                      (*hook)();
              }) {}

    template <typename PageType, typename... Args>
    bool CreatePage(std::string id, Args &&... args) {
        return m_Routes.CreatePage<PageType>(std::move(id),
                                              std::forward<Args>(args)...);
    }

    bool HasPage(const std::string &id) const { return m_Routes.HasPage(id); }
    bool IsCurrentPage(const std::string &id) const { return m_Routes.IsCurrentPage(id); }
    bool IsOpen() const { return m_Routes.IsOpen(); }
    bool RemovePage(const std::string &id) { return m_Routes.RemovePage(id); }
    bool Open(const std::string &id) { return m_Routes.Open(id); }
    bool Push(const std::string &id) { return m_Routes.Push(id); }
    bool Replace(const std::string &id) { return m_Routes.Replace(id); }
    bool Back() { return m_Routes.Back(); }
    bool Close() { return m_Routes.Close(); }
    bool Render() { return m_Routes.Render(); }

    std::function<void()> OpenHook;
    std::function<void()> CloseHook;
    int OpenCount = 0;
    int CloseCount = 0;

private:
    // Declared last: routes and Pages are destroyed before hooks/counters.
    Bui::Menu m_Routes;
};

struct PageState {
    using EnterHandler = std::function<void(Bui::PageEnterReason)>;
    using FrameHandler = std::function<Bui::PageAction()>;
    using LeaveHandler = std::function<void(Bui::PageLeaveReason)>;

    std::vector<Bui::PageEnterReason> EnterReasons;
    std::vector<Bui::PageLeaveReason> LeaveReasons;
    EnterHandler Enter;
    FrameHandler Frame;
    LeaveHandler Leave;
    bool CallbackActive = false;
    bool DestroyedDuringCallback = false;
    int FrameCount = 0;
    int DestructionCount = 0;
};

class CallbackScope {
public:
    explicit CallbackScope(PageState &state) : m_State(state) {
        EXPECT_FALSE(m_State.CallbackActive);
        m_State.CallbackActive = true;
    }

    ~CallbackScope() { m_State.CallbackActive = false; }

private:
    PageState &m_State;
};

class ProbePage final : public Bui::Page {
public:
    explicit ProbePage(PageState &state) : m_State(state) {}

    ~ProbePage() override {
        if (m_State.CallbackActive)
            m_State.DestroyedDuringCallback = true;
        ++m_State.DestructionCount;
    }

    void OnEnter(Bui::PageEnterReason reason) override {
        CallbackScope scope(m_State);
        m_State.EnterReasons.push_back(reason);
        if (m_State.Enter)
            m_State.Enter(reason);
    }

    Bui::PageAction OnFrame() override {
        CallbackScope scope(m_State);
        ++m_State.FrameCount;
        return m_State.Frame ? m_State.Frame() : Bui::PageAction::None();
    }

    void OnLeave(Bui::PageLeaveReason reason) override {
        CallbackScope scope(m_State);
        m_State.LeaveReasons.push_back(reason);
        if (m_State.Leave)
            m_State.Leave(reason);
    }

private:
    PageState &m_State;
};

TEST(BuiPaginationTest, OwnsAndClampsOnlyListPosition) {
    Bui::Pagination pagination;

    pagination.Update(25, 10);
    EXPECT_EQ(pagination.GetPageCount(), 3);
    EXPECT_EQ(pagination.GetFirstItem(), 0);
    EXPECT_FALSE(pagination.CanPrevious());
    EXPECT_TRUE(pagination.CanNext());

    EXPECT_TRUE(pagination.SetPage(2));
    EXPECT_EQ(pagination.GetFirstItem(), 20);
    EXPECT_TRUE(pagination.CanPrevious());
    EXPECT_FALSE(pagination.CanNext());

    pagination.Update(3, 10);
    EXPECT_EQ(pagination.GetPage(), 0);
    EXPECT_EQ(pagination.GetPageCount(), 1);
    EXPECT_FALSE(pagination.SetPage(99));

    pagination.Update(3, 0);
    EXPECT_EQ(pagination.GetPage(), 0);
    EXPECT_EQ(pagination.GetPageCount(), 0);
    EXPECT_FALSE(pagination.Previous());
    EXPECT_FALSE(pagination.Next());
}

TEST(BuiMenuTest, NavigationPairsLifecycleAndKeepsOneSession) {
    PageState first;
    PageState second;
    TestMenu menu;
    ASSERT_TRUE(menu.CreatePage<ProbePage>("first", first));
    ASSERT_TRUE(menu.CreatePage<ProbePage>("second", second));

    ASSERT_TRUE(menu.Open("first"));
    EXPECT_TRUE(menu.IsOpen());
    EXPECT_TRUE(menu.IsCurrentPage("first"));
    EXPECT_EQ(menu.OpenCount, 1);

    ASSERT_TRUE(menu.Push("second"));
    EXPECT_TRUE(menu.IsCurrentPage("second"));
    EXPECT_EQ(menu.OpenCount, 1);

    ASSERT_TRUE(menu.Back());
    EXPECT_TRUE(menu.IsCurrentPage("first"));
    EXPECT_EQ(menu.CloseCount, 0);

    ASSERT_TRUE(menu.Close());
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_EQ(menu.CloseCount, 1);

    EXPECT_EQ(first.EnterReasons,
              (std::vector<Bui::PageEnterReason>{Bui::PageEnterReason::Open,
                                                  Bui::PageEnterReason::Back}));
    EXPECT_EQ(first.LeaveReasons,
              (std::vector<Bui::PageLeaveReason>{Bui::PageLeaveReason::Push,
                                                  Bui::PageLeaveReason::Close}));
    EXPECT_EQ(second.EnterReasons,
              (std::vector<Bui::PageEnterReason>{Bui::PageEnterReason::Push}));
    EXPECT_EQ(second.LeaveReasons,
              (std::vector<Bui::PageLeaveReason>{Bui::PageLeaveReason::Back}));
}

TEST(BuiMenuTest, DestroyingAnOpenControllerClosesBeforeDestroyingPages) {
    PageState state;
    int closeCount = 0;

    {
        TestMenu menu;
        menu.CloseHook = [&] {
            ++closeCount;
            EXPECT_EQ(state.LeaveReasons,
                      (std::vector<Bui::PageLeaveReason>{Bui::PageLeaveReason::Close}));
            EXPECT_EQ(state.DestructionCount, 0);
        };

        ASSERT_TRUE(menu.CreatePage<ProbePage>("page", state));
        ASSERT_TRUE(menu.Open("page"));
    }

    EXPECT_EQ(closeCount, 1);
    EXPECT_EQ(state.DestructionCount, 1);
    EXPECT_FALSE(state.DestroyedDuringCallback);
}

TEST(BuiMenuTest, DestructionContinuesWhenLeaveAndCloseCallbacksThrow) {
    PageState state;
    int closeCount = 0;

    state.Leave = [](Bui::PageLeaveReason reason) {
        EXPECT_EQ(reason, Bui::PageLeaveReason::Close);
        throw std::runtime_error("leave failed");
    };

    {
        TestMenu menu;
        menu.CloseHook = [&] {
            ++closeCount;
            EXPECT_EQ(state.DestructionCount, 0);
            throw std::runtime_error("close failed");
        };

        ASSERT_TRUE(menu.CreatePage<ProbePage>("page", state));
        ASSERT_TRUE(menu.Open("page"));
    }

    EXPECT_EQ(closeCount, 1);
    EXPECT_EQ(state.LeaveReasons,
              (std::vector<Bui::PageLeaveReason>{Bui::PageLeaveReason::Close}));
    EXPECT_EQ(state.DestructionCount, 1);
    EXPECT_FALSE(state.DestroyedDuringCallback);
}

TEST(BuiMenuTest, InvalidOperationsReportFailureWithoutChangingRoute) {
    PageState current;
    TestMenu menu;
    ASSERT_TRUE(menu.CreatePage<ProbePage>("current", current));
    EXPECT_FALSE(menu.CreatePage<ProbePage>("current", current));
    EXPECT_FALSE(menu.Open("missing"));
    EXPECT_FALSE(menu.RemovePage("missing"));
    EXPECT_FALSE(menu.Back());
    EXPECT_EQ(menu.OpenCount, 0);

    ASSERT_TRUE(menu.Open("current"));
    EXPECT_FALSE(menu.Push("missing"));
    EXPECT_FALSE(menu.Replace("missing"));
    EXPECT_TRUE(menu.IsCurrentPage("current"));
    EXPECT_TRUE(current.LeaveReasons.empty());
}

TEST(BuiMenuTest, DirectMutationIsRejectedInsidePageCallbacks) {
    PageState current;
    PageState other;
    TestMenu menu;
    current.Enter = [&](Bui::PageEnterReason) {
        EXPECT_FALSE(menu.RemovePage("current"));
        EXPECT_FALSE(menu.Open("other"));
        EXPECT_FALSE(menu.Close());
    };
    ASSERT_TRUE(menu.CreatePage<ProbePage>("current", current));
    ASSERT_TRUE(menu.CreatePage<ProbePage>("other", other));

    ASSERT_TRUE(menu.Open("current"));
    EXPECT_TRUE(menu.HasPage("current"));
    EXPECT_TRUE(menu.IsCurrentPage("current"));
    EXPECT_TRUE(other.EnterReasons.empty());
}

TEST(BuiMenuTest, SessionCallbacksCannotReenterTheRouteController) {
    PageState current;
    PageState other;
    TestMenu menu;
    ASSERT_TRUE(menu.CreatePage<ProbePage>("current", current));
    ASSERT_TRUE(menu.CreatePage<ProbePage>("other", other));

    menu.OpenHook = [&] {
        EXPECT_FALSE(menu.RemovePage("current"));
        EXPECT_FALSE(menu.Push("other"));
    };
    ASSERT_TRUE(menu.Open("current"));
    EXPECT_TRUE(menu.IsCurrentPage("current"));

    menu.CloseHook = [&] {
        EXPECT_FALSE(menu.Open("other"));
    };
    ASSERT_TRUE(menu.Close());
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_TRUE(other.EnterReasons.empty());
}

TEST(BuiMenuTest, RemovingCurrentPageLeavesBeforeDestroyingIt) {
    PageState state;
    TestMenu menu;
    state.Leave = [&](Bui::PageLeaveReason reason) {
        EXPECT_EQ(reason, Bui::PageLeaveReason::Remove);
        EXPECT_EQ(state.DestructionCount, 0);
        EXPECT_FALSE(menu.RemovePage("self"));
    };
    ASSERT_TRUE(menu.CreatePage<ProbePage>("self", state));
    ASSERT_TRUE(menu.Open("self"));

    EXPECT_TRUE(menu.RemovePage("self"));
    EXPECT_FALSE(menu.HasPage("self"));
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_FALSE(state.DestroyedDuringCallback);
    EXPECT_EQ(state.DestructionCount, 1);
    EXPECT_EQ(state.LeaveReasons,
              (std::vector<Bui::PageLeaveReason>{Bui::PageLeaveReason::Remove}));
    EXPECT_EQ(menu.OpenCount, 1);
    EXPECT_EQ(menu.CloseCount, 1);
}

TEST(BuiMenuTest, FrameActionRunsAfterDrawingAndCanRemoveItsPage) {
    ScopedImGuiContext imgui;
    PageState state;
    TestMenu menu;
    state.Frame = [] { return Bui::PageAction::RemoveSelf(); };
    ASSERT_TRUE(menu.CreatePage<ProbePage>("self", state));
    ASSERT_TRUE(menu.Open("self"));
    ASSERT_EQ(menu.OpenCount, 1);

    imgui.BeginFrame();
    EXPECT_TRUE(menu.Render());
    imgui.EndFrame();

    EXPECT_FALSE(menu.HasPage("self"));
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_FALSE(state.DestroyedDuringCallback);
    EXPECT_EQ(state.DestructionCount, 1);
    EXPECT_EQ(state.FrameCount, 1);
    EXPECT_EQ(state.LeaveReasons,
              (std::vector<Bui::PageLeaveReason>{Bui::PageLeaveReason::Remove}));
    EXPECT_EQ(menu.CloseCount, 1);
}

TEST(BuiMenuTest, RemovingAHistoryPageCannotLeaveADanglingRoute) {
    PageState first;
    PageState second;
    TestMenu menu;
    ASSERT_TRUE(menu.CreatePage<ProbePage>("first", first));
    ASSERT_TRUE(menu.CreatePage<ProbePage>("second", second));
    ASSERT_TRUE(menu.Open("first"));
    ASSERT_TRUE(menu.Push("second"));

    ASSERT_TRUE(menu.RemovePage("first"));
    EXPECT_FALSE(menu.HasPage("first"));
    EXPECT_TRUE(menu.IsCurrentPage("second"));

    ASSERT_TRUE(menu.Back());
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_EQ(second.LeaveReasons,
              (std::vector<Bui::PageLeaveReason>{Bui::PageLeaveReason::Back}));
    EXPECT_EQ(menu.CloseCount, 1);
}

TEST(BuiMenuTest, HistoryLimitRejectsNavigationWithoutLeavingCurrentPage) {
    PageState state;
    TestMenu menu;
    ASSERT_TRUE(menu.CreatePage<ProbePage>("page", state));
    ASSERT_TRUE(menu.Open("page"));

    for (size_t i = 0; i < 32; ++i)
        ASSERT_TRUE(menu.Push("page"));

    const size_t enterCount = state.EnterReasons.size();
    const size_t leaveCount = state.LeaveReasons.size();
    EXPECT_FALSE(menu.Push("page"));
    EXPECT_TRUE(menu.IsCurrentPage("page"));
    EXPECT_EQ(state.EnterReasons.size(), enterCount);
    EXPECT_EQ(state.LeaveReasons.size(), leaveCount);
}

TEST(BuiMenuTest, LifecycleExceptionsLeaveACoherentRouteState) {
    PageState state;
    TestMenu menu;
    state.Enter = [](Bui::PageEnterReason) {
        throw std::runtime_error("enter failed");
    };
    ASSERT_TRUE(menu.CreatePage<ProbePage>("page", state));

    EXPECT_THROW(menu.Open("page"), std::runtime_error);
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_EQ(menu.OpenCount, 0);

    state.Enter = {};
    ASSERT_TRUE(menu.Open("page"));
    state.Leave = [](Bui::PageLeaveReason) {
        throw std::runtime_error("leave failed");
    };

    EXPECT_THROW(menu.Close(), std::runtime_error);
    EXPECT_TRUE(menu.IsCurrentPage("page"));
    EXPECT_EQ(menu.CloseCount, 0);

    state.Leave = {};
    EXPECT_TRUE(menu.Close());
    EXPECT_FALSE(menu.IsOpen());
    EXPECT_EQ(menu.CloseCount, 1);
}

TEST(BuiMenuTest, RenderReportsAnInvalidPageActionWithoutChangingRoute) {
    ScopedImGuiContext imgui;
    PageState state;
    TestMenu menu;
    state.Frame = [] { return Bui::PageAction::Push("missing"); };
    ASSERT_TRUE(menu.CreatePage<ProbePage>("current", state));
    ASSERT_TRUE(menu.Open("current"));

    imgui.BeginFrame();
    EXPECT_FALSE(menu.Render());
    imgui.EndFrame();

    EXPECT_TRUE(menu.IsCurrentPage("current"));
    EXPECT_TRUE(state.LeaveReasons.empty());
}

TEST(BuiMenuTest, RenderRestoresImGuiWindowScopeWhenPageThrows) {
    ScopedImGuiContext imgui;
    PageState state;
    TestMenu menu;
    state.Frame = []() -> Bui::PageAction {
        throw std::runtime_error("draw failed");
    };
    ASSERT_TRUE(menu.CreatePage<ProbePage>("throwing", state));
    ASSERT_TRUE(menu.Open("throwing"));

    imgui.BeginFrame();
    EXPECT_THROW(menu.Render(), std::runtime_error);
    EXPECT_NO_THROW(imgui.EndFrame());
    EXPECT_TRUE(menu.IsCurrentPage("throwing"));
}

} // namespace
