#include "EventTopics.h"

namespace BML_GameEvent {
    // ============================================================================
    // Menu Events
    // ============================================================================

    const char *const TOPIC_PRE_START_MENU = BML_TOPIC_GAME_MENU_PRE_START;
    const char *const TOPIC_POST_START_MENU = BML_TOPIC_GAME_MENU_POST_START;
    const char *const TOPIC_EXIT_GAME = BML_TOPIC_GAME_MENU_EXIT;

    // ============================================================================
    // Level Lifecycle Events
    // ============================================================================

    const char *const TOPIC_PRE_LOAD_LEVEL = BML_TOPIC_GAME_LEVEL_PRE_LOAD;
    const char *const TOPIC_POST_LOAD_LEVEL = BML_TOPIC_GAME_LEVEL_POST_LOAD;
    const char *const TOPIC_START_LEVEL = BML_TOPIC_GAME_LEVEL_START;
    const char *const TOPIC_PRE_RESET_LEVEL = BML_TOPIC_GAME_LEVEL_PRE_RESET;
    const char *const TOPIC_POST_RESET_LEVEL = BML_TOPIC_GAME_LEVEL_POST_RESET;
    const char *const TOPIC_PAUSE_LEVEL = BML_TOPIC_GAME_LEVEL_PAUSE;
    const char *const TOPIC_UNPAUSE_LEVEL = BML_TOPIC_GAME_LEVEL_UNPAUSE;
    const char *const TOPIC_PRE_EXIT_LEVEL = BML_TOPIC_GAME_LEVEL_PRE_EXIT;
    const char *const TOPIC_POST_EXIT_LEVEL = BML_TOPIC_GAME_LEVEL_POST_EXIT;
    const char *const TOPIC_PRE_NEXT_LEVEL = BML_TOPIC_GAME_LEVEL_PRE_NEXT;
    const char *const TOPIC_POST_NEXT_LEVEL = BML_TOPIC_GAME_LEVEL_POST_NEXT;
    const char *const TOPIC_PRE_END_LEVEL = BML_TOPIC_GAME_LEVEL_PRE_END;
    const char *const TOPIC_POST_END_LEVEL = BML_TOPIC_GAME_LEVEL_POST_END;

    // ============================================================================
    // Gameplay Events
    // ============================================================================

    const char *const TOPIC_DEAD = BML_TOPIC_GAME_PLAY_DEAD;
    const char *const TOPIC_BALL_OFF = BML_TOPIC_GAME_PLAY_BALL_OFF;
    const char *const TOPIC_COUNTER_ACTIVE = BML_TOPIC_GAME_PLAY_COUNTER_ACTIVE;
    const char *const TOPIC_COUNTER_INACTIVE = BML_TOPIC_GAME_PLAY_COUNTER_INACTIVE;
    const char *const TOPIC_PRE_CHECKPOINT = BML_TOPIC_GAME_PLAY_PRE_CHECKPOINT;
    const char *const TOPIC_POST_CHECKPOINT = BML_TOPIC_GAME_PLAY_POST_CHECKPOINT;
    const char *const TOPIC_LEVEL_FINISH = BML_TOPIC_GAME_PLAY_LEVEL_FINISH;
    const char *const TOPIC_GAME_OVER = BML_TOPIC_GAME_PLAY_GAME_OVER;
    const char *const TOPIC_EXTRA_POINT = BML_TOPIC_GAME_PLAY_EXTRA_POINT;

    // ============================================================================
    // Life Events
    // ============================================================================

    const char *const TOPIC_PRE_LIFE_UP = BML_TOPIC_GAME_PLAY_PRE_LIFE_UP;
    const char *const TOPIC_POST_LIFE_UP = BML_TOPIC_GAME_PLAY_POST_LIFE_UP;
    const char *const TOPIC_PRE_SUB_LIFE = BML_TOPIC_GAME_PLAY_PRE_SUB_LIFE;
    const char *const TOPIC_POST_SUB_LIFE = BML_TOPIC_GAME_PLAY_POST_SUB_LIFE;

    // ============================================================================
    // Ball Navigation Events
    // ============================================================================

    const char *const TOPIC_BALL_NAV_ACTIVE = BML_TOPIC_GAME_BALL_NAV_ACTIVE;
    const char *const TOPIC_BALL_NAV_INACTIVE = BML_TOPIC_GAME_BALL_NAV_INACTIVE;

    // ============================================================================
    // Camera Navigation Events
    // ============================================================================

    const char *const TOPIC_CAM_NAV_ACTIVE = BML_TOPIC_GAME_CAMERA_NAV_ACTIVE;
    const char *const TOPIC_CAM_NAV_INACTIVE = BML_TOPIC_GAME_CAMERA_NAV_INACTIVE;

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
} // namespace BML_GameEvent
