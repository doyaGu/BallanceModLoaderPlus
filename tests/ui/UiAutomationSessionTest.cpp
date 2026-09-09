#include "UI/Automation/UiAutomationSession.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void Require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_Path = fs::temp_directory_path() / ("bml-ui-session-test-" + std::to_string(stamp));
        fs::create_directories(m_Path);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(m_Path, ignored);
    }

    const fs::path &Path() const { return m_Path; }

  private:
    fs::path m_Path;
};

} // namespace

int main() {
    try {
        TemporaryDirectory temporary;
        UiAutomationSession::PlayerEndpoint player(temporary.Path());
        UiAutomationSession::RunnerEndpoint runner(temporary.Path());

        Require(!runner.ReceiveNext(), "Runner must wait before the first checkpoint");
        Require(player.Publish(UiAutomationSession::CheckpointKind::Input, "input-main-to-options"),
                "Player could not publish the first checkpoint");
        Require(player.HasPendingCheckpoint() && player.PendingSequence() == 1,
                "Player did not retain the pending checkpoint");

        const auto first = runner.ReceiveNext();
        Require(first && first->Sequence == 1 &&
                    first->Kind == UiAutomationSession::CheckpointKind::Input &&
                    first->Name == "input-main-to-options",
                "Runner did not receive the first checkpoint");
        Require(player.PollAcknowledgement().State ==
                    UiAutomationSession::AcknowledgementState::Waiting,
                "Player must wait until the runner acknowledges");
        Require(runner.Acknowledge(*first, true), "Runner could not acknowledge input");
        Require(player.PollAcknowledgement().State ==
                    UiAutomationSession::AcknowledgementState::Succeeded,
                "Player did not observe the successful acknowledgement");
        Require(!player.HasPendingCheckpoint() && runner.NextSequence() == 2,
                "Endpoints did not advance together");

        Require(
            player.Publish(UiAutomationSession::CheckpointKind::Capture, "capture-native-options"),
            "Player could not publish the capture checkpoint");
        const auto second = runner.ReceiveNext();
        Require(second && second->Sequence == 2 &&
                    second->Kind == UiAutomationSession::CheckpointKind::Capture,
                "Runner did not receive the capture checkpoint");
        Require(runner.Acknowledge(*second, false, "window-obstructed"),
                "Runner could not publish a failed acknowledgement");
        const auto failed = player.PollAcknowledgement();
        Require(failed.State == UiAutomationSession::AcknowledgementState::Failed &&
                    failed.Reason == "window-obstructed",
                "Player did not preserve the failure reason");

        std::ofstream partial(temporary.Path() / "checkpoint-000003.msg.tmp",
                              std::ios::binary | std::ios::trunc);
        partial << "format=bml-ui-session-v1\nsequence=3\n";
        partial.close();
        Require(!runner.ReceiveNext(), "Runner consumed a partial checkpoint");
        Require(runner.LastError().empty(), "An unpublished message must be treated as waiting");
        Require(player.Publish(UiAutomationSession::CheckpointKind::Capture, "Bad Name") == false,
                "Player accepted an unsafe checkpoint name");

        std::cout << "Validated sequenced UI automation checkpoint acknowledgements\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
