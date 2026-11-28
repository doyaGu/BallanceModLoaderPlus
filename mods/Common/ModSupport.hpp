#include "Platform/CKDataArrayText.h"
#ifndef BML_MODS_COMMON_MODSUPPORT_HPP
#define BML_MODS_COMMON_MODSUPPORT_HPP

#include <BML/BMLAll.h>
#include <BML/Engine.hpp>
#include <BML/ImGui.hpp>
#include <BML/Behavior.hpp>
#include <BML/Behavior/Blocks.hpp>
#include <BML/ScriptHelper.h>
#include <BML/Guids.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <climits>
#include <cfloat>
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace ModSupport {
using Context = BML::Mod::Context;

inline void Check(int code) {
    if (code != BML_OK)
        throw std::runtime_error("BML operation failed (" + std::to_string(code) + ")");
}

inline CKContext *Engine(const Context &c) {
    return BML::Engine::BorrowContext(c).Value();
}

inline CKRenderContext *Render(const Context &c) {
    return BML::Engine::BorrowRenderContext(c).Value();
}

template <class T> T *Find(const Context &c, const char *name) {
    return static_cast<T *>(
        Engine(c)->GetObjectByNameAndClass((CKSTRING)name, BML::Engine::Detail::Class<T>::Value));
}

inline BML::Gameplay::State GameState() {
    BML::Gameplay::State state{};
    Check(BML::Gameplay::ReadState(state));
    return state;
}

inline bool CheatEnabled() {
    bool value = false;
    Check(BML::Gameplay::ReadCheatEnabled(value));
    return value;
}

inline BML::Input::KeyboardState Keyboard(const Context &c, uint32_t flags = 0) {
    return c.Input().ReadKeyboard(flags).Value();
}

inline void Log(const Context &c, uint32_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char small[1024];
    va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(small, sizeof small, format, copy);
    va_end(copy);
    std::string text;
    if (n >= int(sizeof small)) {
        text.resize(size_t(n) + 1);
        std::vsnprintf(text.data(), text.size(), format, args);
        text.resize(n);
    }
    va_end(args);
    const std::string_view value = n < 0 ? std::string_view("Log formatting failed")
                                   : n < int(sizeof small) ? std::string_view(small, size_t(n))
                                                           : std::string_view(text);
    if (level == BML_LOG_ERROR)
        (void)c.LogError(value);
    else if (level == BML_LOG_WARN)
        (void)c.LogWarn(value);
    else
        (void)c.LogInfo(value);
}

inline BML_ObjectRef Reference(const Context &c, CKObject *o) {
    return o ? BML::Engine::ReferenceObject(c, o).Value() : BML_ObjectRef{};
}

inline void SetIC(const Context &c, CKBeObject *o, bool hierarchy = false) {
    Check(BML::Scene::SetIC(c, Reference(c, o), hierarchy));
}

inline void RestoreIC(const Context &c, CKBeObject *o, bool hierarchy = false) {
    Check(BML::Scene::RestoreIC(c, Reference(c, o), hierarchy));
}

inline void Show(const Context &c, CKBeObject *o, CK_OBJECT_SHOWOPTION option = CKSHOW,
                 bool hierarchy = false) {
    Check(BML::Scene::Show(c, Reference(c, o), option, hierarchy));
}

inline void ExecuteCommand(const Context &context, const char *text) {
    context.Commands().ExecuteLine(text).Take();
}

inline bool ResourceIs(std::string_view path, std::string_view expected) {
    if (path.size() != expected.size())
        return false;
    for (size_t i = 0; i < path.size(); ++i) {
        char a = path[i] == '\\' ? '/' : path[i], b = expected[i] == '\\' ? '/' : expected[i];
        if (a >= 'A' && a <= 'Z')
            a += 32;
        if (b >= 'A' && b <= 'Z')
            b += 32;
        if (a != b)
            return false;
    }
    return true;
}

// Retain each registration, prune completed timers only when new work is added.
class Timers {
public:
    Timers() = default;

    explicit Timers(Context c) : m_Context(c) {}

    template <class F> void AfterTicks(uint64_t ticks, F &&f) {
        Keep(m_Context.Timers().SetTimeoutTicks(
            ticks, [fn = std::forward<F>(f)](const auto &, const auto &) { fn(); }));
    }

    template <class F> void After(double ms, F &&f) {
        Keep(m_Context.Timers().SetTimeout(
            ms, [fn = std::forward<F>(f)](const auto &, const auto &) { fn(); }));
    }

    template <class F> void EveryTicks(uint64_t ticks, F &&f) {
        Keep(m_Context.Timers().SetIntervalTicks(
            ticks,
            [fn = std::forward<F>(f)](const auto &, const auto &) { return fn(); }));
    }

private:
    void Keep(BML::Result<BML::Timer::Registration> r) {
        Check(r.Code());
        m_Values.erase(std::remove_if(m_Values.begin(), m_Values.end(),
                                      [](const auto &v) { return !v.IsValid(); }),
                       m_Values.end());
        m_Values.push_back(r.Take());
    }

    Context m_Context;
    std::vector<BML::Timer::Registration> m_Values;
};

inline BML::Behavior::Session &Session(BML::Behavior::Session &s, const Context &c) {
    if (!s) {
        auto owner = c.Owner().Value().Read().Value();
        auto r = BML::Behavior::Session::Open(owner.Definition.Id);
        if (!r)
            throw std::runtime_error("Behavior session: " + r.GetStatus().Message);
        s = r.Take();
    }
    return s;
}

inline BML::Behavior::Blocks::Physicalize::Options
PhysicsOptions(CK3dEntity *target, bool fixed, float friction, float elasticity, float mass,
               const char *group, bool frozen, bool collision, bool massCenter, float linear,
               float rotational, const char *surface, VxVector center) {
    BML::Behavior::Blocks::Physicalize::Options o;
    o.Target = target;
    o.Fixed = fixed;
    o.Friction = friction;
    o.Elasticity = elasticity;
    o.Mass = mass;
    o.CollisionGroup = group ? group : "";
    o.StartFrozen = frozen;
    o.EnableCollision = collision;
    o.CalculateMassCenter = massCenter;
    o.LinearDamping = linear;
    o.RotationalDamping = rotational;
    o.CollisionSurface = surface ? surface : "";
    o.MassCenter = center;
    return o;
}

inline void RunPhysics(BML::Behavior::Session &s, const Context &c,
                       BML::Behavior::Blocks::Physicalize::Options o, int input = 0) {
    auto block = BML::Behavior::Blocks::Physicalize::Make(Session(s, c), o);
    Check(block.Code());
    auto result = block.Value().Call(BML::Behavior::Selector::At(input), BML::Behavior::Ignore());
    Check(result.Code());
}

inline void Unphysicalize(BML::Behavior::Session &s, const Context &c, CK3dEntity *o) {
    RunPhysics(s, c,
               PhysicsOptions(o, false, .7f, .4f, 1, "", false, true, false, .1f, .1f, "", {}), 1);
}

inline void PhysicalizeBall(BML::Behavior::Session &s, const Context &c, CK3dEntity *target,
                            bool fixed = false, float friction = .7f, float elasticity = .4f,
                            float mass = 1, const char *group = "", bool frozen = false,
                            bool collision = true, bool massCenter = false, float linear = .1f,
                            float rotational = .1f, const char *surface = "", VxVector center = {},
                            VxVector ballCenter = {}, float radius = 2) {
    auto o = PhysicsOptions(target, fixed, friction, elasticity, mass, group, frozen, collision,
                            massCenter, linear, rotational, surface, center);
    o.Geometry = BML::Behavior::Blocks::Physicalize::Shape::Ball;
    o.Center = ballCenter;
    o.Radius = radius;
    RunPhysics(s, c, std::move(o));
}

inline void PhysicalizeConvex(BML::Behavior::Session &s, const Context &c, CK3dEntity *target,
                              bool fixed = false, float friction = .7f, float elasticity = .4f,
                              float mass = 1, const char *group = "", bool frozen = false,
                              bool collision = true, bool massCenter = false, float linear = .1f,
                              float rotational = .1f, const char *surface = "",
                              VxVector center = {}, CKMesh *mesh = nullptr) {
    auto o = PhysicsOptions(target, fixed, friction, elasticity, mass, group, frozen, collision,
                            massCenter, linear, rotational, surface, center);
    o.Mesh = mesh;
    RunPhysics(s, c, std::move(o));
}

inline CKObject *LoadObject(BML::Behavior::Session &s, const Context &c, const char *file,
                            bool rename, const char *master) {
    BML::Behavior::Blocks::ObjectLoad::Options options;
    options.File = file;
    std::replace(options.File.begin(), options.File.end(), '\\', '/');
    options.MasterName = master ? master : "";
    auto block = BML::Behavior::Blocks::ObjectLoad::Make(Session(s, c), options);
    if (!block)
        throw std::runtime_error("Object Load definition: " + block.GetStatus().Message);
    auto call =
        block.Value().Call(BML::Behavior::Selector::At(0), BML::Behavior::Signals().Pouts());
    if (!call)
        throw std::runtime_error("Object Load call: " + call.GetStatus().Message);
    auto frames = call.Value().TakeFrames();
    if (!frames)
        throw std::runtime_error("Object Load frames: " + frames.GetStatus().Message);
    if (frames.Value().Size() == 0)
        return nullptr;
    auto frame = frames.Value()[frames.Value().Size() - 1];
    auto ref = frame.Pout<BML_ObjectRef>(BML::Behavior::Selector::At(1));
    Check(ref.Code());
    auto *o = BML::Engine::BorrowObject<CKObject>(c, ref.Value()).Value();
    if (rename) {
        static uint64_t loadCount = 0;
        const auto suffix = "_BMLLoad_" + std::to_string(++loadCount);
        auto objects = frame.Pout<BML::Behavior::ObjectList>(BML::Behavior::Selector::At(0));
        Check(objects.Code());
        for (const auto object : objects.Value()) {
            auto borrowed = BML::Engine::BorrowObject<CKBeObject>(c, object);
            if (!borrowed)
                continue;
            auto *item = borrowed.Value();
            std::string name = item->GetName() ? item->GetName() : "";
            name += suffix;
            item->SetName((CKSTRING)name.c_str());
        }
    }
    return o;
}

inline int ParseInteger(const std::string &v, int minimum = INT_MIN, int maximum = INT_MAX) {
    return std::clamp(std::atoi(v.c_str()), minimum, maximum);
}

inline float ParseFloat(const std::string &v, float minimum = -FLT_MAX, float maximum = FLT_MAX) {
    return std::clamp(float(std::atof(v.c_str())), minimum, maximum);
}

// Delayed actions keep a 0.4 object reference, which expires when CK deletes
// the object or its scene is reset on either engine.
template <class T> class Object {
public:
    explicit Object(const BML::Mod::Context &context) : m_Context(&context) {}
    Object(const BML::Mod::Context &context, T *object) : Object(context) {
        *this = object;
    }

    Object &operator=(T *object) {
        m_Handle = {};
        if (object) {
            if (auto ref = BML::Engine::ReferenceObject(*m_Context, object))
                m_Handle = ref.Value();
        }
        return *this;
    }

    T *Get() const {
        if (!m_Handle.Domain)
            return nullptr;
        auto object = BML::Engine::BorrowObject<CKObject>(*m_Context, m_Handle);
        return object ? static_cast<T *>(object.Value()) : nullptr;
    }

    operator T *() const { return Get(); }
    T *operator->() const {
        auto *object = Get();
        if (!object)
            throw std::runtime_error("Game object no longer exists");
        return object;
    }

private:
    const BML::Mod::Context *m_Context;
    BML_ObjectRef m_Handle{};
};
} // namespace ModSupport
#endif
