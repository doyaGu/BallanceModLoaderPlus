#include "Gameplay/GameEventHooks.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "BML/IBML.h"
#include "BML/ILogger.h"
#include "BML/IMessageReceiver.h"
#include "BML/ScriptHelper.h"
#include "Loader/ModContext.h"
#include "Behavior/HookBlock.h"
#include "Behavior/Patches.h"

using namespace ScriptHelper;

namespace {
using Receiver = IMessageReceiver;

template<void (Receiver::*Method)()>
int Dispatch(const CKBehaviorContext *, void *argument) {
    auto *receiver = static_cast<Receiver *>(argument);
    (receiver->*Method)();
    return CKBR_OK;
}

BML::Behavior::ObjectRef Reference(CKObject *object) {
    ModContext *context = BML_GetModContext();
    if (!context || !object)
        return {};
    const BML_ObjectRef issued = context->ObjectRefs().Issue(object);
    return {issued.Domain, issued.Slot, issued.Generation};
}

CKBehavior *SinkOf(CKBehaviorLink *link) {
    CKBehaviorIO *sink = link ? link->GetOutBehaviorIO() : nullptr;
    return sink ? sink->GetOwner() : nullptr;
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

// The event callbacks one vanilla graph receives. Each declaration names where
// a callback belongs and which event it delivers, and nothing is edited until
// Install carries the whole set into a single Patch, so a graph either gains
// every hook or keeps its vanilla shape. A callback Block is infrastructure, so
// the Logical view of the graph still reports the vanilla shape, and closing
// the Patch takes every hook back out.
class Hooks {
public:
    Hooks(CKBehavior *graph, Receiver &receiver)
        : m_Graph(graph), m_Receiver(&receiver) {}

    // Runs inside the Link: after the Out that feeds it, before the Node it
    // reaches.
    template<void (Receiver::*Method)()>
    void Before(CKBehaviorLink *link) {
        m_Plan.Before(m_Plan.UseLink(Reference(link)), Callback<Method>());
    }

    // Runs once the Node has fired, on a Link of the Out it fired from.
    template<void (Receiver::*Method)()>
    void After(CKBehavior *node, int output = 0) {
        m_Plan.Tap(m_Plan.UseNode(Reference(node)).Out(output),
                   Callback<Method>());
    }

    BML::Behavior::Status Install(std::string name,
                                  std::vector<std::uintptr_t> &installed);

private:
    template<void (Receiver::*Method)()>
    [[nodiscard]] BML::Behavior::HookBlock::Hook Callback() const {
        return {&Dispatch<Method>, m_Receiver};
    }

    CKBehavior *m_Graph = nullptr;
    Receiver *m_Receiver = nullptr;
    BML::Behavior::GraphEdit m_Plan;
};

BML::Behavior::Status Hooks::Install(std::string name,
                                     std::vector<std::uintptr_t> &installed) {
    ModContext *context = BML_GetModContext();
    if (!context) {
        return {BML::Behavior::Error::InvalidState, CK_OK, CKBR_OK,
                "The Loader is not available."};
    }

    BML::Behavior::PatchId patch = 0;
    const BML::Behavior::Status status = context->BehaviorPatches().Apply(
        context->LoaderBehaviorOwner(), Reference(m_Graph), std::move(name),
        std::move(m_Plan), patch);
    if (status && patch)
        installed.push_back(patch);
    return status;
}
} // namespace

void GameEventHooks::OnLoad(IBML &bml, ILogger &logger) {
    m_BML = &bml;
    m_Receiver = &bml;
    m_Logger = &logger;
}

void GameEventHooks::OnUnload() {
    // Closing each Patch takes the callbacks out of the graphs that still hold
    // them, so no hook can reach a receiver this Mod no longer owns.
    if (ModContext *context = BML_GetModContext()) {
        const BML::Behavior::SessionOwner owner =
            context->LoaderBehaviorOwner();
        for (std::uintptr_t patch : m_Installed)
            (void) context->BehaviorPatches().Close(owner, patch);
    }
    m_Installed.clear();
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

    // Highscore says nothing when it is done, so the end of the level needs an
    // Out on that graph. A Patch appends a port to a Node it names, never to
    // the graph it edits, so this one stays a direct edit of the script.
    CKBehaviorIO *highscoreOutput = highscore->CreateOutput("Out");
    if (!highscoreOutput) {
        RejectPatch("Event_handler", "the Highscore output could not be created");
        return;
    }
    for (CKBehavior *activator : highscoreActivators)
        CreateLink(highscore, activator, highscoreOutput);

    if (m_Logger)
        m_Logger->Info("Insert game lifecycle hooks");

    Hooks hooks(script, *m_Receiver);
    hooks.Before<&Receiver::OnPreStartMenu>(preStartMenu);
    hooks.After<&Receiver::OnPostStartMenu>(postStartMenu);
    hooks.Before<&Receiver::OnExitGame>(onExitGame);

    hooks.Before<&Receiver::OnPreLoadLevel>(preLoadLevel);
    hooks.After<&Receiver::OnPostLoadLevel>(postLoadLevel);
    hooks.After<&Receiver::OnStartLevel>(onStartLevel);

    hooks.After<&Receiver::OnPostResetLevel>(postResetLevel);
    hooks.After<&Receiver::OnPauseLevel>(onPauseLevel);
    hooks.After<&Receiver::OnUnpauseLevel>(onUnpauseLevel);

    hooks.Before<&Receiver::OnPreExitLevel>(preExitLevel);
    hooks.Before<&Receiver::OnPostExitLevel>(postExitLevel);
    hooks.Before<&Receiver::OnPreNextLevel>(preNextLevel);
    hooks.Before<&Receiver::OnPostNextLevel>(postNextLevel);
    hooks.After<&Receiver::OnDead>(onDead);

    hooks.Before<&Receiver::OnPreEndLevel>(preEndLevel);
    hooks.After<&Receiver::OnPostEndLevel>(
        highscore, highscore->GetOutputPosition(highscoreOutput));

    const BML::Behavior::Status status =
        hooks.Install("Event_handler", m_Installed);
    if (!status) {
        RejectPatch("Event_handler", status.Message.c_str());
        return;
    }

    // The reset chain runs inside its own graph, so the Link that carries it
    // belongs to a Patch of that graph and not of the script above it.
    Hooks reset(resetLevel, *m_Receiver);
    reset.Before<&Receiver::OnPreResetLevel>(preResetLevel);
    const BML::Behavior::Status resetStatus =
        reset.Install("Event_handler reset Level", m_Installed);
    if (!resetStatus)
        RejectPatch("Event_handler", resetStatus.Message.c_str());
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

    // Each navigation switch is a graph of its own, so each one takes a Patch
    // of its own.
    Hooks cameraHooks(camera, *m_Receiver);
    cameraHooks.After<&Receiver::OnCamNavActive>(cameraOn);
    cameraHooks.After<&Receiver::OnCamNavInactive>(cameraOff);
    const BML::Behavior::Status cameraStatus =
        cameraHooks.Install("Gameplay_Ingame CamNav", m_Installed);
    if (!cameraStatus)
        RejectPatch("Gameplay_Ingame", cameraStatus.Message.c_str());

    Hooks ballHooks(ball, *m_Receiver);
    ballHooks.After<&Receiver::OnBallNavActive>(ballOn);
    ballHooks.After<&Receiver::OnBallNavInactive>(ballOff);
    const BML::Behavior::Status ballStatus =
        ballHooks.Install("Gameplay_Ingame BallNav", m_Installed);
    if (!ballStatus)
        RejectPatch("Gameplay_Ingame", ballStatus.Message.c_str());
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

    // The post event of a pair belongs at the end of the chain the pre event
    // opens, which is the chain that starts at the Node its Link feeds.
    CKBehavior *postLifeUp = FindEndOfChain(script, SinkOf(lifeUpLink));
    CKBehavior *postSubLife = FindEndOfChain(script, SinkOf(subLifeLink));

    if (!counterActive || !counterInactive || !lifeUpLink || !ballOffLink ||
        !subLifeLink || !extraPointLink || !postLifeUp || !postSubLife) {
        RejectPatch("Gameplay_Energy", "the script does not match the expected vanilla graph");
        return;
    }

    if (m_Logger)
        m_Logger->Info("Insert counter, life, and point hooks");

    Hooks hooks(script, *m_Receiver);
    hooks.Before<&Receiver::OnCounterActive>(counterActive);
    hooks.Before<&Receiver::OnCounterInactive>(counterInactive);
    hooks.Before<&Receiver::OnPreLifeUp>(lifeUpLink);
    hooks.After<&Receiver::OnPostLifeUp>(postLifeUp);
    hooks.Before<&Receiver::OnBallOff>(ballOffLink);
    hooks.Before<&Receiver::OnPreSubLife>(subLifeLink);
    hooks.After<&Receiver::OnPostSubLife>(postSubLife);
    hooks.Before<&Receiver::OnExtraPoint>(extraPointLink);

    const BML::Behavior::Status status =
        hooks.Install("Gameplay_Energy", m_Installed);
    if (!status)
        RejectPatch("Gameplay_Energy", status.Message.c_str());
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
    CKBehavior *postCheckpoint = FindEndOfChain(script, SinkOf(checkpointLink));
    if (!checkpointLink || !gameOverLink || !levelFinishLink || !postCheckpoint) {
        RejectPatch("Gameplay_Events", "the script does not match the expected vanilla graph");
        return;
    }

    if (m_Logger)
        m_Logger->Info("Insert checkpoint, game-over, and level-finish hooks");

    Hooks hooks(script, *m_Receiver);
    hooks.Before<&Receiver::OnPreCheckpointReached>(checkpointLink);
    hooks.After<&Receiver::OnPostCheckpointReached>(postCheckpoint);
    hooks.Before<&Receiver::OnGameOver>(gameOverLink);
    hooks.Before<&Receiver::OnLevelFinish>(levelFinishLink);

    const BML::Behavior::Status status =
        hooks.Install("Gameplay_Events", m_Installed);
    if (!status)
        RejectPatch("Gameplay_Events", status.Message.c_str());
}

void GameEventHooks::RejectPatch(const char *scriptName, const char *reason) const {
    if (m_Logger) {
        m_Logger->Error("Game event hooks are unavailable for %s: %s",
                        scriptName ? scriptName : "unknown script",
                        reason ? reason : "unknown reason");
    }
}
