#include "Behavior/Engine/Pout.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <string>

#include "Behavior/Core/Parameter.h"

namespace BML::Behavior::Internal::Engine {
namespace {

const char *SafeName(CKObject *object) {
    return object && object->GetName() ? object->GetName() : "<unnamed>";
}

bool Fail(int nativeCode, std::string message, ExecutionFault &fault) {
    fault = {ExecutionError::PoutReadFailed, nativeCode, std::move(message)};
    return false;
}

bool Fail(CKParameterOut *parameter, const char *problem,
          ExecutionFault &fault, int nativeCode = CKBR_PARAMETERERROR) {
    return Fail(nativeCode,
                std::string("Pout '") + SafeName(parameter) + "' " + problem,
                fault);
}

bool ReadText(CKParameterOut *parameter, Pout &value, ExecutionFault &fault) {
    const int size = parameter->GetStringValue(nullptr, FALSE);
    if (size < 0)
        return Fail(parameter, "could not be read.", fault, size);
    value.Text.resize(static_cast<std::size_t>(size) + 1u, '\0');
    if (size > 0 && parameter->GetStringValue(value.Text.data(), FALSE) < 0)
        return Fail(parameter, "changed while it was being read.", fault);
    value.Text.resize(std::char_traits<char>::length(value.Text.c_str()));
    return true;
}

bool ReadObject(CKParameterOut *parameter,
                const std::function<ObjectRef(const void *)> &issue,
                Pout &value, ExecutionFault &fault) {
    CKObject *object = parameter->GetValueObject(FALSE);
    if (!object)
        return true;
    if (!issue) {
        return Fail(CKBR_PARAMETERERROR,
                    "An object Pout cannot be retained without ObjectRefs.",
                    fault);
    }
    const ObjectRef reference = issue(object);
    if (reference.IsNull())
        return Fail(CKBR_PARAMETERERROR, "ObjectRefs rejected an object Pout.",
                    fault);
    value.ObjectDomain = reference.Domain;
    value.ObjectSlot = reference.Slot;
    value.ObjectGeneration = reference.Generation;
    return true;
}

bool ReadObjects(CKContext *context, CKParameterOut *parameter,
                 const std::function<ObjectRef(const void *)> &issue,
                 Pout &value, ExecutionFault &fault) {
    const void *storage = parameter->GetReadDataPtr(FALSE);
    XObjectArray *objects = storage
        ? *static_cast<XObjectArray *const *>(storage) : nullptr;
    if (!objects)
        return Fail(parameter, "has no Object List value.", fault);
    if (!issue) {
        return Fail(CKBR_PARAMETERERROR,
                    "An Object List Pout cannot be retained without ObjectRefs.",
                    fault);
    }
    const int count = objects->Size();
    if (count < 0)
        return Fail(parameter, "has an invalid Object List size.", fault);
    value.Objects.reserve(static_cast<std::size_t>(count));
    for (int item = 0; item < count; ++item) {
        const CK_ID id = (*objects)[item];
        if (id == 0) {
            value.Objects.push_back({});
            continue;
        }
        CKObject *object = context ? context->GetObject(id) : nullptr;
        const ObjectRef reference = object ? issue(object) : ObjectRef{};
        if (reference.IsNull()) {
            return Fail(parameter,
                        "contains an object that cannot be retained.", fault);
        }
        value.Objects.push_back(reference);
    }
    return true;
}

template <typename Native>
Native Load(const std::array<std::byte, sizeof(Pout::Components)> &bytes) {
    Native native;
    std::memcpy(&native, bytes.data(), sizeof(native));
    return native;
}

bool ReadNumeric(CKParameterOut *parameter, std::size_t size, Pout &value,
                 ExecutionFault &fault) {
    const int actual = parameter->GetDataSize();
    if (actual < 0 || static_cast<std::size_t>(actual) != size ||
        size > sizeof(value.Components))
        return Fail(parameter, "has an invalid value size.", fault);
    std::array<std::byte, sizeof(Pout::Components)> bytes{};
    const CKERROR error = parameter->GetValue(bytes.data(), FALSE);
    if (error != CK_OK)
        return Fail(parameter, "could not be read.", fault, error);

    auto components = [&value](std::initializer_list<float> items) {
        std::copy(items.begin(), items.end(), value.Components.begin());
        value.ComponentCount = static_cast<std::uint32_t>(items.size());
    };
    switch (value.Kind) {
    case PoutKind::Bool:
        value.Int32 = Load<std::int32_t>(bytes) != 0 ? 1 : 0;
        break;
    case PoutKind::Int32:
        value.Int32 = Load<std::int32_t>(bytes);
        break;
    case PoutKind::Float32:
        value.Float32 = Load<float>(bytes);
        break;
    case PoutKind::Vec2: {
        const auto native = Load<Vx2DVector>(bytes);
        components({native.x, native.y});
        break;
    }
    case PoutKind::Vec3: {
        const auto native = Load<VxVector>(bytes);
        components({native.x, native.y, native.z});
        break;
    }
    case PoutKind::Quaternion: {
        const auto native = Load<VxQuaternion>(bytes);
        components({native.x, native.y, native.z, native.w});
        break;
    }
    case PoutKind::Euler: {
        const auto native = Load<std::array<float, 3>>(bytes);
        components({native[0], native[1], native[2]});
        break;
    }
    case PoutKind::Rect: {
        const auto native = Load<VxRect>(bytes);
        components({native.left, native.top, native.right, native.bottom});
        break;
    }
    case PoutKind::Color: {
        const auto native = Load<VxColor>(bytes);
        components({native.r, native.g, native.b, native.a});
        break;
    }
    case PoutKind::Box: {
        const auto native = Load<VxBbox>(bytes);
        components({native.Min.x, native.Min.y, native.Min.z,
                    native.Max.x, native.Max.y, native.Max.z});
        break;
    }
    case PoutKind::Mat4: {
        auto native = Load<VxMatrix>(bytes);
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column)
                value.Components[static_cast<std::size_t>(row * 4 + column)] =
                    native[row][column];
        }
        value.ComponentCount = 16;
        break;
    }
    default:
        return Fail(parameter, "has an invalid numeric value form.", fault);
    }
    return true;
}

} // namespace

bool PoutForm(CKContext *context, CKParameter *parameter, PoutKind &kind,
              std::size_t &size) {
    if (!parameter)
        return false;
    size = 0;
    if (parameter->GetGUID() == CKPGUID_OBJECTARRAY) {
        kind = PoutKind::ObjectList;
        return true;
    }
    CKParameterManager *manager =
        context ? context->GetParameterManager() : nullptr;
    switch (Parameter::Describe(manager, parameter->GetGUID()).ValueForm) {
    case Parameter::Form::Object:
        kind = PoutKind::Object;
        break;
    case Parameter::Form::Bool:
        kind = PoutKind::Bool;
        size = sizeof(CKBOOL);
        break;
    case Parameter::Form::Int32:
        kind = PoutKind::Int32;
        size = sizeof(int);
        break;
    case Parameter::Form::Float32:
        kind = PoutKind::Float32;
        size = sizeof(float);
        break;
    case Parameter::Form::Utf8:
        kind = PoutKind::Utf8;
        break;
    case Parameter::Form::Vec2:
        kind = PoutKind::Vec2;
        size = sizeof(Vx2DVector);
        break;
    case Parameter::Form::Vec3:
        kind = PoutKind::Vec3;
        size = sizeof(VxVector);
        break;
    case Parameter::Form::Quaternion:
        kind = PoutKind::Quaternion;
        size = sizeof(VxQuaternion);
        break;
    case Parameter::Form::Euler:
        kind = PoutKind::Euler;
        size = sizeof(float) * 3;
        break;
    case Parameter::Form::Rect:
        kind = PoutKind::Rect;
        size = sizeof(VxRect);
        break;
    case Parameter::Form::Color:
        kind = PoutKind::Color;
        size = sizeof(VxColor);
        break;
    case Parameter::Form::Box:
        kind = PoutKind::Box;
        size = sizeof(VxBbox);
        break;
    case Parameter::Form::Mat4:
        kind = PoutKind::Mat4;
        size = sizeof(VxMatrix);
        break;
    case Parameter::Form::Unsupported:
        return false;
    }
    return true;
}

bool ReadPout(CKContext *context, CKParameterOut *parameter, std::size_t size,
              const std::function<ObjectRef(const void *)> &issue,
              Pout &value, ExecutionFault &fault) {
    switch (value.Kind) {
    case PoutKind::Utf8:
        return ReadText(parameter, value, fault);
    case PoutKind::Object:
        return ReadObject(parameter, issue, value, fault);
    case PoutKind::ObjectList:
        return ReadObjects(context, parameter, issue, value, fault);
    default:
        return ReadNumeric(parameter, size, value, fault);
    }
}

} // namespace BML::Behavior::Internal::Engine
