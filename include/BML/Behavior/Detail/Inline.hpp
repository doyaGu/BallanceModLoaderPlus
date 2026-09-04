#ifndef BML_BEHAVIOR_DETAIL_INLINE_HPP
#define BML_BEHAVIOR_DETAIL_INLINE_HPP

#include "BML/Behavior/Session.hpp"

#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace BML::Behavior {

namespace Detail {

inline Status BlockError(std::uint32_t error, std::uint32_t phase,
                         Prototype prototype, CKGUID type,
                         std::string message) {
    Status status;
    status.Error = static_cast<Behavior::Error>(error);
    status.Phase = static_cast<Behavior::Phase>(phase);
    status.Prototype = prototype.Id;
    status.Type = type;
    status.Message = std::move(message);
    return status;
}

inline Result<std::vector<PrototypeInfo>> FindPrototypes(
    const std::shared_ptr<SessionState> &session,
    const PrototypeQuery &query) {
    if (!session || !session->Api || !session->Handle)
        return Result<std::vector<PrototypeInfo>>::Failure(
            BML_ERROR_INVALID_HANDLE);
    if (!BML_IFACE_HAS(session->Api, BML_BehaviorInterface,
                       FindPrototypes))
        return Result<std::vector<PrototypeInfo>>::Failure(
            BML_ERROR_VERSION_MISMATCH);

    try {
        if (query.RequiredManagers.size() >
            (std::numeric_limits<std::uint32_t>::max)())
            return Result<std::vector<PrototypeInfo>>::Failure(
                BML_ERROR_INVALID_PARAMETER);
        std::vector<BML_BehaviorGuid> managers;
        managers.reserve(query.RequiredManagers.size());
        for (CKGUID manager : query.RequiredManagers)
            managers.push_back(WireGuid(manager));

        BML_BehaviorPrototypeQuery wireQuery{};
        wireQuery.StructSize = sizeof(wireQuery);
        if (query.Id) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_PROTOTYPE;
            wireQuery.Prototype = WireGuid(*query.Id);
        }
        if (query.Name) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_NAME;
            wireQuery.Name = Text(*query.Name);
        }
        if (query.Category) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_CATEGORY;
            wireQuery.Category = Text(*query.Category);
        }
        if (query.Provider) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_PROVIDER;
            wireQuery.Provider = Text(*query.Provider);
        }
        if (query.ProviderId) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_PROVIDER_GUID;
            wireQuery.ProviderGuid = WireGuid(*query.ProviderId);
        }
        if (query.CompatibleClass) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_COMPATIBLE_CLASS;
            wireQuery.CompatibleClass = *query.CompatibleClass;
        }
        if (!managers.empty()) {
            wireQuery.Match |= BML_BEHAVIOR_MATCH_REQUIRED_MANAGERS;
            wireQuery.RequiredManagers = managers.data();
            wireQuery.RequiredManagerCount =
                static_cast<std::uint32_t>(managers.size());
        }

        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t count = 0;
        std::uint32_t payloadSize = 0;
        int code = session->Api->FindPrototypes(
            session->Handle, &wireQuery, nullptr, 0,
            sizeof(BML_BehaviorPrototypeInfo), nullptr, 0, &count,
            &payloadSize, &status);
        if (code == BML_OK && count == 0 && payloadSize == 0)
            return Result<std::vector<PrototypeInfo>>::Success(
                {}, ReadStatus(status));
        if (code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<std::vector<PrototypeInfo>>::Failure(
                code, ReadStatus(status));

        std::vector<BML_BehaviorPrototypeInfo> records(count);
        std::vector<std::uint8_t> payload(payloadSize);
        status = EmptyStatus();
        std::uint32_t writtenCount = 0;
        std::uint32_t writtenBytes = 0;
        code = session->Api->FindPrototypes(
            session->Handle, &wireQuery, records.data(), count,
            sizeof(BML_BehaviorPrototypeInfo), payload.data(), payloadSize,
            &writtenCount, &writtenBytes, &status);
        if (code != BML_OK)
            return Result<std::vector<PrototypeInfo>>::Failure(
                code, ReadStatus(status));
        if (writtenCount != count || writtenBytes != payloadSize)
            return Result<std::vector<PrototypeInfo>>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));

        std::vector<PrototypeInfo> found;
        found.reserve(records.size());
        for (const BML_BehaviorPrototypeInfo &record : records) {
            if (record.StructSize < sizeof(record) ||
                record.Ref.StructSize < sizeof(record.Ref) ||
                !RecordsFit<BML_BehaviorManagerInfo>(
                    payload, record.ManagerOffset, record.ManagerCount))
                return Result<std::vector<PrototypeInfo>>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));

            PrototypeInfo info;
            info.Ref = Prototype(NativeGuid(record.Ref.Prototype),
                                 record.Ref.Generation);
            info.Provider = NativeGuid(record.Provider);
            info.Version = record.Version;
            info.CompatibleClass = record.CompatibleClass;
            if (!TextAt(payload, record.Name.Offset, record.Name.Length,
                        info.Name) ||
                !TextAt(payload, record.Category.Offset, record.Category.Length,
                        info.Category) ||
                !TextAt(payload, record.ProviderName.Offset,
                        record.ProviderName.Length, info.ProviderName) ||
                !TextAt(payload, record.Author.Offset, record.Author.Length,
                        info.Author) ||
                !TextAt(payload, record.Description.Offset,
                        record.Description.Length, info.Description))
                return Result<std::vector<PrototypeInfo>>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));

            info.Managers.reserve(record.ManagerCount);
            for (std::uint32_t index = 0; index < record.ManagerCount; ++index) {
                BML_BehaviorManagerInfo manager{};
                if (!RecordAt(payload, record.ManagerOffset, index, manager))
                    return Result<std::vector<PrototypeInfo>>::Failure(
                        BML_ERROR_MALFORMED_MESSAGE, ReadStatus(status));
                info.Managers.push_back(
                    {NativeGuid(manager.Guid), manager.Available != 0});
            }
            found.push_back(std::move(info));
        }
        return Result<std::vector<PrototypeInfo>>::Success(
            std::move(found), ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<std::vector<PrototypeInfo>>::Failure(
            BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<std::vector<PrototypeInfo>>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Behavior::Layout> ReadDeclared(
    const std::shared_ptr<SessionState> &session, Prototype prototype) {
    if (!session || !session->Api || !session->Handle)
        return Result<Behavior::Layout>::Failure(BML_ERROR_INVALID_HANDLE);
    if (!BML_IFACE_HAS(session->Api, BML_BehaviorInterface,
                       ReadDeclaredLayout))
        return Result<Behavior::Layout>::Failure(BML_ERROR_VERSION_MISMATCH);

    BML_BehaviorPrototypeRef requested{};
    requested.StructSize = sizeof(requested);
    requested.Prototype = WireGuid(prototype.Id);
    requested.Generation = prototype.Generation;
    BML_BehaviorLayout wire{};
    wire.StructSize = sizeof(wire);
    BML_BehaviorStatus status = EmptyStatus();
    std::uint32_t payloadSize = 0;
    int code = session->Api->ReadDeclaredLayout(
        session->Handle, &requested, &wire, nullptr, 0, &payloadSize, &status);
    if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
        return Result<Behavior::Layout>::Failure(code, ReadStatus(status));
    if (code == BML_OK && payloadSize != 0) {
        return Result<Behavior::Layout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, prototype, CKGUID(0, 0),
                       "The declared Layout reported an incomplete payload."));
    }

    std::vector<std::uint8_t> payload(payloadSize);
    if (payloadSize) {
        wire = {};
        wire.StructSize = sizeof(wire);
        status = EmptyStatus();
        std::uint32_t written = 0;
        code = session->Api->ReadDeclaredLayout(
            session->Handle, &requested, &wire, payload.data(), payloadSize,
            &written, &status);
        if (code != BML_OK)
            return Result<Behavior::Layout>::Failure(code, ReadStatus(status));
        if (written != payload.size()) {
            return Result<Behavior::Layout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE,
                BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                           BML_BEHAVIOR_PHASE_LAYOUT, prototype, CKGUID(0, 0),
                           "The declared Layout payload is malformed."));
        }
    }

    const bool samePrototype =
        wire.Prototype.Prototype.Data1 == prototype.Id.d1 &&
        wire.Prototype.Prototype.Data2 == prototype.Id.d2;
    if (wire.StructSize < sizeof(wire) ||
        wire.Prototype.StructSize < sizeof(wire.Prototype) ||
        wire.Origin != BML_BEHAVIOR_LAYOUT_DECLARED || !samePrototype ||
        !wire.Prototype.Generation ||
        (prototype.Generation &&
         wire.Prototype.Generation != prototype.Generation)) {
        return Result<Behavior::Layout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, prototype, CKGUID(0, 0),
                       "The declared Layout does not identify the requested Prototype."));
    }

    const Prototype resolved(NativeGuid(wire.Prototype.Prototype),
                             wire.Prototype.Generation);
    Behavior::Layout layout;
    if (!ReadLayout(wire, payload, layout)) {
        return Result<Behavior::Layout>::Failure(
            BML_ERROR_MALFORMED_MESSAGE,
            BlockError(BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE,
                       BML_BEHAVIOR_PHASE_LAYOUT, resolved, CKGUID(0, 0),
                       "The declared Layout payload is malformed."));
    }
    return Result<Behavior::Layout>::Success(std::move(layout),
                                              ReadStatus(status));
}

inline Status CheckDefinition(const BlockDefinition &definition) {
    constexpr std::size_t maximum =
        (std::numeric_limits<std::uint32_t>::max)();
    if (!definition.PrototypeRef.Id.IsValid()) {
        return BlockError(BML_BEHAVIOR_ERROR_PROTOTYPE_NOT_FOUND,
                          BML_BEHAVIOR_PHASE_PROTOTYPE,
                          definition.PrototypeRef, CKGUID(0, 0),
                          "A Block requires a Prototype GUID.");
    }
    if ((definition.TargetKind == BML_BEHAVIOR_TARGET_OBJECT ||
         definition.TargetKind == BML_BEHAVIOR_TARGET_NULL) &&
        !definition.TargetType.IsValid()) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          definition.PrototypeRef, CKGUID(0, 0),
                          "An explicit Target requires a parameter type.");
    }
    if (definition.TargetKind == BML_BEHAVIOR_TARGET_OBJECT &&
        !definition.TargetObject.Domain) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          definition.PrototypeRef, definition.TargetType,
                          "An explicit Target requires a live object reference.");
    }
    if (definition.TargetKind != BML_BEHAVIOR_TARGET_OWNER &&
        definition.TargetKind != BML_BEHAVIOR_TARGET_OBJECT &&
        definition.TargetKind != BML_BEHAVIOR_TARGET_NULL) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          definition.PrototypeRef, definition.TargetType,
                          "The Block Target kind is unknown.");
    }
    const bool bounded =
        definition.Frames.Kind == BML_BEHAVIOR_FRAMES_SIGNALS ||
        definition.Frames.Kind == BML_BEHAVIOR_FRAMES_EACH_FRAME;
    if ((bounded && !definition.Frames.Limit) ||
        (!bounded && definition.Frames.Limit) ||
        (!bounded && definition.Frames.Kind != BML_BEHAVIOR_FRAMES_LATEST &&
         definition.Frames.Kind != BML_BEHAVIOR_FRAMES_NONE)) {
        return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                          BML_BEHAVIOR_PHASE_NONE,
                          definition.PrototypeRef, CKGUID(0, 0),
                          "The Block Frame policy is invalid.");
    }
    if (definition.Settings.size() > maximum ||
        definition.Pins.size() > maximum ||
        definition.Locals.size() > maximum) {
        return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                          BML_BEHAVIOR_PHASE_NONE,
                          definition.PrototypeRef, CKGUID(0, 0),
                          "The Block contains too many bindings.");
    }
    for (const auto &stage : definition.Settings) {
        if (stage.size() > maximum) {
            return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              definition.PrototypeRef, CKGUID(0, 0),
                              "A Block contains too many Settings in one stage.");
        }
    }
    return {};
}

inline std::string SelectorLabel(const BML_BehaviorSelector &selector) {
    if (selector.Kind == BML_BEHAVIOR_SELECTOR_ONLY)
        return "<only>";
    if (selector.Kind == BML_BEHAVIOR_SELECTOR_INDEX)
        return "#" + std::to_string(selector.Index);
    return "'" + std::string(selector.Name.Data ? selector.Name.Data : "",
                              selector.Name.Length) + "'";
}

inline Status CheckSetting(const Behavior::Layout &layout,
                           const SlotValue &binding) {
    const BML_BehaviorSelector selector = Wire::From(binding.Slot);
    std::vector<const Behavior::Slot *> matches;
    for (const Behavior::Slot &slot : layout.Slots) {
        if (slot.Kind != SlotKind::Setting)
            continue;
        if (selector.Kind == BML_BEHAVIOR_SELECTOR_INDEX) {
            if (slot.Index == selector.Index)
                matches.push_back(&slot);
        } else if (selector.Kind == BML_BEHAVIOR_SELECTOR_ONLY) {
            matches.push_back(&slot);
        } else if (selector.Kind == BML_BEHAVIOR_SELECTOR_NAME ||
                   selector.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME) {
            const std::string_view name(
                selector.Name.Data ? selector.Name.Data : "",
                selector.Name.Length);
            if (slot.Name == name)
                matches.push_back(&slot);
        } else {
            return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, CKGUID(0, 0),
                              "A Setting selector kind is unknown.");
        }
    }

    if (matches.empty()) {
        return BlockError(BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
                          BML_BEHAVIOR_PHASE_SETTINGS,
                          layout.PrototypeRef, CKGUID(0, 0),
                          "Setting " + SelectorLabel(selector) +
                              " is absent from the declared Layout.");
    }
    const Behavior::Slot *slot = nullptr;
    if (selector.Kind == BML_BEHAVIOR_SELECTOR_ONLY ||
        selector.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME) {
        if (matches.size() != 1) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, CKGUID(0, 0),
                              "Setting " + SelectorLabel(selector) +
                                  " is ambiguous in the declared Layout.");
        }
        slot = matches.front();
    } else if (selector.Kind == BML_BEHAVIOR_SELECTOR_NAME) {
        if (selector.Occurrence < 0) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, CKGUID(0, 0),
                              "The requested Setting occurrence is absent from the declared Layout.");
        }
        for (const Behavior::Slot *candidate : matches) {
            if (candidate->Occurrence != selector.Occurrence)
                continue;
            if (slot) {
                return BlockError(BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
                                  BML_BEHAVIOR_PHASE_SETTINGS,
                                  layout.PrototypeRef, CKGUID(0, 0),
                                  "The requested Setting occurrence is ambiguous in the declared Layout.");
            }
            slot = candidate;
        }
        if (!slot) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, CKGUID(0, 0),
                              "The requested Setting occurrence is absent from the declared Layout.");
        }
    } else {
        if (matches.size() != 1) {
            return BlockError(BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              layout.PrototypeRef, CKGUID(0, 0),
                              "The Setting index is ambiguous in the declared Layout.");
        }
        slot = matches.front();
    }

    if (!slot->Value) {
        return BlockError(BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED,
                          BML_BEHAVIOR_PHASE_SETTINGS,
                          layout.PrototypeRef, slot->Type,
                          "The Setting parameter type has no public value form.");
    }
    if (*slot->Value != binding.Data.Kind()) {
        return BlockError(BML_BEHAVIOR_ERROR_TYPE_MISMATCH,
                          BML_BEHAVIOR_PHASE_SETTINGS,
                          layout.PrototypeRef, slot->Type,
                          "The Setting value form does not match the declared Layout.");
    }
    return {};
}

inline CompiledBlock::CompiledBlock(BlockDefinition definition)
    : Definition(std::move(definition)) {
    while (Definition.Settings.size() > 1 &&
           Definition.Settings.back().empty())
        Definition.Settings.pop_back();
    if (Definition.Settings.size() == 1 && Definition.Settings.front().empty())
        Definition.Settings.clear();

    SettingBindings.reserve(Definition.Settings.size());
    SettingStages.reserve(Definition.Settings.size());
    for (const std::vector<SlotValue> &stage : Definition.Settings) {
        std::vector<BML_BehaviorBinding> bindings;
        bindings.reserve(stage.size());
        for (const SlotValue &binding : stage) {
            BML_BehaviorBinding wire{};
            wire.StructSize = sizeof(wire);
            wire.Slot = Wire::From(binding.Slot);
            wire.Value = Wire::From(binding.Data);
            bindings.push_back(wire);
        }
        SettingBindings.push_back(std::move(bindings));
    }
    for (const auto &bindings : SettingBindings) {
        BML_BehaviorSettingStage stage{};
        stage.StructSize = sizeof(stage);
        stage.Settings = bindings.empty() ? nullptr : bindings.data();
        stage.SettingCount = static_cast<std::uint32_t>(bindings.size());
        SettingStages.push_back(stage);
    }
    const auto add = [](const std::vector<SlotValue> &from,
                        std::vector<BML_BehaviorBinding> &to) {
        to.reserve(from.size());
        for (const SlotValue &binding : from) {
            BML_BehaviorBinding wire{};
            wire.StructSize = sizeof(wire);
            wire.Slot = Wire::From(binding.Slot);
            wire.Value = Wire::From(binding.Data);
            to.push_back(wire);
        }
    };
    add(Definition.Pins, Pins);
    add(Definition.Locals, Locals);

    Wire.StructSize = sizeof(Wire);
    Wire.Prototype = WireGuid(Definition.PrototypeRef.Id);
    Wire.Target.StructSize = sizeof(Wire.Target);
    Wire.Target.Kind = Definition.TargetKind;
    Wire.Target.Type = WireGuid(Definition.TargetType);
    Wire.Target.Object = Definition.TargetObject;
    Wire.SettingStages = SettingStages.empty() ? nullptr : SettingStages.data();
    Wire.SettingStageCount = static_cast<std::uint32_t>(SettingStages.size());
    Wire.Pins = Pins.empty() ? nullptr : Pins.data();
    Wire.PinCount = static_cast<std::uint32_t>(Pins.size());
    Wire.Locals = Locals.empty() ? nullptr : Locals.data();
    Wire.LocalCount = static_cast<std::uint32_t>(Locals.size());
    Wire.Frames.StructSize = sizeof(Wire.Frames);
    Wire.Frames.Kind = Definition.Frames.Kind;
    Wire.Frames.Limit = Definition.Frames.Limit;
    Wire.PrototypeGeneration = Definition.PrototypeRef.Generation;
}

inline Result<std::shared_ptr<const CompiledBlock>> Compiler::operator()(
    const std::shared_ptr<SessionState> &session,
    BlockDefinition definition) const {
    if (!session || !session->Api || !session->Handle) {
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            BML_ERROR_INVALID_HANDLE);
    }
    Status checked = CheckDefinition(definition);
    if (!checked) {
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            BML_ERROR_INVALID_PARAMETER, std::move(checked));
    }
    auto declared = ReadDeclared(session, definition.PrototypeRef);
    if (!declared) {
        if (declared.Code() == BML_ERROR_UNAVAILABLE) {
            std::shared_ptr<const CompiledBlock> block =
                std::make_shared<CompiledBlock>(std::move(definition));
            return Result<std::shared_ptr<const CompiledBlock>>::Success(
                std::move(block));
        }
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            declared.Code(), declared.GetStatus());
    }
    if (!definition.Settings.empty()) {
        for (const SlotValue &setting : definition.Settings.front()) {
            checked = CheckSetting(declared.Value(), setting);
            if (!checked) {
                return Result<std::shared_ptr<const CompiledBlock>>::Failure(
                    BML_ERROR_FAIL, std::move(checked));
            }
        }
    }
    definition.PrototypeRef = declared.Value().PrototypeRef;
    std::shared_ptr<const CompiledBlock> block =
        std::make_shared<CompiledBlock>(std::move(definition));
    return Result<std::shared_ptr<const CompiledBlock>>::Success(
        std::move(block), declared.GetStatus());
}

} // namespace Detail

inline Result<std::vector<PrototypeInfo>> Session::Prototypes(
    const PrototypeQuery &query) const {
    return Detail::FindPrototypes(m_State, query);
}

inline Result<Behavior::Layout> Session::Layout(Prototype prototype) const {
    return Detail::ReadDeclared(m_State, prototype);
}

namespace Detail {

inline bool ReadObserved(const BML_BehaviorGraphValue &source,
                         const std::vector<std::uint8_t> &payload,
                         ObservedValue &out) {
    if (source.StructSize < sizeof(source))
        return false;
    out = {};
    out.State = static_cast<ObservationState>(source.State);
    out.Source = static_cast<Relation>(source.Relation);
    out.Type = NativeGuid(source.Type);
    if (source.State != BML_BEHAVIOR_VALUE_AVAILABLE)
        return source.Kind == 0 && source.ValueSize == 0;
    if (source.Kind < BML_BEHAVIOR_VALUE_BOOL ||
        source.Kind > BML_BEHAVIOR_VALUE_OBJECT)
        return false;
    out.Kind = static_cast<ValueKind>(source.Kind);
    BML_BehaviorPoutRecord value{};
    value.StructSize = sizeof(value);
    value.Kind = source.Kind;
    value.ValueOffset = source.ValueOffset;
    value.ValueSize = source.ValueSize;
    return ReadPoutData(value, payload, out.Data);
}

inline bool ReadWatchValue(const BML_BehaviorWatchValue &source,
                           ObservedValue &out) {
    if (source.StructSize < sizeof(source) ||
        source.Value.StructSize < sizeof(source.Value))
        return false;
    out = {};
    out.State = static_cast<ObservationState>(source.State);
    out.Source = static_cast<Relation>(source.Relation);
    out.Type = NativeGuid(source.Value.Type);
    if (source.State != BML_BEHAVIOR_VALUE_AVAILABLE)
        return source.Value.Kind == 0;
    if (source.Value.Kind < BML_BEHAVIOR_VALUE_BOOL ||
        source.Value.Kind > BML_BEHAVIOR_VALUE_OBJECT)
        return false;
    out.Kind = static_cast<ValueKind>(source.Value.Kind);
    switch (source.Value.Kind) {
    case BML_BEHAVIOR_VALUE_BOOL:
        out.Data = source.Value.Data.Bool != 0;
        break;
    case BML_BEHAVIOR_VALUE_INT32:
        out.Data = source.Value.Data.Int32;
        break;
    case BML_BEHAVIOR_VALUE_FLOAT32:
        out.Data = source.Value.Data.Float32;
        break;
    case BML_BEHAVIOR_VALUE_UTF8:
        if (!source.Value.Data.Utf8.Data && source.Value.Data.Utf8.Length)
            return false;
        out.Data = std::string(
            source.Value.Data.Utf8.Data ? source.Value.Data.Utf8.Data : "",
            source.Value.Data.Utf8.Length);
        break;
    case BML_BEHAVIOR_VALUE_VEC2: out.Data = source.Value.Data.Vec2; break;
    case BML_BEHAVIOR_VALUE_VEC3: out.Data = source.Value.Data.Vec3; break;
    case BML_BEHAVIOR_VALUE_QUATERNION:
        out.Data = source.Value.Data.Quaternion; break;
    case BML_BEHAVIOR_VALUE_EULER: out.Data = source.Value.Data.Euler; break;
    case BML_BEHAVIOR_VALUE_RECT: out.Data = source.Value.Data.Rect; break;
    case BML_BEHAVIOR_VALUE_COLOR: out.Data = source.Value.Data.Color; break;
    case BML_BEHAVIOR_VALUE_BOX: out.Data = source.Value.Data.Box; break;
    case BML_BEHAVIOR_VALUE_MAT4: out.Data = source.Value.Data.Mat4; break;
    case BML_BEHAVIOR_VALUE_OBJECT: out.Data = source.Value.Data.Object; break;
    default: return false;
    }
    return true;
}

inline bool ReadChange(const BML_BehaviorWatchEvent *source, Change &out) {
    if (!source || source->StructSize < sizeof(*source))
        return false;
    out = {};
    out.Kind = static_cast<ChangeKind>(source->Kind);
    out.Sequence = source->Sequence;
    out.GameFrame = source->Frame;
    out.Before = source->Before;
    out.After = source->After;
    return ReadWatchValue(source->PreviousValue, out.PreviousValue) &&
           ReadWatchValue(source->CurrentValue, out.CurrentValue);
}

template <class Function>
struct WatchFunction {
    using Callable = std::decay_t<Function>;

    explicit WatchFunction(Function &&function)
        : Value(std::forward<Function>(function)) {}

    static void BML_BEHAVIOR_CALL Retain(void *state) {
        ++static_cast<WatchFunction *>(state)->References;
    }
    static void BML_BEHAVIOR_CALL Release(void *state) {
        auto *self = static_cast<WatchFunction *>(state);
        if (self->References.fetch_sub(1) == 1)
            delete self;
    }
    static int BML_BEHAVIOR_CALL Invoke(
        void *state, const BML_BehaviorWatchEvent *source) noexcept {
        try {
            Change change;
            if (!state || !ReadChange(source, change))
                return BML_BEHAVIOR_WATCH_ERROR;
            std::invoke(static_cast<WatchFunction *>(state)->Value, change);
            return BML_BEHAVIOR_WATCH_OK;
        } catch (...) {
            return BML_BEHAVIOR_WATCH_ERROR;
        }
    }

    std::atomic<std::uint32_t> References{1};
    Callable Value;
};

} // namespace Detail

inline Result<Graph> Graph::Read(
    std::shared_ptr<Detail::SessionState> session,
    BML_ObjectRef root, View view) {
    if (!session || !session->Api || !session->Handle || !root.Domain)
        return Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorGraph wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = session->Api->Inspect(
            session->Handle, root, static_cast<std::uint32_t>(view),
            &wire, nullptr, 0, &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Graph>::Failure(code, Detail::ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = Detail::EmptyStatus();
            std::uint32_t written = 0;
            code = session->Api->Inspect(
                session->Handle, root, static_cast<std::uint32_t>(view),
                &wire, payload.data(), payloadSize, &written, &status);
            if (code != BML_OK)
                return Result<Graph>::Failure(code, Detail::ReadStatus(status));
            if (written != payload.size())
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        }
        return Decode(std::move(session), view, wire, payload, status);
    } catch (const std::bad_alloc &) {
        return Result<Graph>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Graph>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Graph::ReadRun(
    std::shared_ptr<Detail::SessionState> session,
    BML_BehaviorRun run, View view) {
    if (!session || !session->Api || !session->Handle || !run ||
        !BML_IFACE_HAS(session->Api, BML_BehaviorInterface, InspectRun))
        return Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorGraph wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = session->Api->InspectRun(
            run, static_cast<std::uint32_t>(view), &wire, nullptr, 0,
            &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Graph>::Failure(code, Detail::ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = Detail::EmptyStatus();
            std::uint32_t written = 0;
            code = session->Api->InspectRun(
                run, static_cast<std::uint32_t>(view), &wire,
                payload.data(), payloadSize, &written, &status);
            if (code != BML_OK)
                return Result<Graph>::Failure(code, Detail::ReadStatus(status));
            if (written != payload.size())
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        }
        return Decode(std::move(session), view, wire, payload, status);
    } catch (const std::bad_alloc &) {
        return Result<Graph>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Graph>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Graph::Decode(
    std::shared_ptr<Detail::SessionState> session, View view,
    const BML_BehaviorGraph &wire,
    const std::vector<std::uint8_t> &payload,
    const BML_BehaviorStatus &status) {
    if (wire.StructSize < sizeof(wire) ||
        wire.View != static_cast<std::uint32_t>(view) ||
        !Detail::RecordsFit<BML_BehaviorGraphNode>(
            payload, wire.NodeOffset, wire.NodeCount) ||
        !Detail::RecordsFit<BML_BehaviorGraphLink>(
            payload, wire.LinkOffset, wire.LinkCount))
        return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);

    Graph graph;
    graph.m_Session = std::move(session);
    graph.m_Root = wire.Root;
    graph.m_View = view;
    graph.m_Generation = wire.Generation;
    graph.m_Fingerprint = wire.Fingerprint;
    graph.m_Nodes.reserve(wire.NodeCount);
    std::unordered_set<std::uint64_t> nodes;
    std::unordered_map<Detail::PortKey,
                       std::pair<std::size_t, std::size_t>,
                       Detail::PortKeyHash> ports;
    nodes.reserve(wire.NodeCount);
    for (std::uint32_t index = 0; index < wire.NodeCount; ++index) {
        BML_BehaviorGraphNode record{};
        if (!Detail::RecordAt(payload, wire.NodeOffset, index, record) ||
            !Detail::RecordsFit<BML_BehaviorGraphPort>(
                payload, record.PortOffset, record.PortCount))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        Node node;
        node.Id = record.Id;
        node.Object = record.Object;
        node.Parent = record.Parent;
        node.Prototype = Detail::NativeGuid(record.Prototype);
        node.Priority = record.Priority;
        node.Active = record.Active != 0;
        if (!node.Id || !nodes.emplace(node.Id).second ||
            !Detail::KnownFlag(record.Active))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        if (!Detail::TextAt(payload, record.Name.Offset,
                            record.Name.Length, node.Name))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        node.Ports.reserve(record.PortCount);
        for (std::uint32_t portIndex = 0;
             portIndex < record.PortCount; ++portIndex) {
            BML_BehaviorGraphPort portRecord{};
            if (!Detail::RecordAt(payload, record.PortOffset,
                                  portIndex, portRecord))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            Port port;
            port.Object = node.Object;
            port.Node = portRecord.Node;
            if (portRecord.Node != node.Id || portRecord.Index < 0 ||
                portRecord.Occurrence < 0 ||
                !Detail::KnownSlotKind(portRecord.Kind) ||
                !Detail::KnownFlag(portRecord.Active))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            port.Kind = static_cast<SlotKind>(portRecord.Kind);
            port.Slot = Selector::At(portRecord.Index);
            port.Index = portRecord.Index;
            port.Occurrence = portRecord.Occurrence;
            port.Active = portRecord.Active != 0;
            if (!Detail::TextAt(payload, portRecord.Name.Offset,
                                 portRecord.Name.Length, port.Name))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            if (!ports.emplace(
                    Detail::PortKey{
                        node.Id, portRecord.Kind, portRecord.Index},
                    std::make_pair(graph.m_Nodes.size(),
                                   node.Ports.size())).second)
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            node.Ports.push_back(std::move(port));
        }
        graph.m_Nodes.push_back(std::move(node));
    }
    const auto endpoint = [&](std::uint64_t nodeId, std::uint32_t kind,
                              std::int32_t index, Port &out) {
        const auto port = ports.find(
            Detail::PortKey{nodeId, kind, index});
        if (port == ports.end())
            return false;
        out = graph.m_Nodes[port->second.first].Ports[port->second.second];
        return true;
    };
    graph.m_Links.reserve(wire.LinkCount);
    std::unordered_set<std::uint64_t> links;
    links.reserve(wire.LinkCount);
    for (std::uint32_t index = 0; index < wire.LinkCount; ++index) {
        BML_BehaviorGraphLink record{};
        if (!Detail::RecordAt(payload, wire.LinkOffset, index, record))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        if (!record.Id || !links.emplace(record.Id).second ||
            record.SourceIndex < 0 || record.TargetIndex < 0 ||
            !Detail::KnownSlotKind(record.SourceKind) ||
            !Detail::KnownSlotKind(record.TargetKind) ||
            !Detail::KnownTruth(record.Pending))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        Link link;
        link.Id = record.Id;
        link.Object = record.Object;
        if (!endpoint(record.SourceNode, record.SourceKind,
                      record.SourceIndex, link.Source) ||
            !endpoint(record.TargetNode, record.TargetKind,
                      record.TargetIndex, link.Target))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        link.InitialDelay = record.InitialDelay;
        link.RemainingDelay = record.RemainingDelay;
        link.Pending = static_cast<TruthValue>(record.Pending);
        graph.m_Links.push_back(std::move(link));
    }
    return Result<Graph>::Success(std::move(graph),
                                  Detail::ReadStatus(status));
}

inline Result<Behavior::Layout> Detail::Run::Layout() const {
    if (!*this)
        return Result<Behavior::Layout>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorLayout wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = m_Session->Api->ReadLiveLayout(
            m_Handle, &wire, nullptr, 0, &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Behavior::Layout>::Failure(code, ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = EmptyStatus();
            std::uint32_t written = 0;
            code = m_Session->Api->ReadLiveLayout(
                m_Handle, &wire, payload.data(), payloadSize,
                &written, &status);
            if (code != BML_OK)
                return Result<Behavior::Layout>::Failure(
                    code, ReadStatus(status));
            if (written != payload.size())
                return Result<Behavior::Layout>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE);
        }
        Behavior::Layout layout;
        if (!ReadLayout(wire, payload, layout) ||
            layout.Origin != LayoutOrigin::Live)
            return Result<Behavior::Layout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE);
        return Result<Behavior::Layout>::Success(
            std::move(layout), ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Layout>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Behavior::Layout>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Detail::Run::Inspect(View view) const {
    if (!*this)
        return Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    return Graph::ReadRun(m_Session, m_Handle, view);
}

inline Result<std::uint64_t> Detail::Run::Set(
    const Behavior::Slot &slot, const Behavior::Value &value) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Set))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target = Wire::From(slot);
    const BML_BehaviorValue wire = Wire::From(value);
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = m_Session->Api->Set(
        m_Handle, &target, &wire, &generation, &status);
    return code == BML_OK
        ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
        : Result<std::uint64_t>::Failure(code, ReadStatus(status));
}

inline Result<std::uint64_t> Detail::Run::Set(
    SlotKind kind, const Selector &slot,
    const Behavior::Value &value) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Set))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target{};
    target.StructSize = sizeof(target);
    target.Kind = static_cast<std::uint32_t>(kind);
    target.Type = Detail::WireGuid(value.Type());
    target.Slot = Wire::From(slot);
    const BML_BehaviorValue wire = Wire::From(value);
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = m_Session->Api->Set(
        m_Handle, &target, &wire, &generation, &status);
    return code == BML_OK
        ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
        : Result<std::uint64_t>::Failure(code, ReadStatus(status));
}

inline Result<std::uint64_t> Detail::Run::Bind(
    const Behavior::Slot &slot, const Port &source,
    Relation relation) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Bind))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target = Wire::From(slot);
    BML_BehaviorValueRef value{};
    value.StructSize = sizeof(value);
    value.Kind = static_cast<std::uint32_t>(source.Kind);
    value.Node = source.Object;
    value.Slot = Wire::From(source.Slot);
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = m_Session->Api->Bind(
        m_Handle, &target, &value,
        static_cast<std::uint32_t>(relation), &generation, &status);
    return code == BML_OK
        ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
        : Result<std::uint64_t>::Failure(code, ReadStatus(status));
}

inline Result<std::uint64_t> Detail::Run::Settings(
    std::initializer_list<SlotValue> values) const {
    if (!*this || values.size() == 0 ||
        !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Configure))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        std::vector<BML_BehaviorBinding> bindings;
        bindings.reserve(values.size());
        for (const SlotValue &value : values) {
            BML_BehaviorBinding wire{};
            wire.StructSize = sizeof(wire);
            wire.Slot = Wire::From(value.Slot);
            wire.Value = Wire::From(value.Data);
            bindings.push_back(wire);
        }
        BML_BehaviorSettingStage stage{};
        stage.StructSize = sizeof(stage);
        stage.Settings = bindings.data();
        stage.SettingCount = static_cast<std::uint32_t>(bindings.size());
        BML_BehaviorStatus status = EmptyStatus();
        std::uint64_t generation = 0;
        const int code = m_Session->Api->Configure(
            m_Handle, &stage, 1, &generation, &status);
        return code == BML_OK
            ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
            : Result<std::uint64_t>::Failure(code, ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<std::uint64_t>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<std::uint64_t>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Graph::Logical() const {
    return Read(m_Session, m_Root, View::Logical);
}

inline Result<Graph> Graph::Live() const {
    return Read(m_Session, m_Root, View::Live);
}

inline Result<ObservedValue> Graph::Read(const Port &port) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle ||
        !port.Object.Domain)
        return Result<ObservedValue>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorSelector selector = Detail::Wire::From(port.Slot);
        BML_BehaviorGraphValue wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = m_Session->Api->ReadValue(
            m_Session->Handle, port.Object,
            static_cast<std::uint32_t>(port.Kind), &selector,
            BML_BEHAVIOR_READ_NON_FORCING, &wire, nullptr, 0,
            &payloadSize, &status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<ObservedValue>::Failure(
                code, Detail::ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = Detail::EmptyStatus();
            std::uint32_t written = 0;
            code = m_Session->Api->ReadValue(
                m_Session->Handle, port.Object,
                static_cast<std::uint32_t>(port.Kind), &selector,
                BML_BEHAVIOR_READ_NON_FORCING, &wire, payload.data(),
                payloadSize, &written, &status);
            if (code != BML_OK)
                return Result<ObservedValue>::Failure(
                    code, Detail::ReadStatus(status));
            if (written != payload.size())
                return Result<ObservedValue>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE);
        }
        ObservedValue observed;
        if (!Detail::ReadObserved(wire, payload, observed))
            return Result<ObservedValue>::Failure(
                BML_ERROR_MALFORMED_MESSAGE);
        return Result<ObservedValue>::Success(
            std::move(observed), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<ObservedValue>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<ObservedValue>::Failure(BML_ERROR_FAIL);
    }
}

template <class Function>
Result<Behavior::Watch> Graph::OpenWatch(
    BML_BehaviorWatchSpec spec, Function &&callback) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<Behavior::Watch>::Failure(BML_ERROR_INVALID_HANDLE);
    using Holder = Detail::WatchFunction<Function>;
    Holder *holder = nullptr;
    try {
        holder = new Holder(std::forward<Function>(callback));
        BML_BehaviorWatchFunction function{};
        function.StructSize = sizeof(function);
        function.State = holder;
        function.Retain = &Holder::Retain;
        function.Release = &Holder::Release;
        function.Invoke = &Holder::Invoke;
        BML_BehaviorWatch handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->Watch(
            m_Session->Handle, &spec, &function, &handle, &status);
        Holder::Release(holder);
        holder = nullptr;
        if (code != BML_OK || !handle) {
            if (handle)
                (void) m_Session->Api->CloseWatch(handle);
            return Result<Behavior::Watch>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<Behavior::Watch>::Success(
            Behavior::Watch(m_Session, handle),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        if (holder)
            Holder::Release(holder);
        return Result<Behavior::Watch>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        if (holder)
            Holder::Release(holder);
        return Result<Behavior::Watch>::Failure(BML_ERROR_FAIL);
    }
}

template <class Function>
Result<Behavior::Watch> Graph::Watch(
    GraphChanged, Function &&callback) const {
    BML_BehaviorWatchSpec spec{};
    spec.StructSize = sizeof(spec);
    spec.Kind = BML_BEHAVIOR_WATCH_GRAPH;
    spec.View = static_cast<std::uint32_t>(m_View);
    spec.Root = m_Root;
    spec.Slot.StructSize = sizeof(spec.Slot);
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

template <class Function>
Result<Behavior::Watch> Graph::Watch(
    LayoutChanged change, Function &&callback) const {
    BML_BehaviorWatchSpec spec{};
    spec.StructSize = sizeof(spec);
    spec.Kind = BML_BEHAVIOR_WATCH_LAYOUT;
    spec.View = static_cast<std::uint32_t>(m_View);
    spec.Node = change.Node;
    spec.Slot.StructSize = sizeof(spec.Slot);
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

template <class Function>
Result<Behavior::Watch> Graph::Watch(
    Sampled change, Function &&callback) const {
    BML_BehaviorWatchSpec spec{};
    spec.StructSize = sizeof(spec);
    spec.Kind = BML_BEHAVIOR_WATCH_SAMPLED_VALUE;
    spec.View = static_cast<std::uint32_t>(m_View);
    spec.Node = change.Value.Object;
    spec.SlotKind = static_cast<std::uint32_t>(change.Value.Kind);
    spec.Slot = Detail::Wire::From(change.Value.Slot);
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

inline Detail::BlockDefinition &Block::Change() {
    if (!m_State)
        throw std::logic_error("Cannot configure an empty Behavior Block.");
    if (m_State.use_count() != 1)
        m_State = std::make_shared<Detail::BlockState>(m_State->Definition);
    else
        m_State->Compiled.reset();
    return m_State->Definition;
}

inline Result<std::shared_ptr<const Detail::CompiledBlock>> Block::Compile() const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle || !m_State)
        return Result<std::shared_ptr<const Detail::CompiledBlock>>::Failure(
            BML_ERROR_INVALID_HANDLE);
    if (m_State->Compiled) {
        return Result<std::shared_ptr<const Detail::CompiledBlock>>::Success(
            m_State->Compiled);
    }
    try {
        auto compiled = Detail::Compiler{}(m_Session, m_State->Definition);
        if (!compiled)
            return compiled;
        m_State->Definition = compiled.Value()->Definition;
        m_State->Compiled = compiled.Value();
        return compiled;
    } catch (const std::bad_alloc &) {
        return Result<std::shared_ptr<const Detail::CompiledBlock>>::Failure(
            BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<std::shared_ptr<const Detail::CompiledBlock>>::Failure(
            BML_ERROR_FAIL);
    }
}

inline Result<void> Block::Validate() const {
    auto compiled = Compile();
    if (!compiled)
        return Result<void>::Failure(compiled.Code(), compiled.GetStatus());
    return Result<void>::Success(compiled.GetStatus());
}

template <class Handle, class Function>
Result<Handle> Block::Open(Function function, ObjectRef owner,
                           const Selector *input,
                           std::optional<FramePolicy> frames) const {
    auto compiled = Compile();
    if (!compiled)
        return Result<Handle>::Failure(compiled.Code(), compiled.GetStatus());
    try {
        BML_BehaviorSelector selector{};
        const BML_BehaviorSelector *selectorPointer = nullptr;
        if (input) {
            selector = Detail::Wire::From(*input);
            selectorPointer = &selector;
        }
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
        BML_BehaviorStatus status = Detail::EmptyStatus();
        BML_BehaviorBlock wire = compiled.Value()->Wire;
        if (frames) {
            wire.Frames.Kind = frames->Kind;
            wire.Frames.Limit = frames->Limit;
        }
        const int code = function(
            m_Session->Handle, owner, &wire, selectorPointer,
            &run, &info, &status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Handle>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Handle>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<Handle>::Success(
            Handle(Detail::Run(m_Session, run)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Handle>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Handle>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Behavior::Call> Block::Call(
    const Selector &input, std::optional<FramePolicy> frames) const {
    return Call(ObjectRef{}, input, frames);
}

inline Result<Behavior::Call> Block::Call(
    ObjectRef owner, const Selector &input,
    std::optional<FramePolicy> frames) const {
    if (!m_Session || !m_Session->Api)
        return Result<Behavior::Call>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Behavior::Call>(m_Session->Api->Call, owner, &input, frames);
}

inline Result<Task> Block::Start(
    const Selector &input, std::optional<FramePolicy> frames) const {
    return Start(ObjectRef{}, input, frames);
}

inline Result<Task> Block::Start(
    ObjectRef owner, const Selector &input,
    std::optional<FramePolicy> frames) const {
    if (!m_Session || !m_Session->Api)
        return Result<Task>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Task>(m_Session->Api->Start, owner, &input, frames);
}

inline Result<Instance> Block::Spawn(
    std::optional<FramePolicy> frames) const {
    return Spawn(ObjectRef{}, frames);
}

inline Result<Instance> Block::Spawn(
    ObjectRef owner, std::optional<FramePolicy> frames) const {
    auto compiled = Compile();
    if (!compiled)
        return Result<Instance>::Failure(compiled.Code(), compiled.GetStatus());
    try {
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
        BML_BehaviorStatus status = Detail::EmptyStatus();
        BML_BehaviorBlock wire = compiled.Value()->Wire;
        if (frames) {
            wire.Frames.Kind = frames->Kind;
            wire.Frames.Limit = frames->Limit;
        }
        const int code = m_Session->Api->Spawn(
            m_Session->Handle, owner, &wire,
            &run, &info, &status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Instance>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Instance>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<Instance>::Success(
            Instance(Detail::Run(m_Session, run)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Instance>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Instance>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Instance> Block::SpawnIn(
    ObjectRef graph, std::optional<FramePolicy> frames) const {
    auto compiled = Compile();
    if (!compiled)
        return Result<Instance>::Failure(compiled.Code(), compiled.GetStatus());
    if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, AttachBlock))
        return Result<Instance>::Failure(BML_ERROR_VERSION_MISMATCH);
    try {
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
        BML_BehaviorStatus status = Detail::EmptyStatus();
        BML_BehaviorBlock wire = compiled.Value()->Wire;
        if (frames) {
            wire.Frames.Kind = frames->Kind;
            wire.Frames.Limit = frames->Limit;
        }
        const int code = m_Session->Api->AttachBlock(
            m_Session->Handle, graph, &wire,
            &run, &info, &status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Instance>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Instance>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        return Result<Instance>::Success(
            Instance(Detail::Run(m_Session, run)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Instance>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Instance>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Instance> Block::SpawnIn(
    const Graph &graph, std::optional<FramePolicy> frames) const {
    return SpawnIn(graph.m_Root, frames);
}

inline void Edit::Encode(WireProgram &out) const {
    const auto encodePort = [](const Port &source) {
        BML_BehaviorPortRef port{};
        port.StructSize = sizeof(port);
        port.Handle = source.Handle;
        port.Kind = source.Kind;
        port.Type = Detail::WireGuid(source.Type);
        port.Slot = Detail::Wire::From(source.Slot);
        return port;
    };
    std::size_t orderCount = 0;
    for (const Step &step : m_Steps)
        orderCount += step.Ordering.size();
    // Both arrays are sized up front, so nothing a step points at moves.
    out.Ordering.clear();
    out.Ordering.reserve(orderCount);
    for (const Step &step : m_Steps) {
        for (const PatchOrder &order : step.Ordering) {
            BML_BehaviorEditOrder wire{};
            wire.StructSize = sizeof(wire);
            wire.Kind = order.Kind;
            wire.Owner = Detail::Text(order.Owner);
            wire.Name = Detail::Text(order.Name);
            out.Ordering.push_back(wire);
        }
    }

    out.Steps.clear();
    out.Steps.reserve(m_Steps.size());
    std::size_t consumed = 0;
    for (const Step &step : m_Steps) {
        BML_BehaviorEditStep wire{};
        wire.StructSize = sizeof(wire);
        wire.Kind = step.Kind;
        wire.Result = step.Result;
        wire.Flags = step.Flags;
        wire.Target = step.Target;
        wire.Node = step.Node;
        wire.SlotKind = step.SlotKind;
        wire.Delay = step.Delay;
        wire.Name = Detail::Text(step.Name);
        wire.Prototype.StructSize = sizeof(wire.Prototype);
        wire.Prototype.Prototype = Detail::WireGuid(step.PrototypeRef.Id);
        wire.Prototype.Generation = step.PrototypeRef.Generation;
        wire.Type = Detail::WireGuid(step.Type);
        wire.Source = encodePort(step.Source);
        wire.Sink = encodePort(step.Sink);
        if (step.Value)
            wire.Value = Detail::Wire::From(*step.Value);
        wire.Hook = step.Hook ? &step.Hook->Function : nullptr;
        wire.Object = step.Object;
        wire.OrderCount = static_cast<std::uint32_t>(step.Ordering.size());
        wire.Ordering = wire.OrderCount
            ? out.Ordering.data() + consumed : nullptr;
        consumed += step.Ordering.size();
        out.Steps.push_back(wire);
    }
}

inline Result<void> Edit::Validate(
    const std::shared_ptr<Detail::SessionState> &session,
    bool durable) const {
    if (m_Code != BML_OK)
        return Result<void>::Failure(m_Code, m_Status);
    if (!session || !session->Api || !session->Handle)
        return Result<void>::Failure(BML_ERROR_INVALID_HANDLE);
    if (m_Session && m_Session != session) {
        Status status;
        status.Error = Behavior::Error::OwnerUnavailable;
        status.Phase = Behavior::Phase::Edit;
        status.Message =
            "The Edit and its destination belong to different Sessions.";
        return Result<void>::Failure(BML_ERROR_INVALID_PARAMETER,
                                     std::move(status));
    }
    if (durable) {
        for (const Step &step : m_Steps) {
            const bool liveReference =
                step.Kind == BML_BEHAVIOR_EDIT_USE_NODE ||
                step.Kind == BML_BEHAVIOR_EDIT_USE_LINK ||
                (step.Value && step.Value->Kind() == ValueKind::Object &&
                 !step.Value->IsNull());
            if (!liveReference)
                continue;
            Status status;
            status.Error = Behavior::Error::WorldBoundValue;
            status.Phase = Behavior::Phase::Edit;
            status.Message =
                "A Plan cannot retain an object reference from one live world.";
            return Result<void>::Failure(BML_ERROR_INVALID_PARAMETER,
                                         std::move(status));
        }
    }
    for (const Detail::BlockDefinition &block : m_Blocks) {
        auto compiled = Detail::Compiler{}(session, block);
        if (!compiled)
            return Result<void>::Failure(compiled.Code(), compiled.GetStatus());
    }
    return Result<void>::Success();
}

inline Result<Patch> Graph::Apply(std::string_view name,
                                  const Edit &edit) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<Patch>::Failure(BML_ERROR_INVALID_HANDLE);
    if (name.empty() || !m_Root.Domain || m_View != View::Logical)
        return Result<Patch>::Failure(BML_ERROR_INVALID_PARAMETER);
    if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, ClosePatch))
        return Result<Patch>::Failure(BML_ERROR_VERSION_MISMATCH);
    Result<void> valid = edit.Validate(m_Session);
    if (!valid)
        return Result<Patch>::Failure(valid.Code(), valid.GetStatus());
    try {
        auto current = Graph::Read(m_Session, m_Root, View::Logical);
        if (!current)
            return Result<Patch>::Failure(current.Code(), current.GetStatus());
        if (current->Fingerprint() != m_Fingerprint) {
            Status changed;
            changed.Error = Error::GraphChanged;
            changed.Phase = Phase::Edit;
            changed.Message =
                "The logical Graph changed after this snapshot was read.";
            return Result<Patch>::Failure(BML_ERROR_BUSY, std::move(changed));
        }

        Edit::WireProgram program;
        edit.Encode(program);

        BML_BehaviorPatchSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Name = Detail::Text(name);
        spec.Graph = m_Root;
        spec.Steps = program.Steps.empty() ? nullptr : program.Steps.data();
        spec.StepCount = static_cast<std::uint32_t>(program.Steps.size());

        BML_BehaviorPatch handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_Session->Api->ApplyPatch(
            m_Session->Handle, &spec, &handle, nullptr, &status);
        if (code != BML_OK || !handle) {
            if (handle)
                (void) m_Session->Api->ClosePatch(m_Session->Handle, handle);
            return Result<Patch>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<Patch>::Success(
            Patch(m_Session, handle), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Patch>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Patch>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Plan> Session::Plan(std::string_view name,
                                  const Scripts &scripts,
                                  const Edit &edit) const {
    if (!m_State || !m_State->Api || !m_State->Handle)
        return Result<Behavior::Plan>::Failure(BML_ERROR_INVALID_HANDLE);
    if (name.empty() || scripts.m_Name.empty())
        return Result<Behavior::Plan>::Failure(BML_ERROR_INVALID_PARAMETER);
    Result<void> valid = edit.Validate(m_State, true);
    if (!valid)
        return Result<Behavior::Plan>::Failure(valid.Code(), valid.GetStatus());
    try {
        Edit::WireProgram program;
        edit.Encode(program);

        BML_BehaviorPlanSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Targets = scripts.m_Count;
        spec.Name = Detail::Text(name);
        spec.Script = Detail::Text(scripts.m_Name);
        spec.Steps = program.Steps.empty() ? nullptr : program.Steps.data();
        spec.StepCount = static_cast<std::uint32_t>(program.Steps.size());

        BML_BehaviorPlan handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = m_State->Api->SubmitPlan(
            m_State->Handle, &spec, &handle, nullptr, &status);
        if (code != BML_OK || !handle) {
            if (handle)
                (void) m_State->Api->ClosePlan(m_State->Handle, handle);
            return Result<Behavior::Plan>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<Behavior::Plan>::Success(
            Behavior::Plan(m_State, handle), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Plan>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Behavior::Plan>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Task> Call::Continue() && {
    if (!m_Run)
        return Result<Task>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = m_Run.m_Session->Api->Continue(
        m_Run.m_Handle, &info, &status);
    if (code != BML_OK)
        return Result<Task>::Failure(code, Detail::ReadStatus(status));
    return Result<Task>::Success(Task(std::move(m_Run)),
                                 Detail::ReadStatus(status));
}


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_DETAIL_INLINE_HPP
