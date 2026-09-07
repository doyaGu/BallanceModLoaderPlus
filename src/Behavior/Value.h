#ifndef BML_BEHAVIOR_VALUE_H
#define BML_BEHAVIOR_VALUE_H

#include <cstddef>
#include <string>
#include <vector>

#include "CKTypes.h"

namespace BML::Behavior::Internal {

enum class ValueKind {
    Raw,
    Text,
    Null,
};

class Value {
public:
    static Value Raw(CKGUID type, const void *data, std::size_t size);
    static Value UntypedRaw(const void *data, std::size_t size);
    static Value Text(CKGUID type, std::string text);
    static Value String(std::string text);
    static Value Null(CKGUID type);

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
    [[nodiscard]] bool IsNull() const noexcept {
        return m_Kind == ValueKind::Null;
    }

    friend bool operator==(const Value &, const Value &) = default;

private:
    ValueKind m_Kind = ValueKind::Raw;
    CKGUID m_Type = CKGUID();
    std::vector<std::byte> m_Bytes;
    std::string m_Text;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_VALUE_H
