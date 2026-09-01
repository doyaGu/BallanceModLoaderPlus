#ifndef BML_BEHAVIOR_PARAMETER_H
#define BML_BEHAVIOR_PARAMETER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "CKAll.h"

namespace BML::Behavior {
struct Status;
}

namespace BML::Behavior {

enum class ValueKind {
    Raw,
    Text,
    Object,
    Snapshot,
    DirectSource,
    SharedSource,
};

class Value {
public:
    static Value Raw(CKGUID type, const void *data, std::size_t size);
    static Value UntypedRaw(const void *data, std::size_t size);
    static Value Text(CKGUID type, std::string text);
    static Value String(std::string text);
    static Value Object(CKGUID type, CKObject *object);
    static Value Snapshot(CKParameter *source);
    static Value DirectSource(CKParameter *source);
    static Value SharedSource(CKParameterIn *source);

    template <typename T>
    static Value From(CKGUID type, const T &value) {
        return Raw(type, &value, sizeof(T));
    }

    [[nodiscard]] ValueKind Kind() const noexcept { return m_Kind; }
    [[nodiscard]] CKGUID Type() const noexcept { return m_Type; }
    [[nodiscard]] const std::vector<std::byte> &Bytes() const noexcept {
        return m_Bytes;
    }
    [[nodiscard]] const std::string &StringValue() const noexcept {
        return m_Text;
    }
    [[nodiscard]] CKObject *ObjectValue() const noexcept { return m_Object; }
    [[nodiscard]] CK_ID ObjectId() const noexcept { return m_ObjectId; }
    [[nodiscard]] CKParameter *ParameterSource() const noexcept {
        return m_Source;
    }
    [[nodiscard]] CK_ID ParameterSourceId() const noexcept {
        return m_SourceId;
    }
    [[nodiscard]] CKParameterIn *SharedParameterSource() const noexcept {
        return m_SharedSource;
    }
    [[nodiscard]] CK_ID SharedParameterSourceId() const noexcept {
        return m_SharedSourceId;
    }

private:
    ValueKind m_Kind = ValueKind::Raw;
    CKGUID m_Type = CKGUID();
    std::vector<std::byte> m_Bytes;
    std::string m_Text;
    CKObject *m_Object = nullptr;
    CK_ID m_ObjectId = 0;
    CKParameter *m_Source = nullptr;
    CK_ID m_SourceId = 0;
    CKParameterIn *m_SharedSource = nullptr;
    CK_ID m_SharedSourceId = 0;
};

namespace Parameter {

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

} // namespace Parameter
} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PARAMETER_H
