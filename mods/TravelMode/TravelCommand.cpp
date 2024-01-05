#include "TravelCommand.h"
#include "TravelMode.h"

void TravelCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (bml->IsPlaying()) {
        if (m_Mod->IsInTravelCam()) {
            m_Mod->ExitTravelCam();
            bml->SendIngameMessage("Exit Travel Camera");
            bml->GetGroupByName("HUD_sprites")->Show();
            bml->GetGroupByName("LifeBalls")->Show();
        } else {
            m_Mod->EnterTravelCam();
            bml->SendIngameMessage("Enter Travel Camera");
            bml->GetGroupByName("HUD_sprites")->Show(CKHIDE);
            bml->GetGroupByName("LifeBalls")->Show(CKHIDE);
        }
    }
}