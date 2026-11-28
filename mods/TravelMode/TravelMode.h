#pragma once

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class TravelMode : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:


    void OnLoad(const BML::Mod::Context &) override;
    void OnUnload(const BML::Mod::Context &) override;
    void OnProcess(const BML::Mod::Context &) override;

    void OnExitGame(const BML::Mod::Context &) override;
    void OnPreExitLevel(const BML::Mod::Context &) override;

    void EnterTravelCam();
    void ExitTravelCam();
    bool IsInTravelCam();

  private:
    CKRenderContext *m_RenderContext = nullptr;
    float m_DeltaTime = 0.0f;

    float m_TravelSpeed = 0.2f;
    float m_RotateSpeed = 0.2f;
    ModSupport::Object<CKCamera> m_TravelCam{m_Context};
};