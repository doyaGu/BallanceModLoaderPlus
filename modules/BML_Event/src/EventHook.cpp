/**
 * @file EventHook.cpp
 * @brief Game Event Hook Implementation for BML_Event Module
 * 
 * Hooks game scripts to capture and publish game events via IMC.
 * Uses ScriptHelper for script manipulation and ModLoader's HookBlock BB.
 */

#include "EventHook.h"
#include "EventTopics.h"
#include "ScriptHelper.h"
#include "BML/Guids/Hooks.h"

#include "bml_loader.h"

namespace BML_Event {

using namespace ScriptHelper;

// Custom callback type matching HookBlock's expectation
typedef int (*CKBehaviorCallback)(const CKBehaviorContext *behcontext, void *arg);

//-----------------------------------------------------------------------------
// Static State
//-----------------------------------------------------------------------------

static CKContext *s_Context = nullptr;
static bool s_Initialized = false;

// Cached topic IDs for all events
static BML_TopicId s_Topics[40] = {0};

// Topic indices (matching EventTopics.h order)
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

//-----------------------------------------------------------------------------
// IMC Event Publishing Callback
//-----------------------------------------------------------------------------

struct HookCallbackData {
    int topicIndex;
};

static HookCallbackData s_CallbackData[IDX_MAX];

static int PublishEventCallback(const CKBehaviorContext *behcontext, void *arg) {
    (void)behcontext;
    
    auto *data = static_cast<HookCallbackData *>(arg);
    if (!data || data->topicIndex < 0 || data->topicIndex >= IDX_MAX)
        return CKBR_OK;
    
    BML_TopicId topic = s_Topics[data->topicIndex];
    if (topic != 0) {
        bmlImcPublish(topic, nullptr, 0);
    }
    
    return CKBR_OK;
}

//-----------------------------------------------------------------------------
// HookBlock Creation (using ModLoader's registered BB)
//-----------------------------------------------------------------------------

static CKBehavior *CreateHookBlock(CKBehavior *script, CKBehaviorCallback callback, void *arg,
                                   int inCount = 1, int outCount = 1) {
    CKBehavior *beh = CreateBB(script, HOOKS_HOOKBLOCK_GUID);
    if (!beh) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Event",
               "Failed to create HookBlock BB - ModLoader may not have registered it");
        return nullptr;
    }
    
    beh->SetLocalParameterValue(0, &callback);
    beh->SetLocalParameterValue(1, &arg);

    XString pinName = "In ";
    for (int i = 0; i < inCount; i++) {
        beh->CreateInput((pinName << i).Str());
    }

    XString poutName = "Out ";
    for (int i = 0; i < outCount; i++) {
        beh->CreateOutput((poutName << i).Str());
    }

    return beh;
}

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static CKBehavior *CreateEventPublisher(CKBehavior *script, int topicIndex) {
    if (topicIndex < 0 || topicIndex >= IDX_MAX)
        return nullptr;
    
    s_CallbackData[topicIndex].topicIndex = topicIndex;
    return CreateHookBlock(script, PublishEventCallback, &s_CallbackData[topicIndex]);
}

//-----------------------------------------------------------------------------
// Topic Registration
//-----------------------------------------------------------------------------

static void RegisterTopics() {
    bmlImcGetTopicId(TOPIC_PRE_START_MENU, &s_Topics[IDX_PRE_START_MENU]);
    bmlImcGetTopicId(TOPIC_POST_START_MENU, &s_Topics[IDX_POST_START_MENU]);
    bmlImcGetTopicId(TOPIC_EXIT_GAME, &s_Topics[IDX_EXIT_GAME]);
    bmlImcGetTopicId(TOPIC_PRE_LOAD_LEVEL, &s_Topics[IDX_PRE_LOAD_LEVEL]);
    bmlImcGetTopicId(TOPIC_POST_LOAD_LEVEL, &s_Topics[IDX_POST_LOAD_LEVEL]);
    bmlImcGetTopicId(TOPIC_START_LEVEL, &s_Topics[IDX_START_LEVEL]);
    bmlImcGetTopicId(TOPIC_PRE_RESET_LEVEL, &s_Topics[IDX_PRE_RESET_LEVEL]);
    bmlImcGetTopicId(TOPIC_POST_RESET_LEVEL, &s_Topics[IDX_POST_RESET_LEVEL]);
    bmlImcGetTopicId(TOPIC_PAUSE_LEVEL, &s_Topics[IDX_PAUSE_LEVEL]);
    bmlImcGetTopicId(TOPIC_UNPAUSE_LEVEL, &s_Topics[IDX_UNPAUSE_LEVEL]);
    bmlImcGetTopicId(TOPIC_PRE_EXIT_LEVEL, &s_Topics[IDX_PRE_EXIT_LEVEL]);
    bmlImcGetTopicId(TOPIC_POST_EXIT_LEVEL, &s_Topics[IDX_POST_EXIT_LEVEL]);
    bmlImcGetTopicId(TOPIC_PRE_NEXT_LEVEL, &s_Topics[IDX_PRE_NEXT_LEVEL]);
    bmlImcGetTopicId(TOPIC_POST_NEXT_LEVEL, &s_Topics[IDX_POST_NEXT_LEVEL]);
    bmlImcGetTopicId(TOPIC_PRE_END_LEVEL, &s_Topics[IDX_PRE_END_LEVEL]);
    bmlImcGetTopicId(TOPIC_POST_END_LEVEL, &s_Topics[IDX_POST_END_LEVEL]);
    bmlImcGetTopicId(TOPIC_DEAD, &s_Topics[IDX_DEAD]);
    bmlImcGetTopicId(TOPIC_BALL_OFF, &s_Topics[IDX_BALL_OFF]);
    bmlImcGetTopicId(TOPIC_COUNTER_ACTIVE, &s_Topics[IDX_COUNTER_ACTIVE]);
    bmlImcGetTopicId(TOPIC_COUNTER_INACTIVE, &s_Topics[IDX_COUNTER_INACTIVE]);
    bmlImcGetTopicId(TOPIC_PRE_CHECKPOINT, &s_Topics[IDX_PRE_CHECKPOINT]);
    bmlImcGetTopicId(TOPIC_POST_CHECKPOINT, &s_Topics[IDX_POST_CHECKPOINT]);
    bmlImcGetTopicId(TOPIC_LEVEL_FINISH, &s_Topics[IDX_LEVEL_FINISH]);
    bmlImcGetTopicId(TOPIC_GAME_OVER, &s_Topics[IDX_GAME_OVER]);
    bmlImcGetTopicId(TOPIC_EXTRA_POINT, &s_Topics[IDX_EXTRA_POINT]);
    bmlImcGetTopicId(TOPIC_PRE_LIFE_UP, &s_Topics[IDX_PRE_LIFE_UP]);
    bmlImcGetTopicId(TOPIC_POST_LIFE_UP, &s_Topics[IDX_POST_LIFE_UP]);
    bmlImcGetTopicId(TOPIC_PRE_SUB_LIFE, &s_Topics[IDX_PRE_SUB_LIFE]);
    bmlImcGetTopicId(TOPIC_POST_SUB_LIFE, &s_Topics[IDX_POST_SUB_LIFE]);
    bmlImcGetTopicId(TOPIC_BALL_NAV_ACTIVE, &s_Topics[IDX_BALL_NAV_ACTIVE]);
    bmlImcGetTopicId(TOPIC_BALL_NAV_INACTIVE, &s_Topics[IDX_BALL_NAV_INACTIVE]);
    bmlImcGetTopicId(TOPIC_CAM_NAV_ACTIVE, &s_Topics[IDX_CAM_NAV_ACTIVE]);
    bmlImcGetTopicId(TOPIC_CAM_NAV_INACTIVE, &s_Topics[IDX_CAM_NAV_INACTIVE]);
}

//-----------------------------------------------------------------------------
// Public API Implementation
//-----------------------------------------------------------------------------

bool InitEventHooks(CKContext *ctx) {
    if (s_Initialized)
        return true;
    
    if (!ctx) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Event",
               "Cannot initialize event hooks: CKContext is null");
        return false;
    }
    
    s_Context = ctx;
    
    // Register all topic IDs
    RegisterTopics();
    
    // Initialize callback data
    for (int i = 0; i < IDX_MAX; i++) {
        s_CallbackData[i].topicIndex = i;
    }
    
    s_Initialized = true;
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event",
           "Event hooks initialized");
    return true;
}

void ShutdownEventHooks() {
    s_Context = nullptr;
    s_Initialized = false;
    
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event",
           "Event hooks shutdown");
}

bool RegisterBaseEventHandler(CKBehavior *script) {
    if (!s_Initialized || !script) {
        return false;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event", 
           "Registering Level Manager hooks");

    // Hook PreStartMenu - before showing start menu
    CKBehavior *preStartMenu = FindFirstBB(script, "show_Title Menu");
    if (preStartMenu) {
        InsertBB(script, FindPreviousLink(script, preStartMenu),
                 CreateEventPublisher(script, IDX_PRE_START_MENU));
    }

    // Hook PostStartMenu - after start menu is shown
    CKBehavior *postStartMenu = FindFirstBB(script, "Title Menu Ingame");
    if (postStartMenu) {
        InsertBB(script, FindPreviousLink(script, postStartMenu),
                 CreateEventPublisher(script, IDX_POST_START_MENU));
    }

    // Hook ExitGame - when exiting game
    CKBehavior *exitGame = FindFirstBB(script, "Exit_To_System");
    if (exitGame) {
        InsertBB(script, FindPreviousLink(script, exitGame),
                 CreateEventPublisher(script, IDX_EXIT_GAME));
    }

    // Hook PreLoadLevel - before loading level
    CKBehavior *loadLevel = FindFirstBB(script, "load_levelinit_nmo");
    if (loadLevel) {
        InsertBB(script, FindPreviousLink(script, loadLevel),
                 CreateEventPublisher(script, IDX_PRE_LOAD_LEVEL));
    }

    // Hook PostLoadLevel - after level is loaded
    CKBehavior *postLoad = FindFirstBB(script, "Start_Level_01");
    if (postLoad) {
        InsertBB(script, FindPreviousLink(script, postLoad),
                 CreateEventPublisher(script, IDX_POST_LOAD_LEVEL));
    }

    // Hook StartLevel - when level starts
    CKBehavior *startLevel = FindFirstBB(script, "Ballrun_Continue");
    if (startLevel) {
        InsertBB(script, FindPreviousLink(script, startLevel),
                 CreateEventPublisher(script, IDX_START_LEVEL));
    }

    return true;
}

bool RegisterGameplayIngame(CKBehavior *script) {
    if (!s_Initialized || !script) {
        return false;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event", 
           "Registering Gameplay_Ingame hooks");

    // Hook Pause events
    CKBehavior *pauseMenu = FindFirstBB(script, "pause_on");
    if (pauseMenu) {
        InsertBB(script, FindPreviousLink(script, pauseMenu),
                 CreateEventPublisher(script, IDX_PAUSE_LEVEL));
    }

    CKBehavior *unpauseMenu = FindFirstBB(script, "pause_off");
    if (unpauseMenu) {
        InsertBB(script, FindPreviousLink(script, unpauseMenu),
                 CreateEventPublisher(script, IDX_UNPAUSE_LEVEL));
    }

    // Hook Reset level events
    CKBehavior *resetLevel = FindFirstBB(script, "restart_level");
    if (resetLevel) {
        InsertBB(script, FindPreviousLink(script, resetLevel),
                 CreateEventPublisher(script, IDX_PRE_RESET_LEVEL));
        CreateLink(script, resetLevel, CreateEventPublisher(script, IDX_POST_RESET_LEVEL));
    }

    // Hook Exit level events
    CKBehavior *exitLevel = FindFirstBB(script, "exit_level");
    if (exitLevel) {
        InsertBB(script, FindPreviousLink(script, exitLevel),
                 CreateEventPublisher(script, IDX_PRE_EXIT_LEVEL));
        CreateLink(script, exitLevel, CreateEventPublisher(script, IDX_POST_EXIT_LEVEL));
    }

    // Hook Dead event
    CKBehavior *dead = FindFirstBB(script, "Death_Reset");
    if (dead) {
        InsertBB(script, FindPreviousLink(script, dead),
                 CreateEventPublisher(script, IDX_DEAD));
    }

    // Hook Ball navigation toggle
    CKBehavior *ballNav = FindFirstBB(script, "Switch On Ball Navigation");
    if (ballNav) {
        InsertBB(script, FindNextLink(script, ballNav, nullptr, -1, 0),
                 CreateEventPublisher(script, IDX_BALL_NAV_ACTIVE));
        InsertBB(script, FindNextLink(script, ballNav, nullptr, -1, 1),
                 CreateEventPublisher(script, IDX_BALL_NAV_INACTIVE));
    }

    // Hook Camera navigation toggle
    CKBehavior *camNav = FindFirstBB(script, "Switch On Cam Navigation");
    if (camNav) {
        InsertBB(script, FindNextLink(script, camNav, nullptr, -1, 0),
                 CreateEventPublisher(script, IDX_CAM_NAV_ACTIVE));
        InsertBB(script, FindNextLink(script, camNav, nullptr, -1, 1),
                 CreateEventPublisher(script, IDX_CAM_NAV_INACTIVE));
    }

    return true;
}

bool RegisterGameplayEnergy(CKBehavior *script) {
    if (!s_Initialized || !script) {
        return false;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event", 
           "Registering Gameplay_Energy hooks");

    // Hook ball off events (falling off the map)
    CKBehavior *ballOff = FindFirstBB(script, "BallManager_ResetBall");
    if (ballOff) {
        InsertBB(script, FindPreviousLink(script, ballOff),
                 CreateEventPublisher(script, IDX_BALL_OFF));
    }

    return true;
}

bool RegisterGameplayEvents(CKBehavior *script) {
    if (!s_Initialized || !script) {
        return false;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Event", 
           "Registering Gameplay_Events hooks");

    // Hook counter events
    CKBehavior *counter = FindFirstBB(script, "Counter");
    if (counter) {
        InsertBB(script, FindNextLink(script, counter, nullptr, -1, 0),
                 CreateEventPublisher(script, IDX_COUNTER_ACTIVE));
        InsertBB(script, FindNextLink(script, counter, nullptr, -1, 1),
                 CreateEventPublisher(script, IDX_COUNTER_INACTIVE));
    }

    // Hook checkpoint events
    CKBehavior *checkpoint = FindFirstBB(script, "set_checkpoint");
    if (checkpoint) {
        InsertBB(script, FindPreviousLink(script, checkpoint),
                 CreateEventPublisher(script, IDX_PRE_CHECKPOINT));
        CreateLink(script, checkpoint, CreateEventPublisher(script, IDX_POST_CHECKPOINT));
    }

    // Hook extra point event
    CKBehavior *extraPoint = FindFirstBB(script, "Extra Point");
    if (extraPoint) {
        InsertBB(script, FindNextLink(script, extraPoint),
                 CreateEventPublisher(script, IDX_EXTRA_POINT));
    }

    // Hook life up events
    CKBehavior *lifeUp = FindFirstBB(script, "add_life");
    if (lifeUp) {
        InsertBB(script, FindPreviousLink(script, lifeUp),
                 CreateEventPublisher(script, IDX_PRE_LIFE_UP));
        CreateLink(script, lifeUp, CreateEventPublisher(script, IDX_POST_LIFE_UP));
    }

    // Hook sub life events
    CKBehavior *subLife = FindFirstBB(script, "sub_life");
    if (subLife) {
        InsertBB(script, FindPreviousLink(script, subLife),
                 CreateEventPublisher(script, IDX_PRE_SUB_LIFE));
        CreateLink(script, subLife, CreateEventPublisher(script, IDX_POST_SUB_LIFE));
    }

    return true;
}

void OnScriptLoaded(const char *filename, CKBehavior *script) {
    if (!s_Initialized || !script || !filename)
        return;

    const char *scriptName = script->GetName();
    if (!scriptName)
        return;

    // Check for known scripts and register appropriate hooks
    if (strcmp(scriptName, "base.nmo") == 0 || strstr(filename, "base.nmo")) {
        RegisterBaseEventHandler(script);
    } else if (strcmp(scriptName, "Gameplay_Ingame") == 0) {
        RegisterGameplayIngame(script);
    } else if (strcmp(scriptName, "Gameplay_Energy") == 0) {
        RegisterGameplayEnergy(script);
    } else if (strcmp(scriptName, "Gameplay_Events") == 0) {
        RegisterGameplayEvents(script);
    }
}

} // namespace BML_Event
