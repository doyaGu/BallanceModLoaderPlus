#ifndef BML_BEHAVIOR_GRAPH_H
#define BML_BEHAVIOR_GRAPH_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "Behavior/Layout.h"
#include "Behavior/ObjectRef.h"
#include "Behavior/Status.h"

namespace BML::Behavior {

enum class GraphView {
    Logical,
    Live,
};

enum class Truth {
    No,
    Yes,
    Unknown,
};

struct NativeRef {
    std::uint64_t Id = 0;
    const void *Address = nullptr;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Id != 0 && Address != nullptr;
    }

    friend bool operator==(const NativeRef &, const NativeRef &) = default;
};

struct GraphPort {
    SlotKind Kind = SlotKind::Input;
    int Index = -1;
    int Occurrence = 0;
    std::string Name;
    bool Active = false;
};

struct GraphNode {
    std::uint64_t Id = 0;
    ObjectRef Object;
    std::uint64_t Parent = 0;
    CKGUID Prototype = CKGUID();
    std::string Name;
    int Priority = 0;
    bool Active = false;
    std::vector<GraphPort> Ports;
};

struct GraphEndpoint {
    std::uint64_t Node = 0;
    SlotKind Kind = SlotKind::Input;
    int Index = -1;

    friend bool operator==(const GraphEndpoint &,
                           const GraphEndpoint &) = default;
};

struct GraphLink {
    std::uint64_t Id = 0;
    ObjectRef Object;
    GraphEndpoint Source;
    GraphEndpoint Target;
    int InitialDelay = 0;
    int RemainingDelay = 0;
    Truth Pending = Truth::Unknown;
};

struct GraphModel {
    GraphView View = GraphView::Logical;
    ObjectRef Root;
    std::uint64_t Generation = 0;
    std::uint64_t Fingerprint = 0;
    std::vector<GraphNode> Nodes;
    std::vector<GraphLink> Links;
};

enum class ReadMode {
    NonForcing,
};

enum class ValueState {
    Available,
    Indeterminate,
    Unsupported,
};

enum class ValueRelation {
    Stored,
    Direct,
    Shared,
    Operation,
};

using ValueData = std::variant<
    std::monostate, bool, std::int32_t, float, std::string,
    std::array<float, 2>, std::array<float, 3>, std::array<float, 4>,
    std::array<float, 6>, std::array<float, 16>, ObjectRef>;

struct GraphValue {
    ValueState State = ValueState::Unsupported;
    ValueRelation Relation = ValueRelation::Stored;
    CKGUID Type = CKGUID();
    Parameter::Form Form = Parameter::Form::Unsupported;
    ValueData Data;

    friend bool operator==(const GraphValue &, const GraphValue &) = default;
};

// The CK adapter owns native identity validation. A NativeRef is never
// dereferenced by the graph model itself.
class GraphSource {
public:
    virtual ~GraphSource() = default;

    virtual Status Refer(void *object, NativeRef &out) = 0;
    virtual Status Read(const NativeRef &root, GraphView view,
                        GraphModel &out) = 0;
    virtual Status ReadLayout(const NativeRef &node, Layout &out) = 0;
    virtual Status ReadValue(const NativeRef &node, const Slot &slot,
                             ReadMode mode, GraphValue &out) = 0;
    virtual Status GraphFingerprint(const NativeRef &root, GraphView view,
                                    std::uint64_t &out) = 0;
    virtual Status LayoutFingerprint(const NativeRef &node,
                                     std::uint64_t &out) = 0;
};

class Runtime;

[[nodiscard]] std::unique_ptr<GraphSource> MakeCKGraphSource(
    CKContext *context, Runtime &runtime,
    std::function<ObjectRef(const void *)> issueObjectRef);

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_GRAPH_H
