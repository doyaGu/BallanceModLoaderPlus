#ifndef BML_BEHAVIOR_SESSION_HPP
#define BML_BEHAVIOR_SESSION_HPP

#include "BML/Behavior/Script.hpp"

#include <memory>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

namespace BML::Behavior {

class Session {
public:
    Session() = default;
    ~Session() { Close(); }
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Session(Session &&) noexcept = default;
    Session &operator=(Session &&other) noexcept {
        if (this != &other) {
            Close();
            m_State = std::move(other.m_State);
        }
        return *this;
    }

    [[nodiscard]] static Result<Session> Open(std::string_view ownerId = {}) {
        const void *found = nullptr;
        int code = BML_GetInterface(BML_BEHAVIOR_INTERFACE_ID,
                                    BML_BEHAVIOR_INTERFACE_MAJOR, &found);
        if (code != BML_OK)
            return Result<Session>::Failure(code);
        const auto *api = static_cast<const BML_BehaviorInterface *>(found);
        // The facade needs the complete 1.0 contract. Later minor members are
        // probed at their call sites instead of rejecting an older loader here.
        if (!api || !BML_BEHAVIOR_HAS_1_0(api))
            return Result<Session>::Failure(BML_ERROR_VERSION_MISMATCH);

        BML_BehaviorSession handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        code = api->OpenSession(Detail::Text(ownerId), &handle, &status);
        code = Detail::WireCode(code, status);
        struct SessionHandle {
            const BML_BehaviorInterface *Api = nullptr;
            BML_BehaviorSession Value = nullptr;
            ~SessionHandle() {
                if (Api && Value)
                    (void) Api->CloseSession(Value);
            }
        } owned{api, handle};
        try {
            if (code != BML_OK || !handle)
                return Result<Session>::Failure(
                    code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                    Detail::ReadStatus(status));
            Session session;
            session.m_State = std::make_shared<Detail::SessionState>();
            session.m_State->Api = api;
            session.m_State->Handle = handle;
            owned.Value = nullptr;
            return Result<Session>::Success(std::move(session),
                                            Detail::ReadStatus(status));
        } catch (const std::bad_alloc &) {
            return Result<Session>::Failure(BML_ERROR_OUT_OF_MEMORY);
        } catch (...) {
            return Result<Session>::Failure(BML_ERROR_FAIL);
        }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_State && m_State->Api && m_State->Handle;
    }
    [[nodiscard]] Block Use(Prototype prototype) const {
        return Block(m_State, prototype);
    }
    [[nodiscard]] Block Use(CKGUID prototype) const {
        return Use(Prototype(prototype));
    }
    [[nodiscard]] Result<std::vector<PrototypeInfo>> Prototypes(
        const PrototypeQuery &query = {}) const;
    [[nodiscard]] Result<Behavior::Layout> Layout(
        Prototype prototype) const;
    [[nodiscard]] Result<Behavior::Layout> Layout(CKGUID prototype) const {
        return Layout(Prototype(prototype));
    }
    [[nodiscard]] Result<Graph> Inspect(ObjectRef root) const {
        return Graph::Read(m_State, root, View::Logical);
    }
    // Turns a CK object this Mod already holds into an interface reference.
    // This is the entry point for a Mod that received a script from the game
    // instead of searching for one by name.
    [[nodiscard]] Result<ObjectRef> Reference(CK_ID object) const {
        if (!*this)
            return Result<ObjectRef>::Failure(BML_ERROR_INVALID_HANDLE);
        if (!BML_IFACE_HAS(m_State->Api, BML_BehaviorInterface, Reference))
            return Result<ObjectRef>::Failure(BML_ERROR_VERSION_MISMATCH);
        ObjectRef reference{};
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_State->Api->Reference(
                m_State->Handle, static_cast<std::uint32_t>(object),
                &reference, &status),
            status);
        if (code != BML_OK)
            return Result<ObjectRef>::Failure(
                code, Detail::ReadStatus(status));
        if (!Detail::ValidObjectRef(reference) || !reference.Domain)
            return Result<ObjectRef>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<ObjectRef>::Success(
            reference, Detail::ReadStatus(status));
    }
    [[nodiscard]] Result<ObjectRef> Reference(CKObject *object) const {
        if (!object)
            return Result<ObjectRef>::Failure(BML_ERROR_INVALID_PARAMETER);
        return Reference(object->GetID());
    }
    // Reads the logical graph of a script this Mod already holds.
    [[nodiscard]] Result<Graph> Inspect(CKBehavior *graph) const {
        const Result<ObjectRef> reference = Reference(graph);
        if (!reference)
            return Result<Graph>::Failure(reference.Code(), reference.GetStatus());
        return Inspect(reference.Value());
    }
    // Creates the root and installs body before returning one inactive Script.
    // A rejected body publishes no Script and returns no partial handle.
    [[nodiscard]] Result<Behavior::Script> CreateScript(
        ObjectRef owner, std::string_view name, const Edit &body,
        std::int32_t priority = 0) const;
    [[nodiscard]] Result<Behavior::Script> CreateScript(
        CKBeObject *owner, std::string_view name, const Edit &body,
        std::int32_t priority = 0) const {
        const Result<ObjectRef> reference = Reference(owner);
        return reference
            ? CreateScript(reference.Value(), name, body, priority)
            : Result<Behavior::Script>::Failure(
                  reference.Code(), reference.GetStatus());
    }
    // Convenience form for one Script rule. The On(...) overload below retains
    // several rules under one Plan identity.
    [[nodiscard]] Result<Behavior::Plan> Plan(
        std::string_view name, const Scripts &scripts,
        const Edit &edit) const;
    template <class First, class... More>
    [[nodiscard]] Result<Behavior::Patch> Apply(
        std::string_view name, First &&first, More &&...more) const;
    template <class First, class... More,
              std::enable_if_t<
                  !std::is_same_v<std::decay_t<First>, Scripts>, int> = 0>
    [[nodiscard]] Result<Behavior::Plan> Plan(
        std::string_view name, First &&first, More &&...more) const;
    void Close() noexcept {
        // Values created from this Session hold their own lease. Releasing the
        // Session value stops this object from admitting work without
        // invalidating Blocks, Runs, Watches, Plans, or Patches that still own
        // native state. The last lease closes the C Session.
        m_State.reset();
    }

    // The escape hatch to the C ABI, for whatever this facade does not cover
    // yet. Both stay valid until Close, and neither transfers ownership.
    [[nodiscard]] BML_BehaviorSession Handle() const noexcept {
        return m_State ? m_State->Handle : nullptr;
    }
    [[nodiscard]] const BML_BehaviorInterface *Api() const noexcept {
        return m_State ? m_State->Api : nullptr;
    }

private:
    [[nodiscard]] Result<Behavior::Patch> Apply(
        std::string_view name,
        std::vector<Detail::PatchTarget> targets) const;
    [[nodiscard]] static Result<Behavior::Patch> Apply(
        const std::shared_ptr<Detail::SessionState> &session,
        std::string_view name,
        std::vector<Detail::PatchTarget> targets);
    [[nodiscard]] Result<Behavior::Plan> Plan(
        std::string_view name, std::vector<Detail::PlanRule> rules) const;
    std::shared_ptr<Detail::SessionState> m_State;

    friend class Graph;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_SESSION_HPP
