#include "Gameplay/GameEventHooks.h"

#include <cstdint>
#include <utility>

#include "BML/IBML.h"
#include "BML/ILogger.h"
#include "BML/IMessageReceiver.h"

namespace {
namespace Behavior = BML::Behavior;
using Receiver = IMessageReceiver;

template<void (Receiver::*Method)()>
Behavior::Hook Callback(Receiver *receiver) {
    return Behavior::Hook([receiver] { (receiver->*Method)(); });
}

Behavior::Edit::Node WaitMessage(Behavior::Edit::Graph &graph,
                                 CKMessageType message) {
    Behavior::NodePattern pattern("Wait Message");
    pattern.Pin(0, Behavior::Value::As(
        CKPGUID_MESSAGE, static_cast<std::int32_t>(message)));
    return graph.Require(std::move(pattern));
}

Behavior::Edit::Node Advance(Behavior::Edit::Graph &graph,
                             Behavior::Edit::Node node, int count) {
    while (count-- > 0)
        node = graph.Next(node);
    return node;
}

Behavior::Edit BaseEventHandler(Receiver *receiver) {
    Behavior::Edit edit;
    auto root = edit.Root();
    Behavior::NodePattern dispatchPattern("Switch On Message");
    dispatchPattern.Ins(2).Outs(11).Pins(11).Pouts(0);
    const auto dispatch = root.Require(std::move(dispatchPattern));
    const auto branch = [&](int output) {
        return root.Next(dispatch.Out(output));
    };

    const auto startMenu = branch(0);
    root.Before(root.Leaving(startMenu),
                Callback<&Receiver::OnPreStartMenu>(receiver));
    root.After(startMenu.Out(),
               Callback<&Receiver::OnPostStartMenu>(receiver));

    const auto exitGame = branch(1);
    root.Before(root.Leaving(exitGame),
                Callback<&Receiver::OnExitGame>(receiver));

    const auto loadLevel = branch(2);
    root.Before(root.Leaving(Advance(root, loadLevel, 2)),
                Callback<&Receiver::OnPreLoadLevel>(receiver));
    root.After(loadLevel.Out(),
               Callback<&Receiver::OnPostLoadLevel>(receiver));
    root.After(branch(3).Out(), Callback<&Receiver::OnStartLevel>(receiver));

    auto reset = root.Require("reset Level").Graph();
    const auto resetFirst = reset.Next(reset.Root().In(0));
    const auto resetSecond = reset.Next(resetFirst);
    reset.Before(reset.Leaving(resetSecond),
                 Callback<&Receiver::OnPreResetLevel>(receiver));
    root.After(branch(4).Out(),
               Callback<&Receiver::OnPostResetLevel>(receiver));
    root.After(branch(5).Out(),
               Callback<&Receiver::OnPauseLevel>(receiver));
    root.After(branch(6).Out(),
               Callback<&Receiver::OnUnpauseLevel>(receiver));

    const auto exitBranches = root.Next(
        root.Require("DeleteCollisionSurfaces"));
    root.Before(root.Leaving(Advance(root, branch(7), 4)),
                Callback<&Receiver::OnPreExitLevel>(receiver));
    root.Before(root.Leaving(root.Next(exitBranches, 0)),
                Callback<&Receiver::OnPostExitLevel>(receiver));
    root.Before(root.Leaving(Advance(root, branch(8), 4)),
                Callback<&Receiver::OnPreNextLevel>(receiver));
    root.Before(root.Leaving(root.Next(exitBranches, 1)),
                Callback<&Receiver::OnPostNextLevel>(receiver));
    root.After(branch(9).Out(), Callback<&Receiver::OnDead>(receiver));

    const auto highscoreNode = root.Require("Highscore");
    root.Before(root.Leaving(branch(10)),
                Callback<&Receiver::OnPreEndLevel>(receiver));
    auto highscore = highscoreNode.Graph();
    const auto completed = highscore.AppendOut("Out");
    highscore.Flow(highscore.Each("Activate Script").Out(), completed);
    root.Tap(highscoreNode.Out("Out"),
             Callback<&Receiver::OnPostEndLevel>(receiver));
    return edit;
}

Behavior::Edit GameplayIngame(Receiver *receiver,
                              CKMessageType cameraOn,
                              CKMessageType cameraOff,
                              CKMessageType ballOn,
                              CKMessageType ballOff) {
    Behavior::Edit edit;
    auto root = edit.Root();
    auto camera = root.Require("CamNav On/Off").Graph();
    camera.Tap(WaitMessage(camera, cameraOn).Out(),
               Callback<&Receiver::OnCamNavActive>(receiver));
    camera.Tap(WaitMessage(camera, cameraOff).Out(),
               Callback<&Receiver::OnCamNavInactive>(receiver));
    auto ball = root.Require("BallNav On/Off").Graph();
    ball.Tap(WaitMessage(ball, ballOn).Out(),
             Callback<&Receiver::OnBallNavActive>(receiver));
    ball.Tap(WaitMessage(ball, ballOff).Out(),
             Callback<&Receiver::OnBallNavInactive>(receiver));
    return edit;
}

Behavior::Edit GameplayEnergy(Receiver *receiver,
                              CKMessageType lifeUpMessage,
                              CKMessageType ballOffMessage,
                              CKMessageType subLifeMessage,
                              CKMessageType extraPointMessage) {
    Behavior::Edit edit;
    auto root = edit.Root();
    const auto dispatch = root.Require("Switch On Message");
    root.Before(root.Leaving(dispatch, 3),
                Callback<&Receiver::OnCounterActive>(receiver));
    root.Before(root.Leaving(dispatch, 1),
                Callback<&Receiver::OnCounterInactive>(receiver));

    const auto lifeUpWait = WaitMessage(root, lifeUpMessage);
    const auto addLife = root.Next(
        lifeUpWait.Out(), Behavior::NodePattern("add Life"));
    const auto lifeUp = root.To(lifeUpWait.Out(), addLife);
    root.Before(lifeUp, Callback<&Receiver::OnPreLifeUp>(receiver));
    root.After(addLife.Out(), Callback<&Receiver::OnPostLifeUp>(receiver));

    const auto ballOffWait = WaitMessage(root, ballOffMessage);
    const auto delay = root.Next(
        ballOffWait.Out(), Behavior::NodePattern("Delayer"));
    root.Before(root.To(ballOffWait.Out(), delay),
                Callback<&Receiver::OnBallOff>(receiver));

    const auto subLifeWait = WaitMessage(root, subLifeMessage);
    const auto subLife = root.Next(
        subLifeWait.Out(), Behavior::NodePattern("sub Life"));
    const auto subtract = root.To(subLifeWait.Out(), subLife);
    root.Before(subtract, Callback<&Receiver::OnPreSubLife>(receiver));
    root.After(subLife.Out(), Callback<&Receiver::OnPostSubLife>(receiver));

    const auto extraPointWait = WaitMessage(root, extraPointMessage);
    const auto show = root.Next(
        extraPointWait.Out(), Behavior::NodePattern("Show"));
    root.Before(root.To(extraPointWait.Out(), show),
                Callback<&Receiver::OnExtraPoint>(receiver));
    return edit;
}

Behavior::Edit GameplayEvents(Receiver *receiver,
                              CKMessageType checkpointMessage,
                              CKMessageType gameOverMessage,
                              CKMessageType levelFinishMessage) {
    Behavior::Edit edit;
    auto root = edit.Root();

    const auto checkpointWait = WaitMessage(root, checkpointMessage);
    const auto resetpoint = root.Next(
        checkpointWait.Out(), Behavior::NodePattern("set Resetpoint"));
    const auto checkpoint = root.To(checkpointWait.Out(), resetpoint);
    root.Before(checkpoint,
                Callback<&Receiver::OnPreCheckpointReached>(receiver));
    root.After(resetpoint.Out(),
               Callback<&Receiver::OnPostCheckpointReached>(receiver));

    const auto gameOverWait = WaitMessage(root, gameOverMessage);
    const auto gameOverSend = root.Next(
        gameOverWait.Out(), Behavior::NodePattern("Send Message"));
    root.Before(root.To(gameOverWait.Out(), gameOverSend),
                Callback<&Receiver::OnGameOver>(receiver));
    const auto levelFinishWait = WaitMessage(root, levelFinishMessage);
    const auto levelFinishSend = root.Next(
        levelFinishWait.Out(), Behavior::NodePattern("Send Message"));
    root.Before(root.To(levelFinishWait.Out(), levelFinishSend),
                Callback<&Receiver::OnLevelFinish>(receiver));
    return edit;
}
} // namespace

void GameEventHooks::OnLoad(IBML &bml, ILogger &logger) {
    m_Receiver = &bml;
    m_Logger = &logger;
    CKMessageManager *messages = bml.GetMessageManager();
    if (!messages) {
        RejectPlan("the Virtools message manager is unavailable");
        return;
    }

    auto opened = Behavior::Session::Open("BML");
    if (!opened) {
        RejectPlan(opened.GetStatus().Message.empty()
            ? "the Behavior interface is unavailable"
            : opened.GetStatus().Message.c_str());
        return;
    }
    m_Behavior = opened.Take();

    Behavior::Edit eventHandler = BaseEventHandler(m_Receiver);
    Behavior::Edit ingame = GameplayIngame(
        m_Receiver,
        messages->AddMessageType("CamNav activate"),
        messages->AddMessageType("CamNav deactivate"),
        messages->AddMessageType("BallNav activate"),
        messages->AddMessageType("BallNav deactivate"));
    Behavior::Edit energy = GameplayEnergy(
        m_Receiver,
        messages->AddMessageType("Life_Up"),
        messages->AddMessageType("Ball Off"),
        messages->AddMessageType("Sub Life"),
        messages->AddMessageType("Extrapoint"));
    Behavior::Edit events = GameplayEvents(
        m_Receiver,
        messages->AddMessageType("Checkpoint reached"),
        messages->AddMessageType("Game Over"),
        messages->AddMessageType("Level_Finish"));

    auto plan = m_Behavior.Plan(
        "Game event hooks",
        Behavior::On(Behavior::Scripts::One("Event_handler"), eventHandler),
        Behavior::On(Behavior::Scripts::One("Gameplay_Ingame"), ingame),
        Behavior::On(Behavior::Scripts::One("Gameplay_Energy"), energy),
        Behavior::On(Behavior::Scripts::One("Gameplay_Events"), events));
    if (!plan) {
        RejectPlan(plan.GetStatus().Message.empty()
            ? "the game-event Plan could not be created"
            : plan.GetStatus().Message.c_str());
        return;
    }
    m_Plan = plan.Take();
    if (m_Logger)
        m_Logger->Info(
            "Install game lifecycle hooks through one Behavior Plan");
}

void GameEventHooks::OnUnload() {
    (void) m_Plan.Close();
    m_Behavior.Reset();
    m_Logger = nullptr;
    m_Receiver = nullptr;
}

void GameEventHooks::RejectPlan(const char *reason) const {
    if (m_Logger) {
        m_Logger->Error("Game event hooks are unavailable: %s",
                        reason ? reason : "unknown reason");
    }
}
