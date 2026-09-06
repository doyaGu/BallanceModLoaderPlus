#ifndef BML_BEHAVIOR_GRAPH_HPP
#define BML_BEHAVIOR_GRAPH_HPP

#include "BML/Behavior/Frames.hpp"
#include "BML/Behavior/Prototype.hpp"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

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

namespace Detail {

struct GraphPortData {
    std::size_t Node = 0;
    SlotKind Kind = SlotKind::Pin;
    std::uint64_t LayoutGeneration = 0;
    CKGUID Type{0, 0};
    bool Dynamic = false;
    std::int32_t Index = -1;
    std::int32_t Occurrence = 0;
    bool Active = false;
    std::string Name;
};

struct GraphNodeData {
    std::uint64_t Id = 0;
    BML_ObjectRef Object{};
    std::uint64_t Parent = 0;
    std::uint64_t LayoutGeneration = 0;
    CKGUID Prototype{0, 0};
    std::int32_t Priority = 0;
    bool Active = false;
    std::string Name;
    std::size_t PortOffset = 0;
    std::size_t PortCount = 0;
};

struct GraphLinkData {
    std::uint64_t Id = 0;
    BML_ObjectRef Object{};
    std::size_t Source = 0;
    std::size_t Target = 0;
    std::int32_t InitialDelay = 0;
    std::int32_t RemainingDelay = 0;
    TruthValue Pending = TruthValue::Unknown;
};

struct GraphOperationData {
    std::uint64_t Id = 0;
    BML_ObjectRef Object{};
    std::uint64_t Owner = 0;
    CKGUID Function{0, 0};
    CKGUID Result{0, 0};
    CKGUID Input1{0, 0};
    CKGUID Input2{0, 0};
    std::string Name;
};

struct GraphData {
    std::size_t Root = 0;
    std::vector<GraphNodeData> Nodes;
    std::vector<GraphPortData> Ports;
    std::vector<GraphLinkData> Links;
    std::vector<GraphOperationData> Operations;
};

} // namespace Detail

template <class ViewType>
class GraphRange;

class Port {
public:
    Port() = default;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] BML_ObjectRef Object() const noexcept;
    [[nodiscard]] std::uint64_t Node() const noexcept;
    [[nodiscard]] std::uint64_t LayoutGeneration() const noexcept;
    [[nodiscard]] SlotKind Kind() const noexcept;
    [[nodiscard]] CKGUID Type() const noexcept;
    [[nodiscard]] bool Dynamic() const noexcept;
    [[nodiscard]] Selector Slot() const;
    [[nodiscard]] std::int32_t Index() const noexcept;
    [[nodiscard]] std::int32_t Occurrence() const noexcept;
    [[nodiscard]] bool Active() const noexcept;
    [[nodiscard]] std::string_view Name() const noexcept;

private:
    Port(std::shared_ptr<const Detail::GraphData> graph,
         std::size_t index) noexcept
        : m_Graph(std::move(graph)), m_Index(index) {}

    std::shared_ptr<const Detail::GraphData> m_Graph;
    std::size_t m_Index = 0;

    template <class> friend class GraphRange;
    friend class Node;
    friend class Link;
    friend class Graph;
    friend class Detail::Run;
};

class Node {
public:
    Node() = default;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] std::uint64_t Id() const noexcept;
    [[nodiscard]] BML_ObjectRef Object() const noexcept;
    [[nodiscard]] std::uint64_t Parent() const noexcept;
    [[nodiscard]] std::uint64_t LayoutGeneration() const noexcept;
    [[nodiscard]] CKGUID Prototype() const noexcept;
    [[nodiscard]] std::int32_t Priority() const noexcept;
    [[nodiscard]] bool Active() const noexcept;
    [[nodiscard]] std::string_view Name() const noexcept;
    [[nodiscard]] GraphRange<Port> Ports() const noexcept;

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
    Node(std::shared_ptr<const Detail::GraphData> graph,
         std::size_t index) noexcept
        : m_Graph(std::move(graph)), m_Index(index) {}

    std::shared_ptr<const Detail::GraphData> m_Graph;
    std::size_t m_Index = 0;

    template <class> friend class GraphRange;
    friend class Graph;
    friend class Edit;
};

class Link {
public:
    Link() = default;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] std::uint64_t Id() const noexcept;
    [[nodiscard]] BML_ObjectRef Object() const noexcept;
    [[nodiscard]] Port Source() const noexcept;
    [[nodiscard]] Port Target() const noexcept;
    [[nodiscard]] std::int32_t InitialDelay() const noexcept;
    [[nodiscard]] std::int32_t RemainingDelay() const noexcept;
    [[nodiscard]] TruthValue Pending() const noexcept;

private:
    Link(std::shared_ptr<const Detail::GraphData> graph,
         std::size_t index) noexcept
        : m_Graph(std::move(graph)), m_Index(index) {}

    std::shared_ptr<const Detail::GraphData> m_Graph;
    std::size_t m_Index = 0;

    template <class> friend class GraphRange;
    friend class Graph;
    friend class Edit;
};

class ParameterOperation {
public:
    ParameterOperation() = default;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] std::uint64_t Id() const noexcept;
    [[nodiscard]] BML_ObjectRef Object() const noexcept;
    [[nodiscard]] std::uint64_t Owner() const noexcept;
    [[nodiscard]] CKGUID Function() const noexcept;
    [[nodiscard]] CKGUID Result() const noexcept;
    [[nodiscard]] CKGUID Input1() const noexcept;
    [[nodiscard]] CKGUID Input2() const noexcept;
    [[nodiscard]] std::string_view Name() const noexcept;

private:
    ParameterOperation(std::shared_ptr<const Detail::GraphData> graph,
                       std::size_t index) noexcept
        : m_Graph(std::move(graph)), m_Index(index) {}

    std::shared_ptr<const Detail::GraphData> m_Graph;
    std::size_t m_Index = 0;

    template <class> friend class GraphRange;
    friend class Graph;
};

template <class ViewType>
class GraphRange {
public:
    GraphRange() = default;
    class Iterator {
    public:
        class Arrow {
        public:
            explicit Arrow(ViewType value) : m_Value(std::move(value)) {}
            [[nodiscard]] const ViewType *operator->() const noexcept {
                return &m_Value;
            }

        private:
            ViewType m_Value;
        };

        using difference_type = std::ptrdiff_t;
        using value_type = ViewType;
        using pointer = Arrow;
        using reference = ViewType;
        using iterator_category = std::random_access_iterator_tag;

        [[nodiscard]] ViewType operator*() const {
            return ViewType(m_Graph, m_Index);
        }
        [[nodiscard]] Arrow operator->() const { return Arrow(**this); }
        [[nodiscard]] ViewType operator[](difference_type offset) const {
            return ViewType(m_Graph, static_cast<std::size_t>(
                static_cast<difference_type>(m_Index) + offset));
        }
        Iterator &operator++() { ++m_Index; return *this; }
        Iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        Iterator &operator--() { --m_Index; return *this; }
        Iterator operator--(int) { auto copy = *this; --*this; return copy; }
        Iterator &operator+=(difference_type offset) {
            m_Index = static_cast<std::size_t>(
                static_cast<difference_type>(m_Index) + offset);
            return *this;
        }
        Iterator &operator-=(difference_type offset) { return *this += -offset; }
        friend Iterator operator+(Iterator it, difference_type offset) {
            return it += offset;
        }
        friend Iterator operator+(difference_type offset, Iterator it) {
            return it += offset;
        }
        friend Iterator operator-(Iterator it, difference_type offset) {
            return it -= offset;
        }
        friend difference_type operator-(Iterator left, Iterator right) {
            return static_cast<difference_type>(left.m_Index) -
                static_cast<difference_type>(right.m_Index);
        }
        friend bool operator==(Iterator left, Iterator right) {
            return left.m_Graph == right.m_Graph &&
                left.m_Index == right.m_Index;
        }
        friend bool operator!=(Iterator left, Iterator right) {
            return !(left == right);
        }
        friend bool operator<(Iterator left, Iterator right) {
            return left.m_Index < right.m_Index;
        }
        friend bool operator>(Iterator left, Iterator right) { return right < left; }
        friend bool operator<=(Iterator left, Iterator right) { return !(right < left); }
        friend bool operator>=(Iterator left, Iterator right) { return !(left < right); }

    private:
        Iterator(std::shared_ptr<const Detail::GraphData> graph,
                 std::size_t index) noexcept
            : m_Graph(std::move(graph)), m_Index(index) {}
        std::shared_ptr<const Detail::GraphData> m_Graph;
        std::size_t m_Index = 0;
        friend class GraphRange;
    };

    [[nodiscard]] bool empty() const noexcept { return m_Count == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return m_Count; }
    [[nodiscard]] ViewType operator[](std::size_t index) const {
        if (index >= m_Count)
            throw std::out_of_range("Behavior Graph view index is out of range.");
        return ViewType(m_Graph, m_Offset + index);
    }
    [[nodiscard]] ViewType front() const { return (*this)[0]; }
    [[nodiscard]] ViewType back() const { return (*this)[m_Count - 1]; }
    [[nodiscard]] Iterator begin() const noexcept {
        return Iterator(m_Graph, m_Offset);
    }
    [[nodiscard]] Iterator end() const noexcept {
        return Iterator(m_Graph, m_Offset + m_Count);
    }

private:
    GraphRange(std::shared_ptr<const Detail::GraphData> graph,
               std::size_t offset, std::size_t count) noexcept
        : m_Graph(std::move(graph)), m_Offset(offset), m_Count(count) {}
    std::shared_ptr<const Detail::GraphData> m_Graph;
    std::size_t m_Offset = 0;
    std::size_t m_Count = 0;
    friend class Node;
    friend class Graph;
};

inline Port Node::Select(SlotKind kind, Selector slot) const {
    if (!*this)
        return {};
    const Detail::GraphNodeData &node = m_Graph->Nodes[m_Index];
    const Detail::GraphPortData *match = nullptr;
    std::size_t matchIndex = 0;
    for (std::size_t index = node.PortOffset;
         index < node.PortOffset + node.PortCount; ++index) {
        const Detail::GraphPortData &port = m_Graph->Ports[index];
        if (port.Kind != kind ||
            !slot.Matches(port.Index, port.Occurrence, port.Name))
            continue;
        if (match && slot.RequiresUniqueMatch()) {
            match = nullptr;
            break;
        }
        match = &port;
        matchIndex = index;
        if (!slot.RequiresUniqueMatch())
            break;
    }
    return match ? Port(m_Graph, matchIndex) : Port{};
}

inline Node::operator bool() const noexcept {
    return m_Graph && m_Index < m_Graph->Nodes.size();
}
inline std::uint64_t Node::Id() const noexcept { return (*this) ? m_Graph->Nodes[m_Index].Id : 0; }
inline BML_ObjectRef Node::Object() const noexcept { return (*this) ? m_Graph->Nodes[m_Index].Object : BML_ObjectRef{}; }
inline std::uint64_t Node::Parent() const noexcept { return (*this) ? m_Graph->Nodes[m_Index].Parent : 0; }
inline std::uint64_t Node::LayoutGeneration() const noexcept { return (*this) ? m_Graph->Nodes[m_Index].LayoutGeneration : 0; }
inline CKGUID Node::Prototype() const noexcept { return (*this) ? m_Graph->Nodes[m_Index].Prototype : CKGUID(0, 0); }
inline std::int32_t Node::Priority() const noexcept { return (*this) ? m_Graph->Nodes[m_Index].Priority : 0; }
inline bool Node::Active() const noexcept { return (*this) && m_Graph->Nodes[m_Index].Active; }
inline std::string_view Node::Name() const noexcept { return (*this) ? std::string_view(m_Graph->Nodes[m_Index].Name) : std::string_view{}; }
inline GraphRange<Port> Node::Ports() const noexcept {
    if (!*this)
        return {};
    const auto &node = m_Graph->Nodes[m_Index];
    return GraphRange<Port>(m_Graph, node.PortOffset, node.PortCount);
}

inline Port::operator bool() const noexcept {
    return m_Graph && m_Index < m_Graph->Ports.size();
}
inline BML_ObjectRef Port::Object() const noexcept {
    return (*this) ? m_Graph->Nodes[m_Graph->Ports[m_Index].Node].Object : BML_ObjectRef{};
}
inline std::uint64_t Port::Node() const noexcept {
    return (*this) ? m_Graph->Nodes[m_Graph->Ports[m_Index].Node].Id : 0;
}
inline std::uint64_t Port::LayoutGeneration() const noexcept { return (*this) ? m_Graph->Ports[m_Index].LayoutGeneration : 0; }
inline SlotKind Port::Kind() const noexcept { return (*this) ? m_Graph->Ports[m_Index].Kind : SlotKind::Pin; }
inline CKGUID Port::Type() const noexcept { return (*this) ? m_Graph->Ports[m_Index].Type : CKGUID(0, 0); }
inline bool Port::Dynamic() const noexcept { return (*this) && m_Graph->Ports[m_Index].Dynamic; }
inline Selector Port::Slot() const { return (*this) ? Selector::At(Index()) : Selector::At(-1); }
inline std::int32_t Port::Index() const noexcept { return (*this) ? m_Graph->Ports[m_Index].Index : -1; }
inline std::int32_t Port::Occurrence() const noexcept { return (*this) ? m_Graph->Ports[m_Index].Occurrence : -1; }
inline bool Port::Active() const noexcept { return (*this) && m_Graph->Ports[m_Index].Active; }
inline std::string_view Port::Name() const noexcept { return (*this) ? std::string_view(m_Graph->Ports[m_Index].Name) : std::string_view{}; }

inline Link::operator bool() const noexcept {
    return m_Graph && m_Index < m_Graph->Links.size();
}
inline std::uint64_t Link::Id() const noexcept { return (*this) ? m_Graph->Links[m_Index].Id : 0; }
inline BML_ObjectRef Link::Object() const noexcept { return (*this) ? m_Graph->Links[m_Index].Object : BML_ObjectRef{}; }
inline Port Link::Source() const noexcept { return (*this) ? Port(m_Graph, m_Graph->Links[m_Index].Source) : Port{}; }
inline Port Link::Target() const noexcept { return (*this) ? Port(m_Graph, m_Graph->Links[m_Index].Target) : Port{}; }
inline std::int32_t Link::InitialDelay() const noexcept { return (*this) ? m_Graph->Links[m_Index].InitialDelay : 0; }
inline std::int32_t Link::RemainingDelay() const noexcept { return (*this) ? m_Graph->Links[m_Index].RemainingDelay : 0; }
inline TruthValue Link::Pending() const noexcept { return (*this) ? m_Graph->Links[m_Index].Pending : TruthValue::Unknown; }

inline ParameterOperation::operator bool() const noexcept {
    return m_Graph && m_Index < m_Graph->Operations.size();
}
inline std::uint64_t ParameterOperation::Id() const noexcept {
    return (*this) ? m_Graph->Operations[m_Index].Id : 0;
}
inline BML_ObjectRef ParameterOperation::Object() const noexcept {
    return (*this) ? m_Graph->Operations[m_Index].Object : BML_ObjectRef{};
}
inline std::uint64_t ParameterOperation::Owner() const noexcept {
    return (*this) ? m_Graph->Operations[m_Index].Owner : 0;
}
inline CKGUID ParameterOperation::Function() const noexcept {
    return (*this) ? m_Graph->Operations[m_Index].Function : CKGUID(0, 0);
}
inline CKGUID ParameterOperation::Result() const noexcept {
    return (*this) ? m_Graph->Operations[m_Index].Result : CKGUID(0, 0);
}
inline CKGUID ParameterOperation::Input1() const noexcept {
    return (*this) ? m_Graph->Operations[m_Index].Input1 : CKGUID(0, 0);
}
inline CKGUID ParameterOperation::Input2() const noexcept {
    return (*this) ? m_Graph->Operations[m_Index].Input2 : CKGUID(0, 0);
}
inline std::string_view ParameterOperation::Name() const noexcept {
    return (*this) ? std::string_view(m_Graph->Operations[m_Index].Name)
                   : std::string_view{};
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
    explicit LayoutChanged(const Behavior::Node &node)
        : Node(node.Object()), LayoutGeneration(node.LayoutGeneration()) {}
    BML_ObjectRef Node{};
    std::uint64_t LayoutGeneration = 0;
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
        return m_Data ? Node(m_Data, m_Data->Root) : Node{};
    }
    [[nodiscard]] std::uint64_t Generation() const noexcept {
        return m_Generation;
    }
    [[nodiscard]] std::uint64_t Fingerprint() const noexcept {
        return m_Fingerprint;
    }
    [[nodiscard]] GraphRange<Node> Nodes() const noexcept {
        return m_Data
            ? GraphRange<Node>(m_Data, 0, m_Data->Nodes.size())
            : GraphRange<Node>{};
    }
    [[nodiscard]] GraphRange<Link> Links() const noexcept {
        return m_Data
            ? GraphRange<Link>(m_Data, 0, m_Data->Links.size())
            : GraphRange<Link>{};
    }
    [[nodiscard]] GraphRange<ParameterOperation> Operations() const noexcept {
        return m_Data
            ? GraphRange<ParameterOperation>(m_Data, 0, m_Data->Operations.size())
            : GraphRange<ParameterOperation>{};
    }
    [[nodiscard]] std::vector<Node> FindAll(
        std::string_view name) const {
        std::vector<Node> matches;
        for (Node node : Nodes()) {
            if (node.Name() == name)
                matches.push_back(node);
        }
        return matches;
    }
    [[nodiscard]] Result<Node> Find(std::string_view name) const {
        Node match;
        for (Node node : Nodes()) {
            if (node.Name() != name)
                continue;
            if (match) {
                Status status;
                status.Error = Error::QueryAmbiguous;
                status.Message = "More than one Behavior node is named '" +
                    std::string(name) + "'.";
                return Result<Node>::Failure(BML_ERROR_FAIL,
                                              std::move(status));
            }
            match = node;
        }
        if (!match) {
            Status status;
            status.Error = Error::QueryNotFound;
            status.Message = "No Behavior node is named '" +
                std::string(name) + "'.";
            return Result<Node>::Failure(BML_ERROR_NOT_FOUND,
                                          std::move(status));
        }
        return Result<Node>::Success(std::move(match));
    }

    [[nodiscard]] Result<Graph> Logical() const;
    [[nodiscard]] Result<Graph> Live() const;
    [[nodiscard]] Result<Behavior::Layout> Layout(
        const Node &node) const;
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
        BML_ObjectRef expectedRoot,
        const BML_BehaviorGraph &wire,
        const std::uint8_t *payload, std::size_t payloadSize,
        const BML_BehaviorStatus &status);
    template <class Function>
    Result<Behavior::Watch> OpenWatch(BML_BehaviorWatchSpec spec,
                                     Function &&callback) const;

    std::shared_ptr<Detail::SessionState> m_Session;
    BML_ObjectRef m_Root{};
    View m_View = View::Logical;
    std::uint64_t m_Generation = 0;
    std::uint64_t m_Fingerprint = 0;
    std::shared_ptr<const Detail::GraphData> m_Data;

    friend class Session;
    friend class Script;
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
