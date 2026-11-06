#include "Api/HandleSequence.h"
#include "Api/TimerApi.h"
#include "Api/ModApi.h"
#include <cmath>
#include <limits>

namespace BML::Api {
int TimerApi::Register(ModContext &ctx, IMod &owner, BML_ModContextHandle handle,
                       const BML_TimerDefinition *definition, BML_TimerCallback callback,
                       void *data, BML_ReleaseCallback release, BML_TimerHandle *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    if (!callback || (data && !release))
        return BML_ERROR_INVALID_PARAMETER;
    if (!AcceptsModWork(ctx, owner))
        return BML_ERROR_FROZEN;
    auto *clock = ctx.GetTimeManager();
    if (!clock)
        return BML_ERROR_UNAVAILABLE;
    return m_Registry.Register(
        &owner, definition,
        [&ctx, &owner, handle, callback, data](const BML_TimerEvent &event, uint32_t &keep) {
            auto invocation = ctx.LockModInvocation();
            auto &loader = ctx.GetModLoader();
            if (!loader.CanDispatchOwner(&owner)) {
                keep = 0;
                return BML_OK;
            }
            int code = BML_ERROR_FAIL;
            try {
                code =
                    loader.InvokeAs(&owner, [&] { return callback(data, handle, &event, &keep); });
            } catch (const std::bad_alloc &) {
                code = BML_ERROR_OUT_OF_MEMORY;
            } catch (...) {
            }
            if (code != BML_OK || keep > 1)
                loader.FailOwner(&owner, code != BML_OK ? code : BML_ERROR_INVALID_PARAMETER,
                                 "Timer callback");
            return code;
        },
        [&ctx, &owner, data, release] {
            if (!release)
                return;
            auto invocation = ctx.LockModInvocation();
            ctx.GetModLoader().InvokeAs(&owner, [&] { release(data); });
        },
        clock->GetMainTickCount(), clock->GetAbsoluteTime() / 1000.0f, out);
}
int TimerApi::Read(IMod &owner, BML_TimerHandle handle, BML_TimerVisitor visitor, void *data) {
    if (!visitor)
        return BML_ERROR_INVALID_PARAMETER;
    return m_Registry.Read(&owner, handle,
                           [&](const BML_TimerEvent &event) { return visitor(data, &event); });
}
int TimerApi::Control(IMod &owner, BML_TimerHandle handle, uint32_t operation) {
    return m_Registry.Control(&owner, handle, operation);
}
void TimerApi::FinishProcess() { m_Registry.FinishProcess(); }
void TimerApi::CleanupOwner(IMod &owner) { m_Registry.CleanupOwner(&owner); }

namespace {
int BML_CDECL Register(BML_ModContextHandle h, const BML_TimerDefinition *d,
                       BML_TimerCallback callback, void *data, BML_ReleaseCallback release,
                       BML_TimerHandle *out) {
    if (out)
        *out = 0;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetTimerApi().Register(ctx, owner, h, d, callback, data, release, out);
    });
}
int BML_CDECL Read(BML_ModContextHandle h, BML_TimerHandle t, BML_TimerVisitor visitor,
                   void *data) {
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetTimerApi().Read(owner, t, visitor, data);
    });
}
int BML_CDECL Pause(BML_ModContextHandle h, BML_TimerHandle t) {
    return WithModContext(
        h, [&](ModContext &ctx, IMod &owner) { return ctx.GetTimerApi().Control(owner, t, 0); });
}
int BML_CDECL Resume(BML_ModContextHandle h, BML_TimerHandle t) {
    return WithModContext(
        h, [&](ModContext &ctx, IMod &owner) { return ctx.GetTimerApi().Control(owner, t, 1); });
}
int BML_CDECL Cancel(BML_ModContextHandle h, BML_TimerHandle t) {
    return WithModContext(
        h, [&](ModContext &ctx, IMod &owner) { return ctx.GetTimerApi().Control(owner, t, 2); });
}
const BML_TimerInterface table{BML_IFACE_HEADER(BML_TimerInterface, BML_TIMER_INTERFACE_ID, 1, 0),
                               Register,
                               Read,
                               Pause,
                               Resume,
                               Cancel};
} // namespace
const BML_TimerInterface &TimerInterface() { return table; }
} // namespace BML::Api
