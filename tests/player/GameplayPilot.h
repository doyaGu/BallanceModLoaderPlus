#pragma once

#include "GameplayInputDriver.h"

#include "VxMath.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BML::PlayerTest {

struct GameplayWaypoint {
    const char *Name = nullptr;
    VxVector Position;
    float Radius = 3.0f;
};

struct GameplayPilotSample {
    VxVector Position;
    VxVector LinearVelocity;
    VxVector CameraRight;
    VxVector CameraForward;
    std::chrono::steady_clock::time_point Time;
    bool HasLinearVelocity = false;
};

struct GameplayPilotDecision {
    std::uint32_t Keys = GameplayKeyNone;
    std::size_t Waypoint = 0;
    const char *WaypointName = nullptr;
    float Distance = 0.0f;
    float Speed = 0.0f;
    float EffectiveAcceleration = 0.0f;
    bool Advanced = false;
    bool Arrived = false;
    bool Stalled = false;
};

// A level-independent feedback controller. A route supplies world-space goals;
// the controller estimates velocity from successive samples and expresses its
// correction in the same camera-relative axes consumed by Ball Navigation.
class GameplayPilot final {
public:
    explicit GameplayPilot(std::vector<GameplayWaypoint> route);

    void Reset(const GameplayPilotSample &sample);
    GameplayPilotDecision Step(const GameplayPilotSample &sample);

    [[nodiscard]] bool Empty() const noexcept { return m_Route.empty(); }
    [[nodiscard]] std::size_t Waypoint() const noexcept { return m_Waypoint; }

private:
    static VxVector Horizontal(VxVector value);
    static float HorizontalLength(const VxVector &value);
    static VxVector NormalizedHorizontal(VxVector value);

    std::vector<GameplayWaypoint> m_Route;
    std::size_t m_Waypoint = 0;
    VxVector m_LastPosition;
    VxVector m_Velocity;
    VxVector m_LastControlDirection;
    VxVector m_StallOrigin;
    std::chrono::steady_clock::time_point m_LastTime{};
    std::chrono::steady_clock::time_point m_StallSince{};
    float m_EffectiveAcceleration = 18.0f;
    std::uint32_t m_LastKeys = GameplayKeyNone;
    bool m_HasSample = false;
};

} // namespace BML::PlayerTest
