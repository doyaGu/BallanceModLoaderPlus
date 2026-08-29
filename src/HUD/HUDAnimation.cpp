#include "HUD/HUD.h"

#include <algorithm>
#include <cmath>

namespace {
    float Ease(float t, EasingType type) {
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

    unsigned int InterpolateChannel(ImU32 start, ImU32 end, int shift, float t) {
        const float startChannel = static_cast<float>((start >> shift) & 0xFFu);
        const float endChannel = static_cast<float>((end >> shift) & 0xFFu);
        return static_cast<unsigned int>(std::lround(startChannel + (endChannel - startChannel) * t));
    }
}

float HUDAnimation::GetCurrentValue() const {
    if (duration <= 0.0f)
        return endValue;
    const float t = std::clamp(elapsed / duration, 0.0f, 1.0f);
    const float easedT = Ease(t, easing);
    return startValue + (endValue - startValue) * easedT;
}

ImU32 HUDAnimation::GetCurrentColor() const {
    if (duration <= 0.0f || elapsed >= duration)
        return endColor;
    if (elapsed <= 0.0f)
        return startColor;

    const float t = Ease(std::clamp(elapsed / duration, 0.0f, 1.0f), easing);
    return (InterpolateChannel(startColor, endColor, IM_COL32_R_SHIFT, t) << IM_COL32_R_SHIFT) |
           (InterpolateChannel(startColor, endColor, IM_COL32_G_SHIFT, t) << IM_COL32_G_SHIFT) |
           (InterpolateChannel(startColor, endColor, IM_COL32_B_SHIFT, t) << IM_COL32_B_SHIFT) |
           (InterpolateChannel(startColor, endColor, IM_COL32_A_SHIFT, t) << IM_COL32_A_SHIFT);
}

void HUDAnimation::Update(float deltaTime) {
    if (finished)
        return;
    elapsed += deltaTime;
    if (elapsed >= duration) {
        elapsed = duration;
        finished = true;
    }
}
