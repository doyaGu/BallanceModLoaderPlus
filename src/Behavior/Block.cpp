#include "Behavior/Block.h"

#include <algorithm>

#include "Behavior/Core/Status.h"
#include "Behavior/Core/Value.h"

namespace BML::Behavior::Internal {
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

BlockSpec &BlockSpec::TargetValue(CKGUID type, Parameter::Binding value) {
    m_TargetMode = value.Kind() == Parameter::BindingKind::Value &&
            value.Literal().IsNull()
        ? TargetMode::ExplicitNull : TargetMode::Explicit;
    m_TargetType = type;
    m_TargetValue = std::move(value);
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

namespace {

std::string SlotLabel(const Slot &slot) {
    if (slot.RequireOnly)
        return "<only>";
    if (!slot.UsesName())
        return "#" + std::to_string(slot.Index);
    return "'" + slot.Name + "'";
}

Status Rejected(Error error, Phase phase, const Layout &layout, CKGUID type,
                std::string message) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = phase;
    status.Details.Prototype = layout.Prototype;
    status.Details.ActualType = type;
    return status;
}

// Only the first stage is checked: a later stage may address Settings that
// the SETTINGSEDITED callback of an earlier one creates.
Status CheckSetting(const Layout &layout, const BlockSpec::Binding &binding,
                    CKParameterManager *parameters) {
    const Slot &selector = binding.Target;
    const SlotInfo *found = nullptr;
    int matches = 0;
    for (const SlotInfo &slot : layout.Slots) {
        if (slot.Kind != SlotKind::Setting)
            continue;
        bool match = true;
        if (selector.UsesName()) {
            match = slot.Name == selector.Name;
            if (match && !selector.RequireUnique &&
                slot.Occurrence != selector.Occurrence)
                continue;
        } else if (!selector.RequireOnly) {
            match = slot.Index == selector.Index;
        }
        if (!match)
            continue;
        found = &slot;
        ++matches;
    }
    if (!matches) {
        return Rejected(Error::SlotNotFound, Phase::Settings, layout, CKGUID(),
                        "Setting " + SlotLabel(selector) +
                            " is absent from the declared Layout.");
    }
    if (matches != 1) {
        return Rejected(Error::AmbiguousSlot, Phase::Settings, layout, CKGUID(),
                        "Setting " + SlotLabel(selector) +
                            " is ambiguous in the declared Layout.");
    }
    if (found->ValueForm == Parameter::Form::Unsupported) {
        return Rejected(Error::ParameterTypeUnsupported, Phase::Settings,
                        layout, found->Type,
                        "The Setting parameter type has no public value form.");
    }
    const Parameter::BindingKind kind = binding.Source.Kind();
    if (kind != Parameter::BindingKind::Value &&
        kind != Parameter::BindingKind::Object)
        return {};
    if (Parameter::Describe(parameters, binding.Source.Type()).ValueForm !=
        found->ValueForm) {
        return Rejected(Error::TypeMismatch, Phase::Settings, layout,
                        found->Type,
                        "The Setting value form does not match the declared Layout.");
    }
    return {};
}

Status CheckParameterType(const Layout &layout,
                          const BlockSpec::ParameterType &parameter) {
    const Slot &selector = parameter.Target;
    const bool pin = selector.Kind == SlotKind::InputParameter;
    const char *family = pin ? "Pin " : "Pout ";
    const SlotInfo *found = nullptr;
    int matches = 0;
    for (const SlotInfo &slot : layout.Slots) {
        if (slot.Kind != selector.Kind)
            continue;
        bool match = true;
        if (selector.UsesName()) {
            match = slot.Name == selector.Name &&
                    (selector.RequireUnique ||
                     slot.Occurrence == selector.Occurrence);
        } else if (!selector.RequireOnly) {
            match = slot.Index == selector.Index;
        }
        if (!match)
            continue;
        found = &slot;
        ++matches;
    }
    if (!matches) {
        const CKDWORD created = pin
            ? (CKBEHAVIOR_VARIABLEPARAMETERINPUTS |
               CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS)
            : (CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS |
               CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS);
        if ((layout.BehaviorFlags & created) != 0)
            return {};
        return Rejected(Error::SlotNotFound, Phase::StaticLayout, layout,
                        parameter.Type,
                        family + SlotLabel(selector) +
                            " is absent from the declared Layout.");
    }
    if (matches != 1) {
        return Rejected(Error::AmbiguousSlot, Phase::StaticLayout, layout,
                        parameter.Type,
                        family + SlotLabel(selector) +
                            " is ambiguous in the declared Layout.");
    }
    if (!found->Dynamic) {
        return Rejected(Error::InterfaceUnsupported, Phase::StaticLayout,
                        layout, parameter.Type,
                        family + SlotLabel(selector) +
                            " belongs to a fixed native interface.");
    }
    return {};
}

} // namespace

Status CheckDeclared(const Layout &layout, const BlockSpec &block,
                     CKParameterManager *parameters) {
    if (!block.Settings().empty()) {
        for (const BlockSpec::Binding &setting : block.Settings().front()) {
            Status checked = CheckSetting(layout, setting, parameters);
            if (!checked)
                return checked;
        }
    }
    if (block.Targeting() != TargetMode::Owner &&
        (layout.BehaviorFlags & CKBEHAVIOR_TARGETABLE) == 0) {
        Status status{Error::TargetInvalid, CKERR_INVALIDPARAMETER,
                      CKBR_PARAMETERERROR,
                      "The Prototype does not declare an explicit Target."};
        status.Details.Stage = Phase::TargetBinding;
        status.Details.Prototype = layout.Prototype;
        status.Details.ActualType = block.TargetType();
        return status;
    }
    for (const auto *types : {&block.PinTypes(), &block.PoutTypes()}) {
        for (const BlockSpec::ParameterType &parameter : *types) {
            Status checked = CheckParameterType(layout, parameter);
            if (!checked)
                return checked;
        }
    }
    return {};
}

} // namespace BML::Behavior::Internal
