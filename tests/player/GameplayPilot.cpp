#include "GameplayPilot.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace BML::PlayerTest {
namespace {

constexpr float kMinimumSampleSeconds = 0.001f;
constexpr float kMaximumSampleSeconds = 0.25f;
constexpr float kVelocitySmoothing = 0.35f;
constexpr float kPredictionSeconds = 0.8f;
constexpr float kPredictionStep = 0.05f;
constexpr float kControlInterval = 0.15f;
constexpr float kLinearDrag = 0.35f;
constexpr float kTurnSpeedWeight = 0.08f;
constexpr float kSwitchCost = 0.12f;
constexpr float kStallDistance = 0.75f;
constexpr auto kStallTime = std::chrono::seconds(4);

float DotHorizontal(const VxVector &left, const VxVector &right) {
    return left.x * right.x + left.z * right.z;
}

VxVector DirectionForKeys(std::uint32_t keys, const VxVector &right,
                          const VxVector &forward) {
    VxVector direction(0.0f, 0.0f, 0.0f);
    if ((keys & GameplayKeyRight) != 0)
        direction += right;
    if ((keys & GameplayKeyLeft) != 0)
        direction -= right;
    if ((keys & GameplayKeyUp) != 0)
        direction += forward;
    if ((keys & GameplayKeyDown) != 0)
        direction -= forward;
    const float length = std::sqrt(direction.x * direction.x +
                                   direction.z * direction.z);
    return length > 0.0001f ? direction / length : direction;
}

float PredictCost(const VxVector &position, const VxVector &velocity,
                  const VxVector &target, const VxVector &direction,
                  float acceleration, bool changesInput) {
    VxVector predictedPosition = position;
    VxVector predictedVelocity = velocity;
    for (float elapsed = 0.0f; elapsed < kPredictionSeconds;
         elapsed += kPredictionStep) {
        if (elapsed < kControlInterval)
            predictedVelocity += direction * (acceleration * kPredictionStep);
        predictedVelocity *= std::max(0.0f, 1.0f - kLinearDrag * kPredictionStep);
        predictedPosition += predictedVelocity * kPredictionStep;
    }

    const VxVector error = target - predictedPosition;
    const float distanceSquared = error.x * error.x + error.z * error.z;
    const float speedSquared = predictedVelocity.x * predictedVelocity.x +
                               predictedVelocity.z * predictedVelocity.z;
    return distanceSquared + speedSquared * kTurnSpeedWeight +
           (changesInput ? kSwitchCost : 0.0f);
}

} // namespace

GameplayPilot::GameplayPilot(std::vector<GameplayWaypoint> route)
    : m_Route(std::move(route)) {}

void GameplayPilot::Reset(const GameplayPilotSample &sample) {
    m_Waypoint = 0;
    m_LastPosition = sample.Position;
    m_Velocity = sample.HasLinearVelocity
        ? Horizontal(sample.LinearVelocity)
        : VxVector(0.0f, 0.0f, 0.0f);
    m_LastControlDirection = VxVector(0.0f, 0.0f, 0.0f);
    m_StallOrigin = sample.Position;
    m_LastTime = sample.Time;
    m_StallSince = sample.Time;
    m_EffectiveAcceleration = 18.0f;
    m_LastKeys = GameplayKeyNone;
    m_HasSample = true;
}

GameplayPilotDecision GameplayPilot::Step(const GameplayPilotSample &sample) {
    GameplayPilotDecision decision;
    if (m_Route.empty()) {
        decision.Arrived = true;
        return decision;
    }
    if (!m_HasSample)
        Reset(sample);

    float seconds = std::chrono::duration<float>(sample.Time - m_LastTime).count();
    seconds = std::clamp(seconds, kMinimumSampleSeconds, kMaximumSampleSeconds);
    const VxVector previousVelocity = m_Velocity;
    const VxVector measured = sample.HasLinearVelocity
        ? Horizontal(sample.LinearVelocity)
        : Horizontal(sample.Position - m_LastPosition) / seconds;
    m_Velocity = sample.HasLinearVelocity
        ? measured
        : m_Velocity * (1.0f - kVelocitySmoothing) +
              measured * kVelocitySmoothing;
    if (m_LastKeys != GameplayKeyNone) {
        const VxVector velocityChange = m_Velocity - previousVelocity;
        const float observedAcceleration =
            DotHorizontal(velocityChange, m_LastControlDirection) / seconds;
        if (observedAcceleration >= 2.0f && observedAcceleration <= 60.0f)
            m_EffectiveAcceleration = m_EffectiveAcceleration * 0.95f +
                                      observedAcceleration * 0.05f;
    }
    m_LastPosition = sample.Position;
    m_LastTime = sample.Time;

    while (m_Waypoint < m_Route.size()) {
        const VxVector error = Horizontal(
            m_Route[m_Waypoint].Position - sample.Position);
        if (HorizontalLength(error) > m_Route[m_Waypoint].Radius)
            break;
        ++m_Waypoint;
        decision.Advanced = true;
        m_StallOrigin = sample.Position;
        m_StallSince = sample.Time;
    }
    if (m_Waypoint >= m_Route.size()) {
        decision.Arrived = true;
        decision.Waypoint = m_Route.size();
        decision.Speed = HorizontalLength(m_Velocity);
        return decision;
    }

    const GameplayWaypoint &waypoint = m_Route[m_Waypoint];
    const VxVector error = Horizontal(waypoint.Position - sample.Position);
    const float distance = HorizontalLength(error);
    const VxVector right = NormalizedHorizontal(sample.CameraRight);
    const VxVector forward = NormalizedHorizontal(sample.CameraForward);
    constexpr std::uint32_t candidates[] = {
        GameplayKeyNone,
        GameplayKeyLeft,
        GameplayKeyRight,
        GameplayKeyUp,
        GameplayKeyDown,
        GameplayKeyLeft | GameplayKeyUp,
        GameplayKeyLeft | GameplayKeyDown,
        GameplayKeyRight | GameplayKeyUp,
        GameplayKeyRight | GameplayKeyDown,
    };
    float bestCost = std::numeric_limits<float>::max();
    VxVector bestDirection(0.0f, 0.0f, 0.0f);
    for (const std::uint32_t keys : candidates) {
        const VxVector direction = DirectionForKeys(keys, right, forward);
        const float cost = PredictCost(
            sample.Position, m_Velocity, waypoint.Position, direction,
            m_EffectiveAcceleration, keys != m_LastKeys);
        if (cost < bestCost) {
            bestCost = cost;
            decision.Keys = keys;
            bestDirection = direction;
        }
    }
    m_LastKeys = decision.Keys;
    m_LastControlDirection = bestDirection;

    if (HorizontalLength(sample.Position - m_StallOrigin) >= kStallDistance) {
        m_StallOrigin = sample.Position;
        m_StallSince = sample.Time;
    }

    decision.Waypoint = m_Waypoint;
    decision.WaypointName = waypoint.Name;
    decision.Distance = distance;
    decision.Speed = HorizontalLength(m_Velocity);
    decision.EffectiveAcceleration = m_EffectiveAcceleration;
    decision.Stalled = sample.Time - m_StallSince >= kStallTime;
    return decision;
}

VxVector GameplayPilot::Horizontal(VxVector value) {
    value.y = 0.0f;
    return value;
}

float GameplayPilot::HorizontalLength(const VxVector &value) {
    return std::sqrt(value.x * value.x + value.z * value.z);
}

VxVector GameplayPilot::NormalizedHorizontal(VxVector value) {
    value = Horizontal(value);
    const float length = HorizontalLength(value);
    if (length <= 0.0001f)
        return VxVector(0.0f, 0.0f, 0.0f);
    return value / length;
}

} // namespace BML::PlayerTest
