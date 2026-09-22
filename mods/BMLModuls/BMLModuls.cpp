#include "BMLModuls.h"

IMod *BMLEntry(IBML *bml) {
    return new BMLModuls(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void BMLModuls::OnLoad() {
    m_BML->RegisterModul("P_Rotboard_Wood");
    m_BML->RegisterModul("P_Rotboard_Wood_Inv");
    m_BML->RegisterModul("P_Dropboard_Wood");

    m_BML->RegisterFloorType("Phys_Floors_F0", 0.0f, 0.3f, 1.0f, "Floor", true);
    m_BML->RegisterFloorType("Phys_Floors_E80", 0.7f, 24.0f, 1.0f, "Floor", true);
    m_BML->RegisterFloorType("Phys_Floors_E400", 0.7f, 120.0f, 1.0f, "Floor", true);
}