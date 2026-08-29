#include "BuiltinHUD.h"

#include <algorithm>
#include <cstring>

#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/Version.h"

const BuiltinHUD::Setting *BuiltinHUD::GetSettings(size_t &count) {
    static const Setting settings[] = {
        {"ShowTitle", &BuiltinHUD::m_ShowTitle,
         [](BuiltinHUD &hud, IProperty *property) { hud.ShowTitle(property->GetBoolean()); },
         OnChange, false},
        {"ShowFPS", &BuiltinHUD::m_ShowFPS,
         [](BuiltinHUD &hud, IProperty *property) { hud.ShowFPS(property->GetBoolean()); },
         OnChange, false},
        {"ShowSRTimer", &BuiltinHUD::m_ShowSRTimer,
         [](BuiltinHUD &hud, IProperty *property) { hud.ShowSRTimer(property->GetBoolean()); },
         OnChange | OnLevelInit, true},
        {"FPSUpdateFrequency", &BuiltinHUD::m_FPSUpdateFrequency,
         [](BuiltinHUD &hud, IProperty *property) {
             hud.SetFPSUpdateFrequency(static_cast<uint32_t>(std::max(1, property->GetInteger())));
         },
         Startup | OnChange, false},
    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 4,
                  "Every built-in HUD config property must have one settings-table entry");

    count = sizeof(settings) / sizeof(settings[0]);
    return settings;
}

void BuiltinHUD::InitConfig(IConfig &config) {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        this->*settings[i].property = config.GetProperty("HUD", settings[i].key);
    }

    config.SetCategoryComment("HUD", "HUD Settings");

    m_ShowTitle->SetComment("Show BML Title at top");
    m_ShowTitle->SetDefaultBoolean(true);

    m_ShowFPS->SetComment("Show FPS at top-left corner");
    m_ShowFPS->SetDefaultBoolean(true);

    m_ShowSRTimer->SetComment("Show SR Timer above Time Score");
    m_ShowSRTimer->SetDefaultBoolean(true);

    m_FPSUpdateFrequency->SetComment(
        "FPS counter update frequency in frames (higher values = less frequent updates, better performance)");
    m_FPSUpdateFrequency->SetDefaultInteger(30);
}

void BuiltinHUD::ApplyConfig() {
    ApplySettings(Startup);
}

bool BuiltinHUD::OnModifyConfig(const char *category, const char *key, IProperty *property) {
    if (!property || std::strcmp(category ? category : "", "HUD") != 0) {
        return false;
    }

    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        const Setting &setting = settings[i];
        if ((setting.when & OnChange) == 0 || this->*setting.property != property) {
            continue;
        }
        if (std::strcmp(key ? key : "", setting.key) != 0) {
            continue;
        }

        ApplySetting(setting, property);
        return true;
    }

    return false;
}

void BuiltinHUD::ApplySettings(ApplyWhen when) {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        if ((settings[i].when & static_cast<unsigned>(when)) != 0) {
            ApplySetting(settings[i], this->*settings[i].property);
        }
    }
}

void BuiltinHUD::ApplySetting(const Setting &setting, IProperty *property) {
    if (!setting.apply || !property) {
        return;
    }
    if (setting.requiresIngame && (!m_BML || !m_BML->IsIngame())) {
        return;
    }

    setting.apply(*this, property);
}

void BuiltinHUD::OnLoad(IBML &bml) {
    m_BML = &bml;
    m_LastCheatState = false;

    auto title = m_Window.AddText("title", "BML Plus " BML_VERSION, AnchorPoint::TopCenter);
    title->SetScale(1.2f);
    title->SetVisible(m_ShowTitle->GetBoolean());
    m_TitleElement = title;

    m_FPSElement = m_Window.AddText("fps", "FPS: 60", AnchorPoint::TopLeft);
    m_FPSElement->SetVisible(m_ShowFPS->GetBoolean());

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
    m_BML = nullptr;
}

void BuiltinHUD::OnProcess(float frameDeltaSeconds, float gameDeltaMilliseconds) {
    m_FPSCounter.Update(frameDeltaSeconds);
    m_SRTimer.Update(gameDeltaMilliseconds);

    UpdateCheatState(m_BML && m_BML->IsCheatEnabled());
    UpdateTimerDisplay();

    if (m_FPSCounter.IsDirty()) m_FPSCounter.ClearDirty();
    if (m_SRTimer.IsDirty()) m_SRTimer.ClearDirty();

    m_Window.OnProcess();
    m_Window.Render();
}

void BuiltinHUD::OnMenuStart() {
    ShowTitle(m_ShowTitle->GetBoolean());
    ShowFPS(m_ShowFPS->GetBoolean());
}

void BuiltinHUD::OnLevelStart() {
    ApplySettings(OnLevelInit);
    ResetSRTimer();
}

void BuiltinHUD::OnLevelExit() {
    ShowSRTimer(false);
}

int BuiltinHUD::GetMode() const {
    int mode = 0;
    if (m_ShowTitle && m_ShowTitle->GetBoolean()) mode |= HUD_TITLE;
    if (m_ShowFPS && m_ShowFPS->GetBoolean()) mode |= HUD_FPS;
    if (m_ShowSRTimer && m_ShowSRTimer->GetBoolean()) mode |= HUD_SR;
    return mode;
}

void BuiltinHUD::SetMode(int mode) {
    if (m_ShowTitle) {
        m_ShowTitle->SetBoolean((mode & HUD_TITLE) != 0);
        ShowTitle(m_ShowTitle->GetBoolean());
    }
    if (m_ShowFPS) {
        m_ShowFPS->SetBoolean((mode & HUD_FPS) != 0);
        ShowFPS(m_ShowFPS->GetBoolean());
    }
    if (m_ShowSRTimer) {
        m_ShowSRTimer->SetBoolean((mode & HUD_SR) != 0);
        ShowSRTimer(m_ShowSRTimer->GetBoolean());
    }
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
