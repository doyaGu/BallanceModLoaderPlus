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

    // Set the display text
    void SetText(const char *text);

    // Get the display text
    const char *GetText() const;

    // Set visibility
    void SetVisible(bool visible);

    // Check if visible
    bool IsVisible() const;

    // Set anchor point
    void SetAnchor(AnchorPoint anchor);

    // Set offset from anchor (in screen-space coordinates: 0.0-1.0)
    void SetOffset(float x, float y);

    // Set color (RGBA format)
    void SetColor(ImU32 color);

    // Set scale
    void SetScale(float scale);

    // Draw the element
    virtual void Draw(ImDrawList *drawList, const ImVec2 &viewportSize);

    // Calculate position based on anchor and viewport
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

    // Reset timer to 0
    void Reset();

    // Start or resume timer
    void Start();

    // Pause timer
    void Pause();

    // Update timer with given delta time (in milliseconds)
    void Update(float deltaTime);

    // Get current time in milliseconds
    float GetTime() const;

    // Get formatted time string (HH:MM:SS.MMM)
    const char *GetFormattedTime() const;

    // Check if timer is running
    bool IsRunning() const;

private:
    float m_Time;                     // Time in milliseconds
    bool m_Running;                   // Is timer running?
    mutable char m_FormattedTime[16]; // Formatted time string

    // Update the formatted time string
    void UpdateFormattedTime() const;
};

class FpsCounter {
public:
    explicit FpsCounter(uint32_t sampleCount = 60);
    ~FpsCounter() = default;

    // Update with a new frame time (in milliseconds)
    void Update(float frameTime);

    // Get the current average FPS
    float GetAverageFps() const;

    // Get formatted FPS string
    const char *GetFormattedFps() const;

    // Set the update frequency (how often the average is recalculated)
    void SetUpdateFrequency(uint32_t frames);

    // Get the current update frequency
    uint32_t GetUpdateFrequency() const;

private:
    std::array<float, 120> m_FrameTimes = {}; // Circular buffer of frame times
    uint32_t m_SampleCount;                   // Number of samples to average
    uint32_t m_CurrentIndex;                  // Current index in circular buffer
    uint32_t m_FrameCounter;                  // Frame counter for update frequency
    uint32_t m_UpdateFrequency;               // How often to update the average
    float m_CurrentAverageFps;                // Current average FPS
    mutable char m_FormattedFps[16] = {};     // Formatted FPS string

    // Recalculate the average FPS
    void RecalculateAverage();
};

class HUD : public Bui::Window {
public:
    HUD();
    ~HUD() override;

    ImGuiWindowFlags GetFlags() override;
    void OnBegin() override;
    void OnDraw() override;

    // Process HUD elements
    void OnProcess();

    // Title display options
    void ShowTitle(bool show = true);

    // FPS display options
    void ShowFPS(bool show = true);
    void SetFPSUpdateFrequency(uint32_t frames);
    void SetFPSPosition(AnchorPoint anchor, float offsetX = 0.0f, float offsetY = 0.0f);

    // SR Timer options
    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    void ShowSRTimer(bool show = true);
    void SetSRTimerPosition(AnchorPoint anchor, float offsetX = 0.0f, float offsetY = 0.0f);
    float GetSRTime() const;

    // Add a custom UI element
    HUDElement *AddElement(const char *text, AnchorPoint anchor = AnchorPoint::TopLeft);

    // Remove a UI element by pointer
    bool RemoveElement(HUDElement *element);

private:
    // UI components
    std::unique_ptr<HUDElement> m_TitleElement;
    std::unique_ptr<HUDElement> m_FPSElement;
    std::unique_ptr<HUDElement> m_SRTimerLabelElement;
    std::unique_ptr<HUDElement> m_SRTimerValueElement;
    std::unique_ptr<HUDElement> m_CheatModeElement;
    std::vector<std::unique_ptr<HUDElement>> m_CustomElements;

    // Performance trackers
    FpsCounter m_FPSCounter;
    SRTimer m_SRTimer;

    // Helper method to set up predefined UI elements
    void SetupDefaultElements();

    // Helper method to update timer-related displays
    void UpdateTimerDisplay();
};

#endif // BML_HUD_H
