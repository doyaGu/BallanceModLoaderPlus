#ifndef BML_BEHAVIOR_PARAMETER_H
#define BML_BEHAVIOR_PARAMETER_H

#include <cstdint>
#include <string>
#include <vector>

#include "CKAll.h"
#include "Behavior/Value.h"

namespace BML::Behavior::Internal {
struct Status;
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

    friend bool operator==(const Binding &, const Binding &) = default;

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

// The source graph for Runtime-created parameters. Virtools stores raw source
// pointers but does not retain their owners, so every managed relation is
// recorded when Runtime binds or re-reads an input. Relations made outside
// this graph do not acquire ownership of a Runtime-created source.
class Sources final {
public:
    explicit Sources(CKContext *context) : m_Context(context) {}

    void Own(CKParameter *source);
    void Update(CKParameterIn *input);
    void Update(CKBehavior *behavior);
    void Remove(CKParameter *source);
    void Remove(CKParameterIn *input);
    void Remove(CKBehavior *behavior);
    void Remove(const CK_ID *ids, int count);
    [[nodiscard]] int Count(CKParameter *source);

private:
    struct Object {
        CK_ID Id = 0;
        CKObject *Address = nullptr;

        [[nodiscard]] bool operator==(const Object &other) const noexcept {
            return Id == other.Id && Address == other.Address;
        }
    };

    struct Source {
        Object Input;
        Object Owner;
        Object Value;
    };

    [[nodiscard]] Object Live(CKObject *object) const;
    [[nodiscard]] CKObject *Live(Object object) const;

    CKContext *m_Context = nullptr;
    std::vector<Object> m_Owned;
    std::vector<Source> m_Sources;
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

// Owns a Virtools value in an ordinary CKParameterLocal so registered copy,
// save/load, and destruction functions remain authoritative. The caller owns
// the returned parameter and destroys it through CKContext.
[[nodiscard]] Status Clone(CKContext *context, CKParameter *source,
                           CKParameterLocal *&out);

// Compares two values through the registered save/load representation when
// the parameter type owns non-trivial state, and through the parameter buffer
// for ordinary value types.
[[nodiscard]] Status Equal(CKContext *context, CKParameter *left,
                           CKParameter *right, bool &equal);

} // namespace Parameter
} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_PARAMETER_H
