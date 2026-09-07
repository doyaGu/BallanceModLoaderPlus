#ifndef BML_BEHAVIOR_EDIT_HPP
#define BML_BEHAVIOR_EDIT_HPP

#include "BML/Behavior/Detail/Hook.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace BML::Behavior {

class Edit;

namespace Detail {
struct EditIdentity {
    mutable Edit *Owner = nullptr;
    mutable std::uint32_t Scope = BML_BEHAVIOR_EDIT_GRAPH;
};
struct PatchSymbols {
    std::shared_ptr<const EditIdentity> Edit;
    std::uint32_t HandleBase = 0;
};
struct GraphEdit {
    std::shared_ptr<SessionState> Session;
    BML_ObjectRef Graph{};
    std::uint64_t Fingerprint = 0;
    std::shared_ptr<const BML::Behavior::Edit> Body;
    std::shared_ptr<const EditIdentity> Symbols;
    int Code = BML_OK;
    Status Failure;
};
struct ScriptEdit {
    std::uint32_t Targets = BML_BEHAVIOR_TARGETS_EACH;
    std::string Script;
    std::shared_ptr<const BML::Behavior::Edit> Body;
    int Code = BML_OK;
    Status Failure;
};
}

class Hook {
public:
    Hook() = default;

    template <class Function,
              class = std::enable_if_t<
                  !std::is_same_v<std::decay_t<Function>, Hook>>>
    Hook(Function &&callback) {
        using Holder = Detail::HookFunction<Function>;
        auto holder = std::make_unique<Holder>(std::forward<Function>(callback));
        auto record = std::make_shared<Detail::HookHolder>();
        record->Function.StructSize = sizeof(record->Function);
        record->Function.State = holder.get();
        record->Function.Retain = &Holder::Retain;
        record->Function.Release = &Holder::Release;
        record->Function.Invoke = &Holder::Invoke;
        (void) holder.release();
        m_Record = std::move(record);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Record != nullptr;
    }

private:
    std::shared_ptr<Detail::HookHolder> m_Record;

    friend class Edit;
};

// A durable authoring intent the Loader owns. Closing the handle retires every
// installation the Plan still holds. A conflict keeps the handle readable;
// retirement continues at later Behavior safe points even if this value dies.
class Plan {
public:
    Plan() = default;
    ~Plan() { (void) Close(); }
    Plan(const Plan &) = delete;
    Plan &operator=(const Plan &) = delete;
    Plan(Plan &&other) noexcept
        : m_Session(std::move(other.m_Session)),
          m_Handle(std::exchange(other.m_Handle, nullptr)) {}
    Plan &operator=(Plan &&other) noexcept {
        if (this != &other) {
            Plan previous(std::move(*this));
            m_Session = std::move(other.m_Session);
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
    }
    [[nodiscard]] Result<PlanInfo> Enable();
    [[nodiscard]] Result<PlanInfo> Disable();
    template <class... More>
    [[nodiscard]] Result<PlanInfo> Replace(
        Detail::ScriptEdit first, More... more) {
        std::vector<Detail::ScriptEdit> edits;
        try {
            edits.reserve(1 + sizeof...(more));
            edits.push_back(std::move(first));
            (edits.push_back(std::move(more)), ...);
        } catch (const std::bad_alloc &) {
            return Result<PlanInfo>::Failure(BML_ERROR_OUT_OF_MEMORY);
        }
        return Replace(std::move(edits));
    }
    // A Plan the Loader accepted is Reconciling until the next frame installs
    // it, so read the state rather than assuming the edit is already live.
    [[nodiscard]] Result<PlanInfo> Info() const {
        if (!*this)
            return Result<PlanInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorPlanInfo wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_Session->Api->ReadPlan(
                m_Session->Handle, m_Handle, &wire, &status),
            status);
        if (code != BML_OK)
            return Result<PlanInfo>::Failure(code, Detail::ReadStatus(status));
        if (wire.StructSize < sizeof(wire) ||
            !Detail::KnownPlanState(wire.State) ||
            !Detail::ValidStatus(wire.Diagnostic))
            return Result<PlanInfo>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        return Result<PlanInfo>::Success(Detail::ReadPlanInfo(wire),
                                        Detail::ReadStatus(status));
    }
    // Reverts what the Plan still owns. If a live graph prevents the inverse,
    // the handle remains valid so Read can describe the conflict and Close can
    // be retried after the graph is restored to the expected after-image.
    [[nodiscard]] Result<CloseState> Close() noexcept {
        if (!m_Handle) {
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (!m_Session || !m_Session->Api || !m_Session->Handle)
            return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
        const int code = m_Session->Api->ClosePlan(
            m_Session->Handle, m_Handle);
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
            m_Handle = nullptr;
            m_Session.reset();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (code == BML_ERROR_BUSY)
            return Result<CloseState>::Success(CloseState::Closing);
        return Result<CloseState>::Failure(code);
    }

private:
    [[nodiscard]] Result<PlanInfo> SetActive(bool active);
    [[nodiscard]] Result<PlanInfo> Replace(
        std::vector<Detail::ScriptEdit> edits);
    Plan(std::shared_ptr<Detail::SessionState> session,
         BML_BehaviorPlan handle)
        : m_Session(std::move(session)), m_Handle(handle) {}

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_BehaviorPlan m_Handle = nullptr;

    friend class Session;
};

// One reversible Patch on a specific live graph. It does not follow script
// names into another world; use Plan when the same intent must be reconciled
// again after loading or reset.
class Patch {
public:
    Patch() = default;
    ~Patch() { (void) Close(); }
    Patch(const Patch &) = delete;
    Patch &operator=(const Patch &) = delete;
    Patch(Patch &&other) noexcept
        : m_Session(std::move(other.m_Session)),
          m_Handle(std::exchange(other.m_Handle, nullptr)),
          m_Edits(std::move(other.m_Edits)) {}
    Patch &operator=(Patch &&other) noexcept {
        if (this != &other) {
            Patch previous(std::move(*this));
            m_Session = std::move(other.m_Session);
            m_Handle = std::exchange(other.m_Handle, nullptr);
            m_Edits = std::move(other.m_Edits);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
    }
    [[nodiscard]] Result<PatchInfo> Enable();
    [[nodiscard]] Result<PatchInfo> Disable();
    template <class... More>
    [[nodiscard]] Result<PatchInfo> Replace(
        Detail::GraphEdit first, More... more) {
        std::vector<Detail::GraphEdit> edits;
        try {
            edits.reserve(1 + sizeof...(more));
            edits.push_back(std::move(first));
            (edits.push_back(std::move(more)), ...);
        } catch (const std::bad_alloc &) {
            return Result<PatchInfo>::Failure(BML_ERROR_OUT_OF_MEMORY);
        }
        return Replace(std::move(edits));
    }
    [[nodiscard]] Result<PatchInfo> Info() const {
        if (!*this)
            return Result<PatchInfo>::Failure(BML_ERROR_INVALID_HANDLE);
        BML_BehaviorPatchInfo wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_Session->Api->ReadPatch(
                m_Session->Handle, m_Handle, &wire, &status),
            status);
        if (code != BML_OK)
            return Result<PatchInfo>::Failure(
                code, Detail::ReadStatus(status));
        if (wire.StructSize < sizeof(wire) || wire.Reserved != 0 ||
            !Detail::KnownPatchState(wire.State) ||
            !Detail::ValidStatus(wire.Diagnostic))
            return Result<PatchInfo>::Failure(
                BML_ERROR_MALFORMED_MESSAGE);
        return Result<PatchInfo>::Success(
            Detail::ReadPatchInfo(wire), Detail::ReadStatus(status));
    }
    // Names the live object a node of this edit compiled to, by the handle the
    // edit program used for it. Busy means the Patch has not reached its safe
    // point yet, so nothing is live to name.
private:
    [[nodiscard]] Result<BML_ObjectRef> ResolveHandle(
        std::uint32_t node) const {
        if (!*this)
            return Result<BML_ObjectRef>::Failure(BML_ERROR_INVALID_HANDLE);
        if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface,
                           ResolvePatchNode))
            return Result<BML_ObjectRef>::Failure(BML_ERROR_VERSION_MISMATCH);
        BML_ObjectRef reference{};
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_Session->Api->ResolvePatchNode(
                m_Session->Handle, m_Handle, node, &reference, &status),
            status);
        if (code != BML_OK)
            return Result<BML_ObjectRef>::Failure(
                code, Detail::ReadStatus(status));
        if (!Detail::ValidObjectRef(reference) || !reference.Domain)
            return Result<BML_ObjectRef>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<BML_ObjectRef>::Success(
            reference, Detail::ReadStatus(status));
    }

public:
    // Accepts the symbolic Node returned by Edit::Add or Edit::Require.
    template <class Handle>
    [[nodiscard]] Result<BML_ObjectRef> Resolve(const Handle &node) const;
    // A revert conflict keeps this handle live for Read and a later retry.
    [[nodiscard]] Result<CloseState> Close() noexcept {
        if (!m_Handle) {
            m_Session.reset();
            m_Edits.clear();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (!m_Session || !m_Session->Api || !m_Session->Handle)
            return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
        const int code = m_Session->Api->ClosePatch(
            m_Session->Handle, m_Handle);
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
            m_Handle = nullptr;
            m_Session.reset();
            m_Edits.clear();
            return Result<CloseState>::Success(CloseState::Closed);
        }
        if (code == BML_ERROR_BUSY)
            return Result<CloseState>::Success(CloseState::Closing);
        return Result<CloseState>::Failure(code);
    }

private:
    [[nodiscard]] Result<PatchInfo> SetActive(bool active);
    [[nodiscard]] Result<PatchInfo> Replace(
        std::vector<Detail::GraphEdit> edits);
    Patch(std::shared_ptr<Detail::SessionState> session,
          BML_BehaviorPatch handle,
          std::vector<Detail::PatchSymbols> edits)
        : m_Session(std::move(session)), m_Handle(handle),
          m_Edits(std::move(edits)) {}

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_BehaviorPatch m_Handle = nullptr;
    std::vector<Detail::PatchSymbols> m_Edits;

    friend class BML::Behavior::Graph;
    friend class Session;
};

// A symbolic transformation that can be applied once to a Graph or retained as
// a Plan. Its Node, Port, Link, and Path names exist only inside this Edit.
class Edit {
public:
    Edit() : m_Identity(std::make_shared<Detail::EditIdentity>()) {
        m_Identity->Owner = this;
    }
    class Graph;
    // Addresses one port of a node the program named.
    class Port {
    public:
        Port() = default;

    private:
        Port(std::shared_ptr<const Detail::EditIdentity> edit,
             std::uint32_t scope, std::uint32_t handle,
             std::uint32_t kind, CKGUID type,
             Behavior::Selector slot)
            : m_Edit(std::move(edit)), m_Id(handle), m_Kind(kind),
              m_Scope(scope),
              m_Type(type), m_Slot(std::move(slot)) {}

        std::shared_ptr<const Detail::EditIdentity> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;
        // A BML_BehaviorSlotKind, or zero when Handle is an appended slot.
        std::uint32_t m_Kind = 0;
        CKGUID m_Type{0, 0};
        Behavior::Selector m_Slot;

        friend class Edit;
    };

    // A node of the matched script, either required or added by the program.
    class Node {
    public:
        Node() = default;

        [[nodiscard]] Port In(Behavior::Selector slot = {}) const {
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_IN, CKGUID(0, 0),
                    std::move(slot)};
        }
        [[nodiscard]] Port In(std::int32_t index) const {
            return In(Behavior::Selector::At(index));
        }
        [[nodiscard]] Port In(std::string_view name) const {
            return In(Behavior::Selector::Unique(name));
        }
        [[nodiscard]] Port Out(Behavior::Selector slot = {}) const {
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_OUT, CKGUID(0, 0),
                    std::move(slot)};
        }
        [[nodiscard]] Port Out(std::int32_t index) const {
            return Out(Behavior::Selector::At(index));
        }
        [[nodiscard]] Port Out(std::string_view name) const {
            return Out(Behavior::Selector::Unique(name));
        }
        [[nodiscard]] Port Pin(Behavior::Selector slot,
                               CKGUID type = CKGUID(0, 0)) const {
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_PIN, type,
                    std::move(slot)};
        }
        [[nodiscard]] Port Pin(std::int32_t index,
                               CKGUID type = CKGUID(0, 0)) const {
            return Pin(Behavior::Selector::At(index), type);
        }
        [[nodiscard]] Port Pin(std::string_view name,
                               CKGUID type = CKGUID(0, 0)) const {
            return Pin(Behavior::Selector::Unique(name), type);
        }
        [[nodiscard]] Port Pout(Behavior::Selector slot,
                                CKGUID type = CKGUID(0, 0)) const {
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_POUT, type,
                    std::move(slot)};
        }
        [[nodiscard]] Port Pout(std::int32_t index,
                                CKGUID type = CKGUID(0, 0)) const {
            return Pout(Behavior::Selector::At(index), type);
        }
        [[nodiscard]] Port Pout(std::string_view name,
                                CKGUID type = CKGUID(0, 0)) const {
            return Pout(Behavior::Selector::Unique(name), type);
        }
        [[nodiscard]] Port Local(Behavior::Selector slot,
                                 CKGUID type = CKGUID(0, 0)) const {
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_LOCAL, type,
                    std::move(slot)};
        }
        [[nodiscard]] Port Local(std::int32_t index,
                                 CKGUID type = CKGUID(0, 0)) const {
            return Local(Behavior::Selector::At(index), type);
        }
        [[nodiscard]] Port Local(std::string_view name,
                                 CKGUID type = CKGUID(0, 0)) const {
            return Local(Behavior::Selector::Unique(name), type);
        }
        [[nodiscard]] Port Target() const {
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_TARGET, CKGUID(0, 0), {}};
        }
        [[nodiscard]] Edit::Graph Graph() const;

    private:
        Node(std::shared_ptr<const Detail::EditIdentity> edit,
              std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id),
              m_Scope(m_Edit ? m_Edit->Scope : 0) {}
        Node(std::shared_ptr<const Detail::EditIdentity> edit,
             std::uint32_t scope, std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id), m_Scope(scope) {}

        std::shared_ptr<const Detail::EditIdentity> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;

        friend class Edit;
        friend class Patch;
    };

    // One behavior link of the matched script.
    class Link {
    public:
        Link() = default;

    private:
        Link(std::shared_ptr<const Detail::EditIdentity> edit,
              std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id),
              m_Scope(m_Edit ? m_Edit->Scope : 0) {}
        std::shared_ptr<const Detail::EditIdentity> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;

        friend class Edit;
    };

    // The unique non-branching chain leaving one port, which is where a Hook
    // that must run after a whole sequence belongs.
    class Path {
    public:
        Path() = default;

    private:
        Path(std::shared_ptr<const Detail::EditIdentity> edit,
              std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id),
              m_Scope(m_Edit ? m_Edit->Scope : 0) {}
        std::shared_ptr<const Detail::EditIdentity> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;

        friend class Edit;
    };

    // A slot the program appended. An appended slot has no author-visible index
    // until the edit compiles, so it is addressed by handle instead.
    class Slot {
    public:
        Slot() = default;

        [[nodiscard]] Port Ref() const {
            return Port{m_Edit, m_Scope, m_Id, 0, CKGUID(0, 0), {}};
        }
        operator Port() const { return Ref(); }

    private:
        Slot(std::shared_ptr<const Detail::EditIdentity> edit,
              std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id),
              m_Scope(m_Edit ? m_Edit->Scope : 0) {}
        std::shared_ptr<const Detail::EditIdentity> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;

        friend class Edit;
    };

    // A CKParameterOperation declared by this Edit. It has no control-flow
    // ports: Input addresses one of its parameter inputs and Result is the
    // lazily evaluated output parameter.
    class Operation {
    public:
        Operation() = default;

        [[nodiscard]] Port Input(std::int32_t index) const {
            const CKGUID type = index == 0 ? m_Input1 :
                index == 1 ? m_Input2 : CKGUID(0, 0);
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_PIN, type,
                    Behavior::Selector::At(index)};
        }
        [[nodiscard]] Port Result() const {
            return {m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_POUT, m_Result,
                    Behavior::Selector::At(0)};
        }

    private:
        Operation(std::shared_ptr<const Detail::EditIdentity> edit,
                  std::uint32_t id, CKGUID result, CKGUID input1,
                  CKGUID input2)
            : m_Edit(std::move(edit)), m_Id(id), m_Result(result),
              m_Scope(m_Edit ? m_Edit->Scope : 0),
              m_Input1(input1), m_Input2(input2) {}

        std::shared_ptr<const Detail::EditIdentity> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;
        CKGUID m_Result{0, 0};
        CKGUID m_Input1{0, 0};
        CKGUID m_Input2{0, 0};

        friend class Edit;
    };

    // One graph scope inside this Edit. Every symbol produced by a scope is
    // local to that graph; connecting it to a different scope is rejected
    // before the program crosses the C seam.
    class Graph {
    public:
        Graph() = default;

        [[nodiscard]] Node Root() const {
            return Node{m_Edit, m_Scope, BML_BEHAVIOR_EDIT_GRAPH};
        }
        [[nodiscard]] Node Require(
            Behavior::Selector selector,
            CKGUID prototype = CKGUID(0, 0)) const {
            return With([&](Edit &edit) {
                return edit.Require(std::move(selector), prototype);
            });
        }
        [[nodiscard]] Node Require(
            std::string_view name,
            CKGUID prototype = CKGUID(0, 0)) const {
            return Require(Behavior::Unique(name), prototype);
        }
        [[nodiscard]] Node Require(CKGUID prototype) const {
            return Require(Behavior::Selector::Only(), prototype);
        }
        [[nodiscard]] Node Require(const Behavior::Node &node) const {
            return With([&](Edit &edit) { return edit.Require(node); });
        }
        [[nodiscard]] Link Require(const Behavior::Link &link) const {
            return With([&](Edit &edit) { return edit.Require(link); });
        }
        [[nodiscard]] Node Use(const Behavior::Node &node) const {
            return With([&](Edit &edit) { return edit.Use(node); });
        }
        [[nodiscard]] Link Use(const Behavior::Link &link) const {
            return With([&](Edit &edit) { return edit.Use(link); });
        }
        [[nodiscard]] Link Between(Port source, Port sink) const {
            return With([&](Edit &edit) {
                return edit.Between(std::move(source), std::move(sink));
            });
        }
        [[nodiscard]] Link Between(Port source, Port sink,
                                   std::int32_t delay) const {
            return With([&](Edit &edit) {
                return edit.Between(
                    std::move(source), std::move(sink), delay);
            });
        }
        [[nodiscard]] Path Follow(Port source) const {
            return With([&](Edit &edit) {
                return edit.Follow(std::move(source));
            });
        }
        [[nodiscard]] Node Add(const Block &block) const {
            return With([&](Edit &edit) { return edit.Add(block); });
        }
        [[nodiscard]] Node AddGraph(std::string_view name,
                                    std::int32_t priority = 0) const {
            return With([&](Edit &edit) {
                return edit.AddGraph(name, priority);
            });
        }
        [[nodiscard]] Node Replace(Node target, const Block &block) const {
            return With([&](Edit &edit) {
                return edit.Replace(std::move(target), block);
            });
        }
        Graph &Remove(Node target) {
            With([&](Edit &edit) { edit.Remove(std::move(target)); });
            return *this;
        }
        [[nodiscard]] Operation AddOperation(
            CKGUID operation, CKGUID result,
            CKGUID input1 = CKGUID(0, 0),
            CKGUID input2 = CKGUID(0, 0)) const {
            return With([&](Edit &edit) {
                return edit.AddOperation(operation, result, input1, input2);
            });
        }
        [[nodiscard]] Slot AppendIn(std::string_view name) const {
            return With([&](Edit &edit) { return edit.AppendIn(Root(), name); });
        }
        [[nodiscard]] Slot AppendOut(std::string_view name) const {
            return With([&](Edit &edit) { return edit.AppendOut(Root(), name); });
        }
        [[nodiscard]] Slot AppendPin(std::string_view name, CKGUID type) const {
            return With([&](Edit &edit) {
                return edit.AppendPin(Root(), name, type);
            });
        }
        [[nodiscard]] Slot AppendPout(std::string_view name, CKGUID type) const {
            return With([&](Edit &edit) {
                return edit.AppendPout(Root(), name, type);
            });
        }
        [[nodiscard]] Slot AppendLocal(std::string_view name, CKGUID type) const {
            return With([&](Edit &edit) {
                return edit.AppendLocal(Root(), name, type);
            });
        }
        Graph &Flow(Port source, Port sink, std::int32_t delay = 0) {
            With([&](Edit &edit) {
                edit.Flow(std::move(source), std::move(sink), delay);
            });
            return *this;
        }
        Graph &FlowCycle(Port source, Port sink, std::int32_t delay = 0) {
            With([&](Edit &edit) {
                edit.FlowCycle(std::move(source), std::move(sink), delay);
            });
            return *this;
        }
        Graph &Bind(Port sink, Behavior::Value value) {
            With([&](Edit &edit) {
                edit.Bind(std::move(sink), std::move(value));
            });
            return *this;
        }
        template <class T,
                  class = std::enable_if_t<
                      !std::is_same_v<std::decay_t<T>, Behavior::Value> &&
                      !std::is_same_v<std::decay_t<T>, Port>>>
        Graph &Bind(Port sink, T &&value) {
            return Bind(std::move(sink),
                        Behavior::Value(std::forward<T>(value)));
        }
        Graph &Bind(Port sink, Port source) {
            With([&](Edit &edit) {
                edit.Bind(std::move(sink), std::move(source));
            });
            return *this;
        }
        Graph &Share(Port sink, Port source) {
            With([&](Edit &edit) {
                edit.Share(std::move(sink), std::move(source));
            });
            return *this;
        }
        Graph &Push(Port source, Port sink) {
            With([&](Edit &edit) {
                edit.Push(std::move(source), std::move(sink));
            });
            return *this;
        }
        Graph &Tap(Port source, Hook hook) {
            With([&](Edit &edit) {
                edit.Tap(std::move(source), std::move(hook));
            });
            return *this;
        }
        Graph &Before(Link link, Hook hook) {
            With([&](Edit &edit) {
                edit.Before(std::move(link), std::move(hook));
            });
            return *this;
        }
        Graph &After(Path path, Hook hook) {
            With([&](Edit &edit) {
                edit.After(std::move(path), std::move(hook));
            });
            return *this;
        }
        Graph &After(Port source, Hook hook) {
            With([&](Edit &edit) {
                edit.After(std::move(source), std::move(hook));
            });
            return *this;
        }
        Graph &Splice(Link link, Node through,
                      std::vector<PatchOrder> ordering = {}) {
            With([&](Edit &edit) {
                edit.Splice(std::move(link), std::move(through),
                            std::move(ordering));
            });
            return *this;
        }
        Graph &Splice(Link link, Port sink, Port source,
                      std::vector<PatchOrder> ordering = {}) {
            With([&](Edit &edit) {
                edit.Splice(std::move(link), std::move(sink),
                            std::move(source), std::move(ordering));
            });
            return *this;
        }
        Graph &Redirect(Link link, Port sink,
                        std::vector<PatchOrder> ordering = {}) {
            With([&](Edit &edit) {
                edit.Redirect(std::move(link), std::move(sink),
                              std::move(ordering));
            });
            return *this;
        }

    private:
        Graph(std::shared_ptr<const Detail::EditIdentity> edit,
              std::uint32_t scope) : m_Edit(std::move(edit)), m_Scope(scope) {}

        template <class Function>
        decltype(auto) With(Function &&function) const {
            Edit *edit = m_Edit ? m_Edit->Owner : nullptr;
            if (!edit)
                throw std::logic_error("The Behavior Edit no longer exists.");
            return edit->InScope(m_Scope, std::forward<Function>(function));
        }

        std::shared_ptr<const Detail::EditIdentity> m_Edit;
        std::uint32_t m_Scope = 0;
        friend class Edit;
        friend class Node;
    };

    Edit(const Edit &) = delete;
    Edit &operator=(const Edit &) = delete;
    Edit(Edit &&other) noexcept;
    Edit &operator=(Edit &&other) noexcept;
    ~Edit();

    [[nodiscard]] Graph Root() const noexcept {
        return Graph{m_Identity, BML_BEHAVIOR_EDIT_GRAPH};
    }

    // Names the one node carrying this name, and this Prototype when one is
    // given. No match, or more than one, leaves the Plan Unsatisfied rather
    // than installing a guess.
    [[nodiscard]] Node Require(
        Behavior::Selector selector,
        CKGUID prototype = CKGUID(0, 0)) {
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_NODE);
        step.Selector = std::move(selector);
        step.PrototypeRef = Prototype(prototype);
        return Node{m_Identity, step.Result};
    }
    [[nodiscard]] Node Require(
        std::string_view name, CKGUID prototype = CKGUID(0, 0)) {
        return Require(Behavior::Unique(name), prototype);
    }
    [[nodiscard]] Node Require(CKGUID prototype) {
        return Require(Behavior::Selector::Only(), prototype);
    }
    [[nodiscard]] Node Require(const Behavior::Node &node) {
        if (!node) {
            RejectLiveView("Node");
            return {};
        }
        if (node.Index() < 0)
            return Node{m_Identity, BML_BEHAVIOR_EDIT_GRAPH};
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_NODE);
        step.Selector = Behavior::At(node.Index());
        step.Name.assign(node.Name());
        step.PrototypeRef = Prototype(node.Prototype());
        step.ExpectedKind = node.Kind();
        step.PortShape = Shape(node);
        return Node{m_Identity, step.Result};
    }
    [[nodiscard]] Link Require(const Behavior::Link &link) {
        if (!link) {
            RejectLiveView("Link");
            return {};
        }
        const Behavior::Port source = link.Source();
        const Behavior::Port sink = link.Target();
        const Behavior::Node sourceNode{
            source.m_Graph, source.m_Graph->Ports[source.m_Index].Node};
        const Behavior::Node sinkNode{
            sink.m_Graph, sink.m_Graph->Ports[sink.m_Index].Node};
        Node from = Require(sourceNode);
        Node to = Require(sinkNode);
        const auto slot = [](const Behavior::Port &port) {
            return port.Name().empty()
                ? Behavior::At(port.Index())
                : Behavior::Named(port.Name(), port.Occurrence());
        };
        return Between(from.Out(slot(source)), to.In(slot(sink)),
                       link.InitialDelay());
    }
    // Names a node this Mod already holds a reference to, instead of searching
    // the graph for it. Only a Patch can carry this; a Plan installs into
    // scripts that do not exist yet, so it refuses a live reference.
    [[nodiscard]] Node Use(const Behavior::Node &node) {
        if (!node) {
            RejectLiveView("Node");
            return {};
        }
        Step &step = Define(BML_BEHAVIOR_EDIT_USE_NODE);
        step.Object = node.Object();
        return Node{m_Identity, step.Result};
    }
    // Names a behavior link this Mod already holds a reference to.
    [[nodiscard]] Link Use(const Behavior::Link &link) {
        if (!link) {
            RejectLiveView("Link");
            return {};
        }
        Step &step = Define(BML_BEHAVIOR_EDIT_USE_LINK);
        step.Object = link.Object();
        return Link{m_Identity, step.Result};
    }
    // Names the one existing link between these ports.
    [[nodiscard]] Link Between(Port source, Port sink) {
        if (!Require(source, "Flow source") ||
            !Require(sink, "Flow sink"))
            return {};
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_LINK);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        return Link{m_Identity, step.Result};
    }
    // Names the one existing link between these ports that also carries this
    // delay, in frames.
    [[nodiscard]] Link Between(Port source, Port sink, std::int32_t delay) {
        if (!Require(source, "Flow source") ||
            !Require(sink, "Flow sink"))
            return {};
        Step &step = Define(BML_BEHAVIOR_EDIT_REQUIRE_LINK);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        step.Flags |= BML_BEHAVIOR_EDIT_HAS_DELAY;
        step.Delay = delay;
        return Link{m_Identity, step.Result};
    }
    [[nodiscard]] Path Follow(Port source) {
        if (!Require(source, "Path source"))
            return {};
        Step &step = Define(BML_BEHAVIOR_EDIT_FOLLOW);
        step.Source = std::move(source);
        return Path{m_Identity, step.Result};
    }
    // Copies one configured Block into this symbolic transformation. Later
    // changes to the source Block cannot change this Edit.
    [[nodiscard]] Node Add(const Block &block) {
        const Prototype fallback = block.m_State
            ? block.m_State->Spec.PrototypeRef : Prototype{};
        auto compiled = block.Compile();
        if (!compiled) {
            if (m_Code == BML_OK) {
                m_Code = compiled.Code();
                m_Status = compiled.GetStatus();
            }
            Step &failed = Define(BML_BEHAVIOR_EDIT_ADD_BLOCK);
            failed.PrototypeRef = fallback;
            return Node{m_Identity, failed.Result};
        }
        if (m_Session && m_Session != block.m_Session && m_Code == BML_OK) {
            m_Code = BML_ERROR_INVALID_PARAMETER;
            m_Status.Error = Behavior::Error::OwnerUnavailable;
            m_Status.Phase = Behavior::Phase::Edit;
            m_Status.Message =
                "Every Block in an Edit must belong to the same Session.";
        } else if (!m_Session) {
            m_Session = block.m_Session;
        }

        const std::shared_ptr<const Detail::CompiledBlock> compiledBlock =
            compiled.Value();
        if (!compiledBlock->Spec.PrototypeRef.Generation &&
            m_Code == BML_OK) {
            m_Code = BML_ERROR_UNAVAILABLE;
            m_Status.Error = Behavior::Error::Unavailable;
            m_Status.Phase = Behavior::Phase::Prototype;
            m_Status.Prototype = compiledBlock->Spec.PrototypeRef.Id;
            m_Status.Message =
                "A Block must resolve its Prototype provider before it can be added to an Edit.";
        }
        Step &step = Define(BML_BEHAVIOR_EDIT_ADD_BLOCK);
        step.Block = compiledBlock;
        const Node node{m_Identity, step.Result};
        return node;
    }
    [[nodiscard]] Node AddGraph(std::string_view name,
                                std::int32_t priority = 0) {
        if (name.empty()) {
            if (m_Code == BML_OK) {
                m_Code = BML_ERROR_INVALID_PARAMETER;
                m_Status.Error = Behavior::Error::ValueInvalid;
                m_Status.Phase = Behavior::Phase::Edit;
                m_Status.Message = "A graph-backed Node requires a name.";
            }
            return {};
        }
        Step &step = Define(BML_BEHAVIOR_EDIT_ADD_GRAPH);
        step.Name.assign(name);
        step.Priority = priority;
        return Node{m_Identity, step.Result};
    }
    // Replaces an existing child Node while preserving its public graph
    // relations. The replacement Block owns its own Settings and Locals.
    // Closing the Patch restores the exact original Node.
    [[nodiscard]] Node Replace(Node target, const Block &block) {
        if (!Require(target, "Replacement target"))
            return {};
        Node replacement = Add(block);
        if (!m_Steps.empty()) {
            Step &step = m_Steps.back();
            step.Kind = BML_BEHAVIOR_EDIT_REPLACE_BLOCK;
            step.Target = target.m_Id;
        }
        return replacement;
    }
    // Removes an existing child Node and its incident control-flow Links for
    // the lifetime of the Patch. The Links are disconnected while parked; the
    // exact native objects, endpoints, and delays are restored when the Patch
    // closes. Remove never destroys the Node. Unrelated Nodes in the graph may
    // remain active, but this Node, its control ports, and every incident Link
    // source must be idle, with no in-flight delay.
    Edit &Remove(Node target) {
        if (!Require(target, "Removal target"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_REMOVE_NODE, 0);
        step.Target = target.m_Id;
        return *this;
    }
    // Adds one native Parameter Operation to the graph. Virtools chooses the
    // concrete function from the operation GUID and this exact type tuple.
    [[nodiscard]] Operation AddOperation(
        CKGUID operation, CKGUID result, CKGUID input1 = CKGUID(0, 0),
        CKGUID input2 = CKGUID(0, 0)) {
        Step &step = Define(BML_BEHAVIOR_EDIT_ADD_OPERATION);
        step.Operation = operation;
        step.OperationResult = result;
        step.OperationInput1 = input1;
        step.OperationInput2 = input2;
        return Operation{m_Identity, step.Result, result, input1, input2};
    }
    [[nodiscard]] Slot AppendIn(Node owner, std::string_view name) {
        return Append(owner, BML_BEHAVIOR_SLOT_IN, name, CKGUID(0, 0));
    }
    [[nodiscard]] Slot AppendOut(Node owner, std::string_view name) {
        return Append(owner, BML_BEHAVIOR_SLOT_OUT, name, CKGUID(0, 0));
    }
    [[nodiscard]] Slot AppendPin(Node owner, std::string_view name, CKGUID type) {
        return Append(owner, BML_BEHAVIOR_SLOT_PIN, name, type);
    }
    [[nodiscard]] Slot AppendPout(Node owner, std::string_view name, CKGUID type) {
        return Append(owner, BML_BEHAVIOR_SLOT_POUT, name, type);
    }
    // A Local belongs to the private state of a graph or Block. It can be
    // added to Graph(), or to a Block created by this Edit, never to a
    // borrowed child Node.
    [[nodiscard]] Slot AppendLocal(Node owner, std::string_view name,
                                   CKGUID type) {
        return Append(owner, BML_BEHAVIOR_SLOT_LOCAL, name, type);
    }

    // Adds a behavior link, delayed by whole frames.
    Edit &Flow(Port source, Port sink, std::int32_t delay = 0) {
        if (!Require(source, "Flow source") ||
            !Require(sink, "Flow sink"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_FLOW, 0);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        step.Delay = delay;
        return *this;
    }
    // Adds a behavior link that may close a same-frame cycle. Without this the
    // Loader rejects a cycle instead of installing one.
    Edit &FlowCycle(Port source, Port sink, std::int32_t delay = 0) {
        if (!Require(source, "Flow source") ||
            !Require(sink, "Flow sink"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_FLOW, 0);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        step.Delay = delay;
        step.Flags |= BML_BEHAVIOR_EDIT_CONFIRM_CYCLE;
        return *this;
    }
    // Writes an owned literal into a port. World-bound object values are not
    // part of the symbolic edit language.
    Edit &Bind(Port sink, Behavior::Value value) {
        if (!Require(sink, "Bind sink"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_BIND_VALUE, 0);
        step.Sink = std::move(sink);
        step.Value.emplace(std::move(value));
        return *this;
    }
    // Makes the sink read the source directly.
    Edit &Bind(Port sink, Port source) {
        if (!Require(sink, "Bind sink") ||
            !Require(source, "Bind source"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_BIND_PORT, 0);
        step.Sink = std::move(sink);
        step.Source = std::move(source);
        return *this;
    }
    // Makes the sink share the parameter the source reads.
    Edit &Share(Port sink, Port source) {
        if (!Require(sink, "Share sink") ||
            !Require(source, "Share source"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_SHARE, 0);
        step.Sink = std::move(sink);
        step.Source = std::move(source);
        return *this;
    }
    // Copies the source into the sink after each execution of the node that
    // owns the source.
    Edit &Push(Port source, Port sink) {
        if (!Require(source, "Push source") ||
            !Require(sink, "Push sink"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_PUSH, 0);
        step.Source = std::move(source);
        step.Sink = std::move(sink);
        return *this;
    }
    // Runs the callback on every link leaving this port, before whatever those
    // links reach. The Hook Block activates its Out once the callback returns.
    Edit &Tap(Port source, Hook hook) {
        if (!Require(source, "Tap source"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_TAP, 0);
        step.Source = std::move(source);
        step.Hook = std::move(hook.m_Record);
        return *this;
    }
    // Runs the callback on this link, before the node the link feeds. This is
    // how a Mod puts its own code between two blocks it did not write.
    Edit &Before(Link target, Hook hook) {
        if (!Require(target, "Before Link"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_BEFORE, 0);
        step.Target = target.m_Id;
        step.Hook = std::move(hook.m_Record);
        return *this;
    }
    // Runs the callback once the chain named by the path has finished.
    Edit &After(Path path, Hook hook) {
        if (!Require(path, "After Path"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_AFTER, 0);
        step.Target = path.m_Id;
        step.Hook = std::move(hook.m_Record);
        return *this;
    }
    // Runs the callback once the chain leaving this port has finished.
    Edit &After(Port source, Hook hook) {
        return After(Follow(std::move(source)), std::move(hook));
    }
    // Reroutes a link through a Block, keeping the delay of the link. Ordering
    // places this Patch relative to the Patches of other Mods spliced onto the
    // same link; a Patch no one submitted constrains nothing.
    Edit &Splice(Link link, Node through,
                  std::vector<PatchOrder> ordering = {}) {
        if (!Require(link, "Splice Link") ||
            !Require(through, "Splice Block"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_SPLICE, 0);
        step.Target = link.m_Id;
        step.Node = through.m_Id;
        step.Ordering = std::move(ordering);
        return *this;
    }
    // Reroutes a link into the sink and out of the source, which is how one
    // Block with several Ins and Outs carries more than one splice.
    Edit &Splice(Link link, Port sink, Port source,
                  std::vector<PatchOrder> ordering = {}) {
        if (!Require(link, "Splice Link") ||
            !Require(sink, "Splice input") ||
            !Require(source, "Splice output"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_SPLICE, 0);
        step.Target = link.m_Id;
        step.Sink = std::move(sink);
        step.Source = std::move(source);
        step.Ordering = std::move(ordering);
        return *this;
    }
    // Sends a link to a different destination. The original destination is
    // dropped while this Patch is open and restored when it closes, so unlike
    // a splice this shows up in the Logical view of the graph. Only one Patch
    // at a time may redirect one link.
    Edit &Redirect(Link link, Port sink,
                    std::vector<PatchOrder> ordering = {}) {
        if (!Require(link, "Redirect Link") ||
            !Require(sink, "Redirect sink"))
            return *this;
        Step &step = Define(BML_BEHAVIOR_EDIT_REDIRECT, 0);
        step.Target = link.m_Id;
        step.Sink = std::move(sink);
        step.Ordering = std::move(ordering);
        return *this;
    }

private:
    struct Step {
        std::uint32_t Kind = 0;
        std::uint32_t Graph = BML_BEHAVIOR_EDIT_GRAPH;
        std::uint32_t Result = 0;
        std::uint32_t Flags = 0;
        std::uint32_t Target = 0;
        std::uint32_t Node = 0;
        std::uint32_t SlotKind = 0;
        std::int32_t Delay = 0;
        std::int32_t Priority = 0;
        std::string Name;
        Behavior::Selector Selector;
        Behavior::Prototype PrototypeRef;
        Behavior::BehaviorKind ExpectedKind = Behavior::BehaviorKind::Function;
        std::uint64_t PortShape = 0;
        std::shared_ptr<const Detail::CompiledBlock> Block;
        CKGUID Type{0, 0};
        Port Source;
        Port Sink;
        std::optional<Behavior::Value> Value;
        std::shared_ptr<Detail::HookHolder> Hook;
        std::vector<PatchOrder> Ordering;
        BML_ObjectRef Object{};
        CKGUID Operation{0, 0};
        CKGUID OperationResult{0, 0};
        CKGUID OperationInput1{0, 0};
        CKGUID OperationInput2{0, 0};
    };

    struct WireProgram {
        std::vector<BML_BehaviorEditOrder> Ordering;
        std::vector<BML_BehaviorEditStep> Steps;
    };

    Step &Define(std::uint32_t kind) {
        return Define(kind, m_NextHandle++);
    }
    Step &Define(std::uint32_t kind, std::uint32_t result) {
        Step step;
        step.Kind = kind;
        step.Graph = m_Identity->Scope;
        step.Result = result;
        m_Steps.push_back(std::move(step));
        return m_Steps.back();
    }
    Slot Append(Node owner, std::uint32_t slotKind, std::string_view name,
                CKGUID type) {
        if (!Require(owner, "Appended port owner"))
            return {};
        Step &step = Define(BML_BEHAVIOR_EDIT_APPEND_SLOT);
        step.Target = owner.m_Id;
        step.SlotKind = slotKind;
        step.Name.assign(name);
        step.Type = type;
        return Slot{m_Identity, step.Result};
    }
    template <class Symbol>
    [[nodiscard]] bool Require(const Symbol &symbol,
                               std::string_view role) {
        if (symbol.m_Edit == m_Identity && symbol.m_Id != 0 &&
            symbol.m_Scope == m_Identity->Scope)
            return true;
        if (m_Code == BML_OK) {
            m_Code = BML_ERROR_INVALID_PARAMETER;
            m_Status.Error = Behavior::Error::GraphLocalityInvalid;
            m_Status.Phase = Behavior::Phase::Edit;
            m_Status.Message = std::string(role) +
                " belongs to a different Behavior Edit.";
        }
        return false;
    }
    void RejectLiveView(std::string_view kind) {
        if (m_Code != BML_OK)
            return;
        m_Code = BML_ERROR_INVALID_PARAMETER;
        m_Status.Error = Behavior::Error::GraphLocalityInvalid;
        m_Status.Phase = Behavior::Phase::Edit;
        m_Status.Message = "An empty Graph " + std::string(kind) +
            " cannot be used by a Behavior Edit.";
    }
    [[nodiscard]] Graph EnterGraph(const Node &node) {
        if (!Require(node, "Nested graph"))
            return {};
        Step &step = Define(BML_BEHAVIOR_EDIT_ENTER_GRAPH);
        step.Target = node.m_Id;
        const std::uint32_t scope = step.Result;
        return Graph{m_Identity, scope};
    }
    template <class Function>
    decltype(auto) InScope(std::uint32_t scope, Function &&function) {
        struct Restore {
            const Detail::EditIdentity &Identity;
            std::uint32_t Previous;
            ~Restore() { Identity.Scope = Previous; }
        } restore{*m_Identity, m_Identity->Scope};
        m_Identity->Scope = scope;
        return std::forward<Function>(function)(*this);
    }
    [[nodiscard]] static std::uint64_t Shape(const Behavior::Node &node) {
        constexpr std::uint64_t offset = 1469598103934665603ull;
        constexpr std::uint64_t prime = 1099511628211ull;
        std::uint64_t hash = offset;
        const auto append = [&](const void *data, std::size_t size) {
            const auto *bytes = static_cast<const unsigned char *>(data);
            for (std::size_t index = 0; index < size; ++index) {
                hash ^= bytes[index];
                hash *= prime;
            }
        };
        for (Behavior::Port port : node.Ports()) {
            const auto kind = static_cast<std::uint32_t>(port.Kind());
            const auto index = port.Index();
            const auto occurrence = port.Occurrence();
            const CKGUID type = port.Type();
            append(&kind, sizeof(kind));
            append(&index, sizeof(index));
            append(&occurrence, sizeof(occurrence));
            append(&type.d1, sizeof(type.d1));
            append(&type.d2, sizeof(type.d2));
            const std::string_view name = port.Name();
            append(name.data(), name.size());
        }
        return hash;
    }
    void Encode(WireProgram &out) const;
    [[nodiscard]] std::shared_ptr<const Edit> Snapshot() const {
        auto copy = std::make_shared<Edit>();
        copy->m_Session = m_Session;
        copy->m_Code = m_Code;
        copy->m_Status = m_Status;
        copy->m_NextHandle = m_NextHandle;
        copy->m_Steps = m_Steps;
        return copy;
    }

    std::shared_ptr<Detail::SessionState> m_Session;
    std::shared_ptr<const Detail::EditIdentity> m_Identity;
    int m_Code = BML_OK;
    Status m_Status;
    std::uint32_t m_NextHandle = BML_BEHAVIOR_EDIT_GRAPH + 1u;
    std::vector<Step> m_Steps;

    [[nodiscard]] Result<void> Validate(
        const std::shared_ptr<Detail::SessionState> &session,
        bool durable = false) const;

    friend class BML::Behavior::Graph;
    friend class Session;
    friend class Patch;
    friend class Plan;
    friend Detail::GraphEdit On(const BML::Behavior::Graph &, const Edit &);
    friend Detail::ScriptEdit On(const Scripts &, const Edit &);
};

inline Edit::Edit(Edit &&other) noexcept
    : m_Session(std::move(other.m_Session)),
      m_Identity(std::move(other.m_Identity)), m_Code(other.m_Code),
      m_Status(std::move(other.m_Status)), m_NextHandle(other.m_NextHandle),
      m_Steps(std::move(other.m_Steps)) {
    if (m_Identity) {
        m_Identity->Owner = this;
        m_Identity->Scope = BML_BEHAVIOR_EDIT_GRAPH;
    }
}

inline Edit &Edit::operator=(Edit &&other) noexcept {
    if (this == &other)
        return *this;
    if (m_Identity && m_Identity->Owner == this)
        m_Identity->Owner = nullptr;
    m_Session = std::move(other.m_Session);
    m_Identity = std::move(other.m_Identity);
    m_Code = other.m_Code;
    m_Status = std::move(other.m_Status);
    m_NextHandle = other.m_NextHandle;
    m_Steps = std::move(other.m_Steps);
    if (m_Identity) {
        m_Identity->Owner = this;
        m_Identity->Scope = BML_BEHAVIOR_EDIT_GRAPH;
    }
    return *this;
}

inline Edit::~Edit() {
    if (m_Identity && m_Identity->Owner == this)
        m_Identity->Owner = nullptr;
}

inline Edit::Graph Edit::Node::Graph() const {
    Edit *edit = m_Edit ? m_Edit->Owner : nullptr;
    if (!edit)
        return {};
    return edit->InScope(m_Scope, [&](Edit &owner) {
        return owner.EnterGraph(*this);
    });
}

class Scripts {
public:
    [[nodiscard]] static Scripts Each(std::string_view name) {
        return Scripts(BML_BEHAVIOR_TARGETS_EACH, name);
    }
    [[nodiscard]] static Scripts One(std::string_view name) {
        return Scripts(BML_BEHAVIOR_TARGETS_ONE, name);
    }

private:
    Scripts(std::uint32_t count, std::string_view name)
        : m_Count(count), m_Name(name) {}

    std::uint32_t m_Count = BML_BEHAVIOR_TARGETS_EACH;
    std::string m_Name;

    friend class Session;
    friend Detail::ScriptEdit On(const Scripts &, const Edit &);
};

inline Detail::GraphEdit On(const Graph &graph, const Edit &edit) {
    Detail::GraphEdit target;
    target.Session = graph.m_Session;
    target.Graph = graph.m_Root;
    target.Fingerprint = graph.m_Fingerprint;
    target.Symbols = edit.m_Identity;
    if (graph.m_View != View::Logical || !target.Graph.Domain) {
        target.Code = BML_ERROR_INVALID_PARAMETER;
        target.Failure.Error = Error::GraphLocalityInvalid;
        target.Failure.Phase = Phase::Edit;
        target.Failure.Message =
            "A Patch target must be a logical Graph snapshot.";
        return target;
    }
    try {
        target.Body = edit.Snapshot();
    } catch (const std::bad_alloc &) {
        target.Code = BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        target.Code = BML_ERROR_FAIL;
    }
    return target;
}

inline Detail::ScriptEdit On(const Scripts &scripts, const Edit &edit) {
    Detail::ScriptEdit target;
    target.Targets = scripts.m_Count;
    target.Script = scripts.m_Name;
    try {
        target.Body = edit.Snapshot();
    } catch (const std::bad_alloc &) {
        target.Code = BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        target.Code = BML_ERROR_FAIL;
    }
    return target;
}

template <class Handle>
Result<BML_ObjectRef> Patch::Resolve(const Handle &node) const {
    static_assert(std::is_same_v<std::decay_t<Handle>, Edit::Node>,
                  "A Behavior Patch resolves only an Edit::Node.");
    if (!*this)
        return Result<BML_ObjectRef>::Failure(BML_ERROR_INVALID_HANDLE);
    const auto symbols = std::find_if(
        m_Edits.begin(), m_Edits.end(), [&](const Detail::PatchSymbols &item) {
            return item.Edit == node.m_Edit;
        });
    if (!node.m_Edit || symbols == m_Edits.end() || !node.m_Id ||
        node.m_Id > UINT32_MAX - symbols->HandleBase) {
        Status status;
        status.Error = Behavior::Error::GraphLocalityInvalid;
        status.Phase = Behavior::Phase::Edit;
        status.Message =
            "The symbolic Node belongs to a different Behavior Edit.";
        return Result<BML_ObjectRef>::Failure(BML_ERROR_INVALID_PARAMETER,
                                               std::move(status));
    }
    return ResolveHandle(node.m_Id + symbols->HandleBase);
}

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_EDIT_HPP
