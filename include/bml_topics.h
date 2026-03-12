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
 * Console
 * ======================================================================== */

#define BML_TOPIC_CONSOLE_COMMAND          "BML/Console/Command"
#define BML_TOPIC_CONSOLE_OUTPUT           "BML/Console/Output"

/* ========================================================================
 * System (Hot Reload)
 * ======================================================================== */

#define BML_TOPIC_SYSTEM_MOD_UNLOAD        "BML/System/ModUnload"
#define BML_TOPIC_SYSTEM_MOD_RELOAD        "BML/System/ModReload"

/* ========================================================================
 * Object/Script Loading
 * ======================================================================== */

#define BML_TOPIC_OBJECTLOAD_LOAD_OBJECT   "BML/ObjectLoad/LoadObject"
#define BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT   "BML/ObjectLoad/LoadScript"

#endif /* BML_TOPICS_H */
