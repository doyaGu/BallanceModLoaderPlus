#ifndef BML_BEHAVIOR_H
#define BML_BEHAVIOR_H

#include "CKAll.h"

#include <type_traits>
#include <vector>

namespace bml {

namespace detail {

template <typename T>
struct IsStringType : std::false_type {};
template <> struct IsStringType<const char *> : std::true_type {};
template <> struct IsStringType<char *> : std::true_type {};

template <typename T>
struct IsObjectType : std::false_type {};
template <> struct IsObjectType<CKObject *> : std::true_type {};

template <typename T, typename = void>
struct IsObjectDerived : std::false_type {};
template <typename T>
struct IsObjectDerived<T, std::enable_if_t<
    std::is_pointer_v<T> &&
    std::is_base_of_v<CKObject, std::remove_pointer_t<T>> &&
    !std::is_same_v<T, CKObject *>
>> : std::true_type {};

inline void WriteParam(CKParameter *param, const char *value) {
    param->SetStringValue((CKSTRING)value);
}

inline void WriteParam(CKParameter *param, CKObject *value) {
    CK_ID id = CKOBJID(value);
    param->SetValue(&id, sizeof(CK_ID));
}

template <typename T>
std::enable_if_t<!IsStringType<T>::value && !IsObjectType<T>::value && !IsObjectDerived<T>::value>
WriteParam(CKParameter *param, T value) {
    param->SetValue(&value, sizeof(T));
}

template <typename T>
std::enable_if_t<IsObjectDerived<T>::value>
WriteParam(CKParameter *param, T value) {
    WriteParam(param, static_cast<CKObject *>(value));
}

inline void WireInputSources(CKBehavior *beh, CKBehavior *owner, bool hasTarget) {
    for (int i = 0; i < beh->GetInputParameterCount(); i++) {
        auto *pin = beh->GetInputParameter(i);
        if (!pin->GetDirectSource()) {
            auto *local = owner->CreateLocalParameter(pin->GetName(), pin->GetGUID());
            pin->SetDirectSource(local);
        }
    }
    if (hasTarget && beh->GetTargetParameter()) {
        auto *tp = beh->GetTargetParameter();
        if (!tp->GetDirectSource()) {
            auto *local = owner->CreateLocalParameter(tp->GetName(), tp->GetGUID());
            tp->SetDirectSource(local);
        }
    }
}

} // namespace detail

// --- Parameter helpers (graph-independent) ---

template <typename T>
inline void SetParam(CKParameter *param, T value) {
    if (param) detail::WriteParam(param, value);
}

template <typename T>
inline T GetParam(CKParameter *param) {
    T res{};
    if (param) param->GetValue(&res);
    return res;
}

// ============================================================================
// BehaviorRunner
//
// Non-owning fluent wrapper for setting parameters on an existing CKBehavior
// and executing it immediately. Designed for cached behaviors that are created
// once and re-executed with different parameter values.
//
// Usage:
//   BehaviorRunner(cachedBeh)
//       .Target(myEntity)
//       .Input(1, 0.7f)
//       .Input(4, "MyGroup")
//       .Execute();
// ============================================================================

class BehaviorRunner {
public:
    BehaviorRunner() = default;
    explicit BehaviorRunner(CKBehavior *beh) : m_Beh(beh) {}

    explicit operator bool() const { return m_Beh != nullptr; }
    CKBehavior *Get() const { return m_Beh; }

    template <typename T>
    BehaviorRunner &Target(T obj) {
        if (!m_Beh) return *this;
        auto *tp = m_Beh->GetTargetParameter();
        if (!tp) return *this;
        auto *src = tp->GetDirectSource();
        if (!src) return *this;
        detail::WriteParam(src, obj);
        return *this;
    }

    template <typename T>
    BehaviorRunner &Input(int i, T value) {
        if (!m_Beh) return *this;
        auto *pin = m_Beh->GetInputParameter(i);
        if (!pin) return *this;
        auto *src = pin->GetDirectSource();
        if (!src) return *this;
        detail::WriteParam(src, value);
        return *this;
    }

    template <typename T>
    BehaviorRunner &Local(int i, T value) {
        if (!m_Beh) return *this;
        auto *param = m_Beh->GetLocalParameter(i);
        if (!param) return *this;
        detail::WriteParam(param, value);
        return *this;
    }

    BehaviorRunner &ApplySettings() {
        if (m_Beh) {
            m_Beh->CallCallbackFunction(CKM_BEHAVIORSETTINGSEDITED);
        }
        return *this;
    }

    void Execute(int slot = 0) {
        if (!m_Beh) return;
        m_Beh->ActivateInput(slot);
        m_Beh->Execute(0);
    }

    template <typename T>
    T ReadOutput(int i) const {
        T val{};
        if (m_Beh) m_Beh->GetOutputParameterValue(i, &val);
        return val;
    }

    CKObject *ReadOutputObj(int i) const {
        return m_Beh ? m_Beh->GetOutputParameterObject(i) : nullptr;
    }

    void *ReadOutputPtr(int i) const {
        return m_Beh ? m_Beh->GetOutputParameterWriteDataPtr(i) : nullptr;
    }

private:
    CKBehavior *m_Beh = nullptr;
};

// ============================================================================
// BehaviorFactory
//
// Creates a new CKBehavior node inside a script graph. Parameters are
// auto-typed from the prototype. Suitable for wiring into behavior graphs.
//
// Usage:
//   auto *node = BehaviorFactory(graph, PHYSICS_RT_PHYSICALIZE)
//       .Target(myEntity)
//       .Input(0, false)
//       .Input(1, 0.7f)
//       .Input(4, "MyGroup")
//       .Build();
// ============================================================================

class BehaviorFactory {
public:
    BehaviorFactory(CKBehavior *script, CKGUID guid, bool useTarget = false)
        : m_Script(script), m_Beh(nullptr) {
        if (!script) return;
        auto *ctx = script->GetCKContext();
        m_Beh = static_cast<CKBehavior *>(ctx->CreateObject(CKCID_BEHAVIOR));
        m_Beh->InitFromGuid(guid);
        if (useTarget)
            m_Beh->UseTarget();
        script->AddSubBehavior(m_Beh);
    }

    CKBehavior *Build() const { return m_Beh; }
    operator CKBehavior *() const { return m_Beh; }
    explicit operator bool() const { return m_Beh != nullptr; }

    template <typename T>
    BehaviorFactory &Target(T obj) {
        if (!m_Beh || !m_Script) return *this;
        auto *pin = m_Beh->GetTargetParameter();
        if (!pin) return *this;
        auto *local = m_Script->CreateLocalParameter(pin->GetName(), pin->GetGUID());
        detail::WriteParam(local, obj);
        pin->SetDirectSource(local);
        return *this;
    }

    BehaviorFactory &Target(CKObject *obj, CKGUID typeGuid) {
        if (!m_Beh || !m_Script) return *this;
        auto *pin = m_Beh->GetTargetParameter();
        if (!pin) return *this;
        auto *local = m_Script->CreateLocalParameter(pin->GetName(), typeGuid);
        detail::WriteParam(local, obj);
        pin->SetDirectSource(local);
        return *this;
    }

    template <typename T>
    BehaviorFactory &Input(int i, T value) {
        if (!m_Beh || !m_Script) return *this;
        auto *pin = m_Beh->GetInputParameter(i);
        if (!pin) return *this;
        auto *local = m_Script->CreateLocalParameter(pin->GetName(), pin->GetGUID());
        detail::WriteParam(local, value);
        pin->SetDirectSource(local);
        return *this;
    }

    template <typename T>
    BehaviorFactory &Input(int i, const char *name, CKGUID guid, T value) {
        if (!m_Beh || !m_Script) return *this;
        auto *local = m_Script->CreateLocalParameter((CKSTRING)name, guid);
        detail::WriteParam(local, value);
        auto *pin = m_Beh->GetInputParameter(i);
        if (pin) pin->SetDirectSource(local);
        return *this;
    }

    template <typename T>
    BehaviorFactory &Local(int i, T value) {
        if (!m_Beh) return *this;
        auto *param = m_Beh->GetLocalParameter(i);
        if (!param) return *this;
        detail::WriteParam(param, value);
        return *this;
    }

    BehaviorFactory &ApplySettings() {
        if (m_Beh) m_Beh->CallCallbackFunction(CKM_BEHAVIORSETTINGSEDITED);
        return *this;
    }

private:
    CKBehavior *m_Script = nullptr;
    CKBehavior *m_Beh = nullptr;
};

// ============================================================================
// BehaviorCache
//
// Manages a cache of pre-created CKBehavior instances for immediate
// re-execution. Each behavior is created once under an owner script; on
// subsequent Get() calls the same instance is returned.
//
// Usage:
//   BehaviorCache cache;
//   cache.Init(ownerScript);
//
//   cache.Get(PHYSICS_RT_PHYSICALIZE)
//       .Target(myEntity)
//       .Input(1, 0.7f)
//       .Execute();
// ============================================================================

class BehaviorCache {
public:
    BehaviorCache() = default;

    bool Init(CKBehavior *owner) {
        if (!owner) return false;
        if (m_Owner != owner) {
            ResetCache();
        }
        m_Owner = owner;
        m_Ctx = owner->GetCKContext();
        return m_Ctx != nullptr;
    }

    BehaviorRunner Get(CKGUID guid, bool useTarget = true) {
        for (auto &e : m_Cache) {
            if (e.guid == guid && e.useTarget == useTarget)
                return BehaviorRunner(e.beh);
        }

        if (!m_Owner || !m_Ctx)
            return BehaviorRunner();

        auto *beh = static_cast<CKBehavior *>(m_Ctx->CreateObject(CKCID_BEHAVIOR));
        beh->InitFromGuid(guid);
        if (useTarget)
            beh->UseTarget();
        m_Owner->AddSubBehavior(beh);

        detail::WireInputSources(beh, m_Owner, useTarget);

        m_Cache.push_back({guid, useTarget, beh});
        return BehaviorRunner(beh);
    }

    CKContext *Context() const { return m_Ctx; }
    CKBehavior *Owner() const { return m_Owner; }

private:
    void ResetCache() {
        if (m_Owner && m_Ctx) {
            for (const auto &entry : m_Cache) {
                if (!entry.beh) {
                    continue;
                }
                m_Owner->RemoveSubBehavior(entry.beh);
                m_Ctx->DestroyObject(entry.beh);
            }
        }
        m_Cache.clear();
    }

    struct Entry {
        CKGUID guid;
        bool useTarget;
        CKBehavior *beh;
    };

    CKContext *m_Ctx = nullptr;
    CKBehavior *m_Owner = nullptr;
    std::vector<Entry> m_Cache;
};

} // namespace bml

#endif // BML_BEHAVIOR_H
