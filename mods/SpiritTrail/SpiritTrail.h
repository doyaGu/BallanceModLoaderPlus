#pragma once

#include <thread>

#include <BML/BMLAll.h>

#define TICK_SPEED 8

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class SpiritTrail : public IMod {
public:
    explicit SpiritTrail(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "SpiritTrail"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Spirit Trail"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Play the historical best record as a translucent ball."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnProcess() override;

    void OnStartLevel() override {
        m_CurSector = 1;
        PreparePlaying();
        PrepareRecording();
    }

    void OnBallNavActive() override {
        StartPlaying();
        StartRecording();
    }

    void OnPauseLevel() override {
        PausePlaying();
        PauseRecording();
        m_SRActivated = false;
    }
    void OnUnpauseLevel() override {
        UnpausePlaying();
        UnpauseRecording();
        m_SRActivated = true;
    }

    void OnCounterActive() override { m_SRActivated = true; }
    void OnCounterInactive() override { m_SRActivated = false; }

    void OnPostResetLevel() override {
        StopPlaying();
        StopRecording();
    }
    void OnPostExitLevel() override {
        StopPlaying();
        StopRecording();
    }

    void OnBallOff() override {
        int lifes;
        m_Energy->GetElementValue(0, 1, &lifes);
        if (m_DeathReset->GetBoolean() || lifes <= 0) {
            m_BML->AddTimer(1000.0f, [this, lifes]() {
                StopPlaying();
                StopRecording();
                if (lifes > 0) {
                    PreparePlaying();
                    PrepareRecording();
                }
            });
        }
    }

    void OnLevelFinish() override { EndRecording(); }
    void OnPostNextLevel() override { StopPlaying(); }

    void OnPreCheckpointReached() override {
        StopPlaying();
        EndRecording();
        m_CurSector = GetCurrentSector() + 1;
        PreparePlaying();
        PrepareRecording();
    }
    void OnPostCheckpointReached() override {
        StartPlaying();
        StartRecording();
    }

    int GetHSScore();
    float GetSRScore() { return m_SRTimer; }
    int GetCurrentBall();
    int GetCurrentSector();

    void SetCurrentBall(int curBall);

    void PreparePlaying();
    void StartPlaying();
    void PausePlaying();
    void UnpausePlaying();
    void StopPlaying();

    void PrepareRecording();
    void StartRecording();
    void PauseRecording();
    void UnpauseRecording();
    void StopRecording();
    void EndRecording();

private:
    std::string m_CurMap;
    std::string m_RecordDir;
    int m_CurSector = 0;

    bool m_WaitRecording = false;
    bool m_IsRecording = false;
    int m_StartHS = 0;
    float m_RecordTimer = 0;
    size_t m_CurBall = 0;
    bool m_RecordPaused = false;

    bool m_WaitPlaying = false;
    bool m_IsPlaying = false;
    float m_PlayTimer = 0;
    size_t m_PlayBall = 0;
    bool m_PlayHSSR = false;
    size_t m_PlayFrame = 0;
    size_t m_PlayTrafo = 0;
    bool m_PlayPaused = false;

    struct Record {
        int hsscore;
        float srscore;

        struct State {
            VxVector pos;
            VxQuaternion rot;
        };

        std::vector<State> states;
        std::vector<std::pair<int, int>> trafo;
    };
    Record m_Record, m_Play[2];
    std::thread m_LoadPlay;
    bool m_LoadingPlay = false;

    IProperty *m_Enabled = nullptr;
    IProperty *m_HSSR = nullptr;
    IProperty *m_DeathReset = nullptr;

    struct SpiritBall {
        std::string name;
        CK3dObject *obj;
        std::vector<CKMaterial *> materials;
    };
    std::vector<SpiritBall> m_DualBalls;

    CKDataArray *m_Energy = nullptr;
    CKDataArray *m_CurLevel = nullptr;
    CKDataArray *m_IngameParam = nullptr;
    float m_SRTimer = 0;
    bool m_SRActivated = false;
};