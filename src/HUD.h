#ifndef BML_HUD_H
#define BML_HUD_H

#include <cstdint>
#include <array>
#include <vector>
#include <memory>

#include "BML/Bui.h"

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
    void SetColor(ImU32 color);
    void SetScale(float scale);

    virtual void Draw(ImDrawList *drawList, const ImVec2 &viewportSize);

    ImVec2 CalculatePosition(const ImVec2 &textSize, const ImVec2 &viewportSize) const;

protected:
    std::string m_Text;
    AnchorPoint m_Anchor;
    ImVec2 m_Offset;
    ImU32 m_Color;
    float m_Scale;
    bool m_Visible;
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
    mutable char m_FormattedTime[16]; // Formatted time string

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
    void OnBegin() override;
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

    bool RemoveElement(HUDElement *element);

private:
    std::unique_ptr<HUDElement> m_TitleElement;
    std::unique_ptr<HUDElement> m_FPSElement;
    std::unique_ptr<HUDElement> m_SRTimerLabelElement;
    std::unique_ptr<HUDElement> m_SRTimerValueElement;
    std::unique_ptr<HUDElement> m_CheatModeElement;
    std::vector<std::unique_ptr<HUDElement>> m_CustomElements;

    FpsCounter m_FPSCounter;
    SRTimer m_SRTimer;

    void SetupDefaultElements();
    void UpdateTimerDisplay();
};

#endif // BML_HUD_H
