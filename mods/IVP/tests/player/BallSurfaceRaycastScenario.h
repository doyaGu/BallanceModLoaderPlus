#ifndef BML_TESTS_PLAYER_BALLSURFACERAYCASTSCENARIO_H
#define BML_TESTS_PLAYER_BALLSURFACERAYCASTSCENARIO_H

#include <cstdint>
#include <memory>
#include <string>

class CK3dEntity;

struct BallSurfaceRaycastResult {
    bool Passed = false;
    bool MappingPassed = false;
    bool RetailBallObserved = false;
    bool FirstHitPassed = false;
    bool SecondHitPassed = false;
    bool MovingSurfaceTracked = false;
    bool StaticSurfacePassed = false;
    bool DirectLedgePassed = false;
    float BallRadius = 0.0f;
    float FirstHitDistance = 0.0f;
    float SecondHitDistance = 0.0f;
    float CenterTravel = 0.0f;
    float StaticSurfaceDistance = 0.0f;
    float DirectLedgeDistance = 0.0f;
    std::string BallName = "<missing>";
    std::string Detail = "not-run";
};

// A gameplay interaction case: cast against the real moving player ball,
// then repeat after it has travelled far enough to require a new world-space
// ray. This exercises the public ray solver against retail ball geometry.
class BallSurfaceRaycastScenario final {
public:
    explicit BallSurfaceRaycastScenario(CK3dEntity *ball);
    ~BallSurfaceRaycastScenario();

    BallSurfaceRaycastScenario(const BallSurfaceRaycastScenario &) = delete;
    BallSurfaceRaycastScenario &operator=(
        const BallSurfaceRaycastScenario &) = delete;

    void Advance();
    [[nodiscard]] std::uint32_t RequestedInputMask() const;
    [[nodiscard]] bool Done() const;
    [[nodiscard]] BallSurfaceRaycastResult Result() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_TESTS_PLAYER_BALLSURFACERAYCASTSCENARIO_H
