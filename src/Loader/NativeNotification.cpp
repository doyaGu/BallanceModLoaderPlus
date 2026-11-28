#include "Loader/NativeNotification.h"
#include "Loader/ModContext.h"
#include <array>
#include <cassert>

struct NativeNotification::Storage {
    enum Kind {
        None,
        Process,
        Render,
        Game,
        Cheat,
        LoadObject,
        LoadScript,
        Command,
        Physicalize,
        Unphysicalize
    } Type = None;

    BML_GameEvent GameValue{};
    BML_RenderEvent RenderValue{};
    BML_CheatEvent CheatValue{};
    BML_LoadObjectEvent LoadObjectValue{};
    BML_LoadScriptEvent LoadScriptValue{};
    BML_CommandEvent CommandValue{};
    BML_PhysicalizeEvent PhysicalizeValue{};
    BML_ObjectEvent ObjectValue{};
    std::array<std::string, 3> Text;
    std::vector<BML_ObjectRef> Objects, Convex, Concave;
    std::vector<BML_Vec3> Centers;
    std::vector<float> Radii;
    std::vector<std::string> Arguments;
    std::vector<const char *> ArgumentViews;
};

namespace {
struct Pool {
    std::vector<std::unique_ptr<NativeNotification::Storage>> Frames;
    size_t Depth = 0;

    NativeNotification::Storage *Acquire() {
        if (Depth == Frames.size())
            Frames.push_back(std::make_unique<NativeNotification::Storage>());
        auto *frame = Frames[Depth++].get();
        frame->Type = NativeNotification::Storage::None;
        return frame;
    }
};

thread_local Pool s_Pool;
} // namespace

NativeNotification::NativeNotification(ModContext &context)
    : m_Context(context), m_Storage(s_Pool.Acquire()) {}

NativeNotification::~NativeNotification() {
    assert(s_Pool.Depth);
    --s_Pool.Depth;
}

void NativeNotification::Prepare(ModEvent event) {
    if (event == ModEvent::OnProcess) {
        m_Storage->Type = Storage::Process;
    } else if (event < ModEvent::OnLoad) {
        m_Storage->Type = Storage::Game;
        m_Storage->GameValue = static_cast<BML_GameEvent>(event);
    }
}

void NativeNotification::Prepare(ModEvent event, CK_RENDER_FLAGS flags) {
    m_Storage->Type = Storage::Render;
    m_Storage->RenderValue = {sizeof(BML_RenderEvent), static_cast<uint32_t>(flags)};
}

void NativeNotification::Prepare(ModEvent event, bool enabled) {
    m_Storage->Type = Storage::Cheat;
    m_Storage->CheatValue = {sizeof(BML_CheatEvent), enabled ? 1u : 0u};
}

void NativeNotification::Prepare(ModEvent event, const char *filename, CKBehavior *script) {
    auto &s = *m_Storage;
    s.Type = Storage::LoadScript;
    s.Text[0] = filename ? filename : "";
    s.LoadScriptValue = {sizeof(BML_LoadScriptEvent), s.Text[0].c_str(),
                         m_Context.ObjectRefs().Issue(script)};
}

void NativeNotification::Prepare(ModEvent event, CK3dEntity *object) {
    m_Storage->Type = Storage::Unphysicalize;
    m_Storage->ObjectValue = {sizeof(BML_ObjectEvent), m_Context.ObjectRefs().Issue(object)};
}

void NativeNotification::Prepare(ModEvent event, BML::CommandRecord *command,
                                 const std::vector<std::string> &args) {
    auto &s = *m_Storage;
    s.Type = Storage::Command;
    s.Text[0] = command ? command->Info.Name : "";
    s.Arguments = args;
    s.ArgumentViews.clear();
    for (size_t i = 1; i < s.Arguments.size(); ++i)
        s.ArgumentViews.push_back(s.Arguments[i].c_str());
    s.CommandValue = {sizeof(BML_CommandEvent),
                      event == ModEvent::OnPreCommandExecute ? BML_COMMAND_EVENT_PRE
                                                             : BML_COMMAND_EVENT_POST,
                      s.Text[0].c_str(),
                      s.Arguments.empty() ? "" : s.Arguments[0].c_str(),
                      s.ArgumentViews.data(),
                      s.ArgumentViews.size(),
                      command && command->Info.Cheat ? 1u : 0u};
}

void NativeNotification::Prepare(ModEvent event, const char *filename, CKBOOL isMap,
                                 const char *masterName, CK_CLASSID filterClass, CKBOOL addToScene,
                                 CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                                 XObjectArray *objects, CKObject *master) {
    auto &s = *m_Storage;
    s.Type = Storage::LoadObject;
    s.Text[0] = filename ? filename : "";
    s.Text[1] = masterName ? masterName : "";
    s.Objects.clear();
    if (objects)
        for (int i = 0; i < objects->Size(); ++i)
            s.Objects.push_back(
                m_Context.ObjectRefs().Issue(objects->GetObject(m_Context.GetCKContext(), i)));
    s.LoadObjectValue = {sizeof(BML_LoadObjectEvent),
                         s.Text[0].c_str(),
                         isMap ? 1u : 0u,
                         s.Text[1].c_str(),
                         static_cast<uint32_t>(filterClass),
                         addToScene ? 1u : 0u,
                         reuseMeshes ? 1u : 0u,
                         reuseMaterials ? 1u : 0u,
                         dynamic ? 1u : 0u,
                         s.Objects.data(),
                         s.Objects.size(),
                         m_Context.ObjectRefs().Issue(master)};
}

void NativeNotification::Prepare(ModEvent event, CK3dEntity *target, CKBOOL fixed, float friction,
                                 float elasticity, float mass, const char *group, CKBOOL frozen,
                                 CKBOOL collision, CKBOOL center, float linear, float rot,
                                 const char *surface, VxVector massCenter, int convexCount,
                                 CKMesh **convex, int sphereCount, VxVector *centers, float *radii,
                                 int concaveCount, CKMesh **concave) {
    auto &s = *m_Storage;
    s.Type = Storage::Physicalize;
    s.Text[0] = group ? group : "";
    s.Text[1] = surface ? surface : "";
    s.Convex.clear();
    s.Concave.clear();
    s.Centers.clear();
    s.Radii.clear();
    for (int i = 0; i < convexCount; ++i)
        s.Convex.push_back(m_Context.ObjectRefs().Issue(convex[i]));
    for (int i = 0; i < concaveCount; ++i)
        s.Concave.push_back(m_Context.ObjectRefs().Issue(concave[i]));
    for (int i = 0; i < sphereCount; ++i) {
        s.Centers.push_back({centers[i].x, centers[i].y, centers[i].z});
        s.Radii.push_back(radii[i]);
    }
    s.PhysicalizeValue = {sizeof(BML_PhysicalizeEvent),
                          m_Context.ObjectRefs().Issue(target),
                          fixed ? 1u : 0u,
                          friction,
                          elasticity,
                          mass,
                          group ? s.Text[0].c_str() : nullptr,
                          frozen ? 1u : 0u,
                          collision ? 1u : 0u,
                          center ? 1u : 0u,
                          linear,
                          rot,
                          surface ? s.Text[1].c_str() : nullptr,
                          {massCenter.x, massCenter.y, massCenter.z},
                          s.Convex.data(),
                          s.Convex.size(),
                          s.Centers.data(),
                          s.Radii.data(),
                          s.Centers.size(),
                          s.Concave.data(),
                          s.Concave.size()};
}

int NativeNotification::Dispatch(NativeMod &mod) const {
    const auto &c = mod.Callbacks();
    const auto &s = *m_Storage;
    void *p = mod.Instance();
    const auto h = mod.Context();
    if (!p)
        return BML_OK;
    switch (s.Type) {
    case Storage::Process:
        return c.OnProcess ? c.OnProcess(p, h) : BML_OK;
    case Storage::Game:
        return c.OnGameEvent ? c.OnGameEvent(p, h, s.GameValue) : BML_OK;
    case Storage::Render:
        return c.OnRender ? c.OnRender(p, h, &s.RenderValue) : BML_OK;
    case Storage::Cheat:
        return c.OnCheatEnabled ? c.OnCheatEnabled(p, h, &s.CheatValue) : BML_OK;
    case Storage::LoadObject:
        return c.OnLoadObject ? c.OnLoadObject(p, h, &s.LoadObjectValue) : BML_OK;
    case Storage::LoadScript:
        return c.OnLoadScript ? c.OnLoadScript(p, h, &s.LoadScriptValue) : BML_OK;
    case Storage::Command:
        return c.OnCommandEvent ? c.OnCommandEvent(p, h, &s.CommandValue) : BML_OK;
    case Storage::Physicalize:
        return c.OnPhysicalize ? c.OnPhysicalize(p, h, &s.PhysicalizeValue) : BML_OK;
    case Storage::Unphysicalize:
        return c.OnUnphysicalize ? c.OnUnphysicalize(p, h, &s.ObjectValue) : BML_OK;
    default:
        return BML_OK;
    }
}

const char *NativeNotification::Name() const noexcept {
    static constexpr const char *names[] = {
        "none",         "OnProcess",    "OnRender",       "OnGameEvent",   "OnCheatEnabled",
        "OnLoadObject", "OnLoadScript", "OnCommandEvent", "OnPhysicalize", "OnUnphysicalize"};
    return names[m_Storage->Type];
}
