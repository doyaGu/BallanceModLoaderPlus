#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class TravelMode : public IMod {
public:
    explicit TravelMode(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "TravelMode"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Travel Mode"; }
    const char *GetAuthor() override { return "Gamepiaynmo & Kakuty"; }
    const char *GetDescription() override { return "Travel Mode for Ballance."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnProcess() override;

    void OnExitGame() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnPreExitLevel() override;

    void EnterTravelCam();
    void ExitTravelCam();
    bool IsInTravelCam();

private:
    CKRenderContext *m_RenderContext = nullptr;
    InputHook *m_InputHook = nullptr;
    float m_DeltaTime = 0.0f;

    bool m_Once = false;
    bool m_Paused = false;

    float m_TravelSpeed = 0.2f;
    CKCamera *m_TravelCam = nullptr;
};