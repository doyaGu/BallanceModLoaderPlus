#include "Behavior/Parameter.h"

#include <cstring>
#include <utility>

#include "Behavior/Status.h"

namespace BML::Behavior {

Value Value::Raw(CKGUID type, const void *data, std::size_t size) {
    Value value;
    value.m_Kind = ValueKind::Raw;
    value.m_Type = type;
    if (data && size != 0) {
        value.m_Bytes.resize(size);
        std::memcpy(value.m_Bytes.data(), data, size);
    }
    return value;
}

Value Value::UntypedRaw(const void *data, std::size_t size) {
    return Raw(CKGUID(), data, size);
}

Value Value::Text(CKGUID type, std::string text) {
    Value value;
    value.m_Kind = ValueKind::Text;
    value.m_Type = type;
    value.m_Text = std::move(text);
    return value;
}

Value Value::String(std::string text) {
    return Text(CKPGUID_STRING, std::move(text));
}

Value Value::Object(CKGUID type, CKObject *object) {
    Value value;
    value.m_Kind = ValueKind::Object;
    value.m_Type = type;
    value.m_Object = object;
    value.m_ObjectId = object ? object->GetID() : 0;
    return value;
}

Value Value::Snapshot(CKParameter *source) {
    Value value;
    value.m_Kind = ValueKind::Snapshot;
    value.m_Source = source;
    value.m_SourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

Value Value::DirectSource(CKParameter *source) {
    Value value;
    value.m_Kind = ValueKind::DirectSource;
    value.m_Source = source;
    value.m_SourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

Value Value::SharedSource(CKParameterIn *source) {
    Value value;
    value.m_Kind = ValueKind::SharedSource;
    value.m_SharedSource = source;
    value.m_SharedSourceId = source ? source->GetID() : 0;
    value.m_Type = source ? source->GetGUID() : CKGUID();
    return value;
}

namespace Parameter {
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
    case ValueKind::Object: {
        CKObject *object = value.ObjectId()
            ? context->GetObject(value.ObjectId()) : nullptr;
        if (object != value.ObjectValue() ||
            (object && object->IsToBeDeleted())) {
            return {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "Object value has expired."};
        }
        const Type type = Describe(context->GetParameterManager(),
                                   parameter->GetGUID());
        if (object && type.ClassId != 0 &&
            !CKIsChildClassOf(object, type.ClassId)) {
            return {Error::TypeMismatch, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "Object value is incompatible with the parameter class."};
        }
        const CK_ID id = object ? object->GetID() : 0;
        error = parameter->SetValue(&id, sizeof(id));
        break;
    }
    case ValueKind::Snapshot: {
        CKObject *source = value.ParameterSourceId()
            ? context->GetObject(value.ParameterSourceId()) : nullptr;
        if (source != value.ParameterSource() || !source ||
            source->IsToBeDeleted() ||
            !CKIsChildClassOf(source, CKCID_PARAMETER)) {
            return {Error::SourceInvalid, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "Snapshot source is invalid."};
        }
        error = parameter->CopyValue(value.ParameterSource(), TRUE);
        break;
    }
    case ValueKind::DirectSource:
    case ValueKind::SharedSource:
        return {Error::InvalidState, CKERR_INVALIDPARAMETER,
                CKBR_PARAMETERERROR,
                "Source relations cannot be written as parameter values."};
    }
    return error == CK_OK
        ? Status{}
        : Status{Error::ValueWriteFailed, error, CKBR_PARAMETERERROR,
                 "CKParameter rejected the value."};
}

} // namespace Parameter
} // namespace BML::Behavior
