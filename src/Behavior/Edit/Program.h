#ifndef BML_BEHAVIOR_EDIT_PROGRAM_H
#define BML_BEHAVIOR_EDIT_PROGRAM_H

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Behavior/Block.h"
#include "Behavior/Edit/Ops.h"
#include "Behavior/Pattern.h"
#include "Behavior/PrototypeCatalog.h"

namespace BML::Behavior::Internal {

struct PathRef {
    std::uint32_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Value != 0;
    }
    [[nodiscard]] bool operator==(const PathRef &) const noexcept = default;
};

// The steps of one Program scope. A step names its operands only through
// handles an earlier step defined, never through a CK object or Layout.
namespace Steps {

enum class NodeRelation {
    Next,
    Previous,
};

struct RelatedNode {
    NodeRelation Relation = NodeRelation::Next;
    Port Endpoint;

    friend bool operator==(const RelatedNode &, const RelatedNode &) = default;
};

// Finds existing Nodes by Pattern, or by the one Link at Related.
struct QueryNode {
    Node Handle;
    NodePattern Pattern;
    bool Many = false;
    std::optional<RelatedNode> Related;

    friend bool operator==(const QueryNode &, const QueryNode &) = default;
};

// Names a Node the author already holds a reference to.
struct UseNode {
    Node Handle;
    ObjectRef Anchor;

    friend bool operator==(const UseNode &, const UseNode &) = default;
};

struct AddBlock {
    Node Handle;
    BlockSpec Block;

    friend bool operator==(const AddBlock &, const AddBlock &) = default;
};

struct AddGraph {
    Node Handle;
    GraphSpec Graph;

    friend bool operator==(const AddGraph &, const AddGraph &) = default;
};

// An operation handle shares the Node namespace, but is never a Node.
struct AddOperation {
    ParameterOperation Handle;
    CKGUID Guid = CKGUID();
    CKGUID Result = CKGUID();
    CKGUID Input1 = CKGUID();
    CKGUID Input2 = CKGUID();

    friend bool operator==(const AddOperation &, const AddOperation &) = default;
};

enum class LinkRelation {
    Between,
    Leaving,
    Entering,
    To,
};

struct QueryLink {
    Link Handle;
    LinkRelation Relation = LinkRelation::Between;
    Port Source;
    Port Sink;
    Node Target;
    std::optional<int> Delay;

    friend bool operator==(const QueryLink &, const QueryLink &) = default;
};

struct UseLink {
    Link Handle;
    ObjectRef Anchor;

    friend bool operator==(const UseLink &, const UseLink &) = default;
};

struct Follow {
    PathRef Handle;
    Port Start;

    friend bool operator==(const Follow &, const Follow &) = default;
};

struct Replace {
    Node Target;
    Node Replacement;

    friend bool operator==(const Replace &, const Replace &) = default;
};

struct Remove {
    Node Target;

    friend bool operator==(const Remove &, const Remove &) = default;
};

struct Flow {
    Port Source;
    Port Sink;
    int Delay = 0;
    Cycle SameFrameCycle = Cycle::Reject;

    friend bool operator==(const Flow &, const Flow &) = default;
};

struct HookFlow {
    Port Source;
    Port Sink;
    HookBlock::Hook Hook;

    friend bool operator==(const HookFlow &, const HookFlow &) = default;
};

struct Set {
    Port Target;
    Parameter::Binding Value;

    friend bool operator==(const Set &, const Set &) = default;
};

struct Bind {
    Port Target;
    BindKind Kind = BindKind::Literal;
    Parameter::Binding Value;
    Port Source;

    friend bool operator==(const Bind &, const Bind &) = default;
};

struct Push {
    Port Source;
    Port Destination;

    friend bool operator==(const Push &, const Push &) = default;
};

struct Tap {
    Port Source;
    HookBlock::Hook Hook;

    friend bool operator==(const Tap &, const Tap &) = default;
};

struct After {
    PathRef Target;
    HookBlock::Hook Hook;

    friend bool operator==(const After &, const After &) = default;
};

struct Before {
    Link Target;
    HookBlock::Hook Hook;

    friend bool operator==(const Before &, const Before &) = default;
};

struct Splice {
    Link Target;
    Port Input;
    Port Output;
    std::vector<Order> Ordering;

    friend bool operator==(const Splice &, const Splice &) = default;
};

struct Redirect {
    Link Target;
    Port Sink;
    std::vector<Order> Ordering;

    friend bool operator==(const Redirect &, const Redirect &) = default;
};

struct RedirectToLink {
    Link Target;
    Link Destination;
    std::vector<Order> Ordering;

    friend bool operator==(const RedirectToLink &,
                           const RedirectToLink &) = default;
};

struct Reconnect {
    Link Target;
    Port Source;
    Port Sink;
    Cycle SameFrameCycle = Cycle::Reject;

    friend bool operator==(const Reconnect &, const Reconnect &) = default;
};

// Appends one In, Out, Pin, Pout, or Local. Handle is the Port later steps
// use; its negative index is an identity, not a Layout position.
struct Append {
    Port Handle;
    Node Owner;
    SlotKind Kind = SlotKind::Input;
    std::string Name;
    CKGUID Type = CKGUID();

    friend bool operator==(const Append &, const Append &) = default;
};

} // namespace Steps

// Canonical graph-edit intent: one ordered list of steps per graph scope.
// Handles are symbolic identities within this value; no CK object, NativeRef,
// or Layout is retained. An added Block keeps the Prototype provider
// generation selected by its author so later Plan installations cannot
// silently switch implementations.
// Resolve reads the complete query before the live Edit mutates a graph.
// UseNode and UseLink are the one exception: they anchor on an ObjectRef the
// author already holds, which makes the intent single-world. UsesIdentity
// reports that, and a Plan refuses such a Program.
class Program final {
public:
    using Step = std::variant<
        Steps::QueryNode, Steps::UseNode, Steps::AddBlock, Steps::AddGraph,
        Steps::AddOperation, Steps::QueryLink, Steps::UseLink, Steps::Follow,
        Steps::Replace, Steps::Remove, Steps::Flow, Steps::HookFlow,
        Steps::Set, Steps::Bind, Steps::Push, Steps::Tap, Steps::After,
        Steps::Before, Steps::Splice, Steps::Redirect, Steps::RedirectToLink,
        Steps::Reconnect, Steps::Append>;

    struct ResolvedSymbols {
        std::map<std::uint32_t, Node> Nodes;
        std::map<int, Port> Ports;
    };

    class Resolver : public PatternValues {
    public:
        virtual ~Resolver() = default;

        virtual Status Begin(const PatchKey &patch, const ObjectRef &graph,
                             Ops &out, GraphModel &base) = 0;
        virtual Status UseNode(Ops &edit, const ObjectRef &node,
                               Node &out) = 0;
        virtual Status UseLink(Ops &edit, const ObjectRef &link,
                               Link &out) = 0;
        virtual Status Add(Ops &edit, BlockSpec block, Node &out) = 0;
        virtual Status AddGraph(Ops &edit, std::string name, int priority,
                                Node &out) = 0;
        virtual Status Tap(Ops &edit, Port source,
                           const HookBlock::Hook &hook) = 0;
        // Puts a callback inside one Link, so control reaches the callback
        // after the source Out fired and before the sink In runs. Both After
        // and Before land here; they differ only in how the author named the
        // Link.
        virtual Status Interpose(Ops &edit, Link link,
                                 const HookBlock::Hook &hook) = 0;
        virtual Status Interpose(Ops &edit, Port source, Port sink,
                                 const HookBlock::Hook &hook) = 0;
    };

    [[nodiscard]] Node Graph() const noexcept { return {1}; }
    [[nodiscard]] Port Entry(int index = 0) const;
    [[nodiscard]] Port Entry(std::string name) const;
    [[nodiscard]] Port Exit(int index = 0) const;
    [[nodiscard]] Port Exit(std::string name) const;

    Node RequireOne(NodePattern pattern);
    Node Each(NodePattern pattern);
    Status Count(Node node, SlotKind kind, int count);
    Status Observe(Port port, Value expected);
    Node Next(Port source);
    Node Next(Port source, NodePattern expected);
    Node Previous(Port sink);
    Node Previous(Port sink, NodePattern expected);
    Link RequireOne(Port source, Port sink,
                    std::optional<int> delay = std::nullopt);
    Link Leaving(Port source);
    Link Entering(Port sink);
    Link To(Port source, Node target);
    // Names a Node or Link the author already holds a reference to, instead of
    // searching the graph for it.
    Node UseNode(const ObjectRef &node);
    Link UseLink(const ObjectRef &link);
    PathRef Follow(Port start);
    Node Add(CKGUID prototype);
    Node Add(PrototypeRef prototype);
    Node Add(BlockSpec block);
    Node AddGraph(std::string name, int priority = 0);
    Node Replace(Node target, BlockSpec block);
    void Remove(Node target);
    ParameterOperation AddOperation(CKGUID operation, CKGUID result,
                                    CKGUID input1, CKGUID input2);

    void Flow(Port source, Port sink, int delay = 0,
              Cycle cycle = Cycle::Reject);
    void Flow(Port source, HookBlock::Hook hook, Port sink);
    void Set(Port target, Parameter::Binding value);
    void Bind(Port target, Parameter::Binding value);
    void Bind(Port target, Port source);
    void Share(Port target, Port source);
    void Push(Port source, Port destination);
    void Tap(Port source, HookBlock::Hook hook);
    void After(PathRef path, HookBlock::Hook hook);
    // Runs a callback on one Link, before the Node that Link feeds. The
    // callback block is infrastructure, so the Logical view of the graph does
    // not change.
    void Before(Link target, HookBlock::Hook hook);
    void Splice(Link target, Node block, std::vector<Order> ordering = {});
    void Splice(Link target, Port input, Port output,
                std::vector<Order> ordering = {});
    // Sends one Link to a different destination. Unlike a Splice, the original
    // destination is dropped while the Patch is open, so the Logical view of
    // the graph reports the new one.
    void Redirect(Link target, Port sink, std::vector<Order> ordering = {});
    // Sends one Link to the destination of another Link without freezing the
    // destination's current In/Exit index into the retained intent.
    void Redirect(Link target, Link destination,
                  std::vector<Order> ordering = {});
    void Reconnect(Link target, Port source, Port sink,
                   Cycle cycle = Cycle::Reject);
    Port AppendIn(Node node, std::string name);
    Port AppendOut(Node node, std::string name);
    Port AppendPin(Node node, std::string name, CKGUID type);
    Port AppendPout(Node node, std::string name, CKGUID type);
    Port AppendLocal(Node node, std::string name, CKGUID type);

    // Enters the graph represented by a Node in this scope. The returned
    // Program owns graph-local symbols and cannot connect them directly to
    // another scope.
    Program &Enter(Node node, std::uint32_t scope);

    struct Nested {
        Node Parent;
        std::uint32_t Scope = 0;
        std::unique_ptr<Program> Body;
    };

    [[nodiscard]] const std::vector<Nested> &NestedGraphs() const noexcept {
        return m_Nested;
    }

    // Checks the retained intent without consulting a CK world. A symbolic
    // Program never owns a world-bound Value.
    [[nodiscard]] Status Validate() const;

    // True once any step anchors on a live ObjectRef, which ties this intent
    // to the world that issued it.
    [[nodiscard]] bool UsesIdentity() const noexcept;

    // Programs compare by authored graph intent, including nested scopes,
    // fixed Prototype generations, literal values, and callback identity.
    // This lets Patch and Plan replacements retain an unchanged prefix.
    [[nodiscard]] bool SameAs(const Program &other) const noexcept;

    // Lowers this scope onto the live graph. Symbols reports which live Edit
    // Node or appended Port each handle resolved to, so an installation can
    // resolve author symbols without retaining a native CK pointer.
    Status Resolve(const PatchKey &patch, const ObjectRef &graph,
                   Resolver &resolver, Ops &out,
                   ResolvedSymbols *symbols = nullptr,
                   bool rootInterfaceExists = false) const;

private:
    [[nodiscard]] std::uint32_t NextNode() noexcept { return ++m_NextNode; }
    [[nodiscard]] std::uint32_t NextLink() noexcept { return ++m_NextLink; }
    [[nodiscard]] std::uint32_t NextPath() noexcept { return ++m_NextPath; }
    [[nodiscard]] Steps::QueryNode *FindQuery(std::uint32_t node) noexcept;
    Port Append(Node node, SlotKind kind, std::string name, CKGUID type);

    std::vector<Step> m_Steps;
    std::vector<Nested> m_Nested;
    std::uint32_t m_NextNode = 1;
    std::uint32_t m_NextLink = 0;
    std::uint32_t m_NextPath = 0;
    std::uint32_t m_NextInterface = 0;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_EDIT_PROGRAM_H
