#ifndef BML_BEHAVIOR_SCRIPT_HPP
#define BML_BEHAVIOR_SCRIPT_HPP

#include "BML/Behavior/Edit.hpp"
#include "BML/Behavior/Detail/Wire.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

namespace BML::Behavior {

enum class ScriptState : std::uint32_t {
    Ready = BML_BEHAVIOR_SCRIPT_READY,
    Closing = BML_BEHAVIOR_SCRIPT_CLOSING,
    Failed = BML_BEHAVIOR_SCRIPT_FAILED,
};

struct ScriptInfo {
    ScriptState State = ScriptState::Ready;
    bool Active = false;
    bool RequestedActive = false;
    BML_ObjectRef Root{};
    BML_ObjectRef Owner{};
    BML_ObjectRef Scene{};
    std::int32_t Priority = 0;
    Behavior::Status LastStatus;
};

namespace Detail {

inline bool KnownScriptState(std::uint32_t state) noexcept {
    return state >= BML_BEHAVIOR_SCRIPT_READY &&
           state <= BML_BEHAVIOR_SCRIPT_FAILED;
}

inline bool SameScriptObject(BML_ObjectRef left,
                             BML_ObjectRef right) noexcept {
    return left.Domain == right.Domain && left.Slot == right.Slot &&
           left.Generation == right.Generation;
}

inline bool ValidScriptInfo(const BML_BehaviorScriptInfo &info) noexcept {
    return info.StructSize >= sizeof(info) && KnownScriptState(info.State) &&
           info.Active <= 1 && info.RequestedActive <= 1 &&
           ValidObjectRef(info.Root) && info.Root.Domain != 0 &&
           ValidObjectRef(info.Owner) && info.Owner.Domain != 0 &&
           ValidObjectRef(info.Scene) && info.Scene.Domain != 0 &&
           ValidStatus(info.Status);
}

inline ScriptInfo ReadScriptInfo(const BML_BehaviorScriptInfo &source) {
    return {static_cast<ScriptState>(source.State), source.Active != 0,
            source.RequestedActive != 0, source.Root, source.Owner,
            source.Scene, source.Priority, ReadStatus(source.Status)};
}

} // namespace Detail

// Owns one top-level graph-backed CKBehavior and the initial Edit installed by
// Session::CreateScript. It starts inactive; later changes use Apply(), and
// Activate() opts into Virtools scheduling at the next Behavior safe point.
class Script {
public:
    Script() = default;
    ~Script() { (void) Close(); }
    Script(const Script &) = delete;
    Script &operator=(const Script &) = delete;
    Script(Script &&other) noexcept
        : m_Session(std::move(other.m_Session)),
          m_Handle(std::exchange(other.m_Handle, nullptr)),
          m_Root(std::exchange(other.m_Root, BML_ObjectRef{})) {}
    Script &operator=(Script &&other) noexcept {
        if (this != &other) {
            Script previous(std::move(*this));
            m_Session = std::move(other.m_Session);
            m_Handle = std::exchange(other.m_Handle, nullptr);
            m_Root = std::exchange(other.m_Root, BML_ObjectRef{});
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
    }
    [[nodiscard]] BML_ObjectRef Object() const noexcept { return m_Root; }

    [[nodiscard]] Result<ScriptInfo> Info() const {
        if (!*this)
            return Result<ScriptInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface,
                           ReadScript))
            return Result<ScriptInfo>::Failure(BML_ERROR_VERSION_MISMATCH);
        BML_BehaviorScriptInfo wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_Session->Api->ReadScript(
                m_Session->Handle, m_Handle, &wire, &status), status);
        if (code != BML_OK)
            return Result<ScriptInfo>::Failure(
                code, Detail::ReadStatus(status));
        if (!Detail::ValidScriptInfo(wire) ||
            !Detail::SameScriptObject(wire.Root, m_Root))
            return Result<ScriptInfo>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<ScriptInfo>::Success(
            Detail::ReadScriptInfo(wire), Detail::ReadStatus(status));
    }

    [[nodiscard]] Result<ScriptInfo> Activate(bool reset = false) {
        return SetActive(true, reset);
    }
    [[nodiscard]] Result<ScriptInfo> Deactivate() {
        return SetActive(false, false);
    }
    [[nodiscard]] Result<Graph> Inspect(View view = View::Logical) const {
        return *this
            ? Graph::Read(m_Session, m_Root, view)
            : Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    }
    [[nodiscard]] Result<Patch> Apply(std::string_view name,
                                      const Edit &edit) const {
        Result<Graph> graph = Inspect(View::Logical);
        return graph
            ? graph->Apply(name, edit)
            : Result<Patch>::Failure(graph.Code(), graph.GetStatus());
    }

    [[nodiscard]] Result<CloseState> Close() noexcept {
        if (!m_Handle) {
            m_Session.reset();
            m_Root = {};
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (!m_Session || !m_Session->Api || !m_Session->Handle)
            return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
        if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface,
                           CloseScript))
            return Result<CloseState>::Failure(BML_ERROR_VERSION_MISMATCH);
        const int code = m_Session->Api->CloseScript(
            m_Session->Handle, m_Handle);
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
            m_Handle = nullptr;
            m_Root = {};
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (code == BML_ERROR_BUSY)
            return Result<CloseState>::Success(CloseState::Closing);
        return Result<CloseState>::Failure(code);
    }

private:
    Script(std::shared_ptr<Detail::SessionState> session,
           BML_BehaviorScript handle, BML_ObjectRef root)
        : m_Session(std::move(session)), m_Handle(handle), m_Root(root) {}

    [[nodiscard]] Result<ScriptInfo> SetActive(bool active, bool reset) {
        if (!*this)
            return Result<ScriptInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface,
                           SetScriptActive))
            return Result<ScriptInfo>::Failure(BML_ERROR_VERSION_MISMATCH);
        BML_BehaviorScriptInfo wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_Session->Api->SetScriptActive(
                m_Session->Handle, m_Handle, active ? 1u : 0u,
                reset ? 1u : 0u, &wire, &status), status);
        if (code != BML_OK)
            return Result<ScriptInfo>::Failure(
                code, Detail::ReadStatus(status));
        if (!Detail::ValidScriptInfo(wire) ||
            !Detail::SameScriptObject(wire.Root, m_Root))
            return Result<ScriptInfo>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<ScriptInfo>::Success(
            Detail::ReadScriptInfo(wire), Detail::ReadStatus(status));
    }

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_BehaviorScript m_Handle = nullptr;
    BML_ObjectRef m_Root{};

    friend class Session;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_SCRIPT_HPP
