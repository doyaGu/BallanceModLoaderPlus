#include "HUD.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>

#include "ModContext.h"
#include "IniFile.h"
#include "PathUtils.h"
#include "StringUtils.h"

HUDElement::HUDElement(const char *text, AnchorPoint anchor)
    : m_AnsiText(text ? text : ""),
      m_Anchor(anchor),
      m_Offset(0.0f, 0.0f),
      m_Color(IM_COL32_WHITE),
      m_Scale(1.0f),
      m_Visible(true) {}

void HUDElement::SetText(const char *text) {
    const char *newText = text ? text : "";
    // Early-out to avoid unnecessary parsing and cache invalidation
    if (m_AnsiText.GetOriginalText() == newText) {
        return;
    }
    m_AnsiText.SetText(newText);
    Invalidate();
}

const char *HUDElement::GetText() const {
    return m_AnsiText.GetOriginalText().c_str();
}

void HUDElement::SetVisible(bool visible) {
    if (m_Visible == visible) return;
    m_Visible = visible;
}

bool HUDElement::IsVisible() const {
    return m_Visible;
}

void HUDElement::SetAnchor(AnchorPoint anchor) {
    if (m_Anchor == anchor) return;
    m_Anchor = anchor;
}

void HUDElement::SetOffset(float x, float y) {
    if (m_Offset.x == x && m_Offset.y == y) return;
    m_Offset = ImVec2(x, y);
}

void HUDElement::SetColor(ImU32 color) {
    if (m_Color == color) return;
    m_Color = color;
}

void HUDElement::SetScale(float scale) {
    float v = scale > 0.0f ? scale : 1.0f;
    if (m_Scale == v) return;
    m_Scale = v;
}

void HUDElement::SetWrapWidthPx(float px) { if (m_WrapWidthPx == px) return; m_WrapWidthPx = px; }
void HUDElement::SetWrapWidthFrac(float frac) { if (m_WrapWidthFrac == frac) return; m_WrapWidthFrac = frac; }
void HUDElement::SetTabColumns(int columns) { int v = std::max(1, columns); if (m_TabColumns == v) return; m_TabColumns = v; }

void HUDElement::EnablePanel(bool enabled) { if (m_DrawPanel == enabled) return; m_DrawPanel = enabled; }
void HUDElement::SetPanelColors(ImU32 bg, ImU32 border) { if (m_PanelBg == bg && m_PanelBorder == border) return; m_PanelBg = bg; m_PanelBorder = border; }
void HUDElement::SetPanelBgColor(ImU32 bg) { if (m_PanelBg == bg) return; m_PanelBg = bg; }
void HUDElement::SetPanelBorderColor(ImU32 border) { if (m_PanelBorder == border) return; m_PanelBorder = border; }
void HUDElement::SetPanelPadding(float padPx) { float v = std::max(0.0f, padPx); if (m_PanelPaddingPx == v) return; m_PanelPaddingPx = v; }
void HUDElement::SetPanelBorderThickness(float px) { float v = std::max(0.0f, px); if (m_PanelBorderThickness == v) return; m_PanelBorderThickness = v; }
void HUDElement::SetPanelRounding(float px) { float v = std::max(0.0f, px); if (m_PanelRounding == v) return; m_PanelRounding = v; }

void HUDElement::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || m_AnsiText.IsEmpty() || !drawList)
        return;

    const float fontSize = ImGui::GetStyle().FontSizeBase * m_Scale;

    ImGui::PushFont(nullptr, fontSize);

    if (!m_AnsiText.IsEmpty()) {
        // Resolve wrap width
        float wrapWidth = FLT_MAX;
        if (m_WrapWidthPx > 0.0f) wrapWidth = m_WrapWidthPx;
        else if (m_WrapWidthFrac > 0.0f) wrapWidth = viewportSize.x * m_WrapWidthFrac;

        ImVec2 textSize = AnsiText::CalculateSize(m_AnsiText, wrapWidth, fontSize, m_TabColumns);
        ImVec2 pos = CalculatePosition(textSize, viewportSize);

        // Optional panel background/border for TUI block
        if (m_DrawPanel) {
            ImVec2 pad(m_PanelPaddingPx, m_PanelPaddingPx);
            ImVec2 p0 = ImVec2(pos.x - pad.x, pos.y - pad.y);
            ImVec2 p1 = ImVec2(pos.x + textSize.x + pad.x, pos.y + textSize.y + pad.y);
            if ((m_PanelBg >> IM_COL32_A_SHIFT) & 0xFF)
                drawList->AddRectFilled(p0, p1, m_PanelBg, m_PanelRounding);
            if (m_PanelBorderThickness > 0.0f && ((m_PanelBorder >> IM_COL32_A_SHIFT) & 0xFF))
                drawList->AddRect(p0, p1, m_PanelBorder, m_PanelRounding, 0, m_PanelBorderThickness);
        }

        const float alpha = ((m_Color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
        AnsiText::Renderer::DrawText(drawList, m_AnsiText, pos, wrapWidth, alpha, fontSize, m_TabColumns);
    }

    ImGui::PopFont();
}

HUDElement *HUDContainer::AddChild(const char *text) {
    auto up = std::make_unique<HUDElement>(text ? text : "", AnchorPoint::TopLeft);
    HUDElement *ptr = up.get();
    m_Children.push_back(std::move(up));
    return ptr;
}

HUDElement *HUDContainer::AddChildNamed(const std::string &name, const char *text) {
    const auto it = m_NamedChildren.find(name);
    if (it != m_NamedChildren.end()) return it->second;
    HUDElement *child = AddChild(text);
    m_NamedChildren[name] = child;
    return child;
}

HUDElement *HUDContainer::FindChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    return (it != m_NamedChildren.end()) ? it->second : nullptr;
}

bool HUDContainer::RemoveChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    if (it == m_NamedChildren.end()) return false;
    HUDElement *ptr = it->second;
    for (auto vit = m_Children.begin(); vit != m_Children.end(); ++vit) {
        if (vit->get() == ptr) { m_Children.erase(vit); break; }
    }
    m_NamedChildren.erase(it);
    return true;
}

HUDContainer *HUDContainer::AddContainerChild(HUDLayoutKind kind, const std::string &name, int gridCols) {
    auto up = std::make_unique<HUDContainer>(kind, gridCols);
    HUDContainer *ptr = up.get();
    m_Children.push_back(std::move(up));
    if (!name.empty()) m_NamedChildren[name] = ptr;
    return ptr;
}

std::unique_ptr<HUDElement> HUDContainer::StealChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    if (it == m_NamedChildren.end()) return nullptr;
    HUDElement *ptr = it->second;
    std::unique_ptr<HUDElement> out;
    for (auto vit = m_Children.begin(); vit != m_Children.end(); ++vit) {
        if (vit->get() == ptr) { out = std::move(*vit); m_Children.erase(vit); break; }
    }
    m_NamedChildren.erase(it);
    return out;
}

void HUDContainer::InsertChild(std::unique_ptr<HUDElement> up, const std::string &name) {
    HUDElement *ptr = up.get();
    if (!name.empty()) m_NamedChildren[name] = ptr;
    m_Children.push_back(std::move(up));
}

void HUDContainer::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList) return;

    // Compute base position and content size
    const float fontSize = ImGui::GetStyle().FontSizeBase * m_Scale;
    ImGui::PushFont(nullptr, fontSize);

    // Measure children
    std::vector<ImVec2> sizes;
    sizes.reserve(m_Children.size());
    ImVec2 content(0, 0);
    if (m_Kind == HUDLayoutKind::Vertical) {
        for (auto &c : m_Children) {
            ImVec2 s = c->CalculateAnsiTextSize(viewportSize);
            sizes.push_back(s);
            content.x = std::max(content.x, s.x);
            if (!sizes.empty()) content.y += m_SpacingPx;
            content.y += s.y;
        }
        if (!m_Children.empty()) content.y -= m_SpacingPx; // remove last spacing
    } else if (m_Kind == HUDLayoutKind::Horizontal) {
        for (auto &c : m_Children) {
            ImVec2 s = c->CalculateAnsiTextSize(viewportSize);
            sizes.push_back(s);
            content.y = std::max(content.y, s.y);
            if (!sizes.empty()) content.x += m_SpacingPx;
            content.x += s.x;
        }
        if (!m_Children.empty()) content.x -= m_SpacingPx;
    } else { // Grid
        int cols = std::max(1, m_GridCols);
        std::vector<float> colW(cols, 0.0f);
        float rowH = 0.0f, maxW = 0.0f, totalH = 0.0f;
        int colIndex = 0;
        for (auto &c : m_Children) {
            ImVec2 s = c->CalculateAnsiTextSize(viewportSize);
            sizes.push_back(s);
            rowH = std::max(rowH, s.y);
            // accumulate max column widths, spacing accounted when summing row width below
            colW[colIndex] = std::max(colW[colIndex], s.x);
            ++colIndex;
            if (colIndex == cols) {
                float sumW = 0.0f; for (int i=0;i<cols;++i) { if (i>0) sumW += m_SpacingPx; sumW += colW[i]; }
                maxW = std::max(maxW, sumW);
                totalH += rowH + m_SpacingPx;
                rowH = 0.0f; colIndex = 0; std::fill(colW.begin(), colW.end(), 0.0f);
            }
        }
        if (colIndex != 0) {
            float sumW = 0.0f; for (int i=0;i<colIndex;++i) { if (i>0) sumW += m_SpacingPx; sumW += colW[i]; }
            maxW = std::max(maxW, sumW);
            totalH += rowH;
        } else if (totalH > 0.0f) {
            totalH -= m_SpacingPx;
        }
        content = ImVec2(maxW, totalH);
    }

    ImVec2 pos = CalculatePosition(content, viewportSize);

    // Optional panel
    if (m_DrawPanel) {
        ImVec2 pad(m_PanelPaddingPx, m_PanelPaddingPx);
        ImVec2 p0 = ImVec2(pos.x - pad.x, pos.y - pad.y);
        ImVec2 p1 = ImVec2(pos.x + content.x + pad.x, pos.y + content.y + pad.y);
        if ((m_PanelBg >> IM_COL32_A_SHIFT) & 0xFF)
            drawList->AddRectFilled(p0, p1, m_PanelBg, m_PanelRounding);
        if (m_PanelBorderThickness > 0.0f && ((m_PanelBorder >> IM_COL32_A_SHIFT) & 0xFF))
            drawList->AddRect(p0, p1, m_PanelBorder, m_PanelRounding, 0, m_PanelBorderThickness);
    }

    // Layout children
    if (m_Kind == HUDLayoutKind::Vertical) {
        float y = 0.0f;
        for (size_t i = 0; i < m_Children.size(); ++i) {
            auto &c = m_Children[i];
            ImVec2 s = sizes[i];
            float dx = 0.0f;
            if (m_AlignX == AlignX::Center) dx = (content.x - s.x) * 0.5f;
            else if (m_AlignX == AlignX::Right) dx = std::max(0.0f, content.x - s.x);
            // Temporarily set child anchor/offset to absolute pixel position
            AnchorPoint oldA = c->GetAnchor(); ImVec2 oldOff = c->GetOffset();
            c->SetAnchor(AnchorPoint::TopLeft);
            c->SetOffset((pos.x + dx) / viewportSize.x, (pos.y + y) / viewportSize.y);
            c->Draw(drawList, viewportSize);
            c->SetAnchor(oldA); c->SetOffset(oldOff.x, oldOff.y);
            y += s.y + m_SpacingPx;
        }
    } else if (m_Kind == HUDLayoutKind::Horizontal) {
        float x = 0.0f;
        for (size_t i = 0; i < m_Children.size(); ++i) {
            auto &c = m_Children[i];
            ImVec2 s = sizes[i];
            float dy = 0.0f;
            if (m_AlignY == AlignY::Middle) dy = (content.y - s.y) * 0.5f;
            else if (m_AlignY == AlignY::Bottom) dy = std::max(0.0f, content.y - s.y);
            AnchorPoint oldA = c->GetAnchor(); ImVec2 oldOff = c->GetOffset();
            c->SetAnchor(AnchorPoint::TopLeft);
            c->SetOffset((pos.x + x) / viewportSize.x, (pos.y + dy) / viewportSize.y);
            c->Draw(drawList, viewportSize);
            c->SetAnchor(oldA); c->SetOffset(oldOff.x, oldOff.y);
            x += s.x + m_SpacingPx;
        }
    } else { // Grid
        int cols = std::max(1, m_GridCols);
        // compute column widths again
        std::vector<float> colW(cols, 0.0f);
        for (size_t i=0;i<sizes.size();++i) { int ci = (int)(i % cols); colW[ci] = std::max(colW[ci], sizes[i].x); }
        int colIndex = 0; float x = 0.0f; float y = 0.0f; float rowH = 0.0f;
        for (size_t i = 0; i < m_Children.size(); ++i) {
            auto &c = m_Children[i]; ImVec2 s = sizes[i];
            float cw = colW[colIndex];
            float dx = 0.0f;
            AlignX ax = m_CellAlignX;
            if (ax == AlignX::Center) dx = std::max(0.0f, (cw - s.x) * 0.5f);
            else if (ax == AlignX::Right) dx = std::max(0.0f, cw - s.x);
            float dy = 0.0f;
            AlignY ay = m_CellAlignY;
            if (ay == AlignY::Middle) dy = std::max(0.0f, (rowH - s.y) * 0.5f);
            else if (ay == AlignY::Bottom) dy = std::max(0.0f, rowH - s.y);
            AnchorPoint oldA = c->GetAnchor(); ImVec2 oldOff = c->GetOffset();
            c->SetAnchor(AnchorPoint::TopLeft);
            c->SetOffset((pos.x + x + dx) / viewportSize.x, (pos.y + y + dy) / viewportSize.y);
            c->Draw(drawList, viewportSize);
            c->SetAnchor(oldA); c->SetOffset(oldOff.x, oldOff.y);
            rowH = std::max(rowH, s.y);
            ++colIndex;
            if (colIndex == cols) { colIndex = 0; x = 0.0f; y += rowH + m_SpacingPx; rowH = 0.0f; }
            else { x += colW[colIndex-1] + m_SpacingPx; }
        }
    }

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

ImVec2 HUDElement::CalculateAnsiTextSize(const ImVec2 &viewportSize) const {
    if (m_AnsiText.IsEmpty()) {
        return ImVec2(0.0f, 0.0f);
    }

    float wrapWidth = FLT_MAX;
    if (m_WrapWidthPx > 0.0f) wrapWidth = m_WrapWidthPx;
    else if (m_WrapWidthFrac > 0.0f) wrapWidth = viewportSize.x * m_WrapWidthFrac;
    const float fontSize = ImGui::GetStyle().FontSizeBase * m_Scale;
    if (m_MeasureCache.textVersion == m_TextVersion &&
        m_MeasureCache.wrapWidth == wrapWidth &&
        m_MeasureCache.fontSize == fontSize &&
        m_MeasureCache.tabCols == m_TabColumns) {
        return m_MeasureCache.size;
    }
    ImVec2 sz = AnsiText::CalculateSize(m_AnsiText, wrapWidth, fontSize, m_TabColumns);
    const_cast<HUDElement*>(this)->m_MeasureCache.textVersion = m_TextVersion;
    const_cast<HUDElement*>(this)->m_MeasureCache.wrapWidth = wrapWidth;
    const_cast<HUDElement*>(this)->m_MeasureCache.fontSize = fontSize;
    const_cast<HUDElement*>(this)->m_MeasureCache.tabCols = m_TabColumns;
    const_cast<HUDElement*>(this)->m_MeasureCache.size = sz;
    return sz;
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

void HUD::OnPreBegin() {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
}

void HUD::OnDraw() {
    const ImVec2 viewportSize = ImGui::GetWindowSize();
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
        if (!m_ActivePage.empty()) {
            if (!element->GetPage().empty() && element->GetPage() != m_ActivePage) continue;
        }
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

    // Expand templates for custom/named elements
    auto expand = [&](HUDElement *e) {
        if (!e || !e->HasTemplate()) return;
        const std::string &tpl = e->GetTemplate();
        std::string out; out.reserve(tpl.size());
        for (size_t i = 0; i < tpl.size(); ) {
            char ch = tpl[i];
            if (ch == '{') {
                size_t j = tpl.find('}', i + 1);
                if (j != std::string::npos) {
                    std::string key = tpl.substr(i + 1, j - (i + 1));
                    std::string val;
                    if (ResolveVar(key, val)) out += val; else { out.push_back('{'); out += key; out.push_back('}'); }
                    i = j + 1; continue;
                }
            }
            out.push_back(ch); ++i;
        }
        e->SetText(out.c_str());
    };
    for (auto &up : m_CustomElements) expand(up.get());
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
    if (m_FPSCounter.GetUpdateFrequency() == (frames > 0 ? frames : 1)) return;
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
    ApplyStyle(elementPtr);
    elementPtr->SetAnchor(anchor);
    m_CustomElements.push_back(std::move(element));
    return elementPtr;
}

HUDElement *HUD::AddAnsiElement(const char *ansiText, AnchorPoint anchor) {
    auto element = std::make_unique<HUDElement>(ansiText, anchor);
    HUDElement *elementPtr = element.get();
    ApplyStyle(elementPtr);
    elementPtr->SetAnchor(anchor);
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

HUDElement *HUD::GetOrCreate(const std::string &id) {
    const auto it = m_Named.find(id);
    if (it != m_Named.end()) return it->second;
    auto up = std::make_unique<HUDElement>("", AnchorPoint::TopLeft);
    HUDElement *ptr = up.get();
    ApplyStyle(ptr);
    m_CustomElements.push_back(std::move(up));
    m_Named[id] = ptr;
    return ptr;
}

HUDElement *HUD::Find(const std::string &id) {
    const auto it = m_Named.find(id);
    return (it != m_Named.end()) ? it->second : nullptr;
}

bool HUD::Remove(const std::string &id) {
    const auto it = m_Named.find(id);
    if (it == m_Named.end()) return false;
    HUDElement *ptr = it->second;
    // Erase from vector
    for (auto vit = m_CustomElements.begin(); vit != m_CustomElements.end(); ++vit) {
        if (vit->get() == ptr) {
            m_CustomElements.erase(vit);
            break;
        }
    }
    m_Named.erase(it);
    return true;
}

std::vector<std::string> HUD::ListIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_Named.size());
    for (const auto &kv : m_Named) ids.push_back(kv.first);
    return ids;
}

bool HUD::ResolveVar(const std::string &name, std::string &out) const {
    if (m_VarProvider && m_VarProvider(name, out)) return true;
    if (name == "fps") {
        char buf[16]; snprintf(buf, sizeof(buf), "%d", (int)(m_FPSCounter.GetAverageFps() + 0.5f));
        out = buf; return true;
    }
    if (name == "fps_line") { out = m_FPSCounter.GetFormattedFps(); return true; }
    if (name == "sr" || name == "time" || name == "sr_time") { out = m_SRTimer.GetFormattedTime(); return true; }
    if (name == "cheat") { out = BML_GetModContext()->IsCheatEnabled() ? "on" : "off"; return true; }
    return false;
}

HUDElement *HUD::FindByPath(const std::string &path) {
    if (path.empty()) return nullptr;
    auto parts = utils::SplitString(path, '.');
    if (parts.empty()) return nullptr;

    HUDElement *cur = Find(parts[0]);
    if (!cur) return nullptr;
    for (size_t i = 1; i < parts.size(); ++i) {
        HUDContainer *cont = dynamic_cast<HUDContainer *>(cur);
        if (!cont) return nullptr;
        cur = cont->FindChild(parts[i]);
        if (!cur) return nullptr;
    }
    return cur;
}

static HUDLayoutKind HUD_InferKindFromName(const std::string &name) {
    std::string s = utils::ToLower(name);
    if (s.find("grid") != std::string::npos) return HUDLayoutKind::Grid;
    if (s.find("hstack") != std::string::npos || s.find("hbox") != std::string::npos || 
        s.find("row") != std::string::npos || s.find("horiz") != std::string::npos)
        return HUDLayoutKind::Horizontal;
    if (s.find("vstack") != std::string::npos || s.find("vbox") != std::string::npos || 
        s.find("col") != std::string::npos)
        return HUDLayoutKind::Vertical;
    return HUDLayoutKind::Vertical;
}

HUDElement *HUD::GetOrCreateChild(const std::string &containerId, const std::string &childId) {
    HUDElement *contE = Find(containerId);
    if (!contE) return nullptr;
    HUDContainer *cont = dynamic_cast<HUDContainer *>(contE);
    if (!cont) return nullptr;
    HUDElement *child = cont->FindChild(childId);
    if (!child) child = cont->AddChildNamed(childId, "");
    return child;
}

HUDContainer *HUD::EnsureContainerPath(const std::string &path, HUDLayoutKind defaultKindForNew) {
    if (path.empty()) return nullptr;
    auto parts = utils::SplitString(path, '.');
    if (parts.empty()) return nullptr;
    // Head/root
    HUDContainer *cur = nullptr;
    HUDElement *head = Find(parts[0]);
    if (!head) {
        // Decide kind via policy
        HUDLayoutKind k = m_CreatePolicy ? m_CreatePolicy("", parts[0], 0) : HUD_InferKindFromName(parts[0]);
        if (k == HUDLayoutKind::Horizontal) cur = AddHStack();
        else if (k == HUDLayoutKind::Grid) cur = AddGrid(1);
        else cur = AddVStack();
        Register(parts[0], cur);
    } else {
        cur = dynamic_cast<HUDContainer *>(head);
        if (!cur) return nullptr;
    }
    // Tail segments
    std::string built = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string &seg = parts[i];
        HUDElement *e = cur->FindChild(seg);
        HUDContainer *next = dynamic_cast<HUDContainer *>(e);
        if (!next) {
            HUDLayoutKind k = m_CreatePolicy ? m_CreatePolicy(path.substr(0, path.find(seg)), seg, (int)i) : HUD_InferKindFromName(seg);
            next = cur->AddContainerChild(k, seg, 1);
        }
        built += '.';
        built += seg;
        // not globally registering child containers to avoid collisions; optionally: Register(built, next);
        cur = next;
    }
    return cur;
}

std::unique_ptr<HUDElement> HUD::StealByPath(const std::string &path) {
    if (path.empty()) return nullptr;
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        // root-level
        HUDElement *e = Find(path);
        if (!e) return nullptr;
        std::unique_ptr<HUDElement> out;
        for (auto it = m_CustomElements.begin(); it != m_CustomElements.end(); ++it) {
            if (it->get() == e) { out = std::move(*it); m_CustomElements.erase(it); break; }
        }
        m_Named.erase(path);
        return out;
    }
    std::string parentPath = path.substr(0, dot);
    std::string name = path.substr(dot + 1);
    HUDElement *pe = FindByPath(parentPath);
    HUDContainer *pc = dynamic_cast<HUDContainer *>(pe);
    if (!pc) return nullptr;
    return pc->StealChild(name);
}

void HUD::AttachToContainer(HUDContainer *dest, std::unique_ptr<HUDElement> up, const std::string &childName) {
    if (!dest || !up) return;
    dest->InsertChild(std::move(up), childName);
}

void HUD::AttachToRoot(std::unique_ptr<HUDElement> up, const std::string &name) {
    if (!up) return;
    HUDElement *ptr = up.get();
    m_CustomElements.push_back(std::move(up));
    if (!name.empty()) Register(name, ptr);
}

static AnchorPoint HUD_ParseAnchor(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s);
    ok = true;
    if (t == "tl" || t == "topleft") return AnchorPoint::TopLeft;
    if (t == "tc" || t == "topcenter") return AnchorPoint::TopCenter;
    if (t == "tr" || t == "topright") return AnchorPoint::TopRight;
    if (t == "ml" || t == "middleleft") return AnchorPoint::MiddleLeft;
    if (t == "mc" || t == "middlecenter" || t == "center") return AnchorPoint::MiddleCenter;
    if (t == "mr" || t == "middleright") return AnchorPoint::MiddleRight;
    if (t == "bl" || t == "bottomleft") return AnchorPoint::BottomLeft;
    if (t == "bc" || t == "bottomcenter") return AnchorPoint::BottomCenter;
    if (t == "br" || t == "bottomright") return AnchorPoint::BottomRight;
    ok = false; return AnchorPoint::TopLeft;
}

static bool HUD_ParseColor(const std::string &s, ImU32 &out) {
    std::string v = utils::TrimStringCopy(s);
    if (!v.empty() && v[0] == '#') { out = AnsiPalette::HexToImU32(v.c_str() + 1); return true; }
    int r = 0, g = 0, b = 0, a = 255; char c; std::stringstream ss(v);
    if (!(ss >> r)) return false; if (ss >> c) {} if (!(ss >> g)) return false; if (ss >> c){} if (!(ss >> b)) return false; if (ss >> c){} ss >> a;
    out = IM_COL32(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), std::clamp(a, 0, 255));
    return true;
}

static AlignX HUD_ParseAlignX(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s); ok = true;
    if (t == "left" || t == "l") return AlignX::Left;
    if (t == "center" || t == "c" || t == "middle" || t == "m") return AlignX::Center;
    if (t == "right" || t == "r") return AlignX::Right;
    ok=false; return AlignX::Left;
}
static AlignY HUD_ParseAlignY(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s); ok = true;
    if (t == "top" || t == "t") return AlignY::Top;
    if (t == "middle" || t == "center" || t == "m" || t == "c") return AlignY::Middle;
    if (t == "bottom" || t == "b") return AlignY::Bottom;
    ok=false; return AlignY::Top;
}

bool HUD::LoadConfig(const std::wstring &path) {
    return LoadConfigIni(path);
}

bool HUD::LoadConfigIni(const std::wstring &path) {
    IniFile ini;
    if (!ini.ParseFromFile(path)) return false;

    // Global style
    if (auto *sec = ini.GetSection("style")) {
        auto get = [&](const char* k){ return ini.GetValue("style", k, ""); };
        bool ok=false; std::string av = get("anchor"); if (!av.empty()) m_Style.anchor = HUD_ParseAnchor(av, ok);
        std::string off = get("offset"); if (!off.empty()) {
            float x=0,y=0; char c; std::stringstream ss(off); if (ss>>x) { if (ss>>c){} ss>>y; } m_Style.offset = ImVec2(x,y);
        }
        std::string col = get("color"); ImU32 colv; if (!col.empty() && HUD_ParseColor(col, colv)) m_Style.color = colv;
        std::string scale = get("scale"); if (!scale.empty()) m_Style.scale = (float)atof(scale.c_str());
        std::string wpx = get("wrap_px"); if (!wpx.empty()) m_Style.wrapWidthPx = (float)atof(wpx.c_str());
        std::string wfr = get("wrap_frac"); if (!wfr.empty()) m_Style.wrapWidthFrac = (float)atof(wfr.c_str());
        std::string tabs = get("tabs"); if (!tabs.empty()) m_Style.tabColumns = std::max(1, atoi(tabs.c_str()));
        std::string pnl = get("panel"); if (!pnl.empty()) m_Style.drawPanel = (utils::ToLower(pnl) == "on" || pnl == "1" || utils::ToLower(pnl)=="true");
        std::string pbg = get("panel_bg"); if (!pbg.empty() && HUD_ParseColor(pbg, colv)) m_Style.panelBg = colv;
        std::string pbd = get("panel_border"); if (!pbd.empty() && HUD_ParseColor(pbd, colv)) m_Style.panelBorder = colv;
        std::string pad = get("padding"); if (!pad.empty()) m_Style.panelPaddingPx = (float)atof(pad.c_str());
        std::string bth = get("border_thickness"); if (!bth.empty()) m_Style.panelBorderThickness = (float)atof(bth.c_str());
        std::string rnd = get("rounding"); if (!rnd.empty()) m_Style.panelRounding = (float)atof(rnd.c_str());
        // Policy mode
        std::string pol = get("policy"); if (!pol.empty()) SetAutoCreatePolicyMode(pol);
        // Page container mappings
        for (const auto &kv : sec->entries) {
            const std::string &k = kv.key;
            if (k.rfind("page_container.", 0) == 0) {
                std::string page = k.substr(std::string("page_container.").size());
                if (!page.empty()) m_PageDefaultContainers[page] = kv.value;
            }
        }
        // Active page
        std::string active = get("active_page"); if (!active.empty()) SetActivePage(active);
    }

    // Elements and containers: section names like element:<id>, vstack:<id>, hstack:<id>, grid:<id>
    for (const auto &name : ini.GetSectionNames()) {
        auto parts = utils::SplitString(name, ':'); if (parts.empty()) continue;
        std::string kind = utils::ToLower(parts[0]); if (kind=="style") continue;
        std::string id = parts.size()>=2?parts[1]:parts[0];
        auto *sec = ini.GetSection(name); if (!sec) continue;
        if (kind == "element") {
            HUDElement *e = GetOrCreate(id);
            // text
            std::string txt = ini.GetValue(name, "text", ""); if (!txt.empty()) e->SetText(txt.c_str());
            // page/template
            std::string page = ini.GetValue(name, "page", ""); if (!page.empty()) e->SetPage(page);
            std::string tpl = ini.GetValue(name, "template", ""); if (!tpl.empty()) e->SetTemplate(tpl);
            // pos
            bool ok=false; std::string av = ini.GetValue(name, "anchor", ""); if (!av.empty()) e->SetAnchor(HUD_ParseAnchor(av, ok));
            std::string off = ini.GetValue(name, "offset", ""); if (!off.empty()) { float x=0,y=0; char c; std::stringstream ss(off); if (ss>>x) { if (ss>>c){} ss>>y; } e->SetOffset(x,y);}            
            // style overrides
            std::string wrap_px = ini.GetValue(name, "wrap_px", ""); if (!wrap_px.empty()) e->SetWrapWidthPx((float)atof(wrap_px.c_str()));
            std::string wrap_fr = ini.GetValue(name, "wrap_frac", ""); if (!wrap_fr.empty()) e->SetWrapWidthFrac((float)atof(wrap_fr.c_str()));
            std::string tabs = ini.GetValue(name, "tabs", ""); if (!tabs.empty()) e->SetTabColumns(std::max(1, atoi(tabs.c_str())));
            std::string vis = ini.GetValue(name, "visible", ""); if (!vis.empty()) e->SetVisible(utils::ToLower(vis)=="on"||vis=="1"||utils::ToLower(vis)=="true");
            std::string scl = ini.GetValue(name, "scale", ""); if (!scl.empty()) e->SetScale((float)atof(scl.c_str()));
            ImU32 col; std::string colstr = ini.GetValue(name, "color", ""); if (!colstr.empty() && HUD_ParseColor(colstr, col)) e->SetColor(col);
            // panel
            std::string pnl = ini.GetValue(name, "panel", ""); if (!pnl.empty()) e->EnablePanel(utils::ToLower(pnl)=="on"||pnl=="1"||utils::ToLower(pnl)=="true");
            colstr = ini.GetValue(name, "panel_bg", ""); if (!colstr.empty() && HUD_ParseColor(colstr, col)) e->SetPanelBgColor(col);
            colstr = ini.GetValue(name, "panel_border", ""); if (!colstr.empty() && HUD_ParseColor(colstr, col)) e->SetPanelBorderColor(col);
            std::string pad = ini.GetValue(name, "padding", ""); if (!pad.empty()) e->SetPanelPadding((float)atof(pad.c_str()));
            std::string bth = ini.GetValue(name, "border_thickness", ""); if (!bth.empty()) e->SetPanelBorderThickness((float)atof(bth.c_str()));
            std::string rnd = ini.GetValue(name, "rounding", ""); if (!rnd.empty()) e->SetPanelRounding((float)atof(rnd.c_str()));
            continue;
        }
        HUDContainer *c = nullptr;
        // Nested container path support: id may be a dotted path, e.g., sidebar.header
        size_t lastDot = id.find_last_of('.');
        if (lastDot == std::string::npos) {
            if (kind == "vstack") c = AddVStack();
            else if (kind == "hstack") c = AddHStack();
            else if (kind == "grid") { int cols = std::max(1, atoi(ini.GetValue(name, "cols", "1").c_str())); c = AddGrid(cols); }
            if (!c) continue;
            m_Named[id] = c;
        } else {
            std::string parentPath = id.substr(0, lastDot);
            std::string childName = id.substr(lastDot + 1);
            HUDContainer *pc = EnsureContainerPath(parentPath, HUDLayoutKind::Vertical);
            if (!pc) continue;
            int cols = std::max(1, atoi(ini.GetValue(name, "cols", "1").c_str()));
            HUDLayoutKind k = (kind == "hstack") ? HUDLayoutKind::Horizontal : (kind == "grid" ? HUDLayoutKind::Grid : HUDLayoutKind::Vertical);
            c = pc->AddContainerChild(k, childName, cols);
            m_Named[id] = c;
        }
        // Pos
        bool ok=false; std::string av = ini.GetValue(name, "anchor", ""); if (!av.empty()) c->SetAnchor(HUD_ParseAnchor(av, ok));
        std::string off = ini.GetValue(name, "offset", ""); if (!off.empty()) { float x=0,y=0; char ch; std::stringstream ss(off); if (ss>>x) { if (ss>>ch){} ss>>y; } c->SetOffset(x,y);}            
        // Panel and spacing
        std::string sp = ini.GetValue(name, "spacing", ""); if (!sp.empty()) c->SetSpacing((float)atof(sp.c_str()));
        std::string pnl = ini.GetValue(name, "panel", ""); if (!pnl.empty()) c->EnablePanel(utils::ToLower(pnl)=="on"||pnl=="1"||utils::ToLower(pnl)=="true");
        ImU32 col; std::string colstr = ini.GetValue(name, "panel_bg", ""); if (!colstr.empty() && HUD_ParseColor(colstr, col)) c->SetPanelBgColor(col);
        colstr = ini.GetValue(name, "panel_border", ""); if (!colstr.empty() && HUD_ParseColor(colstr, col)) c->SetPanelBorderColor(col);
        std::string pad = ini.GetValue(name, "padding", ""); if (!pad.empty()) c->SetPanelPadding((float)atof(pad.c_str()));
        std::string bth = ini.GetValue(name, "border_thickness", ""); if (!bth.empty()) c->SetPanelBorderThickness((float)atof(bth.c_str()));
        std::string rnd = ini.GetValue(name, "rounding", ""); if (!rnd.empty()) c->SetPanelRounding((float)atof(rnd.c_str()));
        // Alignment
        bool okA=false; std::string ax = ini.GetValue(name, "align_x", ""); if (!ax.empty()) c->SetAlignX(HUD_ParseAlignX(ax, okA));
        std::string ay = ini.GetValue(name, "align_y", ""); if (!ay.empty()) c->SetAlignY(HUD_ParseAlignY(ay, okA));
        ax = ini.GetValue(name, "cell_align_x", ""); if (!ax.empty()) c->SetCellAlignX(HUD_ParseAlignX(ax, okA));
        ay = ini.GetValue(name, "cell_align_y", ""); if (!ay.empty()) c->SetCellAlignY(HUD_ParseAlignY(ay, okA));
        // Children text: keys child.N = text
        for (const auto &kv : sec->entries) {
            if (utils::StartsWith(kv.key, std::string("child."))) {
                std::string childName = kv.key.substr(6);
                if (!childName.empty()) {
                    HUDElement *child = c->AddChildNamed(childName, kv.value.c_str());
                    (void)child;
                } else {
                    HUDElement *child = c->AddChild(kv.value.c_str()); (void)child;
                }
            }
        }
    }
    return true;
}

static std::string HUD_AnchorToStr(AnchorPoint a) {
    switch (a) {
        case AnchorPoint::TopLeft: return "topleft"; case AnchorPoint::TopCenter: return "topcenter"; case AnchorPoint::TopRight: return "topright";
        case AnchorPoint::MiddleLeft: return "middleleft"; case AnchorPoint::MiddleCenter: return "middlecenter"; case AnchorPoint::MiddleRight: return "middleright";
        case AnchorPoint::BottomLeft: return "bottomleft"; case AnchorPoint::BottomCenter: return "bottomcenter"; case AnchorPoint::BottomRight: return "bottomright";
    }
    return "topleft";
}
static std::string HUD_ColorToHex(ImU32 c) {
    unsigned r = (c >> IM_COL32_R_SHIFT) & 0xFF;
    unsigned g = (c >> IM_COL32_G_SHIFT) & 0xFF;
    unsigned b = (c >> IM_COL32_B_SHIFT) & 0xFF;
    unsigned a = (c >> IM_COL32_A_SHIFT) & 0xFF;
    char buf[16]; snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", a, r, g, b);
    return std::string(buf);
}
static std::string HUD_AlignXStr(AlignX ax) { return ax==AlignX::Left?"left":(ax==AlignX::Center?"center":"right"); }
static std::string HUD_AlignYStr(AlignY ay) { return ay==AlignY::Top?"top":(ay==AlignY::Middle?"middle":"bottom"); }

static void HUD_CopyBaseProperties(const HUDElement *src, HUDElement *dst) {
    if (!src || !dst) return;
    dst->SetAnchor(src->GetAnchor());
    dst->SetOffset(src->GetOffset().x, src->GetOffset().y);
    dst->SetColor(src->GetColor());
    dst->SetScale(src->GetScale());
    dst->SetWrapWidthPx(src->GetWrapWidthPx());
    dst->SetWrapWidthFrac(src->GetWrapWidthFrac());
    dst->SetTabColumns(src->GetTabColumns());
    dst->EnablePanel(src->IsPanelEnabled());
    dst->SetPanelBgColor(src->GetPanelBgColor());
    dst->SetPanelBorderColor(src->GetPanelBorderColor());
    dst->SetPanelPadding(src->GetPanelPadding());
    dst->SetPanelBorderThickness(src->GetPanelBorderThickness());
    dst->SetPanelRounding(src->GetPanelRounding());
    dst->SetVisible(src->IsVisible());
    // Copy text/template/page
    if (src->HasTemplate()) dst->SetTemplate(src->GetTemplate());
    else dst->SetText(src->GetText());
    if (!src->GetPage().empty()) dst->SetPage(src->GetPage());
}

static std::unique_ptr<HUDElement> HUD_CloneElement(const HUDElement *src) {
    if (!src) return nullptr;
    if (auto cc = dynamic_cast<const HUDContainer *>(src)) {
        auto up = std::make_unique<HUDContainer>(cc->GetKind(), cc->GetGridCols());
        HUDContainer *dst = up.get();
        HUD_CopyBaseProperties(src, dst);
        dst->SetSpacing(cc->GetSpacing());
        dst->SetAlignX(cc->GetAlignX());
        dst->SetAlignY(cc->GetAlignY());
        dst->SetCellAlignX(cc->GetCellAlignX());
        dst->SetCellAlignY(cc->GetCellAlignY());
        // Build reverse map for names
        std::unordered_map<const HUDElement *, std::string> names;
        for (const auto &nk : cc->NamedChildren()) names[nk.second] = nk.first;
        // Clone children preserving order
        for (const auto &ch : cc->Children()) {
            const HUDElement *childSrc = ch.get();
            auto childUp = HUD_CloneElement(childSrc);
            std::string name;
            auto it = names.find(childSrc);
            if (it != names.end()) name = it->second;
            dst->InsertChild(std::move(childUp), name);
        }
        return up;
    } else {
        auto up = std::make_unique<HUDElement>(src->GetText(), src->GetAnchor());
        HUD_CopyBaseProperties(src, up.get());
        return up;
    }
}

bool HUD::SaveConfigIni(const std::wstring &path) const {
    IniFile ini;
    // style
    ini.SetValue("style", "anchor", HUD_AnchorToStr(m_Style.anchor));
    {
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f, %.3f", m_Style.offset.x, m_Style.offset.y);
        ini.SetValue("style", "offset", buf);
    }
    ini.SetValue("style", "color", HUD_ColorToHex(m_Style.color));
    {
        char buf[64]; snprintf(buf, sizeof(buf), "%.2f", m_Style.scale); ini.SetValue("style", "scale", buf);
        snprintf(buf, sizeof(buf), "%.2f", m_Style.wrapWidthPx); ini.SetValue("style", "wrap_px", buf);
        snprintf(buf, sizeof(buf), "%.2f", m_Style.wrapWidthFrac); ini.SetValue("style", "wrap_frac", buf);
    }
    ini.SetValue("style", "tabs", std::to_string(m_Style.tabColumns));
    ini.SetValue("style", "panel", m_Style.drawPanel?"on":"off");
    ini.SetValue("style", "panel_bg", HUD_ColorToHex(m_Style.panelBg));
    ini.SetValue("style", "panel_border", HUD_ColorToHex(m_Style.panelBorder));
    {
        char buf[64]; snprintf(buf, sizeof(buf), "%.2f", m_Style.panelPaddingPx); ini.SetValue("style", "padding", buf);
        snprintf(buf, sizeof(buf), "%.2f", m_Style.panelBorderThickness); ini.SetValue("style", "border_thickness", buf);
        snprintf(buf, sizeof(buf), "%.2f", m_Style.panelRounding); ini.SetValue("style", "rounding", buf);
    }
    // Persist policy mode and page container mappings
    ini.SetValue("style", "policy", GetAutoCreatePolicyModeEffective());
    if (!m_ActivePage.empty()) ini.SetValue("style", "active_page", m_ActivePage);
    for (const auto &pm : m_PageDefaultContainers) {
        std::string key = std::string("page_container.") + pm.first;
        ini.SetValue("style", key, pm.second);
    }

    // Save named elements/containers only (keeps config concise)
    for (const auto &kv : m_Named) {
        const std::string &id = kv.first; HUDElement *e = kv.second;
        if (HUDContainer *c = dynamic_cast<HUDContainer *>(e)) {
            std::string sec;
            // We can't distinguish between vstack/hstack/grid reliably from base; assume grid if GridCols>1, else vstack.
            if (c->GetGridCols() > 1) sec = std::string("grid:") + id; else sec = std::string("vstack:") + id;
            ini.SetValue(sec, "anchor", HUD_AnchorToStr(c->GetAnchor()));
            {
                char buf[64]; snprintf(buf, sizeof(buf), "%.3f, %.3f", c->GetOffset().x, c->GetOffset().y); ini.SetValue(sec, "offset", buf);
            }
            ini.SetValue(sec, "spacing", std::to_string((int)c->GetSpacing()));
            if (c->GetGridCols() > 1) ini.SetValue(sec, "cols", std::to_string(c->GetGridCols()));
            ini.SetValue(sec, "panel", c->IsPanelEnabled()?"on":"off");
            ini.SetValue(sec, "panel_bg", HUD_ColorToHex(c->GetPanelBgColor()));
            ini.SetValue(sec, "panel_border", HUD_ColorToHex(c->GetPanelBorderColor()));
            ini.SetValue(sec, "padding", std::to_string((int)c->GetPanelPadding()));
            ini.SetValue(sec, "border_thickness", std::to_string((int)c->GetPanelBorderThickness()));
            ini.SetValue(sec, "rounding", std::to_string((int)c->GetPanelRounding()));
            ini.SetValue(sec, "align_x", HUD_AlignXStr(c->GetAlignX()));
            ini.SetValue(sec, "align_y", HUD_AlignYStr(c->GetAlignY()));
            ini.SetValue(sec, "cell_align_x", HUD_AlignXStr(c->GetCellAlignX()));
            ini.SetValue(sec, "cell_align_y", HUD_AlignYStr(c->GetCellAlignY()));
            // children: prefer named keys
            if (!c->NamedChildren().empty()) {
                for (const auto &nk : c->NamedChildren()) {
                    std::string key = std::string("child.") + nk.first;
                    ini.SetValue(sec, key, nk.second->GetText());
                }
            } else {
                int idx = 1; for (const auto &up : c->Children()) { std::string key = std::string("child.") + std::to_string(idx++); ini.SetValue(sec, key, up->GetText()); }
            }
            continue;
        }
        // Plain element
        std::string sec = std::string("element:") + id;
        ini.SetValue(sec, "text", e->GetText());
        if (e->HasTemplate()) ini.SetValue(sec, "template", e->GetTemplate());
        if (!e->GetPage().empty()) ini.SetValue(sec, "page", e->GetPage());
        ini.SetValue(sec, "anchor", HUD_AnchorToStr(e->GetAnchor()));
        {
            char buf[64]; snprintf(buf, sizeof(buf), "%.3f, %.3f", e->GetOffset().x, e->GetOffset().y); ini.SetValue(sec, "offset", buf);
        }
        ini.SetValue(sec, "visible", e->IsVisible()?"on":"off");
        {
            char buf[64]; snprintf(buf, sizeof(buf), "%.2f", e->GetScale()); ini.SetValue(sec, "scale", buf);
            snprintf(buf, sizeof(buf), "%.2f", e->GetWrapWidthPx()); ini.SetValue(sec, "wrap_px", buf);
            snprintf(buf, sizeof(buf), "%.2f", e->GetWrapWidthFrac()); ini.SetValue(sec, "wrap_frac", buf);
        }
        ini.SetValue(sec, "tabs", std::to_string(e->GetTabColumns()));
        ini.SetValue(sec, "color", HUD_ColorToHex(e->GetColor()));
        ini.SetValue(sec, "panel", e->IsPanelEnabled()?"on":"off");
        ini.SetValue(sec, "panel_bg", HUD_ColorToHex(e->GetPanelBgColor()));
        ini.SetValue(sec, "panel_border", HUD_ColorToHex(e->GetPanelBorderColor()));
        ini.SetValue(sec, "padding", std::to_string((int)e->GetPanelPadding()));
        ini.SetValue(sec, "border_thickness", std::to_string((int)e->GetPanelBorderThickness()));
        ini.SetValue(sec, "rounding", std::to_string((int)e->GetPanelRounding()));
    }

    return ini.WriteToFile(path);
}

bool HUD::SaveSampleIfMissing() const {
    // Write LoaderDir\\hud.ini if missing
    std::string dir = BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER);
    std::wstring wdir = utils::ToWString(dir);
    std::wstring path = wdir + L"\\hud.ini";
    if (utils::FileExistsW(path)) return false;
    const char *sample =
        "# hud.ini\n"
        "[style]\n"
        "anchor = topRight\n"
        "offset = 0.0, 0.0\n"
        "panel = on\n"
        "panel_bg = #80202020\n"
        "\n"
        "[vstack:sidebar]\n"
        "anchor = tr\n"
        "spacing = 6\n"
        "panel = on\n"
        "child.1 = \x1b[1;33mWarning\x1b[0m: Low HP\n"
        "child.2 = Use \x1b[38;5;39mPotions\x1b[0m now\n"
        "\n"
        "[grid:stats]\n"
        "anchor = bl\n"
        "cols = 2\n"
        "spacing = 8\n"
        "cell_align_x = right\n"
        "child.1 = HP:\t100\n"
        "child.2 = MP:\t80\n";
    return utils::WriteTextFileW(path, utils::ToWString(sample));
}

bool HUD::SaveSubtreeIni(const std::wstring &path, const std::string &rootPath) const {
    const HUDElement *root = const_cast<HUD*>(this)->FindByPath(rootPath);
    if (!root) return false;
    IniFile ini;
    std::function<void(const HUDElement*, const std::string&)> write_element;
    write_element = [&](const HUDElement *e, const std::string &pathKey) {
        if (auto cont = dynamic_cast<const HUDContainer*>(e)) {
            std::string sec;
            switch (cont->GetKind()) {
                case HUDLayoutKind::Horizontal: sec = std::string("hstack:") + pathKey; break;
                case HUDLayoutKind::Grid: sec = std::string("grid:") + pathKey; break;
                case HUDLayoutKind::Vertical: default: sec = std::string("vstack:") + pathKey; break;
            }
            ini.SetValue(sec, "anchor", HUD_AnchorToStr(cont->GetAnchor()));
            {
                char buf[64]; snprintf(buf, sizeof(buf), "%.3f, %.3f", cont->GetOffset().x, cont->GetOffset().y); ini.SetValue(sec, "offset", buf);
            }
            ini.SetValue(sec, "spacing", std::to_string((int)cont->GetSpacing()));
            if (cont->GetKind() == HUDLayoutKind::Grid) ini.SetValue(sec, "cols", std::to_string(cont->GetGridCols()));
            ini.SetValue(sec, "panel", cont->IsPanelEnabled()?"on":"off");
            ini.SetValue(sec, "panel_bg", HUD_ColorToHex(cont->GetPanelBgColor()));
            ini.SetValue(sec, "panel_border", HUD_ColorToHex(cont->GetPanelBorderColor()));
            ini.SetValue(sec, "padding", std::to_string((int)cont->GetPanelPadding()));
            ini.SetValue(sec, "border_thickness", std::to_string((int)cont->GetPanelBorderThickness()));
            ini.SetValue(sec, "rounding", std::to_string((int)cont->GetPanelRounding()));
            ini.SetValue(sec, "align_x", HUD_AlignXStr(cont->GetAlignX()));
            ini.SetValue(sec, "align_y", HUD_AlignYStr(cont->GetAlignY()));
            ini.SetValue(sec, "cell_align_x", HUD_AlignXStr(cont->GetCellAlignX()));
            ini.SetValue(sec, "cell_align_y", HUD_AlignYStr(cont->GetCellAlignY()));
            // children
            std::unordered_map<const HUDElement*, std::string> names;
            for (const auto &nk : cont->NamedChildren()) names[nk.second] = nk.first;
            int idx = 1;
            const auto &children = cont->Children();
            for (const auto &up : children) {
                const HUDElement *ch = up.get();
                std::string seg = names.count(ch)?names[ch]:std::to_string(idx);
                std::string childPath = pathKey + std::string(".").append(seg);
                if (dynamic_cast<const HUDContainer*>(ch)) write_element(ch, childPath);
                else {
                    std::string key = "child." + seg;
                    ini.SetValue(sec, key, ch->GetText());
                }
                ++idx;
            }
        } else {
            std::string sec = std::string("element:") + pathKey;
            ini.SetValue(sec, "text", e->GetText());
            if (e->HasTemplate()) ini.SetValue(sec, "template", e->GetTemplate());
            if (!e->GetPage().empty()) ini.SetValue(sec, "page", e->GetPage());
            ini.SetValue(sec, "anchor", HUD_AnchorToStr(e->GetAnchor()));
            {
                char buf[64]; snprintf(buf, sizeof(buf), "%.3f, %.3f", e->GetOffset().x, e->GetOffset().y); ini.SetValue(sec, "offset", buf);
            }
            ini.SetValue(sec, "visible", e->IsVisible()?"on":"off");
            {
                char buf[64]; snprintf(buf, sizeof(buf), "%.2f", e->GetScale()); ini.SetValue(sec, "scale", buf);
                snprintf(buf, sizeof(buf), "%.2f", e->GetWrapWidthPx()); ini.SetValue(sec, "wrap_px", buf);
                snprintf(buf, sizeof(buf), "%.2f", e->GetWrapWidthFrac()); ini.SetValue(sec, "wrap_frac", buf);
            }
            ini.SetValue(sec, "tabs", std::to_string(e->GetTabColumns()));
            ini.SetValue(sec, "color", HUD_ColorToHex(e->GetColor()));
            ini.SetValue(sec, "panel", e->IsPanelEnabled()?"on":"off");
            ini.SetValue(sec, "panel_bg", HUD_ColorToHex(e->GetPanelBgColor()));
            ini.SetValue(sec, "panel_border", HUD_ColorToHex(e->GetPanelBorderColor()));
            ini.SetValue(sec, "padding", std::to_string((int)e->GetPanelPadding()));
            ini.SetValue(sec, "border_thickness", std::to_string((int)e->GetPanelBorderThickness()));
            ini.SetValue(sec, "rounding", std::to_string((int)e->GetPanelRounding()));
        }
    };
    write_element(root, rootPath);
    return ini.WriteToFile(path);
}

HUDContainer *HUD::AddVStack(AnchorPoint anchor) {
    auto up = std::make_unique<HUDContainer>(HUDLayoutKind::Vertical);
    HUDContainer *ptr = up.get();
    ApplyStyle(ptr); ptr->SetAnchor(anchor);
    m_CustomElements.push_back(std::move(up));
    return ptr;
}

HUDContainer *HUD::AddHStack(AnchorPoint anchor) {
    auto up = std::make_unique<HUDContainer>(HUDLayoutKind::Horizontal);
    HUDContainer *ptr = up.get();
    ApplyStyle(ptr); ptr->SetAnchor(anchor);
    m_CustomElements.push_back(std::move(up));
    return ptr;
}

HUDContainer *HUD::AddGrid(int cols, AnchorPoint anchor) {
    auto up = std::make_unique<HUDContainer>(HUDLayoutKind::Grid, cols);
    HUDContainer *ptr = up.get();
    ApplyStyle(ptr); ptr->SetAnchor(anchor);
    m_CustomElements.push_back(std::move(up));
    return ptr;
}

void HUD::ApplyStyle(HUDElement *e) {
    if (!e) return;
    e->SetAnchor(m_Style.anchor);
    e->SetOffset(m_Style.offset.x, m_Style.offset.y);
    e->SetColor(m_Style.color);
    e->SetScale(m_Style.scale);
    e->SetWrapWidthPx(m_Style.wrapWidthPx);
    e->SetWrapWidthFrac(m_Style.wrapWidthFrac);
    e->SetTabColumns(m_Style.tabColumns);
    e->EnablePanel(m_Style.drawPanel);
    e->SetPanelBgColor(m_Style.panelBg);
    e->SetPanelBorderColor(m_Style.panelBorder);
    e->SetPanelPadding(m_Style.panelPaddingPx);
    e->SetPanelBorderThickness(m_Style.panelBorderThickness);
    e->SetPanelRounding(m_Style.panelRounding);
}

const std::string &HUD::GetPageDefaultContainer(const std::string &page) const {
    auto it = m_PageDefaultContainers.find(page);
    static const std::string empty;
    return (it != m_PageDefaultContainers.end()) ? it->second : empty;
}

void HUD::SetPageDefaultContainer(const std::string &page, const std::string &path) {
    m_PageDefaultContainers[page] = path;
}

void HUD::ClearPageDefaultContainer(const std::string &page) {
    m_PageDefaultContainers.erase(page);
}

void HUD::SetAutoCreatePolicyMode(const std::string &mode) {
    std::string m = utils::ToLower(mode);
    m_CreatePolicyMode = m;
    if (m == "clear") {
        m_CreatePolicy = nullptr;
        return;
    }
    if (m == "vertical") {
        m_CreatePolicy = [](const std::string &, const std::string &, int) { return HUDLayoutKind::Vertical; };
        return;
    }
    if (m == "horizontal") {
        m_CreatePolicy = [](const std::string &, const std::string &, int) { return HUDLayoutKind::Horizontal; };
        return;
    }
    if (m == "grid") {
        m_CreatePolicy = [](const std::string &, const std::string &, int) { return HUDLayoutKind::Grid; };
        return;
    }
    // builtin: name-based inference (default)
    m_CreatePolicy = [](const std::string &, const std::string &seg, int) {
        std::string s = utils::ToLower(seg);
        if (s.find("grid") != std::string::npos) return HUDLayoutKind::Grid;
        if (s.find("hstack") != std::string::npos || s.find("hbox") != std::string::npos || s.find("row") != std::string::npos || s.find("horiz") != std::string::npos)
            return HUDLayoutKind::Horizontal;
        if (s.find("vstack") != std::string::npos || s.find("vbox") != std::string::npos || s.find("col") != std::string::npos)
            return HUDLayoutKind::Vertical;
        return HUDLayoutKind::Vertical;
    };
}

std::string HUD::GetAutoCreatePolicyModeEffective() const {
    return m_CreatePolicyMode.empty() ? std::string("builtin") : m_CreatePolicyMode;
}

std::vector<std::pair<std::string, std::string>> HUD::ListPageDefaultContainers() const {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(m_PageDefaultContainers.size());
    for (const auto &kv : m_PageDefaultContainers) out.emplace_back(kv.first, kv.second);
    return out;
}

std::string HUD::DumpOutline(const std::string &path) const {
    std::string out;
    auto append = [&](int depth, const std::string &line) {
        out.append(std::string(depth * 2, ' '));
        out.append(line);
        out.push_back('\n');
    };

    std::function<void(const HUDElement *, std::string, int)> dump;
    dump = [&](const HUDElement *e, const std::string &name, int depth) {
        const auto *c = dynamic_cast<const HUDContainer *>(e);
        std::string kind = c ? (c->GetKind() == HUDLayoutKind::Grid ? "grid" : (c->GetKind() == HUDLayoutKind::Horizontal ? "hstack" : "vstack")) : "element";
        std::string tag = name.empty() ? kind : (name + " [" + kind + "]");
        if (!e->GetPage().empty()) tag += " (page=" + e->GetPage() + ")";
        // Style bits: wrap/tab/panel
        std::vector<std::string> bits;
        if (e->GetWrapWidthPx() > 0.0f) {
            char buf[32]; snprintf(buf, sizeof(buf), "wrap_px=%.0f", e->GetWrapWidthPx()); bits.emplace_back(buf);
        } else if (e->GetWrapWidthFrac() > 0.0f) {
            char buf[32]; snprintf(buf, sizeof(buf), "wrap_frac=%.2f", e->GetWrapWidthFrac()); bits.emplace_back(buf);
        }
        if (e->GetTabColumns() != AnsiText::kDefaultTabColumns) {
            bits.emplace_back(std::string("tabs=") + std::to_string(e->GetTabColumns()));
        }
        if (e->IsPanelEnabled()) bits.emplace_back("panel");
        if (!bits.empty()) {
            std::string s = " {"; for (size_t i=0;i<bits.size();++i){ if(i) s += ","; s += bits[i]; } s += "}"; tag += s;
        }
        append(depth, tag);
        if (c) {
            // Build reverse name map
            std::unordered_map<const HUDElement*, std::string> names;
            for (const auto &nk : c->NamedChildren()) names[nk.second] = nk.first;
            int idx = 1;
            for (const auto &up : c->Children()) {
                const HUDElement *ch = up.get();
                std::string childName = names.count(ch) ? names[ch] : (std::string("#") + std::to_string(idx));
                dump(ch, childName, depth + 1);
                ++idx;
            }
        }
    };

    if (!path.empty()) {
        const HUDElement *root = const_cast<HUD*>(this)->FindByPath(path);
        if (!root) return out;
        dump(root, path, 0);
        return out;
    }
    // Root: list all custom elements; use names if available
    // Build reverse root name map
    std::unordered_map<const HUDElement*, std::string> rootNames;
    for (const auto &kv : m_Named) rootNames[kv.second] = kv.first;
    int idx = 1;
    for (const auto &up : m_CustomElements) {
        const HUDElement *e = up.get();
        const std::string &nm = rootNames.count(e) ? rootNames[e] : (std::string("#") + std::to_string(idx));
        dump(e, nm, 0);
        ++idx;
    }
    return out;
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

std::unique_ptr<HUDElement> HUD::CloneElement(const HUDElement *src) const {
    return HUD_CloneElement(src);
}
