#pragma once

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

#include <map>

class MapScripts : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:
    void OnLoad(const BML::Mod::Context &context) override {
        m_Context = context;
        m_Timers = ModSupport::Timers(context);
    }


    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;

    void OnPostLoadLevel(const BML::Mod::Context &) override;
    void OnStartLevel(const BML::Mod::Context &) override;
    void OnPreResetLevel(const BML::Mod::Context &) override;
    void OnPostResetLevel(const BML::Mod::Context &) override;
    void OnPauseLevel(const BML::Mod::Context &) override;
    void OnUnpauseLevel(const BML::Mod::Context &) override;
    void OnPreExitLevel(const BML::Mod::Context &) override;
    void OnPreNextLevel(const BML::Mod::Context &) override;
    void OnDead(const BML::Mod::Context &) override;
    void OnPreEndLevel(const BML::Mod::Context &) override;
    void OnPostEndLevel(const BML::Mod::Context &) override;

    void OnCounterActive(const BML::Mod::Context &) override;
    void OnCounterInactive(const BML::Mod::Context &) override;
    void OnBallNavActive(const BML::Mod::Context &) override;
    void OnBallNavInactive(const BML::Mod::Context &) override;
    void OnCamNavActive(const BML::Mod::Context &) override;
    void OnCamNavInactive(const BML::Mod::Context &) override;
    void OnBallOff(const BML::Mod::Context &) override;
    void OnPreCheckpointReached(const BML::Mod::Context &) override;
    void OnPostCheckpointReached(const BML::Mod::Context &) override;
    void OnLevelFinish(const BML::Mod::Context &) override;
    void OnGameOver(const BML::Mod::Context &) override;
    void OnExtraPoint(const BML::Mod::Context &) override;
    void OnPreSubLife(const BML::Mod::Context &) override;
    void OnPostSubLife(const BML::Mod::Context &) override;
    void OnPreLifeUp(const BML::Mod::Context &) override;
    void OnPostLifeUp(const BML::Mod::Context &) override;

  private:
    std::map<std::string, ModSupport::Object<CKBehavior>> m_MapScripts;
};