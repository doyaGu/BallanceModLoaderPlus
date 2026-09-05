#ifndef BML_BEHAVIOR_PROTOTYPE_HPP
#define BML_BEHAVIOR_PROTOTYPE_HPP

#include "BML/Behavior/Value.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace BML::Behavior {

struct Prototype {
    CKGUID Id{0, 0};
    std::uint64_t Generation = 0;

    Prototype() = default;
    Prototype(CKGUID id, std::uint64_t generation = 0)
        : Id(id), Generation(generation) {}
};

// Filters the Prototype registrations visible in the running Player. An empty
// query returns every registration. optional distinguishes an absent filter
// from an intentionally empty text value.
struct PrototypeQuery {
    std::optional<CKGUID> Id;
    std::optional<std::string> Name;
    std::optional<std::string> Category;
    std::optional<std::string> Provider;
    std::optional<CKGUID> ProviderId;
    std::optional<std::int32_t> CompatibleClass;
    std::vector<CKGUID> RequiredManagers;
};

enum class LayoutOrigin : std::uint32_t {
    Declared = BML_BEHAVIOR_LAYOUT_DECLARED,
    Live = BML_BEHAVIOR_LAYOUT_LIVE,
};

enum class BehaviorKind : std::uint32_t {
    Function = BML_BEHAVIOR_KIND_FUNCTION,
    Callback = BML_BEHAVIOR_KIND_CALLBACK,
    Graph = BML_BEHAVIOR_KIND_GRAPH,
};

enum class SlotKind : std::uint32_t {
    In = BML_BEHAVIOR_SLOT_IN,
    Out = BML_BEHAVIOR_SLOT_OUT,
    Pin = BML_BEHAVIOR_SLOT_PIN,
    Pout = BML_BEHAVIOR_SLOT_POUT,
    Setting = BML_BEHAVIOR_SLOT_SETTING,
    Local = BML_BEHAVIOR_SLOT_LOCAL,
    Target = BML_BEHAVIOR_SLOT_TARGET,
};

struct Manager {
    CKGUID Id{0, 0};
    bool Available = false;
};

struct PrototypeInfo {
    Prototype Ref;
    CKGUID Provider{0, 0};
    std::uint32_t Version = 0;
    std::int32_t CompatibleClass = 0;
    std::string Name;
    std::string Category;
    std::string ProviderName;
    std::string Author;
    std::string Description;
    std::vector<Manager> Managers;
};

struct Slot {
    SlotKind Kind = SlotKind::Pin;
    std::uint64_t Generation = 0;
    bool Dynamic = false;
    std::int32_t Index = 0;
    std::int32_t Occurrence = 0;
    CKGUID Type{0, 0};
    std::optional<ValueKind> Value;
    std::string Name;
    std::string TypeName;
};

struct Layout {
    LayoutOrigin Origin = LayoutOrigin::Declared;
    Prototype PrototypeRef;
    std::uint64_t Generation = 0;
    BehaviorKind Kind = BehaviorKind::Function;
    std::int32_t CompatibleClass = 0;
    std::uint32_t PrototypeFlags = 0;
    std::uint32_t BehaviorFlags = 0;
    CKGUID TargetType{0, 0};
    std::string Name;
    std::string Category;
    std::string Provider;
    std::string Author;
    std::string Description;
    std::vector<Manager> Managers;
    std::vector<Slot> Slots;

    [[nodiscard]] const Slot *Find(
        SlotKind kind, std::string_view name) const noexcept {
        const Slot *found = nullptr;
        for (const Slot &slot : Slots) {
            if (slot.Kind != kind || slot.Name != name)
                continue;
            if (found)
                return nullptr;
            found = &slot;
        }
        return found;
    }
    [[nodiscard]] const Slot *Find(
        SlotKind kind, std::string_view name,
        std::int32_t occurrence) const noexcept {
        const auto found = std::find_if(
            Slots.begin(), Slots.end(), [&](const Slot &slot) {
                return slot.Kind == kind && slot.Name == name &&
                       slot.Occurrence == occurrence;
            });
        return found == Slots.end() ? nullptr : &*found;
    }
};


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PROTOTYPE_HPP
