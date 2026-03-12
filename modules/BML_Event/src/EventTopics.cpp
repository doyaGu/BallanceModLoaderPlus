#include "EventTopics.h"

namespace BML_Event {
    // ============================================================================
    // Menu Events
    // ============================================================================

    const char *const TOPIC_PRE_START_MENU = "Game/Menu/PreStartMenu";
    const char *const TOPIC_POST_START_MENU = "Game/Menu/PostStartMenu";
    const char *const TOPIC_EXIT_GAME = "Game/Menu/ExitGame";

    // ============================================================================
    // Level Lifecycle Events
    // ============================================================================

    const char *const TOPIC_PRE_LOAD_LEVEL = "Game/Level/PreLoad";
    const char *const TOPIC_POST_LOAD_LEVEL = "Game/Level/PostLoad";
    const char *const TOPIC_START_LEVEL = "Game/Level/Start";
    const char *const TOPIC_PRE_RESET_LEVEL = "Game/Level/PreReset";
    const char *const TOPIC_POST_RESET_LEVEL = "Game/Level/PostReset";
    const char *const TOPIC_PAUSE_LEVEL = "Game/Level/Pause";
    const char *const TOPIC_UNPAUSE_LEVEL = "Game/Level/Unpause";
    const char *const TOPIC_PRE_EXIT_LEVEL = "Game/Level/PreExit";
    const char *const TOPIC_POST_EXIT_LEVEL = "Game/Level/PostExit";
    const char *const TOPIC_PRE_NEXT_LEVEL = "Game/Level/PreNext";
    const char *const TOPIC_POST_NEXT_LEVEL = "Game/Level/PostNext";
    const char *const TOPIC_PRE_END_LEVEL = "Game/Level/PreEnd";
    const char *const TOPIC_POST_END_LEVEL = "Game/Level/PostEnd";

    // ============================================================================
    // Gameplay Events
    // ============================================================================

    const char *const TOPIC_DEAD = "Game/Gameplay/Dead";
    const char *const TOPIC_BALL_OFF = "Game/Gameplay/BallOff";
    const char *const TOPIC_COUNTER_ACTIVE = "Game/Gameplay/CounterActive";
    const char *const TOPIC_COUNTER_INACTIVE = "Game/Gameplay/CounterInactive";
    const char *const TOPIC_PRE_CHECKPOINT = "Game/Gameplay/PreCheckpoint";
    const char *const TOPIC_POST_CHECKPOINT = "Game/Gameplay/PostCheckpoint";
    const char *const TOPIC_LEVEL_FINISH = "Game/Gameplay/LevelFinish";
    const char *const TOPIC_GAME_OVER = "Game/Gameplay/GameOver";
    const char *const TOPIC_EXTRA_POINT = "Game/Gameplay/ExtraPoint";

    // ============================================================================
    // Life Events
    // ============================================================================

    const char *const TOPIC_PRE_LIFE_UP = "Game/Gameplay/PreLifeUp";
    const char *const TOPIC_POST_LIFE_UP = "Game/Gameplay/PostLifeUp";
    const char *const TOPIC_PRE_SUB_LIFE = "Game/Gameplay/PreSubLife";
    const char *const TOPIC_POST_SUB_LIFE = "Game/Gameplay/PostSubLife";

    // ============================================================================
    // Ball Navigation Events
    // ============================================================================

    const char *const TOPIC_BALL_NAV_ACTIVE = "Game/Ball/NavActive";
    const char *const TOPIC_BALL_NAV_INACTIVE = "Game/Ball/NavInactive";

    // ============================================================================
    // Camera Navigation Events
    // ============================================================================

    const char *const TOPIC_CAM_NAV_ACTIVE = "Game/Camera/NavActive";
    const char *const TOPIC_CAM_NAV_INACTIVE = "Game/Camera/NavInactive";

    // ============================================================================
    // Module API Implementation
    // ============================================================================

    // Topic registration is handled by the module entry point
    // These functions are placeholders for future extension

    bool RegisterEventTopics() {
        // Topics are registered on-demand when first used
        // The IMC system handles topic creation automatically
        return true;
    }

    void UnregisterEventTopics() {
        // Topics persist for the lifetime of the application
        // No explicit unregistration needed
    }
} // namespace BML_Event
