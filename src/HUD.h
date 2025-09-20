#ifndef BML_HUD_H
#define BML_HUD_H

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <type_traits>

#include "BML/Bui.h"
#include "AnsiText.h"

// Constants
namespace HUDConstants {
    static constexpr float DEFAULT_PANEL_PADDING = 4.0f;
    static constexpr float DEFAULT_BORDER_THICKNESS = 1.0f;
    static constexpr float DEFAULT_ROUNDING = 2.0f;
    static constexpr float MIN_SCALE = 0.1f;
    static constexpr float MAX_SCALE = 10.0f;
    static constexpr float PIXEL_SNAP_THRESHOLD = 0.5f;
}

enum class AnchorPoint {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

enum class CoordinateType { Pixels, Normalized };

// Forward declarations
class HUDElement;
class HUDText;
class HUDContainer;
class HUDImage;
class HUDProgressBar;
class HUDSpacer;
class IniFile;

// Type identification system
enum class HUDTypeId : uint8_t {
    Element     = 0,
    Text        = 1,
    Container   = 2,
    Image       = 3,
    ProgressBar = 4,
    Spacer      = 5
};

// Type-safe casting helpers
template <typename T>
constexpr HUDTypeId GetHUDTypeId() {
    if constexpr (std::is_same_v<T, HUDText>) return HUDTypeId::Text;
    else if constexpr (std::is_same_v<T, HUDContainer>) return HUDTypeId::Container;
    else if constexpr (std::is_same_v<T, HUDImage>) return HUDTypeId::Image;
    else if constexpr (std::is_same_v<T, HUDProgressBar>) return HUDTypeId::ProgressBar;
    else if constexpr (std::is_same_v<T, HUDSpacer>) return HUDTypeId::Spacer;
    else return HUDTypeId::Element;
}

struct HUDOffset {
    float x = 0.0f;
    float y = 0.0f;
    CoordinateType type = CoordinateType::Pixels;

    HUDOffset() = default;
    HUDOffset(float x, float y, CoordinateType t = CoordinateType::Pixels) : x(x), y(y), type(t) {}

    ImVec2 ToPixels(const ImVec2 &viewportSize) const {
        if (type == CoordinateType::Normalized) {
            return {x * viewportSize.x, y * viewportSize.y};
        }
        return {x, y};
    }
};

// Animation system
enum class EasingType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

struct HUDAnimation {
    enum PropertyType {
        Alpha,
        PositionX,
        PositionY,
        Scale,
        Color
    };

    PropertyType property;
    float startValue;
    float endValue;
    float duration;
    float elapsed = 0.0f;
    EasingType easing = EasingType::Linear;
    bool finished = false;

    HUDAnimation(PropertyType prop, float start, float end, float dur, EasingType ease = EasingType::Linear)
        : property(prop), startValue(start), endValue(end), duration(dur), easing(ease) {}

    float GetCurrentValue() const;
    void Update(float deltaTime);
    bool IsFinished() const { return finished; }
};

class HUDElement {
public:
    explicit HUDElement();
    virtual ~HUDElement() = default;

    // Type identification
    virtual HUDTypeId GetTypeId() const { return HUDTypeId::Element; }

    // Visibility
    HUDElement &SetVisible(bool visible);
    bool IsVisible() const { return m_Visible; }

    // Positioning
    HUDElement &SetAnchor(AnchorPoint anchor);
    AnchorPoint GetAnchor() const { return m_Anchor; }

    HUDElement &SetOffsetPixels(float x, float y);
    HUDElement &SetOffsetNormalized(float x, float y);
    HUDElement &SetOffset(const HUDOffset &offset) { m_Offset = offset; return *this;}
    const HUDOffset &GetOffset() const { return m_Offset; }

    // Page system
    HUDElement &SetPage(const std::string &page) { m_Page = page; return *this; }
    const std::string &GetPage() const { return m_Page; }

    // Panel styling
    HUDElement &EnablePanel(bool enabled);
    bool IsPanelEnabled() const { return m_DrawPanel; }
    HUDElement &SetPanelColors(ImU32 bg, ImU32 border);
    HUDElement &SetPanelBgColor(ImU32 bg);
    HUDElement &SetPanelBorderColor(ImU32 border);
    HUDElement &SetPanelPadding(float padPx);
    HUDElement &SetPanelBorderThickness(float px);
    HUDElement &SetPanelRounding(float px);

    ImU32 GetPanelBgColor() const { return m_PanelBg; }
    ImU32 GetPanelBorderColor() const { return m_PanelBorder; }
    float GetPanelPadding() const { return m_PanelPaddingPx; }
    float GetPanelBorderThickness() const { return m_PanelBorderThickness; }
    float GetPanelRounding() const { return m_PanelRounding; }

    // Alpha and visibility
    HUDElement &SetLocalAlpha(float a) { m_LocalAlpha = std::clamp(a, 0.0f, 1.0f); return *this; }
    float GetLocalAlpha() const { return m_LocalAlpha; }
    HUDElement &SetAutoVisible(bool on) { m_AutoVisible = on; return *this; }
    bool IsAutoVisible() const { return m_AutoVisible; }
    void SetInheritedAlpha(float a) { m_InheritedAlpha = a; }
    float GetInheritedAlpha() const { return m_InheritedAlpha; }

    HUDElement &Visible(bool visible) { return SetVisible(visible); }
    HUDElement &Anchor(AnchorPoint anchor) { return SetAnchor(anchor); }
    HUDElement &OffsetPx(float x, float y) { return SetOffsetPixels(x, y); }
    HUDElement &OffsetNorm(float x, float y) { return SetOffsetNormalized(x, y); }
    HUDElement &Page(const std::string &page) { return SetPage(page); }
    HUDElement &Panel(bool enabled = true) { return EnablePanel(enabled); }
    HUDElement &PanelColors(ImU32 bg, ImU32 border) { return SetPanelColors(bg, border); }
    HUDElement &PanelBg(ImU32 bg) { return SetPanelBgColor(bg); }
    HUDElement &PanelBorder(ImU32 border) { return SetPanelBorderColor(border); }
    HUDElement &PanelPadding(float px) { return SetPanelPadding(px); }
    HUDElement &PanelBorderThickness(float px) { return SetPanelBorderThickness(px); }
    HUDElement &PanelRounding(float px) { return SetPanelRounding(px); }
    HUDElement &LocalAlpha(float alpha) { return SetLocalAlpha(alpha); }
    HUDElement &AutoVisible(bool enabled) { return SetAutoVisible(enabled); }

    // Lambda-based configurator
    template <typename ConfigFunc>
    HUDElement &Configure(ConfigFunc &&func) {
        func(*this);
        return *this;
    }

    // Animation system
    void AddAnimation(const HUDAnimation &animation);
    void ClearAnimations();
    void UpdateAnimations(float deltaTime);
    bool HasActiveAnimations() const;

    // Serialization support
    virtual void ToIni(IniFile &ini, const std::string &section) const;
    virtual void FromIni(const IniFile &ini, const std::string &section);

    // Virtual interface
    virtual void Draw(ImDrawList *drawList, const ImVec2 &viewportSize);
    virtual ImVec2 GetElementSize(const ImVec2 &viewportSize) const { return {0, 0}; }

    // Position calculation
    ImVec2 CalculatePosition(const ImVec2 &elementSize, const ImVec2 &viewportSize) const;

protected:
    AnchorPoint m_Anchor = AnchorPoint::TopLeft;
    HUDOffset m_Offset;
    bool m_Visible = true;

    // Panel styling
    bool m_DrawPanel = false;
    ImU32 m_PanelBg = IM_COL32(0, 0, 0, 128);
    ImU32 m_PanelBorder = IM_COL32(255, 255, 255, 64);
    float m_PanelPaddingPx = HUDConstants::DEFAULT_PANEL_PADDING;
    float m_PanelBorderThickness = HUDConstants::DEFAULT_BORDER_THICKNESS;
    float m_PanelRounding = HUDConstants::DEFAULT_ROUNDING;

    // Page and alpha
    std::string m_Page;
    float m_InheritedAlpha = 1.0f;
    float m_LocalAlpha = 1.0f;
    bool m_AutoVisible = false;

    // Animation system
    std::vector<HUDAnimation> m_Animations;

    // Utility methods
    static float ValidateScale(float scale);
    static float ValidatePadding(float padding);
    void MarkDirty() { m_IsDirty = true; }

private:
    mutable bool m_IsDirty = true;
};

template <typename T>
std::shared_ptr<T> HUDCast(std::shared_ptr<HUDElement> element) {
    if (!element || element->GetTypeId() != GetHUDTypeId<T>()) return nullptr;
    return std::static_pointer_cast<T>(element);
}

template <typename T>
std::shared_ptr<const T> HUDCast(std::shared_ptr<const HUDElement> element) {
    if (!element || element->GetTypeId() != GetHUDTypeId<T>()) return nullptr;
    return std::static_pointer_cast<const T>(element);
}

// Raw pointer versions for existing element access
template <typename T>
T *HUDCast(HUDElement *element) {
    if (!element || element->GetTypeId() != GetHUDTypeId<T>()) return nullptr;
    return static_cast<T *>(element);
}

template <typename T>
const T *HUDCast(const HUDElement *element) {
    if (!element || element->GetTypeId() != GetHUDTypeId<T>()) return nullptr;
    return static_cast<const T *>(element);
}

class HUDText : public HUDElement {
public:
    explicit HUDText(const char *text = "");
    ~HUDText() override = default;

    // Type identification
    HUDTypeId GetTypeId() const override { return HUDTypeId::Text; }

    // Text content
    HUDText &SetText(const char *text);
    const char *GetText() const;

    // Text styling
    HUDText &SetScale(float scale);
    float GetScale() const { return m_Scale; }

    // Text layout
    HUDText &SetWrapWidthPx(float px);
    float GetWrapWidthPx() const { return m_WrapWidthPx; }
    HUDText &SetWrapWidthFrac(float frac);
    float GetWrapWidthFrac() const { return m_WrapWidthFrac; }
    HUDText &SetTabColumns(int columns);
    int GetTabColumns() const { return m_TabColumns; }

    HUDText &Text(const char *text) { return SetText(text); }
    HUDText &Scale(float scale) { return SetScale(scale); }
    HUDText &WrapPx(float px) { return SetWrapWidthPx(px); }
    HUDText &WrapFrac(float frac) { return SetWrapWidthFrac(frac); }
    HUDText &TabCols(int columns) { return SetTabColumns(columns); }

    // Lambda-based configurator for text-specific settings
    template <typename ConfigFunc>
    HUDText &ConfigureText(ConfigFunc &&func) {
        func(*this);
        return *this;
    }

    // Serialization
    void ToIni(IniFile &ini, const std::string &section) const override;
    void FromIni(const IniFile &ini, const std::string &section) override;

    // Override virtual methods
    void Draw(ImDrawList *drawList, const ImVec2 &viewportSize) override;
    ImVec2 GetElementSize(const ImVec2 &viewportSize) const override;

private:
    AnsiText::AnsiString m_AnsiText;
    float m_Scale = 1.0f;
    float m_WrapWidthPx = -1.0f;
    float m_WrapWidthFrac = 0.0f;
    int m_TabColumns = AnsiText::kDefaultTabColumns;

    // Measurement cache
    mutable struct MeasureCache {
        uint64_t textVersion = 0;
        float wrapWidth = -1.0f;
        float fontSize = -1.0f;
        int tabCols = -1;
        float fontPixelSize = -1.0f;
        ImVec2 size = ImVec2(0, 0);
    } m_MeasureCache;

    uint64_t m_TextVersion = 0;

    void Invalidate() {
        ++m_TextVersion;
        MarkDirty();
    }

    ImVec2 CalculateAnsiTextSize(const ImVec2 &viewportSize) const;
    float ResolveWrapWidth(const ImVec2 &viewportSize) const;
};

class HUDImage : public HUDElement {
public:
    explicit HUDImage(ImTextureID texture = ImTextureID_Invalid, float width = 0, float height = 0);

    HUDTypeId GetTypeId() const override { return HUDTypeId::Image; }

    HUDImage &SetTexture(ImTextureID texture) { m_Texture = texture; MarkDirty(); return *this; }
    HUDImage &SetSize(float width, float height) { m_Width = width; m_Height = height; MarkDirty(); return *this; }
    HUDImage &SetTint(ImU32 tint) { m_Tint = tint; return *this; }

    ImTextureID GetTexture() const { return m_Texture; }
    ImVec2 GetSize() const { return {m_Width, m_Height}; }
    ImU32 GetTint() const { return m_Tint; }

    void ToIni(IniFile &ini, const std::string &section) const override;
    void FromIni(const IniFile &ini, const std::string &section) override;

    void Draw(ImDrawList *drawList, const ImVec2 &viewportSize) override;
    ImVec2 GetElementSize(const ImVec2 &viewportSize) const override;

private:
    ImTextureID m_Texture;
    float m_Width, m_Height;
    ImU32 m_Tint = IM_COL32_WHITE;
};

class HUDProgressBar : public HUDElement {
public:
    explicit HUDProgressBar(float width = 100.0f, float height = 20.0f);

    HUDTypeId GetTypeId() const override { return HUDTypeId::ProgressBar; }

    HUDProgressBar &SetValue(float value) { m_Value = std::clamp(value, m_Min, m_Max); return *this; }
    HUDProgressBar &SetRange(float min, float max) { m_Min = min; m_Max = max; SetValue(m_Value); return *this; }
    HUDProgressBar &SetSize(float width, float height) { m_Width = width; m_Height = height; MarkDirty(); return *this; }
    HUDProgressBar &SetColors(ImU32 bg, ImU32 fill) { m_BgColor = bg; m_FillColor = fill; return *this; }

    float GetValue() const { return m_Value; }
    float GetProgress() const { return (m_Max - m_Min) > 0 ? (m_Value - m_Min) / (m_Max - m_Min) : 0.0f; }
    ImVec2 GetSize() const { return {m_Width, m_Height}; }

    void ToIni(IniFile &ini, const std::string &section) const override;
    void FromIni(const IniFile &ini, const std::string &section) override;

    void Draw(ImDrawList *drawList, const ImVec2 &viewportSize) override;
    ImVec2 GetElementSize(const ImVec2 &viewportSize) const override;

private:
    float m_Width, m_Height;
    float m_Value = 0.0f;
    float m_Min = 0.0f;
    float m_Max = 1.0f;
    ImU32 m_BgColor = IM_COL32(64, 64, 64, 255);
    ImU32 m_FillColor = IM_COL32(0, 255, 0, 255);
};

class HUDSpacer : public HUDElement {
public:
    explicit HUDSpacer(float width = 0.0f, float height = 0.0f);

    HUDTypeId GetTypeId() const override { return HUDTypeId::Spacer; }

    HUDSpacer& SetSize(float width, float height) { m_Width = width; m_Height = height; MarkDirty(); return *this; }
    ImVec2 GetSize() const { return {m_Width, m_Height}; }

    void ToIni(IniFile &ini, const std::string &section) const override;
    void FromIni(const IniFile &ini, const std::string &section) override;

    void Draw(ImDrawList *drawList, const ImVec2 &viewportSize) override {} // Invisible
    ImVec2 GetElementSize(const ImVec2 &viewportSize) const override;

private:
    float m_Width, m_Height;
};

// Global style for default HUD element properties
struct HUDStyle {
    AnchorPoint anchor = AnchorPoint::TopLeft;
    HUDOffset offset;
    float scale = 1.0f;
    float wrapWidthPx = -1.0f;
    float wrapWidthFrac = 0.0f;
    int tabColumns = AnsiText::kDefaultTabColumns;
    bool drawPanel = false;
    ImU32 panelBg = IM_COL32(0, 0, 0, 128);
    ImU32 panelBorder = IM_COL32(255, 255, 255, 64);
    float panelPaddingPx = HUDConstants::DEFAULT_PANEL_PADDING;
    float panelBorderThickness = HUDConstants::DEFAULT_BORDER_THICKNESS;
    float panelRounding = HUDConstants::DEFAULT_ROUNDING;
};

// Container enums
enum class HUDLayoutKind { Vertical, Horizontal, Grid };

enum class AlignX { Left, Center, Right };

enum class AlignY { Top, Middle, Bottom };

class HUDContainer : public HUDElement {
    friend class HUD;

public:
    explicit HUDContainer(HUDLayoutKind kind = HUDLayoutKind::Vertical, int gridCols = 1);

    // Type identification
    HUDTypeId GetTypeId() const override { return HUDTypeId::Container; }

    // Child management
    std::shared_ptr<HUDText> AddChild(const char *text = "");
    std::shared_ptr<HUDText> AddChildNamed(const std::string &name, const char *text = "");
    std::shared_ptr<HUDElement> FindChild(const std::string &name);
    bool RemoveChild(const std::string &name);
    std::shared_ptr<HUDContainer> AddContainerChild(HUDLayoutKind kind, const std::string &name, int gridCols = 1);
    std::shared_ptr<HUDImage> AddImageChild(const std::string &name, ImTextureID texture = ImTextureID_Invalid, float width = 0, float height = 0);
    std::shared_ptr<HUDProgressBar> AddProgressBarChild(const std::string &name, float width = 100, float height = 20);
    std::shared_ptr<HUDSpacer> AddSpacerChild(const std::string &name, float width = 0, float height = 0);

    template <typename T = HUDText>
    std::shared_ptr<T> GetOrAddChild(const std::string &name) {
        static_assert(std::is_base_of_v<HUDElement, T>, "T must derive from HUDElement");

        // Try to find existing child
        auto existing = FindChild(name);
        if (existing) {
            auto typed = HUDCast<T>(existing);
            if (typed) {
                return typed;
            }
            // Type mismatch - remove existing and create new
            RemoveChild(name);
        }

        // Create new child of appropriate type
        if constexpr (std::is_same_v<T, HUDText>) {
            return AddChildNamed(name, "");
        } else if constexpr (std::is_same_v<T, HUDContainer>) {
            return AddContainerChild(HUDLayoutKind::Vertical, name);
        } else if constexpr (std::is_same_v<T, HUDImage>) {
            return AddImageChild(name);
        } else if constexpr (std::is_same_v<T, HUDProgressBar>) {
            return AddProgressBarChild(name);
        } else if constexpr (std::is_same_v<T, HUDSpacer>) {
            return AddSpacerChild(name);
        } else {
            static_assert(std::is_same_v<T, HUDElement>, "Unsupported type for GetOrAddChild");
            auto element = std::make_shared<HUDElement>();
            auto ptr = std::static_pointer_cast<T>(element);
            InsertChild(element, name);
            return ptr;
        }
    }

    // Operator[] overload for easy child access
    template <typename T = HUDText>
    std::shared_ptr<T> operator[](const std::string &name) {
        return GetOrAddChild<T>(name);
    }

    // Non-template version
    std::shared_ptr<HUDElement> operator[](const char *name) {
        return GetOrAddChild<HUDElement>(std::string(name));
    }

    // Advanced child operations
    std::shared_ptr<HUDElement> StealChild(const std::string &name);
    void InsertChild(const std::shared_ptr<HUDElement> &element, const std::string &name);

    // Layout properties
    HUDLayoutKind GetKind() const { return m_Kind; }
    HUDContainer &SetSpacing(float px);
    float GetSpacing() const { return m_SpacingPx; }
    HUDContainer &SetGridCols(int cols);
    int GetGridCols() const { return m_GridCols; }

    // Alignment
    HUDContainer &SetAlignX(AlignX ax) { m_AlignX = ax; return *this; }
    HUDContainer &SetAlignY(AlignY ay) { m_AlignY = ay; return *this; }
    HUDContainer &SetCellAlignX(AlignX ax) { m_CellAlignX = ax; return *this; }
    HUDContainer &SetCellAlignY(AlignY ay) { m_CellAlignY = ay; return *this; }

    AlignX GetAlignX() const { return m_AlignX; }
    AlignY GetAlignY() const { return m_AlignY; }
    AlignX GetCellAlignX() const { return m_CellAlignX; }
    AlignY GetCellAlignY() const { return m_CellAlignY; }

    // Clipping
    HUDContainer &EnableClip(bool on) { m_ClipEnabled = on; return *this; }
    bool IsClipEnabled() const { return m_ClipEnabled; }
    HUDContainer &SetClipPadding(float px) { m_ClipPaddingPx = std::max(0.0f, px); return *this;}
    float GetClipPadding() const { return m_ClipPaddingPx; }

    // Fade animation
    HUDContainer &EnableFade(bool on) { m_FadeEnabled = on; return *this; }
    bool IsFadeEnabled() const { return m_FadeEnabled; }
    HUDContainer &SetAlpha(float a) { m_Alpha = std::clamp(a, 0.0f, 1.0f); return *this; }
    float GetAlpha() const { return m_Alpha; }
    HUDContainer &SetFadeTarget(float a) { m_FadeTarget = std::clamp(a, 0.0f, 1.0f); return *this; }
    float GetFadeTarget() const { return m_FadeTarget; }
    HUDContainer &SetFadeSpeed(float s) { m_FadeSpeed = std::max(0.0f, s); return *this; }
    float GetFadeSpeed() const { return m_FadeSpeed; }

    // Lambda-based configurator for container-specific settings
    template <typename ConfigFunc>
    HUDContainer &ConfigureContainer(ConfigFunc &&func) {
        func(*this);
        return *this;
    }

    // Builder pattern for adding children with configuration
    template <typename T = HUDText, typename ConfigFunc>
    std::shared_ptr<T> AddChildWith(const std::string &name, ConfigFunc &&configFunc) {
        auto child = GetOrAddChild<T>(name);
        configFunc(*child);
        return child;
    }

    // Container access
    size_t GetChildCount() const { return m_Children.size(); }
    std::shared_ptr<HUDElement> GetChild(size_t index) const;
    const std::unordered_map<std::string, std::weak_ptr<HUDElement>> &GetNamedChildren() const { return m_NamedChildren; }

    // Serialization
    void ToIni(IniFile &ini, const std::string &section) const override;
    void FromIni(const IniFile &ini, const std::string &section) override;

    // Virtual overrides
    void Draw(ImDrawList *drawList, const ImVec2 &viewportSize) override;
    ImVec2 GetElementSize(const ImVec2 &viewportSize) const override;
    void TickFade(float dt);

private:
    HUDLayoutKind m_Kind;
    int m_GridCols = 1;
    float m_SpacingPx = 4.0f;
    AlignX m_AlignX = AlignX::Left;
    AlignY m_AlignY = AlignY::Top;
    AlignX m_CellAlignX = AlignX::Left;
    AlignY m_CellAlignY = AlignY::Top;
    bool m_ClipEnabled = false;
    float m_ClipPaddingPx = 0.0f;
    bool m_FadeEnabled = false;
    float m_Alpha = 1.0f;
    float m_FadeTarget = 1.0f;
    float m_FadeSpeed = 3.0f;

    std::vector<std::shared_ptr<HUDElement>> m_Children;
    std::unordered_map<std::string, std::weak_ptr<HUDElement>> m_NamedChildren;

    // Performance optimization - size caching
    mutable bool m_SizeCacheDirty = true;
    mutable ImVec2 m_CachedContentSize = ImVec2(0, 0);
    mutable ImVec2 m_LastViewportSize = ImVec2(0, 0);

    // Layout helpers
    ImVec2 CalculateContentSize(const ImVec2 &viewportSize) const;
    void LayoutVertical(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, float alphaMul);
    void LayoutHorizontal(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, float alphaMul);
    void LayoutGrid(ImDrawList *drawList, const ImVec2 &viewportSize, const ImVec2 &origin, float alphaMul);
    void InvalidateSizeCache() { m_SizeCacheDirty = true; }
};

class HUD : public Bui::Window {
public:
    HUD();
    ~HUD() override;

    // Window interface
    ImGuiWindowFlags GetFlags() override;
    void OnPreBegin() override;
    void OnDraw() override;
    void OnProcess();

    // Element creation
    std::shared_ptr<HUDText> AddText(const char *text, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDText> AddText(const std::string &name, const char *text, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDContainer> AddVStack(AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDContainer> AddVStack(const std::string &name, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDContainer> AddHStack(AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDContainer> AddHStack(const std::string &name, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDContainer> AddGrid(int cols, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDContainer> AddGrid(const std::string &name, int cols, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDImage> AddImage(ImTextureID texture, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDImage> AddImage(const std::string &name, ImTextureID texture, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDProgressBar> AddProgressBar(float width = 100, float height = 20, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDProgressBar> AddProgressBar(const std::string &name, float width = 100, float height = 20, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDSpacer> AddSpacer(float width = 0, float height = 0, AnchorPoint anchor = AnchorPoint::TopLeft);
    std::shared_ptr<HUDSpacer> AddSpacer(const std::string &name, float width = 0, float height = 0, AnchorPoint anchor = AnchorPoint::TopLeft);

    // Element management
    bool RemoveElement(const std::shared_ptr<HUDElement> &element);
    std::shared_ptr<HUDElement> GetOrCreate(const std::string &id);
    std::shared_ptr<HUDElement> Find(const std::string &id) const;
    bool Remove(const std::string &id);
    std::vector<std::string> ListIds() const;
    void Register(const std::string &id, const std::shared_ptr<HUDElement> &e);

    // Path-based operations
    std::shared_ptr<HUDElement> FindByPath(const std::string &path);
    std::shared_ptr<HUDElement> GetOrCreateChild(const std::string &containerId, const std::string &childId);
    std::shared_ptr<HUDContainer> EnsureContainerPath(const std::string &path, HUDLayoutKind defaultKindForNew = HUDLayoutKind::Vertical);
    std::shared_ptr<HUDElement> StealByPath(const std::string &path);
    void AttachToContainer(const std::shared_ptr<HUDContainer> &dest, const std::shared_ptr<HUDElement> &element, const std::string &childName);
    void AttachToRoot(const std::shared_ptr<HUDElement> &element, const std::string &name);

    // Page system
    void SetActivePage(const std::string &name) { m_ActivePage = name; }
    const std::string &GetActivePage() const { return m_ActivePage; }
    const std::string &GetPageDefaultContainer(const std::string &page) const;
    void SetPageDefaultContainer(const std::string &page, const std::string &path);
    void ClearPageDefaultContainer(const std::string &page);
    std::vector<std::pair<std::string, std::string>> ListPageDefaultContainers() const;

    // Auto-creation policy
    using CreatePolicy = std::function<HUDLayoutKind(const std::string &parentPath, const std::string &segment, int depth)>;
    void SetAutoCreatePolicy(CreatePolicy policy) { m_CreatePolicy = std::move(policy); }
    void SetAutoCreatePolicyMode(const std::string &mode);
    const std::string &GetAutoCreatePolicyMode() const { return m_CreatePolicyMode; }
    std::string GetAutoCreatePolicyModeEffective() const;

    // Style management
    void SetStyle(const HUDStyle &style) { m_Style = style; }
    const HUDStyle &GetStyle() const { return m_Style; }

    // Serialization system
    bool SaveLayoutToFile(const std::string &filePath) const;
    bool LoadLayoutFromFile(const std::string &filePath);
    void SaveLayoutToIni(IniFile &ini) const;
    void LoadLayoutFromIni(const IniFile &ini);

    // Utility
    static std::shared_ptr<HUDElement> CloneElement(const std::shared_ptr<const HUDElement> &src) ;

private:
    std::vector<std::shared_ptr<HUDElement>> m_Elements;
    std::unordered_map<std::string, std::weak_ptr<HUDElement>> m_Named;
    std::string m_ActivePage;
    CreatePolicy m_CreatePolicy;
    std::unordered_map<std::string, std::string> m_PageDefaultContainers;
    std::string m_CreatePolicyMode;
    HUDStyle m_Style;

    // Path resolution helpers
    static std::vector<std::string> SplitPath(const std::string &path) ;
    std::shared_ptr<HUDElement> ResolveAbsolutePath(const std::vector<std::string> &segments) const;
    std::shared_ptr<HUDElement> ResolveRelativePath(const std::vector<std::string> &segments) const;
    std::shared_ptr<HUDElement> DescendPath(const std::shared_ptr<HUDElement> &start, const std::vector<std::string> &segments, size_t from) const;

    // Internal helpers
    void ApplyStyle(HUDElement &e);
    void CleanupElementReferences(const std::shared_ptr<HUDElement> &element);
    static std::shared_ptr<HUDElement> CreateElementFromType(const std::string &type);
};

#endif // BML_HUD_H
