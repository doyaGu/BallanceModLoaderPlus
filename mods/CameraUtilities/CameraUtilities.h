#pragma once

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class CameraUtilities : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:


    void OnLoad(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnProcess(const BML::Mod::Context &) override;

  private:
    float m_DeltaTime = 0.0f;

    BML::Config::Property m_CamRot[2] = {};
    BML::Config::Property m_CamY[2] = {};
    BML::Config::Property m_CamZ[2] = {};
    BML::Config::Property m_Cam45 = {};
    BML::Config::Property m_CamReset = {};
    BML::Config::Property m_CamOn = {};

    ModSupport::Object<CK3dEntity> m_CamPos{m_Context};
    ModSupport::Object<CK3dEntity> m_CamOrient{m_Context};
    ModSupport::Object<CK3dEntity> m_CamOrientRef{m_Context};
    ModSupport::Object<CK3dEntity> m_CamTarget{m_Context};
};