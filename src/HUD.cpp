#include "HUD.h"

#include <cmath>
#include <cassert>
#include <algorithm>
#include <limits>
#include <unordered_map>

#include "AnsiPalette.h"

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

std::shared_ptr<HUDElement> HUDElement::Clone() const {
    return std::make_shared<HUDElement>(*this);
}

HUDElement &HUDElement::SetVisible(bool visible) {
    if (m_Visible == visible) return *this;
    m_Visible = visible;
    return *this;
}

HUDElement &HUDElement::SetAnchor(AnchorPoint anchor) {
    if (m_Anchor == anchor) return *this;
    m_Anchor = anchor;
    return *this;
}

HUDElement &HUDElement::SetOffsetPixels(float x, float y) {
    SetOffsetType(CoordinateType::Pixels);
    SetOffsetValues(x, y);
    return *this;
}

HUDElement &HUDElement::SetOffsetNormalized(float x, float y) {
    SetOffsetType(CoordinateType::Normalized);
    SetOffsetValues(x, y);
    return *this;
}

HUDElement &HUDElement::SetOffsetValues(float x, float y) {
    if (m_Offset.x == x && m_Offset.y == y)
        return *this;
    m_Offset.x = x;
    m_Offset.y = y;
    return *this;
}

HUDElement &HUDElement::SetOffsetType(CoordinateType type) {
    if (m_Offset.type == type)
        return *this;
    m_Offset.type = type;
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
        if (!ApplyAnimated(anim.property, anim.GetCurrentValue()))
            anim.finished = true;
    }

    // Remove finished animations
    m_Animations.erase(
        std::remove_if(m_Animations.begin(), m_Animations.end(),
                       [](const HUDAnimation &anim) { return anim.IsFinished(); }),
        m_Animations.end());
}

bool HUDElement::ApplyAnimated(HUDAnimation::PropertyType property, float value) {
    switch (property) {
    case HUDAnimation::Alpha:
        SetLocalAlpha(value);
        return true;
    case HUDAnimation::PositionX:
        SetOffsetValues(value, m_Offset.y);
        return true;
    case HUDAnimation::PositionY:
        SetOffsetValues(m_Offset.x, value);
        return true;
    default:
        return false;
    }
}

bool HUDElement::HasActiveAnimations() const {
    return !m_Animations.empty();
}

ImVec2 HUDElement::ResolveDrawPosition(const ImVec2 &viewportSize) const {
    const ImVec2 elementSize = GetElementSize(viewportSize);
    ImVec2 pos = CalculatePosition(elementSize, viewportSize);
    pos.x = floorf(pos.x + HUDConstants::PIXEL_ROUND_BIAS);
    pos.y = floorf(pos.y + HUDConstants::PIXEL_ROUND_BIAS);
    return pos;
}

void HUDElement::DrawPanel(ImDrawList *drawList, const ImVec2 &pos, const ImVec2 &elementSize, float alpha) const {
    if (!m_DrawPanel || !drawList) return;

    const ImVec2 p0(pos.x - m_PanelPaddingPx, pos.y - m_PanelPaddingPx);
    const ImVec2 p1(pos.x + elementSize.x + m_PanelPaddingPx, pos.y + elementSize.y + m_PanelPaddingPx);

    auto scaleAlpha = [alpha](ImU32 c) -> ImU32 {
        const float a = (((c >> 24) & 0xFF) / 255.0f) * std::clamp(alpha, 0.0f, 1.0f);
        return (c & 0x00FFFFFF) | (static_cast<ImU32>(static_cast<int>(std::roundf(a * 255.0f))) << 24);
    };

    if ((m_PanelBg >> 24) & 0xFF)
        drawList->AddRectFilled(p0, p1, scaleAlpha(m_PanelBg), m_PanelRounding);
    if (m_PanelBorderThickness > 0.0f && ((m_PanelBorder >> 24) & 0xFF))
        drawList->AddRect(p0, p1, scaleAlpha(m_PanelBorder), m_PanelRounding, 0, m_PanelBorderThickness);
}

void HUDElement::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList) return;
    DrawAt(drawList, ResolveDrawPosition(viewportSize), viewportSize, m_InheritedAlpha * m_LocalAlpha);
}

void HUDElement::DrawAt(ImDrawList *drawList, const ImVec2 &pos, const ImVec2 &viewportSize, float alpha) {
    if (!drawList) return;
    DrawPanel(drawList, pos, GetElementSize(viewportSize), alpha);
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

std::shared_ptr<HUDElement> HUDText::Clone() const {
    return std::make_shared<HUDText>(*this);
}

bool HUDText::ApplyAnimated(HUDAnimation::PropertyType property, float value) {
    if (property == HUDAnimation::Scale) {
        SetScale(value);
        return true;
    }
    return HUDElement::ApplyAnimated(property, value);
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

void HUDText::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || m_AnsiText.IsEmpty() || !drawList) return;
    DrawAt(drawList, ResolveDrawPosition(viewportSize), viewportSize, m_InheritedAlpha * m_LocalAlpha);
}

void HUDText::DrawAt(ImDrawList *drawList, const ImVec2 &pos, const ImVec2 &viewportSize, float alpha) {
    if (!drawList || m_AnsiText.IsEmpty()) return;

    const float fontSize = ImGui::GetFontSize() * m_Scale;
    if (fontSize <= 0.0f) return;

    const float wrapWidth = ResolveWrapWidth(viewportSize);
    const ImVec2 textSize = CalculateAnsiTextSize(viewportSize);

    DrawPanel(drawList, pos, textSize, alpha);

    AnsiText::TextOptions drawOptions;
    drawOptions.font = ImGui::GetFont();
    drawOptions.fontSize = fontSize;
    drawOptions.wrapWidth = wrapWidth;
    drawOptions.alpha = std::clamp(alpha, 0.0f, 1.0f);
    drawOptions.tabColumns = m_TabColumns;
    AnsiText::Renderer::DrawText(drawList, m_AnsiText, pos, drawOptions);
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

    AnsiText::TextOptions measureOptions;
    measureOptions.font = ImGui::GetFont();
    measureOptions.fontSize = fontSize;
    measureOptions.wrapWidth = wrapWidth;
    measureOptions.tabColumns = m_TabColumns;
    const ImVec2 size = AnsiText::CalcTextSize(m_AnsiText, measureOptions);

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

std::shared_ptr<HUDElement> HUDImage::Clone() const {
    return std::make_shared<HUDImage>(*this);
}

bool HUDImage::ApplyAnimated(HUDAnimation::PropertyType property, float value) {
    if (property == HUDAnimation::Color) {
        const double color = std::clamp(static_cast<double>(value), 0.0,
                                        static_cast<double>(std::numeric_limits<ImU32>::max()));
        SetTint(static_cast<ImU32>(color));
        return true;
    }
    return HUDElement::ApplyAnimated(property, value);
}

void HUDImage::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList || !m_Texture) return;
    DrawAt(drawList, ResolveDrawPosition(viewportSize), viewportSize, m_InheritedAlpha * m_LocalAlpha);
}

void HUDImage::DrawAt(ImDrawList *drawList, const ImVec2 &pos, const ImVec2 &viewportSize, float alpha) {
    if (!drawList || !m_Texture) return;

    const ImVec2 elementSize = GetElementSize(viewportSize);
    DrawPanel(drawList, pos, elementSize, alpha);

    const float a = (((m_Tint >> 24) & 0xFF) / 255.0f) * std::clamp(alpha, 0.0f, 1.0f);
    const ImU32 tint = (m_Tint & 0x00FFFFFF) | (static_cast<ImU32>(static_cast<int>(std::roundf(a * 255.0f))) << 24);
    drawList->AddImage(m_Texture, pos, ImVec2(pos.x + elementSize.x, pos.y + elementSize.y),
                       ImVec2(0, 0), ImVec2(1, 1), tint);
}

ImVec2 HUDImage::GetElementSize(const ImVec2 &viewportSize) const {
    return {m_Width, m_Height};
}

// HUDProgressBar Implementation
HUDProgressBar::HUDProgressBar(float width, float height)
    : HUDElement(), m_Width(width), m_Height(height) {}

std::shared_ptr<HUDElement> HUDProgressBar::Clone() const {
    return std::make_shared<HUDProgressBar>(*this);
}

bool HUDProgressBar::ApplyAnimated(HUDAnimation::PropertyType property, float value) {
    if (property == HUDAnimation::Color) {
        const double color = std::clamp(static_cast<double>(value), 0.0,
                                        static_cast<double>(std::numeric_limits<ImU32>::max()));
        SetColors(m_BgColor, static_cast<ImU32>(color));
        return true;
    }
    return HUDElement::ApplyAnimated(property, value);
}

void HUDProgressBar::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList) return;
    DrawAt(drawList, ResolveDrawPosition(viewportSize), viewportSize, m_InheritedAlpha * m_LocalAlpha);
}

void HUDProgressBar::DrawAt(ImDrawList *drawList, const ImVec2 &pos, const ImVec2 &viewportSize, float alpha) {
    if (!drawList) return;

    const ImVec2 elementSize = GetElementSize(viewportSize);
    DrawPanel(drawList, pos, elementSize, alpha);

    auto scaleAlpha = [alpha](ImU32 c) -> ImU32 {
        const float a = (((c >> 24) & 0xFF) / 255.0f) * std::clamp(alpha, 0.0f, 1.0f);
        return (c & 0x00FFFFFF) | (static_cast<ImU32>(static_cast<int>(std::roundf(a * 255.0f))) << 24);
    };

    const ImVec2 p1(pos.x + elementSize.x, pos.y + elementSize.y);
    drawList->AddRectFilled(pos, p1, scaleAlpha(m_BgColor));

    const float progress = GetProgress();
    if (progress > 0.0f) {
        const ImVec2 fillEnd(pos.x + elementSize.x * progress, pos.y + elementSize.y);
        drawList->AddRectFilled(pos, fillEnd, scaleAlpha(m_FillColor));
    }
}

ImVec2 HUDProgressBar::GetElementSize(const ImVec2 &viewportSize) const {
    return {m_Width, m_Height};
}

// HUDSpacer Implementation
HUDSpacer::HUDSpacer(float width, float height)
    : HUDElement(), m_Width(width), m_Height(height) {}

std::shared_ptr<HUDElement> HUDSpacer::Clone() const {
    return std::make_shared<HUDSpacer>(*this);
}

ImVec2 HUDSpacer::GetElementSize(const ImVec2 &viewportSize) const {
    return {m_Width, m_Height};
}

// =============================================================================
// HUDContainer Implementation
// =============================================================================

HUDContainer::HUDContainer(HUDLayoutKind kind, int gridCols)
    : HUDElement(), m_Kind(kind), m_GridCols(gridCols > 0 ? gridCols : 1) {}

std::shared_ptr<HUDElement> HUDContainer::Clone() const {
    auto clone = std::make_shared<HUDContainer>(*this);
    clone->m_Children.clear();
    clone->m_NamedChildren.clear();

    clone->m_Children.reserve(m_Children.size());
    for (const auto &child : m_Children) {
        if (!child) {
            clone->m_Children.push_back(nullptr);
            continue;
        }

        auto childClone = child->Clone();
        clone->m_Children.push_back(std::move(childClone));
    }

    for (const auto &[name, index] : m_NamedChildren) {
        if (index < clone->m_Children.size())
            clone->m_NamedChildren.emplace(name, index);
    }

    clone->InvalidateSizeCache();
    return clone;
}

std::shared_ptr<HUDText> HUDContainer::AddChild(const char *text) {
    auto child = std::make_shared<HUDText>(text);
    m_Children.push_back(child);
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDText> HUDContainer::AddChildNamed(const std::string &name, const char *text) {
    auto child = std::make_shared<HUDText>(text);
    m_NamedChildren[name] = m_Children.size();
    m_Children.push_back(child);
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDImage> HUDContainer::AddImageChild(const std::string &name, ImTextureID texture, float width, float height) {
    auto child = std::make_shared<HUDImage>(texture, width, height);
    m_NamedChildren[name] = m_Children.size();
    m_Children.push_back(child);
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDProgressBar> HUDContainer::AddProgressBarChild(const std::string &name, float width, float height) {
    auto child = std::make_shared<HUDProgressBar>(width, height);
    m_NamedChildren[name] = m_Children.size();
    m_Children.push_back(child);
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDSpacer> HUDContainer::AddSpacerChild(const std::string &name, float width, float height) {
    auto child = std::make_shared<HUDSpacer>(width, height);
    m_NamedChildren[name] = m_Children.size();
    m_Children.push_back(child);
    InvalidateSizeCache();
    return child;
}

std::shared_ptr<HUDElement> HUDContainer::FindChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    return it != m_NamedChildren.end() && it->second < m_Children.size()
        ? m_Children[it->second]
        : nullptr;
}

bool HUDContainer::RemoveChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    if (it == m_NamedChildren.end()) return false;
    return DetachChild(it->second) != nullptr;
}

std::shared_ptr<HUDContainer> HUDContainer::AddContainerChild(HUDLayoutKind kind, const std::string &name, int gridCols) {
    auto container = std::make_shared<HUDContainer>(kind, gridCols);
    m_NamedChildren[name] = m_Children.size();
    m_Children.push_back(container);
    InvalidateSizeCache();
    return container;
}

std::shared_ptr<HUDElement> HUDContainer::StealChild(const std::string &name) {
    const auto it = m_NamedChildren.find(name);
    if (it == m_NamedChildren.end()) return nullptr;
    return DetachChild(it->second);
}

void HUDContainer::InsertChild(const std::shared_ptr<HUDElement> &element, const std::string &name) {
    if (element) {
        m_NamedChildren[name] = m_Children.size();
        m_Children.push_back(element);
        InvalidateSizeCache();
    }
}

std::shared_ptr<HUDElement> HUDContainer::DetachChild(size_t index) {
    if (index >= m_Children.size())
        return nullptr;

    std::shared_ptr<HUDElement> element = std::move(m_Children[index]);
    m_Children.erase(m_Children.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto it = m_NamedChildren.begin(); it != m_NamedChildren.end();) {
        if (it->second == index) {
            it = m_NamedChildren.erase(it);
        } else {
            if (it->second > index)
                --it->second;
            ++it;
        }
    }
    InvalidateSizeCache();
    return element;
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

void HUDContainer::Draw(ImDrawList *drawList, const ImVec2 &viewportSize) {
    if (!m_Visible || !drawList) return;
    DrawAt(drawList, ResolveDrawPosition(viewportSize), viewportSize, m_InheritedAlpha * m_LocalAlpha);
}

void HUDContainer::DrawAt(ImDrawList *drawList, const ImVec2 &pos, const ImVec2 &viewportSize, float alpha) {
    if (!drawList) return;

    // Check if any children are visible
    bool hasVisible = false;
    for (const auto &child : m_Children) {
        if (child && child->IsVisible()) { hasVisible = true; break; }
    }
    if (!hasVisible) return;

    const float alphaMul = alpha * (m_FadeEnabled ? m_Alpha : 1.0f);
    const ImVec2 contentSize = CalculateContentSize(viewportSize);

    DrawPanel(drawList, pos, contentSize, alphaMul);

    if (m_ClipEnabled) {
        const ImVec2 clipMin(pos.x - m_ClipPaddingPx, pos.y - m_ClipPaddingPx);
        const ImVec2 clipMax(pos.x + contentSize.x + m_ClipPaddingPx, pos.y + contentSize.y + m_ClipPaddingPx);
        drawList->PushClipRect(clipMin, clipMax, true);
    }

    ImVec2 childOrigin = pos;
    if (m_DrawPanel) {
        childOrigin.x += m_PanelPaddingPx;
        childOrigin.y += m_PanelPaddingPx;
    }

    switch (m_Kind) {
    case HUDLayoutKind::Vertical:
        LayoutVertical(drawList, viewportSize, childOrigin, contentSize, alphaMul);
        break;
    case HUDLayoutKind::Horizontal:
        LayoutHorizontal(drawList, viewportSize, childOrigin, contentSize, alphaMul);
        break;
    case HUDLayoutKind::Grid:
        LayoutGrid(drawList, viewportSize, childOrigin, contentSize, alphaMul);
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

void HUDContainer::LayoutVertical(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, const ImVec2 &contentSize, float alphaMul) {
    float y = origin.y;

    for (auto &child : m_Children) {
        if (!child || !child->IsVisible()) continue;

        const ImVec2 childSize = child->GetElementSize(viewportSize);
        float x = origin.x;

        if (m_AlignX == AlignX::Center) x += (contentSize.x - childSize.x) * 0.5f;
        else if (m_AlignX == AlignX::Right) x += (contentSize.x - childSize.x);

        child->DrawAt(drawList, ImVec2(x, y), viewportSize, alphaMul);
        y += childSize.y + m_SpacingPx;
    }
}

void HUDContainer::LayoutHorizontal(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, const ImVec2 &contentSize, float alphaMul) {
    float x = origin.x;

    for (auto &child : m_Children) {
        if (!child || !child->IsVisible()) continue;

        const ImVec2 childSize = child->GetElementSize(viewportSize);
        float y = origin.y;

        if (m_AlignY == AlignY::Middle) y += (contentSize.y - childSize.y) * 0.5f;
        else if (m_AlignY == AlignY::Bottom) y += (contentSize.y - childSize.y);

        child->DrawAt(drawList, ImVec2(x, y), viewportSize, alphaMul);
        x += childSize.x + m_SpacingPx;
    }
}

void HUDContainer::LayoutGrid(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, const ImVec2 &/*contentSize*/, float alphaMul) {
    // Collect visible children and their sizes (raw pointers to avoid refcount churn)
    struct Entry { HUDElement *element; ImVec2 size; };
    std::vector<Entry> entries;
    entries.reserve(m_Children.size());

    for (auto &child : m_Children) {
        if (child && child->IsVisible())
            entries.push_back({child.get(), child->GetElementSize(viewportSize)});
    }
    if (entries.empty()) return;

    const int cols = std::max(1, m_GridCols);
    const int rows = static_cast<int>((entries.size() + cols - 1) / cols);

    std::vector<float> colW(cols, 0.0f), rowH(rows, 0.0f);
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        colW[i % cols] = std::max(colW[i % cols], entries[i].size.x);
        rowH[i / cols] = std::max(rowH[i / cols], entries[i].size.y);
    }

    std::vector<float> colX(cols, origin.x);
    for (int c = 1; c < cols; ++c)
        colX[c] = colX[c - 1] + colW[c - 1] + m_SpacingPx;

    std::vector<float> rowY(rows, origin.y);
    for (int r = 1; r < rows; ++r)
        rowY[r] = rowY[r - 1] + rowH[r - 1] + m_SpacingPx;

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const int c = i % cols, r = i / cols;
        float x = colX[c], y = rowY[r];

        if (m_CellAlignX == AlignX::Center) x += (colW[c] - entries[i].size.x) * 0.5f;
        else if (m_CellAlignX == AlignX::Right) x += (colW[c] - entries[i].size.x);
        if (m_CellAlignY == AlignY::Middle) y += (rowH[r] - entries[i].size.y) * 0.5f;
        else if (m_CellAlignY == AlignY::Bottom) y += (rowH[r] - entries[i].size.y);

        entries[i].element->DrawAt(drawList, ImVec2(x, y), viewportSize, alphaMul);
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

static void UpdateElementTree(const std::shared_ptr<HUDElement> &element, float deltaTime) {
    if (!element) return;

    element->UpdateAnimations(deltaTime);

    if (auto container = HUDCast<HUDContainer>(element)) {
        container->TickFade(deltaTime);

        const size_t childCount = container->GetChildCount();
        for (size_t i = 0; i < childCount; ++i) {
            if (auto child = container->GetChild(i)) {
                UpdateElementTree(child, deltaTime);
            }
        }
    }
}

void HUD::OnProcess() {
    const float deltaTime = ImGui::GetIO().DeltaTime;

    for (auto &element : m_Elements) {
        UpdateElementTree(element, deltaTime);
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

    const auto it = std::find(m_Elements.begin(), m_Elements.end(), element);
    return it != m_Elements.end() &&
           DetachElement(static_cast<size_t>(std::distance(m_Elements.begin(), it))) != nullptr;
}

std::shared_ptr<HUDElement> HUD::GetOrCreate(const std::string &id) {
    if (auto element = Find(id))
        return element;

    auto element = AddText("", AnchorPoint::TopLeft);
    m_Named[id] = m_Elements.size() - 1;
    return element;
}

std::shared_ptr<HUDElement> HUD::Find(const std::string &id) const {
    const auto it = m_Named.find(id);
    return it != m_Named.end() && it->second < m_Elements.size()
        ? m_Elements[it->second]
        : nullptr;
}

bool HUD::Remove(const std::string &id) {
    const auto it = m_Named.find(id);
    if (it == m_Named.end()) return false;
    return DetachElement(it->second) != nullptr;
}

std::vector<std::string> HUD::ListIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_Named.size());
    for (const auto &pair : m_Named) {
        if (pair.second < m_Elements.size()) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

void HUD::Register(const std::string &id, const std::shared_ptr<HUDElement> &element) {
    if (!element)
        return;

    auto it = std::find(m_Elements.begin(), m_Elements.end(), element);
    if (it == m_Elements.end()) {
        m_Elements.push_back(element);
        m_Named[id] = m_Elements.size() - 1;
    } else {
        m_Named[id] = static_cast<size_t>(std::distance(m_Elements.begin(), it));
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
    auto current = Find(segments[1]);
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
    if (segments.empty()) return nullptr;

    const std::string &first = segments[0];
    if (!first.empty()) {
        if (auto root = Find(first)) {
            if (segments.size() == 1) {
                return root;
            }
            if (auto resolved = DescendPath(root, segments, 1)) {
                return resolved;
            }
        }
    }

    std::shared_ptr<HUDElement> match;
    for (const auto &[rootName, index] : m_Named) {
        if (index >= m_Elements.size()) continue;
        const auto &root = m_Elements[index];

        auto element = DescendPath(root, segments, 0);
        if (!element) continue;

        // A relative path has no root context. Reject ambiguous matches instead
        // of selecting whichever unordered_map entry happens to be visited first.
        if (match && match != element)
            return nullptr;
        match = std::move(element);
    }
    return match;
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
    m_Named[path] = m_Elements.size();
    m_Elements.push_back(container);
    return container;
}

std::shared_ptr<HUDElement> HUD::StealByPath(const std::string &path) {
    auto it = m_Named.find(path);
    if (it == m_Named.end()) return nullptr;
    return DetachElement(it->second);
}

void HUD::AttachToContainer(const std::shared_ptr<HUDContainer> &dest, const std::shared_ptr<HUDElement> &element, const std::string &childName) {
    if (dest && element) {
        dest->InsertChild(element, childName);
    }
}

void HUD::AttachToRoot(const std::shared_ptr<HUDElement> &element, const std::string &name) {
    Register(name, element);
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

    if (auto *textElement = HUDCast<HUDText>(&e)) {
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

std::shared_ptr<HUDElement> HUD::DetachElement(size_t index) {
    if (index >= m_Elements.size())
        return nullptr;

    std::shared_ptr<HUDElement> element = std::move(m_Elements[index]);
    m_Elements.erase(m_Elements.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto it = m_Named.begin(); it != m_Named.end();) {
        if (it->second == index) {
            it = m_Named.erase(it);
        } else {
            if (it->second > index)
                --it->second;
            ++it;
        }
    }
    return element;
}

std::shared_ptr<HUDElement> HUD::CloneElement(const std::shared_ptr<const HUDElement> &src) {
    return src ? src->Clone() : nullptr;
}
