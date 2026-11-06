#include "Api/GameplayRegistrationApi.h"
#include "Api/ModApi.h"
#include "Hooks/RenderHook.h"
#include <cmath>

namespace BML::Api {
namespace {
template <class... Values> bool Finite(Values... values) {
    return (std::isfinite(values) && ...);
}

template <class Body> int Register(BML_ModContextHandle h, Body &&body) {
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        if (ctx.GetModLoader().IsShuttingDown())
            return BML_ERROR_FROZEN;
        if (const auto *native = ctx.GetModLoader().FindNativeMod(&owner);
            native && !native->IsLoading())
            return BML_ERROR_FROZEN;
        body(ctx);
        return BML_OK;
    });
}

template <class Body>
int Mutate(BML_ModContextHandle h, BML_ObjectRef ref, uint32_t hierarchy, Body &&body) {
    if (hierarchy > 1)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &) {
        if (!ref.Domain)
            return BML_OK;
        auto *object = ctx.ObjectRefs().Resolve(ref);
        if (!object)
            return BML_ERROR_OBJECT_INVALID;
        if (!CKIsChildClassOf(object->GetClassID(), CKCID_BEOBJECT))
            return BML_ERROR_TYPE_MISMATCH;
        body(ctx, static_cast<CKBeObject *>(object));
        return BML_OK;
    });
}
} // namespace

int BML_CDECL RegisterBallType(BML_ModContextHandle h, const BML_BallTypeDefinition *d) {
    if (!d || d->StructSize < BML_BALL_TYPE_DEFINITION_1_0_SIZE || !d->BallFile || !*d->BallFile ||
        !d->BallId || !*d->BallId || !d->BallName || !*d->BallName || !d->ObjectName ||
        !*d->ObjectName || !d->CollisionGroup ||
        !Finite(d->Friction, d->Elasticity, d->Mass, d->LinearDamp, d->RotDamp, d->Force,
                d->Radius))
        return BML_ERROR_INVALID_PARAMETER;
    return Register(h, [&](ModContext &ctx) {
        ctx.RegisterBallType(d->BallFile, d->BallId, d->BallName, d->ObjectName, d->Friction,
                             d->Elasticity, d->Mass, d->CollisionGroup, d->LinearDamp, d->RotDamp,
                             d->Force, d->Radius);
    });
}

int BML_CDECL RegisterFloorType(BML_ModContextHandle h, const BML_FloorTypeDefinition *d) {
    if (!d || d->StructSize < BML_FLOOR_TYPE_DEFINITION_1_0_SIZE || !d->Name || !*d->Name ||
        !d->CollisionGroup || d->EnableCollision > 1 ||
        !Finite(d->Friction, d->Elasticity, d->Mass))
        return BML_ERROR_INVALID_PARAMETER;
    return Register(h, [&](ModContext &ctx) {
        ctx.RegisterFloorType(d->Name, d->Friction, d->Elasticity, d->Mass, d->CollisionGroup,
                              d->EnableCollision != 0);
    });
}

int BML_CDECL RegisterModuleBall(BML_ModContextHandle h, const BML_ModuleBallDefinition *d) {
    if (!d || d->StructSize < BML_MODULE_BALL_DEFINITION_1_0_SIZE || !d->Name || !*d->Name ||
        !d->CollisionGroup || d->Fixed > 1 || d->StartFrozen > 1 || d->EnableCollision > 1 ||
        d->CalcMassCenter > 1 ||
        !Finite(d->Friction, d->Elasticity, d->Mass, d->LinearDamp, d->RotDamp, d->Radius))
        return BML_ERROR_INVALID_PARAMETER;
    return Register(h, [&](ModContext &ctx) {
        ctx.RegisterModulBall(d->Name, d->Fixed != 0, d->Friction, d->Elasticity, d->Mass,
                              d->CollisionGroup, d->StartFrozen != 0, d->EnableCollision != 0,
                              d->CalcMassCenter != 0, d->LinearDamp, d->RotDamp, d->Radius);
    });
}

int BML_CDECL RegisterModuleConvex(BML_ModContextHandle h, const BML_ModuleConvexDefinition *d) {
    if (!d || d->StructSize < BML_MODULE_CONVEX_DEFINITION_1_0_SIZE || !d->Name || !*d->Name ||
        !d->CollisionGroup || d->Fixed > 1 || d->StartFrozen > 1 || d->EnableCollision > 1 ||
        d->CalcMassCenter > 1 ||
        !Finite(d->Friction, d->Elasticity, d->Mass, d->LinearDamp, d->RotDamp))
        return BML_ERROR_INVALID_PARAMETER;
    return Register(h, [&](ModContext &ctx) {
        ctx.RegisterModulConvex(d->Name, d->Fixed != 0, d->Friction, d->Elasticity, d->Mass,
                                d->CollisionGroup, d->StartFrozen != 0, d->EnableCollision != 0,
                                d->CalcMassCenter != 0, d->LinearDamp, d->RotDamp);
    });
}

int BML_CDECL RegisterTrafo(BML_ModContextHandle h, const BML_TrafoDefinition *d) {
    if (!d || d->StructSize < BML_TRAFO_DEFINITION_1_0_SIZE || !d->Name || !*d->Name)
        return BML_ERROR_INVALID_PARAMETER;
    return Register(h, [&](ModContext &ctx) { ctx.RegisterTrafo(d->Name); });
}

int BML_CDECL RegisterModule(BML_ModContextHandle h, const BML_ModuleDefinition *d) {
    if (!d || d->StructSize < BML_MODULE_DEFINITION_1_0_SIZE || !d->Name || !*d->Name)
        return BML_ERROR_INVALID_PARAMETER;
    return Register(h, [&](ModContext &ctx) { ctx.RegisterModul(d->Name); });
}

int BML_CDECL ReadGameplayState(BML_GameplayState *out) {
    if (!out || out->StructSize < BML_GAMEPLAY_STATE_1_0_SIZE)
        return BML_ERROR_INVALID_PARAMETER;
    ModContextLease ctx;
    if (!ctx)
        return BML_ERROR_FROZEN;
    if (!ctx->IsMainThread())
        return BML_ERROR_WRONG_THREAD;
    const BML_GameplayState value{sizeof(*out), ctx->IsIngame() ? 1u : 0u,
                                  ctx->IsPaused() ? 1u : 0u, ctx->IsPlaying() ? 1u : 0u};
    WriteOutput(out, value, BML_GAMEPLAY_STATE_1_0_SIZE);
    return BML_OK;
}

int BML_CDECL EnableCheat(BML_ModContextHandle h, uint32_t enabled) {
    if (enabled > 1)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &) {
        ctx.EnableCheat(enabled != 0);
        return BML_OK;
    });
}

int BML_CDECL ExitGame(BML_ModContextHandle h) {
    return WithModContext(h, [&](ModContext &ctx, IMod &) {
        ctx.ExitGame();
        return BML_OK;
    });
}

int BML_CDECL SetIC(BML_ModContextHandle h, BML_ObjectRef ref, uint32_t hierarchy) {
    return Mutate(h, ref, hierarchy,
                  [&](ModContext &ctx, CKBeObject *o) { ctx.SetIC(o, hierarchy != 0); });
}

int BML_CDECL RestoreIC(BML_ModContextHandle h, BML_ObjectRef ref, uint32_t hierarchy) {
    return Mutate(h, ref, hierarchy,
                  [&](ModContext &ctx, CKBeObject *o) { ctx.RestoreIC(o, hierarchy != 0); });
}

int BML_CDECL Show(BML_ModContextHandle h, BML_ObjectRef ref, uint32_t option, uint32_t hierarchy) {
    if (option > CKHIERARCHICALHIDE)
        return BML_ERROR_INVALID_PARAMETER;
    return Mutate(h, ref, hierarchy, [&](ModContext &ctx, CKBeObject *o) {
        ctx.Show(o, static_cast<CK_OBJECT_SHOWOPTION>(option), hierarchy != 0);
    });
}

int BML_CDECL SkipNextRender(BML_ModContextHandle h) {
    return WithModContext(h, [&](ModContext &, IMod &) {
        if (!RenderHook::IsSkipRenderAvailable())
            return BML_ERROR_UNAVAILABLE;
        RenderHook::SkipNextRender();
        return BML_OK;
    });
}

int BML_CDECL SetWidescreenAdjustment(BML_ModContextHandle h, uint32_t enabled) {
    if (enabled > 1)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &, IMod &) {
        if (!RenderHook::IsWidescreenFixAvailable())
            return BML_ERROR_UNAVAILABLE;
        RenderHook::EnableWidescreenFix(enabled != 0);
        return BML_OK;
    });
}
} // namespace BML::Api
