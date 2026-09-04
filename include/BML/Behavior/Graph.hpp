#ifndef BML_BEHAVIOR_GRAPH_HPP
#define BML_BEHAVIOR_GRAPH_HPP

#include "BML/Behavior/Frames.hpp"
#include "BML/Behavior/Prototype.hpp"

namespace BML::Behavior {

enum class View : std::uint32_t {
    Logical = BML_BEHAVIOR_GRAPH_LOGICAL,
    Live = BML_BEHAVIOR_GRAPH_LIVE,
};

enum class TruthValue : std::uint32_t {
    No = BML_BEHAVIOR_FALSE,
    Yes = BML_BEHAVIOR_TRUE,
    Unknown = BML_BEHAVIOR_UNKNOWN,
};

enum class ObservationState : std::uint32_t {
    Available = BML_BEHAVIOR_VALUE_AVAILABLE,
    Indeterminate = BML_BEHAVIOR_VALUE_INDETERMINATE,
    Unsupported = BML_BEHAVIOR_VALUE_UNSUPPORTED,
};

enum class Relation : std::uint32_t {
    Stored = BML_BEHAVIOR_VALUE_STORED,
    Direct = BML_BEHAVIOR_VALUE_DIRECT,
    Shared = BML_BEHAVIOR_VALUE_SHARED,
    Operation = BML_BEHAVIOR_VALUE_OPERATION,
};

struct ObservedValue {
    ObservationState State = ObservationState::Unsupported;
    Relation Source = Relation::Stored;
    CKGUID Type{0, 0};
    std::optional<ValueKind> Kind;
    PoutData Data;
};

struct Port {
    BML_ObjectRef Object{};
    std::uint64_t Node = 0;
    SlotKind Kind = SlotKind::Pin;
    Selector Slot;
    std::int32_t Index = -1;
    std::int32_t Occurrence = 0;
    bool Active = false;
    std::string Name;
};

struct Node {
    std::uint64_t Id = 0;
    BML_ObjectRef Object{};
    std::uint64_t Parent = 0;
    CKGUID Prototype{0, 0};
    std::int32_t Priority = 0;
    bool Active = false;
    std::string Name;
    std::vector<Port> Ports;

    [[nodiscard]] Port In(Selector slot = Selector::Only()) const;
    [[nodiscard]] Port In(std::int32_t index) const {
        return In(Selector::At(index));
    }
    [[nodiscard]] Port In(std::string_view name) const {
        return In(Selector::Unique(name));
    }
    [[nodiscard]] Port Out(Selector slot = Selector::Only()) const;
    [[nodiscard]] Port Out(std::int32_t index) const {
        return Out(Selector::At(index));
    }
    [[nodiscard]] Port Out(std::string_view name) const {
        return Out(Selector::Unique(name));
    }
    [[nodiscard]] Port Pin(Selector slot = Selector::Only()) const;
    [[nodiscard]] Port Pin(std::int32_t index) const {
        return Pin(Selector::At(index));
    }
    [[nodiscard]] Port Pin(std::string_view name) const {
        return Pin(Selector::Unique(name));
    }
    [[nodiscard]] Port Pout(Selector slot = Selector::Only()) const;
    [[nodiscard]] Port Pout(std::int32_t index) const {
        return Pout(Selector::At(index));
    }
    [[nodiscard]] Port Pout(std::string_view name) const {
        return Pout(Selector::Unique(name));
    }
    [[nodiscard]] Port Setting(Selector slot = Selector::Only()) const;
    [[nodiscard]] Port Setting(std::int32_t index) const {
        return Setting(Selector::At(index));
    }
    [[nodiscard]] Port Setting(std::string_view name) const {
        return Setting(Selector::Unique(name));
    }
    [[nodiscard]] Port Local(Selector slot = Selector::Only()) const;
    [[nodiscard]] Port Local(std::int32_t index) const {
        return Local(Selector::At(index));
    }
    [[nodiscard]] Port Local(std::string_view name) const {
        return Local(Selector::Unique(name));
    }
    [[nodiscard]] Port Target() const;

private:
    [[nodiscard]] Port Select(SlotKind kind, Selector slot) const;
};

struct Link {
    std::uint64_t Id = 0;
    BML_ObjectRef Object{};
    Port Source;
    Port Target;
    std::int32_t InitialDelay = 0;
    std::int32_t RemainingDelay = 0;
    TruthValue Pending = TruthValue::Unknown;
};

inline Port Node::Select(SlotKind kind, Selector slot) const {
    const BML_BehaviorSelector wanted = slot.Wire();
    for (const Port &port : Ports) {
        if (port.Kind != kind)
            continue;
        const bool match = wanted.Kind == BML_BEHAVIOR_SELECTOR_ONLY ||
            (wanted.Kind == BML_BEHAVIOR_SELECTOR_INDEX &&
             wanted.Index == port.Index) ||
            ((wanted.Kind == BML_BEHAVIOR_SELECTOR_NAME ||
              wanted.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME) &&
             std::string_view(wanted.Name.Data, wanted.Name.Length) == port.Name &&
             (wanted.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME ||
              wanted.Occurrence == port.Occurrence));
        if (match) {
            Port selected = port;
            selected.Object = Object;
            selected.Slot = std::move(slot);
            return selected;
        }
    }
    Port selected;
    selected.Object = Object;
    selected.Node = Id;
    selected.Kind = kind;
    selected.Slot = std::move(slot);
    return selected;
}

inline Port Node::In(Selector slot) const {
    return Select(SlotKind::In, std::move(slot));
}
inline Port Node::Out(Selector slot) const {
    return Select(SlotKind::Out, std::move(slot));
}
inline Port Node::Pin(Selector slot) const {
    return Select(SlotKind::Pin, std::move(slot));
}
inline Port Node::Pout(Selector slot) const {
    return Select(SlotKind::Pout, std::move(slot));
}
inline Port Node::Setting(Selector slot) const {
    return Select(SlotKind::Setting, std::move(slot));
}
inline Port Node::Local(Selector slot) const {
    return Select(SlotKind::Local, std::move(slot));
}
inline Port Node::Target() const {
    return Select(SlotKind::Target, Selector::Only());
}

struct GraphChanged {};

struct LayoutChanged {
    explicit LayoutChanged(const Behavior::Node &node) : Node(node.Object) {}
    BML_ObjectRef Node{};
};

struct Sampled {
    explicit Sampled(Port value) : Value(std::move(value)) {}
    Port Value;
};

enum class ChangeKind : std::uint32_t {
    Graph = BML_BEHAVIOR_WATCH_GRAPH,
    Layout = BML_BEHAVIOR_WATCH_LAYOUT,
    SampledValue = BML_BEHAVIOR_WATCH_SAMPLED_VALUE,
};

enum class WatchState : std::uint32_t {
    Active = BML_BEHAVIOR_WATCH_ACTIVE,
    Failed = BML_BEHAVIOR_WATCH_FAILED,
};

struct WatchInfo {
    WatchState State = WatchState::Active;
    Status LastStatus;
};

struct Change {
    ChangeKind Kind = ChangeKind::Graph;
    std::uint64_t Sequence = 0;
    std::uint64_t GameFrame = 0;
    std::uint64_t Before = 0;
    std::uint64_t After = 0;
    ObservedValue PreviousValue;
    ObservedValue CurrentValue;
};

// A Plan is durable authoring intent: one exact script name, one symbolic edit,
// and the Loader reconciling the two as scripts load, reload, and are deleted.
enum class PlanState : std::uint32_t {
    Reconciling = BML_BEHAVIOR_PLAN_RECONCILING,
    Active = BML_BEHAVIOR_PLAN_ACTIVE,
    Unsatisfied = BML_BEHAVIOR_PLAN_UNSATISFIED,
    Conflicted = BML_BEHAVIOR_PLAN_CONFLICTED,
    Retiring = BML_BEHAVIOR_PLAN_RETIRING,
};

struct PlanInfo {
    PlanState State = PlanState::Reconciling;
    // The world epoch this Plan was last reconciled against.
    std::uint64_t World = 0;
    std::uint32_t Matches = 0;
    std::uint32_t Installations = 0;
    // Why the Plan is Unsatisfied or Conflicted.
    Behavior::Status LastStatus;

    [[nodiscard]] bool Installed() const noexcept {
        return State == PlanState::Active && Installations != 0;
    }
};

enum class PatchState : std::uint32_t {
    Pending = BML_BEHAVIOR_PATCH_PENDING,
    Active = BML_BEHAVIOR_PATCH_ACTIVE,
    Closing = BML_BEHAVIOR_PATCH_CLOSING,
    Conflicted = BML_BEHAVIOR_PATCH_CONFLICTED,
    Closed = BML_BEHAVIOR_PATCH_CLOSED,
    Failed = BML_BEHAVIOR_PATCH_FAILED,
};

struct PatchInfo {
    PatchState State = PatchState::Pending;
    std::uint32_t Conflicts = 0;
    Behavior::Status LastStatus;

    [[nodiscard]] bool Installed() const noexcept {
        return State == PatchState::Active;
    }
};

// What one Hook callback receives. Every reference is live for the duration of
// the call only.
struct HookEvent {
    float DeltaTime = 0.0f;
    // The Hook Block being executed.
    BML_ObjectRef Block{};
    // The root script that owns the Block, when the Loader can name it.
    BML_ObjectRef Script{};
    // The object the Block is attached to.
    BML_ObjectRef Owner{};
};

enum class HookResult : int {
    // Explicitly stops the enclosing chain: the Hook Block leaves every Out
    // inactive. The callback stays installed and runs on the next activation.
    Error = -1,
    Ok = BML_BEHAVIOR_HOOK_OK,
    // Keep the Hook Block active for one more frame. Its Outs still activate.
    AgainNextFrame = BML_BEHAVIOR_HOOK_AGAIN_NEXT_FRAME,
    // The callback did not complete. The facade returns this when an author
    // callback throws, so no C++ exception crosses the C seam. The Loader keeps
    // the first fault as the Hook diagnostic, stops invoking this occurrence,
    // and lets the Hook Block pass the activation through.
    Fault = BML_BEHAVIOR_HOOK_FAULT,
};

enum class CloseState {
    Closing,
    Closed,
};

// Places one Patch relative to another Patch spliced onto the same link.
struct PatchOrder {
    std::uint32_t Kind = BML_BEHAVIOR_ORDER_BEFORE;
    std::string Owner;
    std::string Name;
};

inline PatchOrder Before(std::string_view owner, std::string_view name) {
    return {BML_BEHAVIOR_ORDER_BEFORE, std::string(owner), std::string(name)};
}
inline PatchOrder After(std::string_view owner, std::string_view name) {
    return {BML_BEHAVIOR_ORDER_AFTER, std::string(owner), std::string(name)};
}

namespace Detail {
struct SessionState;
struct BlockState;
class Run;
}

class Edit;
class Patch;

class Watch {
public:
    Watch() = default;
    ~Watch();
    Watch(const Watch &) = delete;
    Watch &operator=(const Watch &) = delete;
    Watch(Watch &&other) noexcept
        : m_Session(std::move(other.m_Session)),
          m_Handle(std::exchange(other.m_Handle, nullptr)) {}
    Watch &operator=(Watch &&other) noexcept {
        if (this != &other) {
            Watch previous(std::move(*this));
            m_Session = std::move(other.m_Session);
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Session && m_Handle;
    }
    [[nodiscard]] Result<WatchInfo> Info() const;
    [[nodiscard]] Result<CloseState> Close() noexcept;

private:
    Watch(std::shared_ptr<Detail::SessionState> session,
          BML_BehaviorWatch handle)
        : m_Session(std::move(session)), m_Handle(handle) {}
    std::shared_ptr<Detail::SessionState> m_Session;
    BML_BehaviorWatch m_Handle = nullptr;

    friend class Graph;
};

class Graph {
public:
    [[nodiscard]] View Mode() const noexcept { return m_View; }
    [[nodiscard]] Node Root() const noexcept {
        for (const Node &node : m_Nodes) {
            if (node.Object.Domain == m_Root.Domain &&
                node.Object.Slot == m_Root.Slot &&
                node.Object.Generation == m_Root.Generation)
                return node;
        }
        Node root;
        root.Object = m_Root;
        return root;
    }
    [[nodiscard]] std::uint64_t Generation() const noexcept {
        return m_Generation;
    }
    [[nodiscard]] std::uint64_t Fingerprint() const noexcept {
        return m_Fingerprint;
    }
    [[nodiscard]] const std::vector<Node> &Nodes() const noexcept {
        return m_Nodes;
    }
    [[nodiscard]] const std::vector<Link> &Links() const noexcept {
        return m_Links;
    }
    [[nodiscard]] std::vector<Node> FindAll(
        std::string_view name) const {
        std::vector<Node> matches;
        for (const Node &node : m_Nodes) {
            if (node.Name == name)
                matches.push_back(node);
        }
        return matches;
    }
    [[nodiscard]] Result<Node> Find(std::string_view name) const {
        const Node *match = nullptr;
        for (const Node &node : m_Nodes) {
            if (node.Name != name)
                continue;
            if (match) {
                Status status;
                status.Error = Error::QueryAmbiguous;
                status.Message = "More than one Behavior node is named '" +
                    std::string(name) + "'.";
                return Result<Node>::Failure(BML_ERROR_FAIL,
                                              std::move(status));
            }
            match = &node;
        }
        if (!match) {
            Status status;
            status.Error = Error::QueryNotFound;
            status.Message = "No Behavior node is named '" +
                std::string(name) + "'.";
            return Result<Node>::Failure(BML_ERROR_NOT_FOUND,
                                          std::move(status));
        }
        return Result<Node>::Success(*match);
    }

    [[nodiscard]] Result<Graph> Logical() const;
    [[nodiscard]] Result<Graph> Live() const;
    [[nodiscard]] Result<ObservedValue> Read(const Port &port) const;
    [[nodiscard]] Result<Patch> Apply(std::string_view name,
                                      const Edit &edit) const;

    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        GraphChanged, Function &&callback) const;
    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        LayoutChanged change, Function &&callback) const;
    template <class Function>
    [[nodiscard]] Result<Behavior::Watch> Watch(
        Sampled change, Function &&callback) const;
private:
    static Result<Graph> Read(std::shared_ptr<Detail::SessionState> session,
                              BML_ObjectRef root, View view);
    static Result<Graph> ReadRun(
        std::shared_ptr<Detail::SessionState> session,
        BML_BehaviorRun run, View view);
    static Result<Graph> Decode(
        std::shared_ptr<Detail::SessionState> session, View view,
        const BML_BehaviorGraph &wire,
        const std::vector<std::uint8_t> &payload,
        const BML_BehaviorStatus &status);
    template <class Function>
    Result<Behavior::Watch> OpenWatch(BML_BehaviorWatchSpec spec,
                                     Function &&callback) const;

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_ObjectRef m_Root{};
    View m_View = View::Logical;
    std::uint64_t m_Generation = 0;
    std::uint64_t m_Fingerprint = 0;
    std::vector<Node> m_Nodes;
    std::vector<Link> m_Links;

    friend class Session;
    friend class Detail::Run;
    friend class Block;
};

class Session;
class Block;
class Call;
class Task;
class Instance;
class Edit;
class Hook;
class Plan;
class Patch;


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_GRAPH_HPP
