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

template<void (Receiver::*Method)()>
void Before(Behavior::Edit::Graph &graph, Behavior::Edit::Link link,
            Receiver *receiver) {
    graph.Before(std::move(link), Callback<Method>(receiver));
}

template<void (Receiver::*Method)()>
void Tap(Behavior::Edit::Graph &graph, Behavior::Edit::Node node,
         Receiver *receiver, Behavior::Selector output = Behavior::At(0)) {
    graph.Tap(node.Out(std::move(output)), Callback<Method>(receiver));
}

template<void (Receiver::*Method)()>
void After(Behavior::Edit::Graph &graph, Behavior::Edit::Node node,
           Receiver *receiver, Behavior::Selector output = Behavior::At(0)) {
    graph.After(node.Out(std::move(output)), Callback<Method>(receiver));
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
    Before<&Receiver::OnPreStartMenu>(
        root, root.Leaving(startMenu), receiver);
    After<&Receiver::OnPostStartMenu>(root, startMenu, receiver);

    const auto exitGame = branch(1);
    Before<&Receiver::OnExitGame>(root, root.Leaving(exitGame), receiver);

    const auto loadLevel = branch(2);
    Before<&Receiver::OnPreLoadLevel>(
        root, root.Leaving(Advance(root, loadLevel, 2)), receiver);
    After<&Receiver::OnPostLoadLevel>(root, loadLevel, receiver);
    After<&Receiver::OnStartLevel>(root, branch(3), receiver);

    auto reset = root.Require("reset Level").Graph();
    const auto resetFirst = reset.Next(reset.Root().In(0));
    const auto resetSecond = reset.Next(resetFirst);
    Before<&Receiver::OnPreResetLevel>(
        reset, reset.Leaving(resetSecond), receiver);
    After<&Receiver::OnPostResetLevel>(root, branch(4), receiver);
    After<&Receiver::OnPauseLevel>(root, branch(5), receiver);
    After<&Receiver::OnUnpauseLevel>(root, branch(6), receiver);

    const auto exitBranches = root.Next(
        root.Require("DeleteCollisionSurfaces"));
    Before<&Receiver::OnPreExitLevel>(
        root, root.Leaving(Advance(root, branch(7), 4)), receiver);
    Before<&Receiver::OnPostExitLevel>(
        root, root.Leaving(root.Next(exitBranches, 0)), receiver);
    Before<&Receiver::OnPreNextLevel>(
        root, root.Leaving(Advance(root, branch(8), 4)), receiver);
    Before<&Receiver::OnPostNextLevel>(
        root, root.Leaving(root.Next(exitBranches, 1)), receiver);
    After<&Receiver::OnDead>(root, branch(9), receiver);

    const auto highscoreNode = root.Require("Highscore");
    Before<&Receiver::OnPreEndLevel>(
        root, root.Leaving(branch(10)), receiver);
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
    Tap<&Receiver::OnCamNavActive>(
        camera, WaitMessage(camera, cameraOn), receiver);
    Tap<&Receiver::OnCamNavInactive>(
        camera, WaitMessage(camera, cameraOff), receiver);
    auto ball = root.Require("BallNav On/Off").Graph();
    Tap<&Receiver::OnBallNavActive>(
        ball, WaitMessage(ball, ballOn), receiver);
    Tap<&Receiver::OnBallNavInactive>(
        ball, WaitMessage(ball, ballOff), receiver);
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
    Before<&Receiver::OnCounterActive>(
        root, root.Leaving(dispatch, 3), receiver);
    Before<&Receiver::OnCounterInactive>(
        root, root.Leaving(dispatch, 1), receiver);

    const auto lifeUpWait = WaitMessage(root, lifeUpMessage);
    const auto addLife = root.Next(
        lifeUpWait.Out(), Behavior::NodePattern("add Life"));
    const auto lifeUp = root.To(lifeUpWait.Out(), addLife);
    Before<&Receiver::OnPreLifeUp>(root, lifeUp, receiver);
    After<&Receiver::OnPostLifeUp>(root, addLife, receiver);

    const auto ballOffWait = WaitMessage(root, ballOffMessage);
    const auto delay = root.Next(
        ballOffWait.Out(), Behavior::NodePattern("Delayer"));
    Before<&Receiver::OnBallOff>(
        root, root.To(ballOffWait.Out(), delay), receiver);

    const auto subLifeWait = WaitMessage(root, subLifeMessage);
    const auto subLife = root.Next(
        subLifeWait.Out(), Behavior::NodePattern("sub Life"));
    const auto subtract = root.To(subLifeWait.Out(), subLife);
    Before<&Receiver::OnPreSubLife>(root, subtract, receiver);
    After<&Receiver::OnPostSubLife>(root, subLife, receiver);

    const auto extraPointWait = WaitMessage(root, extraPointMessage);
    const auto show = root.Next(
        extraPointWait.Out(), Behavior::NodePattern("Show"));
    Before<&Receiver::OnExtraPoint>(
        root, root.To(extraPointWait.Out(), show), receiver);
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
    Before<&Receiver::OnPreCheckpointReached>(root, checkpoint, receiver);
    After<&Receiver::OnPostCheckpointReached>(root, resetpoint, receiver);

    const auto gameOverWait = WaitMessage(root, gameOverMessage);
    const auto gameOverSend = root.Next(
        gameOverWait.Out(), Behavior::NodePattern("Send Message"));
    Before<&Receiver::OnGameOver>(
        root, root.To(gameOverWait.Out(), gameOverSend), receiver);
    const auto levelFinishWait = WaitMessage(root, levelFinishMessage);
    const auto levelFinishSend = root.Next(
        levelFinishWait.Out(), Behavior::NodePattern("Send Message"));
    Before<&Receiver::OnLevelFinish>(
        root, root.To(levelFinishWait.Out(), levelFinishSend), receiver);
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
    m_Behavior.Close();
    m_Logger = nullptr;
    m_Receiver = nullptr;
}

void GameEventHooks::RejectPlan(const char *reason) const {
    if (m_Logger) {
        m_Logger->Error("Game event hooks are unavailable: %s",
                        reason ? reason : "unknown reason");
    }
}
