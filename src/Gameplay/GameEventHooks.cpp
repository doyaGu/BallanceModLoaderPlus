#include "Gameplay/GameEventHooks.h"

#include <cstring>
#include <vector>

#include "BML/IBML.h"
#include "BML/ILogger.h"
#include "BML/IMessageReceiver.h"
#include "BML/ScriptHelper.h"
#include "Loader/ModContext.h"
#include "Virtools/BallanceBehaviorPresets.h"

using namespace ScriptHelper;

namespace {
using Receiver = IMessageReceiver;

template<void (Receiver::*Method)()>
int Dispatch(const CKBehaviorContext *, void *argument) {
    auto *receiver = static_cast<Receiver *>(argument);
    (receiver->*Method)();
    return CKBR_OK;
}

template<void (Receiver::*Method)()>
CKBehavior *CreateEventHook(CKBehavior *script, Receiver &receiver) {
    ModContext *context = BML_GetModContext();
    if (!context)
        return nullptr;
    BML::Virtools::GraphBlockResult created = context->GetBehaviorRuntime().AddToGraph(
        script, BML::Virtools::Presets::Hook(&Dispatch<Method>, &receiver));
    return created ? created.Behavior : nullptr;
}

CKBehavior *Follow(CKBehavior *graph, CKBehavior *behavior, int count) {
    for (int i = 0; behavior && i < count; ++i)
        behavior = FindNextBB(graph, behavior);
    return behavior;
}

CKBehavior *FindWaitMessage(CKBehavior *script, CKMessageType message) {
    if (!script)
        return nullptr;

    CKBehavior *result = nullptr;
    FindBB(script, [&](CKBehavior *behavior) {
        if (!behavior || behavior->GetInputParameterCount() == 0)
            return true;
        CKParameterIn *input = behavior->GetInputParameter(0);
        CKParameter *source = input ? input->GetDirectSource() : nullptr;
        if (source && GetParamValue<CKMessageType>(source) == message)
            result = behavior;
        return true;
    }, "Wait Message");
    return result;
}
} // namespace

void GameEventHooks::OnLoad(IBML &bml, ILogger &logger) {
    m_BML = &bml;
    m_Receiver = &bml;
    m_Logger = &logger;
}

void GameEventHooks::OnUnload() {
    m_Logger = nullptr;
    m_Receiver = nullptr;
    m_BML = nullptr;
}

void GameEventHooks::OnLoadScript(CKBehavior *script) {
    if (!script || !script->GetName() || !m_BML || !m_Receiver)
        return;

    const char *name = script->GetName();
    if (std::strcmp(name, "Event_handler") == 0)
        PatchBaseEventHandler(script);
    else if (std::strcmp(name, "Gameplay_Ingame") == 0)
        PatchGameplayIngame(script);
    else if (std::strcmp(name, "Gameplay_Energy") == 0)
        PatchGameplayEnergy(script);
    else if (std::strcmp(name, "Gameplay_Events") == 0)
        PatchGameplayEvents(script);
}

void GameEventHooks::PatchBaseEventHandler(CKBehavior *script) {
    CKBehavior *switchOnMessage = FindFirstBB(script, "Switch On Message", false, 2, 11, 11, 0);
    if (!switchOnMessage) {
        RejectPatch("Event_handler", "Switch On Message was not found");
        return;
    }

    const auto branch = [&](int output) {
        return FindNextBB(script, switchOnMessage, nullptr, output, 0);
    };

    CKBehavior *startMenu = branch(0);
    CKBehaviorLink *preStartMenu = startMenu ? FindNextLink(script, startMenu) : nullptr;
    CKBehavior *postStartMenu = startMenu ? FindEndOfChain(script, startMenu) : nullptr;

    CKBehavior *exitGame = branch(1);
    CKBehaviorLink *onExitGame = exitGame ? FindNextLink(script, exitGame) : nullptr;

    CKBehavior *loadLevel = branch(2);
    CKBehavior *preLoadSource = Follow(script, loadLevel, 2);
    CKBehaviorLink *preLoadLevel = preLoadSource ? FindNextLink(script, preLoadSource) : nullptr;
    CKBehavior *postLoadLevel = loadLevel ? FindEndOfChain(script, loadLevel) : nullptr;

    CKBehavior *startLevel = branch(3);
    CKBehavior *onStartLevel = startLevel ? FindEndOfChain(script, startLevel) : nullptr;

    CKBehavior *resetLevel = FindFirstBB(script, "reset Level");
    CKBehavior *resetFirst = resetLevel && resetLevel->GetInputCount() > 0
                                 ? FindNextBB(resetLevel, resetLevel->GetInput(0))
                                 : nullptr;
    CKBehavior *resetSecond = resetFirst ? FindNextBB(resetLevel, resetFirst) : nullptr;
    CKBehaviorLink *preResetLevel = resetSecond ? FindNextLink(resetLevel, resetSecond) : nullptr;
    CKBehavior *resetBranch = branch(4);
    CKBehavior *postResetLevel = resetBranch ? FindEndOfChain(script, resetBranch) : nullptr;

    CKBehavior *pauseLevel = branch(5);
    CKBehavior *onPauseLevel = pauseLevel ? FindEndOfChain(script, pauseLevel) : nullptr;
    CKBehavior *unpauseLevel = branch(6);
    CKBehavior *onUnpauseLevel = unpauseLevel ? FindEndOfChain(script, unpauseLevel) : nullptr;

    CKBehavior *deleteCollisions = FindFirstBB(script, "DeleteCollisionSurfaces");
    CKBehavior *exitBranches = deleteCollisions ? FindNextBB(script, deleteCollisions) : nullptr;

    CKBehavior *exitLevel = Follow(script, branch(7), 4);
    CKBehaviorLink *preExitLevel = exitLevel ? FindNextLink(script, exitLevel) : nullptr;
    CKBehavior *postExitSource = exitBranches
                                     ? FindNextBB(script, exitBranches, nullptr, 0, 0)
                                     : nullptr;
    CKBehaviorLink *postExitLevel = postExitSource ? FindNextLink(script, postExitSource) : nullptr;

    CKBehavior *nextLevel = Follow(script, branch(8), 4);
    CKBehaviorLink *preNextLevel = nextLevel ? FindNextLink(script, nextLevel) : nullptr;
    CKBehavior *postNextSource = exitBranches
                                     ? FindNextBB(script, exitBranches, nullptr, 1, 0)
                                     : nullptr;
    CKBehaviorLink *postNextLevel = postNextSource ? FindNextLink(script, postNextSource) : nullptr;

    CKBehavior *dead = branch(9);
    CKBehavior *onDead = dead ? FindEndOfChain(script, dead) : nullptr;

    CKBehavior *highscore = FindFirstBB(script, "Highscore");
    std::vector<CKBehavior *> highscoreActivators;
    if (highscore) {
        FindBB(highscore, [&](CKBehavior *behavior) {
            highscoreActivators.push_back(behavior);
            return true;
        }, "Activate Script");
    }
    CKBehavior *endLevel = branch(10);
    CKBehaviorLink *preEndLevel = endLevel ? FindNextLink(script, endLevel) : nullptr;

    if (!preStartMenu || !postStartMenu || !onExitGame ||
        !preLoadLevel || !postLoadLevel || !onStartLevel ||
        !preResetLevel || !postResetLevel || !onPauseLevel || !onUnpauseLevel ||
        !preExitLevel || !postExitLevel || !preNextLevel || !postNextLevel ||
        !onDead || !highscore || highscoreActivators.empty() || !preEndLevel) {
        RejectPatch("Event_handler", "the script does not match the expected vanilla graph");
        return;
    }

    CKBehaviorIO *highscoreOutput = highscore->CreateOutput("Out");
    if (!highscoreOutput) {
        RejectPatch("Event_handler", "the Highscore output could not be created");
        return;
    }

    if (m_Logger)
        m_Logger->Info("Insert game lifecycle hooks");

    InsertBB(script, preStartMenu, CreateEventHook<&Receiver::OnPreStartMenu>(script, *m_Receiver));
    CreateLink(script, postStartMenu, CreateEventHook<&Receiver::OnPostStartMenu>(script, *m_Receiver));
    InsertBB(script, onExitGame, CreateEventHook<&Receiver::OnExitGame>(script, *m_Receiver));

    InsertBB(script, preLoadLevel, CreateEventHook<&Receiver::OnPreLoadLevel>(script, *m_Receiver));
    CreateLink(script, postLoadLevel, CreateEventHook<&Receiver::OnPostLoadLevel>(script, *m_Receiver));
    CreateLink(script, onStartLevel, CreateEventHook<&Receiver::OnStartLevel>(script, *m_Receiver));

    InsertBB(script, preResetLevel, CreateEventHook<&Receiver::OnPreResetLevel>(script, *m_Receiver));
    CreateLink(script, postResetLevel, CreateEventHook<&Receiver::OnPostResetLevel>(script, *m_Receiver));
    CreateLink(script, onPauseLevel, CreateEventHook<&Receiver::OnPauseLevel>(script, *m_Receiver));
    CreateLink(script, onUnpauseLevel, CreateEventHook<&Receiver::OnUnpauseLevel>(script, *m_Receiver));

    InsertBB(script, preExitLevel, CreateEventHook<&Receiver::OnPreExitLevel>(script, *m_Receiver));
    InsertBB(script, postExitLevel, CreateEventHook<&Receiver::OnPostExitLevel>(script, *m_Receiver));
    InsertBB(script, preNextLevel, CreateEventHook<&Receiver::OnPreNextLevel>(script, *m_Receiver));
    InsertBB(script, postNextLevel, CreateEventHook<&Receiver::OnPostNextLevel>(script, *m_Receiver));
    CreateLink(script, onDead, CreateEventHook<&Receiver::OnDead>(script, *m_Receiver));

    for (CKBehavior *activator : highscoreActivators)
        CreateLink(highscore, activator, highscoreOutput);

    InsertBB(script, preEndLevel, CreateEventHook<&Receiver::OnPreEndLevel>(script, *m_Receiver));
    CreateLink(script, highscoreOutput,
               CreateEventHook<&Receiver::OnPostEndLevel>(script, *m_Receiver));
}

void GameEventHooks::PatchGameplayIngame(CKBehavior *script) {
    CKMessageManager *messages = m_BML ? m_BML->GetMessageManager() : nullptr;
    CKBehavior *camera = FindFirstBB(script, "CamNav On/Off");
    CKBehavior *ball = FindFirstBB(script, "BallNav On/Off");
    if (!messages || !camera || !ball) {
        RejectPatch("Gameplay_Ingame", "navigation event graphs are unavailable");
        return;
    }

    CKBehavior *cameraOn = FindWaitMessage(camera, messages->AddMessageType("CamNav activate"));
    CKBehavior *cameraOff = FindWaitMessage(camera, messages->AddMessageType("CamNav deactivate"));
    CKBehavior *ballOn = FindWaitMessage(ball, messages->AddMessageType("BallNav activate"));
    CKBehavior *ballOff = FindWaitMessage(ball, messages->AddMessageType("BallNav deactivate"));
    if (!cameraOn || !cameraOff || !ballOn || !ballOff) {
        RejectPatch("Gameplay_Ingame", "navigation messages do not match the expected vanilla graph");
        return;
    }

    if (m_Logger)
        m_Logger->Info("Insert ball and camera navigation hooks");
    CreateLink(camera, cameraOn, CreateEventHook<&Receiver::OnCamNavActive>(camera, *m_Receiver));
    CreateLink(camera, cameraOff, CreateEventHook<&Receiver::OnCamNavInactive>(camera, *m_Receiver));
    CreateLink(ball, ballOn, CreateEventHook<&Receiver::OnBallNavActive>(ball, *m_Receiver));
    CreateLink(ball, ballOff, CreateEventHook<&Receiver::OnBallNavInactive>(ball, *m_Receiver));
}

void GameEventHooks::PatchGameplayEnergy(CKBehavior *script) {
    CKMessageManager *messages = m_BML ? m_BML->GetMessageManager() : nullptr;
    CKBehavior *switchOnMessage = FindFirstBB(script, "Switch On Message");
    if (!messages || !switchOnMessage) {
        RejectPatch("Gameplay_Energy", "message graph is unavailable");
        return;
    }

    CKBehaviorLink *counterActive = FindNextLink(script, switchOnMessage, nullptr, 3);
    CKBehaviorLink *counterInactive = FindNextLink(script, switchOnMessage, nullptr, 1);
    CKBehavior *lifeUp = FindWaitMessage(script, messages->AddMessageType("Life_Up"));
    CKBehavior *ballOff = FindWaitMessage(script, messages->AddMessageType("Ball Off"));
    CKBehavior *subLife = FindWaitMessage(script, messages->AddMessageType("Sub Life"));
    CKBehavior *extraPoint = FindWaitMessage(script, messages->AddMessageType("Extrapoint"));

    CKBehaviorLink *lifeUpLink = lifeUp ? FindNextLink(script, lifeUp, "add Life") : nullptr;
    CKBehaviorLink *ballOffLink = ballOff ? FindNextLink(script, ballOff, "Delayer") : nullptr;
    CKBehaviorLink *subLifeLink = subLife ? FindNextLink(script, subLife, "sub Life") : nullptr;
    CKBehaviorLink *extraPointLink = extraPoint ? FindNextLink(script, extraPoint, "Show") : nullptr;

    if (!counterActive || !counterInactive || !lifeUpLink || !ballOffLink ||
        !subLifeLink || !extraPointLink) {
        RejectPatch("Gameplay_Energy", "the script does not match the expected vanilla graph");
        return;
    }

    if (m_Logger)
        m_Logger->Info("Insert counter, life, and point hooks");
    InsertBB(script, counterActive, CreateEventHook<&Receiver::OnCounterActive>(script, *m_Receiver));
    InsertBB(script, counterInactive, CreateEventHook<&Receiver::OnCounterInactive>(script, *m_Receiver));

    CKBehavior *lifeUpHook = CreateEventHook<&Receiver::OnPreLifeUp>(script, *m_Receiver);
    InsertBB(script, lifeUpLink, lifeUpHook);
    CreateLink(script, FindEndOfChain(script, lifeUpHook),
               CreateEventHook<&Receiver::OnPostLifeUp>(script, *m_Receiver));
    InsertBB(script, ballOffLink, CreateEventHook<&Receiver::OnBallOff>(script, *m_Receiver));

    CKBehavior *subLifeHook = CreateEventHook<&Receiver::OnPreSubLife>(script, *m_Receiver);
    InsertBB(script, subLifeLink, subLifeHook);
    CreateLink(script, FindEndOfChain(script, subLifeHook),
               CreateEventHook<&Receiver::OnPostSubLife>(script, *m_Receiver));
    InsertBB(script, extraPointLink, CreateEventHook<&Receiver::OnExtraPoint>(script, *m_Receiver));
}

void GameEventHooks::PatchGameplayEvents(CKBehavior *script) {
    CKMessageManager *messages = m_BML ? m_BML->GetMessageManager() : nullptr;
    if (!messages) {
        RejectPatch("Gameplay_Events", "message manager is unavailable");
        return;
    }

    CKBehavior *checkpoint = FindWaitMessage(script, messages->AddMessageType("Checkpoint reached"));
    CKBehavior *gameOver = FindWaitMessage(script, messages->AddMessageType("Game Over"));
    CKBehavior *levelFinish = FindWaitMessage(script, messages->AddMessageType("Level_Finish"));
    CKBehaviorLink *checkpointLink = checkpoint
                                         ? FindNextLink(script, checkpoint, "set Resetpoint")
                                         : nullptr;
    CKBehaviorLink *gameOverLink = gameOver ? FindNextLink(script, gameOver, "Send Message") : nullptr;
    CKBehaviorLink *levelFinishLink = levelFinish
                                          ? FindNextLink(script, levelFinish, "Send Message")
                                          : nullptr;
    if (!checkpointLink || !gameOverLink || !levelFinishLink) {
        RejectPatch("Gameplay_Events", "the script does not match the expected vanilla graph");
        return;
    }

    if (m_Logger)
        m_Logger->Info("Insert checkpoint, game-over, and level-finish hooks");
    CKBehavior *checkpointHook = CreateEventHook<&Receiver::OnPreCheckpointReached>(script, *m_Receiver);
    InsertBB(script, checkpointLink, checkpointHook);
    CreateLink(script, FindEndOfChain(script, checkpointHook),
               CreateEventHook<&Receiver::OnPostCheckpointReached>(script, *m_Receiver));
    InsertBB(script, gameOverLink, CreateEventHook<&Receiver::OnGameOver>(script, *m_Receiver));
    InsertBB(script, levelFinishLink, CreateEventHook<&Receiver::OnLevelFinish>(script, *m_Receiver));
}

void GameEventHooks::RejectPatch(const char *scriptName, const char *reason) const {
    if (m_Logger) {
        m_Logger->Error("Game event hooks are unavailable for %s: %s",
                        scriptName ? scriptName : "unknown script",
                        reason ? reason : "unknown reason");
    }
}
