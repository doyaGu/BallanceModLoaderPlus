#ifndef BML_BEHAVIOR_PARAMETER_H
#define BML_BEHAVIOR_PARAMETER_H

#include <cstdint>
#include <string>

#include "CKAll.h"
#include "Behavior/Value.h"

namespace BML::Behavior {
struct Status;
}

namespace BML::Behavior {
namespace Parameter {

enum class BindingKind : std::uint32_t {
    Value,
    Object,
    Copy,
    Direct,
    Shared,
};

// A live Virtools parameter binding. Owned literal data remains in Value;
// CK objects and parameter relations are world-bound and retain both their
// object id and address until Runtime validates them against the current world.
class Binding {
public:
    Binding() = default;
    Binding(Value value);

    static Binding Object(CKGUID type, CKObject *object);
    static Binding Copy(CKParameter *source);
    static Binding Direct(CKParameter *source);
    static Binding Shared(CKParameterIn *source);

    [[nodiscard]] BindingKind Kind() const noexcept { return m_Kind; }
    [[nodiscard]] CKGUID Type() const noexcept { return m_Type; }
    [[nodiscard]] const Value &Literal() const noexcept { return m_Value; }
    [[nodiscard]] CKObject *ObjectValue() const noexcept { return m_Object; }
    [[nodiscard]] CK_ID ObjectId() const noexcept { return m_ObjectId; }
    [[nodiscard]] CKParameter *Source() const noexcept { return m_Source; }
    [[nodiscard]] CK_ID SourceId() const noexcept { return m_SourceId; }
    [[nodiscard]] CKParameterIn *SharedSource() const noexcept {
        return m_Shared;
    }
    [[nodiscard]] CK_ID SharedSourceId() const noexcept {
        return m_SharedId;
    }

private:
    BindingKind m_Kind = BindingKind::Value;
    CKGUID m_Type = CKGUID();
    Value m_Value;
    CKObject *m_Object = nullptr;
    CK_ID m_ObjectId = 0;
    CKParameter *m_Source = nullptr;
    CK_ID m_SourceId = 0;
    CKParameterIn *m_Shared = nullptr;
    CK_ID m_SharedId = 0;
};

enum class Form : std::uint32_t {
    Unsupported,
    Bool,
    Int32,
    Float32,
    Utf8,
    Vec2,
    Vec3,
    Quaternion,
    Euler,
    Rect,
    Color,
    Box,
    Mat4,
    Object,
};

struct Type {
    CKGUID Guid = CKGUID();
    CKGUID DerivedFrom = CKGUID();
    std::string Name;
    CK_CLASSID ClassId = 0;
    int Size = 0;
    Form ValueForm = Form::Unsupported;
    bool Valid = false;
    bool ProviderOwned = false;

    [[nodiscard]] bool Supported() const noexcept {
        return ValueForm != Form::Unsupported;
    }
    [[nodiscard]] bool ObjectDerived() const noexcept {
        return ValueForm == Form::Object;
    }
};

// Reads and copies the current Virtools type description. No pointer returned
// by CKParameterManager survives this call.
[[nodiscard]] Type Describe(CKParameterManager *manager, CKGUID type);

// Virtools owns type compatibility. This helper keeps all Behavior callers on
// the same argument order and handles an unavailable manager consistently.
[[nodiscard]] bool Compatible(CKParameterManager *manager, CKGUID destination,
                              CKGUID source) noexcept;

// Writes one owned Value through Virtools' registered parameter semantics.
// Source relations are graph structure and are deliberately not accepted.
[[nodiscard]] Status Write(CKContext *context, CKParameter *parameter,
                           const Value &value);
[[nodiscard]] Status Write(CKContext *context, CKParameter *parameter,
                           const Binding &binding);

} // namespace Parameter
} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PARAMETER_H
