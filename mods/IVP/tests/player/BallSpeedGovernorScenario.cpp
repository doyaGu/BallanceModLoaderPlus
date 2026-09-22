#include "BallSpeedGovernorScenario.h"

#include "GameplayPilot.h"

#include <BML/IVP/IVP.h>

#include <CKAll.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace {

constexpr char kRetailSha256[] =
    "E72E4AFCFA5C33A7D3D27776137F8C997B3C52D89D8A8A4745F1CA21E45893EC";
constexpr float kEntrySpeed = 2.0f;
constexpr float kMinimumAngularSpeed = 0.1f;
constexpr float kEntryTravel = 0.5f;
constexpr float kSpeedCap = 1.25f;
constexpr float kSpeedTolerance = 0.02f;
constexpr float kMinimumLimitedTravel = 0.75f;
constexpr float kReleaseSpeedMargin = 0.5f;
constexpr float kMinimumReleasedTravel = 0.5f;
constexpr auto kSettleTime = std::chrono::milliseconds(500);
constexpr auto kEntryTimeout = std::chrono::seconds(5);
constexpr auto kGovernDuration = std::chrono::milliseconds(1500);
constexpr auto kReleaseTimeout = std::chrono::seconds(4);

float Length(const VxVector &value) {
    return std::sqrt(value.x * value.x + value.y * value.y +
                     value.z * value.z);
}

float Distance(const VxVector &left, const VxVector &right) {
    return Length(VxVector(left.x - right.x, left.y - right.y,
                           left.z - right.z));
}

float Length(const IVP_U_Float_Point &value) {
    return static_cast<float>(std::sqrt(value.quad_length()));
}

IVP_U_Float_Point LinearVelocity(const IVP_Core &core) {
    IVP_U_Float_Point value;
    value.add(&core.speed, &core.speed_change);
    return value;
}

IVP_U_Float_Point AngularVelocity(const IVP_Core &core) {
    IVP_U_Float_Point value;
    value.add(&core.rot_speed, &core.rot_speed_change);
    return value;
}

void SetVelocity(IVP_Core &core,
                 const IVP_U_Float_Point &linear,
                 const IVP_U_Float_Point &angular) {
    // Ballance's adapter observes effective velocity as speed + speed_change.
    // Queue the requested effective velocity for the next PSI and clear the
    // already committed component, matching the retail field semantics.
    core.speed.set_to_zero();
    core.speed_change.set(&linear);
    core.rot_speed.set_to_zero();
    core.rot_speed_change.set(&angular);
}

bool Finite(const VxVector &value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

} // namespace

class BallSpeedGovernorScenario::Impl final {
public:
    explicit Impl(CK3dEntity *ball)
        : Ball(ball), StageStartedAt(std::chrono::steady_clock::now()) {
        ResultValue.SpeedCap = kSpeedCap;
    }

    void Advance() {
        if (Finished)
            return;
        if (!ResolveStableMapping())
            return;

        switch (CurrentStage) {
        case Stage::Settling:
            AdvanceSettling();
            break;
        case Stage::Accelerating:
            AdvanceAccelerating();
            break;
        case Stage::Governing:
            AdvanceGovernor();
            break;
        case Stage::Released:
            AdvanceReleased();
            break;
        case Stage::Finished:
            break;
        }
    }

    std::uint32_t RequestedInputMask() const {
        return CurrentStage == Stage::Settling ||
                       CurrentStage == Stage::Finished
                   ? BML::PlayerTest::GameplayKeyNone
                   : BML::PlayerTest::GameplayKeyUp;
    }

    bool Done() const { return Finished; }
    BallSpeedGovernorResult Result() const { return ResultValue; }

private:
    enum class Stage {
        Settling,
        Accelerating,
        Governing,
        Released,
        Finished,
    };

    bool ResolveStableMapping() {
        if (!Ball) {
            Finish(false, "ball-missing");
            return false;
        }

        BML::IVP::ApiInfo info{};
        IVP_Real_Object *realObject = BML::IVP::RealObject(Ball);
        IVP_Core *core = BML::IVP::Core(Ball);
        BML_IVP_PhysicsObject *physicsObject = BML::IVP::PhysicsObject(Ball);
        const bool mapped =
            BML::IVP::ReadApiInfo(info) == BML_OK &&
            std::strcmp(info.ImageSha256, kRetailSha256) == 0 &&
            (info.Capabilities & BML_IVP_CAP_ORIGINAL_DLL_LOCK) != 0 &&
            realObject && core && physicsObject &&
            physicsObject->RealObject == realObject &&
            realObject->get_core() == core &&
            core->get_environment() == BML::IVP::Environment();
        if (!mapped) {
            Finish(false, "player-ball-ivp-mapping");
            return false;
        }
        if ((RealObject && RealObject != realObject) || (Core && Core != core)) {
            Finish(false, "player-ball-mapping-changed");
            return false;
        }
        RealObject = realObject;
        Core = core;
        ResultValue.MappingPassed = true;
        if (Ball->GetName())
            ResultValue.BallName = Ball->GetName();
        return true;
    }

    void AdvanceSettling() {
        if (Elapsed() < kSettleTime)
            return;
        Ball->GetPosition(&AccelerationStart);
        if (!Finite(AccelerationStart)) {
            Finish(false, "ball-position-not-finite");
            return;
        }
        SetStage(Stage::Accelerating);
    }

    void AdvanceAccelerating() {
        VxVector position;
        Ball->GetPosition(&position);
        const float linearSpeed = Length(LinearVelocity(*Core));
        const float angularSpeed = Length(AngularVelocity(*Core));
        if (linearSpeed >= kEntrySpeed &&
            angularSpeed >= kMinimumAngularSpeed &&
            Distance(position, AccelerationStart) >= kEntryTravel) {
            ResultValue.RollingStateObserved = true;
            ResultValue.EntrySpeed = linearSpeed;
            GovernorStart = position;
            SetStage(Stage::Governing);
            return;
        }
        if (Elapsed() >= kEntryTimeout)
            Finish(false, "normal-input-produced-no-rolling-state");
    }

    void AdvanceGovernor() {
        IVP_U_Float_Point linear = LinearVelocity(*Core);
        IVP_U_Float_Point angular = AngularVelocity(*Core);
        const float speedBeforeLimit = Length(linear);
        if (speedBeforeLimit > kSpeedCap) {
            const float angularBeforeLimit = Length(angular);
            const float factor = kSpeedCap / speedBeforeLimit;
            linear.mult(factor);
            angular.mult(factor);
            SetVelocity(*Core, linear, angular);
            RealObject->ensure_in_simulation();
            ResultValue.LimitApplied = true;
            const float angularAfterLimit = Length(AngularVelocity(*Core));
            ResultValue.AngularVelocityCoupled =
                angularBeforeLimit <= kSpeedTolerance ||
                std::fabs(angularAfterLimit -
                          angularBeforeLimit * factor) <= kSpeedTolerance;
        }

        const float limitedSpeed = Length(LinearVelocity(*Core));
        ResultValue.MaximumLimitedSpeed = std::max(
            ResultValue.MaximumLimitedSpeed, limitedSpeed);
        if (Elapsed() < kGovernDuration)
            return;

        VxVector position;
        Ball->GetPosition(&position);
        ResultValue.LimitedTravel = Distance(position, GovernorStart);
        ResultValue.LimitRespected =
            ResultValue.LimitApplied &&
            ResultValue.MaximumLimitedSpeed <= kSpeedCap + kSpeedTolerance;
        ResultValue.ControllableUnderLimit =
            ResultValue.LimitedTravel >= kMinimumLimitedTravel;
        if (!ResultValue.LimitRespected ||
            !ResultValue.AngularVelocityCoupled ||
            !ResultValue.ControllableUnderLimit) {
            Finish(false, !ResultValue.LimitRespected
                              ? "speed-cap-not-respected"
                              : (!ResultValue.AngularVelocityCoupled
                                     ? "angular-speed-not-coupled"
                                     : "limited-ball-did-not-travel"));
            return;
        }

        ReleaseStart = position;
        SetStage(Stage::Released);
    }

    void AdvanceReleased() {
        VxVector position;
        Ball->GetPosition(&position);
        ResultValue.ReleasedSpeed = Length(LinearVelocity(*Core));
        ResultValue.ReleasedTravel = Distance(position, ReleaseStart);
        if (ResultValue.ReleasedSpeed >= kSpeedCap + kReleaseSpeedMargin &&
            ResultValue.ReleasedTravel >= kMinimumReleasedTravel) {
            ResultValue.AcceleratedAfterRelease = true;
            Finish(true, "complete");
            return;
        }
        if (Elapsed() >= kReleaseTimeout)
            Finish(false, "ball-did-not-accelerate-after-release");
    }

    std::chrono::steady_clock::duration Elapsed() const {
        return std::chrono::steady_clock::now() - StageStartedAt;
    }

    void SetStage(Stage stage) {
        CurrentStage = stage;
        StageStartedAt = std::chrono::steady_clock::now();
    }

    void Finish(bool passed, const char *detail) {
        ResultValue.Passed = passed;
        ResultValue.Detail = detail;
        CurrentStage = Stage::Finished;
        Finished = true;
    }

    CK3dEntity *Ball = nullptr;
    IVP_Real_Object *RealObject = nullptr;
    IVP_Core *Core = nullptr;
    Stage CurrentStage = Stage::Settling;
    std::chrono::steady_clock::time_point StageStartedAt{};
    VxVector AccelerationStart;
    VxVector GovernorStart;
    VxVector ReleaseStart;
    bool Finished = false;
    BallSpeedGovernorResult ResultValue;
};

BallSpeedGovernorScenario::BallSpeedGovernorScenario(CK3dEntity *ball)
    : m_Impl(std::make_unique<Impl>(ball)) {}

BallSpeedGovernorScenario::~BallSpeedGovernorScenario() = default;

void BallSpeedGovernorScenario::Advance() { m_Impl->Advance(); }

std::uint32_t BallSpeedGovernorScenario::RequestedInputMask() const {
    return m_Impl->RequestedInputMask();
}

bool BallSpeedGovernorScenario::Done() const { return m_Impl->Done(); }

BallSpeedGovernorResult BallSpeedGovernorScenario::Result() const {
    return m_Impl->Result();
}
