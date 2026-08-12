// The numbers that say which loader event a stream from Events.h carries. A Mod
// draining that stream reads the kind first and then the payload that goes with it, so this
// is the list to switch on. Events.h is the C++ side of the same stream and the place the
// payload structs are.
//
// Every kind here is also a virtual a Mod can override instead, which is the other way to
// hear the same events. The run starting at 1 is the IMessageReceiver callbacks, where the
// name follows the kind, BML_EVENT_POST_START_MENU being OnPostStartMenu. The run starting
// at 64 is the IMod callbacks that come with data, and there the names were already chosen:
// BML_EVENT_CONFIG_MODIFIED is OnModifyConfig, BML_EVENT_CHEAT_CHANGED is OnCheatEnabled,
// and BML_EVENT_COMMAND_PRE is OnPreCommandExecute.
//
// Nothing here is one of those callbacks, though. These are plain numbers inside a payload,
// and they keep their values from release to release so that a record encoded by one version
// still reads the same way in the next, which is why the two runs start where they do and
// every value after the first of a run is left to follow on.
//
// The values do not say when an event arrives. The PRE and POST pairs are before and after
// the loader's own handling of the same thing, and the ordering rules are the pump's, not
// this list's.
#ifndef BML_EVENT_KINDS_H
#define BML_EVENT_KINDS_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

typedef enum BML_EVENT_KIND {
    BML_EVENT_PRE_START_MENU = 1,
    BML_EVENT_POST_START_MENU,
    BML_EVENT_EXIT_GAME,
    BML_EVENT_PRE_LOAD_LEVEL,
    BML_EVENT_POST_LOAD_LEVEL,
    BML_EVENT_START_LEVEL,
    BML_EVENT_PRE_RESET_LEVEL,
    BML_EVENT_POST_RESET_LEVEL,
    BML_EVENT_PAUSE_LEVEL,
    BML_EVENT_UNPAUSE_LEVEL,
    BML_EVENT_PRE_EXIT_LEVEL,
    BML_EVENT_POST_EXIT_LEVEL,
    BML_EVENT_PRE_NEXT_LEVEL,
    BML_EVENT_POST_NEXT_LEVEL,
    BML_EVENT_DEAD,
    BML_EVENT_PRE_END_LEVEL,
    BML_EVENT_POST_END_LEVEL,
    BML_EVENT_COUNTER_ACTIVE,
    BML_EVENT_COUNTER_INACTIVE,
    BML_EVENT_BALL_NAV_ACTIVE,
    BML_EVENT_BALL_NAV_INACTIVE,
    BML_EVENT_CAM_NAV_ACTIVE,
    BML_EVENT_CAM_NAV_INACTIVE,
    BML_EVENT_BALL_OFF,
    BML_EVENT_PRE_CHECKPOINT_REACHED,
    BML_EVENT_POST_CHECKPOINT_REACHED,
    BML_EVENT_LEVEL_FINISH,
    BML_EVENT_GAME_OVER,
    BML_EVENT_EXTRA_POINT,
    BML_EVENT_PRE_SUB_LIFE,
    BML_EVENT_POST_SUB_LIFE,
    BML_EVENT_PRE_LIFE_UP,
    BML_EVENT_POST_LIFE_UP,
    BML_EVENT_LOAD_OBJECT = 64,
    BML_EVENT_LOAD_SCRIPT,
    BML_EVENT_PHYSICALIZE,
    BML_EVENT_UNPHYSICALIZE,
    BML_EVENT_COMMAND_PRE,
    BML_EVENT_COMMAND_POST,
    BML_EVENT_CONFIG_MODIFIED,
    BML_EVENT_CHEAT_CHANGED,
} BML_EVENT_KIND;

BML_END_CDECLS

#endif // BML_EVENT_KINDS_H
