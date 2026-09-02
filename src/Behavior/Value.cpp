#include "Behavior/Value.h"

#include <cstring>
#include <utility>

#include "CKAll.h"

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

Value Value::Null(CKGUID type) {
    Value value;
    value.m_Kind = ValueKind::Null;
    value.m_Type = type;
    return value;
}

} // namespace BML::Behavior
