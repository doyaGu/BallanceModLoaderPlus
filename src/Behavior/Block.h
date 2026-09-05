#ifndef BML_BEHAVIOR_BLOCK_H
#define BML_BEHAVIOR_BLOCK_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "CKAll.h"
#include "Behavior/Callback.h"
#include "Behavior/Layout.h"
#include "Behavior/Parameter.h"

namespace BML::Behavior {

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
    };

    explicit BlockSpec(CKGUID prototype = CKGUID()) : m_Prototype(prototype) {
        m_SettingStages.emplace_back();
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

private:
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

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_BLOCK_H
