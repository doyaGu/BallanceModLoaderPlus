#include "HUD/HUDRuntime.h"

#include <algorithm>
#include <cstring>

#include "Loader/ModContext.h"
#include "Config/Config.h"
#include "BML/Version.h"

const HUDRuntime::Setting *HUDRuntime::GetSettings(size_t &count) {
    static const Setting settings[] = {
        {"ShowTitle", &HUDRuntime::m_ShowTitle,
         [](HUDRuntime &hud, Property *property) { hud.ShowTitle(property->GetBoolean()); },
         OnChange, false},
        {"ShowFPS", &HUDRuntime::m_ShowFPS,
         [](HUDRuntime &hud, Property *property) { hud.ShowFPS(property->GetBoolean()); }, OnChange,
         false},
        {"ShowSRTimer", &HUDRuntime::m_ShowSRTimer,
         [](HUDRuntime &hud, Property *property) { hud.ShowSRTimer(property->GetBoolean()); },
         OnChange | OnLevelInit, true},
        {"FPSUpdateFrequency", &HUDRuntime::m_FPSUpdateFrequency,
         [](HUDRuntime &hud, Property *property) {
             hud.SetFPSUpdateFrequency(static_cast<uint32_t>(std::max(1, property->GetInteger())));
         },
         Startup | OnChange, false},
    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 4,
                  "Every built-in HUD config property must have one settings-table entry");

    count = sizeof(settings) / sizeof(settings[0]);
    return settings;
}

void HUDRuntime::InitConfig(Config &config) {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        this->*settings[i].property = config.GetProperty("HUD", settings[i].key);
    }

    config.SetCategoryComment("HUD", "HUD Settings");

    m_ShowTitle->SetComment("Show BML Title at top");
    m_ShowTitle->SetDefaultBoolean(true);

    m_ShowFPS->SetComment("Show FPS at top-left corner");
    m_ShowFPS->SetDefaultBoolean(false);

    m_ShowSRTimer->SetComment("Show SR Timer above Time Score");
    m_ShowSRTimer->SetDefaultBoolean(false);

    m_FPSUpdateFrequency->SetComment(
        "Rendered frames between FPS updates. Also refreshes after 250 ms when a frame is rendered.");
    m_FPSUpdateFrequency->SetDefaultInteger(30);
}

void HUDRuntime::ApplyConfig() {
    ApplySettings(Startup);
}

bool HUDRuntime::OnModifyConfig(const char *category, const char *key, Property *property) {
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

void HUDRuntime::ApplySettings(ApplyWhen when) {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        if ((settings[i].when & static_cast<unsigned>(when)) != 0) {
            ApplySetting(settings[i], this->*settings[i].property, when == OnLevelInit);
        }
    }
}

void HUDRuntime::ApplySetting(const Setting &setting, Property *property, bool levelStarting) {
    if (!setting.apply || !property) {
        return;
    }
    if (setting.requiresIngame && !levelStarting && (!m_BML || !m_BML->IsIngame())) {
        return;
    }

    setting.apply(*this, property);
}

void HUDRuntime::OnLoad(ModContext &bml) {
    m_BML = &bml;
    m_LastCheatState = false;

    auto title = m_Window.AddText("title", "BML Plus " BML_VERSION, AnchorPoint::TopCenter);
    title->SetScale(1.2f);
    title->SetVisible(m_ShowTitle->GetBoolean());
    m_TitleElement = title;

    m_FPSCounter.Reset();
    m_FPSElement = m_Window.AddText("fps", m_FPSCounter.GetFormattedFps(), AnchorPoint::TopLeft);
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

void HUDRuntime::OnUnload() {
    m_TitleElement = nullptr;
    m_FPSElement = nullptr;
    m_SRElement = nullptr;
    m_CheatElement = nullptr;
    m_BML = nullptr;
}

void HUDRuntime::OnProcess(uint32_t gameTick, float gameDeltaMilliseconds,
                           bool hasGameplayTimer) {
    if (!hasGameplayTimer || m_SRTimer.UsesManualClock())
        m_SRTimer.Update(gameTick, gameDeltaMilliseconds);

    UpdateCheatState(m_BML && m_BML->IsCheatEnabled());
    UpdateFPSDisplay();
    UpdateTimerDisplay();

    m_Window.OnProcess();
    m_Window.Render();
}

void HUDRuntime::OnRenderFrame() {
    m_FPSCounter.RecordFrame(FpsCounter::Clock::now());
}

void HUDRuntime::OnMenuStart() {
    ShowTitle(m_ShowTitle->GetBoolean());
    ShowFPS(m_ShowFPS->GetBoolean());
}

void HUDRuntime::OnLevelStart() {
    ApplySettings(OnLevelInit);
}

void HUDRuntime::OnLevelExit() {
    ShowSRTimer(false);
}

int HUDRuntime::GetMode() const {
    int mode = 0;
    if (m_ShowTitle && m_ShowTitle->GetBoolean()) mode |= HUD_TITLE;
    if (m_ShowFPS && m_ShowFPS->GetBoolean()) mode |= HUD_FPS;
    if (m_ShowSRTimer && m_ShowSRTimer->GetBoolean()) mode |= HUD_SR;
    return mode;
}

void HUDRuntime::SetMode(int mode) {
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

void HUDRuntime::ShowTitle(bool show) {
    if (m_TitleElement) {
        m_TitleElement->SetVisible(show);
    }
}

void HUDRuntime::ShowFPS(bool show) {
    if (m_FPSElement) {
        m_FPSElement->SetVisible(show);
        if (show)
            UpdateFPSDisplay();
    }
}

void HUDRuntime::ShowSRTimer(bool show) {
    if (m_SRElement) {
        m_SRElement->SetVisible(show);
        if (show)
            UpdateSRTimerDisplay();
    }
}

void HUDRuntime::StartSRTimer(uint32_t gameTick) {
    m_SRTimer.Start(gameTick);
    UpdateSRTimerFade();
}

void HUDRuntime::PauseSRTimer() {
    m_SRTimer.Pause();
    UpdateSRTimerFade();
}

void HUDRuntime::UpdateGameplayTimer(unsigned inputs, float deltaTimeMs) {
    m_SRTimer.UpdateGameplay(inputs, deltaTimeMs);
    if (inputs != 0)
        UpdateSRTimerFade();
}

void HUDRuntime::UpdateSRTimerFade() {
    if (auto container = HUDCast<HUDContainer>(m_SRElement)) {
        if (container->IsFadeEnabled())
            container->SetFadeTarget(m_SRTimer.IsRunning() ? 1.0f : 0.5f);
    }
}

void HUDRuntime::ResetSRTimer() {
    m_SRTimer.Reset();
}

float HUDRuntime::GetSRTime() const {
    return m_SRTimer.GetTime();
}

void HUDRuntime::SetFPSUpdateFrequency(uint32_t frames) {
    const uint32_t normalized = frames > 0 ? frames : 1;
    if (m_FPSCounter.GetUpdateFrequency() != normalized) {
        m_FPSCounter.SetUpdateFrequency(normalized);
    }
}

void HUDRuntime::UpdateFPSDisplay() {
    if (!m_FPSElement || !m_FPSElement->IsVisible() || !m_FPSCounter.IsDirty())
        return;
    if (auto textElement = HUDCast<HUDText>(m_FPSElement)) {
        textElement->SetText(m_FPSCounter.GetFormattedFps());
        m_FPSCounter.ClearDirty();
    }
}

void HUDRuntime::UpdateTimerDisplay() {
    if (m_SRElement && m_SRElement->IsVisible() && m_SRTimer.IsDirty())
        UpdateSRTimerDisplay();
}

void HUDRuntime::UpdateSRTimerDisplay() {
    if (auto container = HUDCast<HUDContainer>(m_SRElement)) {
        if (auto text = HUDCast<HUDText>(container->FindChild("value")))
            text->SetText(m_SRTimer.GetFormattedTime());
    }
}

void HUDRuntime::UpdateCheatState(bool cheatEnabled) {
    if (cheatEnabled == m_LastCheatState) {
        return;
    }

    m_LastCheatState = cheatEnabled;
    if (m_CheatElement) {
        m_CheatElement->SetVisible(cheatEnabled);
    }
}
