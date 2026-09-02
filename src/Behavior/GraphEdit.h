#ifndef BML_BEHAVIOR_GRAPHEDIT_H
#define BML_BEHAVIOR_GRAPHEDIT_H

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Behavior/Edit.h"

namespace BML::Behavior {

// A Node query contains semantic identity only and is resolved against the
// target graph each time this edit is compiled. An empty field is not a
// wildcard authoring shortcut unless the other field identifies the Node.
struct NodeQuery {
    std::string Name;
    CKGUID Prototype = CKGUID();

    [[nodiscard]] explicit operator bool() const noexcept {
        return !Name.empty() || Prototype.IsValid();
    }
};

struct PathRef {
    std::uint32_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Value != 0;
    }
    [[nodiscard]] bool operator==(const PathRef &) const noexcept = default;
};

// Canonical graph-edit intent. Handles are symbolic identities within this
// value; no CK object, ObjectRef, NativeRef, Layout, or provider generation is
// retained. Compile resolves the complete query before the live Edit mutates
// a graph.
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
        virtual Status Add(Edit &edit, CKGUID prototype, Node &out) = 0;
        virtual Status Tap(Edit &edit, Port source,
                           const HookBlock::Hook &hook) = 0;
        virtual Status After(Edit &edit, Link link,
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
    PathRef Follow(Port start);
    Node Add(CKGUID prototype);

    void Flow(Port source, Port sink, int delay = 0,
              Cycle cycle = Cycle::Reject);
    void Bind(Port target, Value value);
    void Bind(Port target, Port source);
    void Share(Port target, Port source);
    void Push(Port source, Port destination);
    void Tap(Port source, HookBlock::Hook hook);
    void After(PathRef path, HookBlock::Hook hook);
    void Splice(Link target, Node block, std::vector<Order> ordering = {});
    void Splice(Link target, Port input, Port output,
                std::vector<Order> ordering = {});
    Port AppendIn(Node node, std::string name);
    Port AppendOut(Node node, std::string name);
    Port AppendPin(Node node, std::string name, CKGUID type);
    Port AppendPout(Node node, std::string name, CKGUID type);
    Port AppendLocal(Node node, std::string name, CKGUID type);

    // Checks the retained intent without consulting a CK world. A symbolic
    // edit never owns a world-bound Value.
    [[nodiscard]] Status Validate() const;

    Status Compile(const PatchKey &patch, const ObjectRef &graph,
                   Compiler &compiler, Edit &out) const;

private:
    struct EditNode {
        Node Handle;
        NodeQuery Query;
        CKGUID Prototype = CKGUID();
        bool Added = false;
    };

    struct EditLink {
        Link Handle;
        Port Source;
        Port Sink;
        std::optional<int> Delay;
    };

    struct EditInterface {
        Port Handle;
        Node Owner;
        SlotKind Kind = SlotKind::Input;
        std::string Name;
        CKGUID Type = CKGUID();
        std::uint32_t Ordinal = 0;
    };

    struct EditPath {
        PathRef Handle;
        Port Start;
    };

    struct EditTap {
        Port Source;
        HookBlock::Hook Hook;
        std::uint32_t Ordinal = 0;
    };

    struct EditAfter {
        PathRef Target;
        HookBlock::Hook Hook;
        std::uint32_t Ordinal = 0;
    };

    using Action = std::variant<EditFlow, EditBind, EditPush, EditSplice,
                                EditInterface, EditTap,
                                EditAfter>;

    [[nodiscard]] std::uint32_t NextNode() noexcept { return ++m_NextNode; }
    [[nodiscard]] std::uint32_t NextLink() noexcept { return ++m_NextLink; }
    [[nodiscard]] std::uint32_t NextPath() noexcept { return ++m_NextPath; }
    Port Append(Node node, SlotKind kind, std::string name, CKGUID type);

    std::vector<EditNode> m_Nodes;
    std::vector<EditLink> m_Links;
    std::vector<EditPath> m_Paths;
    std::vector<Action> m_Actions;
    std::uint32_t m_NextNode = 1;
    std::uint32_t m_NextLink = 0;
    std::uint32_t m_NextPath = 0;
    std::uint32_t m_NextInterface = 0;
    std::uint32_t m_NextAction = 1;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_GRAPHEDIT_H
