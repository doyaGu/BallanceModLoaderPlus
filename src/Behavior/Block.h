#ifndef BML_BEHAVIOR_BLOCK_H
#define BML_BEHAVIOR_BLOCK_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "CKAll.h"
#include "BML/Behavior/Detail/BlockAccess.hpp"
#include "Behavior/Callback.h"
#include "Behavior/Layout.h"
#include "Behavior/Parameter.h"

namespace BML::Behavior::Internal {

enum class TargetMode {
    Owner,
    Explicit,
    ExplicitNull,
};

// The complete native configuration of one Block. Execution retention and
// graph relations belong to the Run or Edit that uses the Block, not here.
class BlockSpec {
public:
    struct Binding {
        Slot Target;
        Parameter::Binding Source;

        friend bool operator==(const Binding &, const Binding &) = default;
    };

    explicit BlockSpec(CKGUID prototype = CKGUID()) : m_Prototype(prototype) {
        m_SettingStages.emplace_back();
    }

    template <class Options>
    [[nodiscard]] static BlockSpec From(const Options &options) {
        BlockSpec block(Options::Prototype());
        BML::Behavior::Detail::BlockAccess::Configure(options, block);
        return block;
    }

    BlockSpec &TargetOwner();
    BlockSpec &Target(CKGUID type, CKObject *object);
    BlockSpec &NullTarget(CKGUID type);
    BlockSpec &TargetSource(CKGUID type, CKParameter *source);
    BlockSpec &TargetShared(CKGUID type, CKParameterIn *source);
    BlockSpec &Setting(Slot slot, Parameter::Binding value);
    BlockSpec &NextSettingStage();
    BlockSpec &Input(Slot slot, Parameter::Binding value);
    BlockSpec &Local(Slot slot, Parameter::Binding value);
    BlockSpec &AddInput(std::string name);
    BlockSpec &AddOutput(std::string name);
    BlockSpec &KeepAlive(std::shared_ptr<CallbackResource> resource);

    template <class T>
    BlockSpec &Pin(std::string_view name, CKGUID type, T &&value) {
        return Input(Slot::Named(SlotKind::InputParameter,
                                 std::string(name), type),
                     Literal(type, std::forward<T>(value)));
    }
    template <class T>
    BlockSpec &Pin(int index, CKGUID type, T &&value) {
        return Input(Slot::At(SlotKind::InputParameter, index, type),
                     Literal(type, std::forward<T>(value)));
    }
    BlockSpec &ObjectPin(std::string_view name, CKGUID type,
                         CKObject *object) {
        return Input(Slot::Named(SlotKind::InputParameter,
                                 std::string(name), type),
                     Parameter::Binding::Object(type, object));
    }
    BlockSpec &ObjectPin(int index, CKGUID type, CKObject *object) {
        return Input(Slot::At(SlotKind::InputParameter, index, type),
                     Parameter::Binding::Object(type, object));
    }
    template <class T>
    BlockSpec &Setting(std::string_view name, CKGUID type, T &&value) {
        return Setting(Slot::Named(SlotKind::Setting,
                                   std::string(name), type),
                       Literal(type, std::forward<T>(value)));
    }
    template <class T>
    BlockSpec &Setting(int index, CKGUID type, T &&value) {
        return Setting(Slot::At(SlotKind::Setting, index, type),
                       Literal(type, std::forward<T>(value)));
    }
    BlockSpec &NextStage() { return NextSettingStage(); }

    BlockSpec &PrototypeGeneration(std::uint64_t generation) noexcept {
        m_PrototypeGeneration = generation;
        return *this;
    }

    [[nodiscard]] CKGUID Prototype() const noexcept { return m_Prototype; }
    [[nodiscard]] std::uint64_t PrototypeGeneration() const noexcept {
        return m_PrototypeGeneration;
    }
    // A durable Plan may retain owned literals, but not pointers into one CK
    // world.
    [[nodiscard]] bool WorldBound() const noexcept;
    [[nodiscard]] const std::vector<std::vector<Binding>> &Settings() const
        noexcept { return m_SettingStages; }
    [[nodiscard]] const std::vector<Binding> &Pins() const noexcept {
        return m_Inputs;
    }
    [[nodiscard]] const std::vector<Binding> &Locals() const noexcept {
        return m_Locals;
    }

    friend bool operator==(const BlockSpec &, const BlockSpec &) = default;

private:
    template <class T>
    static Parameter::Binding Literal(CKGUID type, T &&value) {
        using Source = std::remove_cv_t<std::remove_reference_t<T>>;
        if constexpr (std::is_same_v<Source, std::string> ||
                      std::is_same_v<Source, std::string_view>) {
            return Value::Text(type, std::string(value));
        } else if constexpr (std::is_same_v<Source, const char *> ||
                             std::is_same_v<Source, char *>) {
            return Value::Text(type, value ? std::string(value) : std::string());
        } else if constexpr (std::is_same_v<Source, bool>) {
            const CKBOOL native = value ? TRUE : FALSE;
            return Value::From(type, native);
        } else {
            return Value::From(type, value);
        }
    }

    CKGUID m_Prototype = CKGUID();
    std::uint64_t m_PrototypeGeneration = 0;
    TargetMode m_TargetMode = TargetMode::Owner;
    CKGUID m_TargetType = CKGUID();
    Parameter::Binding m_TargetValue;
    std::vector<std::vector<Binding>> m_SettingStages;
    std::vector<Binding> m_Inputs;
    std::vector<Binding> m_Locals;
    std::vector<std::string> m_AddedInputs;
    std::vector<std::string> m_AddedOutputs;
    std::vector<std::shared_ptr<CallbackResource>> m_KeepAlive;

    friend class Runtime;
    friend class Edit;
    friend class CKEdit;
};

} // namespace BML::Behavior::Internal
#endif // BML_BEHAVIOR_BLOCK_H
