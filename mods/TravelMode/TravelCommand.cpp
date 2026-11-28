#include "TravelCommand.h"
#include "TravelMode.h"

void TravelCommand::Execute(const BML::Mod::Context &context,
                            const std::vector<std::string> &args) {
    if (ModSupport::GameState().Playing) {
        if (m_Mod->IsInTravelCam()) {
            m_Mod->ExitTravelCam();
            BML::UI::AddMessage("Exit Travel Camera");
            ModSupport::Find<CKGroup>(context, "HUD_sprites")->Show();
            ModSupport::Find<CKGroup>(context, "LifeBalls")->Show();
        } else {
            m_Mod->EnterTravelCam();
            BML::UI::AddMessage("Enter Travel Camera");
            ModSupport::Find<CKGroup>(context, "HUD_sprites")->Show(CKHIDE);
            ModSupport::Find<CKGroup>(context, "LifeBalls")->Show(CKHIDE);
        }
    }
}