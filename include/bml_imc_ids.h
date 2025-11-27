#ifndef BML_IMC_IDS_H
#define BML_IMC_IDS_H

/**
 * @file bml_imc_ids.h
 * @brief Pre-registered Topic/RPC identifiers for well-known system events
 * 
 * ============================================================================
 * PRE-REGISTERED TOPIC/RPC IDS
 * ============================================================================
 * 
 * These IDs are pre-registered by the IMC system at startup. They provide
 * stable, well-known identifiers for common system events and RPCs.
 * 
 * Why Pre-registered IDs?
 * - Mods can subscribe to system events without string lookup overhead
 * - Compile-time constant IDs enable switch statements and static dispatch
 * - Binary compatibility: mods work across BML versions without recompilation
 * - Zero-cost abstraction: no runtime string hashing for common events
 * 
 * ID Allocation Ranges:
 * - Range 1-999:       BML internal (system lifecycle, module events)
 * - Range 1000-1999:   Core gameplay (level, player, objects)
 * - Range 2000-2999:   Physics/collision
 * - Range 3000-3999:   Input/camera
 * - Range 4000-4999:   UI/HUD
 * - Range 5000-5999:   Mod management
 * - Range 6000-9999:   Reserved for future use
 * - Range 10000-19999: Pre-registered RPCs
 * - Range 20000+:      Dynamic allocation (runtime GetTopicId/GetRpcId)
 * 
 * Usage:
 *   // Subscribe using pre-registered ID (fastest)
 *   bmlImcSubscribe(BML_TOPIC_ID_OnLoadLevel, handler, user_data, &sub);
 *   
 *   // Or get ID by name (for custom topics)
 *   BML_TopicId topic;
 *   bmlImcGetTopicId("my.custom.topic", &topic);
 * 
 * @see docs/stable-id/IMC_ID_STABILITY.md for detailed rules
 */

#include "bml_types.h"

/* ========================================================================
 * Special Constants
 * ======================================================================== */

/** @brief Invalid Topic/RPC ID (never assigned) */
#define BML_IMC_INVALID_ID          0u

/** @brief First ID available for dynamic allocation */
#define BML_IMC_DYNAMIC_ID_START    20000u

/* ========================================================================
 * BML Internal Topics (1-999)
 * 
 * System-level events published by the BML runtime.
 * ======================================================================== */

/** @brief System lifecycle - published during BML startup/shutdown */
#define BML_TOPIC_ID_PreStartup         1u   /**< Before module loading */
#define BML_TOPIC_ID_PostStartup        2u   /**< After all modules loaded */
#define BML_TOPIC_ID_PreShutdown        3u   /**< Before module unloading */
#define BML_TOPIC_ID_PostShutdown       4u   /**< After all modules unloaded */

/** @brief Module lifecycle - published when mods load/unload */
#define BML_TOPIC_ID_ModLoaded          10u  /**< Payload: BML_ModInfo* */
#define BML_TOPIC_ID_ModUnloaded        11u  /**< Payload: BML_ModInfo* */
#define BML_TOPIC_ID_ModError           12u  /**< Payload: BML_ModError* */

/* ========================================================================
 * Core Gameplay Events (1000-1999)
 * 
 * Game state events published by the Ballance runtime hooks.
 * ======================================================================== */

/** @brief Level lifecycle (1000-1009) */
#define BML_TOPIC_ID_OnPreLoadLevel     1000u  /**< Before level file loads */
#define BML_TOPIC_ID_OnLoadLevel        1001u  /**< Level file loaded */
#define BML_TOPIC_ID_OnPostLoadLevel    1002u  /**< Level fully initialized */
#define BML_TOPIC_ID_OnStartLevel       1003u  /**< Gameplay begins */
#define BML_TOPIC_ID_OnPreExitLevel     1004u  /**< Before level unload */
#define BML_TOPIC_ID_OnExitLevel        1005u  /**< Level unloaded */

/** @brief Gameplay state (1010-1019) */
#define BML_TOPIC_ID_OnPauseLevel       1010u  /**< Game paused */
#define BML_TOPIC_ID_OnUnpauseLevel     1011u  /**< Game resumed */
#define BML_TOPIC_ID_OnCounterActive    1012u  /**< Timer started */
#define BML_TOPIC_ID_OnCounterInactive  1013u  /**< Timer stopped */

/** @brief Player/Ball events (1020-1029) */
#define BML_TOPIC_ID_OnBallOff          1020u  /**< Ball fell off */
#define BML_TOPIC_ID_OnBallNavActive    1021u  /**< Ball nav enabled */
#define BML_TOPIC_ID_OnBallNavInactive  1022u  /**< Ball nav disabled */
#define BML_TOPIC_ID_OnCheckpointReached 1023u /**< Checkpoint activated */

/** @brief Object interactions (1030-1039) */
#define BML_TOPIC_ID_OnExtraPointAdded  1030u  /**< Extra point collected */
#define BML_TOPIC_ID_OnSubLevelAdded    1031u  /**< Sublevel loaded */
#define BML_TOPIC_ID_OnPreCheckpointReached 1032u /**< Before checkpoint */

/* ========================================================================
 * Physics/Collision Events (2000-2999)
 * 
 * Physics engine callbacks. High-frequency - use priority filtering.
 * ======================================================================== */

/** @brief Collision callbacks (2000-2009) */
#define BML_TOPIC_ID_OnCollisionEnter   2000u  /**< Collision began */
#define BML_TOPIC_ID_OnCollisionStay    2001u  /**< Collision ongoing */
#define BML_TOPIC_ID_OnCollisionExit    2002u  /**< Collision ended */

/** @brief Physics updates (2010-2019) */
#define BML_TOPIC_ID_OnPhysicsUpdate    2010u  /**< Per-frame physics tick */

/* ========================================================================
 * Input/Camera Events (3000-3999)
 * 
 * User input and camera state changes.
 * ======================================================================== */

/** @brief Keyboard input (3000-3009) */
#define BML_TOPIC_ID_OnKeyDown          3000u  /**< Key pressed */
#define BML_TOPIC_ID_OnKeyUp            3001u  /**< Key released */

/** @brief Mouse input (3002-3009) */
#define BML_TOPIC_ID_OnMouseMove        3002u  /**< Mouse moved */
#define BML_TOPIC_ID_OnMouseButton      3003u  /**< Mouse button event */

/** @brief Camera (3010-3019) */
#define BML_TOPIC_ID_OnCameraChange     3010u  /**< Camera state changed */

/* ========================================================================
 * UI/HUD Events (4000-4999)
 * 
 * User interface and HUD state changes.
 * ======================================================================== */

/** @brief HUD updates (4000-4009) */
#define BML_TOPIC_ID_OnHUDUpdate        4000u  /**< HUD needs refresh */
#define BML_TOPIC_ID_OnMenuOpen         4001u  /**< Menu opened */
#define BML_TOPIC_ID_OnMenuClose        4002u  /**< Menu closed */

/** @brief Command system (4010-4019) */
#define BML_TOPIC_ID_OnCommandExecute   4010u  /**< Console command run */

/* ========================================================================
 * Mod Management Events (5000-5999)
 * 
 * Mod lifecycle and configuration events.
 * ======================================================================== */

/** @brief Mod lifecycle (5000-5009) */
#define BML_TOPIC_ID_OnModAttach        5000u  /**< Mod attached to game */
#define BML_TOPIC_ID_OnModDetach        5001u  /**< Mod detached from game */

/** @brief Configuration (5010-5019) */
#define BML_TOPIC_ID_OnConfigChange     5010u  /**< Config value changed */

/* ========================================================================
 * Pre-registered RPCs (10000-19999)
 * 
 * Well-known RPC endpoints provided by BML runtime.
 * ======================================================================== */

/** @brief Mod information RPCs (10000-10099) */
#define BML_RPC_ID_GetModInfo           10000u /**< Query mod metadata */
#define BML_RPC_ID_GetModList           10001u /**< List loaded mods */
#define BML_RPC_ID_GetModState          10002u /**< Query mod state */

/** @brief Configuration RPCs (10100-10199) */
#define BML_RPC_ID_GetConfig            10100u /**< Read config value */
#define BML_RPC_ID_SetConfig            10101u /**< Write config value */
#define BML_RPC_ID_ResetConfig          10102u /**< Reset to default */

/** @brief Gameplay query RPCs (10200-10299) */
#define BML_RPC_ID_GetPlayerPosition    10200u /**< Get ball position */
#define BML_RPC_ID_GetLevelInfo         10201u /**< Get level metadata */
#define BML_RPC_ID_GetCheckpointCount   10202u /**< Get checkpoint count */
#define BML_RPC_ID_GetCurrentCheckpoint 10203u /**< Get active checkpoint */

/* ========================================================================
 * Dynamic ID Allocation (20000+)
 * 
 * IDs >= 20000 are dynamically assigned at runtime via GetTopicId/GetRpcId.
 * Custom mod topics and RPCs are allocated from this range.
 * 
 * Usage:
 *   BML_TopicId myTopic;
 *   bmlImcGetTopicId("mymod.custom.event", &myTopic);
 *   // myTopic will be >= 20000
 * ======================================================================== */

#endif /* BML_IMC_IDS_H */
