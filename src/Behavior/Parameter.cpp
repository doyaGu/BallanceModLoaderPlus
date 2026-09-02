#include "Behavior/Parameter.h"

#include <utility>

#include "Behavior/Status.h"

namespace BML::Behavior {

namespace Parameter {

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
    binding.m_ObjectId = object->GetID();
    return binding;
}

Binding Binding::Copy(CKParameter *source) {
    Binding binding;
    binding.m_Kind = BindingKind::Copy;
    binding.m_Type = source ? source->GetGUID() : CKGUID();
    binding.m_Source = source;
    binding.m_SourceId = source ? source->GetID() : 0;
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
    binding.m_Type = source ? source->GetGUID() : CKGUID();
    binding.m_Shared = source;
    binding.m_SharedId = source ? source->GetID() : 0;
    return binding;
}

namespace {

bool HasProviderRepresentation(const CKParameterTypeDesc &type) noexcept {
    return type.CreateDefaultFunction || type.DeleteFunction ||
           type.CopyFunction || type.SaveLoadFunction || type.CheckFunction;
}

bool Derived(CKParameterManager *manager, CKGUID type, CKGUID base) noexcept {
    return type == base ||
           (manager && manager->IsDerivedFrom(type, base) != FALSE);
}

bool PlainDerived(CKParameterManager *manager, CKGUID type, CKGUID base,
                  int size, const CKParameterTypeDesc &description) noexcept {
    if (type == base)
        return true;
    return Derived(manager, type, base) && description.DefaultSize == size &&
           !HasProviderRepresentation(description);
}

} // namespace

Type Describe(CKParameterManager *manager, CKGUID type) {
    Type result;
    result.Guid = type;
    if (!manager || !type.IsValid())
        return result;

    CKParameterTypeDesc *description =
        manager->GetParameterTypeDescription(type);
    if (!description || !description->Valid)
        return result;

    result.DerivedFrom = description->DerivedFrom;
    result.Name = description->TypeName.Str();
    result.ClassId = static_cast<CK_CLASSID>(description->Cid);
    result.Size = description->DefaultSize;
    result.Valid = true;
    result.ProviderOwned = HasProviderRepresentation(*description);

    if (Derived(manager, type, CKPGUID_OBJECT))
        result.ValueForm = Form::Object;
    else if (PlainDerived(manager, type, CKPGUID_BOOL, sizeof(CKBOOL), *description))
        result.ValueForm = Form::Bool;
    else if (PlainDerived(manager, type, CKPGUID_INT, sizeof(std::int32_t), *description))
        result.ValueForm = Form::Int32;
    else if (PlainDerived(manager, type, CKPGUID_FLOAT, sizeof(float), *description))
        result.ValueForm = Form::Float32;
    else if (Derived(manager, type, CKPGUID_STRING))
        result.ValueForm = Form::Utf8;
    else if (PlainDerived(manager, type, CKPGUID_2DVECTOR, sizeof(Vx2DVector), *description))
        result.ValueForm = Form::Vec2;
    else if (PlainDerived(manager, type, CKPGUID_VECTOR, sizeof(VxVector), *description))
        result.ValueForm = Form::Vec3;
    else if (PlainDerived(manager, type, CKPGUID_QUATERNION, sizeof(VxQuaternion), *description))
        result.ValueForm = Form::Quaternion;
    else if (PlainDerived(manager, type, CKPGUID_EULERANGLES, sizeof(float) * 3, *description))
        result.ValueForm = Form::Euler;
    else if (PlainDerived(manager, type, CKPGUID_RECT, sizeof(VxRect), *description))
        result.ValueForm = Form::Rect;
    else if (PlainDerived(manager, type, CKPGUID_COLOR, sizeof(VxColor), *description))
        result.ValueForm = Form::Color;
    else if (PlainDerived(manager, type, CKPGUID_BOX, sizeof(VxBbox), *description))
        result.ValueForm = Form::Box;
    else if (PlainDerived(manager, type, CKPGUID_MATRIX, sizeof(VxMatrix), *description))
        result.ValueForm = Form::Mat4;
    return result;
}

bool Compatible(CKParameterManager *manager, CKGUID destination,
                CKGUID source) noexcept {
    return manager && destination.IsValid() && source.IsValid() &&
           manager->IsTypeCompatible(destination, source) != FALSE;
}

Status Write(CKContext *context, CKParameter *parameter,
             const Value &value) {
    if (!context || !parameter)
        return {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "Parameter is not writable."};

    if (value.Type().IsValid() &&
        !Compatible(context->GetParameterManager(), parameter->GetGUID(),
                    value.Type())) {
        return {Error::TypeMismatch, CKERR_INVALIDPARAMETER,
                CKBR_PARAMETERERROR,
                "Parameter value type is incompatible with the slot type."};
    }

    CKERROR error = CK_OK;
    switch (value.Kind()) {
    case ValueKind::Raw:
        if (value.Bytes().empty())
            return {Error::ValueWriteFailed, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "Raw parameter value is empty."};
        if (parameter->GetDataSize() !=
            static_cast<int>(value.Bytes().size())) {
            return {Error::TypeMismatch, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "Raw value size does not match the registered parameter type."};
        }
        error = parameter->SetValue(
            value.Bytes().data(), static_cast<int>(value.Bytes().size()));
        break;
    case ValueKind::Text:
        error = parameter->SetStringValue(
            const_cast<CKSTRING>(value.StringValue().c_str()));
        break;
    case ValueKind::Null: {
        const Type type = Describe(context->GetParameterManager(),
                                   parameter->GetGUID());
        if (!type.ObjectDerived()) {
            return {Error::TypeMismatch, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "A null Value requires an object-derived parameter type."};
        }
        const CK_ID id = 0;
        error = parameter->SetValue(&id, sizeof(id));
        break;
    }
    }
    return error == CK_OK
        ? Status{}
        : Status{Error::ValueWriteFailed, error, CKBR_PARAMETERERROR,
                 "CKParameter rejected the value."};
}

Status Write(CKContext *context, CKParameter *parameter,
             const Binding &binding) {
    if (binding.Kind() == BindingKind::Value)
        return Write(context, parameter, binding.Literal());
    if (!context || !parameter)
        return {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "Parameter is not writable."};

    CKERROR error = CK_OK;
    if (binding.Kind() == BindingKind::Object) {
        CKObject *object = binding.ObjectId()
            ? context->GetObject(binding.ObjectId()) : nullptr;
        if (object != binding.ObjectValue() || !object ||
            object->IsToBeDeleted()) {
            return {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "Object binding has expired."};
        }
        if (binding.Type().IsValid() &&
            !Compatible(context->GetParameterManager(), parameter->GetGUID(),
                        binding.Type())) {
            return {Error::TypeMismatch, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "Object binding type is incompatible with the slot type."};
        }
        const Type type = Describe(context->GetParameterManager(),
                                   parameter->GetGUID());
        if (type.ClassId != 0 && !CKIsChildClassOf(object, type.ClassId)) {
            return {Error::TypeMismatch, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "Object binding is incompatible with the parameter class."};
        }
        const CK_ID id = object->GetID();
        error = parameter->SetValue(&id, sizeof(id));
    } else if (binding.Kind() == BindingKind::Copy) {
        CKObject *source = binding.SourceId()
            ? context->GetObject(binding.SourceId()) : nullptr;
        if (source != binding.Source() || !source ||
            source->IsToBeDeleted() ||
            !CKIsChildClassOf(source, CKCID_PARAMETER)) {
            return {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "Parameter copy source is invalid."};
        }
        error = parameter->CopyValue(binding.Source(), TRUE);
    } else {
        return {Error::InvalidState, CKERR_INVALIDPARAMETER,
                CKBR_PARAMETERERROR,
                "Direct and shared sources require an input parameter."};
    }
    return error == CK_OK
        ? Status{}
        : Status{Error::ValueWriteFailed, error, CKBR_PARAMETERERROR,
                 "CKParameter rejected the binding."};
}

} // namespace Parameter
} // namespace BML::Behavior
