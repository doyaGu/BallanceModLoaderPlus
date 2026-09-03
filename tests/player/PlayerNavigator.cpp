#include "PlayerNavigator.h"

#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/InputHook.h>
#include <BML/ScriptHelper.h>

#include <cstring>
#include <utility>

namespace BML::PlayerTest {

void PlayerNavigator::Attach(IBML *bml, ILogger *logger) {
    m_BML = bml;
    m_Logger = logger;
}

void PlayerNavigator::OnPostStartMenu() {
    m_MenuReady = true;
    m_MenuStartedAt = std::chrono::steady_clock::now();
}

void PlayerNavigator::OnStartLevel() {
    m_LevelStarted = true;
    m_LevelStartedAt = std::chrono::steady_clock::now();
}

void PlayerNavigator::OnControlReady() {
    if (m_ControlReady)
        return;
    m_ControlReady = true;
    m_ControlReadyAt = std::chrono::steady_clock::now();
}

void PlayerNavigator::Observe(InputHook *input) {
    if (!m_LevelStarted)
        return;
    DiscoverTutorialKeys();
    ObserveTutorialInput(input);
    ObserveTutorialState();
}

PlayerNavigator::Step PlayerNavigator::OpenLevelMenu() {
    if (!m_MenuReady)
        return Step::Pending;
    if (++m_MenuFrames < kMenuDelayFrames)
        return Step::Pending;

    CKBehavior *menuMain = m_BML->GetScriptByName("Menu_Main");
    if (!menuMain) {
        m_Error = "menu-main-not-ready";
        return Step::Pending;
    }

    CKBehavior *start = ScriptHelper::FindFirstBB(
        menuMain, "Start", false, 1, 1);
    if (!start) {
        m_Error = "menu-start-path-not-found";
        return Step::Failed;
    }

    // Menu.nmo: Main Menu.Button 1 pressed -> Menu_Main/Start.In 0.
    // Activate the click's proven downstream seam and leave the original
    // Activate Script graph to open Menu_Start.
    start->ActivateInput(0);
    start->Activate();
    m_MenuOpened = true;
    m_Logger->Info("Player menu: opened=true path=Menu_Main/Start.In0");
    return Step::Done;
}

PlayerNavigator::Step PlayerNavigator::ChooseLevel() {
    CKBehavior *menuScript = m_BML->GetScriptByName("Menu_Start");
    CK2dEntity *levelButton = m_BML->Get2dEntityByName("M_Start_But_01");
    if (!menuScript || !levelButton || !levelButton->IsVisible()) {
        m_Error = "level-menu-not-ready";
        return Step::Pending;
    }

    CKBehavior *levelMenu = ScriptHelper::FindFirstBB(
        menuScript, "Start Menu", false, 1, 2);
    if (!levelMenu) {
        m_Error = "level-menu-path-not-found";
        return Step::Failed;
    }

    CKBehavior *buttonBehavior = nullptr;
    for (int i = 0; i < levelMenu->GetSubBehaviorCount(); ++i) {
        CKBehavior *candidate = levelMenu->GetSubBehavior(i);
        if (!candidate || !candidate->GetName() ||
            std::strcmp(candidate->GetName(), "TT PushButton2") != 0 ||
            candidate->GetOutputCount() <= 2 ||
            !candidate->GetTargetParameter()) {
            continue;
        }
        CKParameter *targetSource =
            candidate->GetTargetParameter()->GetRealSource();
        if (targetSource && targetSource->GetValueObject() == levelButton) {
            buttonBehavior = candidate;
            break;
        }
    }
    if (!buttonBehavior) {
        m_Error = "level-button-path-not-found";
        return Step::Failed;
    }

    CKBehaviorLink *mouseDown = ScriptHelper::FindNextLink(
        levelMenu, buttonBehavior, "Parameter Selector", 2);
    CKBehaviorIO *selectorInput =
        mouseDown ? mouseDown->GetOutBehaviorIO() : nullptr;
    CKBehavior *selector = selectorInput ? selectorInput->GetOwner() : nullptr;
    int selectorInputIndex = -1;
    if (selector) {
        for (int i = 0; i < selector->GetInputCount(); ++i) {
            if (selector->GetInput(i) == selectorInput) {
                selectorInputIndex = i;
                break;
            }
        }
    }
    if (!selector || selectorInputIndex != 0) {
        m_Error = "level-button-link-mismatch";
        return Step::Failed;
    }

    // Menu.nmo: M_Start_But_01.Mouse Down -> Parameter Selector.In 0.
    // Deliver exactly that link's effect; the selector and every following
    // message/test/load block remain the shipped graph's responsibility.
    selector->ActivateInput(selectorInputIndex);
    selector->Activate();
    m_LevelChosen = true;
    m_Logger->Info(
        "Player menu: level=1 path=Start_Menu/Parameter_Selector.In0");
    return Step::Done;
}

void PlayerNavigator::DiscoverTutorialKeys() {
    if (m_TutorialKeysInspected)
        return;
    m_TutorialRoot = m_BML->GetScriptByName("Gameplay_Tutorial");
    m_TutorialAction = m_TutorialRoot ? ScriptHelper::FindFirstBB(
        m_TutorialRoot, "Tut continue/exit", true) : nullptr;
    if (!m_TutorialAction)
        return;

    m_TutorialKeysInspected = true;
    std::vector<std::pair<CKBehavior *, CKKEYBOARD>> keys;
    for (int index = 0; index < m_TutorialAction->GetSubBehaviorCount();
         ++index) {
        CKBehavior *child = m_TutorialAction->GetSubBehavior(index);
        if (!child || !child->GetName() ||
            std::strcmp(child->GetName(), "Key") != 0 ||
            child->GetInputParameterCount() < 1) {
            continue;
        }
        CKParameterIn *parameter = child->GetInputParameter(0);
        CKParameter *source = parameter ? parameter->GetRealSource() : nullptr;
        CKKEYBOARD key = static_cast<CKKEYBOARD>(0);
        if (source && source->GetGUID() == CKPGUID_KEY &&
            source->GetValue(&key) == CK_OK) {
            keys.emplace_back(child, key);
        }
    }
    if (keys.size() >= 2) {
        m_TutorialContinueBlock = keys[0].first;
        m_TutorialExitBlock = keys[1].first;
        m_TutorialExitKey = keys[1].second;
        m_TutorialExitDeclared =
            m_TutorialExitKey != static_cast<CKKEYBOARD>(0);
        CollectBehaviorsByName(
            m_TutorialExitBlock, "Key Event", m_TutorialExitEvents);
    }
    m_TutorialWait = ScriptHelper::FindFirstBB(
        m_TutorialAction, "wait for continue", true);
    m_TutorialChapter = m_TutorialAction->GetParent();
    m_TutorialText = ScriptHelper::FindFirstBB(
        m_TutorialRoot, "Tutorial Text", true);
    m_TutorialTextBlock = m_TutorialText ? ScriptHelper::FindFirstBB(
        m_TutorialText, "2D Text", true) : nullptr;
    m_Logger->Info(
        "Gameplay tutorial keys: count=%d exit_declared=%s "
        "continue=%u exit=%u",
        static_cast<int>(keys.size()),
        m_TutorialExitDeclared ? "true" : "false",
        keys.empty() ? 0u : static_cast<unsigned>(keys[0].second),
        keys.size() < 2 ? 0u : static_cast<unsigned>(keys[1].second));
}

void PlayerNavigator::CollectBehaviorsByName(
    CKBehavior *root, const char *name, std::vector<CKBehavior *> &found) {
    if (!root || !name)
        return;
    for (int index = 0; index < root->GetSubBehaviorCount(); ++index) {
        CKBehavior *child = root->GetSubBehavior(index);
        if (!child)
            continue;
        if (child->GetName() && std::strcmp(child->GetName(), name) == 0)
            found.push_back(child);
        CollectBehaviorsByName(child, name, found);
    }
}

void PlayerNavigator::ObserveTutorialInput(InputHook *input) {
    if (!input)
        return;
    // Observe the input against the listener state from the previous game
    // frame. The shipped graph may consume the exit key and deactivate the
    // listener before the owning Mod's OnProcess callback runs.
    if (!m_TutorialExitReady || m_TutorialExitInputObserved ||
        m_TutorialExitKey == static_cast<CKKEYBOARD>(0) ||
        !input->oIsKeyDown(m_TutorialExitKey)) {
        return;
    }
    m_TutorialExitInputObserved = true;
    m_TutorialExitInputAt = std::chrono::steady_clock::now();
    m_Logger->Info("Gameplay input: tutorial_exit=true key=%u",
                   static_cast<unsigned>(m_TutorialExitKey));
}

void PlayerNavigator::ObserveTutorialState() {
    if (!m_TutorialKeysInspected || !m_TutorialRoot ||
        !m_TutorialAction || !m_TutorialExitBlock)
        return;

    const bool rootActive = m_TutorialRoot->IsActive() != FALSE;
    const bool chapterActive = m_TutorialChapter &&
        m_TutorialChapter->IsActive() != FALSE;
    const bool actionActive = m_TutorialAction->IsActive() != FALSE;
    const bool waitActive = m_TutorialWait &&
        m_TutorialWait->IsActive() != FALSE;
    const bool continueActive = m_TutorialContinueBlock &&
        m_TutorialContinueBlock->IsActive() != FALSE;
    const bool exitActive = m_TutorialExitBlock->IsActive() != FALSE;
    int activeExitEvents = 0;
    for (CKBehavior *event : m_TutorialExitEvents) {
        if (event && event->IsActive() != FALSE)
            ++activeExitEvents;
    }
    const bool exitListening = activeExitEvents != 0;
    const bool exitInputActive = m_TutorialExitBlock->GetInputCount() > 0 &&
        m_TutorialExitBlock->IsInputActive(0) != FALSE;
    const bool exitOutputActive = m_TutorialExitBlock->GetOutputCount() > 0 &&
        m_TutorialExitBlock->IsOutputActive(0) != FALSE;
    const bool textActive = m_TutorialTextBlock &&
        m_TutorialTextBlock->IsActive() != FALSE;
    CK2dEntity *textFrame = m_TutorialTextBlock
        ? CK2dEntity::Cast(m_TutorialTextBlock->GetTarget()) : nullptr;
    const bool textFrameVisible = textFrame &&
        textFrame->IsVisible() != FALSE;
    const char *text = m_TutorialTextBlock &&
        m_TutorialTextBlock->GetInputParameterCount() > 1
        ? static_cast<const char *>(
            m_TutorialTextBlock->GetInputParameterReadDataPtr(1))
        : nullptr;
    const bool textPresent = text && *text;
    const bool promptActive = textActive && textPresent;

    const std::uint32_t state =
        (rootActive ? 1u : 0u) |
        (chapterActive ? 2u : 0u) |
        (actionActive ? 4u : 0u) |
        (waitActive ? 8u : 0u) |
        (continueActive ? 16u : 0u) |
        (exitActive ? 32u : 0u) |
        (exitInputActive ? 64u : 0u) |
        (exitOutputActive ? 128u : 0u) |
        (textActive ? 256u : 0u) |
        (textFrameVisible ? 512u : 0u) |
        (textPresent ? 1024u : 0u) |
        (exitListening ? 2048u : 0u);
    if (!m_TutorialStateObserved || state != m_TutorialState) {
        m_TutorialStateObserved = true;
        m_TutorialState = state;
        m_Logger->Info(
            "Gameplay tutorial state: root=%s chapter=%s action=%s "
            "wait=%s continue_key=%s exit_key=%s exit_in=%s exit_out=%s "
            "exit_events=%d/%d text=%s frame=%s content=%s",
            rootActive ? "true" : "false",
            chapterActive ? "true" : "false",
            actionActive ? "true" : "false",
            waitActive ? "true" : "false",
            continueActive ? "true" : "false",
            exitActive ? "true" : "false",
            exitInputActive ? "true" : "false",
            exitOutputActive ? "true" : "false",
            activeExitEvents,
            static_cast<int>(m_TutorialExitEvents.size()),
            textActive ? "true" : "false",
            textFrameVisible ? "true" : "false",
            textPresent ? "true" : "false");
    }

    if (promptActive && !m_TutorialPromptActive) {
        m_TutorialPromptActive = true;
        m_TutorialFrameRequested = true;
        m_Logger->Info("Gameplay tutorial: prompt_active=true");
    }

    if (exitListening && !m_TutorialExitReady) {
        m_TutorialExitReadyObserved = true;
        m_Logger->Info("Gameplay tutorial: exit_ready=true");
    }
    m_TutorialExitReady = exitListening;

    if (!m_TutorialExited && m_TutorialExitInputObserved && !exitListening) {
        const auto now = std::chrono::steady_clock::now();
        const auto latency =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_TutorialExitInputAt);
        m_TutorialExited = true;
        m_TutorialExitedAt = now;
        m_TutorialExitedByInput = m_TutorialExitReadyObserved &&
            latency.count() >= 0 && latency <= kTutorialInputResponseTime;
        m_Logger->Info(
            "Gameplay tutorial: exited=true by_input=%s latency_ms=%lld "
            "root_active=%s action_active=%s",
            m_TutorialExitedByInput ? "true" : "false",
            static_cast<long long>(latency.count()),
            rootActive ? "true" : "false",
            actionActive ? "true" : "false");
    }
}

} // namespace BML::PlayerTest
