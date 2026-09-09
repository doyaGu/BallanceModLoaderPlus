#include "UiAutomationProtocol.h"
#include "UiPlayerHarness.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void Require(bool condition, const std::string &message, std::vector<std::string> &failures) {
    if (!condition)
        failures.push_back(message);
}

std::map<std::string, std::string> ParseArguments(int argc, char **argv) {
    std::map<std::string, std::string> arguments;
    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc || std::string(argv[index]).rfind("--", 0) != 0)
            throw std::runtime_error("Arguments must be --name value pairs");
        arguments[std::string(argv[index]).substr(2)] = argv[index + 1];
    }
    return arguments;
}

std::string GetRequired(const std::map<std::string, std::string> &arguments,
                        const std::string &name) {
    const auto found = arguments.find(name);
    if (found == arguments.end() || found->second.empty())
        throw std::runtime_error("Missing --" + name);
    return found->second;
}

bool Contains(const std::string &text, const std::string &marker) {
    return text.find(marker) != std::string::npos;
}

bool HasCheckpoint(const UiTest::PlayerRunResult &run, UiAutomationSession::CheckpointKind kind,
                   const std::string &name) {
    return std::any_of(run.HandledCheckpoints.begin(), run.HandledCheckpoints.end(),
                       [&](const UiAutomationSession::Checkpoint &checkpoint) {
                           return checkpoint.Kind == kind && checkpoint.Name == name;
                       });
}

bool HasContinuousCheckpointSequence(const UiTest::PlayerRunResult &run) {
    for (std::size_t index = 0; index < run.HandledCheckpoints.size(); ++index) {
        if (run.HandledCheckpoints[index].Sequence != index + 1)
            return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    try {
        const auto arguments = ParseArguments(argc, argv);
        const fs::path scenarioDirectory = GetRequired(arguments, "scenario-dir");
        const fs::path sourceRoot = GetRequired(arguments, "source-root");
        const std::string scenarioName = GetRequired(arguments, "scenario");
        const auto catalog = UiTest::LoadScenarioCatalog(scenarioDirectory, sourceRoot);
        const auto selected =
            std::find_if(catalog.begin(), catalog.end(),
                         [&](const auto &scenario) { return scenario.Name == scenarioName; });
        if (selected == catalog.end())
            throw std::runtime_error("Unknown UI scenario: " + scenarioName);

        UiTest::PlayerRunRequest request;
        request.BallanceRoot = GetRequired(arguments, "ballance-root");
        request.BuildDll = GetRequired(arguments, "build-dll");
        request.ArtifactsDirectory = GetRequired(arguments, "artifacts");
        request.SelectedScenario = *selected;
        const auto run = UiTest::RunPlayerScenario(request);

        std::vector<std::string> failures;
        Require(run.InstallRestored, "install-restored", failures);
        Require(
            HasCheckpoint(run, UiAutomationSession::CheckpointKind::Capture, "capture-game-menu"),
            "game-menu-entered", failures);
        Require(fs::is_regular_file(run.ResultPath), "result-created", failures);

        if (fs::is_regular_file(run.ResultPath)) {
            const auto result = UiTest::ReadScenarioResult(run.ResultPath);
            Require(result.ScenarioName == selected->Name, "result-scenario", failures);
            Require(result.TestName == selected->TestName, "result-test", failures);
            Require(result.Passed && result.Failures == 0, "scenario-passed", failures);
        }
        Require(run.WindowActivated, "foreground-window", failures);
        Require(run.InjectedInputSequences == run.ExpectedInputSequences, "native-input-sequences",
                failures);
        Require(run.SessionFailures.empty(), "session-acknowledgements", failures);
        Require(HasContinuousCheckpointSequence(run), "session-checkpoint-sequence", failures);
        Require(run.HandledCheckpoints.size() ==
                    static_cast<std::size_t>(run.ExpectedInputSequences + 3),
                "session-checkpoint-count", failures);

        for (const std::string name :
             {std::string("GameMenu"), std::string("NativeOptions"), selected->CaptureName}) {
            const auto capture = run.Captures.find(name);
            Require(capture != run.Captures.end() && capture->second.Captured, "capture-" + name,
                    failures);
            if (capture != run.Captures.end() && capture->second.Captured) {
                Require(capture->second.Width == 800 && capture->second.Height == 600,
                        "capture-resolution-" + name, failures);
            }
        }

        Require(HasCheckpoint(run, UiAutomationSession::CheckpointKind::Input,
                              "input-main-to-options") &&
                    HasCheckpoint(run, UiAutomationSession::CheckpointKind::Input,
                                  "input-options-to-imgui") &&
                    HasCheckpoint(run, UiAutomationSession::CheckpointKind::Capture,
                                  "capture-native-options"),
                "native-imgui-round-trip", failures);
        if (selected->Input != UiTest::InputProfile::ModList) {
            Require(HasCheckpoint(run, UiAutomationSession::CheckpointKind::Input,
                                  "input-options-to-main") &&
                        HasCheckpoint(run, UiAutomationSession::CheckpointKind::Input,
                                      "input-main-to-start") &&
                        HasCheckpoint(run, UiAutomationSession::CheckpointKind::Input,
                                      "input-dismiss-tutorial"),
                    "level-entry-flow", failures);
        }
        if (selected->Input == UiTest::InputProfile::LevelOne) {
            Require(HasCheckpoint(run, UiAutomationSession::CheckpointKind::Input,
                                  "input-start-to-level-1"),
                    "native-level-selection", failures);
        }

        Require(HasCheckpoint(run, UiAutomationSession::CheckpointKind::Capture,
                              "capture-" + selected->Surface),
                "surface-without-native-menu-overlap", failures);
        for (const auto &marker : selected->RequiredLogs)
            Require(Contains(run.ModLoaderLog, marker), "required-log:" + marker, failures);

        Require(!Contains(run.PlayerLog, "Error : PreProcess") &&
                    !Contains(run.PlayerLog, "Error : PostProcess") &&
                    !Contains(run.PlayerLog, "Assertion failed") &&
                    !Contains(run.ModLoaderLog, "Assertion failed"),
                "no-imgui-or-frame-errors", failures);
        Require(!run.TimedOut && run.ExitCode == 0, "clean-shutdown", failures);

        if (!failures.empty()) {
            std::cerr << "UI scenario '" << selected->Name << "' failed:";
            for (const auto &failure : failures)
                std::cerr << "\n  - " << failure;
            std::cerr << "\nTrace: " << run.TracePath << "\nPlayer trace: " << run.PlayerTracePath
                      << "\nSession: " << run.SessionDirectory << '\n';
            return 1;
        }
        std::cout << "UI scenario '" << selected->Name
                  << "' passed with real foreground input at 800x600\n"
                  << "Artifacts: " << fs::absolute(request.ArtifactsDirectory) << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
