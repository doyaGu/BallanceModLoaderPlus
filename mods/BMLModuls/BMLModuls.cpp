#include "BMLModuls.h"

BML_DEFINE_MOD(BMLModuls, {
    .Id = "BMLModuls",
    .Name = "BML Moduls",
    .Version = BML_VERSION,
    .Author = "Gamepiaynmo",
    .Description = "Add some new moduls to the game.",
    .MinimumLoaderVersion = BML_VERSION,
})

void BMLModuls::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    ModSupport::Check(BML::Gameplay::RegisterModule(m_Context, {"P_Rotboard_Wood"}));
    ModSupport::Check(BML::Gameplay::RegisterModule(m_Context, {"P_Rotboard_Wood_Inv"}));
    ModSupport::Check(BML::Gameplay::RegisterModule(m_Context, {"P_Dropboard_Wood"}));

    ModSupport::Check(BML::Gameplay::RegisterFloorType(
        m_Context, {"Phys_Floors_F0", 0.0f, 0.3f, 1.0f, "Floor", true}));
    ModSupport::Check(BML::Gameplay::RegisterFloorType(
        m_Context, {"Phys_Floors_E80", 0.7f, 24.0f, 1.0f, "Floor", true}));
    ModSupport::Check(BML::Gameplay::RegisterFloorType(
        m_Context, {"Phys_Floors_E400", 0.7f, 120.0f, 1.0f, "Floor", true}));
}