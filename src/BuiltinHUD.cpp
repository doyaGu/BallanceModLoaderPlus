#include "BuiltinHUD.h"

#include <cstring>

#include "BML/Version.h"

void BuiltinHUD::OnLoad(bool showTitle, bool showFPS) {
    m_LastCheatState = false;

    auto title = m_Window.AddText("title", "BML Plus " BML_VERSION, AnchorPoint::TopCenter);
    title->SetScale(1.2f);
    title->SetVisible(showTitle);
    m_TitleElement = title;

    m_FPSElement = m_Window.AddText("fps", "FPS: 60", AnchorPoint::TopLeft);
    m_FPSElement->SetVisible(showFPS);

    auto sr = m_Window.AddVStack("sr", AnchorPoint::BottomLeft);
    sr->SetOffsetNormalized(0.03f, -0.155f);
    sr->SetVisible(false);
    sr->AddChildNamed("label", "SR Timer");
    sr->AddChildNamed("value", "  00:00:00.000");
    m_SRElement = sr;

    m_CheatElement = m_Window.AddText(
        "cheat", "\x1b[38;2;255;200;60mCheat Mode Enabled\x1b[0m", AnchorPoint::BottomCenter);
    m_CheatElement->SetOffsetNormalized(0.0f, -0.12f);
    m_CheatElement->SetVisible(false);

    UpdateTimerDisplay();
}

void BuiltinHUD::OnUnload() {
    m_TitleElement = nullptr;
    m_FPSElement = nullptr;
    m_SRElement = nullptr;
    m_CheatElement = nullptr;
}

void BuiltinHUD::OnProcess(float frameDeltaSeconds, float gameDeltaMilliseconds, bool cheatEnabled) {
    m_FPSCounter.Update(frameDeltaSeconds);
    m_SRTimer.Update(gameDeltaMilliseconds);

    UpdateCheatState(cheatEnabled);
    UpdateTimerDisplay();

    if (m_FPSCounter.IsDirty()) m_FPSCounter.ClearDirty();
    if (m_SRTimer.IsDirty()) m_SRTimer.ClearDirty();

    m_Window.OnProcess();
    m_Window.Render();
}

void BuiltinHUD::RestorePrimaryVisibility(bool showTitle, bool showFPS) {
    ShowTitle(showTitle);
    ShowFPS(showFPS);
}

void BuiltinHUD::EndLevel() {
    ShowSRTimer(false);
}

void BuiltinHUD::ShowTitle(bool show) {
    if (m_TitleElement) {
        m_TitleElement->SetVisible(show);
    }
}

void BuiltinHUD::ShowFPS(bool show) {
    if (m_FPSElement) {
        m_FPSElement->SetVisible(show);
    }
}

void BuiltinHUD::ShowSRTimer(bool show) {
    if (m_SRElement) {
        m_SRElement->SetVisible(show);
    }
}

void BuiltinHUD::StartSRTimer() {
    m_SRTimer.Start();
    if (auto container = HUDCast<HUDContainer>(m_SRElement)) {
        if (container->IsFadeEnabled()) container->SetFadeTarget(1.0f);
    }
}

void BuiltinHUD::PauseSRTimer() {
    m_SRTimer.Pause();
    if (auto container = HUDCast<HUDContainer>(m_SRElement)) {
        if (container->IsFadeEnabled()) container->SetFadeTarget(0.5f);
    }
}

void BuiltinHUD::ResetSRTimer() {
    m_SRTimer.Reset();
}

float BuiltinHUD::GetSRTime() const {
    return m_SRTimer.GetTime();
}

void BuiltinHUD::SetFPSUpdateFrequency(uint32_t frames) {
    const uint32_t normalized = frames > 0 ? frames : 1;
    if (m_FPSCounter.GetUpdateFrequency() != normalized) {
        m_FPSCounter.SetUpdateFrequency(normalized);
    }
}

void BuiltinHUD::UpdateTimerDisplay() {
    if (m_FPSElement) {
        if (auto textElement = HUDCast<HUDText>(m_FPSElement)) {
            if (m_FPSElement->IsVisible() &&
                (m_FPSCounter.IsDirty() || std::strlen(textElement->GetText()) == 0)) {
                textElement->SetText(m_FPSCounter.GetFormattedFps());
            }
        }
    }

    if (m_SRElement && m_SRElement->IsVisible()) {
        if (auto container = HUDCast<HUDContainer>(m_SRElement)) {
            if (auto value = container->FindChild("value")) {
                if (auto textElement = HUDCast<HUDText>(value)) {
                    if (m_SRTimer.IsDirty() || std::strcmp(textElement->GetText(), "  00:00:00.000") == 0) {
                        textElement->SetText(m_SRTimer.GetFormattedTime());
                    }
                }
            }
        }
    }
}

void BuiltinHUD::UpdateCheatState(bool cheatEnabled) {
    if (cheatEnabled == m_LastCheatState) {
        return;
    }

    m_LastCheatState = cheatEnabled;
    if (m_CheatElement) {
        m_CheatElement->SetVisible(cheatEnabled);
    }
}
