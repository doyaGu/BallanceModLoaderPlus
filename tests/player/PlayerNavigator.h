#pragma once

#include "CKAll.h"

#include <chrono>
#include <cstdint>
#include <vector>

class IBML;
class ILogger;
class InputHook;

namespace BML::PlayerTest {

// Drives the shipped Ballance menu graph from the main menu into Level 01 and
// observes the tutorial listener the runner's key injection has to satisfy.
// Both Player acceptance Mods share this, so the log lines the PowerShell
// runners match live in one place.
class PlayerNavigator {
public:
    enum class Step {
        // The shipped graph is not ready yet. Try again next frame.
        Pending,
        Done,
        // The expected authored path is missing. Error() says which one.
        Failed,
    };

    void Attach(IBML *bml, ILogger *logger);

    void OnPostStartMenu();
    void OnStartLevel();
    void OnControlReady();

    // Runs once per frame before the owning Mod advances its own phase.
    void Observe(InputHook *input);

    Step OpenLevelMenu();
    Step ChooseLevel();

    [[nodiscard]] const char *Error() const { return m_Error; }
    [[nodiscard]] bool MenuReady() const { return m_MenuReady; }
    [[nodiscard]] bool MenuOpened() const { return m_MenuOpened; }
    [[nodiscard]] bool LevelChosen() const { return m_LevelChosen; }
    [[nodiscard]] bool LevelStarted() const { return m_LevelStarted; }
    [[nodiscard]] bool ControlReady() const { return m_ControlReady; }
    [[nodiscard]] bool TutorialExitDeclared() const {
        return m_TutorialExitDeclared;
    }
    [[nodiscard]] bool TutorialExitReadyObserved() const {
        return m_TutorialExitReadyObserved;
    }
    [[nodiscard]] bool TutorialExitInputObserved() const {
        return m_TutorialExitInputObserved;
    }
    [[nodiscard]] bool TutorialExited() const { return m_TutorialExited; }
    [[nodiscard]] bool TutorialExitedByInput() const {
        return m_TutorialExitedByInput;
    }
    // Set once the tutorial prompt is on screen, so the owning Mod can save a
    // frame that proves the prompt was visible.
    [[nodiscard]] bool TutorialFrameRequested() const {
        return m_TutorialFrameRequested;
    }

    [[nodiscard]] std::chrono::steady_clock::time_point MenuStartedAt() const {
        return m_MenuStartedAt;
    }
    [[nodiscard]] std::chrono::steady_clock::time_point LevelStartedAt() const {
        return m_LevelStartedAt;
    }
    [[nodiscard]] std::chrono::steady_clock::time_point ControlReadyAt() const {
        return m_ControlReadyAt;
    }

private:
    static constexpr int kMenuDelayFrames = 5;
    static constexpr auto kTutorialInputResponseTime =
        std::chrono::milliseconds(1500);

    static void CollectBehaviorsByName(CKBehavior *root, const char *name,
                                       std::vector<CKBehavior *> &found);

    void DiscoverTutorialKeys();
    void ObserveTutorialInput(InputHook *input);
    void ObserveTutorialState();

    IBML *m_BML = nullptr;
    ILogger *m_Logger = nullptr;
    const char *m_Error = "menu-timeout";
    int m_MenuFrames = 0;
    bool m_MenuReady = false;
    bool m_MenuOpened = false;
    bool m_LevelChosen = false;
    bool m_LevelStarted = false;
    bool m_ControlReady = false;
    bool m_TutorialKeysInspected = false;
    bool m_TutorialExitDeclared = false;
    CKKEYBOARD m_TutorialExitKey = static_cast<CKKEYBOARD>(0);
    CKBehavior *m_TutorialRoot = nullptr;
    CKBehavior *m_TutorialChapter = nullptr;
    CKBehavior *m_TutorialAction = nullptr;
    CKBehavior *m_TutorialWait = nullptr;
    CKBehavior *m_TutorialText = nullptr;
    CKBehavior *m_TutorialTextBlock = nullptr;
    CKBehavior *m_TutorialContinueBlock = nullptr;
    CKBehavior *m_TutorialExitBlock = nullptr;
    std::vector<CKBehavior *> m_TutorialExitEvents;
    bool m_TutorialStateObserved = false;
    std::uint32_t m_TutorialState = 0;
    bool m_TutorialExitReady = false;
    bool m_TutorialPromptActive = false;
    bool m_TutorialFrameRequested = false;
    bool m_TutorialExitReadyObserved = false;
    bool m_TutorialExitInputObserved = false;
    bool m_TutorialExited = false;
    bool m_TutorialExitedByInput = false;
    std::chrono::steady_clock::time_point m_MenuStartedAt{};
    std::chrono::steady_clock::time_point m_LevelStartedAt{};
    std::chrono::steady_clock::time_point m_ControlReadyAt{};
    std::chrono::steady_clock::time_point m_TutorialExitInputAt{};
    std::chrono::steady_clock::time_point m_TutorialExitedAt{};
};

} // namespace BML::PlayerTest
