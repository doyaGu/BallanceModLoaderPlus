#include "HUD.h"

#include <cmath>
#include <cassert>
#include <algorithm>
#include <unordered_map>

#include "AnsiPalette.h"
#include "IniFile.h"

// =============================================================================
// Animation Implementation
// =============================================================================

float EaseFunction(float t, EasingType type) {
    switch (type) {
    case EasingType::Linear:
        return t;
    case EasingType::EaseIn:
        return t * t;
    case EasingType::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case EasingType::EaseInOut:
        return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    default:
        return t;
    }
}

float HUDAnimation::GetCurrentValue() const {
    if (duration <= 0.0f) return endValue;
    float t = std::clamp(elapsed / duration, 0.0f, 1.0f);
    float easedT = EaseFunction(t, easing);
    return startValue + (endValue - startValue) * easedT;
}

void HUDAnimation::Update(float deltaTime) {
    if (finished) return;
    elapsed += deltaTime;
    if (elapsed >= duration) {
        elapsed = duration;
        finished = true;
    }
}

// =============================================================================
// HUDElement Implementation
// =============================================================================

HUDElement::HUDElement() = default;

HUDElement &HUDElement::SetVisible(bool visible) {
    if (m_Visible == visible) return *this;
    m_Visible = visible;
    MarkDirty();
    return *this;
}

HUDElement &HUDElement::SetAnchor(AnchorPoint anchor) {
    if (m_Anchor == anchor) return *this;
    m_Anchor = anchor;
    return *this;
}

HUDElement &HUDElement::SetOffsetPixels(float x, float y) {
    HUDOffset newOffset(x, y, CoordinateType::Pixels);
    if (m_Offset.x == newOffset.x && m_Offset.y == newOffset.y && m_Offset.type == newOffset.type) return *this;
    m_Offset = newOffset;
    return *this;
}

HUDElement &HUDElement::SetOffsetNormalized(float x, float y) {
    HUDOffset newOffset(x, y, CoordinateType::Normalized);
    if (m_Offset.x == newOffset.x && m_Offset.y == newOffset.y && m_Offset.type == newOffset.type) return *this;
    m_Offset = newOffset;
    return *this;
}

HUDElement &HUDElement::EnablePanel(bool enabled) {
    if (m_DrawPanel == enabled) return *this;
    m_DrawPanel = enabled;
    return *this;
}

HUDElement &HUDElement::SetPanelColors(ImU32 bg, ImU32 border) {
    if (m_PanelBg == bg && m_PanelBorder == border) return *this;
    m_PanelBg = bg;
    m_PanelBorder = border;
    return *this;
}

HUDElement &HUDElement::SetPanelBgColor(ImU32 bg) {
    if (m_PanelBg == bg) return *this;
    m_PanelBg = bg;
    return *this;
}

HUDElement &HUDElement::SetPanelBorderColor(ImU32 border) {
    if (m_PanelBorder == border) return *this;
    m_PanelBorder = border;
    return *this;
}

HUDElement &HUDElement::SetPanelPadding(float padPx) {
    const float v = ValidatePadding(padPx);
    if (m_PanelPaddingPx == v) return *this;
    m_PanelPaddingPx = v;
    return *this;
}

HUDElement &HUDElement::SetPanelBorderThickness(float px) {
    const float v = ValidatePadding(px);
    if (m_PanelBorderThickness == v) return *this;
    m_PanelBorderThickness = v;
    return *this;
}

HUDElement &HUDElement::SetPanelRounding(float px) {
    const float v = ValidatePadding(px);
    if (m_PanelRounding == v) return *this;
    m_PanelRounding = v;
    return *this;
}

void HUDElement::AddAnimation(const HUDAnimation &animation) {
    m_Animations.push_back(animation);
}

void HUDElement::ClearAnimations() {
    m_Animations.clear();
}

void HUDElement::UpdateAnimations(float deltaTime) {
    for (auto &anim : m_Animations) {
        anim.Update(deltaTime);

        // Apply animation to element properties
        const float value = anim.GetCurrentValue();
        switch (anim.property) {
        case HUDAnimation::Alpha:
            SetLocalAlpha(value);
            break;
        case HUDAnimation::PositionX:
            SetOffsetPixels(value, m_Offset.y);
            break;
        case HUDAnimation::PositionY:
            SetOffsetPixels(m_Offset.x, value);
            break;
            // Add more property cases as needed
        }
    }

    // Remove finished animations
    m_Animations.erase(
        std::remove_if(m_Animations.begin(), m_Animations.end(),
                       [](const HUDAnimation &anim) { return anim.IsFinished(); }),
        m_Animations.end());
}

bool HUDElement::HasActiveAnimations() const {
    return !m_Animations.empty();
}

void HUDElement::ToIni(IniFile &ini, const std::string &section) const {
    ini.SetValue(section, "type", "element");
    ini.SetValue(section, "visible", m_Visible ? "true" : "false");
    ini.SetValue(section, "anchor", std::to_string(static_cast<int>(m_Anchor)));
    ini.SetValue(section, "offset_x", std::to_string(m_Offset.x));
    ini.SetValue(section, "offset_y", std::to_string(m_Offset.y));
    ini.SetValue(section, "offset_type", m_Offset.type == CoordinateType::Pixels ? "pixels" : "normalized");
    ini.SetValue(section, "page", m_Page);
    ini.SetValue(section, "local_alpha", std::to_string(m_LocalAlpha));

    if (m_DrawPanel) {
        ini.SetValue(section, "panel_enabled", "true");
        ini.SetValue(section, "panel_bg", std::to_string(m_PanelBg));
        ini.SetValue(section, "panel_border", std::to_string(m_PanelBorder));
        ini.SetValue(section, "panel_padding", std::to_string(m_PanelPaddingPx));
        ini.SetValue(section, "panel_border_thickness", std::to_string(m_PanelBorderThickness));
        ini.SetValue(section, "panel_rounding", std::to_string(m_PanelRounding));
    }
}

void HUDElement::FromIni(const IniFile &ini, const std::string &section) {
    SetVisible(ini.GetValue(section, "visible", "true") == "true");
    SetAnchor(static_cast<AnchorPoint>(std::stoi(ini.GetValue(section, "anchor", "0"))));

    const float offsetX = std::stof(ini.GetValue(section, "offset_x", "0"));
    const float offsetY = std::stof(ini.GetValue(section, "offset_y", "0"));
    const std::string offsetType = ini.GetValue(section, "offset_type", "pixels");

    if (offsetType == "normalized") {
        SetOffsetNormalized(offsetX, offsetY);
    } else {
        SetOffsetPixels(offsetX, offsetY);
    }

    SetPage(ini.GetValue(section, "page"));
    SetLocalAlpha(std::stof(ini.GetValue(section, "local_alpha", "1.0")));

    if (ini.GetValue(section, "panel_enabled") == "true") {
        EnablePanel(true);
        SetPanelBgColor(std::stoul(ini.GetValue(section, "panel_bg", std::to_string(m_PanelBg))));
        SetPanelBorderColor(std::stoul(ini.GetValue(section, "panel_border", std::to_string(m_PanelBorder))));
        SetPanelPadding(std::stof(ini.GetValue(section, "panel_padding", std::to_string(m_PanelPaddingPx))));
        SetPanelBorderThickness(
            std::stof(ini.GetValue(section, "panel_border_thickness", std::to_string(m_PanelBorderThickness))));
        SetPanelRounding(std::stof(ini.GetValue(section, "panel_rounding", std::to_string(m_PanelRounding))));
    }
}

void HUDElement::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList) return;

    const ImVec2 elementSize = GetElementSize(viewportSize);
    ImVec2 pos = CalculatePosition(elementSize, viewportSize);
    pos.x = floorf(pos.x + HUDConstants::PIXEL_SNAP_THRESHOLD);
    pos.y = floorf(pos.y + HUDConstants::PIXEL_SNAP_THRESHOLD);

    if (m_DrawPanel) {
        const ImVec2 pad(m_PanelPaddingPx, m_PanelPaddingPx);
        const ImVec2 p0 = ImVec2(pos.x - pad.x, pos.y - pad.y);
        const ImVec2 p1 = ImVec2(pos.x + elementSize.x + pad.x, pos.y + elementSize.y + pad.y);

        auto scaleAlpha = [&](ImU32 c) -> ImU32 {
            const float a = ((((c >> 24) & 0xFF) / 255.0f) *
                std::clamp(m_InheritedAlpha * m_LocalAlpha, 0.0f, 1.0f));
            const int ai = static_cast<int>(std::roundf(a * 255.0f));
            return (c & 0x00FFFFFF) | (static_cast<ImU32>(ai) << 24);
        };

        if ((m_PanelBg >> 24) & 0xFF) {
            drawList->AddRectFilled(p0, p1, scaleAlpha(m_PanelBg), m_PanelRounding);
        }
        if (m_PanelBorderThickness > 0.0f && ((m_PanelBorder >> 24) & 0xFF)) {
            drawList->AddRect(p0, p1, scaleAlpha(m_PanelBorder), m_PanelRounding, 0, m_PanelBorderThickness);
        }
    }
}

ImVec2 HUDElement::CalculatePosition(const ImVec2 &elementSize, const ImVec2 &viewportSize) const {
    ImVec2 pos;

    // Calculate base position based on anchor
    switch (m_Anchor) {
    case AnchorPoint::TopLeft:
        pos = ImVec2(0, 0);
        break;
    case AnchorPoint::TopCenter:
        pos = ImVec2(viewportSize.x * 0.5f - elementSize.x * 0.5f, 0);
        break;
    case AnchorPoint::TopRight:
        pos = ImVec2(viewportSize.x - elementSize.x, 0);
        break;
    case AnchorPoint::MiddleLeft:
        pos = ImVec2(0, viewportSize.y * 0.5f - elementSize.y * 0.5f);
        break;
    case AnchorPoint::MiddleCenter:
        pos = ImVec2(viewportSize.x * 0.5f - elementSize.x * 0.5f, viewportSize.y * 0.5f - elementSize.y * 0.5f);
        break;
    case AnchorPoint::MiddleRight:
        pos = ImVec2(viewportSize.x - elementSize.x, viewportSize.y * 0.5f - elementSize.y * 0.5f);
        break;
    case AnchorPoint::BottomLeft:
        pos = ImVec2(0, viewportSize.y - elementSize.y);
        break;
    case AnchorPoint::BottomCenter:
        pos = ImVec2(viewportSize.x * 0.5f - elementSize.x * 0.5f, viewportSize.y - elementSize.y);
        break;
    case AnchorPoint::BottomRight:
        pos = ImVec2(viewportSize.x - elementSize.x, viewportSize.y - elementSize.y);
        break;
    }

    // Apply offset
    ImVec2 offsetPixels = m_Offset.ToPixels(viewportSize);
    pos.x += offsetPixels.x;
    pos.y += offsetPixels.y;

    return pos;
}

float HUDElement::ValidateScale(float scale) {
    return std::clamp(scale, HUDConstants::MIN_SCALE, HUDConstants::MAX_SCALE);
}

float HUDElement::ValidatePadding(float padding) {
    return std::max(0.0f, padding);
}

// =============================================================================
// HUDText Implementation
// =============================================================================

HUDText::HUDText(const char *text) : HUDElement(), m_AnsiText(text ? text : "") {}

HUDText &HUDText::SetText(const char *text) {
    const char *newText = text ? text : "";
    if (m_AnsiText.GetOriginalText() == newText) return *this;

    m_AnsiText.SetText(newText);
    Invalidate();
    return *this;
}

const char *HUDText::GetText() const {
    return m_AnsiText.GetOriginalText().c_str();
}

HUDText &HUDText::SetScale(float scale) {
    const float v = ValidateScale(scale);
    if (m_Scale == v) return *this;
    m_Scale = v;
    Invalidate();
    return *this;
}

HUDText &HUDText::SetWrapWidthPx(float px) {
    if (m_WrapWidthPx == px) return *this;
    m_WrapWidthPx = px;
    Invalidate();
    return *this;
}

HUDText &HUDText::SetWrapWidthFrac(float frac) {
    if (m_WrapWidthFrac == frac) return *this;
    m_WrapWidthFrac = frac;
    Invalidate();
    return *this;
}

HUDText &HUDText::SetTabColumns(int columns) {
    const int v = std::max(1, columns);
    if (m_TabColumns == v) return *this;
    m_TabColumns = v;
    Invalidate();
    return *this;
}

void HUDText::ToIni(IniFile &ini, const std::string &section) const {
    HUDElement::ToIni(ini, section);
    ini.SetValue(section, "type", "text");
    ini.SetValue(section, "text", GetText());
    ini.SetValue(section, "scale", std::to_string(m_Scale));
    ini.SetValue(section, "wrap_width_px", std::to_string(m_WrapWidthPx));
    ini.SetValue(section, "wrap_width_frac", std::to_string(m_WrapWidthFrac));
    ini.SetValue(section, "tab_columns", std::to_string(m_TabColumns));
}

void HUDText::FromIni(const IniFile &ini, const std::string &section) {
    HUDElement::FromIni(ini, section);
    SetText(ini.GetValue(section, "text").c_str());
    SetScale(std::stof(ini.GetValue(section, "scale", std::to_string(m_Scale))));
    SetWrapWidthPx(std::stof(ini.GetValue(section, "wrap_width_px", std::to_string(m_WrapWidthPx))));
    SetWrapWidthFrac(std::stof(ini.GetValue(section, "wrap_width_frac", std::to_string(m_WrapWidthFrac))));
    SetTabColumns(std::stoi(ini.GetValue(section, "tab_columns", std::to_string(m_TabColumns))));
}

void HUDText::Draw(ImDrawList* drawList, const ImVec2& viewportSize) {
    if (!m_Visible || m_AnsiText.IsEmpty() || !drawList) return;

    const float fontSize = ImGui::GetFontSize() * m_Scale;
    if (fontSize <= 0.0f) return;

    const float wrapWidth = ResolveWrapWidth(viewportSize);
    const ImVec2 textSize = CalculateAnsiTextSize(viewportSize);

    const bool hasWrap = (wrapWidth != FLT_MAX && wrapWidth > 0.0f);
    const float boxWidth = hasWrap ? wrapWidth : textSize.x;

    // Anchor against the box so panel/anchor stay stable
    const ImVec2 boxSize(boxWidth, textSize.y);
    ImVec2 pos = CalculatePosition(boxSize, viewportSize);

    // Pixel snap
    pos.x = floorf(pos.x + HUDConstants::PIXEL_SNAP_THRESHOLD);
    pos.y = floorf(pos.y + HUDConstants::PIXEL_SNAP_THRESHOLD);

    // Draw panel FIRST
    HUDElement::Draw(drawList, viewportSize);

    // Effective alpha
    const float alpha = std::clamp(m_InheritedAlpha, 0.0f, 1.0f) * std::clamp(m_LocalAlpha, 0.0f, 1.0f);

    // Renderer must use AddText(font, fontSize, pos, color, text, ..., wrapWidth)
    AnsiText::Renderer::DrawText(drawList, m_AnsiText, pos, wrapWidth, alpha, fontSize, -1.0f, m_TabColumns);
}

ImVec2 HUDText::GetElementSize(const ImVec2 &viewportSize) const {
    return CalculateAnsiTextSize(viewportSize);
}

ImVec2 HUDText::CalculateAnsiTextSize(const ImVec2 &viewportSize) const {
    if (m_AnsiText.IsEmpty()) return {0, 0};

    const float fontSize = ImGui::GetFontSize() * m_Scale;
    if (fontSize <= 0.0f) return {0, 0};

    const float fontPixelSize = ImGui::GetFontSize();
    const float wrapWidth = ResolveWrapWidth(viewportSize);

    // Check cache validity
    if (m_MeasureCache.textVersion == m_TextVersion &&
        m_MeasureCache.wrapWidth == wrapWidth &&
        m_MeasureCache.fontSize == fontSize &&
        m_MeasureCache.tabCols == m_TabColumns &&
        std::fabs(m_MeasureCache.fontPixelSize - fontPixelSize) < 1e-3f) {
        return m_MeasureCache.size;
    }

    // Calculate new size
    const ImVec2 size = AnsiText::CalculateSize(m_AnsiText, wrapWidth, fontSize, -1.0f, m_TabColumns);

    // Update cache
    m_MeasureCache.textVersion = m_TextVersion;
    m_MeasureCache.wrapWidth = wrapWidth;
    m_MeasureCache.fontSize = fontSize;
    m_MeasureCache.fontPixelSize = fontPixelSize;
    m_MeasureCache.tabCols = m_TabColumns;
    m_MeasureCache.size = size;

    return size;
}

float HUDText::ResolveWrapWidth(const ImVec2 &viewportSize) const {
    if (m_WrapWidthPx > 0.0f) return m_WrapWidthPx;
    if (m_WrapWidthFrac > 0.0f) return viewportSize.x * m_WrapWidthFrac;
    return FLT_MAX;
}

// HUDImage Implementation
HUDImage::HUDImage(ImTextureID texture, float width, float height)
    : HUDElement(), m_Texture(texture), m_Width(width), m_Height(height) {}

void HUDImage::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList || !m_Texture) return;

    const ImVec2 elementSize = GetElementSize(viewportSize);
    ImVec2 pos = CalculatePosition(elementSize, viewportSize);
    pos.x = floorf(pos.x + HUDConstants::PIXEL_SNAP_THRESHOLD);
    pos.y = floorf(pos.y + HUDConstants::PIXEL_SNAP_THRESHOLD);

    // Apply alpha
    ImU32 tintWithAlpha = m_Tint;
    float alpha = (((tintWithAlpha >> 24) & 0xFF) / 255.0f) *
        std::clamp(m_InheritedAlpha * m_LocalAlpha, 0.0f, 1.0f);
    int ai = static_cast<int>(std::roundf(alpha * 255.0f));
    tintWithAlpha = (tintWithAlpha & 0x00FFFFFF) | (static_cast<ImU32>(ai) << 24);

    // Draw panel if enabled
    HUDElement::Draw(drawList, viewportSize);

    drawList->AddImage(m_Texture, pos, ImVec2(pos.x + elementSize.x, pos.y + elementSize.y),
                       ImVec2(0, 0), ImVec2(1, 1), tintWithAlpha);
}

ImVec2 HUDImage::GetElementSize(const ImVec2 &viewportSize) const {
    return {m_Width, m_Height};
}

void HUDImage::ToIni(IniFile &ini, const std::string &section) const {
    HUDElement::ToIni(ini, section);
    ini.SetValue(section, "type", "image");
    ini.SetValue(section, "width", std::to_string(m_Width));
    ini.SetValue(section, "height", std::to_string(m_Height));
    ini.SetValue(section, "tint", std::to_string(m_Tint));
}

void HUDImage::FromIni(const IniFile &ini, const std::string &section) {
    HUDElement::FromIni(ini, section);
    SetSize(std::stof(ini.GetValue(section, "width", std::to_string(m_Width))),
            std::stof(ini.GetValue(section, "height", std::to_string(m_Height))));
    SetTint(std::stoul(ini.GetValue(section, "tint", std::to_string(m_Tint))));
}

// HUDProgressBar Implementation
HUDProgressBar::HUDProgressBar(float width, float height)
    : HUDElement(), m_Width(width), m_Height(height) {}

void HUDProgressBar::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList) return;

    const ImVec2 elementSize = GetElementSize(viewportSize);
    ImVec2 pos = CalculatePosition(elementSize, viewportSize);
    pos.x = floorf(pos.x + HUDConstants::PIXEL_SNAP_THRESHOLD);
    pos.y = floorf(pos.y + HUDConstants::PIXEL_SNAP_THRESHOLD);

    // Apply alpha
    auto scaleAlpha = [&](ImU32 c) -> ImU32 {
        const float a = ((((c >> 24) & 0xFF) / 255.0f) *
            std::clamp(m_InheritedAlpha * m_LocalAlpha, 0.0f, 1.0f));
        const int ai = static_cast<int>(std::roundf(a * 255.0f));
        return (c & 0x00FFFFFF) | (static_cast<ImU32>(ai) << 24);
    };

    // Draw panel if enabled
    HUDElement::Draw(drawList, viewportSize);

    // Draw background
    const ImVec2 p1 = ImVec2(pos.x + elementSize.x, pos.y + elementSize.y);
    drawList->AddRectFilled(pos, p1, scaleAlpha(m_BgColor));

    // Draw fill
    const float progress = GetProgress();
    if (progress > 0.0f) {
        ImVec2 fillEnd = ImVec2(pos.x + elementSize.x * progress, pos.y + elementSize.y);
        drawList->AddRectFilled(pos, fillEnd, scaleAlpha(m_FillColor));
    }
}

ImVec2 HUDProgressBar::GetElementSize(const ImVec2 &viewportSize) const {
    return {m_Width, m_Height};
}

void HUDProgressBar::ToIni(IniFile &ini, const std::string &section) const {
    HUDElement::ToIni(ini, section);
    ini.SetValue(section, "type", "progressbar");
    ini.SetValue(section, "width", std::to_string(m_Width));
    ini.SetValue(section, "height", std::to_string(m_Height));
    ini.SetValue(section, "value", std::to_string(m_Value));
    ini.SetValue(section, "min", std::to_string(m_Min));
    ini.SetValue(section, "max", std::to_string(m_Max));
    ini.SetValue(section, "bg_color", std::to_string(m_BgColor));
    ini.SetValue(section, "fill_color", std::to_string(m_FillColor));
}

void HUDProgressBar::FromIni(const IniFile &ini, const std::string &section) {
    HUDElement::FromIni(ini, section);
    SetSize(std::stof(ini.GetValue(section, "width", std::to_string(m_Width))),
            std::stof(ini.GetValue(section, "height", std::to_string(m_Height))));
    SetRange(std::stof(ini.GetValue(section, "min", std::to_string(m_Min))),
             std::stof(ini.GetValue(section, "max", std::to_string(m_Max))));
    SetValue(std::stof(ini.GetValue(section, "value", std::to_string(m_Value))));
    SetColors(std::stoul(ini.GetValue(section, "bg_color", std::to_string(m_BgColor))),
              std::stoul(ini.GetValue(section, "fill_color", std::to_string(m_FillColor))));
}

// HUDSpacer Implementation
HUDSpacer::HUDSpacer(float width, float height)
    : HUDElement(), m_Width(width), m_Height(height) {}

ImVec2 HUDSpacer::GetElementSize(const ImVec2 &viewportSize) const {
    return {m_Width, m_Height};
}

void HUDSpacer::ToIni(IniFile &ini, const std::string &section) const {
    HUDElement::ToIni(ini, section);
    ini.SetValue(section, "type", "spacer");
    ini.SetValue(section, "width", std::to_string(m_Width));
    ini.SetValue(section, "height", std::to_string(m_Height));
}

void HUDSpacer::FromIni(const IniFile &ini, const std::string &section) {
    HUDElement::FromIni(ini, section);
    SetSize(std::stof(ini.GetValue(section, "width", std::to_string(m_Width))),
            std::stof(ini.GetValue(section, "height", std::to_string(m_Height))));
}

// =============================================================================
// HUDContainer Implementation
// =============================================================================

HUDContainer::HUDContainer(HUDLayoutKind kind, int gridCols)
    : HUDElement(), m_Kind(kind), m_GridCols(gridCols > 0 ? gridCols : 1) {}

std::shared_ptr<HUDText> HUDContainer::AddChild(const char *text) {
    auto child = std::make_shared<HUDText>(text);
    m_Children.push_back(child);
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDText> HUDContainer::AddChildNamed(const std::string &name, const char *text) {
    auto child = std::make_shared<HUDText>(text);
    m_Children.push_back(child);
    m_NamedChildren[name] = child;
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDImage> HUDContainer::AddImageChild(const std::string &name, ImTextureID texture, float width, float height) {
    auto child = std::make_shared<HUDImage>(texture, width, height);
    m_Children.push_back(child);
    m_NamedChildren[name] = child;
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDProgressBar> HUDContainer::AddProgressBarChild(const std::string &name, float width, float height) {
    auto child = std::make_shared<HUDProgressBar>(width, height);
    m_Children.push_back(child);
    m_NamedChildren[name] = child;
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDSpacer> HUDContainer::AddSpacerChild(const std::string &name, float width, float height) {
    auto child = std::make_shared<HUDSpacer>(width, height);
    m_Children.push_back(child);
    m_NamedChildren[name] = child;
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDElement> HUDContainer::FindChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    if (it != m_NamedChildren.end()) {
        return it->second.lock();
    }
    return nullptr;
}

bool HUDContainer::RemoveChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    if (it == m_NamedChildren.end()) return false;

    const auto element = it->second.lock();
    if (!element) {
        m_NamedChildren.erase(it);
        return false;
    }

    m_NamedChildren.erase(it);

    const auto cit = std::find(m_Children.begin(), m_Children.end(), element);
    if (cit != m_Children.end()) {
        m_Children.erase(cit);
        InvalidateSizeCache();
        return true;
    }
    return false;
}

std::shared_ptr<HUDContainer> HUDContainer::AddContainerChild(HUDLayoutKind kind, const std::string &name, int gridCols) {
    auto container = std::make_shared<HUDContainer>(kind, gridCols);
    m_NamedChildren[name] = container;
    m_Children.push_back(container);
    InvalidateSizeCache();
    return container;
}

std::shared_ptr<HUDElement> HUDContainer::StealChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    if (it == m_NamedChildren.end()) return nullptr;

    auto element = it->second.lock();
    if (!element) {
        m_NamedChildren.erase(it);
        return nullptr;
    }

    m_NamedChildren.erase(it);

    const auto cit = std::find(m_Children.begin(), m_Children.end(), element);
    if (cit != m_Children.end()) {
        m_Children.erase(cit);
        InvalidateSizeCache();
        return element;
    }
    return nullptr;
}

void HUDContainer::InsertChild(const std::shared_ptr<HUDElement> &element, const std::string &name) {
    if (element) {
        m_NamedChildren[name] = element;
        m_Children.push_back(element);
        InvalidateSizeCache();
    }
}

HUDContainer &HUDContainer::SetSpacing(float px) {
    const float v = ValidatePadding(px);
    if (m_SpacingPx == v) return *this;
    m_SpacingPx = v;
    InvalidateSizeCache();
    return *this;
}

HUDContainer &HUDContainer::SetGridCols(int cols) {
    const int v = cols > 0 ? cols : 1;
    if (m_GridCols == v) return *this;
    m_GridCols = v;
    InvalidateSizeCache();
    return *this;
}

std::shared_ptr<HUDElement> HUDContainer::GetChild(size_t index) const {
    return (index < m_Children.size()) ? m_Children[index] : nullptr;
}

void HUDContainer::ToIni(IniFile &ini, const std::string &section) const {
    HUDElement::ToIni(ini, section);
    ini.SetValue(section, "type", "container");
    ini.SetValue(section, "layout_kind", std::to_string(static_cast<int>(m_Kind)));
    ini.SetValue(section, "grid_cols", std::to_string(m_GridCols));
    ini.SetValue(section, "spacing", std::to_string(m_SpacingPx));
    ini.SetValue(section, "align_x", std::to_string(static_cast<int>(m_AlignX)));
    ini.SetValue(section, "align_y", std::to_string(static_cast<int>(m_AlignY)));
    ini.SetValue(section, "cell_align_x", std::to_string(static_cast<int>(m_CellAlignX)));
    ini.SetValue(section, "cell_align_y", std::to_string(static_cast<int>(m_CellAlignY)));

    if (m_ClipEnabled) {
        ini.SetValue(section, "clip_enabled", "true");
        ini.SetValue(section, "clip_padding", std::to_string(m_ClipPaddingPx));
    }

    if (m_FadeEnabled) {
        ini.SetValue(section, "fade_enabled", "true");
        ini.SetValue(section, "fade_alpha", std::to_string(m_Alpha));
        ini.SetValue(section, "fade_target", std::to_string(m_FadeTarget));
        ini.SetValue(section, "fade_speed", std::to_string(m_FadeSpeed));
    }

    // Save children - only save named children to maintain relationship
    std::vector<std::pair<std::string, std::shared_ptr<HUDElement>>> validChildren;
    for (const auto &[name, weakChild] : m_NamedChildren) {
        if (auto child = weakChild.lock()) {
            validChildren.emplace_back(name, child);
        }
    }

    ini.SetValue(section, "child_count", std::to_string(validChildren.size()));
    int childIndex = 0;
    for (const auto &[name, child] : validChildren) {
        std::string childKey = "child_";
        childKey.append(std::to_string(childIndex)).append("_name");
        std::string childSectionKey = "child_";
        childSectionKey.append(std::to_string(childIndex)).append("_section");
        std::string childSection = section;
        childSection.append("_child_").append(name);

        ini.SetValue(section, childKey, name);
        ini.SetValue(section, childSectionKey, childSection);
        child->ToIni(ini, childSection);
        childIndex++;
    }
}

void HUDContainer::FromIni(const IniFile &ini, const std::string &section) {
    HUDElement::FromIni(ini, section);
    m_Kind = static_cast<HUDLayoutKind>(std::stoi(ini.GetValue(section, "layout_kind", "0")));
    SetGridCols(std::stoi(ini.GetValue(section, "grid_cols", std::to_string(m_GridCols))));
    SetSpacing(std::stof(ini.GetValue(section, "spacing", std::to_string(m_SpacingPx))));
    SetAlignX(static_cast<AlignX>(std::stoi(ini.GetValue(section, "align_x", "0"))));
    SetAlignY(static_cast<AlignY>(std::stoi(ini.GetValue(section, "align_y", "0"))));
    SetCellAlignX(static_cast<AlignX>(std::stoi(ini.GetValue(section, "cell_align_x", "0"))));
    SetCellAlignY(static_cast<AlignY>(std::stoi(ini.GetValue(section, "cell_align_y", "0"))));

    if (ini.GetValue(section, "clip_enabled") == "true") {
        EnableClip(true);
        SetClipPadding(std::stof(ini.GetValue(section, "clip_padding", std::to_string(m_ClipPaddingPx))));
    }

    if (ini.GetValue(section, "fade_enabled") == "true") {
        EnableFade(true);
        SetAlpha(std::stof(ini.GetValue(section, "fade_alpha", std::to_string(m_Alpha))));
        SetFadeTarget(std::stof(ini.GetValue(section, "fade_target", std::to_string(m_FadeTarget))));
        SetFadeSpeed(std::stof(ini.GetValue(section, "fade_speed", std::to_string(m_FadeSpeed))));
    }
}

void HUDContainer::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList) return;

    // Gather visible children
    struct Item {
        std::shared_ptr<HUDElement> element;
        ImVec2 size;
    };
    std::vector<Item> items;
    items.reserve(m_Children.size());

    for (const auto &child : m_Children) {
        if (child && child->IsVisible()) {
            items.push_back({child, child->GetElementSize(viewportSize)});
        }
    }

    if (items.empty()) return;

    const float alphaMul = m_InheritedAlpha * m_LocalAlpha * (m_FadeEnabled ? m_Alpha : 1.0f);
    const ImVec2 contentSize = CalculateContentSize(viewportSize);

    // Use GetElementSize() which properly accounts for panel padding in anchor calculations
    ImVec2 origin = CalculatePosition(contentSize, viewportSize);
    origin.x = floorf(origin.x + HUDConstants::PIXEL_SNAP_THRESHOLD);
    origin.y = floorf(origin.y + HUDConstants::PIXEL_SNAP_THRESHOLD);

    // Draw panel if enabled
    HUDElement::Draw(drawList, viewportSize);

    // Optional clipping
    if (m_ClipEnabled) {
        const auto clipMin = ImVec2(origin.x - m_ClipPaddingPx, origin.y - m_ClipPaddingPx);
        const auto clipMax = ImVec2(origin.x + contentSize.x + m_ClipPaddingPx, origin.y + contentSize.y + m_ClipPaddingPx);
        drawList->PushClipRect(clipMin, clipMax, true);
    }

    // Adjust origin for panel padding if enabled
    ImVec2 childOrigin = origin;
    if (m_DrawPanel) {
        childOrigin.x += m_PanelPaddingPx;
        childOrigin.y += m_PanelPaddingPx;
    }

    // Layout children based on kind
    switch (m_Kind) {
    case HUDLayoutKind::Vertical:
        LayoutVertical(drawList, viewportSize, childOrigin, alphaMul);
        break;
    case HUDLayoutKind::Horizontal:
        LayoutHorizontal(drawList, viewportSize, childOrigin, alphaMul);
        break;
    case HUDLayoutKind::Grid:
        LayoutGrid(drawList, viewportSize, childOrigin, alphaMul);
        break;
    }

    if (m_ClipEnabled) {
        drawList->PopClipRect();
    }
}

ImVec2 HUDContainer::CalculateContentSize(const ImVec2 &viewportSize) const {
    // Check cache validity - improved with viewport size consideration
    if (!m_SizeCacheDirty &&
        m_LastViewportSize.x == viewportSize.x &&
        m_LastViewportSize.y == viewportSize.y) {
        return m_CachedContentSize;
    }

    std::vector<ImVec2> childSizes;
    for (auto &child : m_Children) {
        if (child && child->IsVisible()) {
            childSizes.push_back(child->GetElementSize(viewportSize));
        }
    }

    if (childSizes.empty()) {
        m_CachedContentSize = ImVec2(0, 0);
        m_SizeCacheDirty = false;
        m_LastViewportSize = viewportSize;
        return m_CachedContentSize;
    }

    ImVec2 content(0, 0);
    const float spacing = m_SpacingPx;

    switch (m_Kind) {
    case HUDLayoutKind::Vertical:
        for (const auto &size : childSizes) {
            content.x = std::max(content.x, size.x);
            content.y += size.y;
        }
        if (childSizes.size() > 1) {
            content.y += spacing * (childSizes.size() - 1);
        }
        break;

    case HUDLayoutKind::Horizontal:
        for (const auto &size : childSizes) {
            content.x += size.x;
            content.y = std::max(content.y, size.y);
        }
        if (childSizes.size() > 1) {
            content.x += spacing * (childSizes.size() - 1);
        }
        break;

    case HUDLayoutKind::Grid: {
        const int cols = std::max(1, m_GridCols);
        const int rows = static_cast<int>((childSizes.size() + cols - 1) / cols);

        std::vector<float> colW(cols, 0.0f), rowH(rows, 0.0f);
        for (int i = 0; i < static_cast<int>(childSizes.size()); ++i) {
            int c = i % cols, r = i / cols;
            colW[c] = std::max(colW[c], childSizes[i].x);
            rowH[r] = std::max(rowH[r], childSizes[i].y);
        }

        for (float w : colW) content.x += w;
        for (float h : rowH) content.y += h;
        if (cols > 1) content.x += spacing * (cols - 1);
        if (rows > 1) content.y += spacing * (rows - 1);
        break;
    }
    }

    // Update cache
    m_CachedContentSize = content;
    m_SizeCacheDirty = false;
    m_LastViewportSize = viewportSize;

    return content;
}

ImVec2 HUDContainer::GetElementSize(const ImVec2 &viewportSize) const {
    return CalculateContentSize(viewportSize);
}

void HUDContainer::LayoutVertical(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, float alphaMul) {
    const ImVec2 contentSize = CalculateContentSize(viewportSize);
    float y = origin.y;

    for (auto &child : m_Children) {
        if (!child || !child->IsVisible()) continue;

        const ImVec2 childSize = child->GetElementSize(viewportSize);
        float x = origin.x;

        if (m_AlignX == AlignX::Center) x += (contentSize.x - childSize.x) * 0.5f;
        else if (m_AlignX == AlignX::Right) x += (contentSize.x - childSize.x);

        const AnchorPoint oldA = child->GetAnchor();
        const HUDOffset oldOff = child->GetOffset();

        child->SetInheritedAlpha(alphaMul);
        child->SetAnchor(AnchorPoint::TopLeft);
        child->SetOffsetPixels(x, y);
        child->Draw(drawList, viewportSize);

        child->SetAnchor(oldA);
        child->SetOffset(oldOff);

        y += childSize.y + m_SpacingPx;
    }
}

void HUDContainer::LayoutHorizontal(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, float alphaMul) {
    const ImVec2 contentSize = CalculateContentSize(viewportSize);
    float x = origin.x;

    for (auto &child : m_Children) {
        if (!child || !child->IsVisible()) continue;

        const ImVec2 childSize = child->GetElementSize(viewportSize);
        float y = origin.y;

        if (m_AlignY == AlignY::Middle) y += (contentSize.y - childSize.y) * 0.5f;
        else if (m_AlignY == AlignY::Bottom) y += (contentSize.y - childSize.y);

        const AnchorPoint oldA = child->GetAnchor();
        const HUDOffset oldOff = child->GetOffset();

        child->SetInheritedAlpha(alphaMul);
        child->SetAnchor(AnchorPoint::TopLeft);
        child->SetOffsetPixels(x, y);
        child->Draw(drawList, viewportSize);

        child->SetAnchor(oldA);
        child->SetOffset(oldOff);

        x += childSize.x + m_SpacingPx;
    }
}

void HUDContainer::LayoutGrid(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, float alphaMul) {
    std::vector<std::shared_ptr<HUDElement>> visibleChildren;
    std::vector<ImVec2> childSizes;

    for (auto &child : m_Children) {
        if (child && child->IsVisible()) {
            visibleChildren.push_back(child);
            childSizes.push_back(child->GetElementSize(viewportSize));
        }
    }

    if (visibleChildren.empty()) return;

    const int cols = std::max(1, m_GridCols);
    const int rows = static_cast<int>((visibleChildren.size() + cols - 1) / cols);

    // Calculate column widths and row heights
    std::vector<float> colW(cols, 0.0f), rowH(rows, 0.0f);
    for (int i = 0; i < static_cast<int>(visibleChildren.size()); ++i) {
        const int c = i % cols;
        const int r = i / cols;
        colW[c] = std::max(colW[c], childSizes[i].x);
        rowH[r] = std::max(rowH[r], childSizes[i].y);
    }

    // Calculate column and row positions
    std::vector<float> colX(cols, origin.x);
    for (int c = 1; c < cols; ++c) {
        colX[c] = colX[c - 1] + colW[c - 1] + m_SpacingPx;
    }

    std::vector<float> rowY(rows, origin.y);
    for (int r = 1; r < rows; ++r) {
        rowY[r] = rowY[r - 1] + rowH[r - 1] + m_SpacingPx;
    }

    for (int i = 0; i < static_cast<int>(visibleChildren.size()); ++i) {
        const int c = i % cols;
        const int r = i / cols;
        const ImVec2 childSize = childSizes[i];
        float x = colX[c], y = rowY[r];

        // Cell alignment
        if (m_CellAlignX == AlignX::Center) x += (colW[c] - childSize.x) * 0.5f;
        else if (m_CellAlignX == AlignX::Right) x += (colW[c] - childSize.x);
        if (m_CellAlignY == AlignY::Middle) y += (rowH[r] - childSize.y) * 0.5f;
        else if (m_CellAlignY == AlignY::Bottom) y += (rowH[r] - childSize.y);

        const AnchorPoint oldA = visibleChildren[i]->GetAnchor();
        const HUDOffset oldOff = visibleChildren[i]->GetOffset();

        visibleChildren[i]->SetInheritedAlpha(alphaMul);
        visibleChildren[i]->SetAnchor(AnchorPoint::TopLeft);
        visibleChildren[i]->SetOffsetPixels(x, y);
        visibleChildren[i]->Draw(drawList, viewportSize);

        visibleChildren[i]->SetAnchor(oldA);
        visibleChildren[i]->SetOffset(oldOff);
    }
}

void HUDContainer::TickFade(float dt) {
    if (!m_FadeEnabled) return;
    if (m_Alpha == m_FadeTarget) return;

    const float dir = (m_FadeTarget > m_Alpha) ? 1.0f : -1.0f;
    const float step = m_FadeSpeed * dt * dir;
    const float next = m_Alpha + step;

    if ((dir > 0 && next >= m_FadeTarget) || (dir < 0 && next <= m_FadeTarget)) {
        m_Alpha = m_FadeTarget;
    } else {
        m_Alpha = next;
    }
}

// =============================================================================
// HUD Implementation
// =============================================================================

HUD::HUD() : Bui::Window("HUD") {}

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
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
}

void HUD::OnDraw() {
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;

    for (auto &element : m_Elements) {
        if (element) {
            const std::string &page = element->GetPage();
            if (!m_ActivePage.empty() && !page.empty() && page != m_ActivePage) {
                continue;
            }
            element->Draw(drawList, viewportSize);
        }
    }
}

void HUD::OnProcess() {
    const float deltaTime = ImGui::GetIO().DeltaTime;

    for (auto &element : m_Elements) {
        if (element) {
            // Update animations
            element->UpdateAnimations(deltaTime);

            // Update container fade animations
            if (auto container = HUDCast<HUDContainer>(element)) {
                container->TickFade(deltaTime);
            }
        }
    }
}

std::shared_ptr<HUDText> HUD::AddText(const char *text, AnchorPoint anchor) {
    auto element = std::make_shared<HUDText>(text);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    return element;
}

std::shared_ptr<HUDText> HUD::AddText(const std::string &name, const char *text, AnchorPoint anchor) {
    auto element = std::make_shared<HUDText>(text);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    Register(name, element);
    return element;
}

std::shared_ptr<HUDContainer> HUD::AddVStack(AnchorPoint anchor) {
    auto container = std::make_shared<HUDContainer>(HUDLayoutKind::Vertical);
    ApplyStyle(*container);
    container->SetAnchor(anchor);
    m_Elements.push_back(container);
    return container;
}

std::shared_ptr<HUDContainer> HUD::AddVStack(const std::string &name, AnchorPoint anchor) {
    auto container = std::make_shared<HUDContainer>(HUDLayoutKind::Vertical);
    ApplyStyle(*container);
    container->SetAnchor(anchor);
    m_Elements.push_back(container);
    Register(name, container);
    return container;
}

std::shared_ptr<HUDContainer> HUD::AddHStack(AnchorPoint anchor) {
    auto container = std::make_shared<HUDContainer>(HUDLayoutKind::Horizontal);
    ApplyStyle(*container);
    container->SetAnchor(anchor);
    m_Elements.push_back(container);
    return container;
}

std::shared_ptr<HUDContainer> HUD::AddHStack(const std::string &name, AnchorPoint anchor) {
    auto container = std::make_shared<HUDContainer>(HUDLayoutKind::Horizontal);
    ApplyStyle(*container);
    container->SetAnchor(anchor);
    m_Elements.push_back(container);
    Register(name, container);
    return container;
}

std::shared_ptr<HUDContainer> HUD::AddGrid(int cols, AnchorPoint anchor) {
    auto container = std::make_shared<HUDContainer>(HUDLayoutKind::Grid, cols);
    ApplyStyle(*container);
    container->SetAnchor(anchor);
    m_Elements.push_back(container);
    return container;
}

std::shared_ptr<HUDContainer> HUD::AddGrid(const std::string &name, int cols, AnchorPoint anchor) {
    auto container = std::make_shared<HUDContainer>(HUDLayoutKind::Grid, cols);
    ApplyStyle(*container);
    container->SetAnchor(anchor);
    m_Elements.push_back(container);
    Register(name, container);
    return container;
}

std::shared_ptr<HUDImage> HUD::AddImage(ImTextureID texture, AnchorPoint anchor) {
    auto element = std::make_shared<HUDImage>(texture);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    return element;
}

std::shared_ptr<HUDImage> HUD::AddImage(const std::string &name, ImTextureID texture, AnchorPoint anchor) {
    auto element = std::make_shared<HUDImage>(texture);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    Register(name, element);
    return element;
}

std::shared_ptr<HUDProgressBar> HUD::AddProgressBar(float width, float height, AnchorPoint anchor) {
    auto element = std::make_shared<HUDProgressBar>(width, height);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    return element;
}

std::shared_ptr<HUDProgressBar> HUD::AddProgressBar(const std::string &name, float width, float height, AnchorPoint anchor) {
    auto element = std::make_shared<HUDProgressBar>(width, height);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    Register(name, element);
    return element;
}

std::shared_ptr<HUDSpacer> HUD::AddSpacer(float width, float height, AnchorPoint anchor) {
    auto element = std::make_shared<HUDSpacer>(width, height);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    return element;
}

std::shared_ptr<HUDSpacer> HUD::AddSpacer(const std::string &name, float width, float height, AnchorPoint anchor) {
    auto element = std::make_shared<HUDSpacer>(width, height);
    ApplyStyle(*element);
    element->SetAnchor(anchor);
    m_Elements.push_back(element);
    Register(name, element);
    return element;
}

bool HUD::RemoveElement(const std::shared_ptr<HUDElement> &element) {
    if (!element) return false;

    CleanupElementReferences(element);

    const auto it = std::find(m_Elements.begin(), m_Elements.end(), element);
    if (it != m_Elements.end()) {
        m_Elements.erase(it);
        return true;
    }
    return false;
}

std::shared_ptr<HUDElement> HUD::GetOrCreate(const std::string &id) {
    const auto it = m_Named.find(id);
    if (it != m_Named.end()) {
        if (auto element = it->second.lock()) {
            return element;
        }
        // Weak pointer expired, remove it
        m_Named.erase(it);
    }

    auto element = AddText("", AnchorPoint::TopLeft);
    m_Named[id] = element;
    return element;
}

std::shared_ptr<HUDElement> HUD::Find(const std::string &id) const {
    const auto it = m_Named.find(id);
    if (it != m_Named.end()) {
        return it->second.lock();
    }
    return nullptr;
}

bool HUD::Remove(const std::string &id) {
    const auto it = m_Named.find(id);
    if (it == m_Named.end()) return false;

    const auto element = it->second.lock();
    m_Named.erase(it);

    if (element) {
        return RemoveElement(element);
    }
    return false;
}

std::vector<std::string> HUD::ListIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_Named.size());
    for (const auto &pair : m_Named) {
        if (!pair.second.expired()) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

void HUD::Register(const std::string &id, const std::shared_ptr<HUDElement> &e) { m_Named[id] = e; }

// Serialization implementation
bool HUD::SaveLayoutToFile(const std::string &filePath) const {
    IniFile ini;
    SaveLayoutToIni(ini);
    return ini.WriteToFile(std::wstring(filePath.begin(), filePath.end()));
}

bool HUD::LoadLayoutFromFile(const std::string &filePath) {
    IniFile ini;
    if (!ini.ParseFromFile(std::wstring(filePath.begin(), filePath.end()))) {
        return false;
    }
    LoadLayoutFromIni(ini);
    return true;
}

void HUD::SaveLayoutToIni(IniFile &ini) const {
    // Save global settings
    ini.SetValue("hud", "active_page", m_ActivePage);

    // Count valid named elements
    std::vector<std::pair<std::string, std::shared_ptr<HUDElement>>> validElements;
    for (const auto &[name, weakElement] : m_Named) {
        if (auto element = weakElement.lock()) {
            validElements.emplace_back(name, element);
        }
    }

    ini.SetValue("hud", "element_count", std::to_string(validElements.size()));

    // Save each named element
    int elementIndex = 0;
    for (const auto &[name, element] : validElements) {
        std::string elementKey = "element_" + std::to_string(elementIndex) + "_name";
        std::string sectionKey = "element_" + std::to_string(elementIndex) + "_section";
        std::string section = "element_" + name;

        ini.SetValue("hud", elementKey, name);
        ini.SetValue("hud", sectionKey, section);
        element->ToIni(ini, section);
        elementIndex++;
    }
}

void HUD::LoadLayoutFromIni(const IniFile &ini) {
    // Clear existing elements
    m_Elements.clear();
    m_Named.clear();

    // Load global settings
    SetActivePage(ini.GetValue("hud", "active_page"));

    // Load elements
    int elementCount = std::stoi(ini.GetValue("hud", "element_count", "0"));
    for (int i = 0; i < elementCount; i++) {
        std::string elementKey = "element_" + std::to_string(i) + "_name";
        std::string sectionKey = "element_" + std::to_string(i) + "_section";

        std::string name = ini.GetValue("hud", elementKey);
        std::string section = ini.GetValue("hud", sectionKey);

        if (name.empty() || section.empty()) continue;

        std::string type = ini.GetValue(section, "type");
        auto element = CreateElementFromType(type);
        if (element) {
            element->FromIni(ini, section);
            m_Named[name] = element;
            m_Elements.push_back(element);
        }
    }
}

std::shared_ptr<HUDElement> HUD::CreateElementFromType(const std::string &type) {
    if (type == "text") {
        return std::make_shared<HUDText>();
    } else if (type == "container") {
        return std::make_shared<HUDContainer>();
    } else if (type == "image") {
        return std::make_shared<HUDImage>();
    } else if (type == "progressbar") {
        return std::make_shared<HUDProgressBar>();
    } else if (type == "spacer") {
        return std::make_shared<HUDSpacer>();
    } else {
        return std::make_shared<HUDElement>(); // Base element
    }
}

// Path resolution helpers (updated for shared_ptr)
std::shared_ptr<HUDElement> HUD::FindByPath(const std::string &path) {
    if (path.empty()) return nullptr;

    // Fast path for exact id match
    if (auto exact = Find(path)) return exact;

    const bool isAbsolute = !path.empty() && path[0] == '/';
    std::vector<std::string> segments = SplitPath(path);

    if (isAbsolute) {
        return ResolveAbsolutePath(segments);
    } else {
        return ResolveRelativePath(segments);
    }
}

std::vector<std::string> HUD::SplitPath(const std::string &path) {
    std::vector<std::string> segments;
    std::string current;

    for (char ch : path) {
        if (ch == '/') {
            segments.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    segments.push_back(current);

    // Clean up empty segments (except leading empty from absolute paths)
    std::vector<std::string> cleaned;
    cleaned.reserve(segments.size());
    for (size_t i = 0; i < segments.size(); ++i) {
        if (segments[i].empty() && i != 0) continue;
        cleaned.push_back(std::move(segments[i]));
    }

    return cleaned;
}

std::shared_ptr<HUDElement> HUD::ResolveAbsolutePath(const std::vector<std::string> &segments) const {
    if (segments.empty() || !segments[0].empty()) return nullptr; // must start with '/'
    if (segments.size() == 1) return nullptr;

    // Start from root namespace by name
    auto it = m_Named.find(segments[1]);
    if (it == m_Named.end()) return nullptr;
    auto current = it->second.lock();
    if (!current) return nullptr;

    for (size_t i = 2; i < segments.size(); ++i) {
        const auto &s = segments[i];
        if (s.empty() || s == ".") continue;
        if (s == "..") return nullptr; // no parent traversal from root

        auto container = HUDCast<HUDContainer>(current);
        if (!container) return nullptr;

        current = container->FindChild(s);
        if (!current) return nullptr;
    }
    return current;
}

std::shared_ptr<HUDElement> HUD::ResolveRelativePath(const std::vector<std::string> &segments) const {
    // Try from any named root
    for (const auto &[rootName, weakRoot] : m_Named) {
        auto root = weakRoot.lock();
        if (!root) continue;

        if (auto element = ResolveAbsolutePath([&] {
            std::vector<std::string> tmp{"" /* leading slash */, rootName};
            tmp.insert(tmp.end(), segments.begin(), segments.end());
            return tmp;
        }()))
            return element;
    }
    return nullptr;
}

std::shared_ptr<HUDElement> HUD::DescendPath(const std::shared_ptr<HUDElement> &start, const std::vector<std::string> &segments, size_t from) const {
    auto current = start;
    std::vector<std::shared_ptr<HUDContainer>> parents;

    for (size_t i = from; i < segments.size(); ++i) {
        const std::string &segment = segments[i];
        if (segment.empty()) continue;
        if (segment == ".") continue;

        if (segment == "..") {
            if (!parents.empty()) {
                current = parents.back();
                parents.pop_back();
            } else {
                return nullptr;
            }
            continue;
        }

        auto container = HUDCast<HUDContainer>(current);
        if (!container) return nullptr;

        parents.push_back(container);
        current = container->FindChild(segment);
        if (!current) return nullptr;
    }

    return current;
}

std::shared_ptr<HUDElement> HUD::GetOrCreateChild(const std::string &containerId, const std::string &childId) {
    auto element = Find(containerId);
    if (auto container = HUDCast<HUDContainer>(element)) {
        auto existing = container->FindChild(childId);
        if (existing) return existing;
        return container->AddChildNamed(childId, "");
    }
    return nullptr;
}

std::shared_ptr<HUDContainer> HUD::EnsureContainerPath(const std::string &path, HUDLayoutKind defaultKindForNew) {
    auto existing = Find(path);
    if (auto container = HUDCast<HUDContainer>(existing)) {
        return container;
    }

    auto container = std::make_shared<HUDContainer>(defaultKindForNew);
    m_Named[path] = container;
    m_Elements.push_back(container);
    return container;
}

std::shared_ptr<HUDElement> HUD::StealByPath(const std::string &path) {
    auto it = m_Named.find(path);
    if (it == m_Named.end()) return nullptr;

    auto element = it->second.lock();
    if (!element) {
        m_Named.erase(it);
        return nullptr;
    }

    m_Named.erase(it);

    auto eit = std::find(m_Elements.begin(), m_Elements.end(), element);
    if (eit != m_Elements.end()) {
        m_Elements.erase(eit);
        return element;
    }
    return nullptr;
}

void HUD::AttachToContainer(const std::shared_ptr<HUDContainer> &dest, const std::shared_ptr<HUDElement> &element, const std::string &childName) {
    if (dest && element) {
        dest->InsertChild(element, childName);
    }
}

void HUD::AttachToRoot(const std::shared_ptr<HUDElement> &element, const std::string &name) {
    if (element) {
        m_Named[name] = element;
        m_Elements.push_back(element);
    }
}

void HUD::SetAutoCreatePolicyMode(const std::string &mode) {
    m_CreatePolicyMode = mode;
}

std::string HUD::GetAutoCreatePolicyModeEffective() const {
    return m_CreatePolicyMode;
}

const std::string &HUD::GetPageDefaultContainer(const std::string &page) const {
    const auto it = m_PageDefaultContainers.find(page);
    static std::string empty;
    return (it != m_PageDefaultContainers.end()) ? it->second : empty;
}

void HUD::SetPageDefaultContainer(const std::string &page, const std::string &path) {
    m_PageDefaultContainers[page] = path;
}

void HUD::ClearPageDefaultContainer(const std::string &page) {
    m_PageDefaultContainers.erase(page);
}

std::vector<std::pair<std::string, std::string>> HUD::ListPageDefaultContainers() const {
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(m_PageDefaultContainers.size());
    for (const auto &pair : m_PageDefaultContainers) {
        result.emplace_back(pair);
    }
    return result;
}

void HUD::ApplyStyle(HUDElement &e) {
    e.SetAnchor(m_Style.anchor);
    e.SetOffset(m_Style.offset);

    if (auto textElement = HUDCast<HUDText>(std::shared_ptr<HUDElement>(&e, [](HUDElement*){}))) {
        textElement->SetScale(m_Style.scale);
        textElement->SetWrapWidthPx(m_Style.wrapWidthPx);
        textElement->SetWrapWidthFrac(m_Style.wrapWidthFrac);
        textElement->SetTabColumns(m_Style.tabColumns);
    }

    if (m_Style.drawPanel) {
        e.EnablePanel(true);
        e.SetPanelBgColor(m_Style.panelBg);
        e.SetPanelBorderColor(m_Style.panelBorder);
        e.SetPanelPadding(m_Style.panelPaddingPx);
        e.SetPanelBorderThickness(m_Style.panelBorderThickness);
        e.SetPanelRounding(m_Style.panelRounding);
    }
}

void HUD::CleanupElementReferences(const std::shared_ptr<HUDElement> &element) {
    for (auto it = m_Named.begin(); it != m_Named.end();) {
        if (it->second.lock() == element) {
            it = m_Named.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<HUDElement> HUD::CloneElement(const std::shared_ptr<const HUDElement> &src) {
    if (!src) return nullptr;

    std::shared_ptr<HUDElement> clone;

    const auto textSrc = HUDCast<const HUDText>(src);
    if (textSrc) {
        auto textClone = std::make_shared<HUDText>(textSrc->GetText());
        textClone->SetScale(textSrc->GetScale());
        textClone->SetWrapWidthPx(textSrc->GetWrapWidthPx());
        textClone->SetWrapWidthFrac(textSrc->GetWrapWidthFrac());
        textClone->SetTabColumns(textSrc->GetTabColumns());
        clone = textClone;
    } else {
        clone = std::make_shared<HUDElement>();
    }

    clone->SetAnchor(src->GetAnchor());
    clone->SetOffset(src->GetOffset());
    clone->SetVisible(src->IsVisible());
    clone->SetPage(src->GetPage());

    if (src->IsPanelEnabled()) {
        clone->EnablePanel(true);
        clone->SetPanelBgColor(src->GetPanelBgColor());
        clone->SetPanelBorderColor(src->GetPanelBorderColor());
        clone->SetPanelPadding(src->GetPanelPadding());
        clone->SetPanelBorderThickness(src->GetPanelBorderThickness());
        clone->SetPanelRounding(src->GetPanelRounding());
    }

    return clone;
}
