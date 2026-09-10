#include "ScriptImcRecord.h"

#include <new>
#include <utility>

#include "BML/ImcWire.hpp"
#include "CKAngelScriptAdapter.h"
#include "Loader/ModContext.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"

namespace BML {
namespace {

const CKAngelScriptAdapter::Api *CurrentArrayApi() {
    ScriptMod *mod = ScriptModRuntime::GetCurrentScriptMod();
    return mod ? &mod->GetRuntimeForFacade().GetApi() : nullptr;
}

template <class Value, class ScriptValue = Value>
int CopyFromScriptArray(const void *array, std::vector<Value> &out) {
    const CKAngelScriptAdapter::Api *api = CurrentArrayApi();
    if (!array || !api || !api->ArrayGetSize || !api->ArrayGetConstElementAddress)
        return BML_ERROR_INVALID_PARAMETER;

    CKDWORD size = 0;
    if (api->ArrayGetSize(const_cast<void *>(array), &size) != CKAS_OK)
        return BML_ERROR_INVALID_PARAMETER;
    try {
        std::vector<Value> values;
        values.reserve(size);
        for (CKDWORD index = 0; index < size; ++index) {
            const void *address = nullptr;
            if (api->ArrayGetConstElementAddress(array, index, &address) != CKAS_OK || !address)
                return BML_ERROR_INVALID_PARAMETER;
            values.push_back(static_cast<Value>(*static_cast<const ScriptValue *>(address)));
        }
        out = std::move(values);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

template <class Value, class ScriptValue = Value>
int CopyToScriptArray(void *array, const std::vector<Value> &values) {
    const CKAngelScriptAdapter::Api *api = CurrentArrayApi();
    if (!array || !api || !api->ArrayResize || !api->ArraySetElementValue)
        return BML_ERROR_INVALID_PARAMETER;
    if (values.size() > static_cast<std::size_t>(UINT32_MAX))
        return BML_ERROR_INVALID_PARAMETER;
    if (api->ArrayResize(array, static_cast<CKDWORD>(values.size())) != CKAS_OK)
        return BML_ERROR_OUT_OF_MEMORY;
    for (CKDWORD index = 0; index < values.size(); ++index) {
        const ScriptValue value = static_cast<ScriptValue>(values[index]);
        if (api->ArraySetElementValue(array, index, &value) != CKAS_OK)
            return BML_ERROR_INVALID_PARAMETER;
    }
    return BML_OK;
}

int CopyObjectsFromScriptArray(const void *array, std::vector<BML_ObjectRef> &out) {
    const CKAngelScriptAdapter::Api *api = CurrentArrayApi();
    ModContext *context = BML_GetModContext();
    if (!array || !api || !api->ArrayGetSize || !api->ArrayGetConstElementAddress || !context)
        return BML_ERROR_INVALID_PARAMETER;

    CKDWORD size = 0;
    if (api->ArrayGetSize(const_cast<void *>(array), &size) != CKAS_OK)
        return BML_ERROR_INVALID_PARAMETER;
    try {
        std::vector<BML_ObjectRef> values;
        values.reserve(size);
        for (CKDWORD index = 0; index < size; ++index) {
            const void *address = nullptr;
            if (api->ArrayGetConstElementAddress(array, index, &address) != CKAS_OK || !address)
                return BML_ERROR_INVALID_PARAMETER;
            CKObject *object = *static_cast<CKObject *const *>(address);
            if (!object)
                return BML_ERROR_OBJECT_INVALID;
            values.push_back(context->ObjectRefs().Issue(object));
            if (!values.back().Domain)
                return BML_ERROR_OBJECT_INVALID;
        }
        out = std::move(values);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

int CopyObjectsToScriptArray(void *array, const std::vector<BML_ObjectRef> &values) {
    const CKAngelScriptAdapter::Api *api = CurrentArrayApi();
    ModContext *context = BML_GetModContext();
    if (!array || !api || !api->ArrayResize || !api->ArraySetElementValue || !context)
        return BML_ERROR_INVALID_PARAMETER;
    if (values.size() > static_cast<std::size_t>(UINT32_MAX))
        return BML_ERROR_INVALID_PARAMETER;
    if (api->ArrayResize(array, static_cast<CKDWORD>(values.size())) != CKAS_OK)
        return BML_ERROR_OUT_OF_MEMORY;
    for (CKDWORD index = 0; index < values.size(); ++index) {
        CKObject *object = context->ObjectRefs().Resolve(values[index]);
        if (!object)
            return BML_ERROR_OBJECT_INVALID;
        if (api->ArraySetElementValue(array, index, &object) != CKAS_OK)
            return BML_ERROR_INVALID_PARAMETER;
    }
    return BML_OK;
}

} // namespace

void ScriptImcRecord::AddRef() { ++m_RefCount; }

void ScriptImcRecord::Release() {
    if (--m_RefCount == 0)
        delete this;
}

void ScriptImcRecord::SetError(int status) {
    if (m_Status == BML_OK)
        m_Status = status;
}

template <class T>
void ScriptImcRecord::Write(unsigned int id, T value) {
    if (m_Status != BML_OK)
        return;
    if (!id || m_IsDecoded) {
        SetError(BML_ERROR_INVALID_PARAMETER);
        return;
    }
    try {
        m_Values.insert_or_assign(id, Value(std::move(value)));
    } catch (const std::bad_alloc &) {
        SetError(BML_ERROR_OUT_OF_MEMORY);
    }
}

void ScriptImcRecord::WriteBool(unsigned int id, bool value) { Write(id, value); }
void ScriptImcRecord::WriteInt(unsigned int id, int value) { Write(id, value); }
void ScriptImcRecord::WriteFloat(unsigned int id, float value) { Write(id, value); }
void ScriptImcRecord::WriteInt64(unsigned int id, std::int64_t value) { Write(id, value); }
void ScriptImcRecord::WriteUInt64(unsigned int id, std::uint64_t value) { Write(id, value); }
void ScriptImcRecord::WriteDouble(unsigned int id, double value) { Write(id, value); }
void ScriptImcRecord::WriteString(unsigned int id, const std::string &value) { Write(id, value); }

void ScriptImcRecord::WriteBytes(unsigned int id, const void *values) {
    std::vector<std::uint8_t> copied;
    const int status = CopyFromScriptArray<std::uint8_t>(values, copied);
    if (status == BML_OK) Write(id, std::move(copied)); else SetError(status);
}

void ScriptImcRecord::WriteObject(unsigned int id, CKObject *value) {
    ModContext *context = BML_GetModContext();
    if (!context || !value) {
        SetError(BML_ERROR_OBJECT_INVALID);
        return;
    }
    const BML_ObjectRef reference = context->ObjectRefs().Issue(value);
    if (!reference.Domain) SetError(BML_ERROR_OBJECT_INVALID); else Write(id, reference);
}

void ScriptImcRecord::WriteVec2(unsigned int id, const BML_Vec2 &value) { Write(id, value); }
void ScriptImcRecord::WriteVec3(unsigned int id, const BML_Vec3 &value) { Write(id, value); }
void ScriptImcRecord::WriteMat4(unsigned int id, const BML_Mat4 &value) { Write(id, value); }

#define BML_SCRIPT_IMC_WRITE_ARRAY(Name, Value, ScriptValue) \
    void ScriptImcRecord::Write##Name##Array(unsigned int id, const void *values) { \
        std::vector<Value> copied; \
        const int status = CopyFromScriptArray<Value, ScriptValue>(values, copied); \
        if (status == BML_OK) Write(id, std::move(copied)); else SetError(status); \
    }

BML_SCRIPT_IMC_WRITE_ARRAY(Bool, bool, asBYTE)
BML_SCRIPT_IMC_WRITE_ARRAY(Int, int, int)
BML_SCRIPT_IMC_WRITE_ARRAY(Float, float, float)
BML_SCRIPT_IMC_WRITE_ARRAY(Int64, std::int64_t, std::int64_t)
BML_SCRIPT_IMC_WRITE_ARRAY(UInt64, std::uint64_t, std::uint64_t)
BML_SCRIPT_IMC_WRITE_ARRAY(Double, double, double)
BML_SCRIPT_IMC_WRITE_ARRAY(String, std::string, std::string)
BML_SCRIPT_IMC_WRITE_ARRAY(Vec2, BML_Vec2, BML_Vec2)
BML_SCRIPT_IMC_WRITE_ARRAY(Vec3, BML_Vec3, BML_Vec3)
BML_SCRIPT_IMC_WRITE_ARRAY(Mat4, BML_Mat4, BML_Mat4)

#undef BML_SCRIPT_IMC_WRITE_ARRAY

void ScriptImcRecord::WriteObjectArray(unsigned int id, const void *values) {
    std::vector<BML_ObjectRef> copied;
    const int status = CopyObjectsFromScriptArray(values, copied);
    if (status == BML_OK) Write(id, std::move(copied)); else SetError(status);
}

int ScriptImcRecord::Find(unsigned int id, FieldSlice &field) const {
    if (m_Status != BML_OK)
        return m_Status;
    const auto found = m_Fields.find(id);
    if (found == m_Fields.end())
        return BML_ERROR_MALFORMED_MESSAGE;
    field = found->second;
    return BML_OK;
}

bool ScriptImcRecord::Has(unsigned int id) const {
    if (m_Status != BML_OK)
        return false;
    return m_IsDecoded ? m_Fields.find(id) != m_Fields.end()
                       : m_Values.find(id) != m_Values.end();
}

#define BML_SCRIPT_IMC_READ_SCALAR(Name, Value) \
    int ScriptImcRecord::Read##Name(unsigned int id, Value &value) const { \
        FieldSlice slice; \
        int status = Find(id, slice); \
        if (status != BML_OK) return status; \
        const Imc::Wire::FieldView field{id, static_cast<Imc::Wire::Kind>(slice.Kind), \
                                         m_Bytes.data() + slice.Offset, slice.Size}; \
        return Imc::Wire::Reader::Read##Name(field, value); \
    }

BML_SCRIPT_IMC_READ_SCALAR(Bool, bool)
BML_SCRIPT_IMC_READ_SCALAR(Int, int)
BML_SCRIPT_IMC_READ_SCALAR(Float, float)
BML_SCRIPT_IMC_READ_SCALAR(Int64, std::int64_t)
BML_SCRIPT_IMC_READ_SCALAR(UInt64, std::uint64_t)
BML_SCRIPT_IMC_READ_SCALAR(Double, double)
BML_SCRIPT_IMC_READ_SCALAR(String, std::string)
BML_SCRIPT_IMC_READ_SCALAR(Vec2, BML_Vec2)
BML_SCRIPT_IMC_READ_SCALAR(Vec3, BML_Vec3)
BML_SCRIPT_IMC_READ_SCALAR(Mat4, BML_Mat4)

#undef BML_SCRIPT_IMC_READ_SCALAR

int ScriptImcRecord::ReadBytes(unsigned int id, void *values) const {
    FieldSlice slice;
    int status = Find(id, slice);
    std::vector<std::uint8_t> decoded;
    if (status == BML_OK) {
        const Imc::Wire::FieldView field{id, static_cast<Imc::Wire::Kind>(slice.Kind),
                                         m_Bytes.data() + slice.Offset, slice.Size};
        status = Imc::Wire::Reader::ReadBytes(field, decoded);
    }
    return status == BML_OK ? CopyToScriptArray(values, decoded) : status;
}

int ScriptImcRecord::ReadObject(unsigned int id, CKObject *&value) const {
    value = nullptr;
    FieldSlice slice;
    int status = Find(id, slice);
    BML_ObjectRef reference{};
    if (status == BML_OK) {
        const Imc::Wire::FieldView field{id, static_cast<Imc::Wire::Kind>(slice.Kind),
                                         m_Bytes.data() + slice.Offset, slice.Size};
        status = Imc::Wire::Reader::ReadObject(field, reference);
    }
    ModContext *context = BML_GetModContext();
    if (status != BML_OK || !context)
        return status != BML_OK ? status : BML_ERROR_UNAVAILABLE;
    value = context->ObjectRefs().Resolve(reference);
    return value ? BML_OK : BML_ERROR_OBJECT_INVALID;
}

#define BML_SCRIPT_IMC_READ_ARRAY(Name, Value, ScriptValue) \
    int ScriptImcRecord::Read##Name##Array(unsigned int id, void *values) const { \
        FieldSlice slice; \
        int status = Find(id, slice); \
        std::vector<Value> decoded; \
        if (status == BML_OK) { \
            const Imc::Wire::FieldView field{id, static_cast<Imc::Wire::Kind>(slice.Kind), \
                                             m_Bytes.data() + slice.Offset, slice.Size}; \
            status = Imc::Wire::Reader::Read##Name##Array(field, decoded); \
        } \
        return status == BML_OK ? CopyToScriptArray<Value, ScriptValue>(values, decoded) : status; \
    }

BML_SCRIPT_IMC_READ_ARRAY(Bool, bool, asBYTE)
BML_SCRIPT_IMC_READ_ARRAY(Int, int, int)
BML_SCRIPT_IMC_READ_ARRAY(Float, float, float)
BML_SCRIPT_IMC_READ_ARRAY(Int64, std::int64_t, std::int64_t)
BML_SCRIPT_IMC_READ_ARRAY(UInt64, std::uint64_t, std::uint64_t)
BML_SCRIPT_IMC_READ_ARRAY(Double, double, double)
BML_SCRIPT_IMC_READ_ARRAY(String, std::string, std::string)
BML_SCRIPT_IMC_READ_ARRAY(Vec2, BML_Vec2, BML_Vec2)
BML_SCRIPT_IMC_READ_ARRAY(Vec3, BML_Vec3, BML_Vec3)
BML_SCRIPT_IMC_READ_ARRAY(Mat4, BML_Mat4, BML_Mat4)

#undef BML_SCRIPT_IMC_READ_ARRAY

int ScriptImcRecord::ReadObjectArray(unsigned int id, void *values) const {
    FieldSlice slice;
    int status = Find(id, slice);
    std::vector<BML_ObjectRef> decoded;
    if (status == BML_OK) {
        const Imc::Wire::FieldView field{id, static_cast<Imc::Wire::Kind>(slice.Kind),
                                         m_Bytes.data() + slice.Offset, slice.Size};
        status = Imc::Wire::Reader::ReadObjectArray(field, decoded);
    }
    return status == BML_OK ? CopyObjectsToScriptArray(values, decoded) : status;
}

int ScriptImcRecord::Encode(std::vector<std::uint8_t> &bytes) const {
    if (m_Status != BML_OK)
        return m_Status;
    if (m_IsDecoded) {
        try {
            bytes = m_Bytes;
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    std::size_t size = 0;
    for (const auto &[id, value] : m_Values) {
        const bool added = std::visit(
            [id, &size](const auto &typed) {
                return Imc::Wire::Detail::AddFieldSize(size, id, typed);
            }, value);
        if (!added)
            return BML_ERROR_INVALID_PARAMETER;
    }

    try {
        std::vector<std::uint8_t> encoded(size);
        Imc::Wire::Writer writer(encoded.data(), encoded.size());
        int status = writer.Begin();
        for (const auto &[id, value] : m_Values) {
            if (status != BML_OK)
                break;
            status = std::visit(
                [id, &writer](const auto &typed) {
                    return Imc::Wire::Detail::WriteField(writer, id, typed);
                }, value);
        }
        if (status == BML_OK)
            status = writer.Finish();
        if (status == BML_OK)
            bytes = std::move(encoded);
        return status;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

int ScriptImcRecord::Parse(const void *data, std::size_t size) {
    m_Status = BML_OK;
    m_IsDecoded = true;
    m_Values.clear();
    m_Bytes.clear();
    m_Fields.clear();
    try {
        const auto *begin = static_cast<const std::uint8_t *>(data);
        if (size && !begin)
            return m_Status = BML_ERROR_MALFORMED_MESSAGE;
        if (size)
            m_Bytes.assign(begin, begin + size);
        Imc::Wire::Reader reader(m_Bytes.data(), m_Bytes.size());
        int status = reader.Begin();
        Imc::Wire::FieldView field;
        while (status == BML_OK && (status = reader.Next(field)) == BML_OK) {
            if (m_Fields.find(field.Id) != m_Fields.end())
                return m_Status = BML_ERROR_MALFORMED_MESSAGE;
            FieldSlice slice;
            slice.Kind = static_cast<unsigned int>(field.WireKind);
            slice.Offset = static_cast<std::size_t>(field.Data - m_Bytes.data());
            slice.Size = field.Size;
            m_Fields.emplace(field.Id, slice);
        }
        if (status == BML_ERROR_NOT_FOUND)
            status = reader.Finish();
        m_Status = status;
        return status;
    } catch (const std::bad_alloc &) {
        return m_Status = BML_ERROR_OUT_OF_MEMORY;
    }
}

ScriptImcRecord *ScriptImcRecord::Decode(const BML_ImcMessage &message) {
    auto *record = new (std::nothrow) ScriptImcRecord();
    if (!record)
        return nullptr;
    record->Parse(message.Data, message.DataSize);
    return record;
}

ScriptImcRecord *CreateScriptImcRecord() {
    return new (std::nothrow) ScriptImcRecord();
}

} // namespace BML
