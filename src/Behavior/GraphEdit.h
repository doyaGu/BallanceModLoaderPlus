#ifndef BML_BEHAVIOR_GRAPHEDIT_H
#define BML_BEHAVIOR_GRAPHEDIT_H

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Behavior/Block.h"
#include "Behavior/Edit.h"
#include "Behavior/PrototypeCatalog.h"

namespace BML::Behavior::Internal {

// A Node query contains semantic identity only and is resolved against the
// target graph each time this edit is compiled. An empty field is not a
// wildcard authoring shortcut unless the other field identifies the Node.
struct NodeQuery {
    enum class Kind {
        Index,
        Name,
        Only,
    } Selector = Kind::Only;
    int Index = -1;
    std::string Name;
    int Occurrence = 0;
    bool Unique = false;
    CKGUID Prototype = CKGUID();
    std::optional<BehaviorKind> ExpectedKind;
    std::uint64_t PortShape = 0;

    NodeQuery() = default;
    NodeQuery(std::string name, CKGUID prototype = CKGUID())
        : Selector(name.empty() ? Kind::Only : Kind::Name),
          Name(std::move(name)), Unique(true), Prototype(prototype) {}

    [[nodiscard]] explicit operator bool() const noexcept {
        return (Selector == Kind::Index && Index >= 0) ||
            (Selector == Kind::Name && !Name.empty()) ||
            (Selector == Kind::Only && Prototype.IsValid());
    }

    friend bool operator==(const NodeQuery &, const NodeQuery &) = default;
};

struct GraphNodeSpec {
    std::string Name;
    int Priority = 0;

    friend bool operator==(const GraphNodeSpec &, const GraphNodeSpec &) = default;
};

struct PathRef {
    std::uint32_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Value != 0;
    }
    [[nodiscard]] bool operator==(const PathRef &) const noexcept = default;
};

// Canonical graph-edit intent. Handles are symbolic identities within this
// value; no CK object, NativeRef, or Layout is retained. An added Block keeps
// the Prototype provider generation selected by its author so later Plan
// installations cannot silently switch implementations.
// Compile resolves the complete query before the live Edit mutates a graph.
// UseNode and UseLink are the one exception: they anchor on an ObjectRef the
// author already holds, which makes the intent single-world. UsesIdentity
// reports that, and a durable Plan refuses such an edit.
class GraphEdit final {
public:
    class Compiler {
    public:
        virtual ~Compiler() = default;

        virtual Status Begin(const PatchKey &patch, const ObjectRef &graph,
                             Edit &out, GraphModel &base) = 0;
        virtual Status UseNode(Edit &edit, const ObjectRef &node,
                               Node &out) = 0;
        virtual Status UseLink(Edit &edit, const ObjectRef &link,
                               Link &out) = 0;
        virtual Status Add(Edit &edit, BlockSpec block, Node &out) = 0;
        virtual Status AddGraph(Edit &edit, std::string name, int priority,
                                Node &out) = 0;
        virtual Status Tap(Edit &edit, Port source,
                           const HookBlock::Hook &hook) = 0;
        // Puts a callback inside one Link, so control reaches the callback
        // after the source Out fired and before the sink In runs. Both After
        // and Before land here; they differ only in how the author named the
        // Link.
        virtual Status Interpose(Edit &edit, Link link,
                                 const HookBlock::Hook &hook) = 0;
    };

    [[nodiscard]] Node Graph() const noexcept { return {1}; }
    [[nodiscard]] Port Entry(int index = 0) const;
    [[nodiscard]] Port Entry(std::string name) const;
    [[nodiscard]] Port Exit(int index = 0) const;
    [[nodiscard]] Port Exit(std::string name) const;

    Node RequireOne(NodeQuery query);
    Link RequireOne(Port source, Port sink,
                    std::optional<int> delay = std::nullopt);
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
    void Bind(Port target, Value value);
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
    Port AppendIn(Node node, std::string name);
    Port AppendOut(Node node, std::string name);
    Port AppendPin(Node node, std::string name, CKGUID type);
    Port AppendPout(Node node, std::string name, CKGUID type);
    Port AppendLocal(Node node, std::string name, CKGUID type);

    // Enters the graph represented by a Node in this scope. The returned
    // GraphEdit owns graph-local symbols and cannot connect them directly to
    // another scope.
    GraphEdit &Enter(Node node, std::uint32_t scope);

    struct Nested {
        Node Parent;
        std::uint32_t Scope = 0;
        std::unique_ptr<GraphEdit> Body;
    };

    [[nodiscard]] const std::vector<Nested> &NestedGraphs() const noexcept {
        return m_Nested;
    }

    // Checks the retained intent without consulting a CK world. A symbolic
    // edit never owns a world-bound Value.
    [[nodiscard]] Status Validate() const;

    // True once any step anchors on a live ObjectRef, which ties this intent
    // to the world that issued it.
    [[nodiscard]] bool UsesIdentity() const noexcept;

    // Definitions compare by authored graph intent, including nested scopes,
    // fixed Prototype generations, literal values, and callback identity.
    // This lets Patch and Plan replacements retain an unchanged prefix.
    [[nodiscard]] bool SameAs(const GraphEdit &other) const noexcept;

    // Nodes reports which live Edit Node each handle of this intent compiled
    // to, so a caller can read the result back after the Edit is applied.
    Status Compile(const PatchKey &patch, const ObjectRef &graph,
                   Compiler &compiler, Edit &out,
                   std::map<std::uint32_t, Node> *nodes = nullptr,
                   bool rootInterfaceExists = false) const;

private:
    struct EditNode {
        Node Handle;
        NodeQuery Query;
        std::optional<BlockSpec> Block;
        std::optional<GraphNodeSpec> Subgraph;
        // Set instead of Query when the author named the Node by identity.
        ObjectRef Anchor;

        [[nodiscard]] bool Authored() const noexcept {
            return Block.has_value() || Subgraph.has_value();
        }


        friend bool operator==(const EditNode &, const EditNode &) = default;
    };

    struct EditLink {
        Link Handle;
        Port Source;
        Port Sink;
        std::optional<int> Delay;
        // Set instead of the endpoint query when the author named the Link by
        // identity.
        ObjectRef Anchor;

        friend bool operator==(const EditLink &, const EditLink &) = default;
    };

    struct Operation {
        ParameterOperation Handle;
        CKGUID Guid = CKGUID();
        CKGUID Result = CKGUID();
        CKGUID Input1 = CKGUID();
        CKGUID Input2 = CKGUID();

        friend bool operator==(const Operation &, const Operation &) = default;
    };

    struct EditInterface {
        Port Handle;
        Node Owner;
        SlotKind Kind = SlotKind::Input;
        std::string Name;
        CKGUID Type = CKGUID();
        std::uint32_t Ordinal = 0;

        friend bool operator==(const EditInterface &,
                               const EditInterface &) = default;
    };

    struct EditPath {
        PathRef Handle;
        Port Start;

        friend bool operator==(const EditPath &, const EditPath &) = default;
    };

    struct EditTap {
        Port Source;
        HookBlock::Hook Hook;
        std::uint32_t Ordinal = 0;

        friend bool operator==(const EditTap &, const EditTap &) = default;
    };

    struct EditAfter {
        PathRef Target;
        HookBlock::Hook Hook;
        std::uint32_t Ordinal = 0;

        friend bool operator==(const EditAfter &, const EditAfter &) = default;
    };

    struct EditBefore {
        Link Target;
        HookBlock::Hook Hook;
        std::uint32_t Ordinal = 0;

        friend bool operator==(const EditBefore &, const EditBefore &) = default;
    };

    struct EditReplace {
        Node Target;
        Node Replacement;
        std::uint32_t Ordinal = 0;

        friend bool operator==(const EditReplace &, const EditReplace &) = default;
    };

    struct EditRemove {
        Node Target;
        std::uint32_t Ordinal = 0;

        friend bool operator==(const EditRemove &, const EditRemove &) = default;
    };

    using Action = std::variant<EditFlow, EditBind, EditPush, EditSplice,
                                EditRedirect, EditInterface, EditTap,
                                EditAfter, EditBefore>;

    [[nodiscard]] std::uint32_t NextNode() noexcept { return ++m_NextNode; }
    [[nodiscard]] std::uint32_t NextLink() noexcept { return ++m_NextLink; }
    [[nodiscard]] std::uint32_t NextPath() noexcept { return ++m_NextPath; }
    Port Append(Node node, SlotKind kind, std::string name, CKGUID type);

    std::vector<EditNode> m_Nodes;
    std::vector<Operation> m_Operations;
    std::vector<EditLink> m_Links;
    std::vector<EditPath> m_Paths;
    std::vector<EditReplace> m_Replacements;
    std::vector<EditRemove> m_Removals;
    std::vector<Action> m_Actions;
    std::vector<Nested> m_Nested;
    std::uint32_t m_NextNode = 1;
    std::uint32_t m_NextLink = 0;
    std::uint32_t m_NextPath = 0;
    std::uint32_t m_NextInterface = 0;
    std::uint32_t m_NextAction = 1;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_GRAPHEDIT_H
