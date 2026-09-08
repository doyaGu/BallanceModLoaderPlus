#ifndef BML_BEHAVIOR_EDIT_H
#define BML_BEHAVIOR_EDIT_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Behavior/Graph.h"
#include "Behavior/HookBlock.h"
#include "Behavior/Topology.h"

namespace BML::Behavior::Internal {

struct Node;

struct ParameterOperation;

struct Link {
    std::uint32_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return Value != 0; }

    friend bool operator==(const Link &, const Link &) = default;
};

struct Port {
    std::uint32_t Owner = 0;
    Slot Selector;
    // Non-zero only for a Port declared by this Edit. It identifies the
    // declaration independently of its eventual position in the live Layout.
    std::uint32_t Interface = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Owner != 0;
    }


    friend bool operator==(const Port &, const Port &) = default;
};

struct Node {
    std::uint32_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return Value != 0; }

    [[nodiscard]] Port In(int index = 0) const;
    [[nodiscard]] Port In(std::string name) const;
    [[nodiscard]] Port Out(int index = 0) const;
    [[nodiscard]] Port Out(std::string name) const;
    [[nodiscard]] Port Pin(int index = 0) const;
    [[nodiscard]] Port Pin(std::string name) const;
    [[nodiscard]] Port Pout(int index = 0) const;
    [[nodiscard]] Port Pout(std::string name) const;
    [[nodiscard]] Port Local(int index = 0) const;
    [[nodiscard]] Port Local(std::string name) const;
    [[nodiscard]] Port Target() const;

    friend bool operator==(const Node &, const Node &) = default;
};

// Symbolic identity of one CKParameterOperation in an Edit. Its handle shares
// the Edit object namespace with Nodes, but it is never treated as a Behavior
// Node.
struct ParameterOperation {
    std::uint32_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return Value != 0; }
    [[nodiscard]] Port Input(int index) const;
    [[nodiscard]] Port Result() const;

    friend bool operator==(const ParameterOperation &,
                           const ParameterOperation &) = default;
};

enum class Cycle {
    Reject,
    Confirmed,
};

enum class BindKind {
    Literal,
    Direct,
    Shared,
};

enum class NodeRole {
    Logical,
    Infrastructure,
};

struct EditFlow {
    Port Source;
    Port Sink;
    int Delay = 0;
    Cycle SameFrameCycle = Cycle::Reject;
    std::uint32_t Ordinal = 0;

    friend bool operator==(const EditFlow &, const EditFlow &) = default;
};

struct EditBind {
    Port Target;
    BindKind Kind = BindKind::Literal;
    Parameter::Binding Value;
    Port Source;
    std::uint32_t Ordinal = 0;

    friend bool operator==(const EditBind &, const EditBind &) = default;
};

struct EditPush {
    Port Source;
    Port Destination;
    std::uint32_t Ordinal = 0;

    friend bool operator==(const EditPush &, const EditPush &) = default;
};

struct EditTap {
    Port Source;
    std::shared_ptr<HookBlock::Binding> Callback;
    std::uint32_t Ordinal = 0;
};

struct EditSplice {
    Link Target;
    Node Block;
    Port Input;
    Port Output;
    std::vector<Order> Ordering;
    std::uint32_t Ordinal = 0;

    friend bool operator==(const EditSplice &, const EditSplice &) = default;
};

// Sends one Link to a different destination. The original destination is
// journaled so closing the Patch puts the Link back.
struct EditRedirect {
    Link Target;
    Port Sink;
    std::vector<Order> Ordering;
    std::uint32_t Ordinal = 0;

    friend bool operator==(const EditRedirect &, const EditRedirect &) = default;
};

struct EditReplace {
    Node Target;
    Node Replacement;
    std::uint32_t Ordinal = 0;
};

struct CheckedReplace {
    Node Target;
    Node Replacement;
    std::uint32_t Ordinal = 0;
};

struct EditRemove {
    Node Target;
    std::uint32_t Ordinal = 0;
};

struct CheckedRemove {
    Node Target;
    std::uint32_t Ordinal = 0;
};

struct InterfacePort {
    std::uint32_t Identity = 0;
    Node Owner;
    SlotInfo Slot;
    std::uint32_t Ordinal = 0;
    bool InBlockSpec = false;
};

struct ResolvedPort {
    Node Owner;
    Slot Selector;
    SlotInfo Slot;
    std::uint32_t Interface = 0;
    NativeRef Native;
    bool Appended = false;
    bool Operation = false;
};

struct EditOperation {
    ParameterOperation Handle;
    CKGUID Operation = CKGUID();
    CKGUID Result = CKGUID();
    CKGUID Input1 = CKGUID();
    CKGUID Input2 = CKGUID();
    std::uint32_t Ordinal = 0;
};

struct CheckedFlow {
    ResolvedPort Source;
    ResolvedPort Sink;
    int Delay = 0;
    Cycle SameFrameCycle = Cycle::Reject;
    std::uint32_t Ordinal = 0;
};

struct CheckedBind {
    ResolvedPort Target;
    BindKind Kind = BindKind::Literal;
    Parameter::Binding Value;
    ResolvedPort Source;
    std::uint32_t Ordinal = 0;
};

struct CheckedPush {
    ResolvedPort Source;
    ResolvedPort Destination;
    std::uint32_t Ordinal = 0;
};

struct CheckedTap {
    ResolvedPort Source;
    std::shared_ptr<HookBlock::Binding> Callback;
    std::uint32_t Ordinal = 0;
};

struct CheckedSplice {
    LinkBase Target;
    ResolvedPort Input;
    ResolvedPort Output;
    std::vector<Order> Ordering;
    std::uint32_t Ordinal = 0;
};

struct CheckedRedirect {
    LinkBase Target;
    ResolvedPort Sink;
    std::vector<Order> Ordering;
    std::uint32_t Ordinal = 0;
};

struct CheckedEdit {
    std::vector<CheckedFlow> Flows;
    std::vector<CheckedBind> Binds;
    std::vector<CheckedPush> Pushes;
    std::vector<CheckedTap> Taps;
    std::vector<CheckedSplice> Splices;
    std::vector<CheckedRedirect> Redirects;
    std::vector<CheckedReplace> Replacements;
    std::vector<CheckedRemove> Removals;
};

struct GraphSpec {
    std::string Name;
    int Priority = 0;
};

// A side-effect-free additive graph plan. Node and Port values are logical
// plan identities; native CK objects are only resolved by the CK adapter after
// the complete candidate has passed validation.
class Edit final {
public:
    Edit() = default;
    Edit(PatchKey key, NativeRef graph, Layout layout);

    [[nodiscard]] const PatchKey &Key() const noexcept { return m_Key; }
    [[nodiscard]] NativeRef GraphRef() const noexcept {
        return m_Nodes.empty() ? NativeRef{} : m_Nodes.front().Native;
    }
    [[nodiscard]] Node Graph() const noexcept { return {1}; }
    [[nodiscard]] Port Entry(int index = 0) const;
    [[nodiscard]] Port Entry(std::string name) const;
    [[nodiscard]] Port Exit(int index = 0) const;
    [[nodiscard]] Port Exit(std::string name) const;

    Node Use(NativeRef native, Layout layout);
    Link Use(ObjectRef anchor);
    Node Add(BlockSpec block, Layout declared,
             NodeRole role = NodeRole::Logical);
    Node AddGraph(std::string name, int priority = 0,
                  NodeRole role = NodeRole::Logical);
    ParameterOperation AddOperation(CKGUID operation, CKGUID result,
                                    CKGUID input1, CKGUID input2);

    void Flow(Port source, Port sink, int delay = 0,
              Cycle cycle = Cycle::Reject);
    void Bind(Port target, Parameter::Binding value);
    void Bind(Port target, Port source);
    void Share(Port target, Port source);
    void Push(Port source, Port destination);
    void Tap(Port source, std::shared_ptr<HookBlock::Binding> callback);
    void Splice(Link target, Node block, std::vector<Order> ordering = {});
    void Splice(Link target, Port input, Port output,
                std::vector<Order> ordering = {});
    // Sends a Link to a different In or Exit. The Link keeps its source and
    // its delay; only its destination changes, and closing the Patch restores
    // the original destination.
    void Redirect(Link target, Port sink, std::vector<Order> ordering = {});
    void Replace(Node target, Node replacement);
    void Remove(Node target);
    Port AppendIn(Node node, std::string name);
    Port AppendOut(Node node, std::string name);
    Port AppendPin(Node node, std::string name, CKGUID type);
    Port AppendPout(Node node, std::string name, CKGUID type);
    Port AppendLocal(Node node, std::string name, CKGUID type);

    Status Validate(const GraphModel &base, CheckedEdit &out) const;

private:
    struct EditNode {
        Node Handle;
        NativeRef Native;
        Layout Shape;
        std::optional<BlockSpec> Block;
        std::optional<GraphSpec> Subgraph;
        NodeRole Role = NodeRole::Logical;

        [[nodiscard]] bool Authored() const noexcept {
            return Block.has_value() || Subgraph.has_value();
        }
    };

    struct EditLink {
        Link Handle;
        ObjectRef Anchor;
    };

    Port Append(Node node, SlotKind kind, std::string name, CKGUID type,
                bool inBlockSpec = false);
    [[nodiscard]] const EditNode *Find(Node node) const noexcept;
    [[nodiscard]] EditNode *Find(Node node) noexcept;
    [[nodiscard]] const EditOperation *Find(
        ParameterOperation operation) const noexcept;
    [[nodiscard]] std::uint32_t NextOrdinal() noexcept;

    PatchKey m_Key;
    std::vector<EditNode> m_Nodes;
    std::vector<EditOperation> m_Operations;
    std::vector<InterfacePort> m_Interface;
    std::vector<EditFlow> m_Flows;
    std::vector<EditBind> m_Binds;
    std::vector<EditPush> m_Pushes;
    std::vector<EditTap> m_Taps;
    std::vector<EditLink> m_Links;
    std::vector<EditSplice> m_Splices;
    std::vector<EditRedirect> m_Redirects;
    std::vector<EditReplace> m_Replacements;
    std::vector<EditRemove> m_Removals;
    std::uint32_t m_NextNode = 1;
    std::uint32_t m_NextLink = 0;
    std::uint32_t m_NextAction = 1;
    std::uint64_t m_ExpectedFingerprint = 0;

    friend class CKEdit;
    friend class GraphEdit;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_EDIT_H
