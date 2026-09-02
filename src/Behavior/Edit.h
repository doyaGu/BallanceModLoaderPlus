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

namespace BML::Behavior {

struct Node;

struct Link {
    std::uint32_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return Value != 0; }

    friend bool operator==(const Link &, const Link &) = default;
};

struct Port {
    std::uint32_t Owner = 0;
    Slot Selector;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Owner != 0;
    }
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

enum class Cycle {
    Reject,
    Confirmed,
};

enum class BindKind {
    Literal,
    Direct,
    Shared,
};

struct EditFlow {
    Port Source;
    Port Sink;
    int Delay = 0;
    Cycle SameFrameCycle = Cycle::Reject;
    std::uint32_t Ordinal = 0;
};

struct EditBind {
    Port Target;
    BindKind Kind = BindKind::Literal;
    Value Literal;
    Port Source;
    std::uint32_t Ordinal = 0;
};

struct EditPush {
    Port Source;
    Port Destination;
    std::uint32_t Ordinal = 0;
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
};

struct InterfacePort {
    Node Owner;
    SlotInfo Slot;
    std::uint32_t Ordinal = 0;
    bool InBlockSpec = false;
};

struct ResolvedPort {
    Node Owner;
    Slot Selector;
    SlotInfo Slot;
    bool Appended = false;
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
    Value Literal;
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

struct CheckedEdit {
    std::vector<CheckedFlow> Flows;
    std::vector<CheckedBind> Binds;
    std::vector<CheckedPush> Pushes;
    std::vector<CheckedTap> Taps;
    std::vector<CheckedSplice> Splices;
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
    Node Add(Spec block, Layout declared);

    void Flow(Port source, Port sink, int delay = 0,
              Cycle cycle = Cycle::Reject);
    void Bind(Port target, Value value);
    void Bind(Port target, Port source);
    void Share(Port target, Port source);
    void Push(Port source, Port destination);
    void Tap(Port source, std::shared_ptr<HookBlock::Binding> callback);
    void Splice(Link target, Node block, std::vector<Order> ordering = {});
    void Splice(Link target, Port input, Port output,
                std::vector<Order> ordering = {});
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
        std::optional<Spec> Block;
    };

    struct EditLink {
        Link Handle;
        ObjectRef Anchor;
    };

    Port Append(Node node, SlotKind kind, std::string name, CKGUID type,
                bool inBlockSpec = false);
    [[nodiscard]] const EditNode *Find(Node node) const noexcept;
    [[nodiscard]] EditNode *Find(Node node) noexcept;
    [[nodiscard]] std::uint32_t NextOrdinal() noexcept;

    PatchKey m_Key;
    std::vector<EditNode> m_Nodes;
    std::vector<InterfacePort> m_Interface;
    std::vector<EditFlow> m_Flows;
    std::vector<EditBind> m_Binds;
    std::vector<EditPush> m_Pushes;
    std::vector<EditTap> m_Taps;
    std::vector<EditLink> m_Links;
    std::vector<EditSplice> m_Splices;
    std::uint32_t m_NextNode = 1;
    std::uint32_t m_NextLink = 0;
    std::uint32_t m_NextAction = 1;
    std::uint64_t m_ExpectedFingerprint = 0;

    friend class CKEdit;
    friend class GraphEdit;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_EDIT_H
