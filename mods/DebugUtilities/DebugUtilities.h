#pragma once

#include <string>
#include <map>

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class DebugUtilities : public IMod {
    friend class CommandSector;
public:
    explicit DebugUtilities(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "DebugUtilities"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Debug Utilities"; }
    const char *GetAuthor() override { return "Gamepiaynmo & Kakuty"; }
    const char *GetDescription() override { return "Ballance Debug Utilities."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

    void OnProcess() override;

    void OnCheatEnabled(bool enable) override;
    void OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) override;

    void OnStartLevel() override;
    void OnPreResetLevel() override;
    void OnPostResetLevel() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnPostExitLevel() override;
    void OnPostNextLevel() override;
    void OnDead() override;
    void OnPostEndLevel() override;
    void OnLevelFinish() override;

    bool IsInLevel() const { return m_InLevel && !m_Paused; }

    void ChangeBallSpeed(float times);
    void ResetBall();

    int GetSectorCount();
    void SetSector(int index);

private:
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);
    void OnEditScript_Gameplay_Events(CKBehavior *script);

    void OnProcess_Suicide();
    void OnProcess_ChangeBall();
    void OnProcess_ResetBall();
    void OnProcess_ChangeSpeed();
    void OnProcess_AddLife();
    void OnProcess_SkipRender();
    void OnProcess_Summon();

    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    InputHook *m_InputHook = nullptr;
    float m_DeltaTime = 0.0f;

    bool m_InLevel = false;
    bool m_Paused = false;

    bool m_SkipRender = false;
    IProperty* m_SkipRenderKey = nullptr;

    IProperty *m_BallCheat[2] = {};

    IProperty *m_EnableSuicideKey = nullptr;
    IProperty *m_Suicide = nullptr;
    bool m_SuicideCd = false;

    IProperty *m_ChangeBall[3] = {};
    int m_ChangeBallCd = 0;

    IProperty *m_SpeedNotification = nullptr;
    IProperty *m_SpeedupBall = nullptr;
    bool m_Speedup = false;

    IProperty *m_AddLife = nullptr;
    bool m_AddLifeCd = false;

    CKDataArray *m_PhysicsBall = nullptr;
    CKParameter *m_Force = nullptr;
    std::map<std::string, float> m_Forces;

    IProperty *m_ResetBall = nullptr;
    CKParameterLocal *m_BallForce[2] = {};
    CKBehavior *m_SetNewBall = nullptr;

    CKParameter *m_CurTrafo = nullptr;
    CKDataArray *m_CurLevel = nullptr;
    CKDataArray *m_IngameParam = nullptr;

    CK3dEntity *m_CamOrientRef = nullptr;
    CK3dEntity *m_CamTarget = nullptr;
    CKParameter *m_CurSector = nullptr;
    CKBehavior *m_PhysicsNewBall = nullptr;
    CKBehavior *m_DynamicPos = nullptr;

    IProperty *m_AddBall[4] = {};
    int m_CurSel = -1;
    CK3dEntity *m_CurObj = nullptr;
    CK3dEntity *m_Balls[4] = {};
    std::vector<std::pair<int, CK3dEntity *>> m_TempBalls;
    IProperty *m_MoveKeys[6] = {};
};