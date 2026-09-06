#include "Behavior/Block.h"

#include <algorithm>

#include "Behavior/Value.h"

namespace BML::Behavior::Internal {
namespace {

bool SameSelector(const Slot &left, const Slot &right) {
    if (left.Kind != right.Kind || left.UsesName() != right.UsesName())
        return false;
    return left.UsesName()
        ? left.Name == right.Name && left.Occurrence == right.Occurrence
        : left.Index == right.Index;
}

} // namespace

BlockSpec &BlockSpec::TargetOwner() {
    m_TargetMode = TargetMode::Owner;
    m_TargetType = CKGUID();
    m_TargetValue = Parameter::Binding();
    return *this;
}

BlockSpec &BlockSpec::Target(CKGUID type, CKObject *object) {
    m_TargetMode = object ? TargetMode::Explicit : TargetMode::ExplicitNull;
    m_TargetType = type;
    m_TargetValue = Parameter::Binding::Object(type, object);
    return *this;
}

BlockSpec &BlockSpec::NullTarget(CKGUID type) {
    m_TargetMode = TargetMode::ExplicitNull;
    m_TargetType = type;
    m_TargetValue = Value::Null(type);
    return *this;
}

BlockSpec &BlockSpec::TargetSource(CKGUID type, CKParameter *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = Parameter::Binding::Direct(source);
    return *this;
}

BlockSpec &BlockSpec::TargetShared(CKGUID type, CKParameterIn *source) {
    m_TargetMode = TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = Parameter::Binding::Shared(source);
    return *this;
}

BlockSpec &BlockSpec::Setting(Slot slot, Parameter::Binding value) {
    slot.Kind = SlotKind::Setting;
    m_SettingStages.back().push_back({std::move(slot), std::move(value)});
    return *this;
}

BlockSpec &BlockSpec::NextSettingStage() {
    if (!m_SettingStages.back().empty())
        m_SettingStages.emplace_back();
    return *this;
}

BlockSpec &BlockSpec::Input(Slot slot, Parameter::Binding value) {
    slot.Kind = SlotKind::InputParameter;
    for (Binding &binding : m_Inputs) {
        if (SameSelector(binding.Target, slot)) {
            binding = {std::move(slot), std::move(value)};
            return *this;
        }
    }
    m_Inputs.push_back({std::move(slot), std::move(value)});
    return *this;
}

BlockSpec &BlockSpec::Local(Slot slot, Parameter::Binding value) {
    slot.Kind = SlotKind::Local;
    for (Binding &binding : m_Locals) {
        if (SameSelector(binding.Target, slot)) {
            binding = {std::move(slot), std::move(value)};
            return *this;
        }
    }
    m_Locals.push_back({std::move(slot), std::move(value)});
    return *this;
}

BlockSpec &BlockSpec::AddInput(std::string name) {
    m_AddedInputs.push_back(std::move(name));
    return *this;
}

BlockSpec &BlockSpec::AddOutput(std::string name) {
    m_AddedOutputs.push_back(std::move(name));
    return *this;
}

BlockSpec &BlockSpec::KeepAlive(std::shared_ptr<CallbackResource> resource) {
    if (resource)
        m_KeepAlive.push_back(std::move(resource));
    return *this;
}

bool BlockSpec::WorldBound() const noexcept {
    const auto bound = [](const Parameter::Binding &binding) {
        switch (binding.Kind()) {
        case Parameter::BindingKind::Value:
            return false;
        case Parameter::BindingKind::Object:
            return binding.ObjectValue() != nullptr;
        case Parameter::BindingKind::Copy:
        case Parameter::BindingKind::Direct:
        case Parameter::BindingKind::Shared:
            return true;
        }
        return true;
    };
    if (m_TargetMode == TargetMode::Explicit && bound(m_TargetValue))
        return true;
    for (const auto &stage : m_SettingStages) {
        for (const Binding &binding : stage) {
            if (bound(binding.Source))
                return true;
        }
    }
    for (const Binding &binding : m_Inputs) {
        if (bound(binding.Source))
            return true;
    }
    for (const Binding &binding : m_Locals) {
        if (bound(binding.Source))
            return true;
    }
    return false;
}

} // namespace BML::Behavior::Internal
