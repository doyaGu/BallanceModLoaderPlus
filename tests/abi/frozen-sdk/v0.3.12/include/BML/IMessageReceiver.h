#ifndef BML_IMESSAGERECEIVER_H
#define BML_IMESSAGERECEIVER_H

#include "BML/Defines.h"

class BML_EXPORT IMessageReceiver {
    public:
    virtual void OnPreStartMenu() {};
    virtual void OnPostStartMenu() {};

    virtual void OnExitGame() {};

    virtual void OnPreLoadLevel() {};
    virtual void OnPostLoadLevel() {};

    virtual void OnStartLevel() {};

    virtual void OnPreResetLevel() {};
    virtual void OnPostResetLevel() {};

    virtual void OnPauseLevel() {};
    virtual void OnUnpauseLevel() {};

    virtual void OnPreExitLevel() {};
    virtual void OnPostExitLevel() {};

    virtual void OnPreNextLevel() {};
    virtual void OnPostNextLevel() {};

    virtual void OnDead() {};

    virtual void OnPreEndLevel() {};
    virtual void OnPostEndLevel() {};

    virtual void OnCounterActive() {};
    virtual void OnCounterInactive() {};

    virtual void OnBallNavActive() {};
    virtual void OnBallNavInactive() {};

    virtual void OnCamNavActive() {};
    virtual void OnCamNavInactive() {};

    virtual void OnBallOff() {};

    virtual void OnPreCheckpointReached() {};
    virtual void OnPostCheckpointReached() {};

    virtual void OnLevelFinish() {};

    virtual void OnGameOver() {};

    virtual void OnExtraPoint() {};

    virtual void OnPreSubLife() {};
    virtual void OnPostSubLife() {};

    virtual void OnPreLifeUp() {};
    virtual void OnPostLifeUp() {};
};

#endif // BML_IMESSAGERECEIVER_H
