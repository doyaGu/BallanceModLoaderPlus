#pragma once

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class DeformedWB : public BML::Mod::Instance {
    BML::Mod::Context m_Context;
    ModSupport::Timers m_Timers;
    BML::Behavior::Session m_Behaviors;
    std::vector<BML::Command::Registration> m_Commands;

  public:


    void OnLoad(const BML::Mod::Context &) override;
    void OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) override;
    void OnStartLevel(const BML::Mod::Context &) override;

  private:
    ModSupport::Object<CKMesh> m_BallMesh{};
    std::vector<VxVector> m_Vertices;
    std::vector<VxVector> m_Normals;
    BML::Config::Property m_Enabled = {};
    BML::Config::Property m_Extent = {};
};
