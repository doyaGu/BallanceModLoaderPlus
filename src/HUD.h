#ifndef BML_HUD_H
#define BML_HUD_H

#include <cstdint>
#include <array>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>
#include <utility>

#include "BML/Bui.h"

#include "AnsiText.h"
#include "AnsiPalette.h"

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

class HUDElement {
public:
    explicit HUDElement(const char *text = "", AnchorPoint anchor = AnchorPoint::TopLeft);
    virtual ~HUDElement() = default;

    void SetText(const char *text);
    const char *GetText() const;

    void SetVisible(bool visible);
    bool IsVisible() const;

    void SetAnchor(AnchorPoint anchor);
    void SetOffset(float x, float y);
    AnchorPoint GetAnchor() const { return m_Anchor; }
    ImVec2 GetOffset() const { return m_Offset; }
    void SetColor(ImU32 color);
    ImU32 GetColor() const { return m_Color; }
    void SetScale(float scale);
    float GetScale() const { return m_Scale; }
    void SetWrapWidthPx(float px);
    float GetWrapWidthPx() const { return m_WrapWidthPx; }
    void SetWrapWidthFrac(float frac);
    float GetWrapWidthFrac() const { return m_WrapWidthFrac; }
    void SetTabColumns(int columns);
    int GetTabColumns() const { return m_TabColumns; }
    void SetPage(const std::string &page) { m_Page = page; }
    const std::string &GetPage() const { return m_Page; }

    void SetTemplate(const std::string &tpl) {
        if (m_Template == tpl) return; // avoid unnecessary invalidation
        m_Template = tpl;
        m_UseTemplate = !tpl.empty();
        Invalidate();
    }
    bool HasTemplate() const { return m_UseTemplate; }
    const std::string &GetTemplate() const { return m_Template; }

    void EnablePanel(bool enabled);
    bool IsPanelEnabled() const { return m_DrawPanel; }
    void SetPanelColors(ImU32 bg, ImU32 border);
    void SetPanelBgColor(ImU32 bg);
    void SetPanelBorderColor(ImU32 border);
    void SetPanelPadding(float padPx);
    float GetPanelPadding() const { return m_PanelPaddingPx; }
    void SetPanelBorderThickness(float px);
    float GetPanelBorderThickness() const { return m_PanelBorderThickness; }
    void SetPanelRounding(float px);
    float GetPanelRounding() const { return m_PanelRounding; }
    ImU32 GetPanelBgColor() const { return m_PanelBg; }
    ImU32 GetPanelBorderColor() const { return m_PanelBorder; }

    virtual void Draw(ImDrawList *drawList, const ImVec2 &viewportSize);

    ImVec2 CalculatePosition(const ImVec2 &textSize, const ImVec2 &viewportSize) const;
    ImVec2 CalculateAnsiTextSize(const ImVec2 &viewportSize) const;

protected:
    AnsiText::AnsiString m_AnsiText;
    AnchorPoint m_Anchor;
    ImVec2 m_Offset;
    ImU32 m_Color;
    float m_Scale;
    bool m_Visible;

    // Layout
    float m_WrapWidthPx = -1.0f;      // >0: pixels, otherwise disabled
    float m_WrapWidthFrac = 0.0f;     // >0: fraction of viewport width
    int   m_TabColumns = AnsiText::kDefaultTabColumns;

    // Panel styling (optional background like TUI block)
    bool  m_DrawPanel = false;
    ImU32 m_PanelBg = IM_COL32(0,0,0,128);
    ImU32 m_PanelBorder = IM_COL32(255,255,255,64);
    float m_PanelPaddingPx = 4.0f;
    float m_PanelBorderThickness = 1.0f;
    float m_PanelRounding = 2.0f;

    // Extra metadata
    std::string m_Page;          // page tag (empty = global)
    std::string m_Template;      // optional template string
    bool m_UseTemplate = false;

    // Measurement cache
    struct MeasureCache { uint64_t textVersion = 0; float wrapWidth = -1.0f; float fontSize = -1.0f; int tabCols = -1; ImVec2 size = ImVec2(0, 0); } m_MeasureCache;
    uint64_t m_TextVersion = 0; // bump on text/template change
    void Invalidate() { ++m_TextVersion; }
};

// Global style for default HUD element properties
struct HUDStyle {
    AnchorPoint anchor = AnchorPoint::TopLeft;
    ImVec2 offset = ImVec2(0.0f, 0.0f);
    ImU32 color = IM_COL32_WHITE;
    float scale = 1.0f;
    float wrapWidthPx = -1.0f;   // <=0 disabled
    float wrapWidthFrac = 0.0f;  // <=0 disabled
    int tabColumns = AnsiText::kDefaultTabColumns;
    // Panel defaults
    bool drawPanel = false;
    ImU32 panelBg = IM_COL32(0, 0, 0, 128);
    ImU32 panelBorder = IM_COL32(255, 255, 255, 64);
    float panelPaddingPx = 4.0f;
    float panelBorderThickness = 1.0f;
    float panelRounding = 2.0f;
};

// Container kinds
enum class HUDLayoutKind { Vertical, Horizontal, Grid };
enum class AlignX { Left, Center, Right };
enum class AlignY { Top, Middle, Bottom };

class HUDContainer : public HUDElement {
public:
    explicit HUDContainer(HUDLayoutKind kind = HUDLayoutKind::Vertical, int gridCols = 1)
        : HUDElement("", AnchorPoint::TopLeft), m_Kind(kind), m_GridCols(gridCols > 0 ? gridCols : 1) {}

    HUDElement *AddChild(const char *text = "");
    HUDElement *AddChildNamed(const std::string &name, const char *text = "");
    HUDElement *FindChild(const std::string &name);
    bool RemoveChild(const std::string &name);
    HUDContainer *AddContainerChild(HUDLayoutKind kind, const std::string &name, int gridCols = 1);
    HUDLayoutKind GetKind() const { return m_Kind; }
    std::unique_ptr<HUDElement> StealChild(const std::string &name);
    void InsertChild(std::unique_ptr<HUDElement> up, const std::string &name);
    HUDElement *AddAnsiChild(const char *ansiText) { return AddChild(ansiText); }
    void SetSpacing(float px) { float v = px >= 0.0f ? px : 0.0f; if (m_SpacingPx == v) return; m_SpacingPx = v; }
    void SetGridCols(int cols) { int v = cols > 0 ? cols : 1; if (m_GridCols == v) return; m_GridCols = v; }

    void Draw(ImDrawList *drawList, const ImVec2 &viewportSize) override;

    std::vector<std::unique_ptr<HUDElement>> &Children() { return m_Children; }
    const std::vector<std::unique_ptr<HUDElement>> &Children() const { return m_Children; }
    void SetAlignX(AlignX ax) { if (m_AlignX == ax) return; m_AlignX = ax; }
    void SetAlignY(AlignY ay) { if (m_AlignY == ay) return; m_AlignY = ay; }
    void SetCellAlignX(AlignX ax) { if (m_CellAlignX == ax) return; m_CellAlignX = ax; }
    void SetCellAlignY(AlignY ay) { if (m_CellAlignY == ay) return; m_CellAlignY = ay; }
    
    float GetSpacing() const { return m_SpacingPx; }
    int GetGridCols() const { return m_GridCols; }
    AlignX GetAlignX() const { return m_AlignX; }
    AlignY GetAlignY() const { return m_AlignY; }
    AlignX GetCellAlignX() const { return m_CellAlignX; }
    AlignY GetCellAlignY() const { return m_CellAlignY; }

private:
    HUDLayoutKind m_Kind;
    int m_GridCols = 1;
    float m_SpacingPx = 4.0f;
    AlignX m_AlignX = AlignX::Left;    // for Vertical: horizontal alignment; for Grid: default cell X
    AlignY m_AlignY = AlignY::Top;     // for Horizontal: vertical alignment; for Grid: default cell Y
    AlignX m_CellAlignX = AlignX::Left; // grid-specific overrides
    AlignY m_CellAlignY = AlignY::Top;
    std::vector<std::unique_ptr<HUDElement>> m_Children;
    std::unordered_map<std::string, HUDElement*> m_NamedChildren;
public:
    const std::unordered_map<std::string, HUDElement*>& NamedChildren() const { return m_NamedChildren; }
};

class SRTimer {
public:
    SRTimer();
    ~SRTimer() = default;

    void Reset();
    void Start();
    void Pause();

    void Update(float deltaTime);

    float GetTime() const;
    const char *GetFormattedTime() const;

    bool IsRunning() const;

private:
    float m_Time;                     // Time in milliseconds
    bool m_Running;                   // Is timer running?
    mutable char m_FormattedTime[16] = {}; // Formatted time string

    void UpdateFormattedTime() const;
};

class FpsCounter {
public:
    explicit FpsCounter(uint32_t sampleCount = 60);
    ~FpsCounter() = default;

    void Update(float frameTime);

    float GetAverageFps() const;
    const char *GetFormattedFps() const;

    void SetUpdateFrequency(uint32_t frames);
    uint32_t GetUpdateFrequency() const;

private:
    std::array<float, 120> m_FrameTimes = {}; // Circular buffer of frame times
    uint32_t m_SampleCount;                   // Number of samples to average
    uint32_t m_CurrentIndex;                  // Current index in circular buffer
    uint32_t m_FrameCounter;                  // Frame counter for update frequency
    uint32_t m_UpdateFrequency;               // How often to update the average
    float m_CurrentAverageFps;                // Current average FPS
    mutable char m_FormattedFps[16] = {};     // Formatted FPS string

    void RecalculateAverage();
};

class HUD : public Bui::Window {
public:
    HUD();
    ~HUD() override;

    ImGuiWindowFlags GetFlags() override;
    void OnPreBegin() override;
    void OnDraw() override;

    void OnProcess();

    void ShowTitle(bool show = true);

    void ShowFPS(bool show = true);
    void SetFPSUpdateFrequency(uint32_t frames);
    void SetFPSPosition(AnchorPoint anchor, float offsetX = 0.0f, float offsetY = 0.0f);

    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    void ShowSRTimer(bool show = true);
    void SetSRTimerPosition(AnchorPoint anchor, float offsetX = 0.0f, float offsetY = 0.0f);
    float GetSRTime() const;

    HUDElement *AddElement(const char *text, AnchorPoint anchor = AnchorPoint::TopLeft);
    HUDElement *AddAnsiElement(const char *ansiText, AnchorPoint anchor = AnchorPoint::TopLeft);
    HUDContainer *AddVStack(AnchorPoint anchor = AnchorPoint::TopLeft);
    HUDContainer *AddHStack(AnchorPoint anchor = AnchorPoint::TopLeft);
    HUDContainer *AddGrid(int cols, AnchorPoint anchor = AnchorPoint::TopLeft);

    bool RemoveElement(HUDElement *element);

    // Named elements for TUI-like customization
    HUDElement *GetOrCreate(const std::string &id);
    HUDElement *Find(const std::string &id);
    bool Remove(const std::string &id);
    std::vector<std::string> ListIds() const;
    void Register(const std::string &id, HUDElement *e) { m_Named[id] = e; }
    HUDElement *FindByPath(const std::string &path);
    HUDElement *GetOrCreateChild(const std::string &containerId, const std::string &childId);
    HUDContainer *EnsureContainerPath(const std::string &path, HUDLayoutKind defaultKindForNew = HUDLayoutKind::Vertical);
    std::unique_ptr<HUDElement> StealByPath(const std::string &path);
    void AttachToContainer(HUDContainer *dest, std::unique_ptr<HUDElement> up, const std::string &childName);
    void AttachToRoot(std::unique_ptr<HUDElement> up, const std::string &name);

    // Pages and variables
    void SetActivePage(const std::string &name) { m_ActivePage = name; }
    const std::string &GetActivePage() const { return m_ActivePage; }
    using VarProvider = std::function<bool(const std::string &, std::string &)>;
    void SetVarProvider(VarProvider provider) { m_VarProvider = std::move(provider); }

    // Policy hook for auto-created container kinds
    using CreatePolicy = std::function<HUDLayoutKind(const std::string &parentPath, const std::string &segment, int depth)>;
    void SetAutoCreatePolicy(CreatePolicy policy) { m_CreatePolicy = std::move(policy); }
    void SetAutoCreatePolicyMode(const std::string &mode);
    const std::string &GetAutoCreatePolicyMode() const { return m_CreatePolicyMode; }
    std::string GetAutoCreatePolicyModeEffective() const;

    // Outline dump (root if path empty, else subtree)
    std::string DumpOutline(const std::string &path = std::string()) const;
    const std::string &GetPageDefaultContainer(const std::string &page) const;
    void SetPageDefaultContainer(const std::string &page, const std::string &path);
    void ClearPageDefaultContainer(const std::string &page);
    std::vector<std::pair<std::string, std::string>> ListPageDefaultContainers() const;

    // Global style controls
    void SetStyle(const HUDStyle &style) { m_Style = style; }
    const HUDStyle &GetStyle() const { return m_Style; }

    // Config save/load methods
    bool LoadConfig(const std::wstring &path);
    bool LoadConfigIni(const std::wstring &path);
    bool SaveConfigIni(const std::wstring &path) const;
    bool SaveSampleIfMissing() const;
    bool SaveSubtreeIni(const std::wstring &path, const std::string &rootPath) const;

    // Deep clone element or container
    std::unique_ptr<HUDElement> CloneElement(const HUDElement *src) const;

private:
    std::unique_ptr<HUDElement> m_TitleElement;
    std::unique_ptr<HUDElement> m_FPSElement;
    std::unique_ptr<HUDElement> m_SRTimerLabelElement;
    std::unique_ptr<HUDElement> m_SRTimerValueElement;
    std::unique_ptr<HUDElement> m_CheatModeElement;
    std::vector<std::unique_ptr<HUDElement>> m_CustomElements;
    std::unordered_map<std::string, HUDElement*> m_Named;
    std::string m_ActivePage;
    VarProvider m_VarProvider;
    bool ResolveVar(const std::string &name, std::string &out) const;
    CreatePolicy m_CreatePolicy;
    std::unordered_map<std::string, std::string> m_PageDefaultContainers;
    std::string m_CreatePolicyMode; // builtin|vertical|horizontal|grid|clear

    FpsCounter m_FPSCounter;
    SRTimer m_SRTimer;

    void SetupDefaultElements();
    void UpdateTimerDisplay();
    void ApplyStyle(HUDElement *e);
    HUDStyle m_Style;
};

#endif // BML_HUD_H
