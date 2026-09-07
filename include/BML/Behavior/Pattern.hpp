#ifndef BML_BEHAVIOR_PATTERN_HPP
#define BML_BEHAVIOR_PATTERN_HPP

#include "BML/Behavior/Prototype.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace BML::Behavior {

// A structural description of a Node in one graph scope. A Pattern contains only
// facts Virtools can observe again in another world; it never retains a live
// Node or an author predicate.
class NodePattern {
public:
    NodePattern() = default;
    NodePattern(Selector selector) : m_Selector(std::move(selector)) {}
    NodePattern(std::string_view uniqueName)
        : m_Selector(Selector::Unique(uniqueName)) {}

    NodePattern &Prototype(CKGUID prototype) {
        m_Prototype = prototype;
        return *this;
    }
    NodePattern &Kind(BehaviorKind kind) {
        m_Kind = kind;
        return *this;
    }

    NodePattern &Ins(std::int32_t count) {
        return Count(SlotKind::In, count);
    }
    NodePattern &Outs(std::int32_t count) {
        return Count(SlotKind::Out, count);
    }
    NodePattern &Pins(std::int32_t count) {
        return Count(SlotKind::Pin, count);
    }
    NodePattern &Pouts(std::int32_t count) {
        return Count(SlotKind::Pout, count);
    }
    NodePattern &Settings(std::int32_t count) {
        return Count(SlotKind::Setting, count);
    }
    NodePattern &Locals(std::int32_t count) {
        return Count(SlotKind::Local, count);
    }

    template <class T>
    NodePattern &Pin(Selector slot, T &&value) {
        return Observe(SlotKind::Pin, std::move(slot),
                       Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    NodePattern &Pin(std::int32_t index, T &&value) {
        return Pin(Selector::At(index), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Pin(std::string_view name, T &&value) {
        return Pin(Selector::Unique(name), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Pout(Selector slot, T &&value) {
        return Observe(SlotKind::Pout, std::move(slot),
                       Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    NodePattern &Pout(std::int32_t index, T &&value) {
        return Pout(Selector::At(index), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Pout(std::string_view name, T &&value) {
        return Pout(Selector::Unique(name), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Setting(Selector slot, T &&value) {
        return Observe(SlotKind::Setting, std::move(slot),
                       Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    NodePattern &Setting(std::int32_t index, T &&value) {
        return Setting(Selector::At(index), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Setting(std::string_view name, T &&value) {
        return Setting(Selector::Unique(name), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Local(Selector slot, T &&value) {
        return Observe(SlotKind::Local, std::move(slot),
                       Behavior::Value(std::forward<T>(value)));
    }
    template <class T>
    NodePattern &Local(std::int32_t index, T &&value) {
        return Local(Selector::At(index), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Local(std::string_view name, T &&value) {
        return Local(Selector::Unique(name), std::forward<T>(value));
    }
    template <class T>
    NodePattern &Target(T &&value) {
        return Observe(SlotKind::Target, Selector::Only(),
                       Behavior::Value(std::forward<T>(value)));
    }

private:
    struct PortCount {
        SlotKind Kind = SlotKind::In;
        std::int32_t Count = -1;
    };
    struct PortValue {
        SlotKind Kind = SlotKind::Pin;
        Selector Slot;
        Behavior::Value Expected;
    };

    NodePattern &Count(SlotKind kind, std::int32_t count) {
        for (PortCount &condition : m_Counts) {
            if (condition.Kind == kind) {
                condition.Count = count;
                return *this;
            }
        }
        m_Counts.push_back({kind, count});
        return *this;
    }
    NodePattern &Observe(SlotKind kind, Selector slot,
                         Behavior::Value expected) {
        m_Values.push_back(
            {kind, std::move(slot), std::move(expected)});
        return *this;
    }

    Selector m_Selector;
    CKGUID m_Prototype{0, 0};
    std::optional<BehaviorKind> m_Kind;
    std::uint64_t m_PortShape = 0;
    std::vector<PortCount> m_Counts;
    std::vector<PortValue> m_Values;

    friend class Edit;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PATTERN_HPP
