#ifndef BML_TESTS_PLAYER_BALLCONSTRAINTSCENARIO_H
#define BML_TESTS_PLAYER_BALLCONSTRAINTSCENARIO_H

#include <cstdint>
#include <memory>
#include <string>

class CK3dEntity;

struct BallConstraintResult {
    bool Passed = false;
    bool MappingPassed = false;
    bool ConstraintCreated = false;
    bool EndpointMappingPassed = false;
    bool CoreMembershipPassed = false;
    bool AnchoredUnderInput = false;
    bool TranslationAxesFreed = false;
    bool MovementResumed = false;
    float AnchoredTravel = 0.0f;
    float ReleasedTravel = 0.0f;
    std::string BallName = "<missing>";
    std::string Detail = "not-run";
};

// Create a retail ballsocket between the world and Ballance's active player
// ball, drive against it through normal gameplay input, then free all three
// translation axes and verify that the same ball can move again.
class BallConstraintScenario final {
public:
    explicit BallConstraintScenario(CK3dEntity *ball);
    ~BallConstraintScenario();

    BallConstraintScenario(const BallConstraintScenario &) = delete;
    BallConstraintScenario &operator=(const BallConstraintScenario &) = delete;

    void Advance();
    [[nodiscard]] std::uint32_t RequestedInputMask() const;
    [[nodiscard]] bool Done() const;
    [[nodiscard]] BallConstraintResult Result() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_TESTS_PLAYER_BALLCONSTRAINTSCENARIO_H
