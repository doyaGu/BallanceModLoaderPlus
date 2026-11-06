#pragma once

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class DynamicFov : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:


    void OnLoad(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnLoadScript(const BML::Mod::Context &, const BML::Mod::LoadScriptEvent &event) override;
    void OnProcess(const BML::Mod::Context &) override;
    void OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) override;

    void SetInactive() {
        m_Timers.AfterTicks(1, [this]() { m_IsActive = false; });
    }

  private:
    std::vector<BML::Behavior::Instance> m_Hooks;
    ModSupport::Object<CKCamera> m_InGameCam{};
    ModSupport::Object<CKDataArray> m_CurLevel{};
    ModSupport::Object<CKBehavior> m_IngameScript{};
    ModSupport::Object<CKBehavior> m_DynamicPos{};

    bool m_IsActive = false;
    bool m_WasPaused = false;
    VxVector m_LastPos;

    BML::Config::Property m_Enabled = {};
};