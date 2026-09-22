#include "BallConstraintScenario.h"

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
constexpr float kMaximumAnchoredTravel = 0.30f;
constexpr float kMinimumReleasedTravel = 0.50f;
constexpr auto kSettleTime = std::chrono::milliseconds(500);
constexpr auto kAnchoredObservationTime = std::chrono::milliseconds(1200);
constexpr auto kReleasedTimeout = std::chrono::seconds(5);

float Distance(const VxVector &left, const VxVector &right) {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

class BallConstraintScenario::Impl final {
public:
    explicit Impl(CK3dEntity *ball)
        : Ball(ball), StageStartedAt(std::chrono::steady_clock::now()) {}

    ~Impl() { ReleaseConstraint(); }

    void Advance() {
        if (Finished || !ResolveStableMapping())
            return;

        switch (CurrentStage) {
        case Stage::Settling: AdvanceSettling(); break;
        case Stage::Anchored: AdvanceAnchored(); break;
        case Stage::Released: AdvanceReleased(); break;
        case Stage::Finished: break;
        }
    }

    std::uint32_t RequestedInputMask() const {
        return CurrentStage == Stage::Anchored ||
                       CurrentStage == Stage::Released
                   ? BML::PlayerTest::GameplayKeyUp
                   : BML::PlayerTest::GameplayKeyNone;
    }

    bool Done() const { return Finished; }
    BallConstraintResult Result() const { return ResultValue; }

private:
    enum class Stage { Settling, Anchored, Released, Finished };

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
        return true;
    }

    void AdvanceSettling() {
        if (Elapsed() < kSettleTime)
            return;

        IVP_U_Matrix objectMatrix;
        RealObject->get_m_world_f_object_AT(&objectMatrix);
        IVP_U_Point anchor = *objectMatrix.get_position();
        IVP_Template_Constraint definition;
        definition.set_ballsocket_ws(nullptr, &anchor, RealObject);
        Constraint = static_cast<IVP_Constraint_Local *>(
            Environment->create_constraint(&definition));
        ResultValue.ConstraintCreated = Constraint != nullptr;
        if (!Constraint) {
            Finish(false, "retail-constraint-create-failed");
            return;
        }

        ResultValue.EndpointMappingPassed =
            Constraint->get_objectR() == nullptr &&
            Constraint->get_objectA() == RealObject;
        IVP_U_Vector<IVP_Core> *cores =
            Constraint->get_associated_controlled_cores();
        ResultValue.CoreMembershipPassed =
            cores && cores->len() == 1 && cores->element_at(0) == Core;
        if (!ResultValue.EndpointMappingPassed ||
            !ResultValue.CoreMembershipPassed) {
            Finish(false, "retail-constraint-layout-mismatch");
            return;
        }

        Ball->GetPosition(&AnchoredStart);
        SetStage(Stage::Anchored);
    }

    void AdvanceAnchored() {
        VxVector position;
        Ball->GetPosition(&position);
        ResultValue.AnchoredTravel = std::max(
            ResultValue.AnchoredTravel, Distance(position, AnchoredStart));
        if (Elapsed() < kAnchoredObservationTime)
            return;

        ResultValue.AnchoredUnderInput =
            ResultValue.AnchoredTravel <= kMaximumAnchoredTravel;
        if (!ResultValue.AnchoredUnderInput) {
            Finish(false, "ballsocket-did-not-anchor-ball");
            return;
        }

        Constraint->free_translation_axis(IVP_INDEX_X);
        Constraint->free_translation_axis(IVP_INDEX_Y);
        Constraint->free_translation_axis(IVP_INDEX_Z);
        ResultValue.TranslationAxesFreed = true;
        Ball->GetPosition(&ReleasedStart);
        SetStage(Stage::Released);
    }

    void AdvanceReleased() {
        VxVector position;
        Ball->GetPosition(&position);
        ResultValue.ReleasedTravel = Distance(position, ReleasedStart);
        if (ResultValue.ReleasedTravel >= kMinimumReleasedTravel) {
            ResultValue.MovementResumed = true;
            Finish(true, "complete");
            return;
        }
        if (Elapsed() >= kReleasedTimeout)
            Finish(false, "ball-did-not-move-after-freeing-axes");
    }

    std::chrono::steady_clock::duration Elapsed() const {
        return std::chrono::steady_clock::now() - StageStartedAt;
    }

    void SetStage(Stage stage) {
        CurrentStage = stage;
        StageStartedAt = std::chrono::steady_clock::now();
    }

    void ReleaseConstraint() {
        if (!Constraint)
            return;
        delete Constraint;
        Constraint = nullptr;
    }

    void Finish(bool passed, const char *detail) {
        ResultValue.Passed = passed;
        ResultValue.Detail = detail;
        ReleaseConstraint();
        CurrentStage = Stage::Finished;
        Finished = true;
    }

    CK3dEntity *Ball = nullptr;
    IVP_Environment *Environment = nullptr;
    IVP_Real_Object *RealObject = nullptr;
    IVP_Core *Core = nullptr;
    IVP_Constraint_Local *Constraint = nullptr;
    Stage CurrentStage = Stage::Settling;
    std::chrono::steady_clock::time_point StageStartedAt{};
    VxVector AnchoredStart;
    VxVector ReleasedStart;
    bool Finished = false;
    BallConstraintResult ResultValue;
};

BallConstraintScenario::BallConstraintScenario(CK3dEntity *ball)
    : m_Impl(std::make_unique<Impl>(ball)) {}

BallConstraintScenario::~BallConstraintScenario() = default;

void BallConstraintScenario::Advance() { m_Impl->Advance(); }

std::uint32_t BallConstraintScenario::RequestedInputMask() const {
    return m_Impl->RequestedInputMask();
}

bool BallConstraintScenario::Done() const { return m_Impl->Done(); }

BallConstraintResult BallConstraintScenario::Result() const {
    return m_Impl->Result();
}
