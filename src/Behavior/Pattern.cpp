#include "Behavior/Pattern.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <sstream>
#include <type_traits>

namespace BML::Behavior::Internal {
namespace {

Status Failure(Error error, std::string message) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

bool Readable(SlotKind kind) noexcept {
    return kind == SlotKind::Target ||
        kind == SlotKind::InputParameter ||
        kind == SlotKind::OutputParameter ||
        kind == SlotKind::Setting || kind == SlotKind::Local;
}

std::uint64_t PortShape(const GraphNode &node) {
    constexpr std::uint64_t offset = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    const auto append = [&](const void *data, std::size_t size) {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t index = 0; index < size; ++index) {
            hash ^= bytes[index];
            hash *= prime;
        }
    };
    for (const GraphPort &port : node.Ports) {
        const auto kind = static_cast<std::uint32_t>(port.Kind) + 1u;
        append(&kind, sizeof(kind));
        append(&port.Index, sizeof(port.Index));
        append(&port.Occurrence, sizeof(port.Occurrence));
        append(&port.Type.d1, sizeof(port.Type.d1));
        append(&port.Type.d2, sizeof(port.Type.d2));
        append(port.Name.data(), port.Name.size());
    }
    return hash;
}

Status ResolveSlot(const GraphNode &node, const Slot &selector,
                   const GraphPort *&out) {
    out = nullptr;
    std::vector<const GraphPort *> matches;
    for (const GraphPort &port : node.Ports) {
        if (port.Kind != selector.Kind)
            continue;
        if (selector.ExpectedType.IsValid() &&
            port.Type != selector.ExpectedType)
            continue;
        if (selector.UsesName()) {
            if (port.Name == selector.Name)
                matches.push_back(&port);
        } else if (selector.RequireOnly || port.Index == selector.Index) {
            matches.push_back(&port);
        }
    }
    if (matches.empty())
        return {};
    if ((selector.RequireOnly ||
         (selector.UsesName() && selector.RequireUnique)) &&
        matches.size() != 1)
        return {};
    const int occurrence = selector.UsesName() ? selector.Occurrence : 0;
    if (occurrence < 0 ||
        occurrence >= static_cast<int>(matches.size()))
        return {};
    out = matches[static_cast<std::size_t>(occurrence)];
    return {};
}

template <class T>
bool RawEquals(const Value &expected, const T &actual) {
    return expected.Bytes().size() == sizeof(T) &&
        std::memcmp(expected.Bytes().data(), &actual, sizeof(T)) == 0;
}

template <std::size_t Size>
bool RawArrayEquals(const Value &expected,
                    const std::array<float, Size> &actual) {
    return expected.Bytes().size() == sizeof(actual) &&
        std::memcmp(expected.Bytes().data(), actual.data(),
                    sizeof(actual)) == 0;
}

bool ValueEquals(const Value &expected, const GraphValue &actual) {
    if (actual.State != ValueState::Available ||
        expected.Type() != actual.Type)
        return false;
    if (expected.Kind() == ValueKind::Text) {
        const auto *text = std::get_if<std::string>(&actual.Data);
        return text && *text == expected.StringValue();
    }
    if (expected.Kind() == ValueKind::Null) {
        const auto *object = std::get_if<ObjectRef>(&actual.Data);
        return object && object->IsNull();
    }

    switch (actual.Form) {
    case Parameter::Form::Bool: {
        const auto *value = std::get_if<bool>(&actual.Data);
        const CKBOOL native = value && *value ? TRUE : FALSE;
        return value && RawEquals(expected, native);
    }
    case Parameter::Form::Int32: {
        const auto *value = std::get_if<std::int32_t>(&actual.Data);
        return value && RawEquals(expected, *value);
    }
    case Parameter::Form::Float32: {
        const auto *value = std::get_if<float>(&actual.Data);
        return value && RawEquals(expected, *value);
    }
    case Parameter::Form::Vec2: {
        const auto *value = std::get_if<std::array<float, 2>>(&actual.Data);
        return value && RawArrayEquals(expected, *value);
    }
    case Parameter::Form::Vec3:
    case Parameter::Form::Euler: {
        const auto *value = std::get_if<std::array<float, 3>>(&actual.Data);
        return value && RawArrayEquals(expected, *value);
    }
    case Parameter::Form::Quaternion:
    case Parameter::Form::Rect:
    case Parameter::Form::Color: {
        const auto *value = std::get_if<std::array<float, 4>>(&actual.Data);
        return value && RawArrayEquals(expected, *value);
    }
    case Parameter::Form::Box: {
        const auto *value = std::get_if<std::array<float, 6>>(&actual.Data);
        return value && RawArrayEquals(expected, *value);
    }
    case Parameter::Form::Mat4: {
        const auto *value = std::get_if<std::array<float, 16>>(&actual.Data);
        return value && RawArrayEquals(expected, *value);
    }
    default:
        return false;
    }
}

bool ShapeMatches(const GraphNode &node, const NodePattern &pattern) {
    if (pattern.PortShape && PortShape(node) != pattern.PortShape)
        return false;
    for (const NodePattern::PortCount &condition : pattern.PortCounts) {
        const int count = static_cast<int>(std::count_if(
            node.Ports.begin(), node.Ports.end(),
            [&](const GraphPort &port) { return port.Kind == condition.Kind; }));
        if (count != condition.Count)
            return false;
    }
    return true;
}

} // namespace

NodePattern::operator bool() const noexcept {
    return (Selector == SelectorKind::Index && Index >= 0) ||
        (Selector == SelectorKind::Name && !Name.empty()) ||
        Prototype.IsValid() || ExpectedKind.has_value() || PortShape != 0 ||
        !PortCounts.empty() || !PortValues.empty();
}

Status NodePattern::Validate() const {
    if (!*this)
        return Failure(Error::QueryNotFound,
                       "A Node Pattern has no observable condition.");
    if (Selector == SelectorKind::Index && Index < 0)
        return Failure(Error::InvalidArgument,
                       "A Node Pattern index cannot be negative.");
    if (Selector == SelectorKind::Name &&
        (Name.empty() || Occurrence < 0))
        return Failure(Error::InvalidArgument,
                       "A Node Pattern name or occurrence is invalid.");

    std::set<SlotKind> counted;
    for (const PortCount &condition : PortCounts) {
        if (condition.Count < 0)
            return Failure(Error::InvalidArgument,
                           "A Node Pattern port count cannot be negative.");
        if (!counted.insert(condition.Kind).second)
            return Failure(Error::InvalidArgument,
                           "A Node Pattern repeats one port-count condition.");
    }
    for (const PortValue &condition : PortValues) {
        if (!Readable(condition.Port.Kind))
            return Failure(Error::TypeMismatch,
                           "A Node Pattern can observe only Target, Pin, Pout, Setting, or Local values.");
        if ((!condition.Port.UsesName() &&
             !condition.Port.RequireOnly && condition.Port.Index < 0) ||
            !condition.Expected.Type().IsValid())
            return Failure(Error::InvalidArgument,
                           "A Node Pattern value requires a valid port and Virtools type.");
    }
    return {};
}

Status ResolveAll(const GraphModel &graph, std::uint64_t parent,
                  const NodePattern &pattern, PatternValues &values,
                  std::vector<const GraphNode *> &out) {
    out.clear();
    Status status = pattern.Validate();
    if (!status)
        return status;

    for (const GraphNode &candidate : graph.Nodes) {
        if (candidate.Parent != parent)
            continue;
        if (pattern.Selector == NodePattern::SelectorKind::Index &&
            candidate.Index != pattern.Index)
            continue;
        if (pattern.Selector == NodePattern::SelectorKind::Name &&
            candidate.Name != pattern.Name)
            continue;
        if (pattern.Selector != NodePattern::SelectorKind::Name &&
            !pattern.Name.empty() && candidate.Name != pattern.Name)
            continue;
        if (pattern.Prototype.IsValid() &&
            candidate.Prototype != pattern.Prototype)
            continue;
        if (pattern.ExpectedKind && candidate.Kind != *pattern.ExpectedKind)
            continue;
        if (!ShapeMatches(candidate, pattern))
            continue;

        bool matches = true;
        for (const NodePattern::PortValue &condition : pattern.PortValues) {
            const GraphPort *port = nullptr;
            status = ResolveSlot(candidate, condition.Port, port);
            if (!status)
                return status;
            if (!port) {
                matches = false;
                break;
            }
            GraphValue current;
            Slot slot = Slot::At(port->Kind, port->Index, port->Type);
            status = values.ReadPatternValue(candidate, slot, current);
            if (!status)
                return status;
            if (!ValueEquals(condition.Expected, current)) {
                matches = false;
                break;
            }
        }
        if (matches)
            out.push_back(&candidate);
    }

    if (out.empty())
        return Failure(Error::QueryNotFound,
                       "A Node Pattern matched no Node.");
    std::stable_sort(out.begin(), out.end(),
                     [](const GraphNode *left, const GraphNode *right) {
                         return left->Index < right->Index;
                     });
    return {};
}

Status Resolve(const GraphModel &graph, std::uint64_t parent,
               const NodePattern &pattern, PatternValues &values,
               std::vector<const GraphNode *> &out) {
    Status status = ResolveAll(graph, parent, pattern, values, out);
    if (!status)
        return status;
    if (pattern.Selector == NodePattern::SelectorKind::Name &&
        !pattern.Unique) {
        if (pattern.Occurrence < 0 ||
            pattern.Occurrence >= static_cast<int>(out.size())) {
            out.clear();
            return Failure(Error::QueryNotFound,
                           "A Node Pattern name occurrence does not exist.");
        }
        const GraphNode *selected = out[static_cast<std::size_t>(
            pattern.Occurrence)];
        out.assign(1, selected);
    }
    if (out.size() != 1) {
        std::ostringstream message;
        message << "A Node Pattern matched " << out.size()
                << " Nodes; Require cannot choose between them.";
        out.clear();
        return Failure(Error::QueryAmbiguous, message.str());
    }
    return {};
}

} // namespace BML::Behavior::Internal
