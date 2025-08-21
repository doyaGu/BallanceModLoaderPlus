#include "HUD.h"

#include "ModContext.h"

HUDElement::HUDElement(const char *text, AnchorPoint anchor)
    : m_Text(text ? text : ""),
      m_Anchor(anchor),
      m_Offset(0.0f, 0.0f),
      m_Color(IM_COL32_WHITE),
      m_Scale(1.0f),
      m_Visible(true) {
}

void HUDElement::SetText(const char *text) {
    m_Text = text ? text : "";
}

const char *HUDElement::GetText() const {
    return m_Text.c_str();
}

void HUDElement::SetVisible(bool visible) {
    m_Visible = visible;
}

bool HUDElement::IsVisible() const {
    return m_Visible;
}

void HUDElement::SetAnchor(AnchorPoint anchor) {
    m_Anchor = anchor;
}

void HUDElement::SetOffset(float x, float y) {
    m_Offset = ImVec2(x, y);
}

void HUDElement::SetColor(ImU32 color) {
    m_Color = color;
}

void HUDElement::SetScale(float scale) {
    m_Scale = scale > 0.0f ? scale : 1.0f;
}

void HUDElement::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || m_Text.empty() || !drawList)
        return;

    float oldScale = ImGui::GetFont()->Scale;

    ImGui::GetFont()->Scale *= m_Scale;
    ImGui::PushFont(ImGui::GetFont());

    ImVec2 textSize = ImGui::CalcTextSize(m_Text.c_str());
    ImVec2 pos = CalculatePosition(textSize, viewportSize);

    drawList->AddText(pos, m_Color, m_Text.c_str());

    ImGui::GetFont()->Scale = oldScale;
    ImGui::PopFont();
}

ImVec2 HUDElement::CalculatePosition(const ImVec2 &textSize, const ImVec2 &viewportSize) const {
    ImVec2 pos(0.0f, 0.0f);

    // Calculate base position based on anchor
    switch (m_Anchor) {
    case AnchorPoint::TopLeft:
        pos = ImVec2(0.0f, 0.0f);
        break;
    case AnchorPoint::TopCenter:
        pos = ImVec2((viewportSize.x - textSize.x) * 0.5f, 0.0f);
        break;
    case AnchorPoint::TopRight:
        pos = ImVec2(viewportSize.x - textSize.x, 0.0f);
        break;
    case AnchorPoint::MiddleLeft:
        pos = ImVec2(0.0f, (viewportSize.y - textSize.y) * 0.5f);
        break;
    case AnchorPoint::MiddleCenter:
        pos = ImVec2((viewportSize.x - textSize.x) * 0.5f, (viewportSize.y - textSize.y) * 0.5f);
        break;
    case AnchorPoint::MiddleRight:
        pos = ImVec2(viewportSize.x - textSize.x, (viewportSize.y - textSize.y) * 0.5f);
        break;
    case AnchorPoint::BottomLeft:
        pos = ImVec2(0.0f, viewportSize.y - textSize.y);
        break;
    case AnchorPoint::BottomCenter:
        pos = ImVec2((viewportSize.x - textSize.x) * 0.5f, viewportSize.y - textSize.y);
        break;
    case AnchorPoint::BottomRight:
        pos = ImVec2(viewportSize.x - textSize.x, viewportSize.y - textSize.y);
        break;
    }

    // Apply offset (converting from 0-1 range to pixel coordinates)
    pos.x += m_Offset.x * viewportSize.x;
    pos.y += m_Offset.y * viewportSize.y;

    return pos;
}

SRTimer::SRTimer() : m_Time(0.0f), m_Running(false) {
    strcpy(m_FormattedTime, "00:00:00.000");
}

void SRTimer::Reset() {
    m_Time = 0.0f;
    UpdateFormattedTime();
}

void SRTimer::Start() {
    m_Running = true;
}

void SRTimer::Pause() {
    m_Running = false;
}

void SRTimer::Update(float deltaTime) {
    if (m_Running) {
        m_Time += deltaTime;
        UpdateFormattedTime();
    }
}

float SRTimer::GetTime() const {
    return m_Time;
}

const char *SRTimer::GetFormattedTime() const {
    return m_FormattedTime;
}

bool SRTimer::IsRunning() const {
    return m_Running;
}

void SRTimer::UpdateFormattedTime() const {
    int counter = static_cast<int>(m_Time);
    int ms = counter % 1000;
    counter /= 1000;
    int s = counter % 60;
    counter /= 60;
    int m = counter % 60;
    counter /= 60;
    int h = counter % 100;

    sprintf(m_FormattedTime, "%02d:%02d:%02d.%03d", h, m, s, ms);
}

FpsCounter::FpsCounter(uint32_t sampleCount)
    : m_SampleCount(std::min<uint32_t>(sampleCount, 120)),
      m_CurrentIndex(0),
      m_FrameCounter(0),
      m_UpdateFrequency(15),
      // Update every 15 frames by default
      m_CurrentAverageFps(0.0f) {
    // Initialize frame times to reasonable default (16.7ms ~ 60 FPS)
    std::fill(m_FrameTimes.begin(), m_FrameTimes.end(), 16.7f);
    strcpy(m_FormattedFps, "FPS: 60");
}

void FpsCounter::Update(float frameTime) {
    // Ensure frameTime is valid
    if (frameTime <= 0.0f) {
        frameTime = 16.7f; // Default to ~60 FPS
    }

    // Add new frame time to buffer
    m_FrameTimes[m_CurrentIndex] = frameTime;
    m_CurrentIndex = (m_CurrentIndex + 1) % m_SampleCount;

    // Check if it's time to recalculate the average
    m_FrameCounter++;
    if (m_FrameCounter >= m_UpdateFrequency) {
        RecalculateAverage();
        m_FrameCounter = 0;
    }
}

float FpsCounter::GetAverageFps() const {
    return m_CurrentAverageFps;
}

const char *FpsCounter::GetFormattedFps() const {
    return m_FormattedFps;
}

void FpsCounter::SetUpdateFrequency(uint32_t frames) {
    m_UpdateFrequency = frames > 0 ? frames : 1;
}

uint32_t FpsCounter::GetUpdateFrequency() const {
    return m_UpdateFrequency;
}

void FpsCounter::RecalculateAverage() {
    float totalTime = 0.0f;
    for (uint32_t i = 0; i < m_SampleCount; ++i) {
        totalTime += m_FrameTimes[i];
    }

    // Calculate average frame time and convert to FPS
    float averageFrameTime = totalTime / static_cast<float>(m_SampleCount);
    m_CurrentAverageFps = 1000.0f / averageFrameTime;

    // Format the FPS string
    sprintf(m_FormattedFps, "FPS: %d", static_cast<int>(m_CurrentAverageFps + 0.5f));
}

HUD::HUD() : Bui::Window("HUD") {
    Show();
    SetupDefaultElements();
}

HUD::~HUD() = default;

ImGuiWindowFlags HUD::GetFlags() {
    return ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;
}

void HUD::OnBegin() {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
}

void HUD::OnDraw() {
    const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    if (m_TitleElement) {
        m_TitleElement->Draw(drawList, viewportSize);
    }

    if (m_FPSElement) {
        m_FPSElement->Draw(drawList, viewportSize);
    }

    if (BML_GetModContext()->IsCheatEnabled() && m_CheatModeElement) {
        m_CheatModeElement->Draw(drawList, viewportSize);
    }

    if (m_SRTimerLabelElement && m_SRTimerValueElement) {
        m_SRTimerLabelElement->Draw(drawList, viewportSize);
        m_SRTimerValueElement->Draw(drawList, viewportSize);
    }

    for (const auto &element : m_CustomElements) {
        element->Draw(drawList, viewportSize);
    }
}

void HUD::OnProcess() {
    // Update FPS counter
    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);
    m_FPSCounter.Update(stats.TotalFrameTime);
    if (m_FPSElement) {
        m_FPSElement->SetText(m_FPSCounter.GetFormattedFps());
    }

    // Update SR timer
    m_SRTimer.Update(BML_GetCKContext()->GetTimeManager()->GetLastDeltaTime());
    UpdateTimerDisplay();
}

void HUD::ShowTitle(bool show) {
    if (m_TitleElement) {
        m_TitleElement->SetVisible(show);
    }
}

void HUD::ShowFPS(bool show) {
    if (m_FPSElement) {
        m_FPSElement->SetVisible(show);
    }
}

void HUD::SetFPSUpdateFrequency(uint32_t frames) {
    m_FPSCounter.SetUpdateFrequency(frames);
}

void HUD::SetFPSPosition(AnchorPoint anchor, float offsetX, float offsetY) {
    if (m_FPSElement) {
        m_FPSElement->SetAnchor(anchor);
        m_FPSElement->SetOffset(offsetX, offsetY);
    }
}

void HUD::StartSRTimer() {
    m_SRTimer.Start();
}

void HUD::PauseSRTimer() {
    m_SRTimer.Pause();
}

void HUD::ResetSRTimer() {
    m_SRTimer.Reset();
    m_SRTimer.Pause();
    UpdateTimerDisplay();
}

void HUD::ShowSRTimer(bool show) {
    if (m_SRTimerLabelElement && m_SRTimerValueElement) {
        m_SRTimerLabelElement->SetVisible(show);
        m_SRTimerValueElement->SetVisible(show);
    }
}

void HUD::SetSRTimerPosition(AnchorPoint anchor, float offsetX, float offsetY) {
    if (m_SRTimerLabelElement && m_SRTimerValueElement) {
        // Position the label
        m_SRTimerLabelElement->SetAnchor(anchor);
        m_SRTimerLabelElement->SetOffset(offsetX, offsetY);

        // Position the value beneath the label (adjust if needed)
        m_SRTimerValueElement->SetAnchor(anchor);
        m_SRTimerValueElement->SetOffset(offsetX + 0.02f, offsetY + 0.025f);
    }
}

float HUD::GetSRTime() const {
    return m_SRTimer.GetTime();
}

HUDElement *HUD::AddElement(const char *text, AnchorPoint anchor) {
    auto element = std::make_unique<HUDElement>(text, anchor);
    HUDElement *elementPtr = element.get();
    m_CustomElements.push_back(std::move(element));
    return elementPtr;
}

bool HUD::RemoveElement(HUDElement *element) {
    if (!element) {
        return false;
    }

    for (auto it = m_CustomElements.begin(); it != m_CustomElements.end(); ++it) {
        if (it->get() == element) {
            m_CustomElements.erase(it);
            return true;
        }
    }

    return false;
}

void HUD::SetupDefaultElements() {
    // Title element (centered at top)
    m_TitleElement = std::make_unique<HUDElement>("BML Plus " BML_VERSION, AnchorPoint::TopCenter);
    m_TitleElement->SetScale(1.2f);
    m_TitleElement->SetVisible(false);

    // FPS counter (top-left corner)
    m_FPSElement = std::make_unique<HUDElement>("FPS: 60", AnchorPoint::TopLeft);
    m_FPSElement->SetVisible(false);

    // SR Timer elements (bottom-left area)
    m_SRTimerLabelElement = std::make_unique<HUDElement>("SR Timer", AnchorPoint::BottomLeft);
    m_SRTimerLabelElement->SetOffset(0.03f, -0.155f);
    m_SRTimerLabelElement->SetVisible(false);

    m_SRTimerValueElement = std::make_unique<HUDElement>("00:00:00.000", AnchorPoint::BottomLeft);
    m_SRTimerValueElement->SetOffset(0.05f, -0.13f);
    m_SRTimerValueElement->SetVisible(false);

    // Cheat mode indicator (centered at bottom)
    m_CheatModeElement = std::make_unique<HUDElement>("Cheat Mode Enabled", AnchorPoint::BottomCenter);
    m_CheatModeElement->SetOffset(0.0f, -0.12f);
    m_CheatModeElement->SetColor(IM_COL32(255, 200, 60, 255)); // Yellow-orange color
}

void HUD::UpdateTimerDisplay() {
    if (m_SRTimerValueElement) {
        m_SRTimerValueElement->SetText(m_SRTimer.GetFormattedTime());
    }
}
