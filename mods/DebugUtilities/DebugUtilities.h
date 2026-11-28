#pragma once

#include <string>
#include <map>

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class DebugUtilities : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;
    friend class CommandSector;

  public:


    void OnLoad(const BML::Mod::Context &) override;
    void OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnLoadScript(const BML::Mod::Context &, const BML::Mod::LoadScriptEvent &event) override;

    void OnProcess(const BML::Mod::Context &) override;

    void OnCheatEnabled(const BML::Mod::Context &, const BML::Mod::CheatEvent &event) override;

    void OnStartLevel(const BML::Mod::Context &) override;
    void OnPreResetLevel(const BML::Mod::Context &) override;
    void OnPostResetLevel(const BML::Mod::Context &) override;
    void OnPauseLevel(const BML::Mod::Context &) override;
    void OnUnpauseLevel(const BML::Mod::Context &) override;
    void OnPostExitLevel(const BML::Mod::Context &) override;
    void OnPostNextLevel(const BML::Mod::Context &) override;
    void OnDead(const BML::Mod::Context &) override;
    void OnPostEndLevel(const BML::Mod::Context &) override;
    void OnLevelFinish(const BML::Mod::Context &) override;

    bool IsInLevel() const { return m_InLevel && !m_Paused; }

    void ChangeBallSpeed(float times);
    void ResetBall();

    int GetSectorCount();
    void SetSector(int index);

  private:
    BML::Input::Block m_ChangeBallBlock, m_SummonBlock;
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);
    void OnEditScript_Gameplay_Events(CKBehavior *script);
    void UpdateBallForceKey(int index, bool enabled);

    void OnProcess_Suicide();
    void OnProcess_ChangeBall();
    void OnProcess_ResetBall();
    void OnProcess_ChangeSpeed();
    void OnProcess_AddLife();
    void OnProcess_SkipRender();
    void OnProcess_Summon();

    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    float m_DeltaTime = 0.0f;
    float m_SpeedTimes = 1.0f;

    bool m_InLevel = false;
    bool m_Paused = false;

    bool m_SkipRender = false;
    BML::Config::Property m_SkipRenderKey = {};

    BML::Config::Property m_BallCheat[2] = {};

    BML::Config::Property m_EnableSuicideKey = {};
    BML::Config::Property m_Suicide = {};
    bool m_SuicideCd = false;

    BML::Config::Property m_ChangeBall[3] = {};
    int m_ChangeBallCd = 0;

    BML::Config::Property m_SpeedNotification = {};
    BML::Config::Property m_SpeedupBall = {};
    bool m_Speedup = false;

    BML::Config::Property m_AddLife = {};
    bool m_AddLifeCd = false;

    ModSupport::Object<CKDataArray> m_PhysicsBall{m_Context};
    ModSupport::Object<CKParameter> m_Force{m_Context};
    std::map<std::string, float> m_Forces;

    BML::Config::Property m_ResetBall = {};
    ModSupport::Object<CKParameterLocal> m_BallForce[2]{ModSupport::Object<CKParameterLocal>{m_Context}, ModSupport::Object<CKParameterLocal>{m_Context}};
    ModSupport::Object<CKBehavior> m_SetNewBall{m_Context};

    ModSupport::Object<CKParameter> m_CurTrafo{m_Context};
    ModSupport::Object<CKDataArray> m_CurLevel{m_Context};
    ModSupport::Object<CKDataArray> m_IngameParam{m_Context};

    ModSupport::Object<CK3dEntity> m_CamOrientRef{m_Context};
    ModSupport::Object<CK3dEntity> m_CamTarget{m_Context};
    ModSupport::Object<CKParameter> m_CurSector{m_Context};
    ModSupport::Object<CKBehavior> m_PhysicsNewBall{m_Context};
    ModSupport::Object<CKBehavior> m_DynamicPos{m_Context};

    BML::Config::Property m_AddBall[4] = {};
    int m_CurSel = -1;
    ModSupport::Object<CK3dEntity> m_CurObj{m_Context};
    ModSupport::Object<CK3dEntity> m_Balls[4]{ModSupport::Object<CK3dEntity>{m_Context}, ModSupport::Object<CK3dEntity>{m_Context}, ModSupport::Object<CK3dEntity>{m_Context}, ModSupport::Object<CK3dEntity>{m_Context}};
    std::vector<std::pair<int, CK3dEntity *>> m_TempBalls;
    BML::Config::Property m_MoveKeys[6] = {};
};
