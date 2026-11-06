#pragma once

#include <vector>

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class DualBallControl : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:


    void OnLoad(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnLoadScript(const BML::Mod::Context &, const BML::Mod::LoadScriptEvent &event) override;

    void OnProcess(const BML::Mod::Context &) override;

    void OnPostLoadLevel(const BML::Mod::Context &) override;
    void OnStartLevel(const BML::Mod::Context &) override {
        if (m_DualActive) {
            m_Timers.AfterTicks(1, [this]() {
                ResetDualBallMatrix();
                PhysicalizeDualBall();
            });
        }
    }

    void OnBallNavActive(const BML::Mod::Context &) override { m_BallNavActive = true; }
    void OnBallNavInactive(const BML::Mod::Context &) override { m_BallNavActive = false; }

    void OnPostResetLevel(const BML::Mod::Context &) override {
        if (m_DualActive) {
            ReleaseDualBall();
            m_DualBallType = 2;
        }
    }
    void OnPostExitLevel(const BML::Mod::Context &) override {
        if (m_DualActive) {
            ReleaseDualBall();
            ReleaseLevel();
        }
    }
    void OnPostNextLevel(const BML::Mod::Context &) override {
        if (m_DualActive) {
            ReleaseDualBall();
            ReleaseLevel();
        }
    }

    void OnBallOff(const BML::Mod::Context &) override {
        if (m_DualActive) {
            int lifes;
            m_Energy->GetElementValue(0, 1, &lifes);
            m_Timers.After(1000.0f, [this, lifes]() {
                ReleaseDualBall();
                if (lifes > 0) {
                    ResetDualBallMatrix();
                    PhysicalizeDualBall();
                }
            });
        }
    }

  private:
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);

    int GetCurrentSector();
    int GetCurrentBallType();
    void ReleaseDualBall();

    void PhysicalizeDualBall();
    void ResetDualBallMatrix();
    void ReleaseLevel();

    BML::Config::Property m_SwitchKey = {};
    bool m_DualActive = false;
    bool m_BallNavActive = false;
    float m_SwitchProgress = 0.0f;

    std::vector<CK3dObject *> m_Balls;
    std::vector<CK3dObject *> m_DualBalls;
    std::vector<std::string> m_TrafoTypes;
    ModSupport::Object<CKDataArray> m_Energy{};
    ModSupport::Object<CKDataArray> m_CurLevel{};
    ModSupport::Object<CKDataArray> m_IngameParam{};

    struct PhysicsBall {
        std::string surface;
        float friction, elasticity, mass, linearDamp, rotDamp;
    };
    std::vector<PhysicsBall> m_PhysicsBalls;

    std::vector<CK3dObject *> m_DualResets;
    std::vector<CK3dObject *> m_DualFlames;
    int m_DualBallType = 0;
    ModSupport::Object<CKGroup> m_DepthTestCubes{};

    ModSupport::Object<CK3dEntity> m_CamTarget{};
    ModSupport::Object<CK3dEntity> m_CamMF{};
    ModSupport::Object<CK3dEntity> m_CamPos{};
    ModSupport::Object<CKTargetCamera> m_InGameCam{};

    ModSupport::Object<CKBehavior> m_SetNewBall{};
    ModSupport::Object<CKBehavior> m_DynamicPos{};
    ModSupport::Object<CKBehavior> m_DeactivateBall{};
    ModSupport::Object<CKParameter> m_CurTrafo{};
    ModSupport::Object<CKParameter> m_NearestTrafo{};
};