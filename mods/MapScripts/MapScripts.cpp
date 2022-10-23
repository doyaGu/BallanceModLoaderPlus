#include "MapScripts.h"

IMod *BMLEntry(IBML *bml) {
    return new MapScripts(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

template<typename T>
void *func_addr(T func) {
    return *reinterpret_cast<void **>(&func);
}

void MapScripts::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {
#define FILL_MAP(FUNC) \
    m_MapScripts[func_addr(&MapScripts::FUNC)] = m_BML->GetScriptByName("BML_" #FUNC)

    if (isMap) {
        FILL_MAP(OnPostLoadLevel);
        FILL_MAP(OnStartLevel);
        FILL_MAP(OnPreResetLevel);
        FILL_MAP(OnPostResetLevel);
        FILL_MAP(OnPauseLevel);
        FILL_MAP(OnUnpauseLevel);
        FILL_MAP(OnPreExitLevel);
        FILL_MAP(OnPreNextLevel);
        FILL_MAP(OnDead);
        FILL_MAP(OnPreEndLevel);
        FILL_MAP(OnPostEndLevel);
        FILL_MAP(OnCounterActive);
        FILL_MAP(OnCounterInactive);
        FILL_MAP(OnBallNavActive);
        FILL_MAP(OnBallNavInactive);
        FILL_MAP(OnCamNavActive);
        FILL_MAP(OnCamNavInactive);
        FILL_MAP(OnBallOff);
        FILL_MAP(OnPreCheckpointReached);
        FILL_MAP(OnPostCheckpointReached);
        FILL_MAP(OnLevelFinish);
        FILL_MAP(OnGameOver);
        FILL_MAP(OnExtraPoint);
        FILL_MAP(OnPreSubLife);
        FILL_MAP(OnPostSubLife);
        FILL_MAP(OnPreLifeUp);
        FILL_MAP(OnPostLifeUp);
    }
#undef FILL_MAP
}

#define ACT_SCRIPT(FUNC) { \
    CKBehavior* script = m_MapScripts[func_addr(&MapScripts::FUNC)]; \
    if (script != nullptr) { \
        CKBeObject* obj = script->GetOwner(); \
        CKScene* scene = m_BML->GetCKContext()->GetCurrentScene(); \
        scene->Activate(obj, false); \
        scene->Activate(script, true); \
    } \
}

void MapScripts::OnPostLoadLevel() {
    ACT_SCRIPT(OnPostLoadLevel);
}

void MapScripts::OnStartLevel() {
    ACT_SCRIPT(OnStartLevel);
}

void MapScripts::OnPreResetLevel() {
    ACT_SCRIPT(OnPreResetLevel);
}

void MapScripts::OnPostResetLevel() {
    ACT_SCRIPT(OnPostResetLevel);
}

void MapScripts::OnPauseLevel() {
    ACT_SCRIPT(OnPauseLevel);
}

void MapScripts::OnUnpauseLevel() {
    ACT_SCRIPT(OnUnpauseLevel);
}

void MapScripts::OnPreExitLevel() {
    ACT_SCRIPT(OnPreExitLevel);
}

void MapScripts::OnPreNextLevel() {
    ACT_SCRIPT(OnPreNextLevel);
}

void MapScripts::OnDead() {
    ACT_SCRIPT(OnDead);
}

void MapScripts::OnPreEndLevel() {
    ACT_SCRIPT(OnPreEndLevel);
}

void MapScripts::OnPostEndLevel() {
    ACT_SCRIPT(OnPostEndLevel);
}

void MapScripts::OnCounterActive() {
    ACT_SCRIPT(OnCounterActive);
}

void MapScripts::OnCounterInactive() {
    ACT_SCRIPT(OnCounterInactive);
}

void MapScripts::OnBallNavActive() {
    ACT_SCRIPT(OnBallNavActive);
}

void MapScripts::OnBallNavInactive() {
    ACT_SCRIPT(OnBallNavInactive);
}

void MapScripts::OnCamNavActive() {
    ACT_SCRIPT(OnCamNavActive);
}

void MapScripts::OnCamNavInactive() {
    ACT_SCRIPT(OnCamNavInactive);
}

void MapScripts::OnBallOff() {
    ACT_SCRIPT(OnBallOff);
}

void MapScripts::OnPreCheckpointReached() {
    ACT_SCRIPT(OnPreCheckpointReached);
}

void MapScripts::OnPostCheckpointReached() {
    ACT_SCRIPT(OnPostCheckpointReached);
}

void MapScripts::OnLevelFinish() {
    ACT_SCRIPT(OnLevelFinish);
}

void MapScripts::OnGameOver() {
    ACT_SCRIPT(OnGameOver);
}

void MapScripts::OnExtraPoint() {
    ACT_SCRIPT(OnExtraPoint);
}

void MapScripts::OnPreSubLife() {
    ACT_SCRIPT(OnPreSubLife);
}

void MapScripts::OnPostSubLife() {
    ACT_SCRIPT(OnPostSubLife);
}

void MapScripts::OnPreLifeUp() {
    ACT_SCRIPT(OnPreLifeUp);
}

void MapScripts::OnPostLifeUp() {
    ACT_SCRIPT(OnPostLifeUp);
}

#undef ACT_SCRIPT