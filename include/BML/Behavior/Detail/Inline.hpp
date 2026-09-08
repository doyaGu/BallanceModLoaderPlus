#ifndef BML_BEHAVIOR_DETAIL_INLINE_HPP
#define BML_BEHAVIOR_DETAIL_INLINE_HPP

#include "BML/Behavior/Session.hpp"
#include "BML/Behavior/Detail/EditProgram.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
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
        code = WireCode(code, status);
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
        code = WireCode(code, status);
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
                if (!RecordAt(payload, record.ManagerOffset, index, manager) ||
                    !KnownFlag(manager.Available))
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
    code = WireCode(code, status);
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
        code = WireCode(code, status);
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

inline Status CheckBlock(const BlockSpec &spec) {
    constexpr std::size_t maximum =
        (std::numeric_limits<std::uint32_t>::max)();
    if (!spec.PrototypeRef.Id.IsValid()) {
        return BlockError(BML_BEHAVIOR_ERROR_PROTOTYPE_NOT_FOUND,
                          BML_BEHAVIOR_PHASE_PROTOTYPE,
                          spec.PrototypeRef, CKGUID(0, 0),
                          "A Block requires a Prototype GUID.");
    }
    if ((spec.TargetKind == BML_BEHAVIOR_TARGET_OBJECT ||
         spec.TargetKind == BML_BEHAVIOR_TARGET_NULL) &&
        !spec.TargetType.IsValid()) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          spec.PrototypeRef, CKGUID(0, 0),
                          "An explicit Target requires a parameter type.");
    }
    if (spec.TargetKind == BML_BEHAVIOR_TARGET_OBJECT &&
        !spec.TargetObject.Domain) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          spec.PrototypeRef, spec.TargetType,
                          "An explicit Target requires a live object reference.");
    }
    if (spec.TargetKind != BML_BEHAVIOR_TARGET_OWNER &&
        spec.TargetKind != BML_BEHAVIOR_TARGET_OBJECT &&
        spec.TargetKind != BML_BEHAVIOR_TARGET_NULL) {
        return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                          BML_BEHAVIOR_PHASE_TARGET,
                          spec.PrototypeRef, spec.TargetType,
                          "The Block Target kind is unknown.");
    }
    if (spec.Settings.size() > maximum ||
        spec.Pins.size() > maximum ||
        spec.Locals.size() > maximum ||
        spec.PinTypes.size() > maximum ||
        spec.PoutTypes.size() > maximum) {
        return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                          BML_BEHAVIOR_PHASE_NONE,
                          spec.PrototypeRef, CKGUID(0, 0),
                          "The Block contains too many bindings.");
    }
    for (const auto &stage : spec.Settings) {
        if (stage.size() > maximum) {
            return BlockError(BML_BEHAVIOR_ERROR_VALUE_INVALID,
                              BML_BEHAVIOR_PHASE_SETTINGS,
                              spec.PrototypeRef, CKGUID(0, 0),
                              "A Block contains too many Settings in one stage.");
        }
    }
    const auto checkTypes = [&](const auto &types) -> Status {
        for (const BlockSpec::ParameterType &parameter : types) {
            const BML_BehaviorSelector slot = Wire::From(parameter.Slot);
            if (!parameter.Type.IsValid()) {
                return BlockError(
                    BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNAVAILABLE,
                    BML_BEHAVIOR_PHASE_LAYOUT, spec.PrototypeRef,
                    parameter.Type,
                    "A variable parameter requires a registered type GUID.");
            }
            if ((slot.Kind == BML_BEHAVIOR_SELECTOR_INDEX && slot.Index < 0) ||
                ((slot.Kind == BML_BEHAVIOR_SELECTOR_NAME ||
                  slot.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME) &&
                 slot.Name.Length == 0)) {
                return BlockError(
                    BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
                    BML_BEHAVIOR_PHASE_LAYOUT, spec.PrototypeRef,
                    parameter.Type,
                    "A variable parameter selector is empty or invalid.");
            }
        }
        return {};
    };
    Status types = checkTypes(spec.PinTypes);
    if (!types)
        return types;
    types = checkTypes(spec.PoutTypes);
    if (!types)
        return types;
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

inline Status CheckTarget(const Behavior::Layout &layout,
                          const BlockSpec &spec) {
    if (spec.TargetKind == BML_BEHAVIOR_TARGET_OWNER)
        return {};
    if ((layout.BehaviorFlags & CKBEHAVIOR_TARGETABLE) != 0)
        return {};
    return BlockError(BML_BEHAVIOR_ERROR_TARGET_INVALID,
                      BML_BEHAVIOR_PHASE_TARGET,
                      layout.PrototypeRef, spec.TargetType,
                      "The Prototype does not declare an explicit Target.");
}

inline Status CheckParameterType(
    const Behavior::Layout &layout,
    const BlockSpec::ParameterType &parameter,
    SlotKind kind) {
    const BML_BehaviorSelector selector = Wire::From(parameter.Slot);
    std::vector<const Behavior::Slot *> matches;
    for (const Behavior::Slot &slot : layout.Slots) {
        if (slot.Kind != kind)
            continue;
        bool match = false;
        switch (selector.Kind) {
        case BML_BEHAVIOR_SELECTOR_INDEX:
            match = slot.Index == selector.Index;
            break;
        case BML_BEHAVIOR_SELECTOR_NAME:
            match = slot.Name == std::string_view(
                        selector.Name.Data, selector.Name.Length) &&
                    slot.Occurrence == selector.Occurrence;
            break;
        case BML_BEHAVIOR_SELECTOR_UNIQUE_NAME:
            match = slot.Name == std::string_view(
                        selector.Name.Data, selector.Name.Length);
            break;
        case BML_BEHAVIOR_SELECTOR_ONLY:
            match = true;
            break;
        default:
            break;
        }
        if (match)
            matches.push_back(&slot);
    }
    if (matches.empty()) {
        const std::uint32_t flag = kind == SlotKind::Pin
            ? (CKBEHAVIOR_VARIABLEPARAMETERINPUTS |
               CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS)
            : (CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS |
               CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS);
        if ((layout.BehaviorFlags & flag) != 0)
            return {};
        return BlockError(
            BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND,
            BML_BEHAVIOR_PHASE_LAYOUT, layout.PrototypeRef,
            parameter.Type,
            std::string(kind == SlotKind::Pin ? "Pin " : "Pout ") +
                SelectorLabel(selector) +
                " is absent from the declared Layout.");
    }
    if (matches.size() != 1) {
        return BlockError(
            BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS,
            BML_BEHAVIOR_PHASE_LAYOUT, layout.PrototypeRef,
            parameter.Type,
            std::string(kind == SlotKind::Pin ? "Pin " : "Pout ") +
                SelectorLabel(selector) +
                " is ambiguous in the declared Layout.");
    }
    if (!matches.front()->Dynamic) {
        return BlockError(
            BML_BEHAVIOR_ERROR_INTERFACE_UNSUPPORTED,
            BML_BEHAVIOR_PHASE_LAYOUT, layout.PrototypeRef,
            parameter.Type,
            std::string(kind == SlotKind::Pin ? "Pin " : "Pout ") +
                SelectorLabel(selector) +
                " belongs to a fixed native interface.");
    }
    return {};
}

inline CompiledBlock::CompiledBlock(BlockSpec spec,
                                    bool declared)
    : Spec(std::move(spec)), Declared(declared) {
    while (Spec.Settings.size() > 1 &&
           Spec.Settings.back().empty())
        Spec.Settings.pop_back();
    if (Spec.Settings.size() == 1 && Spec.Settings.front().empty())
        Spec.Settings.clear();

    SettingBindings.reserve(Spec.Settings.size());
    SettingStages.reserve(Spec.Settings.size());
    for (const std::vector<SlotValue> &stage : Spec.Settings) {
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
    add(Spec.Pins, Pins);
    add(Spec.Locals, Locals);
    const auto addTypes = [](
        const std::vector<BlockSpec::ParameterType> &from,
        std::vector<BML_BehaviorParameterType> &to) {
        to.reserve(from.size());
        for (const BlockSpec::ParameterType &parameter : from) {
            BML_BehaviorParameterType wire{};
            wire.StructSize = sizeof(wire);
            wire.Slot = Wire::From(parameter.Slot);
            wire.Type = WireGuid(parameter.Type);
            to.push_back(wire);
        }
    };
    addTypes(Spec.PinTypes, PinTypes);
    addTypes(Spec.PoutTypes, PoutTypes);

    Wire.StructSize = sizeof(Wire);
    Wire.Prototype = WireGuid(Spec.PrototypeRef.Id);
    Wire.Target.StructSize = sizeof(Wire.Target);
    Wire.Target.Kind = Spec.TargetKind;
    Wire.Target.Type = WireGuid(Spec.TargetType);
    Wire.Target.Object = Spec.TargetObject;
    Wire.SettingStages = SettingStages.empty() ? nullptr : SettingStages.data();
    Wire.SettingStageCount = static_cast<std::uint32_t>(SettingStages.size());
    Wire.Pins = Pins.empty() ? nullptr : Pins.data();
    Wire.PinCount = static_cast<std::uint32_t>(Pins.size());
    Wire.Locals = Locals.empty() ? nullptr : Locals.data();
    Wire.LocalCount = static_cast<std::uint32_t>(Locals.size());
    Wire.PinTypes = PinTypes.empty() ? nullptr : PinTypes.data();
    Wire.PinTypeCount = static_cast<std::uint32_t>(PinTypes.size());
    Wire.PoutTypes = PoutTypes.empty() ? nullptr : PoutTypes.data();
    Wire.PoutTypeCount = static_cast<std::uint32_t>(PoutTypes.size());
    Wire.PrototypeGeneration = Spec.PrototypeRef.Generation;
}

inline Result<std::shared_ptr<const CompiledBlock>> Compiler::operator()(
    const std::shared_ptr<SessionState> &session,
    BlockSpec spec, bool requireDeclared) const {
    if (!session || !session->Api || !session->Handle) {
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            BML_ERROR_INVALID_HANDLE);
    }
    Status checked = CheckBlock(spec);
    if (!checked) {
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            BML_ERROR_INVALID_PARAMETER, std::move(checked));
    }
    auto declared = ReadDeclared(session, spec.PrototypeRef);
    if (!declared) {
        if (declared.Code() == BML_ERROR_UNAVAILABLE) {
            if (requireDeclared)
                return Result<std::shared_ptr<const CompiledBlock>>::Failure(
                    declared.Code(), declared.GetStatus());
            std::shared_ptr<const CompiledBlock> block =
                std::make_shared<CompiledBlock>(std::move(spec), false);
            return Result<std::shared_ptr<const CompiledBlock>>::Success(
                std::move(block));
        }
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            declared.Code(), declared.GetStatus());
    }
    if (!spec.Settings.empty()) {
        for (const SlotValue &setting : spec.Settings.front()) {
            checked = CheckSetting(declared.Value(), setting);
            if (!checked) {
                return Result<std::shared_ptr<const CompiledBlock>>::Failure(
                    BML_ERROR_FAIL, std::move(checked));
            }
        }
    }
    checked = CheckTarget(declared.Value(), spec);
    if (!checked) {
        return Result<std::shared_ptr<const CompiledBlock>>::Failure(
            BML_ERROR_FAIL, std::move(checked));
    }
    for (const BlockSpec::ParameterType &parameter : spec.PinTypes) {
        checked = CheckParameterType(
            declared.Value(), parameter, SlotKind::Pin);
        if (!checked)
            return Result<std::shared_ptr<const CompiledBlock>>::Failure(
                BML_ERROR_FAIL, std::move(checked));
    }
    for (const BlockSpec::ParameterType &parameter : spec.PoutTypes) {
        checked = CheckParameterType(
            declared.Value(), parameter, SlotKind::Pout);
        if (!checked)
            return Result<std::shared_ptr<const CompiledBlock>>::Failure(
                BML_ERROR_FAIL, std::move(checked));
    }
    spec.PrototypeRef = declared.Value().PrototypeRef;
    std::shared_ptr<const CompiledBlock> block =
        std::make_shared<CompiledBlock>(std::move(spec), true);
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
    if (source.StructSize < sizeof(source) ||
        !KnownObservationState(source.State) ||
        !KnownRelation(source.Relation))
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
    if (!ReadPoutData(value, payload, out.Data))
        return false;
    if (source.Kind == BML_BEHAVIOR_VALUE_OBJECT &&
        !ValidObjectRef(std::get<BML_ObjectRef>(out.Data)))
        return false;
    return true;
}

inline bool ReadWatchValue(const BML_BehaviorWatchValue &source,
                           ObservedValue &out) {
    if (source.StructSize < sizeof(source) ||
        source.Value.StructSize < sizeof(source.Value) ||
        !KnownObservationState(source.State) ||
        !KnownRelation(source.Relation))
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
        if (!KnownFlag(source.Value.Data.Bool))
            return false;
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
    case BML_BEHAVIOR_VALUE_OBJECT:
        if (!ValidObjectRef(source.Value.Data.Object))
            return false;
        out.Data = source.Value.Data.Object;
        break;
    default: return false;
    }
    return true;
}

inline bool ReadChange(const BML_BehaviorWatchEvent *source, Change &out) {
    if (!source || source->StructSize < sizeof(*source) ||
        source->Kind < BML_BEHAVIOR_WATCH_GRAPH ||
        source->Kind > BML_BEHAVIOR_WATCH_SAMPLED_VALUE)
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
    if (!session || !session->Api || !session->Handle ||
        !Detail::ValidObjectRef(root) || !root.Domain)
        return Result<Graph>::Failure(BML_ERROR_INVALID_HANDLE);
    try {
        BML_BehaviorGraph wire{};
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        const int code = Detail::ReadPayload(
            *session, [&](void *payload, std::uint32_t capacity,
                          std::uint32_t &written) {
                wire = {};
                wire.StructSize = sizeof(wire);
                status = Detail::EmptyStatus();
                return Detail::WireCode(session->Api->Inspect(
                    session->Handle, root, static_cast<std::uint32_t>(view),
                    &wire, payload, capacity, &written, &status), status);
            }, payloadSize);
        if (code != BML_OK)
            return Result<Graph>::Failure(code, Detail::ReadStatus(status));
        const std::uint8_t *payload = session->Buffer.data();
        return Decode(std::move(session), view, root, wire,
                      payload, payloadSize, status);
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
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        const int code = Detail::ReadPayload(
            *session, [&](void *payload, std::uint32_t capacity,
                          std::uint32_t &written) {
                wire = {};
                wire.StructSize = sizeof(wire);
                status = Detail::EmptyStatus();
                return Detail::WireCode(session->Api->InspectRun(
                    run, static_cast<std::uint32_t>(view), &wire,
                    payload, capacity, &written, &status), status);
            }, payloadSize);
        if (code != BML_OK)
            return Result<Graph>::Failure(code, Detail::ReadStatus(status));
        const std::uint8_t *payload = session->Buffer.data();
        return Decode(std::move(session), view, {}, wire,
                      payload, payloadSize, status);
    } catch (const std::bad_alloc &) {
        return Result<Graph>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Graph>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Graph> Graph::Decode(
    std::shared_ptr<Detail::SessionState> session, View view,
    BML_ObjectRef expectedRoot,
    const BML_BehaviorGraph &wire,
    const std::uint8_t *payloadData, std::size_t payloadSize,
    const BML_BehaviorStatus &status) {
    const Detail::PayloadView payload{payloadData, payloadSize};
    const auto malformed = [](std::string message) {
        Behavior::Status detail;
        detail.Message = std::move(message);
        return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE,
                                      std::move(detail));
    };
    if (wire.StructSize < sizeof(wire) ||
        wire.View != static_cast<std::uint32_t>(view) ||
        !Detail::ValidObjectRef(wire.Root) || !wire.Root.Domain ||
        (expectedRoot.Domain &&
         !(Detail::ObjectKey(expectedRoot) == Detail::ObjectKey(wire.Root))) ||
        !Detail::RecordsFit<BML_BehaviorGraphNode>(
            payload, wire.NodeOffset, wire.NodeCount) ||
        !Detail::RecordsFit<BML_BehaviorGraphLink>(
            payload, wire.LinkOffset, wire.LinkCount) ||
        !Detail::RecordsFit<BML_BehaviorGraphOperation>(
            payload, wire.OperationOffset, wire.OperationCount))
        return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);

    Graph graph;
    graph.m_Session = std::move(session);
    graph.m_Root = wire.Root;
    graph.m_View = view;
    graph.m_Generation = wire.Generation;
    graph.m_Fingerprint = wire.Fingerprint;
    auto data = std::make_shared<Detail::GraphData>();
    std::size_t portCount = 0;
    for (std::uint32_t index = 0; index < wire.NodeCount; ++index) {
        BML_BehaviorGraphNode record{};
        if (!Detail::RecordAt(payload, wire.NodeOffset, index, record) ||
            !Detail::RecordsFit<BML_BehaviorGraphPort>(
                payload, record.PortOffset, record.PortCount) ||
            record.PortCount >
                (std::numeric_limits<std::size_t>::max)() - portCount)
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        portCount += record.PortCount;
    }
    data->Nodes.reserve(wire.NodeCount);
    data->Ports.reserve(portCount);
    if (wire.LinkCount >
            (std::numeric_limits<std::size_t>::max)() - wire.NodeCount ||
        wire.OperationCount >
            (std::numeric_limits<std::size_t>::max)() - wire.NodeCount -
                wire.LinkCount)
        return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
    const std::size_t identityCount =
        static_cast<std::size_t>(wire.NodeCount) + wire.LinkCount +
        wire.OperationCount;
    std::unordered_set<std::uint64_t> identities;
    std::unordered_set<Detail::ObjectKey, Detail::ObjectKeyHash> objects;
    std::unordered_map<Detail::PortKey, std::size_t,
                       Detail::PortKeyHash> ports;
    identities.reserve(identityCount);
    objects.reserve(identityCount);
    ports.reserve(portCount);
    for (std::uint32_t index = 0; index < wire.NodeCount; ++index) {
        BML_BehaviorGraphNode record{};
        if (!Detail::RecordAt(payload, wire.NodeOffset, index, record) ||
            !Detail::RecordsFit<BML_BehaviorGraphPort>(
                payload, record.PortOffset, record.PortCount))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        Detail::GraphNodeData node;
        node.Id = record.Id;
        node.Object = record.Object;
        node.Parent = record.Parent;
        node.Index = record.Index;
        node.Occurrence = record.Occurrence;
        node.LayoutGeneration = record.LayoutGeneration;
        if (record.Kind != BML_BEHAVIOR_KIND_FUNCTION &&
            record.Kind != BML_BEHAVIOR_KIND_CALLBACK &&
            record.Kind != BML_BEHAVIOR_KIND_GRAPH)
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        node.Kind = static_cast<BehaviorKind>(record.Kind);
        node.Prototype = Detail::NativeGuid(record.Prototype);
        node.Priority = record.Priority;
        node.Active = record.Active != 0;
        if (!node.Id || !Detail::ValidObjectRef(node.Object) ||
            !node.Object.Domain || node.Index < -1 || node.Occurrence < 0 ||
            !node.LayoutGeneration ||
            !identities.emplace(node.Id).second ||
            !objects.emplace(node.Object).second ||
            !Detail::KnownFlag(record.Active))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        if (!Detail::TextAt(payload, record.Name.Offset,
                            record.Name.Length, node.Name))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        node.PortOffset = data->Ports.size();
        node.PortCount = record.PortCount;
        const std::size_t nodeIndex = data->Nodes.size();
        std::unordered_map<std::uint32_t, std::int32_t> portIndices;
        std::map<std::pair<std::uint32_t, std::string>, std::int32_t>
            portOccurrences;
        for (std::uint32_t portIndex = 0;
             portIndex < record.PortCount; ++portIndex) {
            BML_BehaviorGraphPort portRecord{};
            if (!Detail::RecordAt(payload, record.PortOffset,
                                  portIndex, portRecord))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            Detail::GraphPortData port;
            port.Node = nodeIndex;
            port.LayoutGeneration = portRecord.LayoutGeneration;
            if (portRecord.Node != node.Id || portRecord.Index < 0 ||
                portRecord.LayoutGeneration != node.LayoutGeneration ||
                portRecord.Occurrence < 0 ||
                !Detail::KnownSlotKind(portRecord.Kind) ||
                (portRecord.Flags & ~BML_BEHAVIOR_SLOT_DYNAMIC) != 0 ||
                !Detail::KnownFlag(portRecord.Active) ||
                (portRecord.Kind != BML_BEHAVIOR_SLOT_IN &&
                 portRecord.Kind != BML_BEHAVIOR_SLOT_OUT &&
                 portRecord.Active != 0))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            port.Kind = static_cast<SlotKind>(portRecord.Kind);
            port.Type = Detail::NativeGuid(portRecord.Type);
            port.Dynamic =
                (portRecord.Flags & BML_BEHAVIOR_SLOT_DYNAMIC) != 0;
            port.Index = portRecord.Index;
            port.Occurrence = portRecord.Occurrence;
            port.Active = portRecord.Active != 0;
            if (!Detail::TextAt(payload, portRecord.Name.Offset,
                                 portRecord.Name.Length, port.Name))
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            const std::int32_t expectedIndex =
                portIndices[portRecord.Kind]++;
            const std::int32_t expectedOccurrence =
                portOccurrences[{portRecord.Kind, port.Name}]++;
            if ((port.Kind != SlotKind::Local &&
                 port.Index != expectedIndex) ||
                port.Occurrence != expectedOccurrence)
                return malformed(
                    "A Behavior Graph port has a non-canonical index or name occurrence.");
            if (!ports.emplace(
                    Detail::PortKey{
                        node.Id, portRecord.Kind, portRecord.Index},
                    data->Ports.size()).second)
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            data->Ports.push_back(std::move(port));
        }
        data->Nodes.push_back(std::move(node));
    }
    std::size_t rootCount = 0;
    std::uint64_t rootId = 0;
    for (std::size_t index = 0; index < data->Nodes.size(); ++index) {
        const Detail::GraphNodeData &node = data->Nodes[index];
        const bool root = node.Object.Domain == graph.m_Root.Domain &&
            node.Object.Slot == graph.m_Root.Slot &&
            node.Object.Generation == graph.m_Root.Generation;
        if (!root)
            continue;
        if (node.Parent != 0)
            return malformed(
                "The Behavior Graph root unexpectedly has a parent.");
        ++rootCount;
        rootId = node.Id;
        data->Root = index;
    }
    if (rootCount != 1)
        return malformed(
            "The Behavior Graph does not contain exactly one root record.");
    std::int32_t childIndex = 0;
    std::unordered_map<std::string, std::int32_t> nodeOccurrences;
    for (const Detail::GraphNodeData &node : data->Nodes) {
        if (node.Id == rootId) {
            if (node.Index != -1 || node.Occurrence != 0)
                return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            continue;
        }
        const std::int32_t expectedOccurrence =
            nodeOccurrences[node.Name]++;
        if (node.Parent != rootId || node.Index != childIndex++ ||
            node.Occurrence != expectedOccurrence)
            return malformed(
                "A Behavior Graph child has a non-canonical parent, index, or name occurrence.");
    }
    const auto endpoint = [&](std::uint64_t nodeId, std::uint32_t kind,
                              std::int32_t index, std::size_t &out) {
        const auto port = ports.find(
            Detail::PortKey{nodeId, kind, index});
        if (port == ports.end())
            return false;
        out = port->second;
        return true;
    };
    data->Links.reserve(wire.LinkCount);
    for (std::uint32_t index = 0; index < wire.LinkCount; ++index) {
        BML_BehaviorGraphLink record{};
        if (!Detail::RecordAt(payload, wire.LinkOffset, index, record))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        if (!record.Id || !Detail::ValidObjectRef(record.Object) ||
            !record.Object.Domain ||
            !identities.emplace(record.Id).second ||
            !objects.emplace(record.Object).second ||
            record.SourceIndex < 0 || record.TargetIndex < 0 ||
            (record.SourceKind != BML_BEHAVIOR_SLOT_IN &&
             record.SourceKind != BML_BEHAVIOR_SLOT_OUT) ||
            (record.TargetKind != BML_BEHAVIOR_SLOT_IN &&
             record.TargetKind != BML_BEHAVIOR_SLOT_OUT) ||
            !Detail::KnownTruth(record.Pending))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        if (record.SourceOrder < 0)
            return malformed(
                "A Behavior Graph Link has an invalid source order.");
        Detail::GraphLinkData link;
        link.Id = record.Id;
        link.Object = record.Object;
        if (!endpoint(record.SourceNode, record.SourceKind,
                      record.SourceIndex, link.Source) ||
            !endpoint(record.TargetNode, record.TargetKind,
                      record.TargetIndex, link.Target))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        link.SourceOrder = record.SourceOrder;
        link.InitialDelay = record.InitialDelay;
        link.RemainingDelay = record.RemainingDelay;
        link.Pending = static_cast<TruthValue>(record.Pending);
        data->Links.push_back(std::move(link));
    }
    data->OutgoingLinks.resize(data->Links.size());
    for (std::size_t index = 0; index < data->OutgoingLinks.size(); ++index)
        data->OutgoingLinks[index] = index;
    std::sort(
        data->OutgoingLinks.begin(), data->OutgoingLinks.end(),
        [&](std::size_t left, std::size_t right) {
            const auto &a = data->Links[left];
            const auto &b = data->Links[right];
            if (a.Source != b.Source)
                return a.Source < b.Source;
            if (a.SourceOrder != b.SourceOrder)
                return a.SourceOrder < b.SourceOrder;
            return a.Id < b.Id;
        });
    for (std::size_t index = 1; index < data->OutgoingLinks.size(); ++index) {
        const auto &before = data->Links[data->OutgoingLinks[index - 1]];
        const auto &after = data->Links[data->OutgoingLinks[index]];
        if (before.Source == after.Source &&
            before.SourceOrder == after.SourceOrder)
            return malformed(
                "Two Behavior Graph Links occupy the same source order.");
    }
    data->IncomingLinks.resize(data->Links.size());
    for (std::size_t index = 0; index < data->IncomingLinks.size(); ++index)
        data->IncomingLinks[index] = index;
    std::stable_sort(
        data->IncomingLinks.begin(), data->IncomingLinks.end(),
        [&](std::size_t left, std::size_t right) {
            return data->Links[left].Target < data->Links[right].Target;
        });
    data->Operations.reserve(wire.OperationCount);
    for (std::uint32_t index = 0; index < wire.OperationCount; ++index) {
        BML_BehaviorGraphOperation record{};
        if (!Detail::RecordAt(payload, wire.OperationOffset, index, record))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        Detail::GraphOperationData operation;
        operation.Id = record.Id;
        operation.Object = record.Object;
        operation.Owner = record.Owner;
        operation.Function = Detail::NativeGuid(record.Function);
        operation.Result = Detail::NativeGuid(record.Result);
        operation.Input1 = Detail::NativeGuid(record.Input1);
        operation.Input2 = Detail::NativeGuid(record.Input2);
        if (!operation.Id || operation.Owner != rootId ||
            !Detail::ValidObjectRef(operation.Object) ||
            !operation.Object.Domain || !operation.Function.IsValid() ||
            !operation.Result.IsValid() ||
            (!operation.Input1.IsValid() && operation.Input2.IsValid()) ||
            !identities.emplace(operation.Id).second ||
            !objects.emplace(operation.Object).second ||
            !Detail::TextAt(payload, record.Name.Offset,
                            record.Name.Length, operation.Name))
            return Result<Graph>::Failure(BML_ERROR_MALFORMED_MESSAGE);
        data->Operations.push_back(std::move(operation));
    }
    graph.m_Data = std::move(data);
    return Result<Graph>::Success(std::move(graph),
                                  Detail::ReadStatus(status));
}

namespace Detail {

inline Result<Link> UniqueLink(LinkRange links, std::string_view direction) {
    auto current = links.begin();
    if (current == links.end()) {
        Status status;
        status.Error = Error::LinkNotFound;
        status.Phase = Phase::Edit;
        status.Message = "No Behavior Link is " + std::string(direction) +
            " the selected graph object.";
        return Result<Link>::Failure(BML_ERROR_NOT_FOUND, std::move(status));
    }
    Link match = *current;
    if (++current != links.end()) {
        Status status;
        status.Error = Error::QueryAmbiguous;
        status.Phase = Phase::Edit;
        status.Message = "More than one Behavior Link is " +
            std::string(direction) + " the selected graph object.";
        return Result<Link>::Failure(BML_ERROR_FAIL, std::move(status));
    }
    return Result<Link>::Success(std::move(match));
}

} // namespace Detail

inline Result<Link> Graph::Entering(const Node &node) const {
    return Detail::UniqueLink(Incoming(node), "entering");
}

inline Result<Link> Graph::Entering(const Port &port) const {
    return Detail::UniqueLink(Incoming(port), "entering");
}

inline Result<Link> Graph::Leaving(const Node &node) const {
    return Detail::UniqueLink(Outgoing(node), "leaving");
}

inline Result<Link> Graph::Leaving(const Port &port) const {
    return Detail::UniqueLink(Outgoing(port), "leaving");
}

inline Result<Node> Graph::Previous(const Node &node) const {
    auto link = Entering(node);
    if (!link)
        return Result<Node>::Failure(link.Code(), link.GetStatus());
    const Port source = link->Source();
    return Result<Node>::Success(
        Node(m_Data, m_Data->Ports[source.m_Index].Node));
}

inline Result<Node> Graph::Previous(const Port &port) const {
    auto link = Entering(port);
    if (!link)
        return Result<Node>::Failure(link.Code(), link.GetStatus());
    const Port source = link->Source();
    return Result<Node>::Success(
        Node(m_Data, m_Data->Ports[source.m_Index].Node));
}

inline Result<Node> Graph::Next(const Node &node) const {
    auto link = Leaving(node);
    if (!link)
        return Result<Node>::Failure(link.Code(), link.GetStatus());
    const Port target = link->Target();
    return Result<Node>::Success(
        Node(m_Data, m_Data->Ports[target.m_Index].Node));
}

inline Result<Node> Graph::Next(const Port &port) const {
    auto link = Leaving(port);
    if (!link)
        return Result<Node>::Failure(link.Code(), link.GetStatus());
    const Port target = link->Target();
    return Result<Node>::Success(
        Node(m_Data, m_Data->Ports[target.m_Index].Node));
}

inline Result<Graph> Graph::Inspect(const Node &node) const {
    if (!m_Data || node.m_Graph != m_Data) {
        Status status;
        status.Error = Error::GraphLocalityInvalid;
        status.Phase = Phase::Edit;
        status.Message = "The Behavior Node belongs to another Graph snapshot.";
        return Result<Graph>::Failure(BML_ERROR_INVALID_PARAMETER,
                                      std::move(status));
    }
    if (!node.IsGraph()) {
        Status status;
        status.Error = Error::InterfaceUnsupported;
        status.Phase = Phase::Edit;
        status.Message = "The selected Behavior Node is not graph-backed.";
        return Result<Graph>::Failure(BML_ERROR_INVALID_PARAMETER,
                                      std::move(status));
    }
    return Read(m_Session, node.Object(), m_View);
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
        code = WireCode(code, status);
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
            code = WireCode(code, status);
            if (code != BML_OK)
                return Result<Behavior::Layout>::Failure(
                    code, ReadStatus(status));
            if (written != payload.size())
                return Result<Behavior::Layout>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE);
        }
        Behavior::Layout layout;
        if (!ReadLayout(wire, payload, layout) ||
            layout.Origin != LayoutOrigin::Live ||
            layout.PrototypeRef.Id != m_Prototype.Id ||
            (m_Prototype.Generation &&
             layout.PrototypeRef.Generation != m_Prototype.Generation))
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

inline Result<Behavior::Slot> ResolveLiveSlot(
    const Behavior::Layout &layout, SlotKind kind,
    const Selector &selector) {
    const BML_BehaviorSelector wire = Detail::Wire::From(selector);
    const Behavior::Slot *match = nullptr;
    for (const Behavior::Slot &slot : layout.Slots) {
        if (slot.Kind != kind)
            continue;
        bool selected = false;
        switch (wire.Kind) {
        case BML_BEHAVIOR_SELECTOR_ONLY:
            selected = true;
            break;
        case BML_BEHAVIOR_SELECTOR_INDEX:
            selected = slot.Index == wire.Index;
            break;
        case BML_BEHAVIOR_SELECTOR_NAME:
            selected = slot.Occurrence == wire.Occurrence &&
                slot.Name == std::string_view(
                    wire.Name.Data ? wire.Name.Data : "", wire.Name.Length);
            break;
        case BML_BEHAVIOR_SELECTOR_UNIQUE_NAME:
            selected = slot.Name == std::string_view(
                wire.Name.Data ? wire.Name.Data : "", wire.Name.Length);
            break;
        default:
            return Result<Behavior::Slot>::Failure(
                BML_ERROR_INVALID_PARAMETER);
        }
        if (!selected)
            continue;
        if (match) {
            Status status;
            status.Error = Error::SlotAmbiguous;
            status.Phase = Phase::Binding;
            status.Message = "The live Behavior slot selector is ambiguous.";
            return Result<Behavior::Slot>::Failure(
                BML_ERROR_FAIL, std::move(status));
        }
        match = &slot;
    }
    if (!match) {
        Status status;
        status.Error = Error::SlotNotFound;
        status.Phase = Phase::Binding;
        status.Message = "The live Behavior slot selector did not match.";
        return Result<Behavior::Slot>::Failure(
            BML_ERROR_NOT_FOUND, std::move(status));
    }
    return Result<Behavior::Slot>::Success(*match);
}

inline Result<std::uint64_t> Detail::Run::Set(
    const Behavior::Slot &slot, const Behavior::Value &value) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Set))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target = Wire::From(slot);
    const BML_BehaviorValue wire = Wire::From(value);
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = WireCode(
        m_Session->Api->Set(
            m_Handle, &target, &wire, &generation, &status),
        status);
    return code == BML_OK
        ? Result<std::uint64_t>::Success(generation, ReadStatus(status))
        : Result<std::uint64_t>::Failure(code, ReadStatus(status));
}

inline Result<std::uint64_t> Detail::Run::Set(
    SlotKind kind, const Selector &slot,
    const Behavior::Value &value) const {
    if (!*this)
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    auto layout = Layout();
    if (!layout)
        return Result<std::uint64_t>::Failure(
            layout.Code(), layout.GetStatus());
    auto resolved = ResolveLiveSlot(layout.Value(), kind, slot);
    if (!resolved)
        return Result<std::uint64_t>::Failure(
            resolved.Code(), resolved.GetStatus());
    return Set(resolved.Value(), value);
}

inline Result<std::uint64_t> Detail::Run::Bind(
    const Behavior::Slot &slot, const Port &source,
    Relation relation) const {
    if (!*this || !BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface, Bind))
        return Result<std::uint64_t>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorSlotRef target = Wire::From(slot);
    BML_BehaviorValueRef value{};
    value.StructSize = sizeof(value);
    value.Kind = static_cast<std::uint32_t>(source.Kind());
    value.LayoutGeneration = source.LayoutGeneration();
    value.Node = source.Object();
    value.Slot = Wire::From(source.Slot());
    BML_BehaviorStatus status = EmptyStatus();
    std::uint64_t generation = 0;
    const int code = WireCode(
        m_Session->Api->Bind(
            m_Handle, &target, &value,
            static_cast<std::uint32_t>(relation), &generation, &status),
        status);
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
        const int code = WireCode(
            m_Session->Api->Configure(
                m_Handle, &stage, 1, &generation, &status),
            status);
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

inline Result<Behavior::Layout> Graph::Layout(const Node &node) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<Behavior::Layout>::Failure(BML_ERROR_INVALID_HANDLE);
    if (!node || node.m_Graph != m_Data) {
        Status status;
        status.Error = Error::GraphLocalityInvalid;
        status.Phase = Phase::Layout;
        status.Message = "The Node belongs to a different Graph snapshot.";
        return Result<Behavior::Layout>::Failure(
            BML_ERROR_INVALID_PARAMETER, std::move(status));
    }
    try {
        BML_BehaviorLayout wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = m_Session->Api->ReadNodeLayout(
            m_Session->Handle, node.Object(), &wire, nullptr, 0,
            &payloadSize, &status);
        code = Detail::WireCode(code, status);
        if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Behavior::Layout>::Failure(
                code, Detail::ReadStatus(status));
        std::vector<std::uint8_t> payload(payloadSize);
        if (payloadSize) {
            wire = {};
            wire.StructSize = sizeof(wire);
            status = Detail::EmptyStatus();
            std::uint32_t written = 0;
            code = m_Session->Api->ReadNodeLayout(
                m_Session->Handle, node.Object(), &wire, payload.data(),
                payloadSize, &written, &status);
            code = Detail::WireCode(code, status);
            if (code != BML_OK)
                return Result<Behavior::Layout>::Failure(
                    code, Detail::ReadStatus(status));
            if (written != payload.size())
                return Result<Behavior::Layout>::Failure(
                    BML_ERROR_MALFORMED_MESSAGE);
        }
        Behavior::Layout layout;
        if (!Detail::ReadLayout(wire, payload, layout) ||
            layout.Origin != LayoutOrigin::Live ||
            layout.PrototypeRef.Id != node.Prototype())
            return Result<Behavior::Layout>::Failure(
                BML_ERROR_MALFORMED_MESSAGE);
        if (layout.Generation != node.LayoutGeneration()) {
            Status changed;
            changed.Error = Error::LayoutChanged;
            changed.Phase = Phase::Binding;
            changed.Message =
                "The Node belongs to an older Behavior Layout.";
            return Result<Behavior::Layout>::Failure(
                BML_ERROR_FAIL, std::move(changed));
        }
        return Result<Behavior::Layout>::Success(
            std::move(layout), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Layout>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Behavior::Layout>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<ObservedValue> Graph::Read(const Port &port) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<ObservedValue>::Failure(BML_ERROR_INVALID_HANDLE);
    if (!port || port.m_Graph != m_Data) {
        Status status;
        status.Error = Error::GraphLocalityInvalid;
        status.Phase = Phase::Binding;
        status.Message = "The Port belongs to a different Graph snapshot.";
        return Result<ObservedValue>::Failure(
            BML_ERROR_INVALID_PARAMETER, std::move(status));
    }
    try {
        BML_BehaviorSelector selector = Detail::Wire::From(port.Slot());
        BML_BehaviorGraphValue wire{};
        wire.StructSize = sizeof(wire);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        std::uint32_t payloadSize = 0;
        int code = m_Session->Api->ReadValue(
            m_Session->Handle, port.Object(),
            static_cast<std::uint32_t>(port.Kind()), port.LayoutGeneration(),
            &selector,
            BML_BEHAVIOR_READ_NON_FORCING, &wire, nullptr, 0,
            &payloadSize, &status);
        code = Detail::WireCode(code, status);
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
                m_Session->Handle, port.Object(),
                static_cast<std::uint32_t>(port.Kind()),
                port.LayoutGeneration(), &selector,
                BML_BEHAVIOR_READ_NON_FORCING, &wire, payload.data(),
                payloadSize, &written, &status);
            code = Detail::WireCode(code, status);
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
        const int code = Detail::WireCode(
            m_Session->Api->Watch(
                m_Session->Handle, &spec, &function, &handle, &status),
            status);
        Behavior::Watch owned(m_Session, handle);
        Holder::Release(holder);
        holder = nullptr;
        if (code != BML_OK || !handle) {
            return Result<Behavior::Watch>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<Behavior::Watch>::Success(
            std::move(owned),
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
    spec.LayoutGeneration = change.LayoutGeneration;
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
    spec.Node = change.Value.Object();
    spec.SlotKind = static_cast<std::uint32_t>(change.Value.Kind());
    spec.LayoutGeneration = change.Value.LayoutGeneration();
    spec.Slot = Detail::Wire::From(change.Value.Slot());
    spec.Read = BML_BEHAVIOR_READ_NON_FORCING;
    return OpenWatch(spec, std::forward<Function>(callback));
}

inline Detail::BlockSpec &Block::Change() {
    if (!m_State)
        throw std::logic_error("Cannot configure an empty Behavior Block.");
    if (m_State.use_count() != 1)
        m_State = std::make_shared<Detail::BlockState>(m_State->Spec);
    else
        m_State->Compiled.reset();
    return m_State->Spec;
}

inline void Block::SetType(
    std::vector<Detail::BlockSpec::ParameterType> &types,
    const Selector &slot, CKGUID type) {
    const BML_BehaviorSelector requested = Detail::Wire::From(slot);
    const auto same = [&](const Detail::BlockSpec::ParameterType &current) {
        const BML_BehaviorSelector existing =
            Detail::Wire::From(current.Slot);
        if (existing.Kind != requested.Kind ||
            existing.Index != requested.Index ||
            existing.Occurrence != requested.Occurrence ||
            existing.Name.Length != requested.Name.Length)
            return false;
        if (!existing.Name.Length)
            return true;
        return std::string_view(existing.Name.Data, existing.Name.Length) ==
            std::string_view(requested.Name.Data, requested.Name.Length);
    };
    const auto found = std::find_if(types.begin(), types.end(), same);
    if (found == types.end())
        types.push_back({slot, type});
    else
        found->Type = type;
}

inline Result<std::shared_ptr<const Detail::CompiledBlock>> Block::Compile(
    bool requireDeclared) const {
    if (!m_Session || !m_Session->Api || !m_Session->Handle || !m_State)
        return Result<std::shared_ptr<const Detail::CompiledBlock>>::Failure(
            BML_ERROR_INVALID_HANDLE);
    if (m_State->Compiled &&
        (!requireDeclared || m_State->Compiled->Declared)) {
        return Result<std::shared_ptr<const Detail::CompiledBlock>>::Success(
            m_State->Compiled);
    }
    try {
        auto compiled = Detail::Compiler{}(
            m_Session, m_State->Spec, requireDeclared);
        if (!compiled)
            return compiled;
        m_State->Spec = compiled.Value()->Spec;
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
    auto compiled = Compile(true);
    if (!compiled)
        return Result<void>::Failure(compiled.Code(), compiled.GetStatus());
    return Result<void>::Success(compiled.GetStatus());
}

inline Status Block::Accept(const BML_BehaviorRunInfo &info,
                            RunKind kind) const {
    Status status;
    const auto reject = [&](Error error, std::string message) {
        status.Error = error;
        status.Phase = Phase::Prototype;
        status.Prototype = m_State
            ? m_State->Spec.PrototypeRef.Id : CKGUID(0, 0);
        status.Message = std::move(message);
    };
    if (!m_State || !Detail::ValidRunInfo(info) ||
        info.Kind != static_cast<std::uint32_t>(kind)) {
        reject(Error::StateInvalid,
               "The Behavior Run description is malformed.");
        return status;
    }

    const Prototype actual(
        Detail::NativeGuid(info.Prototype.Prototype),
        info.Prototype.Generation);
    const Prototype selected = m_State->Spec.PrototypeRef;
    if (!Detail::HasGuid(actual.Id) || actual.Id != selected.Id) {
        reject(Error::PrototypeChanged,
               "The created Behavior does not use the selected Prototype.");
        return status;
    }
    if (selected.Generation &&
        actual.Generation != selected.Generation) {
        reject(Error::PrototypeChanged,
               "The created Behavior uses another Prototype provider generation.");
        return status;
    }
    if (!selected.Generation && actual.Generation) {
        // This is the first authoritative provider identity for a Block whose
        // declared Layout was unavailable. All unchanged copies share it.
        m_State->Spec.PrototypeRef = actual;
        m_State->Compiled.reset();
    }
    return status;
}

template <class Handle, class Function>
Result<Handle> Block::Open(Function function, RunKind kind, ObjectRef owner,
                           const Selector *input, FramePolicy frames) const {
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
        BML_BehaviorFramePolicy wireFrames{};
        wireFrames.StructSize = sizeof(wireFrames);
        wireFrames.Kind = frames.Kind;
        wireFrames.Limit = frames.Limit;
        wireFrames.Flags = frames.Flags;
        const int code = Detail::WireCode(
            function(m_Session->Handle, owner, &wire, &wireFrames, selectorPointer,
                     &run, &info, &status),
            status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Handle>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Handle>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        const Prototype prototype(
            Detail::NativeGuid(info.Prototype.Prototype),
            info.Prototype.Generation);
        Detail::Run owned(m_Session, run, kind, prototype);
        Status accepted = Accept(info, kind);
        if (!accepted) {
            return Result<Handle>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, std::move(accepted));
        }
        return Result<Handle>::Success(
            Handle(std::move(owned)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Handle>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Handle>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Behavior::Call> Block::Call(
    const Selector &input, FramePolicy frames) const {
    return Call(ObjectRef{}, input, frames);
}

inline Result<Behavior::Call> Block::Call(
    ObjectRef owner, const Selector &input,
    FramePolicy frames) const {
    if (!m_Session || !m_Session->Api)
        return Result<Behavior::Call>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Behavior::Call>(m_Session->Api->Call, RunKind::Call,
                                owner, &input, frames);
}

inline Result<Task> Block::Start(
    const Selector &input, FramePolicy frames) const {
    return Start(ObjectRef{}, input, frames);
}

inline Result<Task> Block::Start(
    ObjectRef owner, const Selector &input,
    FramePolicy frames) const {
    if (!m_Session || !m_Session->Api)
        return Result<Task>::Failure(BML_ERROR_INVALID_HANDLE);
    return Open<Task>(m_Session->Api->Start, RunKind::Task,
                      owner, &input, frames);
}

inline Result<Instance> Block::Spawn(
    FramePolicy frames) const {
    return Spawn(ObjectRef{}, frames);
}

inline Result<Instance> Block::Spawn(
    ObjectRef owner, FramePolicy frames) const {
    auto compiled = Compile();
    if (!compiled)
        return Result<Instance>::Failure(compiled.Code(), compiled.GetStatus());
    try {
        BML_BehaviorRun run = nullptr;
        BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
        BML_BehaviorStatus status = Detail::EmptyStatus();
        BML_BehaviorBlock wire = compiled.Value()->Wire;
        BML_BehaviorFramePolicy wireFrames{};
        wireFrames.StructSize = sizeof(wireFrames);
        wireFrames.Kind = frames.Kind;
        wireFrames.Limit = frames.Limit;
        wireFrames.Flags = frames.Flags;
        const int code = Detail::WireCode(
            m_Session->Api->Spawn(
                m_Session->Handle, owner, &wire, &wireFrames,
                &run, &info, &status),
            status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Instance>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Instance>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        const Prototype prototype(
            Detail::NativeGuid(info.Prototype.Prototype),
            info.Prototype.Generation);
        Detail::Run owned(
            m_Session, run, RunKind::Instance, prototype);
        Status accepted = Accept(info, RunKind::Instance);
        if (!accepted) {
            return Result<Instance>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, std::move(accepted));
        }
        return Result<Instance>::Success(
            Instance(std::move(owned)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Instance>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Instance>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Instance> Block::SpawnIn(
    ObjectRef graph, FramePolicy frames) const {
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
        BML_BehaviorFramePolicy wireFrames{};
        wireFrames.StructSize = sizeof(wireFrames);
        wireFrames.Kind = frames.Kind;
        wireFrames.Limit = frames.Limit;
        wireFrames.Flags = frames.Flags;
        const int code = Detail::WireCode(
            m_Session->Api->AttachBlock(
                m_Session->Handle, graph, &wire, &wireFrames,
                &run, &info, &status),
            status);
        if (code != BML_OK) {
            if (run && m_Session->Api->CloseRun)
                m_Session->Api->CloseRun(run);
            return Result<Instance>::Failure(code, Detail::ReadStatus(status));
        }
        if (!run)
            return Result<Instance>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
        const Prototype prototype(
            Detail::NativeGuid(info.Prototype.Prototype),
            info.Prototype.Generation);
        Detail::Run owned(
            m_Session, run, RunKind::Instance, prototype);
        Status accepted = Accept(info, RunKind::Instance);
        if (!accepted) {
            return Result<Instance>::Failure(
                BML_ERROR_MALFORMED_MESSAGE, std::move(accepted));
        }
        return Result<Instance>::Success(
            Instance(std::move(owned)),
            Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Instance>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Instance>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Instance> Block::SpawnIn(
    const Graph &graph, FramePolicy frames) const {
    return SpawnIn(graph.m_Root, frames);
}

inline Result<Task> Call::Continue() {
    if (!m_Run)
        return Result<Task>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorRunInfo info = Detail::EmptyRunInfo();
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = Detail::WireCode(
        m_Run.m_Session->Api->Continue(
            m_Run.m_Handle, &info, &status),
        status);
    if (code != BML_OK)
        return Result<Task>::Failure(code, Detail::ReadStatus(status));
    if (!m_Run.Matches(info, RunKind::Task))
        return Result<Task>::Failure(BML_ERROR_MALFORMED_MESSAGE,
                                     Detail::ReadStatus(status));
    m_Run.m_Kind = RunKind::Task;
    return Result<Task>::Success(Task(std::move(m_Run)),
                                 Detail::ReadStatus(status));
}


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_DETAIL_INLINE_HPP
