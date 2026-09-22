#ifndef BML_TESTS_PLAYER_BALLSPEEDGOVERNORSCENARIO_H
#define BML_TESTS_PLAYER_BALLSPEEDGOVERNORSCENARIO_H

#include <cstdint>
#include <memory>
#include <string>

class CK3dEntity;

struct BallSpeedGovernorResult {
    bool Passed = false;
    bool MappingPassed = false;
    bool RollingStateObserved = false;
    bool LimitApplied = false;
    bool LimitRespected = false;
    bool AngularVelocityCoupled = false;
    bool ControllableUnderLimit = false;
    bool AcceleratedAfterRelease = false;
    float SpeedCap = 0.0f;
    float EntrySpeed = 0.0f;
    float MaximumLimitedSpeed = 0.0f;
    float LimitedTravel = 0.0f;
    float ReleasedSpeed = 0.0f;
    float ReleasedTravel = 0.0f;
    std::string BallName = "<missing>";
    std::string Detail = "not-run";
};

// A real gameplay-mod scenario: apply a speed governor to Ballance's active
// player ball while normal Ball Navigation input remains in control, then
// remove the governor and observe the original game accelerate the ball again.
class BallSpeedGovernorScenario final {
public:
    explicit BallSpeedGovernorScenario(CK3dEntity *ball);
    ~BallSpeedGovernorScenario();

    BallSpeedGovernorScenario(const BallSpeedGovernorScenario &) = delete;
    BallSpeedGovernorScenario &operator=(
        const BallSpeedGovernorScenario &) = delete;

    void Advance();
    [[nodiscard]] std::uint32_t RequestedInputMask() const;
    [[nodiscard]] bool Done() const;
    [[nodiscard]] BallSpeedGovernorResult Result() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_TESTS_PLAYER_BALLSPEEDGOVERNORSCENARIO_H
