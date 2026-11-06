#pragma once

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class BallSticky : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:


    void OnLoad(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnLoadScript(const BML::Mod::Context &, const BML::Mod::LoadScriptEvent &event) override;
    void OnProcess(const BML::Mod::Context &) override;

  private:
    void OnLoadLevelinit(XObjectArray *objArray);
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);

    ModSupport::Object<CK3dEntity> m_BallRef[2]{};
    ModSupport::Object<CK3dEntity> m_CamTarget{};
    ModSupport::Object<CKDataArray> m_CurLevel{};
    float m_StickyImpulse = 0;
    ModSupport::Object<CKParameter> m_StickyForce[2]{};
};