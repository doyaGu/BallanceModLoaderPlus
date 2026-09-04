#ifndef BML_BEHAVIOR_GRAPHEDIT_H
#define BML_BEHAVIOR_GRAPHEDIT_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Behavior/Edit.h"
#include "Behavior/PrototypeCatalog.h"

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
// value; no CK object, NativeRef, or Layout is retained. An added Block keeps
// the Prototype provider generation selected by its author so later Plan
// installations cannot silently switch implementations.
// Compile resolves the complete query before the live Edit mutates a graph.
// UseNode and UseLink are the one exception: they anchor on an ObjectRef the
// author already holds, which makes the intent single-world. UsesIdentity
// reports that, and a durable Plan refuses such an edit.
class GraphEdit final {
public:
    using Settings = std::vector<std::pair<Slot, Value>>;
    using SettingStages = std::vector<Settings>;

    class Compiler {
    public:
        virtual ~Compiler() = default;

        virtual Status Begin(const PatchKey &patch, const ObjectRef &graph,
                             Edit &out, GraphModel &base) = 0;
        virtual Status UseNode(Edit &edit, const ObjectRef &node,
                               Node &out) = 0;
        virtual Status UseLink(Edit &edit, const ObjectRef &link,
                               Link &out) = 0;
        // Creates one Block. Settings are part of what the Block is, so they
        // arrive with the Prototype and are written through the Block's own
        // creation lifecycle rather than poked in afterwards.
        virtual Status Add(Edit &edit, PrototypeRef prototype,
                           const SettingStages &settings,
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
    // Declares the value of one Setting of a Block this intent adds. A Setting
    // can rebuild the layout of a block, so only an added Block accepts one.
    Status Setting(Node node, Slot slot, Value value,
                   bool nextStage = false);

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

    // Checks the retained intent without consulting a CK world. A symbolic
    // edit never owns a world-bound Value.
    [[nodiscard]] Status Validate() const;

    // True once any step anchors on a live ObjectRef, which ties this intent
    // to the world that issued it.
    [[nodiscard]] bool UsesIdentity() const noexcept;

    // Nodes reports which live Edit Node each handle of this intent compiled
    // to, so a caller can read the result back after the Edit is applied.
    Status Compile(const PatchKey &patch, const ObjectRef &graph,
                   Compiler &compiler, Edit &out,
                   std::map<std::uint32_t, Node> *nodes = nullptr) const;

private:
    struct EditNode {
        Node Handle;
        NodeQuery Query;
        PrototypeRef Prototype;
        SettingStages Settings;
        bool Added = false;
        // Set instead of Query when the author named the Node by identity.
        ObjectRef Anchor;
    };

    struct EditLink {
        Link Handle;
        Port Source;
        Port Sink;
        std::optional<int> Delay;
        // Set instead of the endpoint query when the author named the Link by
        // identity.
        ObjectRef Anchor;
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

    struct EditBefore {
        Link Target;
        HookBlock::Hook Hook;
        std::uint32_t Ordinal = 0;
    };

    using Action = std::variant<EditFlow, EditBind, EditPush, EditSplice,
                                EditRedirect, EditInterface, EditTap,
                                EditAfter, EditBefore>;

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
