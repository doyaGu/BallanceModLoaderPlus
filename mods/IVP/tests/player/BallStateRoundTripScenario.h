#ifndef BML_TESTS_PLAYER_BALLSTATEROUNDTRIPSCENARIO_H
#define BML_TESTS_PLAYER_BALLSTATEROUNDTRIPSCENARIO_H

#include <cstdint>
#include <memory>
#include <string>

class CK3dEntity;

struct BallStateRoundTripResult {
    bool Passed = false;
    bool MappingPassed = false;
    bool MovingStateCaptured = false;
    bool DivergenceObserved = false;
    bool PositionRestored = false;
    bool RotationRestored = false;
    bool LinearVelocityRestored = false;
    bool AngularVelocityRestored = false;
    bool SimulationContinued = false;
    float CapturedSpeed = 0.0f;
    float DivergedDistance = 0.0f;
    float PositionError = 0.0f;
    float RotationError = 0.0f;
    float LinearVelocityError = 0.0f;
    float AngularVelocityError = 0.0f;
    float ContinuedDistance = 0.0f;
    std::string BallName = "<missing>";
    std::string Detail = "not-run";
};

// Advanced example kept separate from the ordinary gameplay test. It captures
// the active ball while rolling, lets it move away, restores transform and
// velocity, then observes the retail physics simulation continue.
class BallStateRoundTripScenario final {
public:
    explicit BallStateRoundTripScenario(CK3dEntity *ball);
    ~BallStateRoundTripScenario();

    BallStateRoundTripScenario(const BallStateRoundTripScenario &) = delete;
    BallStateRoundTripScenario &operator=(
        const BallStateRoundTripScenario &) = delete;

    void Advance();
    [[nodiscard]] std::uint32_t RequestedInputMask() const;
    [[nodiscard]] bool Done() const;
    [[nodiscard]] BallStateRoundTripResult Result() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_TESTS_PLAYER_BALLSTATEROUNDTRIPSCENARIO_H
