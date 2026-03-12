#ifndef BML_EVENT_TOPICS_H
#define BML_EVENT_TOPICS_H

/**
 * @file EventTopics.h
 * @brief Game Event Topic ID Constants for IMC Pub/Sub
 * 
 * This header defines all standard game event topic IDs that are published
 * by the event system. Mods can subscribe to these topics to receive
 * game lifecycle and gameplay event notifications.
 * 
 * Topic Naming Convention:
 * - "Game/Menu/*"        - Menu-related events (start menu, exit)
 * - "Game/Level/*"       - Level lifecycle events (load, start, reset, exit)
 * - "Game/Gameplay/*"    - Gameplay events (checkpoint, life, death)
 * - "Game/Ball/*"        - Ball-related events (active, inactive, off)
 * - "Game/Camera/*"      - Camera-related events (active, inactive)
 */

namespace BML_Event {
    // ============================================================================
    // Menu Events
    // ============================================================================

    /// @brief Posted before entering the start menu
    extern const char *const TOPIC_PRE_START_MENU;

    /// @brief Posted after entering the start menu
    extern const char *const TOPIC_POST_START_MENU;

    /// @brief Posted when exiting the game
    extern const char *const TOPIC_EXIT_GAME;

    // ============================================================================
    // Level Lifecycle Events
    // ============================================================================

    /// @brief Posted before loading a level
    extern const char *const TOPIC_PRE_LOAD_LEVEL;

    /// @brief Posted after loading a level
    extern const char *const TOPIC_POST_LOAD_LEVEL;

    /// @brief Posted when a level starts
    extern const char *const TOPIC_START_LEVEL;

    /// @brief Posted before resetting a level
    extern const char *const TOPIC_PRE_RESET_LEVEL;

    /// @brief Posted after resetting a level
    extern const char *const TOPIC_POST_RESET_LEVEL;

    /// @brief Posted when pausing a level
    extern const char *const TOPIC_PAUSE_LEVEL;

    /// @brief Posted when unpausing a level
    extern const char *const TOPIC_UNPAUSE_LEVEL;

    /// @brief Posted before exiting a level
    extern const char *const TOPIC_PRE_EXIT_LEVEL;

    /// @brief Posted after exiting a level
    extern const char *const TOPIC_POST_EXIT_LEVEL;

    /// @brief Posted before going to the next level
    extern const char *const TOPIC_PRE_NEXT_LEVEL;

    /// @brief Posted after going to the next level
    extern const char *const TOPIC_POST_NEXT_LEVEL;

    /// @brief Posted before ending a level
    extern const char *const TOPIC_PRE_END_LEVEL;

    /// @brief Posted after ending a level (highscore shown)
    extern const char *const TOPIC_POST_END_LEVEL;

    // ============================================================================
    // Gameplay Events
    // ============================================================================

    /// @brief Posted when the player dies
    extern const char *const TOPIC_DEAD;

    /// @brief Posted when the ball falls off
    extern const char *const TOPIC_BALL_OFF;

    /// @brief Posted when the counter becomes active
    extern const char *const TOPIC_COUNTER_ACTIVE;

    /// @brief Posted when the counter becomes inactive
    extern const char *const TOPIC_COUNTER_INACTIVE;

    /// @brief Posted before reaching a checkpoint
    extern const char *const TOPIC_PRE_CHECKPOINT;

    /// @brief Posted after reaching a checkpoint
    extern const char *const TOPIC_POST_CHECKPOINT;

    /// @brief Posted when the level is finished
    extern const char *const TOPIC_LEVEL_FINISH;

    /// @brief Posted on game over
    extern const char *const TOPIC_GAME_OVER;

    /// @brief Posted when collecting an extra point
    extern const char *const TOPIC_EXTRA_POINT;

    // ============================================================================
    // Life Events
    // ============================================================================

    /// @brief Posted before gaining a life
    extern const char *const TOPIC_PRE_LIFE_UP;

    /// @brief Posted after gaining a life
    extern const char *const TOPIC_POST_LIFE_UP;

    /// @brief Posted before losing a life
    extern const char *const TOPIC_PRE_SUB_LIFE;

    /// @brief Posted after losing a life
    extern const char *const TOPIC_POST_SUB_LIFE;

    // ============================================================================
    // Ball Navigation Events
    // ============================================================================

    /// @brief Posted when ball navigation becomes active
    extern const char *const TOPIC_BALL_NAV_ACTIVE;

    /// @brief Posted when ball navigation becomes inactive
    extern const char *const TOPIC_BALL_NAV_INACTIVE;

    // ============================================================================
    // Camera Navigation Events
    // ============================================================================

    /// @brief Posted when camera navigation becomes active
    extern const char *const TOPIC_CAM_NAV_ACTIVE;

    /// @brief Posted when camera navigation becomes inactive
    extern const char *const TOPIC_CAM_NAV_INACTIVE;

    // ============================================================================
    // Module API
    // ============================================================================

    /**
     * @brief Register all event topics with the IMC system
     * @return true on success, false on failure
     */
    bool RegisterEventTopics();

    /**
     * @brief Unregister all event topics
     */
    void UnregisterEventTopics();
} // namespace BML_Event

#endif // BML_EVENT_TOPICS_H
