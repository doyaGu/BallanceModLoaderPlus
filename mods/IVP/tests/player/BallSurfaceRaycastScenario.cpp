#include "BallSurfaceRaycastScenario.h"

#include "GameplayPilot.h"

#include <BML/IVP/IVP.h>

#include <CKAll.h>

#include <chrono>
#include <cmath>
#include <cstring>

namespace {

constexpr char kRetailSha256[] =
    "E72E4AFCFA5C33A7D3D27776137F8C997B3C52D89D8A8A4745F1CA21E45893EC";
constexpr float kMinimumSpeed = 0.5f;
constexpr float kMinimumEntryTravel = 0.15f;
constexpr float kMinimumBetweenHitsTravel = 0.5f;
constexpr float kRaySurfaceGap = 0.75f;
constexpr float kDistanceTolerance = 0.03f;
constexpr float kNormalTolerance = 0.03f;
constexpr float kStaticSurfaceRayLength = 20.0f;
constexpr auto kSettleTime = std::chrono::milliseconds(500);
constexpr auto kMotionTimeout = std::chrono::seconds(5);

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

VxVector CenterOf(IVP_Real_Object &object) {
    IVP_U_Matrix matrix;
    object.get_m_world_f_object_AT(&matrix);
    const IVP_U_Point *position = matrix.get_position();
    return VxVector(static_cast<float>(position->k[0]),
                    static_cast<float>(position->k[1]),
                    static_cast<float>(position->k[2]));
}

bool CastFromPositiveX(IVP_Real_Object &object, float radius,
                       float &hitDistance) {
    const VxVector center = CenterOf(object);
    IVP_Ray_Solver_Template configuration{};
    configuration.ray_start_point.set(center.x + radius + kRaySurfaceGap,
                                      center.y, center.z);
    configuration.ray_normized_direction.set(-1.0f, 0.0f, 0.0f);
    configuration.ray_length = 2.0f * (radius + kRaySurfaceGap);

    IVP_Ray_Solver_Min ray(&configuration);
    ray.check_ray_against_object(&object);
    const IVP_Ray_Hit *hit = ray.get_ray_hit();
    if (!hit)
        return false;

    hitDistance = hit->hit_distance;
    return hit->hit_real_object == &object &&
           hit->hit_compact_ledge == nullptr &&
           hit->hit_compact_triangle == nullptr &&
           std::fabs(hit->hit_distance - kRaySurfaceGap) <=
               kDistanceTolerance &&
           std::fabs(hit->hit_surface_direction_os.k[0] - 1.0f) <=
               kNormalTolerance &&
           std::fabs(hit->hit_surface_direction_os.k[1]) <=
               kNormalTolerance &&
           std::fabs(hit->hit_surface_direction_os.k[2]) <=
               kNormalTolerance;
}

bool CastStaticSurfaceBelow(
    IVP_Environment &environment, IVP_Real_Object &ball,
    float &surfaceDistance, float &directLedgeDistance,
    bool &directLedgePassed) {
    const VxVector center = CenterOf(ball);
    IVP_Ray_Solver_Template configuration{};
    configuration.ray_start_point.set(center.x, center.y, center.z);
    configuration.ray_normized_direction.set(0.0f, -1.0f, 0.0f);
    configuration.ray_length = kStaticSurfaceRayLength;
    configuration.ray_flags = IVP_RAY_SOLVER_IGNORE_MOVINGS;

    IVP_Ray_Solver_Min environmentRay(&configuration);
    environmentRay.check_ray_against_all_objects_in_sim(&environment);
    const IVP_Ray_Hit *surfaceHit = environmentRay.get_ray_hit();
    if (!surfaceHit || !surfaceHit->hit_real_object ||
        surfaceHit->hit_real_object == &ball ||
        surfaceHit->hit_real_object->get_type() != IVP_POLYGON ||
        !surfaceHit->hit_compact_ledge ||
        !surfaceHit->hit_compact_triangle ||
        !std::isfinite(surfaceHit->hit_distance) ||
        surfaceHit->hit_distance <= 0.0f ||
        surfaceHit->hit_distance > kStaticSurfaceRayLength) {
        return false;
    }
    surfaceDistance = surfaceHit->hit_distance;

    const IVP_Compact_Ledge *ledge = surfaceHit->hit_compact_ledge;
    IVP_Real_Object *surfaceObject = surfaceHit->hit_real_object;
    IVP_Ray_Solver_Min directRay(&configuration);
    directRay.check_ray_against_compact_ledge_os(ledge, surfaceObject);
    const IVP_Ray_Hit *directHit = directRay.get_ray_hit();
    if (!directHit)
        return true;

    directLedgeDistance = directHit->hit_distance;
    directLedgePassed =
        directHit->hit_real_object == surfaceObject &&
        directHit->hit_compact_ledge == ledge &&
        directHit->hit_compact_triangle != nullptr &&
        std::isfinite(directHit->hit_distance) &&
        std::fabs(directHit->hit_distance - surfaceHit->hit_distance) <=
            kDistanceTolerance;
    return true;
}

} // namespace

class BallSurfaceRaycastScenario::Impl final {
public:
    explicit Impl(CK3dEntity *ball)
        : Ball(ball), StageStartedAt(std::chrono::steady_clock::now()) {}

    void Advance() {
        if (Finished || !ResolveStableMapping())
            return;

        switch (CurrentStage) {
        case Stage::Settling:
            if (Elapsed() >= kSettleTime) {
                ResultValue.StaticSurfacePassed = CastStaticSurfaceBelow(
                    *Environment, *RealObject,
                    ResultValue.StaticSurfaceDistance,
                    ResultValue.DirectLedgeDistance,
                    ResultValue.DirectLedgePassed);
                if (!ResultValue.StaticSurfacePassed) {
                    Finish(false, "static-level-surface-ray-missed");
                    break;
                }
                if (!ResultValue.DirectLedgePassed) {
                    Finish(false, "direct-compact-ledge-ray-mismatch");
                    break;
                }
                Ball->GetPosition(&AccelerationStart);
                SetStage(Stage::SeekingFirstHit);
            }
            break;
        case Stage::SeekingFirstHit:
            AdvanceFirstHit();
            break;
        case Stage::SeekingSecondHit:
            AdvanceSecondHit();
            break;
        case Stage::Finished:
            break;
        }
    }

    std::uint32_t RequestedInputMask() const {
        return CurrentStage == Stage::SeekingFirstHit ||
                       CurrentStage == Stage::SeekingSecondHit
                   ? BML::PlayerTest::GameplayKeyUp
                   : BML::PlayerTest::GameplayKeyNone;
    }

    bool Done() const { return Finished; }
    BallSurfaceRaycastResult Result() const { return ResultValue; }

private:
    enum class Stage { Settling, SeekingFirstHit, SeekingSecondHit, Finished };

    bool ResolveStableMapping() {
        BML::IVP::ApiInfo info{};
        IVP_Real_Object *realObject =
            Ball ? BML::IVP::RealObject(Ball) : nullptr;
        IVP_Core *core = Ball ? BML::IVP::Core(Ball) : nullptr;
        BML_IVP_PhysicsObject *physicsObject =
            Ball ? BML::IVP::PhysicsObject(Ball) : nullptr;
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
        if ((RealObject && RealObject != realObject) ||
            (Core && Core != core)) {
            Finish(false, "player-ball-mapping-changed");
            return false;
        }
        RealObject = realObject;
        Core = core;
        Environment = core->get_environment();
        ResultValue.MappingPassed = true;
        if (Ball->GetName())
            ResultValue.BallName = Ball->GetName();

        if (RealObject->get_type() != IVP_BALL) {
            Finish(false, "player-object-is-not-retail-ball");
            return false;
        }
        ResultValue.BallRadius = RealObject->to_ball()->get_radius();
        ResultValue.RetailBallObserved =
            std::isfinite(ResultValue.BallRadius) &&
            ResultValue.BallRadius > 0.0f;
        if (!ResultValue.RetailBallObserved) {
            Finish(false, "retail-ball-radius-invalid");
            return false;
        }
        return true;
    }

    void AdvanceFirstHit() {
        VxVector position;
        Ball->GetPosition(&position);
        if (Length(LinearVelocity(*Core)) >= kMinimumSpeed &&
            Distance(position, AccelerationStart) >= kMinimumEntryTravel) {
            ResultValue.FirstHitPassed = CastFromPositiveX(
                *RealObject, ResultValue.BallRadius,
                ResultValue.FirstHitDistance);
            if (!ResultValue.FirstHitPassed) {
                Finish(false, "first-moving-ball-ray-missed");
                return;
            }
            FirstCenter = CenterOf(*RealObject);
            SetStage(Stage::SeekingSecondHit);
            return;
        }
        if (Elapsed() >= kMotionTimeout)
            Finish(false, "normal-input-produced-no-moving-ball");
    }

    void AdvanceSecondHit() {
        const VxVector center = CenterOf(*RealObject);
        ResultValue.CenterTravel = Distance(center, FirstCenter);
        if (ResultValue.CenterTravel >= kMinimumBetweenHitsTravel) {
            ResultValue.SecondHitPassed = CastFromPositiveX(
                *RealObject, ResultValue.BallRadius,
                ResultValue.SecondHitDistance);
            ResultValue.MovingSurfaceTracked =
                ResultValue.FirstHitPassed && ResultValue.SecondHitPassed;
            Finish(ResultValue.MovingSurfaceTracked,
                   ResultValue.MovingSurfaceTracked
                       ? "complete"
                       : "second-moving-ball-ray-missed");
            return;
        }
        if (Elapsed() >= kMotionTimeout)
            Finish(false, "moving-ball-did-not-reach-second-ray");
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
    IVP_Environment *Environment = nullptr;
    Stage CurrentStage = Stage::Settling;
    std::chrono::steady_clock::time_point StageStartedAt{};
    VxVector AccelerationStart;
    VxVector FirstCenter;
    bool Finished = false;
    BallSurfaceRaycastResult ResultValue;
};

BallSurfaceRaycastScenario::BallSurfaceRaycastScenario(CK3dEntity *ball)
    : m_Impl(std::make_unique<Impl>(ball)) {}

BallSurfaceRaycastScenario::~BallSurfaceRaycastScenario() = default;

void BallSurfaceRaycastScenario::Advance() { m_Impl->Advance(); }

std::uint32_t BallSurfaceRaycastScenario::RequestedInputMask() const {
    return m_Impl->RequestedInputMask();
}

bool BallSurfaceRaycastScenario::Done() const { return m_Impl->Done(); }

BallSurfaceRaycastResult BallSurfaceRaycastScenario::Result() const {
    return m_Impl->Result();
}
