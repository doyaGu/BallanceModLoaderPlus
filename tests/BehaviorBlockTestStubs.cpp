#include "Behavior/Parameter.h"

#include <utility>

namespace BML::Behavior::Parameter {

Binding::Binding(Value value)
    : m_Kind(BindingKind::Value), m_Type(value.Type()),
      m_Value(std::move(value)) {}

Binding Binding::Object(CKGUID type, CKObject *object) {
    if (!object)
        return Value::Null(type);
    Binding binding;
    binding.m_Kind = BindingKind::Object;
    binding.m_Type = type;
    binding.m_Object = object;
    binding.m_ObjectId = 1;
    return binding;
}

Binding Binding::Copy(CKParameter *source) {
    Binding binding;
    binding.m_Kind = BindingKind::Copy;
    binding.m_Source = source;
    binding.m_SourceId = source ? 1 : 0;
    return binding;
}

Binding Binding::Direct(CKParameter *source) {
    Binding binding = Copy(source);
    binding.m_Kind = BindingKind::Direct;
    return binding;
}

Binding Binding::Shared(CKParameterIn *source) {
    Binding binding;
    binding.m_Kind = BindingKind::Shared;
    binding.m_Shared = source;
    binding.m_SharedId = source ? 1 : 0;
    return binding;
}

} // namespace BML::Behavior::Parameter
