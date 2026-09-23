#include <BML/Command.hpp>
#include <BML/Bui.h>
#include <BML/DataShare.hpp>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/ModMenu.hpp>

#include "PlayerProbe.h"

#include <cstddef>
#include <cstring>
#include <string>

namespace {

constexpr char DataShareName[] = "bml.test.public-authoring";
constexpr char DataShareKey[] = "value";
constexpr int DataShareValue = 42;

struct DataShareState {
    bool callback = false;
    bool cleanup = false;
    int value = 0;
};

class PublicAuthoringPage final : public BML::ModMenu::Page {
public:
    PublicAuthoringPage()
        : Page("public-authoring", "Public API Page",
               "Exercises a Native Mod page through the public C++ facade") {}

    void Attach(ILogger *logger) noexcept { m_Logger = logger; }

protected:
    BML::ModMenu::PageAction OnFrame() override {
        if (m_Logger && !m_Drew) {
            m_Logger->Info("Public authoring page: draw");
            m_Drew = true;
        }
        Bui::Title("Public API Page");
        const float buttonWidth = Bui::GetButtonSize(Bui::BUTTON_MAIN).x;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        if (availableWidth > buttonWidth)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availableWidth - buttonWidth) * 0.5f);
        if (Bui::MainButton("Open child"))
            return BML::ModMenu::PageAction::Push("public-authoring-child");
        return BML::ModMenu::PageAction::None();
    }

    void OnEnter(BML::ModMenu::PageEnterReason reason) override {
        m_Drew = false;
        if (!m_Logger)
            return;

        const char *name = "push";
        if (reason == BML::ModMenu::PageEnterReason::Replace)
            name = "replace";
        else if (reason == BML::ModMenu::PageEnterReason::Back)
            name = "back";
        m_Logger->Info("Public authoring page: enter=%s", name);
    }

    void OnLeave(BML::ModMenu::PageLeaveReason reason) override {
        if (!m_Logger)
            return;

        const char *name = "close";
        if (reason == BML::ModMenu::PageLeaveReason::Back)
            name = "back";
        else if (reason == BML::ModMenu::PageLeaveReason::Push)
            name = "push";
        else if (reason == BML::ModMenu::PageLeaveReason::Replace)
            name = "replace";
        m_Logger->Info("Public authoring page: leave=%s", name);
    }

private:
    ILogger *m_Logger = nullptr;
    bool m_Drew = false;
};

class PublicAuthoringChildPage final : public BML::ModMenu::Page {
public:
    PublicAuthoringChildPage()
        : Page("public-authoring-child", "Child", "",
               BML::ModMenu::PageVisibility::Hidden) {}

protected:
    BML::ModMenu::PageAction OnFrame() override {
        Bui::Title("Public API Child");
        return BML::ModMenu::PageAction::None();
    }
};

class PublicAuthoringTest final : public IMod {
public:
    explicit PublicAuthoringTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "PublicAuthoringTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Public Authoring Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Validates the public Command, DataShare, and Mod Menu facades";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        using BML::PlayerTest::ProbeReport;
        ProbeReport::Reset();

        m_Page.Attach(GetLogger());
        const bool command = TestCommand();
        const bool dataShare = TestDataShare();
        const int pageStatus = m_Page.Register(GetID());
        const int childStatus = m_ChildPage.Register(GetID());
        const bool page = pageStatus == BML_OK && m_Page.IsRegistered() &&
                          childStatus == BML_OK && m_ChildPage.IsRegistered();
        const bool passed = command && dataShare && page;

        GetLogger()->Info(
            "Public authoring load: status=%s command=%s data_share=%s page=%s",
            passed ? "pass" : "fail", command ? "true" : "false",
            dataShare ? "true" : "false", page ? "true" : "false");
        if (passed)
            ProbeReport::Pass("completed");
        else
            ProbeReport::Fail("public-authoring");
    }

    void OnUnload() override {
        const int childStatus = m_ChildPage.Unregister();
        const int pageStatus = m_Page.Unregister();
        const int commandStatus = m_Command.Unregister();
        m_DataRequest.Reset();
        if (m_DataShare) {
            m_DataShare.Remove(DataShareKey);
            m_DataShare.Reset();
        }
        const bool passed = pageStatus == BML_OK && childStatus == BML_OK &&
                            commandStatus == BML_OK;
        GetLogger()->Info(
            "Public authoring unload: status=%s command=%d page=%d child=%d",
            passed ? "pass" : "fail", commandStatus, pageStatus, childStatus);
    }

private:
    static void BML_CDECL ReceiveData(const char *, const void *data,
                                      std::size_t size,
                                      void *userData) noexcept {
        auto *state = static_cast<DataShareState *>(userData);
        if (!state || !data || size != sizeof(int))
            return;
        std::memcpy(&state->value, data, sizeof(state->value));
        state->callback = true;
    }

    static void BML_CDECL CleanupData(const char *, void *userData) noexcept {
        auto *state = static_cast<DataShareState *>(userData);
        if (state)
            state->cleanup = true;
    }

    bool TestCommand() {
        BML::Command::Definition definition;
        definition.Name = "public-authoring";
        definition.Alias = "pap";
        definition.Description = "Public Native Mod authoring probe";
        definition.Usage = "public-authoring <value>";
        definition.Category = "test";

        const int registerStatus = m_Command.Register(
            definition,
            [this](const BML::Command::Invocation &invocation) {
                const std::string argument(invocation.Argument(0));
                GetLogger()->Info("Public authoring command: argument=%s",
                                  argument.c_str());
                const std::string output = "public-authoring:" + argument;
                const int writeStatus = invocation.Write(output);
                return writeStatus == BML_OK
                    ? BML_COMMAND_STATUS_SUCCESS
                    : BML_COMMAND_STATUS_FAILURE;
            },
            [](const BML::Command::CompletionRequest &,
               const BML::Command::Completion &completion) {
                for (const char *candidate : {"alpha", "beta", "gamma"}) {
                    const int status = completion.Add(candidate);
                    if (status != BML_OK)
                        return status;
                }
                return BML_OK;
            },
            GetID());
        if (registerStatus != BML_OK || !m_Command.IsRegistered())
            return false;

        BML::Command::Info info;
        const int findStatus = BML::Command::Find("pap", info);
        int commandStatus = BML_COMMAND_STATUS_FAILURE;
        const int executeStatus = BML::Command::ExecuteLine(
            "public-authoring direct", commandStatus);
        return findStatus == BML_OK && info.Handle == m_Command.Handle() &&
            info.Name == definition.Name && executeStatus == BML_OK &&
            commandStatus == BML_COMMAND_STATUS_SUCCESS;
    }

    bool TestDataShare() {
        m_DataShare = BML::DataShare(DataShareName);
        if (!m_DataShare)
            return false;

        m_DataShare.Remove(DataShareKey);
        m_DataState = {};
        m_DataRequest = m_DataShare.RequestForOwner(
            GetID(), DataShareKey, &ReceiveData, &m_DataState, &CleanupData);
        if (!m_DataRequest)
            return false;

        const bool stored = m_DataShare.Set(
            DataShareKey, &DataShareValue, sizeof(DataShareValue));
        int copied = 0;
        std::size_t fullSize = 0;
        const int copyStatus = m_DataShare.CopyEx(
            DataShareKey, &copied, sizeof(copied), &fullSize);
        const bool fired = m_DataState.callback && m_DataState.cleanup &&
            m_DataState.value == DataShareValue;
        m_DataRequest.Reset();
        return stored && fired && copyStatus == 1 &&
            fullSize == sizeof(copied) && copied == DataShareValue;
    }

    BML::Command::Registration m_Command;
    BML::DataShare m_DataShare;
    BML::DataShareRequest m_DataRequest;
    DataShareState m_DataState;
    PublicAuthoringPage m_Page;
    PublicAuthoringChildPage m_ChildPage;
};

} // namespace

BML_PLAYER_PROBE_READ_EXPORT()

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) {
    return new PublicAuthoringTest(bml);
}

BML_MOD_ENTRY(void) BMLExit(IMod *mod) {
    delete mod;
}
