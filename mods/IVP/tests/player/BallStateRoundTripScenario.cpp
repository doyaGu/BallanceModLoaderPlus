#include "BallStateRoundTripScenario.h"

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
constexpr float kCaptureSpeed = 0.5f;
constexpr float kCaptureTravel = 0.15f;
constexpr float kDivergenceTravel = 1.0f;
constexpr float kPositionTolerance = 0.35f;
constexpr float kRotationTolerance = 0.05f;
constexpr float kVelocityTolerance = 0.01f;
constexpr float kContinuedTravel = 0.10f;
constexpr auto kSettleTime = std::chrono::milliseconds(500);
constexpr auto kCaptureTimeout = std::chrono::seconds(5);
constexpr auto kDivergenceTimeout = std::chrono::seconds(5);
constexpr auto kRestoreObservationTime = std::chrono::milliseconds(100);
constexpr auto kContinueTimeout = std::chrono::seconds(3);

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

float Distance(const IVP_U_Float_Point &left,
               const IVP_U_Float_Point &right) {
    const float x = left.k[0] - right.k[0];
    const float y = left.k[1] - right.k[1];
    const float z = left.k[2] - right.k[2];
    return std::sqrt(x * x + y * y + z * z);
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

float RotationDifference(const IVP_U_Matrix &left,
                         const IVP_U_Matrix &right) {
    float difference = 0.0f;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            difference = std::max(
                difference,
                static_cast<float>(std::fabs(
                    left.get_elem(row, column) -
                    right.get_elem(row, column))));
        }
    }
    return difference;
}

} // namespace

class BallStateRoundTripScenario::Impl final {
public:
    explicit Impl(CK3dEntity *ball)
        : Ball(ball), StageStartedAt(std::chrono::steady_clock::now()) {}

    void Advance() {
        if (Finished || !ResolveStableMapping())
            return;
        switch (CurrentStage) {
        case Stage::Settling: AdvanceSettling(); break;
        case Stage::Accelerating: AdvanceAccelerating(); break;
        case Stage::Diverging: AdvanceDiverging(); break;
        case Stage::ObservingRestore: AdvanceRestoreObservation(); break;
        case Stage::ObservingContinuation: AdvanceContinuation(); break;
        case Stage::Finished: break;
        }
    }

    std::uint32_t RequestedInputMask() const {
        return CurrentStage == Stage::Accelerating ||
                       CurrentStage == Stage::Diverging
                   ? BML::PlayerTest::GameplayKeyUp
                   : BML::PlayerTest::GameplayKeyNone;
    }

    bool Done() const { return Finished; }
    BallStateRoundTripResult Result() const { return ResultValue; }

private:
    enum class Stage {
        Settling, Accelerating, Diverging, ObservingRestore,
        ObservingContinuation, Finished,
    };

    bool ResolveStableMapping() {
        BML::IVP::ApiInfo info{};
        IVP_Real_Object *realObject = Ball ? BML::IVP::RealObject(Ball) : nullptr;
        IVP_Core *core = Ball ? BML::IVP::Core(Ball) : nullptr;
        BML_IVP_PhysicsObject *physicsObject =
            Ball ? BML::IVP::PhysicsObject(Ball) : nullptr;
        const bool mapped =
            BML::IVP::ReadApiInfo(info) == BML_OK &&
            std::strcmp(info.ImageSha256, kRetailSha256) == 0 &&
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
        if (Ball->GetName()) ResultValue.BallName = Ball->GetName();
        return true;
    }

    void AdvanceSettling() {
        if (Elapsed() < kSettleTime) return;
        Ball->GetPosition(&AccelerationStart);
        SetStage(Stage::Accelerating);
    }

    void AdvanceAccelerating() {
        VxVector position;
        Ball->GetPosition(&position);
        const IVP_U_Float_Point linear = LinearVelocity(*Core);
        if (Length(linear) >= kCaptureSpeed &&
            Distance(position, AccelerationStart) >= kCaptureTravel) {
            CapturedCkPosition = position;
            Ball->GetQuaternion(&CapturedCkRotation);
            RealObject->get_m_world_f_object_AT(&CapturedObjectMatrix);
            CapturedCoreMatrix = *Core->get_m_world_f_core_PSI();
            CapturedLinearVelocity = linear;
            CapturedAngularVelocity = AngularVelocity(*Core);
            ResultValue.CapturedSpeed = Length(linear);
            ResultValue.MovingStateCaptured = true;
            SetStage(Stage::Diverging);
            return;
        }
        if (Elapsed() >= kCaptureTimeout)
            Finish(false, "normal-input-produced-no-moving-state");
    }

    void AdvanceDiverging() {
        VxVector position;
        Ball->GetPosition(&position);
        ResultValue.DivergedDistance = Distance(position, CapturedCkPosition);
        if (ResultValue.DivergedDistance >= kDivergenceTravel) {
            ResultValue.DivergenceObserved = true;
            Restore();
            SetStage(Stage::ObservingRestore);
            return;
        }
        if (Elapsed() >= kDivergenceTimeout)
            Finish(false, "player-ball-did-not-diverge");
    }

    void Restore() {
        const IVP_U_Matrix current = *Core->get_m_world_f_core_PSI();
        IVP_U_Matrix relative;
        current.mimult4(&CapturedCoreMatrix, &relative);
        Core->transform_PSI_matrizes_core(&relative);
        Ball->SetPosition(&CapturedCkPosition);
        Ball->SetQuaternion(&CapturedCkRotation);

        Core->speed.set_to_zero();
        Core->speed_change.set(&CapturedLinearVelocity);
        Core->rot_speed.set_to_zero();
        Core->rot_speed_change.set(&CapturedAngularVelocity);
        RealObject->ensure_in_simulation();

        ResultValue.LinearVelocityError =
            Distance(LinearVelocity(*Core), CapturedLinearVelocity);
        ResultValue.AngularVelocityError =
            Distance(AngularVelocity(*Core), CapturedAngularVelocity);
        ResultValue.LinearVelocityRestored =
            ResultValue.LinearVelocityError <= kVelocityTolerance;
        ResultValue.AngularVelocityRestored =
            ResultValue.AngularVelocityError <= kVelocityTolerance;

        IVP_U_Matrix restored;
        RealObject->get_m_world_f_object_AT(&restored);
        const IVP_U_Point *position = restored.get_position();
        const VxVector ivpPosition(static_cast<float>(position->k[0]),
                                   static_cast<float>(position->k[1]),
                                   static_cast<float>(position->k[2]));
        ResultValue.PositionError = Distance(ivpPosition, CapturedCkPosition);
        ResultValue.RotationError =
            RotationDifference(restored, CapturedObjectMatrix);
        ResultValue.PositionRestored =
            ResultValue.PositionError <= kPositionTolerance;
        ResultValue.RotationRestored =
            ResultValue.RotationError <= kRotationTolerance;
    }

    void AdvanceRestoreObservation() {
        if (Elapsed() < kRestoreObservationTime) return;
        if (!ResultValue.PositionRestored || !ResultValue.RotationRestored ||
            !ResultValue.LinearVelocityRestored ||
            !ResultValue.AngularVelocityRestored) {
            Finish(false, "restored-state-mismatch");
            return;
        }
        Ball->GetPosition(&ContinuationStart);
        SetStage(Stage::ObservingContinuation);
    }

    void AdvanceContinuation() {
        VxVector position;
        Ball->GetPosition(&position);
        ResultValue.ContinuedDistance = Distance(position, ContinuationStart);
        if (ResultValue.ContinuedDistance >= kContinuedTravel) {
            ResultValue.SimulationContinued = true;
            Finish(true, "complete");
            return;
        }
        if (Elapsed() >= kContinueTimeout)
            Finish(false, "restored-state-did-not-continue");
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
    VxVector CapturedCkPosition;
    VxQuaternion CapturedCkRotation;
    VxVector ContinuationStart;
    IVP_U_Matrix CapturedObjectMatrix;
    IVP_U_Matrix CapturedCoreMatrix;
    IVP_U_Float_Point CapturedLinearVelocity;
    IVP_U_Float_Point CapturedAngularVelocity;
    bool Finished = false;
    BallStateRoundTripResult ResultValue;
};

BallStateRoundTripScenario::BallStateRoundTripScenario(CK3dEntity *ball)
    : m_Impl(std::make_unique<Impl>(ball)) {}
BallStateRoundTripScenario::~BallStateRoundTripScenario() = default;
void BallStateRoundTripScenario::Advance() { m_Impl->Advance(); }
std::uint32_t BallStateRoundTripScenario::RequestedInputMask() const {
    return m_Impl->RequestedInputMask();
}
bool BallStateRoundTripScenario::Done() const { return m_Impl->Done(); }
BallStateRoundTripResult BallStateRoundTripScenario::Result() const {
    return m_Impl->Result();
}
