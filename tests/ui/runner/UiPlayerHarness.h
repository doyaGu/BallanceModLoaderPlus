#ifndef BML_TESTS_UI_PLAYER_HARNESS_H
#define BML_TESTS_UI_PLAYER_HARNESS_H

#include "session/UiAutomationSession.h"
#include "UiAutomationProtocol.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace UiTest {

struct CaptureResult {
    std::filesystem::path Path;
    bool Captured = false;
    int Width = 0;
    int Height = 0;
};

struct PlayerRunRequest {
    std::filesystem::path BallanceRoot;
    std::filesystem::path BuildDll;
    std::filesystem::path PublicAuthoringMod;
    std::filesystem::path ArtifactsDirectory;
    Scenario SelectedScenario;
    int Width = 800;
    int Height = 600;
    int TimeoutSeconds = 180;
};

struct PlayerRunResult {
    int ExitCode = -1;
    bool TimedOut = false;
    bool WindowActivated = false;
    bool InstallRestored = false;
    int InjectedInputSequences = 0;
    int ExpectedInputSequences = 0;
    std::string ModLoaderLog;
    std::string PlayerLog;
    std::filesystem::path ResultPath;
    std::filesystem::path TracePath;
    std::filesystem::path PlayerTracePath;
    std::filesystem::path SessionDirectory;
    std::map<std::string, CaptureResult> Captures;
    std::vector<UiAutomationSession::Checkpoint> HandledCheckpoints;
    std::vector<std::string> SessionFailures;
};

PlayerRunResult RunPlayerScenario(const PlayerRunRequest &request);

} // namespace UiTest

#endif
