/**
 * @file EventMod.cpp
 * @brief BML_Event Module -- Game Event Hook and Module Entry Point
 *
 * Restores the original BML script hook topology while publishing IMC topics
 * instead of dispatching legacy IMessageReceiver callbacks.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "bml_virtools_payloads.h"

#include <cstring>
#include <unordered_set>

#include "BML/Guids/Hooks.h"
#include "BML/ScriptGraph.h"
#include "bml_services.hpp"
#include "bml_topics.h"
#include "EventTopics.h"

// ---------------------------------------------------------------------------
// File-scope hook helpers (formerly EventHook.cpp)
// ---------------------------------------------------------------------------

typedef int (*CKBehaviorCallback)(const CKBehaviorContext *behcontext, void *arg);

namespace {

const bml::ModuleServices *s_Services = nullptr;

enum TopicIndex {
    IDX_PRE_START_MENU = 0,
    IDX_POST_START_MENU,
    IDX_EXIT_GAME,
    IDX_PRE_LOAD_LEVEL,
    IDX_POST_LOAD_LEVEL,
    IDX_START_LEVEL,
    IDX_PRE_RESET_LEVEL,
    IDX_POST_RESET_LEVEL,
    IDX_PAUSE_LEVEL,
    IDX_UNPAUSE_LEVEL,
    IDX_PRE_EXIT_LEVEL,
    IDX_POST_EXIT_LEVEL,
    IDX_PRE_NEXT_LEVEL,
    IDX_POST_NEXT_LEVEL,
    IDX_PRE_END_LEVEL,
    IDX_POST_END_LEVEL,
    IDX_DEAD,
    IDX_BALL_OFF,
    IDX_COUNTER_ACTIVE,
    IDX_COUNTER_INACTIVE,
    IDX_PRE_CHECKPOINT,
    IDX_POST_CHECKPOINT,
    IDX_LEVEL_FINISH,
    IDX_GAME_OVER,
    IDX_EXTRA_POINT,
    IDX_PRE_LIFE_UP,
    IDX_POST_LIFE_UP,
    IDX_PRE_SUB_LIFE,
    IDX_POST_SUB_LIFE,
    IDX_BALL_NAV_ACTIVE,
    IDX_BALL_NAV_INACTIVE,
    IDX_CAM_NAV_ACTIVE,
    IDX_CAM_NAV_INACTIVE,
    IDX_MAX
};

struct HookCallbackData {
    int topicIndex;
};

static CKContext *s_Context = nullptr;
static bool s_Initialized = false;
static BML_TopicId s_Topics[IDX_MAX] = {};
static HookCallbackData s_CallbackData[IDX_MAX] = {};

// Retained state topic IDs
static BML_TopicId s_TopicGamePhase = 0;
static BML_TopicId s_TopicPaused = 0;

static void PublishRetainedState(const BML_ImcBusInterface *imc, BML_TopicId topic,
                                  const void *data, size_t size) {
    if (!imc || !imc->PublishState || !s_Services || topic == 0) return;
    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    msg.data = data;
    msg.size = size;
    imc->PublishState(s_Services->Handle(), topic, &msg);
}

static void UpdateGamePhase(BML_GamePhase phase) {
    if (!s_Services) return;
    auto *imcBus = s_Services->Interfaces().ImcBus;
    uint32_t value = static_cast<uint32_t>(phase);
    PublishRetainedState(imcBus, s_TopicGamePhase, &value, sizeof(value));
}

static void UpdatePaused(BML_Bool paused) {
    if (!s_Services) return;
    auto *imcBus = s_Services->Interfaces().ImcBus;
    PublishRetainedState(imcBus, s_TopicPaused, &paused, sizeof(paused));
}

static std::unordered_set<CK_ID> s_BaseScripts;
static std::unordered_set<CK_ID> s_GameplayIngameScripts;
static std::unordered_set<CK_ID> s_GameplayEnergyScripts;
static std::unordered_set<CK_ID> s_GameplayEventsScripts;

static int PublishEventCallback(const CKBehaviorContext *behcontext, void *arg) {
    (void)behcontext;

    auto *data = static_cast<HookCallbackData *>(arg);
    if (!data || data->topicIndex < 0 || data->topicIndex >= IDX_MAX) {
        return CKBR_OK;
    }

    const BML_TopicId topic = s_Topics[data->topicIndex];
    auto *imcBus = s_Services ? s_Services->Interfaces().ImcBus : nullptr;
    if (topic != 0 && imcBus) {
        if (imcBus->Publish && s_Services->Handle()) {
            imcBus->Publish(s_Services->Handle(), topic, nullptr, 0);
        }

        // Update retained game state based on event type
        switch (data->topicIndex) {
        case IDX_POST_START_MENU:
            UpdateGamePhase(BML_GAME_PHASE_MENU);
            break;
        case IDX_PRE_LOAD_LEVEL:
            UpdateGamePhase(BML_GAME_PHASE_LOADING);
            break;
        case IDX_START_LEVEL:
            UpdateGamePhase(BML_GAME_PHASE_PLAYING);
            UpdatePaused(BML_FALSE);
            break;
        case IDX_PAUSE_LEVEL:
            UpdateGamePhase(BML_GAME_PHASE_PAUSED);
            UpdatePaused(BML_TRUE);
            break;
        case IDX_UNPAUSE_LEVEL:
            UpdateGamePhase(BML_GAME_PHASE_PLAYING);
            UpdatePaused(BML_FALSE);
            break;
        case IDX_POST_EXIT_LEVEL:
            UpdateGamePhase(BML_GAME_PHASE_MENU);
            UpdatePaused(BML_FALSE);
            break;
        default:
            break;
        }
    }

    return CKBR_OK;
}

static CKBehavior *CreateHookBlock(bml::Graph &graph, CKBehaviorCallback callback, void *arg,
                                   int inCount = 1, int outCount = 1) {
    CKBehavior *behavior = graph.CreateBehavior(HOOKS_HOOKBLOCK_GUID);
    if (!behavior) {
        s_Services->Log().Error("Failed to create HookBlock BB");
        return nullptr;
    }

    behavior->SetLocalParameterValue(0, &callback);
    behavior->SetLocalParameterValue(1, &arg);

    XString inputName = "In ";
    for (int index = 0; index < inCount; ++index) {
        behavior->CreateInput((inputName << index).Str());
    }

    XString outputName = "Out ";
    for (int index = 0; index < outCount; ++index) {
        behavior->CreateOutput((outputName << index).Str());
    }

    return behavior;
}

static CKBehavior *CreateEventPublisher(bml::Graph &graph, int topicIndex) {
    if (topicIndex < 0 || topicIndex >= IDX_MAX) {
        return nullptr;
    }

    s_CallbackData[topicIndex].topicIndex = topicIndex;
    return CreateHookBlock(graph, PublishEventCallback, &s_CallbackData[topicIndex]);
}

static bool MarkRegistered(std::unordered_set<CK_ID> &registry, CKBehavior *script) {
    if (!script) {
        return false;
    }

    const CK_ID id = script->GetID();
    return registry.insert(id).second;
}

static void InsertPublisher(bml::Graph &graph, CKBehaviorLink *link, int topicIndex) {
    if (!link) return;
    CKBehavior *publisher = CreateEventPublisher(graph, topicIndex);
    if (!publisher) return;
    graph.Insert(link, publisher);
}

static void LinkPublisher(bml::Graph &graph, CKBehavior *source, int topicIndex,
                          int inPos = 0, int outPos = 0, int delay = 0) {
    if (!source) return;
    CKBehavior *publisher = CreateEventPublisher(graph, topicIndex);
    if (!publisher) return;
    graph.Link(source, publisher, inPos, outPos, delay);
}

static void RegisterTopics() {
    auto *imcBus = s_Services ? s_Services->Interfaces().ImcBus : nullptr;
    if (!imcBus || !imcBus->GetTopicId) {
        return;
    }

    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_START_MENU, &s_Topics[IDX_PRE_START_MENU]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_START_MENU, &s_Topics[IDX_POST_START_MENU]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_EXIT_GAME, &s_Topics[IDX_EXIT_GAME]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_LOAD_LEVEL, &s_Topics[IDX_PRE_LOAD_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_LOAD_LEVEL, &s_Topics[IDX_POST_LOAD_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_START_LEVEL, &s_Topics[IDX_START_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_RESET_LEVEL, &s_Topics[IDX_PRE_RESET_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_RESET_LEVEL, &s_Topics[IDX_POST_RESET_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PAUSE_LEVEL, &s_Topics[IDX_PAUSE_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_UNPAUSE_LEVEL, &s_Topics[IDX_UNPAUSE_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_EXIT_LEVEL, &s_Topics[IDX_PRE_EXIT_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_EXIT_LEVEL, &s_Topics[IDX_POST_EXIT_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_NEXT_LEVEL, &s_Topics[IDX_PRE_NEXT_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_NEXT_LEVEL, &s_Topics[IDX_POST_NEXT_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_END_LEVEL, &s_Topics[IDX_PRE_END_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_END_LEVEL, &s_Topics[IDX_POST_END_LEVEL]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_DEAD, &s_Topics[IDX_DEAD]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_BALL_OFF, &s_Topics[IDX_BALL_OFF]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_COUNTER_ACTIVE, &s_Topics[IDX_COUNTER_ACTIVE]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_COUNTER_INACTIVE, &s_Topics[IDX_COUNTER_INACTIVE]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_CHECKPOINT, &s_Topics[IDX_PRE_CHECKPOINT]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_CHECKPOINT, &s_Topics[IDX_POST_CHECKPOINT]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_LEVEL_FINISH, &s_Topics[IDX_LEVEL_FINISH]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_GAME_OVER, &s_Topics[IDX_GAME_OVER]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_EXTRA_POINT, &s_Topics[IDX_EXTRA_POINT]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_LIFE_UP, &s_Topics[IDX_PRE_LIFE_UP]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_LIFE_UP, &s_Topics[IDX_POST_LIFE_UP]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_PRE_SUB_LIFE, &s_Topics[IDX_PRE_SUB_LIFE]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_POST_SUB_LIFE, &s_Topics[IDX_POST_SUB_LIFE]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_BALL_NAV_ACTIVE, &s_Topics[IDX_BALL_NAV_ACTIVE]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_BALL_NAV_INACTIVE, &s_Topics[IDX_BALL_NAV_INACTIVE]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_CAM_NAV_ACTIVE, &s_Topics[IDX_CAM_NAV_ACTIVE]);
    imcBus->GetTopicId(imcBus->Context, BML_Event::TOPIC_CAM_NAV_INACTIVE, &s_Topics[IDX_CAM_NAV_INACTIVE]);

    // Retained state topics
    imcBus->GetTopicId(imcBus->Context, BML_TOPIC_STATE_GAME_PHASE, &s_TopicGamePhase);
    imcBus->GetTopicId(imcBus->Context, BML_TOPIC_STATE_PAUSED, &s_TopicPaused);

    // Publish initial state
    UpdateGamePhase(BML_GAME_PHASE_IDLE);
    UpdatePaused(BML_FALSE);
}

static bool IsScriptBehavior(CKObject *object) {
    if (!object || object->GetClassID() != CKCID_BEHAVIOR) {
        return false;
    }

    auto *behavior = static_cast<CKBehavior *>(object);
    return (behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) != 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// BML_Event namespace -- script registration functions
// ---------------------------------------------------------------------------

namespace BML_Event {

static int ScanLoadedScripts(CKContext *ctx) {
    if (!ctx || !s_Services) {
        return 0;
    }

    const int behaviorCount = ctx->GetObjectsCountByClassID(CKCID_BEHAVIOR);
    CK_ID *behaviorIds = ctx->GetObjectsListByClassID(CKCID_BEHAVIOR);
    if (behaviorCount <= 0 || !behaviorIds) {
        return 0;
    }

    int processed = 0;
    for (int index = 0; index < behaviorCount; ++index) {
        CKObject *object = ctx->GetObject(behaviorIds[index]);
        if (!IsScriptBehavior(object)) {
            continue;
        }

        OnScriptLoaded("base.cmo", static_cast<CKBehavior *>(object));
        ++processed;
    }

    s_Services->Log().Info("Scanned %d loaded scripts for event hook registration", processed);
    return processed;
}

static bool RegisterBaseEventHandler(CKBehavior *script) {
    if (!s_Initialized || !script || !MarkRegistered(s_BaseScripts, script)) {
        return false;
    }

    bml::Graph graph(script);
    auto sw = graph.Find("Switch On Message", false, 2, 11, 11, 0);
    if (!sw) {
        s_Services->Log().Warn("Failed to locate base event handler switch block");
        return false;
    }

    InsertPublisher(graph, graph.From(sw).Out(0).Next().NextLink(), IDX_PRE_START_MENU);
    LinkPublisher(graph, graph.From(sw).Out(0).Next().End(), IDX_POST_START_MENU);

    InsertPublisher(graph, graph.From(sw).Out(1).Next().NextLink(), IDX_EXIT_GAME);

    auto loadLevelChain = graph.From(sw).Out(2).Next().Next().Next();
    InsertPublisher(graph, loadLevelChain.NextLink(), IDX_PRE_LOAD_LEVEL);
    LinkPublisher(graph, graph.From(sw).Out(2).Next().End(), IDX_POST_LOAD_LEVEL);

    LinkPublisher(graph, graph.From(sw).Out(3).Next().End(), IDX_START_LEVEL);

    auto resetLevel = graph.Find("reset Level");
    if (resetLevel) {
        bml::Graph rl(resetLevel);
        auto resetChain = rl.From(resetLevel->GetInput(0)).Next().Next();
        InsertPublisher(rl, resetChain.NextLink(), IDX_PRE_RESET_LEVEL);
        LinkPublisher(graph, graph.From(sw).Out(4).Next().End(), IDX_POST_RESET_LEVEL);
    }

    LinkPublisher(graph, graph.From(sw).Out(5).Next().End(), IDX_PAUSE_LEVEL);
    LinkPublisher(graph, graph.From(sw).Out(6).Next().End(), IDX_UNPAUSE_LEVEL);

    auto deleteCS = graph.Find("DeleteCollisionSurfaces");
    CKBehavior *bs = deleteCS ? graph.From(deleteCS).Next() : nullptr;
    auto exitLevelChain = graph.From(sw).Out(7).Skip(5);
    InsertPublisher(graph, exitLevelChain.NextLink(), IDX_PRE_EXIT_LEVEL);
    if (bs) {
        InsertPublisher(graph, graph.From(bs).Next(nullptr, 0).NextLink(), IDX_POST_EXIT_LEVEL);
    }

    auto nextLevelChain = graph.From(sw).Out(8).Skip(5);
    InsertPublisher(graph, nextLevelChain.NextLink(), IDX_PRE_NEXT_LEVEL);
    if (bs) {
        InsertPublisher(graph, graph.From(bs).Next(nullptr, 1).NextLink(), IDX_POST_NEXT_LEVEL);
    }

    LinkPublisher(graph, graph.From(sw).Out(9).Next().End(), IDX_DEAD);

    auto highscore = graph.Find("Highscore");
    if (highscore) {
        highscore->AddOutput("Out");
        bml::Graph hsGraph(highscore);
        highscore.FindAll([&hsGraph](CKBehavior *behavior) {
            hsGraph.Link(behavior, hsGraph.Raw()->GetOutput(0));
            return true;
        }, "Activate Script");

        InsertPublisher(graph, graph.From(sw).Out(10).Next().NextLink(), IDX_PRE_END_LEVEL);
        LinkPublisher(graph, highscore, IDX_POST_END_LEVEL);
    }

    return true;
}

static bool RegisterGameplayIngame(CKBehavior *script) {
    if (!s_Initialized || !script || !MarkRegistered(s_GameplayIngameScripts, script)) {
        return false;
    }

    bml::Graph graph(script);
    auto camNavOnOff = graph.Find("CamNav On/Off");
    auto ballNavOnOff = graph.Find("BallNav On/Off");
    CKMessageManager *messageManager = s_Context ? s_Context->GetMessageManager() : nullptr;
    if (!camNavOnOff || !ballNavOnOff || !messageManager) {
        s_Services->Log().Warn("Failed to locate gameplay ingame navigation blocks");
        return false;
    }

    CKMessageType camOn = messageManager->AddMessageType("CamNav activate");
    CKMessageType camOff = messageManager->AddMessageType("CamNav deactivate");
    CKMessageType ballOn = messageManager->AddMessageType("BallNav activate");
    CKMessageType ballOff = messageManager->AddMessageType("BallNav deactivate");

    CKBehavior *camOnWait = nullptr;
    CKBehavior *camOffWait = nullptr;
    camNavOnOff.FindAll([camOn, camOff, &camOnWait, &camOffWait](CKBehavior *behavior) {
        auto message = bml::GetParam<CKMessageType>(behavior->GetInputParameter(0)->GetDirectSource());
        if (message == camOn) camOnWait = behavior;
        if (message == camOff) camOffWait = behavior;
        return true;
    }, "Wait Message");

    CKBehavior *ballOnWait = nullptr;
    CKBehavior *ballOffWait = nullptr;
    ballNavOnOff.FindAll([ballOn, ballOff, &ballOnWait, &ballOffWait](CKBehavior *behavior) {
        auto message = bml::GetParam<CKMessageType>(behavior->GetInputParameter(0)->GetDirectSource());
        if (message == ballOn) ballOnWait = behavior;
        if (message == ballOff) ballOffWait = behavior;
        return true;
    }, "Wait Message");

    bml::Graph camGraph(camNavOnOff);
    bml::Graph ballGraph(ballNavOnOff);

    if (camOnWait) {
        CKBehavior *publisher = CreateEventPublisher(camGraph, IDX_CAM_NAV_ACTIVE);
        if (publisher) camGraph.Link(camOnWait, publisher);
    }
    if (camOffWait) {
        CKBehavior *publisher = CreateEventPublisher(camGraph, IDX_CAM_NAV_INACTIVE);
        if (publisher) camGraph.Link(camOffWait, publisher);
    }
    if (ballOnWait) {
        CKBehavior *publisher = CreateEventPublisher(ballGraph, IDX_BALL_NAV_ACTIVE);
        if (publisher) ballGraph.Link(ballOnWait, publisher);
    }
    if (ballOffWait) {
        CKBehavior *publisher = CreateEventPublisher(ballGraph, IDX_BALL_NAV_INACTIVE);
        if (publisher) ballGraph.Link(ballOffWait, publisher);
    }

    return true;
}

static bool RegisterGameplayEnergy(CKBehavior *script) {
    if (!s_Initialized || !script || !MarkRegistered(s_GameplayEnergyScripts, script)) {
        return false;
    }

    bml::Graph graph(script);
    auto switchOnMessage = graph.Find("Switch On Message");
    if (switchOnMessage) {
        InsertPublisher(graph, graph.From(switchOnMessage).NextLink(nullptr, 3), IDX_COUNTER_ACTIVE);
        InsertPublisher(graph, graph.From(switchOnMessage).NextLink(nullptr, 1), IDX_COUNTER_INACTIVE);
    }

    CKMessageManager *messageManager = s_Context ? s_Context->GetMessageManager() : nullptr;
    if (!messageManager) {
        return switchOnMessage != nullptr;
    }

    CKMessageType lifeUp = messageManager->AddMessageType("Life_Up");
    CKMessageType ballOff = messageManager->AddMessageType("Ball Off");
    CKMessageType subLife = messageManager->AddMessageType("Sub Life");
    CKMessageType extraPoint = messageManager->AddMessageType("Extrapoint");

    CKBehavior *lifeUpWait = nullptr;
    CKBehavior *ballOffWait = nullptr;
    CKBehavior *subLifeWait = nullptr;
    CKBehavior *extraPointWait = nullptr;
    graph.FindAll([lifeUp, ballOff, subLife, extraPoint,
                   &lifeUpWait, &ballOffWait, &subLifeWait, &extraPointWait](CKBehavior *behavior) {
        auto message = bml::GetParam<CKMessageType>(behavior->GetInputParameter(0)->GetDirectSource());
        if (message == lifeUp) lifeUpWait = behavior;
        if (message == ballOff) ballOffWait = behavior;
        if (message == subLife) subLifeWait = behavior;
        if (message == extraPoint) extraPointWait = behavior;
        return true;
    }, "Wait Message");

    if (lifeUpWait) {
        CKBehavior *preLifeUp = CreateEventPublisher(graph, IDX_PRE_LIFE_UP);
        CKBehaviorLink *link = graph.From(lifeUpWait).NextLink("add Life");
        if (preLifeUp && link) {
            graph.Insert(link, preLifeUp);
            LinkPublisher(graph, graph.From(preLifeUp).End(), IDX_POST_LIFE_UP);
        }
    }

    if (ballOffWait) {
        InsertPublisher(graph, graph.From(ballOffWait).NextLink("Delayer"), IDX_BALL_OFF);
    }

    if (subLifeWait) {
        CKBehavior *preSubLife = CreateEventPublisher(graph, IDX_PRE_SUB_LIFE);
        CKBehaviorLink *link = graph.From(subLifeWait).NextLink("sub Life");
        if (preSubLife && link) {
            graph.Insert(link, preSubLife);
            LinkPublisher(graph, graph.From(preSubLife).End(), IDX_POST_SUB_LIFE);
        }
    }

    if (extraPointWait) {
        InsertPublisher(graph, graph.From(extraPointWait).NextLink("Show"), IDX_EXTRA_POINT);
    }

    return true;
}

static bool RegisterGameplayEvents(CKBehavior *script) {
    if (!s_Initialized || !script || !MarkRegistered(s_GameplayEventsScripts, script)) {
        return false;
    }

    CKMessageManager *messageManager = s_Context ? s_Context->GetMessageManager() : nullptr;
    if (!messageManager) {
        s_Services->Log().Warn("Cannot register gameplay events without message manager");
        return false;
    }

    bml::Graph graph(script);

    CKMessageType checkpoint = messageManager->AddMessageType("Checkpoint reached");
    CKMessageType gameOver = messageManager->AddMessageType("Game Over");
    CKMessageType levelFinish = messageManager->AddMessageType("Level_Finish");

    CKBehavior *checkpointWait = nullptr;
    CKBehavior *gameOverWait = nullptr;
    CKBehavior *levelFinishWait = nullptr;
    graph.FindAll([checkpoint, gameOver, levelFinish,
                   &checkpointWait, &gameOverWait, &levelFinishWait](CKBehavior *behavior) {
        auto message = bml::GetParam<CKMessageType>(behavior->GetInputParameter(0)->GetDirectSource());
        if (message == checkpoint) checkpointWait = behavior;
        if (message == gameOver) gameOverWait = behavior;
        if (message == levelFinish) levelFinishWait = behavior;
        return true;
    }, "Wait Message");

    if (checkpointWait) {
        CKBehavior *preCheckpoint = CreateEventPublisher(graph, IDX_PRE_CHECKPOINT);
        CKBehaviorLink *link = graph.From(checkpointWait).NextLink("set Resetpoint");
        if (preCheckpoint && link) {
            graph.Insert(link, preCheckpoint);
            LinkPublisher(graph, graph.From(preCheckpoint).End(), IDX_POST_CHECKPOINT);
        }
    }

    if (gameOverWait) {
        InsertPublisher(graph, graph.From(gameOverWait).NextLink("Send Message"), IDX_GAME_OVER);
    }

    if (levelFinishWait) {
        InsertPublisher(graph, graph.From(levelFinishWait).NextLink("Send Message"), IDX_LEVEL_FINISH);
    }

    return true;
}

static void OnScriptLoaded(const char *filename, CKBehavior *script) {
    if (!s_Initialized || !script) {
        return;
    }

    const char *scriptName = script->GetName();
    if (!scriptName) {
        return;
    }

    if (std::strcmp(scriptName, "Event_handler") == 0 ||
        (filename && std::strstr(filename, "base.nmo") != nullptr) ||
        std::strcmp(scriptName, "Level Manager") == 0 ||
        std::strcmp(scriptName, "Gameplay.nmo Level Manager") == 0) {
        RegisterBaseEventHandler(script);
    } else if (std::strcmp(scriptName, "Gameplay_Ingame") == 0) {
        RegisterGameplayIngame(script);
    } else if (std::strcmp(scriptName, "Gameplay_Energy") == 0) {
        RegisterGameplayEnergy(script);
    } else if (std::strcmp(scriptName, "Gameplay_Events") == 0) {
        RegisterGameplayEvents(script);
    }
}

} // namespace BML_Event

// ---------------------------------------------------------------------------
// EventMod -- module entry point
// ---------------------------------------------------------------------------

class EventMod : public bml::HookModule {
    bool m_ScanCompleted = false;

    const char *HookLogCategory() const override { return "BML_Event"; }

    void EnsureScanned(CKContext *ctx) {
        if (m_ScanCompleted || !ctx) return;
        if (BML_Event::ScanLoadedScripts(ctx) > 0) {
            m_ScanCompleted = true;
        }
    }

    bool InitHook(CKContext *ctx) override {
        if (s_Initialized) {
            return true;
        }

        s_Services = &Services();

        if (!ctx) {
            s_Services->Log().Error("Cannot initialize event hooks: CKContext is null");
            return false;
        }

        s_Context = ctx;
        RegisterTopics();
        for (int index = 0; index < IDX_MAX; ++index) {
            s_CallbackData[index].topicIndex = index;
        }

        s_Initialized = true;
        s_Services->Log().Info("Event hooks initialized");

        EnsureScanned(ctx);
        return true;
    }

    void ShutdownHook() override {
        s_Services->Log().Info("Event hooks shutdown");

        s_Context = nullptr;
        s_Initialized = false;
        s_TopicGamePhase = 0;
        s_TopicPaused = 0;
        s_BaseScripts.clear();
        s_GameplayIngameScripts.clear();
        s_GameplayEnergyScripts.clear();
        s_GameplayEventsScripts.clear();
        s_Services = nullptr;
    }

    BML_Result OnModuleAttach() override {
        // Eager init
        CKContext *ctx = bml::virtools::GetCKContext(Services());
        TryInitHook(ctx);

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &msg) {
            auto *payload = msg.As<BML_ScriptLoadEvent>();
            if (!payload || !payload->script) return;

            CKContext *context = payload->script->GetCKContext();
            if (!context) return;
            TryInitHook(context);
            BML_Event::OnScriptLoaded(payload->filename, payload->script);
        });

        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
            if (!payload) return;
            TryInitHook(payload->context);
        });

        return BML_RESULT_OK;
    }

    void OnModuleDetach() override {
        m_ScanCompleted = false;
    }
};

BML_DEFINE_MODULE(EventMod)
