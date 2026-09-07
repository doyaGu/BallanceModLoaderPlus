#include "Gameplay/GameEventHooks.h"

#include <cstring>
#include <string_view>
#include <utility>

#include "BML/IBML.h"
#include "BML/ILogger.h"
#include "BML/IMessageReceiver.h"
#include "Gameplay/BehaviorGraph.h"

namespace {
namespace Behavior = BML::Behavior;
using Receiver = IMessageReceiver;
using namespace Gameplay::Graph;

template<void (Receiver::*Method)()>
Behavior::Hook Callback(Receiver *receiver) {
    return Behavior::Hook([receiver] { (receiver->*Method)(); });
}

Behavior::Node FindWaitMessage(const Behavior::Graph &graph,
                               CKMessageType message) {
    Behavior::Node found;
    for (Behavior::Node node : graph.Nodes()) {
        if (node.Name() != "Wait Message")
            continue;
        const auto value = graph.Read(node.Pin(0));
        if (!value || value->State != Behavior::ObservationState::Available)
            continue;
        const auto *current = std::get_if<std::int32_t>(&value->Data);
        if (!current || *current != static_cast<std::int32_t>(message))
            continue;
        if (found)
            return {};
        found = node;
    }
    return found;
}

template<void (Receiver::*Method)()>
void Before(Behavior::Edit::Graph &edit, const Behavior::Link &link,
            Receiver *receiver) {
    edit.Before(edit.Require(link), Callback<Method>(receiver));
}

template<void (Receiver::*Method)()>
void After(Behavior::Edit::Graph &edit, const Behavior::Node &node,
           Receiver *receiver, Behavior::Selector output = Behavior::At(0)) {
    edit.Tap(edit.Require(node).Out(std::move(output)),
             Callback<Method>(receiver));
}
} // namespace

void GameEventHooks::OnLoad(IBML &bml, ILogger &logger) {
    m_BML = &bml;
    m_Receiver = &bml;
    m_Logger = &logger;
    auto opened = BML::Behavior::Session::Open("BML");
    if (!opened) {
        RejectPatch("Loader", opened.GetStatus().Message.empty()
            ? "the Behavior interface is unavailable"
            : opened.GetStatus().Message.c_str());
        return;
    }
    m_Behavior = std::move(opened).Value();

    auto plan = m_Behavior.Plan(
        "Game event hooks",
        BML::Behavior::On(BML::Behavior::Scripts::One("Event_handler"),
                          m_Edits[0]),
        BML::Behavior::On(BML::Behavior::Scripts::One("Gameplay_Ingame"),
                          m_Edits[1]),
        BML::Behavior::On(BML::Behavior::Scripts::One("Gameplay_Energy"),
                          m_Edits[2]),
        BML::Behavior::On(BML::Behavior::Scripts::One("Gameplay_Events"),
                          m_Edits[3]));
    if (!plan) {
        RejectPatch("Loader", plan.GetStatus().Message.empty()
            ? "the game-event Plan could not be created"
            : plan.GetStatus().Message.c_str());
        return;
    }
    m_Plan = std::move(plan).Value();
}

void GameEventHooks::OnUnload() {
    (void) m_Plan.Close();
    m_Behavior.Close();
    m_Logger = nullptr;
    m_Receiver = nullptr;
    m_BML = nullptr;
}

void GameEventHooks::OnLoadScript(CKBehavior *script) {
    if (!script || !script->GetName() || !m_Receiver || !m_Behavior ||
        !m_Plan)
        return;
    auto inspected = m_Behavior.Inspect(script);
    if (!inspected) {
        RejectPatch(script->GetName(), inspected.GetStatus().Message.empty()
            ? "the script graph could not be inspected"
            : inspected.GetStatus().Message.c_str());
        return;
    }
    const BML::Behavior::Graph graph = std::move(inspected).Value();
    const char *name = script->GetName();
    if (std::strcmp(name, "Event_handler") == 0)
        PatchBaseEventHandler(graph);
    else if (std::strcmp(name, "Gameplay_Ingame") == 0)
        PatchGameplayIngame(graph);
    else if (std::strcmp(name, "Gameplay_Energy") == 0)
        PatchGameplayEnergy(graph);
    else if (std::strcmp(name, "Gameplay_Events") == 0)
        PatchGameplayEvents(graph);
}

void GameEventHooks::PatchBaseEventHandler(
    const BML::Behavior::Graph &script) {
    namespace Behavior = BML::Behavior;
    const Behavior::Node dispatch = Find(
        script, "Switch On Message", 2, 11, 11, 0);
    if (!dispatch) {
        RejectPatch("Event_handler", "Switch On Message was not found");
        return;
    }
    const auto branch = [&](int output) { return Next(script, dispatch, output); };

    const Behavior::Node startMenu = branch(0);
    const Behavior::Link preStartMenu = Leaving(script, startMenu);
    const Behavior::Node postStartMenu = EndOfChain(script, startMenu);
    const Behavior::Node exitGame = branch(1);
    const Behavior::Link onExitGame = Leaving(script, exitGame);
    const Behavior::Node loadLevel = branch(2);
    const Behavior::Link preLoadLevel = Leaving(script, Follow(script, loadLevel, 2));
    const Behavior::Node postLoadLevel = EndOfChain(script, loadLevel);
    const Behavior::Node onStartLevel = EndOfChain(script, branch(3));

    const Behavior::Node resetLevel = Find(script, "reset Level");
    auto resetGraph = resetLevel ? script.Inspect(resetLevel)
                                 : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    Behavior::Node resetFirst;
    Behavior::Node resetSecond;
    Behavior::Link preResetLevel;
    if (resetGraph) {
        const auto first = resetGraph->Next(resetGraph->Root().In(0));
        resetFirst = first ? first.Value() : Behavior::Node{};
        resetSecond = Next(*resetGraph, resetFirst);
        preResetLevel = Leaving(*resetGraph, resetSecond);
    }
    const Behavior::Node postResetLevel = EndOfChain(script, branch(4));
    const Behavior::Node onPauseLevel = EndOfChain(script, branch(5));
    const Behavior::Node onUnpauseLevel = EndOfChain(script, branch(6));

    const Behavior::Node deleteCollisions = Find(script, "DeleteCollisionSurfaces");
    const Behavior::Node exitBranches = Next(script, deleteCollisions);
    const Behavior::Link preExitLevel = Leaving(script, Follow(script, branch(7), 4));
    const Behavior::Link postExitLevel = Leaving(script, Next(script, exitBranches, 0));
    const Behavior::Link preNextLevel = Leaving(script, Follow(script, branch(8), 4));
    const Behavior::Link postNextLevel = Leaving(script, Next(script, exitBranches, 1));
    const Behavior::Node onDead = EndOfChain(script, branch(9));

    const Behavior::Node highscore = Find(script, "Highscore");
    auto highscoreGraph = highscore ? script.Inspect(highscore)
                                    : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    const Behavior::Link preEndLevel = Leaving(script, branch(10));
    bool hasActivator = false;
    if (highscoreGraph) {
        for (Behavior::Node node : highscoreGraph->Nodes())
            hasActivator = hasActivator || node.Name() == "Activate Script";
    }

    if (!preStartMenu || !postStartMenu || !onExitGame || !preLoadLevel ||
        !postLoadLevel || !onStartLevel || !resetGraph || !preResetLevel ||
        !postResetLevel || !onPauseLevel || !onUnpauseLevel ||
        !preExitLevel || !postExitLevel || !preNextLevel || !postNextLevel ||
        !onDead || !highscoreGraph || !hasActivator || !preEndLevel) {
        RejectPatch("Event_handler",
                    "the script does not match the expected vanilla graph");
        return;
    }

    Behavior::Edit replacement;
    auto root = replacement.Root();
    Before<&Receiver::OnPreStartMenu>(root, preStartMenu, m_Receiver);
    After<&Receiver::OnPostStartMenu>(root, postStartMenu, m_Receiver);
    Before<&Receiver::OnExitGame>(root, onExitGame, m_Receiver);
    Before<&Receiver::OnPreLoadLevel>(root, preLoadLevel, m_Receiver);
    After<&Receiver::OnPostLoadLevel>(root, postLoadLevel, m_Receiver);
    After<&Receiver::OnStartLevel>(root, onStartLevel, m_Receiver);
    After<&Receiver::OnPostResetLevel>(root, postResetLevel, m_Receiver);
    After<&Receiver::OnPauseLevel>(root, onPauseLevel, m_Receiver);
    After<&Receiver::OnUnpauseLevel>(root, onUnpauseLevel, m_Receiver);
    Before<&Receiver::OnPreExitLevel>(root, preExitLevel, m_Receiver);
    Before<&Receiver::OnPostExitLevel>(root, postExitLevel, m_Receiver);
    Before<&Receiver::OnPreNextLevel>(root, preNextLevel, m_Receiver);
    Before<&Receiver::OnPostNextLevel>(root, postNextLevel, m_Receiver);
    After<&Receiver::OnDead>(root, onDead, m_Receiver);
    Before<&Receiver::OnPreEndLevel>(root, preEndLevel, m_Receiver);

    auto reset = root.Require(resetLevel).Graph();
    Before<&Receiver::OnPreResetLevel>(reset, preResetLevel, m_Receiver);

    const auto highscoreNode = root.Require(highscore);
    auto highscoreBody = highscoreNode.Graph();
    const auto highscoreOutput = highscoreBody.AppendOut("Out");
    for (Behavior::Node activator : highscoreGraph->Nodes()) {
        if (activator.Name() == "Activate Script")
            highscoreBody.Flow(highscoreBody.Require(activator).Out(),
                               highscoreOutput);
    }
    root.Tap(highscoreNode.Out("Out"),
             Callback<&Receiver::OnPostEndLevel>(m_Receiver));

    m_Edits[0] = std::move(replacement);
    if (m_Logger)
        m_Logger->Info("Insert game lifecycle hooks");
    (void) ReplacePlan("Event_handler");
}

void GameEventHooks::PatchGameplayIngame(
    const BML::Behavior::Graph &script) {
    namespace Behavior = BML::Behavior;
    CKMessageManager *messages = m_BML ? m_BML->GetMessageManager() : nullptr;
    const Behavior::Node camera = Find(script, "CamNav On/Off");
    const Behavior::Node ball = Find(script, "BallNav On/Off");
    auto cameraGraph = camera ? script.Inspect(camera)
                              : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    auto ballGraph = ball ? script.Inspect(ball)
                          : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    if (!messages || !cameraGraph || !ballGraph) {
        RejectPatch("Gameplay_Ingame", "navigation event graphs are unavailable");
        return;
    }

    const Behavior::Node cameraOn = FindWaitMessage(
        *cameraGraph, messages->AddMessageType("CamNav activate"));
    const Behavior::Node cameraOff = FindWaitMessage(
        *cameraGraph, messages->AddMessageType("CamNav deactivate"));
    const Behavior::Node ballOn = FindWaitMessage(
        *ballGraph, messages->AddMessageType("BallNav activate"));
    const Behavior::Node ballOff = FindWaitMessage(
        *ballGraph, messages->AddMessageType("BallNav deactivate"));
    if (!cameraOn || !cameraOff || !ballOn || !ballOff) {
        RejectPatch("Gameplay_Ingame",
                    "navigation messages do not match the expected vanilla graph");
        return;
    }

    Behavior::Edit replacement;
    auto root = replacement.Root();
    auto cameraBody = root.Require(camera).Graph();
    After<&Receiver::OnCamNavActive>(cameraBody, cameraOn, m_Receiver);
    After<&Receiver::OnCamNavInactive>(cameraBody, cameraOff, m_Receiver);
    auto ballBody = root.Require(ball).Graph();
    After<&Receiver::OnBallNavActive>(ballBody, ballOn, m_Receiver);
    After<&Receiver::OnBallNavInactive>(ballBody, ballOff, m_Receiver);
    m_Edits[1] = std::move(replacement);
    if (m_Logger)
        m_Logger->Info("Insert ball and camera navigation hooks");
    (void) ReplacePlan("Gameplay_Ingame");
}

void GameEventHooks::PatchGameplayEnergy(
    const BML::Behavior::Graph &script) {
    namespace Behavior = BML::Behavior;
    CKMessageManager *messages = m_BML ? m_BML->GetMessageManager() : nullptr;
    const Behavior::Node dispatch = Find(script, "Switch On Message");
    if (!messages || !dispatch) {
        RejectPatch("Gameplay_Energy", "message graph is unavailable");
        return;
    }

    const Behavior::Link counterActive = Leaving(script, dispatch, 3);
    const Behavior::Link counterInactive = Leaving(script, dispatch, 1);
    const Behavior::Node lifeUp = FindWaitMessage(
        script, messages->AddMessageType("Life_Up"));
    const Behavior::Node ballOff = FindWaitMessage(
        script, messages->AddMessageType("Ball Off"));
    const Behavior::Node subLife = FindWaitMessage(
        script, messages->AddMessageType("Sub Life"));
    const Behavior::Node extraPoint = FindWaitMessage(
        script, messages->AddMessageType("Extrapoint"));
    const Behavior::Link lifeUpLink = LeavingFor(script, lifeUp, "add Life");
    const Behavior::Link ballOffLink = LeavingFor(script, ballOff, "Delayer");
    const Behavior::Link subLifeLink = LeavingFor(script, subLife, "sub Life");
    const Behavior::Link extraPointLink = LeavingFor(script, extraPoint, "Show");
    const Behavior::Node postLifeUp = EndOfChain(script, Sink(script, lifeUpLink));
    const Behavior::Node postSubLife = EndOfChain(script, Sink(script, subLifeLink));
    if (!counterActive || !counterInactive || !lifeUpLink || !ballOffLink ||
        !subLifeLink || !extraPointLink || !postLifeUp || !postSubLife) {
        RejectPatch("Gameplay_Energy",
                    "the script does not match the expected vanilla graph");
        return;
    }

    Behavior::Edit replacement;
    auto root = replacement.Root();
    Before<&Receiver::OnCounterActive>(root, counterActive, m_Receiver);
    Before<&Receiver::OnCounterInactive>(root, counterInactive, m_Receiver);
    Before<&Receiver::OnPreLifeUp>(root, lifeUpLink, m_Receiver);
    After<&Receiver::OnPostLifeUp>(root, postLifeUp, m_Receiver);
    Before<&Receiver::OnBallOff>(root, ballOffLink, m_Receiver);
    Before<&Receiver::OnPreSubLife>(root, subLifeLink, m_Receiver);
    After<&Receiver::OnPostSubLife>(root, postSubLife, m_Receiver);
    Before<&Receiver::OnExtraPoint>(root, extraPointLink, m_Receiver);
    m_Edits[2] = std::move(replacement);
    if (m_Logger)
        m_Logger->Info("Insert counter, life, and point hooks");
    (void) ReplacePlan("Gameplay_Energy");
}

void GameEventHooks::PatchGameplayEvents(
    const BML::Behavior::Graph &script) {
    namespace Behavior = BML::Behavior;
    CKMessageManager *messages = m_BML ? m_BML->GetMessageManager() : nullptr;
    if (!messages) {
        RejectPatch("Gameplay_Events", "message manager is unavailable");
        return;
    }
    const Behavior::Node checkpoint = FindWaitMessage(
        script, messages->AddMessageType("Checkpoint reached"));
    const Behavior::Node gameOver = FindWaitMessage(
        script, messages->AddMessageType("Game Over"));
    const Behavior::Node levelFinish = FindWaitMessage(
        script, messages->AddMessageType("Level_Finish"));
    const Behavior::Link checkpointLink = LeavingFor(
        script, checkpoint, "set Resetpoint");
    const Behavior::Link gameOverLink = LeavingFor(
        script, gameOver, "Send Message");
    const Behavior::Link levelFinishLink = LeavingFor(
        script, levelFinish, "Send Message");
    const Behavior::Node postCheckpoint = EndOfChain(
        script, Sink(script, checkpointLink));
    if (!checkpointLink || !gameOverLink || !levelFinishLink ||
        !postCheckpoint) {
        RejectPatch("Gameplay_Events",
                    "the script does not match the expected vanilla graph");
        return;
    }

    Behavior::Edit replacement;
    auto root = replacement.Root();
    Before<&Receiver::OnPreCheckpointReached>(root, checkpointLink, m_Receiver);
    After<&Receiver::OnPostCheckpointReached>(root, postCheckpoint, m_Receiver);
    Before<&Receiver::OnGameOver>(root, gameOverLink, m_Receiver);
    Before<&Receiver::OnLevelFinish>(root, levelFinishLink, m_Receiver);
    m_Edits[3] = std::move(replacement);
    if (m_Logger)
        m_Logger->Info("Insert checkpoint, game-over, and level-finish hooks");
    (void) ReplacePlan("Gameplay_Events");
}

bool GameEventHooks::ReplacePlan(const char *scriptName) {
    auto replaced = m_Plan.Replace(
        BML::Behavior::On(BML::Behavior::Scripts::One("Event_handler"),
                          m_Edits[0]),
        BML::Behavior::On(BML::Behavior::Scripts::One("Gameplay_Ingame"),
                          m_Edits[1]),
        BML::Behavior::On(BML::Behavior::Scripts::One("Gameplay_Energy"),
                          m_Edits[2]),
        BML::Behavior::On(BML::Behavior::Scripts::One("Gameplay_Events"),
                          m_Edits[3]));
    if (replaced)
        return true;
    RejectPatch(scriptName, replaced.GetStatus().Message.empty()
        ? "the game-event Plan could not be replaced"
        : replaced.GetStatus().Message.c_str());
    return false;
}

void GameEventHooks::RejectPatch(const char *scriptName,
                                 const char *reason) const {
    if (m_Logger) {
        m_Logger->Error("Game event hooks are unavailable for %s: %s",
                        scriptName ? scriptName : "unknown script",
                        reason ? reason : "unknown reason");
    }
}
