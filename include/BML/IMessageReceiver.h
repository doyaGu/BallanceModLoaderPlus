// The gameplay callbacks, inherited by IMod. A Mod overrides the ones it needs and
// the loader calls them; nothing here is called by a Mod.
//
// These are not polled and not queued. The loader inserts a hook block into the
// game's own scripts, Event_handler, Gameplay_Ingame, Gameplay_Energy and
// Gameplay_Events, at the exact point where the game handles the matching Virtools
// message, so the callback runs in the middle of the game's own logic on the game
// thread. Work done here delays the game's script for as long as it takes, and the
// hooks only exist once those scripts have loaded, which is why nothing here fires
// before the main menu appears.
//
// Where a Pre and a Post callback come as a pair, they bracket the game's own work
// at that spot: Pre runs before the block chain that handles the message, Post after
// it has finished. So the state a Mod wants to read before a change belongs in the
// Pre one and the result belongs in the Post one.
//
// The loader updates IBML::IsIngame, IsPaused and IsPlaying after the callback
// returns, so inside these functions those three still report the state before the
// event. Act on the callback rather than reading the flags back.
//
// The loader catches whatever a callback throws, logs it against the Mod's id, and
// carries on with the next Mod.
#ifndef BML_IMESSAGERECEIVER_H
#define BML_IMESSAGERECEIVER_H

#include "BML/Defines.h"

class BML_EXPORT IMessageReceiver {
    public:
    // The eleven callbacks below follow the messages Event_handler switches on, in
    // its own order: Start Menu, Exit Game, Load Level, Start Level, Reset Level,
    // Pause Level, Unpause Level, Exit Level, Next Level, Dead, End Level.

    // The main menu is coming up, at game start and every time the player returns to
    // it.
    virtual void OnPreStartMenu() {};
    virtual void OnPostStartMenu() {};

    // The player is quitting. The game closes right after the game's own handling of
    // this, so it is the last chance to write anything out. IBML::ExitGame produces
    // the same event.
    virtual void OnExitGame() {};

    // The level's files are read between these two, so the level's objects do not
    // exist yet in OnPreLoadLevel and do in OnPostLoadLevel.
    virtual void OnPreLoadLevel() {};
    virtual void OnPostLoadLevel() {};

    // The level is built and about to be played. This is where to look up the
    // objects of the level, since the ones from the previous level are gone by now.
    virtual void OnStartLevel() {};

    // The player restarted the whole level, which is not the same as losing the ball
    // and continuing from a checkpoint.
    virtual void OnPreResetLevel() {};
    virtual void OnPostResetLevel() {};

    virtual void OnPauseLevel() {};
    virtual void OnUnpauseLevel() {};

    // The player left the level for the menu. The game deletes the level's collision
    // surfaces during its own handling and the rest of the level follows, so let go of
    // anything belonging to the level in OnPreExitLevel rather than in the Post one.
    virtual void OnPreExitLevel() {};
    virtual void OnPostExitLevel() {};

    // Moving on to the next level, which the game does once a level has been
    // finished. As when exiting, the old level's collision surfaces are already gone
    // by the Post call.
    virtual void OnPreNextLevel() {};
    virtual void OnPostNextLevel() {};

    // The game's Dead message.
    virtual void OnDead() {};

    // The level is over. OnPostEndLevel runs after the game has computed the
    // highscore, so the score is final by then.
    virtual void OnPreEndLevel() {};
    virtual void OnPostEndLevel() {};

    // The energy counter of Gameplay_Energy started or stopped running. The loader's
    // own speedrun timer follows these two, so they mark the stretches that count as
    // played time.
    virtual void OnCounterActive() {};
    virtual void OnCounterInactive() {};

    // The BallNav activate and deactivate messages, which is the player gaining and
    // losing control of the ball. Between an active and the next inactive is the part
    // of a level where the player is actually steering, which is narrower than
    // IBML::IsPlaying.
    virtual void OnBallNavActive() {};
    virtual void OnBallNavInactive() {};

    // The CamNav activate and deactivate messages, the same thing for camera
    // control.
    virtual void OnCamNavActive() {};
    virtual void OnCamNavInactive() {};

    // The game's Ball Off message, sent when the ball leaves the level. The game
    // waits before acting on it, so the life bookkeeping arrives later, in
    // OnPreSubLife.
    virtual void OnBallOff() {};

    // A checkpoint was reached. The game sets the new resetpoint between these two, so
    // the one it would respawn the ball at is still the previous one in
    // OnPreCheckpointReached.
    virtual void OnPreCheckpointReached() {};
    virtual void OnPostCheckpointReached() {};

    // The game's Level_Finish message, sent when the player completes the level.
    virtual void OnLevelFinish() {};

    // The game's Game Over message.
    virtual void OnGameOver() {};

    // The game's Extrapoint message.
    virtual void OnExtraPoint() {};

    // The game subtracts a life between these two, so the count in its Energy array
    // is the old one in OnPreSubLife and the new one in OnPostSubLife. The same holds
    // for OnPreLifeUp and OnPostLifeUp, which bracket the game adding one.
    virtual void OnPreSubLife() {};
    virtual void OnPostSubLife() {};

    virtual void OnPreLifeUp() {};
    virtual void OnPostLifeUp() {};
};

#endif // BML_IMESSAGERECEIVER_H
