/**
 * @file bml_topics.h
 * @brief Canonical IMC topic string definitions
 *
 * All shared IMC topic strings used across BML core, the engine bridge, and
 * first-party modules are defined here. Modules should include this header
 * and use the macros instead of hardcoding topic strings.
 *
 * Module-local topics (e.g. BML_Event's Game/* topics) are intentionally
 * excluded; they belong in their own module-local headers.
 */

#ifndef BML_TOPICS_H
#define BML_TOPICS_H

/* ========================================================================
 * Engine Lifecycle
 * ======================================================================== */

#define BML_TOPIC_ENGINE_INIT              "BML/Engine/Init"
#define BML_TOPIC_ENGINE_END               "BML/Engine/End"
#define BML_TOPIC_ENGINE_PLAY              "BML/Engine/Play"
#define BML_TOPIC_ENGINE_PAUSE             "BML/Engine/Pause"
#define BML_TOPIC_ENGINE_RESET             "BML/Engine/Reset"
#define BML_TOPIC_ENGINE_POST_RESET        "BML/Engine/PostReset"

/* ========================================================================
 * Frame Processing
 * ======================================================================== */

#define BML_TOPIC_ENGINE_PRE_PROCESS       "BML/Engine/PreProcess"
#define BML_TOPIC_ENGINE_POST_PROCESS      "BML/Engine/PostProcess"

/* ========================================================================
 * Rendering
 * ======================================================================== */

#define BML_TOPIC_ENGINE_PRE_RENDER        "BML/Engine/PreRender"
#define BML_TOPIC_ENGINE_POST_RENDER       "BML/Engine/PostRender"
#define BML_TOPIC_ENGINE_POST_SPRITE_RENDER "BML/Engine/PostSpriteRender"

/* ========================================================================
 * Input
 * ======================================================================== */

#define BML_TOPIC_INPUT_KEY_DOWN           "BML/Input/KeyDown"
#define BML_TOPIC_INPUT_KEY_UP             "BML/Input/KeyUp"
#define BML_TOPIC_INPUT_MOUSE_BUTTON       "BML/Input/MouseButton"
#define BML_TOPIC_INPUT_MOUSE_MOVE         "BML/Input/MouseMove"

/* ========================================================================
 * UI
 * ======================================================================== */

#define BML_TOPIC_UI_DRAW                  "BML/UI/Draw"

/* ========================================================================
 * State (retained cross-module state topics)
 *
 * State topics use PublishState/CopyState for retained values that persist
 * until explicitly updated. New subscribers immediately receive the last
 * published value.
 * ======================================================================== */

#define BML_TOPIC_STATE_CUSTOM_MAP_NAME    "BML/State/CustomMapName"
#define BML_TOPIC_STATE_CHEAT_ENABLED      "BML/State/CheatEnabled"

/**
 * Retained game phase (payload: BML_GamePhase enum value as uint32_t).
 * Published by BML_Event on lifecycle transitions.
 */
#define BML_TOPIC_STATE_GAME_PHASE         "BML/State/GamePhase"

/**
 * Retained current level number (payload: int32_t, 0 = no level).
 * Reserved -- not yet published by any built-in module. Game-aware modules
 * (e.g. BML_Gameplay) may publish to this topic in the future.
 */
#define BML_TOPIC_STATE_CURRENT_LEVEL      "BML/State/CurrentLevel"

/**
 * Retained pause state (payload: BML_Bool, BML_TRUE = paused).
 * Published by BML_Event on pause/unpause.
 */
#define BML_TOPIC_STATE_PAUSED             "BML/State/Paused"

/* ========================================================================
 * Console
 * ======================================================================== */

#define BML_TOPIC_CONSOLE_COMMAND          "BML/Console/Command"
#define BML_TOPIC_CONSOLE_OUTPUT           "BML/Console/Output"

/* ========================================================================
 * Game Commands (fire-and-forget control requests)
 *
 * Modules publish to these topics to request engine-level actions.
 * ModLoader subscribes and executes them on the main thread.
 * ======================================================================== */

/** Request game exit.  Payload: none. */
#define BML_TOPIC_COMMAND_EXIT_GAME        "BML/Command/ExitGame"

/**
 * Send an in-game message to the HUD/console.
 * Payload: const char * (null-terminated UTF-8 message string).
 */
#define BML_TOPIC_COMMAND_SEND_MESSAGE     "BML/Command/SendMessage"

/**
 * Set Initial Conditions on a CKBeObject.
 * Payload: BML_SetICCommand (see bml_virtools_payloads.h).
 */
#define BML_TOPIC_COMMAND_SET_IC           "BML/Command/SetIC"

/**
 * Restore Initial Conditions on a CKBeObject.
 * Payload: BML_RestoreICCommand (see bml_virtools_payloads.h).
 */
#define BML_TOPIC_COMMAND_RESTORE_IC       "BML/Command/RestoreIC"

/**
 * Show or hide a CKBeObject.
 * Payload: BML_ShowCommand (see bml_virtools_payloads.h).
 */
#define BML_TOPIC_COMMAND_SHOW             "BML/Command/Show"

/* ========================================================================
 * System (Hot Reload)
 * ======================================================================== */

#define BML_TOPIC_SYSTEM_MOD_UNLOAD        "BML/System/ModUnload"
#define BML_TOPIC_SYSTEM_MOD_RELOAD        "BML/System/ModReload"

/* ========================================================================
 * Interface Lifecycle
 *
 * Published by the core when interfaces are registered or unregistered.
 * Payload: const char * interface_id (null-terminated).
 * ======================================================================== */

#define BML_TOPIC_INTERFACE_REGISTERED     "BML/Interface/Registered"
#define BML_TOPIC_INTERFACE_UNREGISTERED   "BML/Interface/Unregistered"

/* ========================================================================
 * RPC Endpoints -- Naming Convention
 *
 * RPC endpoints follow the pattern: "BML/<Module>/Rpc/<Action>"
 * Example: "BML/Network/Rpc/HttpGet"
 *
 * Module-specific RPC endpoints should be defined in the module's own
 * header, not here. This section documents the convention only.
 * ======================================================================== */

/* ========================================================================
 * Object/Script Loading
 * ======================================================================== */

#define BML_TOPIC_OBJECTLOAD_LOAD_OBJECT   "BML/ObjectLoad/LoadObject"
#define BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT   "BML/ObjectLoad/LoadScript"

#endif /* BML_TOPICS_H */
