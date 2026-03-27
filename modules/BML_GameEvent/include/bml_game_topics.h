#ifndef BML_GAME_TOPICS_H
#define BML_GAME_TOPICS_H

/*
 * Canonical first-party gameplay topic strings published by BML_Event.
 * Keep these in public headers so modules do not hardcode topic literals.
 */

#define BML_TOPIC_GAME_MENU_PRE_START      "Game/Menu/PreStartMenu"
#define BML_TOPIC_GAME_MENU_POST_START     "Game/Menu/PostStartMenu"
#define BML_TOPIC_GAME_MENU_EXIT           "Game/Menu/ExitGame"

#define BML_TOPIC_GAME_LEVEL_PRE_LOAD      "Game/Level/PreLoad"
#define BML_TOPIC_GAME_LEVEL_POST_LOAD     "Game/Level/PostLoad"
#define BML_TOPIC_GAME_LEVEL_START         "Game/Level/Start"
#define BML_TOPIC_GAME_LEVEL_PRE_RESET     "Game/Level/PreReset"
#define BML_TOPIC_GAME_LEVEL_POST_RESET    "Game/Level/PostReset"
#define BML_TOPIC_GAME_LEVEL_PAUSE         "Game/Level/Pause"
#define BML_TOPIC_GAME_LEVEL_UNPAUSE       "Game/Level/Unpause"
#define BML_TOPIC_GAME_LEVEL_PRE_EXIT      "Game/Level/PreExit"
#define BML_TOPIC_GAME_LEVEL_POST_EXIT     "Game/Level/PostExit"
#define BML_TOPIC_GAME_LEVEL_PRE_NEXT      "Game/Level/PreNext"
#define BML_TOPIC_GAME_LEVEL_POST_NEXT     "Game/Level/PostNext"
#define BML_TOPIC_GAME_LEVEL_PRE_END       "Game/Level/PreEnd"
#define BML_TOPIC_GAME_LEVEL_POST_END      "Game/Level/PostEnd"

#define BML_TOPIC_GAME_PLAY_DEAD           "Game/Gameplay/Dead"
#define BML_TOPIC_GAME_PLAY_BALL_OFF       "Game/Gameplay/BallOff"
#define BML_TOPIC_GAME_PLAY_COUNTER_ACTIVE "Game/Gameplay/CounterActive"
#define BML_TOPIC_GAME_PLAY_COUNTER_INACTIVE "Game/Gameplay/CounterInactive"
#define BML_TOPIC_GAME_PLAY_PRE_CHECKPOINT "Game/Gameplay/PreCheckpoint"
#define BML_TOPIC_GAME_PLAY_POST_CHECKPOINT "Game/Gameplay/PostCheckpoint"
#define BML_TOPIC_GAME_PLAY_LEVEL_FINISH   "Game/Gameplay/LevelFinish"
#define BML_TOPIC_GAME_PLAY_GAME_OVER      "Game/Gameplay/GameOver"
#define BML_TOPIC_GAME_PLAY_EXTRA_POINT    "Game/Gameplay/ExtraPoint"
#define BML_TOPIC_GAME_PLAY_PRE_LIFE_UP    "Game/Gameplay/PreLifeUp"
#define BML_TOPIC_GAME_PLAY_POST_LIFE_UP   "Game/Gameplay/PostLifeUp"
#define BML_TOPIC_GAME_PLAY_PRE_SUB_LIFE   "Game/Gameplay/PreSubLife"
#define BML_TOPIC_GAME_PLAY_POST_SUB_LIFE  "Game/Gameplay/PostSubLife"

#define BML_TOPIC_GAME_BALL_NAV_ACTIVE     "Game/Ball/NavActive"
#define BML_TOPIC_GAME_BALL_NAV_INACTIVE   "Game/Ball/NavInactive"

#define BML_TOPIC_GAME_CAMERA_NAV_ACTIVE   "Game/Camera/NavActive"
#define BML_TOPIC_GAME_CAMERA_NAV_INACTIVE "Game/Camera/NavInactive"

#endif /* BML_GAME_TOPICS_H */
