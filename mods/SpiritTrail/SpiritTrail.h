#pragma once

#include <thread>

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

#define TICK_SPEED 8

class SpiritTrail : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:
    void OnUnload(const BML::Mod::Context &) override;


    void OnLoad(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnProcess(const BML::Mod::Context &) override;

    void OnStartLevel(const BML::Mod::Context &) override {
        m_CurSector = 1;
        PreparePlaying();
        PrepareRecording();
    }

    void OnBallNavActive(const BML::Mod::Context &) override {
        StartPlaying();
        StartRecording();
    }

    void OnPauseLevel(const BML::Mod::Context &) override {
        PausePlaying();
        PauseRecording();
        m_SRActivated = false;
    }
    void OnUnpauseLevel(const BML::Mod::Context &) override {
        UnpausePlaying();
        UnpauseRecording();
        m_SRActivated = true;
    }

    void OnCounterActive(const BML::Mod::Context &) override { m_SRActivated = true; }
    void OnCounterInactive(const BML::Mod::Context &) override { m_SRActivated = false; }

    void OnPostResetLevel(const BML::Mod::Context &) override {
        StopPlaying();
        StopRecording();
    }
    void OnPostExitLevel(const BML::Mod::Context &) override {
        StopPlaying();
        StopRecording();
    }

    void OnBallOff(const BML::Mod::Context &) override {
        int lifes;
        m_Energy->GetElementValue(0, 1, &lifes);
        if (m_DeathReset.Get<bool>() || lifes <= 0) {
            m_Timers.After(1000.0f, [this, lifes]() {
                StopPlaying();
                StopRecording();
                if (lifes > 0) {
                    PreparePlaying();
                    PrepareRecording();
                }
            });
        }
    }

    void OnLevelFinish(const BML::Mod::Context &) override { EndRecording(); }
    void OnPostNextLevel(const BML::Mod::Context &) override { StopPlaying(); }

    void OnPreCheckpointReached(const BML::Mod::Context &) override {
        StopPlaying();
        EndRecording();
        m_CurSector = GetCurrentSector() + 1;
        PreparePlaying();
        PrepareRecording();
    }
    void OnPostCheckpointReached(const BML::Mod::Context &) override {
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
        int hsscore = INT_MIN;
        float srscore = FLT_MAX;

        struct State {
            VxVector pos;
            VxQuaternion rot;
        };

        std::vector<State> states;
        std::vector<std::pair<int, int>> trafo;
    };
    Record m_Record, m_Play[2];
    std::jthread m_LoadPlay;
    std::jthread m_SaveRecord;
    std::string m_TrailsRoot;
    bool m_LoadingPlay = false;

    BML::Config::Property m_Enabled = {};
    BML::Config::Property m_HSSR = {};
    BML::Config::Property m_DeathReset = {};

    struct SpiritBall {
        std::string name;
        ModSupport::Object<CK3dObject> obj;
        std::vector<ModSupport::Object<CKMaterial>> materials;
    };
    std::vector<SpiritBall> m_DualBalls;

    ModSupport::Object<CKDataArray> m_Energy{};
    ModSupport::Object<CKDataArray> m_CurLevel{};
    ModSupport::Object<CKDataArray> m_IngameParam{};
    float m_SRTimer = 0;
    bool m_SRActivated = false;
};