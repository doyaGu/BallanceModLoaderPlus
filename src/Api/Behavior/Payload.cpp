#include "Api/Behavior/Codec.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "BML/ImcWire.hpp"
#include "Behavior/Pattern.h"
#include "Loader/ModContext.h"

namespace BML::Api::Behavior {
namespace {

using BML::Behavior::Internal::ManagerRequirement;
using BML::Behavior::Internal::Truth;
using BML::Behavior::Internal::Value;
using BML::Behavior::Internal::ValueRelation;
using BML::Behavior::Internal::ValueState;
using BML::Behavior::Internal::WatchEvent;
using BML::Behavior::Internal::WatchKind;

class BehaviorPayload final {
public:
    template <typename T>
    bool Reserve(std::size_t count, std::uint32_t &offset) {
        if (!Align(alignof(T)) || count > UINT32_MAX / sizeof(T))
            return false;
        const std::size_t size = count * sizeof(T);
        if (m_Bytes.size() > UINT32_MAX ||
            size > UINT32_MAX - m_Bytes.size())
            return false;
        offset = static_cast<std::uint32_t>(m_Bytes.size());
        m_Bytes.resize(m_Bytes.size() + size, 0);
        return true;
    }

    template <typename T>
    void Store(std::uint32_t offset, std::size_t index, const T &record) {
        std::memcpy(m_Bytes.data() + offset + index * sizeof(T),
                    &record, sizeof(record));
    }

    bool Text(std::string_view native, BML_BehaviorText &text) {
        std::string storage;
        const std::string_view value = Utf8Text(native, storage);
        if (value.size() > UINT32_MAX || m_Bytes.size() > UINT32_MAX ||
            value.size() > UINT32_MAX - m_Bytes.size())
            return false;
        text.Offset = static_cast<std::uint32_t>(m_Bytes.size());
        text.Length = static_cast<std::uint32_t>(value.size());
        m_Bytes.insert(m_Bytes.end(), value.begin(), value.end());
        return true;
    }

    bool Bytes(const void *value, std::size_t size, std::uint32_t &offset) {
        if ((!value && size) || m_Bytes.size() > UINT32_MAX ||
            size > UINT32_MAX - m_Bytes.size())
            return false;
        offset = static_cast<std::uint32_t>(m_Bytes.size());
        const auto *bytes = static_cast<const std::uint8_t *>(value);
        if (size)
            m_Bytes.insert(m_Bytes.end(), bytes, bytes + size);
        return true;
    }

    bool Value(const void *value, std::size_t size, std::uint32_t &offset) {
        return Align(BML_BEHAVIOR_VALUE_ALIGNMENT) &&
            Bytes(value, size, offset);
    }

    [[nodiscard]] const std::vector<std::uint8_t> &Bytes() const noexcept {
        return m_Bytes;
    }

private:
    bool Align(std::size_t alignment) {
        const std::size_t remainder = m_Bytes.size() % alignment;
        if (!remainder)
            return true;
        const std::size_t padding = alignment - remainder;
        if (m_Bytes.size() > UINT32_MAX ||
            padding > UINT32_MAX - m_Bytes.size())
            return false;
        m_Bytes.insert(m_Bytes.end(), padding, 0);
        return true;
    }

    std::vector<std::uint8_t> m_Bytes;
};

std::uint32_t PublicValueKind(Parameter::Form form) noexcept {
    switch (form) {
    case Parameter::Form::Bool: return BML_BEHAVIOR_VALUE_BOOL;
    case Parameter::Form::Int32: return BML_BEHAVIOR_VALUE_INT32;
    case Parameter::Form::Float32: return BML_BEHAVIOR_VALUE_FLOAT32;
    case Parameter::Form::Utf8: return BML_BEHAVIOR_VALUE_UTF8;
    case Parameter::Form::Vec2: return BML_BEHAVIOR_VALUE_VEC2;
    case Parameter::Form::Vec3: return BML_BEHAVIOR_VALUE_VEC3;
    case Parameter::Form::Quaternion: return BML_BEHAVIOR_VALUE_QUATERNION;
    case Parameter::Form::Euler: return BML_BEHAVIOR_VALUE_EULER;
    case Parameter::Form::Rect: return BML_BEHAVIOR_VALUE_RECT;
    case Parameter::Form::Color: return BML_BEHAVIOR_VALUE_COLOR;
    case Parameter::Form::Box: return BML_BEHAVIOR_VALUE_BOX;
    case Parameter::Form::Mat4: return BML_BEHAVIOR_VALUE_MAT4;
    case Parameter::Form::Object: return BML_BEHAVIOR_VALUE_OBJECT;
    case Parameter::Form::Unsupported: return 0;
    }
    return 0;
}

std::uint32_t PublicSlotKind(SlotKind kind) noexcept {
    switch (kind) {
    case SlotKind::Input: return BML_BEHAVIOR_SLOT_IN;
    case SlotKind::Output: return BML_BEHAVIOR_SLOT_OUT;
    case SlotKind::InputParameter: return BML_BEHAVIOR_SLOT_PIN;
    case SlotKind::OutputParameter: return BML_BEHAVIOR_SLOT_POUT;
    case SlotKind::Setting: return BML_BEHAVIOR_SLOT_SETTING;
    case SlotKind::Local: return BML_BEHAVIOR_SLOT_LOCAL;
    case SlotKind::Target: return BML_BEHAVIOR_SLOT_TARGET;
    }
    return 0;
}

bool AddManagers(const std::vector<ManagerRequirement> &managers,
                 BehaviorPayload &payload, std::uint32_t &offset,
                 std::uint32_t &count) {
    if (managers.size() > UINT32_MAX)
        return false;
    count = static_cast<std::uint32_t>(managers.size());
    if (managers.empty())
        return true;
    if (!payload.Reserve<BML_BehaviorManagerInfo>(managers.size(), offset))
        return false;
    for (std::size_t index = 0; index < managers.size(); ++index) {
        BML_BehaviorManagerInfo manager{};
        manager.StructSize = sizeof(manager);
        manager.Guid = Guid(managers[index].Guid);
        manager.Available = managers[index].Available ? 1u : 0u;
        payload.Store(offset, index, manager);
    }
    return true;
}

bool AddPrototype(const PrototypeInfo &prototype, BehaviorPayload &payload,
                  BML_BehaviorPrototypeInfo &record) {
    record = {};
    record.StructSize = sizeof(record);
    record.Ref.StructSize = sizeof(record.Ref);
    record.Ref.Prototype = Guid(prototype.Ref.Guid);
    record.Ref.Generation = prototype.Ref.Generation;
    record.Provider = Guid(prototype.Provider.Guid);
    record.Version = prototype.Version;
    record.CompatibleClass = prototype.CompatibleClass;
    return payload.Text(prototype.Name, record.Name) &&
           payload.Text(prototype.Category, record.Category) &&
           payload.Text(prototype.Provider.Name, record.ProviderName) &&
           payload.Text(prototype.Author, record.Author) &&
           payload.Text(prototype.Description, record.Description) &&
           AddManagers(prototype.Managers, payload, record.ManagerOffset,
                       record.ManagerCount);
}

bool AddLayout(const Layout &from, BehaviorPayload &payload,
               BML_BehaviorLayout &record) {
    record = {};
    record.StructSize = sizeof(record);
    record.Origin = from.Origin == BML::Behavior::Internal::LayoutOrigin::Declared
        ? BML_BEHAVIOR_LAYOUT_DECLARED : BML_BEHAVIOR_LAYOUT_LIVE;
    record.Prototype.StructSize = sizeof(record.Prototype);
    record.Prototype.Prototype = Guid(from.Prototype);
    record.Prototype.Generation = from.ProviderGeneration;
    record.LayoutGeneration = from.Generation;
    switch (from.Kind) {
    case BML::Behavior::Internal::BehaviorKind::Function:
        record.Kind = BML_BEHAVIOR_KIND_FUNCTION;
        break;
    case BML::Behavior::Internal::BehaviorKind::Callback:
        record.Kind = BML_BEHAVIOR_KIND_CALLBACK;
        break;
    case BML::Behavior::Internal::BehaviorKind::Graph:
        record.Kind = BML_BEHAVIOR_KIND_GRAPH;
        break;
    }
    record.CompatibleClass = from.CompatibleClass;
    record.PrototypeFlags = from.PrototypeFlags;
    record.BehaviorFlags = from.BehaviorFlags;
    record.TargetType = Guid(from.TargetType);
    if (!payload.Text(from.PrototypeName, record.Name) ||
        !payload.Text(from.Category, record.Category) ||
        !payload.Text(from.ProviderName, record.ProviderName) ||
        !payload.Text(from.Author, record.Author) ||
        !payload.Text(from.Description, record.Description) ||
        !AddManagers(from.Managers, payload, record.ManagerOffset,
                     record.ManagerCount))
        return false;
    if (from.Slots.size() > UINT32_MAX)
        return false;
    record.SlotCount = static_cast<std::uint32_t>(from.Slots.size());
    if (from.Slots.empty())
        return true;
    if (!payload.Reserve<BML_BehaviorSlotRecord>(
            from.Slots.size(), record.SlotOffset))
        return false;
    for (std::size_t index = 0; index < from.Slots.size(); ++index) {
        const BML::Behavior::Internal::SlotInfo &slot = from.Slots[index];
        BML_BehaviorSlotRecord output{};
        output.StructSize = sizeof(output);
        output.Kind = PublicSlotKind(slot.Kind);
        if (slot.Dynamic)
            output.Flags |= BML_BEHAVIOR_SLOT_DYNAMIC;
        output.ValueKind = PublicValueKind(slot.ValueForm);
        if (output.ValueKind)
            output.Flags |= BML_BEHAVIOR_SLOT_VALUE_SUPPORTED;
        output.Index = slot.Index;
        output.Occurrence = slot.Occurrence;
        output.Type = Guid(slot.Type);
        if (!payload.Text(slot.Name, output.Name) ||
            !payload.Text(slot.TypeName, output.TypeName))
            return false;
        payload.Store(record.SlotOffset, index, output);
    }
    return true;
}

std::uint32_t PublicTruth(Truth value) noexcept {
    switch (value) {
    case Truth::No: return BML_BEHAVIOR_FALSE;
    case Truth::Yes: return BML_BEHAVIOR_TRUE;
    case Truth::Unknown: return BML_BEHAVIOR_UNKNOWN;
    }
    return BML_BEHAVIOR_UNKNOWN;
}

std::uint32_t PublicValueState(ValueState state) noexcept {
    switch (state) {
    case ValueState::Available: return BML_BEHAVIOR_VALUE_AVAILABLE;
    case ValueState::Indeterminate: return BML_BEHAVIOR_VALUE_INDETERMINATE;
    case ValueState::Unsupported: return BML_BEHAVIOR_VALUE_UNSUPPORTED;
    }
    return BML_BEHAVIOR_VALUE_UNSUPPORTED;
}

std::uint32_t PublicRelation(ValueRelation relation) noexcept {
    switch (relation) {
    case ValueRelation::Stored: return BML_BEHAVIOR_VALUE_STORED;
    case ValueRelation::Direct: return BML_BEHAVIOR_VALUE_DIRECT;
    case ValueRelation::Shared: return BML_BEHAVIOR_VALUE_SHARED;
    case ValueRelation::Operation: return BML_BEHAVIOR_VALUE_OPERATION;
    }
    return BML_BEHAVIOR_VALUE_STORED;
}

std::uint32_t PublicWatchKind(WatchKind kind) noexcept {
    switch (kind) {
    case WatchKind::GraphChanged: return BML_BEHAVIOR_WATCH_GRAPH;
    case WatchKind::LayoutChanged: return BML_BEHAVIOR_WATCH_LAYOUT;
    case WatchKind::SampledValueChanged:
        return BML_BEHAVIOR_WATCH_SAMPLED_VALUE;
    }
    return 0;
}

std::uint32_t PublicWatchState(BML::Behavior::Internal::WatchState state) noexcept {
    switch (state) {
    case BML::Behavior::Internal::WatchState::Active:
        return BML_BEHAVIOR_WATCH_ACTIVE;
    case BML::Behavior::Internal::WatchState::Failed:
        return BML_BEHAVIOR_WATCH_FAILED;
    }
    return BML_BEHAVIOR_WATCH_FAILED;
}

bool AddGraph(const GraphModel &source, BehaviorPayload &payload,
              BML_BehaviorGraph &graph) {
    graph = {};
    graph.StructSize = sizeof(graph);
    graph.View = source.View == GraphView::Logical
        ? BML_BEHAVIOR_GRAPH_LOGICAL : BML_BEHAVIOR_GRAPH_LIVE;
    graph.Root = {source.Root.Domain, source.Root.Slot, source.Root.Generation};
    graph.Generation = source.Generation;
    graph.Fingerprint = source.Fingerprint;
    if (source.Nodes.size() > UINT32_MAX || source.Links.size() > UINT32_MAX ||
        source.Operations.size() > UINT32_MAX)
        return false;
    graph.NodeCount = static_cast<std::uint32_t>(source.Nodes.size());
    graph.LinkCount = static_cast<std::uint32_t>(source.Links.size());
    graph.OperationCount = static_cast<std::uint32_t>(source.Operations.size());
    if (!source.Nodes.empty() &&
        !payload.Reserve<BML_BehaviorGraphNode>(
            source.Nodes.size(), graph.NodeOffset))
        return false;
    if (!source.Links.empty() &&
        !payload.Reserve<BML_BehaviorGraphLink>(
            source.Links.size(), graph.LinkOffset))
        return false;
    if (!source.Operations.empty() &&
        !payload.Reserve<BML_BehaviorGraphOperation>(
            source.Operations.size(), graph.OperationOffset))
        return false;

    for (std::size_t index = 0; index < source.Nodes.size(); ++index) {
        const auto &node = source.Nodes[index];
        BML_BehaviorGraphNode record{};
        record.StructSize = sizeof(record);
        record.Id = node.Id;
        record.Object = {node.Object.Domain, node.Object.Slot,
                         node.Object.Generation};
        record.Parent = node.Parent;
        record.Index = node.Index;
        record.Occurrence = node.Occurrence;
        switch (node.Kind) {
        case BML::Behavior::Internal::BehaviorKind::Function:
            record.Kind = BML_BEHAVIOR_KIND_FUNCTION;
            break;
        case BML::Behavior::Internal::BehaviorKind::Callback:
            record.Kind = BML_BEHAVIOR_KIND_CALLBACK;
            break;
        case BML::Behavior::Internal::BehaviorKind::Graph:
            record.Kind = BML_BEHAVIOR_KIND_GRAPH;
            break;
        }
        switch (node.Role) {
        case BML::Behavior::Internal::NodeRole::Logical:
            record.Role = BML_BEHAVIOR_NODE_LOGICAL;
            break;
        case BML::Behavior::Internal::NodeRole::Infrastructure:
            record.Role = BML_BEHAVIOR_NODE_INFRASTRUCTURE;
            break;
        case BML::Behavior::Internal::NodeRole::Retired:
            record.Role = BML_BEHAVIOR_NODE_RETIRED;
            break;
        }
        record.LayoutGeneration = node.LayoutGeneration;
        record.Shape = BML::Behavior::Internal::PortShape(node);
        record.Prototype = Guid(node.Prototype);
        record.Priority = node.Priority;
        record.Active = node.Active ? 1u : 0u;
        if (!payload.Text(node.Name, record.Name) ||
            node.Ports.size() > UINT32_MAX)
            return false;
        record.PortCount = static_cast<std::uint32_t>(node.Ports.size());
        if (!node.Ports.empty() &&
            !payload.Reserve<BML_BehaviorGraphPort>(
                node.Ports.size(), record.PortOffset))
            return false;
        for (std::size_t portIndex = 0;
             portIndex < node.Ports.size(); ++portIndex) {
            const auto &port = node.Ports[portIndex];
            BML_BehaviorGraphPort portRecord{};
            portRecord.StructSize = sizeof(portRecord);
            portRecord.Node = node.Id;
            portRecord.LayoutGeneration = port.LayoutGeneration;
            portRecord.Kind = PublicSlotKind(port.Kind);
            if (port.Dynamic)
                portRecord.Flags |= BML_BEHAVIOR_SLOT_DYNAMIC;
            portRecord.Index = port.Index;
            portRecord.Occurrence = port.Occurrence;
            portRecord.Type = Guid(port.Type);
            portRecord.Active = port.Active ? 1u : 0u;
            if (!payload.Text(port.Name, portRecord.Name))
                return false;
            payload.Store(record.PortOffset, portIndex, portRecord);
        }
        payload.Store(graph.NodeOffset, index, record);
    }

    for (std::size_t index = 0; index < source.Links.size(); ++index) {
        const auto &link = source.Links[index];
        BML_BehaviorGraphLink record{};
        record.StructSize = sizeof(record);
        record.Id = link.Id;
        record.Object = {link.Object.Domain, link.Object.Slot,
                         link.Object.Generation};
        record.SourceNode = link.Source.Node;
        record.SourceKind = PublicSlotKind(link.Source.Kind);
        record.SourceIndex = link.Source.Index;
        record.TargetNode = link.Target.Node;
        record.TargetKind = PublicSlotKind(link.Target.Kind);
        record.TargetIndex = link.Target.Index;
        record.SourceOrder = link.SourceOrder;
        record.InitialDelay = link.InitialDelay;
        record.RemainingDelay = link.RemainingDelay;
        record.Pending = PublicTruth(link.Pending);
        payload.Store(graph.LinkOffset, index, record);
    }
    for (std::size_t index = 0; index < source.Operations.size(); ++index) {
        const auto &operation = source.Operations[index];
        BML_BehaviorGraphOperation record{};
        record.StructSize = sizeof(record);
        record.Id = operation.Id;
        record.Object = {operation.Object.Domain, operation.Object.Slot,
                         operation.Object.Generation};
        record.Owner = operation.Owner;
        record.Function = Guid(operation.Function);
        record.Result = Guid(operation.Result);
        record.Input1 = Guid(operation.Input1);
        record.Input2 = Guid(operation.Input2);
        if (!payload.Text(operation.Name, record.Name))
            return false;
        payload.Store(graph.OperationOffset, index, record);
    }
    return true;
}

bool AddGraphValue(const GraphValue &source, BehaviorPayload &payload,
                   BML_BehaviorGraphValue &record) {
    record = {};
    record.StructSize = sizeof(record);
    record.State = PublicValueState(source.State);
    record.Relation = PublicRelation(source.Relation);
    record.Type = Guid(source.Type);
    if (source.State != ValueState::Available)
        return true;
    record.Kind = PublicValueKind(source.Form);
    if (!record.Kind)
        return false;

    std::uint8_t bytes[64]{};
    std::size_t size = 0;
    const auto storeFloat = [&](std::size_t index, float value) {
        BML::Imc::Wire::Detail::Store32(
            bytes + index * 4, std::bit_cast<std::uint32_t>(value));
    };
    switch (source.Form) {
    case Parameter::Form::Bool: {
        const bool *value = std::get_if<bool>(&source.Data);
        if (!value)
            return false;
        BML::Imc::Wire::Detail::Store32(bytes, *value ? 1u : 0u);
        size = 4;
        break;
    }
    case Parameter::Form::Int32: {
        const auto *value = std::get_if<std::int32_t>(&source.Data);
        if (!value)
            return false;
        BML::Imc::Wire::Detail::Store32(
            bytes, static_cast<std::uint32_t>(*value));
        size = 4;
        break;
    }
    case Parameter::Form::Float32: {
        const float *value = std::get_if<float>(&source.Data);
        if (!value)
            return false;
        storeFloat(0, *value);
        size = 4;
        break;
    }
    case Parameter::Form::Utf8: {
        const std::string *native = std::get_if<std::string>(&source.Data);
        if (!native)
            return false;
        std::string storage;
        const std::string_view value = Utf8Text(*native, storage);
        if (value.size() > UINT32_MAX)
            return false;
        record.ValueSize = static_cast<std::uint32_t>(value.size());
        return payload.Value(value.data(), value.size(), record.ValueOffset);
    }
    case Parameter::Form::Object: {
        const auto *value = std::get_if<BML::Behavior::Internal::ObjectRef>(&source.Data);
        if (!value)
            return false;
        BML::Imc::Wire::Detail::Store32(bytes, value->Domain);
        BML::Imc::Wire::Detail::Store32(bytes + 4, value->Slot);
        BML::Imc::Wire::Detail::Store32(bytes + 8, value->Generation);
        size = 12;
        break;
    }
    case Parameter::Form::Vec2: {
        const auto *value = std::get_if<std::array<float, 2>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 8;
        break;
    }
    case Parameter::Form::Vec3:
    case Parameter::Form::Euler: {
        const auto *value = std::get_if<std::array<float, 3>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 12;
        break;
    }
    case Parameter::Form::Quaternion:
    case Parameter::Form::Rect:
    case Parameter::Form::Color: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 16;
        break;
    }
    case Parameter::Form::Box: {
        const auto *value = std::get_if<std::array<float, 6>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 24;
        break;
    }
    case Parameter::Form::Mat4: {
        const auto *value = std::get_if<std::array<float, 16>>(&source.Data);
        if (!value)
            return false;
        for (std::size_t index = 0; index < value->size(); ++index)
            storeFloat(index, (*value)[index]);
        size = 64;
        break;
    }
    case Parameter::Form::Unsupported:
        return false;
    }
    record.ValueSize = static_cast<std::uint32_t>(size);
    return payload.Value(bytes, size, record.ValueOffset);
}

// A UTF-8 value points into storage, which must outlive the callback.
bool WriteWatchValue(const GraphValue &source,
                     BML_BehaviorWatchValue &out, std::string &storage) {
    out = {};
    out.StructSize = sizeof(out);
    out.State = PublicValueState(source.State);
    out.Relation = PublicRelation(source.Relation);
    out.Value.StructSize = sizeof(out.Value);
    out.Value.Type = Guid(source.Type);
    if (source.State != ValueState::Available)
        return true;
    out.Value.Kind = PublicValueKind(source.Form);
    switch (source.Form) {
    case Parameter::Form::Bool: {
        const bool *value = std::get_if<bool>(&source.Data);
        if (value) out.Value.Data.Bool = *value ? 1u : 0u;
        return value != nullptr;
    }
    case Parameter::Form::Int32: {
        const auto *value = std::get_if<std::int32_t>(&source.Data);
        if (value) out.Value.Data.Int32 = *value;
        return value != nullptr;
    }
    case Parameter::Form::Float32: {
        const float *value = std::get_if<float>(&source.Data);
        if (value) out.Value.Data.Float32 = *value;
        return value != nullptr;
    }
    case Parameter::Form::Utf8: {
        const auto *native = std::get_if<std::string>(&source.Data);
        if (!native)
            return false;
        const std::string_view value = Utf8Text(*native, storage);
        out.Value.Data.Utf8 = {value.data(),
            static_cast<std::uint32_t>(value.size())};
        return value.size() <= UINT32_MAX;
    }
    case Parameter::Form::Object: {
        const auto *value = std::get_if<BML::Behavior::Internal::ObjectRef>(&source.Data);
        if (value) out.Value.Data.Object = {
            value->Domain, value->Slot, value->Generation};
        return value != nullptr;
    }
    case Parameter::Form::Vec2: {
        const auto *value = std::get_if<std::array<float, 2>>(&source.Data);
        if (value) out.Value.Data.Vec2 = {(*value)[0], (*value)[1]};
        return value != nullptr;
    }
    case Parameter::Form::Vec3: {
        const auto *value = std::get_if<std::array<float, 3>>(&source.Data);
        if (value) out.Value.Data.Vec3 = {(*value)[0], (*value)[1], (*value)[2]};
        return value != nullptr;
    }
    case Parameter::Form::Euler: {
        const auto *value = std::get_if<std::array<float, 3>>(&source.Data);
        if (value) out.Value.Data.Euler = {(*value)[0], (*value)[1], (*value)[2]};
        return value != nullptr;
    }
    case Parameter::Form::Quaternion: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (value) out.Value.Data.Quaternion = {
            (*value)[0], (*value)[1], (*value)[2], (*value)[3]};
        return value != nullptr;
    }
    case Parameter::Form::Rect: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (value) out.Value.Data.Rect = {
            (*value)[0], (*value)[1], (*value)[2], (*value)[3]};
        return value != nullptr;
    }
    case Parameter::Form::Color: {
        const auto *value = std::get_if<std::array<float, 4>>(&source.Data);
        if (value) out.Value.Data.Color = {
            (*value)[0], (*value)[1], (*value)[2], (*value)[3]};
        return value != nullptr;
    }
    case Parameter::Form::Box: {
        const auto *value = std::get_if<std::array<float, 6>>(&source.Data);
        if (value) out.Value.Data.Box = {
            {(*value)[0], (*value)[1], (*value)[2]},
            {(*value)[3], (*value)[4], (*value)[5]}};
        return value != nullptr;
    }
    case Parameter::Form::Mat4: {
        const auto *value = std::get_if<std::array<float, 16>>(&source.Data);
        if (value)
            std::memcpy(&out.Value.Data.Mat4, value->data(), sizeof(BML_Mat4));
        return value != nullptr;
    }
    case Parameter::Form::Unsupported:
        return false;
    }
    return false;
}

} // namespace

int WritePrototypes(const std::vector<PrototypeInfo> &found,
                    BML_BehaviorPrototypeInfo *prototypes,
                    std::uint32_t prototypeCapacity,
                    std::uint32_t prototypeStride, void *payload,
                    std::uint32_t payloadCapacity,
                    std::uint32_t *outPrototypeCount,
                    std::uint32_t *outPayloadSize) {
    BehaviorPayload wire;
    std::vector<BML_BehaviorPrototypeInfo> records(found.size());
    for (std::size_t index = 0; index < found.size(); ++index) {
        if (!AddPrototype(found[index], wire, records[index]))
            return BML_ERROR_OUT_OF_MEMORY;
    }
    if (records.size() > UINT32_MAX || wire.Bytes().size() > UINT32_MAX)
        return BML_ERROR_OUT_OF_MEMORY;
    if (!FitsStrided(records.size(), prototypeStride,
                     sizeof(BML_BehaviorPrototypeInfo)))
        return BML_ERROR_OUT_OF_MEMORY;
    *outPrototypeCount = static_cast<std::uint32_t>(records.size());
    *outPayloadSize = static_cast<std::uint32_t>(wire.Bytes().size());
    if (prototypeCapacity < records.size() ||
        payloadCapacity < wire.Bytes().size())
        return BML_ERROR_BUFFER_TOO_SMALL;

    auto *recordBytes = reinterpret_cast<std::uint8_t *>(prototypes);
    for (std::size_t index = 0; index < records.size(); ++index)
        std::memcpy(recordBytes + index * prototypeStride,
                    &records[index], sizeof(records[index]));
    if (!wire.Bytes().empty())
        std::memcpy(payload, wire.Bytes().data(), wire.Bytes().size());
    return BML_OK;
}

int WriteLayoutResult(const Layout &source, BML_BehaviorLayout *layout,
                      void *payload, std::uint32_t payloadCapacity,
                      std::uint32_t *outPayloadSize) {
    BehaviorPayload wire;
    BML_BehaviorLayout record{};
    if (!AddLayout(source, wire, record) || wire.Bytes().size() > UINT32_MAX)
        return BML_ERROR_OUT_OF_MEMORY;
    *outPayloadSize = static_cast<std::uint32_t>(wire.Bytes().size());
    if (payloadCapacity < wire.Bytes().size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *layout = record;
    if (!wire.Bytes().empty())
        std::memcpy(payload, wire.Bytes().data(), wire.Bytes().size());
    return BML_OK;
}

void WriteWatchInfo(BML_BehaviorWatchInfo *out,
                    const BML::Behavior::Internal::WatchInfo &info) noexcept {
    *out = {};
    out->StructSize = sizeof(*out);
    out->State = PublicWatchState(info.State);
    out->Diagnostic.StructSize = sizeof(out->Diagnostic);
    WriteStatus(&out->Diagnostic, info.Diagnostic);
}

int WriteGraphResult(const GraphModel &source, BML_BehaviorGraph *graph,
                     void *payload, std::uint32_t payloadCapacity,
                     std::uint32_t *outPayloadSize) {
    BehaviorPayload bytes;
    BML_BehaviorGraph wire{};
    if (!AddGraph(source, bytes, wire) || bytes.Bytes().size() > UINT32_MAX)
        return BML_ERROR_OUT_OF_MEMORY;
    *outPayloadSize = static_cast<std::uint32_t>(bytes.Bytes().size());
    if (payloadCapacity < bytes.Bytes().size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *graph = wire;
    if (!bytes.Bytes().empty())
        std::memcpy(payload, bytes.Bytes().data(), bytes.Bytes().size());
    return BML_OK;
}

int WriteGraphValueResult(const GraphValue &source,
                          BML_BehaviorGraphValue *value,
                          void *payload, std::uint32_t payloadCapacity,
                          std::uint32_t *outPayloadSize) {
    BehaviorPayload bytes;
    BML_BehaviorGraphValue wire{};
    if (!AddGraphValue(source, bytes, wire) ||
        bytes.Bytes().size() > UINT32_MAX)
        return BML_ERROR_OUT_OF_MEMORY;
    *outPayloadSize = static_cast<std::uint32_t>(bytes.Bytes().size());
    if (payloadCapacity < bytes.Bytes().size())
        return BML_ERROR_BUFFER_TOO_SMALL;
    *value = wire;
    if (!bytes.Bytes().empty())
        std::memcpy(payload, bytes.Bytes().data(), bytes.Bytes().size());
    return BML_OK;
}

BML::Behavior::Internal::WatchBinding::Function WatchThunk(
    ModContext &context, const BML_BehaviorWatchFunction &function) {
    return [owner = &context, function](const WatchEvent &event) -> Status {
        const auto failed = [](const char *message) {
            return Status{Error::CallbackFailed, CK_OK, CKBR_BEHAVIORERROR,
                          message};
        };
        BML_BehaviorWatchEvent wire{};
        wire.StructSize = sizeof(wire);
        wire.Kind = PublicWatchKind(event.Kind);
        wire.Sequence = event.Sequence;
        wire.Frame = event.Frame;
        wire.Before = event.Before;
        wire.After = event.After;
        std::string previous;
        std::string current;
        if (!WriteWatchValue(event.PreviousValue, wire.PreviousValue, previous) ||
            !WriteWatchValue(event.CurrentValue, wire.CurrentValue, current))
            return failed("A Behavior Watch value could not be represented.");
        auto invocation = owner->LockModInvocation();
        int result = BML_BEHAVIOR_WATCH_ERROR;
        try {
            result = function.Invoke(function.State, &wire);
        } catch (...) {
            return failed("A Behavior Watch callback threw an exception.");
        }
        if (result != BML_BEHAVIOR_WATCH_OK)
            return failed("A Behavior Watch callback reported an error.");
        return {};
    };
}

} // namespace BML::Api::Behavior
