#pragma once

#include <vector>

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class DualBallControl : public IMod {
public:
    explicit DualBallControl(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "DualBallControl"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Dual Ball Control"; }
    const char *GetAuthor() override { return "Zzq_203 & Gamepiaynmo"; }
    const char *GetDescription() override { return "Support loading dual ball maps."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

    void OnProcess() override;

    void OnPostLoadLevel() override;
    void OnStartLevel() override {
        if (m_DualActive) {
            m_BML->AddTimer(1ul, [this]() {
                ResetDualBallMatrix();
                PhysicalizeDualBall();
            });
        }
    }

    void OnBallNavActive() override { m_BallNavActive = true; }
    void OnBallNavInactive() override { m_BallNavActive = false; }

    void OnPostResetLevel() override {
        if (m_DualActive) {
            ReleaseDualBall();
            m_DualBallType = 2;
        }
    }
    void OnPostExitLevel() override {
        if (m_DualActive) {
            ReleaseDualBall();
            ReleaseLevel();
        }
    }
    void OnPostNextLevel() override {
        if (m_DualActive) {
            ReleaseDualBall();
            ReleaseLevel();
        }
    }

    void OnBallOff() override {
        if (m_DualActive) {
            int lifes;
            m_Energy->GetElementValue(0, 1, &lifes);
            m_BML->AddTimer(1000.0f, [this, lifes]() {
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

    IProperty *m_SwitchKey = nullptr;
    bool m_DualActive = false;
    bool m_BallNavActive = false;
    float m_SwitchProgress = 0.0f;

    std::vector<CK3dObject *> m_Balls;
    std::vector<CK3dObject *> m_DualBalls;
    std::vector<std::string> m_TrafoTypes;
    CKDataArray *m_Energy = nullptr;
    CKDataArray *m_CurLevel = nullptr;
    CKDataArray *m_IngameParam = nullptr;

    struct PhysicsBall {
        std::string surface;
        float friction, elasticity, mass, linearDamp, rotDamp;
    };
    std::vector<PhysicsBall> m_PhysicsBalls;

    std::vector<CK3dObject *> m_DualResets;
    std::vector<CK3dObject *> m_DualFlames;
    int m_DualBallType = 0;
    CKGroup *m_DepthTestCubes = nullptr;

    CK3dEntity *m_CamTarget = nullptr;
    CK3dEntity *m_CamMF = nullptr;
    CK3dEntity *m_CamPos = nullptr;
    CKTargetCamera *m_InGameCam = nullptr;

    CKBehavior *m_SetNewBall = nullptr;
    CKBehavior *m_DynamicPos = nullptr;
    CKBehavior *m_DeactivateBall = nullptr;
    CKParameter *m_CurTrafo = nullptr;
    CKParameter *m_NearestTrafo = nullptr;
};