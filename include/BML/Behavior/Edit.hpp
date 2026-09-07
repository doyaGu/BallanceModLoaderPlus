#ifndef BML_BEHAVIOR_EDIT_HPP
#define BML_BEHAVIOR_EDIT_HPP

#include "BML/Behavior/Detail/Hook.hpp"
#include "BML/Behavior/Pattern.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace BML::Behavior {

class Edit;

namespace Detail {
enum class EditContext {
    Patch,
    Plan,
};
struct EditProgram;
struct EditStep;
struct EditWire;
struct PatchSymbols {
    std::weak_ptr<EditProgram> Edit;
    std::uint32_t HandleBase = 0;
};
struct PatchTarget {
    std::shared_ptr<SessionState> Session;
    BML_ObjectRef Graph{};
    std::uint64_t Fingerprint = 0;
    std::shared_ptr<const BML::Behavior::Edit> Body;
    std::weak_ptr<EditProgram> Symbols;
    int Code = BML_OK;
    Status Failure;
};
struct PlanRule {
    std::uint32_t Targets = BML_BEHAVIOR_TARGETS_EACH;
    std::string Script;
    std::shared_ptr<const BML::Behavior::Edit> Body;
    int Code = BML_OK;
    Status Failure;
};
struct PatchWire;
struct PlanWire;
}

class Hook {
public:
    Hook() = default;

    template <class Function,
              class = std::enable_if_t<
                  !std::is_same_v<std::decay_t<Function>, Hook>>>
    Hook(Function &&callback);

    [[nodiscard]] explicit operator bool() const noexcept;

private:
    std::shared_ptr<Detail::HookHolder> m_Record;

    friend class Edit;
};

// A cross-world authoring Plan the Loader owns. Closing the handle retires every
// installation the Plan still holds. A conflict keeps the handle readable;
// retirement continues at later Behavior safe points even if this value dies.
class Plan {
public:
    Plan() = default;
    ~Plan();
    Plan(const Plan &) = delete;
    Plan &operator=(const Plan &) = delete;
    Plan(Plan &&other) noexcept;
    Plan &operator=(Plan &&other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] Result<PlanInfo> Enable();
    [[nodiscard]] Result<PlanInfo> Disable();
    template <class... More>
    [[nodiscard]] Result<PlanInfo> Replace(
        Detail::PlanRule first, More... more);
    // A Plan the Loader accepted is Reconciling until the next frame installs
    // it, so read the state rather than assuming the edit is already live.
    [[nodiscard]] Result<PlanInfo> Info() const;
    // Reverts what the Plan still owns. If a live graph prevents the inverse,
    // the handle remains valid so Read can describe the conflict and Close can
    // be retried after the graph is restored to the expected after-image.
    [[nodiscard]] Result<CloseState> Close() noexcept;

private:
    [[nodiscard]] Result<PlanInfo> SetActive(bool active);
    [[nodiscard]] Result<PlanInfo> Replace(
        std::vector<Detail::PlanRule> rules);
    Plan(std::shared_ptr<Detail::SessionState> session,
         BML_BehaviorPlan handle);

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
    ~Patch();
    Patch(const Patch &) = delete;
    Patch &operator=(const Patch &) = delete;
    Patch(Patch &&other) noexcept;
    Patch &operator=(Patch &&other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] Result<PatchInfo> Enable();
    [[nodiscard]] Result<PatchInfo> Disable();
    template <class... More>
    [[nodiscard]] Result<PatchInfo> Replace(
        Detail::PatchTarget first, More... more);
    [[nodiscard]] Result<PatchInfo> Info() const;
    // Names the live object a node of this edit compiled to, by the handle the
    // edit program used for it. Busy means the Patch has not reached its safe
    // point yet, so nothing is live to name.
private:
    [[nodiscard]] Result<BML_ObjectRef> ResolveHandle(
        std::uint32_t node) const;

public:
    // Accepts the symbolic Node returned by Edit::Add or Edit::Require.
    template <class Handle>
    [[nodiscard]] Result<BML_ObjectRef> Resolve(const Handle &node) const;
    // A revert conflict keeps this handle live for Read and a later retry.
    [[nodiscard]] Result<CloseState> Close() noexcept;

private:
    [[nodiscard]] Result<PatchInfo> SetActive(bool active);
    [[nodiscard]] Result<PatchInfo> Replace(
        std::vector<Detail::PatchTarget> targets);
    Patch(std::shared_ptr<Detail::SessionState> session,
          BML_BehaviorPatch handle,
          std::vector<Detail::PatchSymbols> edits);

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
    Edit();
    class Graph;
    class Node;
    class Nodes;
    class Ports;
    // Addresses one port of a node the program named.
    class Port {
    public:
        Port() = default;

    private:
        Port(std::weak_ptr<Detail::EditProgram> edit,
             std::uint32_t scope, std::uint32_t handle,
             std::uint32_t kind, CKGUID type,
             Behavior::Selector slot)
            : m_Edit(std::move(edit)), m_Id(handle), m_Kind(kind),
              m_Scope(scope),
              m_Type(type), m_Slot(std::move(slot)) {}

        std::weak_ptr<Detail::EditProgram> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;
        // A BML_BehaviorSlotKind, or zero when Handle is an appended slot.
        std::uint32_t m_Kind = 0;
        CKGUID m_Type{0, 0};
        Behavior::Selector m_Slot;

        friend class Edit;
        friend class Ports;
        friend class Nodes;
    };

    // The corresponding port on every Node selected by Graph::Each.
    class Ports {
    public:
        Ports() = default;

    private:
        explicit Ports(Port port) : m_Port(std::move(port)) {}
        Port m_Port;

        friend class Edit;
        friend class Graph;
        friend class Nodes;
    };

    // A stable, non-empty selection of child Nodes. Operations on its Ports
    // are repeated in native child-index order when the Edit is compiled.
    class Nodes {
    public:
        Nodes() = default;

        [[nodiscard]] Ports In(Behavior::Selector slot = {}) const {
            return Ports(Port{m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_IN,
                              CKGUID(0, 0), std::move(slot)});
        }
        [[nodiscard]] Ports In(std::int32_t index) const {
            return In(Behavior::At(index));
        }
        [[nodiscard]] Ports In(std::string_view name) const {
            return In(Behavior::Unique(name));
        }
        [[nodiscard]] Ports Out(Behavior::Selector slot = {}) const {
            return Ports(Port{m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_OUT,
                              CKGUID(0, 0), std::move(slot)});
        }
        [[nodiscard]] Ports Out(std::int32_t index) const {
            return Out(Behavior::At(index));
        }
        [[nodiscard]] Ports Out(std::string_view name) const {
            return Out(Behavior::Unique(name));
        }
        [[nodiscard]] Ports Pin(Behavior::Selector slot,
                                CKGUID type = CKGUID(0, 0)) const {
            return Ports(Port{m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_PIN,
                              type, std::move(slot)});
        }
        [[nodiscard]] Ports Pin(std::int32_t index,
                                CKGUID type = CKGUID(0, 0)) const {
            return Pin(Behavior::At(index), type);
        }
        [[nodiscard]] Ports Pin(std::string_view name,
                                CKGUID type = CKGUID(0, 0)) const {
            return Pin(Behavior::Unique(name), type);
        }
        [[nodiscard]] Ports Pout(Behavior::Selector slot,
                                 CKGUID type = CKGUID(0, 0)) const {
            return Ports(Port{m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_POUT,
                              type, std::move(slot)});
        }
        [[nodiscard]] Ports Pout(std::int32_t index,
                                 CKGUID type = CKGUID(0, 0)) const {
            return Pout(Behavior::At(index), type);
        }
        [[nodiscard]] Ports Pout(std::string_view name,
                                 CKGUID type = CKGUID(0, 0)) const {
            return Pout(Behavior::Unique(name), type);
        }
        [[nodiscard]] Ports Local(Behavior::Selector slot,
                                  CKGUID type = CKGUID(0, 0)) const {
            return Ports(Port{m_Edit, m_Scope, m_Id, BML_BEHAVIOR_SLOT_LOCAL,
                              type, std::move(slot)});
        }
        [[nodiscard]] Ports Local(std::int32_t index,
                                  CKGUID type = CKGUID(0, 0)) const {
            return Local(Behavior::At(index), type);
        }
        [[nodiscard]] Ports Local(std::string_view name,
                                  CKGUID type = CKGUID(0, 0)) const {
            return Local(Behavior::Unique(name), type);
        }
        [[nodiscard]] Ports Target() const {
            return Ports(Port{m_Edit, m_Scope, m_Id,
                              BML_BEHAVIOR_SLOT_TARGET, CKGUID(0, 0), {}});
        }

    private:
        Nodes(std::weak_ptr<Detail::EditProgram> edit,
              std::uint32_t scope, std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id), m_Scope(scope) {}

        std::weak_ptr<Detail::EditProgram> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;

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
        Node(std::weak_ptr<Detail::EditProgram> edit,
             std::uint32_t scope, std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id), m_Scope(scope) {}

        std::weak_ptr<Detail::EditProgram> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;

        friend class Edit;
        friend class Patch;
        friend class Nodes;
    };

    // One behavior link of the matched script.
    class Link {
    public:
        Link() = default;

    private:
        Link(std::weak_ptr<Detail::EditProgram> edit,
             std::uint32_t scope, std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id), m_Scope(scope) {}
        std::weak_ptr<Detail::EditProgram> m_Edit;
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
        Path(std::weak_ptr<Detail::EditProgram> edit,
             std::uint32_t scope, std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id), m_Scope(scope) {}
        std::weak_ptr<Detail::EditProgram> m_Edit;
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
        Slot(std::weak_ptr<Detail::EditProgram> edit,
             std::uint32_t scope, std::uint32_t id)
            : m_Edit(std::move(edit)), m_Id(id), m_Scope(scope) {}
        std::weak_ptr<Detail::EditProgram> m_Edit;
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
        Operation(std::weak_ptr<Detail::EditProgram> edit,
                  std::uint32_t scope, std::uint32_t id, CKGUID result, CKGUID input1,
                  CKGUID input2)
            : m_Edit(std::move(edit)), m_Id(id), m_Result(result),
              m_Scope(scope),
              m_Input1(input1), m_Input2(input2) {}

        std::weak_ptr<Detail::EditProgram> m_Edit;
        std::uint32_t m_Id = 0;
        std::uint32_t m_Scope = 0;
        CKGUID m_Result{0, 0};
        CKGUID m_Input1{0, 0};
        CKGUID m_Input2{0, 0};

        friend class Edit;
    };

    // One graph scope inside this Edit. Every symbol produced by a scope is
    // local to that graph; connecting it to a different scope is rejected
    // before the program crosses the C seam. Graph is a cheap value; authoring
    // operations return values so chains beginning at Root() never expose a
    // reference to a temporary scope.
    class Graph {
    public:
        Graph() = default;

        [[nodiscard]] Node Root() const;
        [[nodiscard]] Node Require(
            Behavior::Selector selector,
            CKGUID prototype = CKGUID(0, 0)) const;
        [[nodiscard]] Node Require(
            std::string_view name,
            CKGUID prototype = CKGUID(0, 0)) const;
        [[nodiscard]] Node Require(CKGUID prototype) const;
        [[nodiscard]] Node Require(NodePattern pattern) const;
        [[nodiscard]] Nodes Each(NodePattern pattern) const;
        [[nodiscard]] Nodes Each(std::string_view name) const;
        [[nodiscard]] Node Require(const Behavior::Node &node) const;
        [[nodiscard]] Link Require(const Behavior::Link &link) const;
        [[nodiscard]] Node Use(const Behavior::Node &node) const;
        [[nodiscard]] Link Use(const Behavior::Link &link) const;
        [[nodiscard]] Link Between(Port source, Port sink) const;
        [[nodiscard]] Link Between(Port source, Port sink,
                                   std::int32_t delay) const;
        [[nodiscard]] Node Next(Port source) const;
        [[nodiscard]] Node Next(Port source, NodePattern expected) const;
        [[nodiscard]] Node Next(Node source, std::int32_t output = 0) const;
        [[nodiscard]] Node Previous(Port sink) const;
        [[nodiscard]] Node Previous(Port sink, NodePattern expected) const;
        [[nodiscard]] Node Previous(Node sink, std::int32_t input = 0) const;
        [[nodiscard]] Link Leaving(Port source) const;
        [[nodiscard]] Link Leaving(Node source,
                                   std::int32_t output = 0) const;
        [[nodiscard]] Link Entering(Port sink) const;
        [[nodiscard]] Link Entering(Node sink,
                                    std::int32_t input = 0) const;
        [[nodiscard]] Link To(Port source, Node target) const;
        [[nodiscard]] Path Follow(Port source) const;
        [[nodiscard]] Node Add(const Block &block) const;
        [[nodiscard]] Node AddGraph(std::string_view name,
                                    std::int32_t priority = 0) const;
        [[nodiscard]] Node Replace(Node target, const Block &block) const;
        Graph Remove(Node target) const;
        [[nodiscard]] Operation AddOperation(
            CKGUID operation, CKGUID result,
            CKGUID input1 = CKGUID(0, 0),
            CKGUID input2 = CKGUID(0, 0)) const;
        [[nodiscard]] Slot AppendIn(std::string_view name) const;
        [[nodiscard]] Slot AppendIn(Node owner, std::string_view name) const;
        [[nodiscard]] Slot AppendOut(std::string_view name) const;
        [[nodiscard]] Slot AppendOut(Node owner, std::string_view name) const;
        [[nodiscard]] Slot AppendPin(std::string_view name, CKGUID type) const;
        [[nodiscard]] Slot AppendPin(Node owner, std::string_view name,
                                     CKGUID type) const;
        [[nodiscard]] Slot AppendPout(std::string_view name, CKGUID type) const;
        [[nodiscard]] Slot AppendPout(Node owner, std::string_view name,
                                      CKGUID type) const;
        [[nodiscard]] Slot AppendLocal(std::string_view name, CKGUID type) const;
        [[nodiscard]] Slot AppendLocal(Node owner, std::string_view name,
                                       CKGUID type) const;
        Graph Flow(Port source, Port sink, std::int32_t delay = 0) const;
        Graph Flow(Ports sources, Port sink, std::int32_t delay = 0) const;
        Graph Flow(Port source, Ports sinks, std::int32_t delay = 0) const;
        Graph FlowCycle(Port source, Port sink, std::int32_t delay = 0) const;
        Graph FlowCycle(Ports sources, Port sink,
                        std::int32_t delay = 0) const;
        Graph FlowCycle(Port source, Ports sinks,
                        std::int32_t delay = 0) const;
        Graph Bind(Port sink, Behavior::Value value) const;
        Graph Bind(Ports sinks, Behavior::Value value) const;
        template <class T,
                  class = std::enable_if_t<
                      !std::is_same_v<std::decay_t<T>, Behavior::Value> &&
                      !std::is_convertible_v<T, Port>>>
        Graph Bind(Ports sinks, T &&value) const;
        template <class T,
                  class = std::enable_if_t<
                      !std::is_same_v<std::decay_t<T>, Behavior::Value> &&
                      !std::is_convertible_v<T, Port>>>
        Graph Bind(Port sink, T &&value) const;
        Graph Bind(Port sink, Port source) const;
        Graph Bind(Ports sinks, Port source) const;
        Graph Share(Port sink, Port source) const;
        Graph Share(Ports sinks, Port source) const;
        Graph Push(Port source, Port sink) const;
        Graph Push(Ports sources, Port sink) const;
        Graph Push(Port source, Ports sinks) const;
        Graph Tap(Port source, Hook hook) const;
        Graph Tap(Ports sources, Hook hook) const;
        Graph Before(Link link, Hook hook) const;
        Graph After(Path path, Hook hook) const;
        Graph After(Port source, Hook hook) const;
        Graph Splice(Link link, Node through,
                     std::vector<PatchOrder> ordering = {}) const;
        Graph Splice(Link link, Port sink, Port source,
                     std::vector<PatchOrder> ordering = {}) const;
        Graph Redirect(Link link, Port sink,
                       std::vector<PatchOrder> ordering = {}) const;
        Graph Redirect(Link link, Link destination,
                       std::vector<PatchOrder> ordering = {}) const;

    private:
        Graph(std::weak_ptr<Detail::EditProgram> edit,
              std::uint32_t scope) : m_Edit(std::move(edit)), m_Scope(scope) {}

        [[nodiscard]] std::shared_ptr<Detail::EditProgram> Program() const;

        std::weak_ptr<Detail::EditProgram> m_Edit;
        std::uint32_t m_Scope = 0;
        friend class Edit;
        friend class Node;
    };

    Edit(const Edit &) = delete;
    Edit &operator=(const Edit &) = delete;
    Edit(Edit &&other) noexcept = default;
    Edit &operator=(Edit &&other) noexcept = default;
    ~Edit() = default;

    [[nodiscard]] Graph Root() const noexcept;

private:
    static Detail::EditStep &Define(
        Detail::EditProgram &program, std::uint32_t scope,
        std::uint32_t kind);
    static Detail::EditStep &Define(
        Detail::EditProgram &program, std::uint32_t scope,
        std::uint32_t kind, std::uint32_t result);
    static Slot Append(
        const std::shared_ptr<Detail::EditProgram> &program,
        std::uint32_t scope, Node owner, std::uint32_t slotKind,
        std::string_view name, CKGUID type);
    template <class Symbol>
    [[nodiscard]] static bool Require(
        Detail::EditProgram &program, std::uint32_t scope,
        const Symbol &symbol, std::string_view role);
    static void RejectLiveView(
        Detail::EditProgram &program, std::string_view kind);
    [[nodiscard]] static Graph EnterGraph(
        const std::shared_ptr<Detail::EditProgram> &program,
        std::uint32_t scope, const Node &node);
    [[nodiscard]] static std::uint64_t Shape(const Behavior::Node &node);
    void Encode(Detail::EditWire &out) const;
    [[nodiscard]] std::shared_ptr<const Edit> Snapshot() const;

    std::shared_ptr<Detail::EditProgram> m_Program;

    [[nodiscard]] Result<void> Validate(
        const std::shared_ptr<Detail::SessionState> &session,
        Detail::EditContext context = Detail::EditContext::Patch) const;

    friend class BML::Behavior::Graph;
    friend class Session;
    friend class Patch;
    friend class Plan;
    friend struct Detail::PatchWire;
    friend struct Detail::PlanWire;
    friend Detail::PatchTarget On(const BML::Behavior::Graph &, const Edit &);
    friend Detail::PlanRule On(const Scripts &, const Edit &);
};

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
    friend Detail::PlanRule On(const Scripts &, const Edit &);
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_EDIT_HPP
