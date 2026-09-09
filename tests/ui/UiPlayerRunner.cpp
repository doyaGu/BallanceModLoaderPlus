#include "UiAutomationProtocol.h"
#include "UiPlayerHarness.h"

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
        Require(Contains(run.ModLoaderLog, "On Message PostStartMenu"), "game-menu-entered",
                failures);
        Require(fs::is_regular_file(run.ResultPath), "result-created", failures);

        if (fs::is_regular_file(run.ResultPath)) {
            const auto result = UiTest::ReadScenarioResult(run.ResultPath);
            Require(result.ScenarioName == selected->Name, "result-scenario", failures);
            Require(result.TestName == selected->TestName, "result-test", failures);
            Require(result.Passed && result.Failures == 0, "scenario-passed", failures);
        }
        Require(Contains(run.ModLoaderLog, "UI automation: scenario=" + selected->Name +
                                               " selected=true test=" + selected->TestName),
                "scenario-selected", failures);
        Require(run.WindowActivated, "foreground-window", failures);
        Require(run.InjectedInputSequences == run.ExpectedInputSequences, "native-input-sequences",
                failures);

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

        Require(Contains(run.ModLoaderLog, "native_transition=main-to-options requested=true "
                                           "input=keyboard") &&
                    Contains(run.ModLoaderLog, "native_transition=options-to-imgui "
                                               "requested=true input=keyboard") &&
                    Contains(run.ModLoaderLog, "native_transition=imgui-to-options observed=true"),
                "native-imgui-round-trip", failures);
        if (selected->Input == UiTest::InputProfile::LevelOne) {
            Require(Contains(run.ModLoaderLog, "native_transition=options-to-main "
                                               "requested=true input=keyboard") &&
                        Contains(run.ModLoaderLog, "native_transition=main-to-start "
                                                   "requested=true input=keyboard") &&
                        Contains(run.ModLoaderLog, "native_transition=start-to-level-1 "
                                                   "requested=true input=keyboard") &&
                        Contains(run.ModLoaderLog, "native_transition=start-to-level-1 "
                                                   "observed=true") &&
                        Contains(run.ModLoaderLog, "gameplay_tutorial=dismiss requested=true "
                                                   "input=keyboard"),
                    "level-one-flow", failures);
        }

        const std::string surfaceMarker = "surface=" + selected->Surface +
                                          " native_options_visible=false native_main_visible=false "
                                          "native_start_visible=false layout_valid=true";
        Require(Contains(run.ModLoaderLog, surfaceMarker), "surface-without-native-menu-overlap",
                failures);
        for (const auto &marker : selected->RequiredLogs)
            Require(Contains(run.ModLoaderLog, marker), "required-log:" + marker, failures);

        Require(!Contains(run.PlayerLog, "Error : PreProcess") &&
                    !Contains(run.PlayerLog, "Error : PostProcess") &&
                    !Contains(run.PlayerLog, "Assertion failed") &&
                    !Contains(run.ModLoaderLog, "Assertion failed"),
                "no-imgui-or-frame-errors", failures);
        Require(!run.TimedOut && run.ExitCode == 0 && Contains(run.ModLoaderLog, "Goodbye!"),
                "clean-shutdown", failures);

        if (!failures.empty()) {
            std::cerr << "UI scenario '" << selected->Name << "' failed:";
            for (const auto &failure : failures)
                std::cerr << "\n  - " << failure;
            std::cerr << "\nTrace: " << run.TracePath << "\nPlayer trace: " << run.PlayerTracePath
                      << '\n';
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
