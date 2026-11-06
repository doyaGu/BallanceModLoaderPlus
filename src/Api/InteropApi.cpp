#include "Api/InteropApi.h"
#include "Api/ModApi.h"
#include "UI/BuiHost.h"
#include "UI/Overlay.h"
#include "imgui_internal.h"

namespace BML::Api {
namespace {
int BML_CDECL Context(BML_ModContextHandle h, void **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    return WithModContext(h, [&](ModContext &ctx, IMod &) {
        *out = ctx.GetCKContext();
        return *out ? BML_OK : BML_ERROR_UNAVAILABLE;
    });
}
int BML_CDECL Reference(BML_ModContextHandle h, const void *object, BML_ObjectRef *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = {};
    return WithModContext(h, [&](ModContext &ctx, IMod &) {
        *out =
            ctx.ObjectRefs().Issue(const_cast<CKObject *>(static_cast<const CKObject *>(object)));
        return !object || out->Domain ? BML_OK : BML_ERROR_OBJECT_INVALID;
    });
}
int BML_CDECL Object(BML_ModContextHandle h, BML_ObjectRef object, uint32_t expectedClass,
                     void **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    return WithModContext(h, [&](ModContext &ctx, IMod &) {
        auto *value = ctx.ObjectRefs().Resolve(object);
        if (!value ||
            !CKIsChildClassOf(value->GetClassID(), static_cast<CK_CLASSID>(expectedClass)))
            return BML_ERROR_OBJECT_INVALID;
        *out = value;
        return BML_OK;
    });
}
int BML_CDECL ImGuiContext(BML_ModContextHandle h, void **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    return WithModContext(h, [&](ModContext &, IMod &) {
        *out = Overlay::GetImGuiContext();
        return *out ? BML_OK : BML_ERROR_UNAVAILABLE;
    });
}
int BML_CDECL Image(BML_ModContextHandle h, void *drawList, BML_Vec2 position, uint32_t kind,
                    int32_t state, const char *text, BML_Vec2 alignment) {
    if (!drawList || !text || kind > static_cast<uint32_t>(BuiHost::BUTTON_MINUS))
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &, IMod &) {
        auto *context = Overlay::GetImGuiContext();
        if (!context || !context->WithinFrameScope)
            return BML_ERROR_FROZEN;
        Overlay::ImGuiContextScope scope;
        auto *list = static_cast<ImDrawList *>(drawList);
        if (list->_Data != &context->DrawListSharedData)
            return BML_ERROR_INVALID_PARAMETER;
        BuiHost::AddButtonImage(list, {position.x, position.y},
                                static_cast<BuiHost::ButtonType>(kind), state, text,
                                {alignment.x, alignment.y});
        return BML_OK;
    });
}
const BML_EngineInterface engine{
    BML_IFACE_HEADER(BML_EngineInterface, BML_ENGINE_INTERFACE_ID, 1, 0), Context, Reference,
    Object};
const BML_ImGuiInterface imgui{BML_IFACE_HEADER(BML_ImGuiInterface, BML_IMGUI_INTERFACE_ID, 1, 0),
                               ImGuiContext, Image};
} // namespace
const BML_EngineInterface &EngineInterface() { return engine; }
const BML_ImGuiInterface &ImGuiInterface() { return imgui; }
} // namespace BML::Api
