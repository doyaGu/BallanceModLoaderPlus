#include "ScriptInteropConsumerBridge.h"

#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "BML/InteropApi.h"

#include "InteropRegistry.h"
#include "InteropSessionService.h"
#include "ModContext.h"
#include "ScriptInteropArray.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"
#include "ScriptFunctionSupport.h"
#include "ScriptStringInterop.h"

#ifdef GetObject
#undef GetObject
#endif

namespace BML {
namespace {

std::string g_ConsumerBridgeRegistrationError;

bool Register(asIScriptEngine *engine, int status, const char *declaration, const char **errorMessage) {
    if (status >= 0)
        return true;
    g_ConsumerBridgeRegistrationError = "Failed to register Interop consumer declaration: ";
    g_ConsumerBridgeRegistrationError += declaration ? declaration : "";
    g_ConsumerBridgeRegistrationError += " returned ";
    g_ConsumerBridgeRegistrationError += std::to_string(status);
    if (engine)
        engine->SetDefaultNamespace("");
    if (errorMessage)
        *errorMessage = g_ConsumerBridgeRegistrationError.c_str();
    return false;
}

struct ActiveCall {
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    BML_InteropCallContext CallContext{};
};

int GetActiveCall(ActiveCall &out) {
    out = {};
    if (RejectScriptRestrictedHostCall("BML::Interop"))
        return BML_ERROR_FROZEN;
    ScriptMod *owner = ScriptModRuntime::GetCurrentScriptMod();
    if (!owner || !owner->GetModContext())
        return BML_ERROR_INTEROP_UNSUPPORTED;
    out.Context = owner->GetModContext();
    out.Owner = owner;
    out.CallContext = out.Context->GetInteropSessions().CreateContextForOwner(owner->GetID());
    return out.Context->GetInteropSessions().ValidateContext(&out.CallContext, true);
}

int GetStoredCall(ModContext *context, const std::string &owner, BML_InteropCallContext &out) {
    out = {};
    if (!context || owner.empty())
        return BML_ERROR_INTEROP_HANDLE_STALE;
    out = context->GetInteropSessions().CreateContextForOwner(owner.c_str());
    return context->GetInteropSessions().ValidateContext(&out, true);
}

int GetHandleCall(ModContext *context, const std::string &owner, BML_InteropCallContext &out) {
    ScriptMod *current = ScriptModRuntime::GetCurrentScriptMod();
    if (!current || !current->GetModContext() || current->GetModContext() != context ||
        owner != current->GetID()) {
        out = {};
        return BML_ERROR_INTEROP_HANDLE_STALE;
    }
    return GetStoredCall(context, owner, out);
}

ScriptMod *GetArrayOwner(const std::string &owner) {
    ScriptMod *current = ScriptModRuntime::GetCurrentScriptMod();
    return current && owner == current->GetID() ? current : nullptr;
}

class ScriptInteropRecord final {
public:
    ScriptInteropRecord(ModContext *context, std::string owner, BML_RecordRef record)
        : m_Context(context), m_Owner(std::move(owner)), m_Record(record) {}
    ~ScriptInteropRecord() { ReleaseNative(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return Status() == BML_OK; }
    int Status() const {
        BML_InteropCallContext call{};
        uint32_t schema = 0;
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordSchema(&call, m_Record, &schema)
                   : status;
    }
    uint32_t GetSchema() const {
        BML_InteropCallContext call{};
        uint32_t schema = 0;
        return GetCall(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordSchema(&call, m_Record, &schema) == BML_OK
                   ? schema
                   : 0;
    }
    uint64_t GetSequence() const {
        BML_InteropCallContext call{};
        uint64_t value = 0;
        return GetCall(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordSequence(&call, m_Record, &value) == BML_OK
                   ? value
                   : 0;
    }
    uint64_t GetTimestamp() const {
        BML_InteropCallContext call{};
        uint64_t value = 0;
        return GetCall(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordTimestamp(&call, m_Record, &value) == BML_OK
                   ? value
                   : 0;
    }

    int GetBool(uint32_t field, bool &out) const {
        out = false;
        BML_InteropCallContext call{};
        int value = 0;
        const int status = GetCall(call);
        if (status != BML_OK)
            return status;
        const int getStatus = m_Context->GetInteropRegistry().RecordGetBool(&call, m_Record, field, &value);
        if (getStatus == BML_OK)
            out = value != 0;
        return getStatus;
    }
    int GetInt(uint32_t field, int &out) const {
        out = 0;
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordGetInt(&call, m_Record, field, &out)
                   : status;
    }
    int GetFloat(uint32_t field, float &out) const {
        out = 0.0f;
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordGetFloat(&call, m_Record, field, &out)
                   : status;
    }
    int GetString(uint32_t field, std::string &out) const {
        out.clear();
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        if (status != BML_OK)
            return status;
        size_t required = 0;
        int getStatus = m_Context->GetInteropRegistry().RecordGetString(&call, m_Record, field, nullptr, 0, &required);
        if (getStatus != BML_OK)
            return getStatus;
        try {
            std::string value(required ? required : 1, '\0');
            getStatus = m_Context->GetInteropRegistry().RecordGetString(
                &call, m_Record, field, value.data(), value.size(), &required);
            if (getStatus == BML_OK)
                out.assign(value.data(), required > 0 ? required - 1 : 0);
            return getStatus;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }
    int GetObject(uint32_t field, BML_ObjectRef &out) const {
        out = {};
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordGetObject(&call, m_Record, field, &out)
                   : status;
    }
    int GetVec2(uint32_t field, BML_Vec2 &out) const {
        out = {};
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordGetVec2(&call, m_Record, field, &out)
                   : status;
    }
    int GetVec3(uint32_t field, BML_Vec3 &out) const {
        out = {};
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordGetVec3(&call, m_Record, field, &out)
                   : status;
    }
    int GetMat4(uint32_t field, BML_Mat4 &out) const {
        out = {};
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordGetMat4(&call, m_Record, field, &out)
                   : status;
    }

    int Borrow(uint32_t field,
               BML_INTEROP_FIELD_TYPE type,
               const void **outData,
               size_t *outCount,
               size_t *outElementSize) const {
        if (outData) *outData = nullptr;
        if (outCount) *outCount = 0;
        if (outElementSize) *outElementSize = 0;
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().RecordBorrowValue(
                         &call, m_Record, field, type, outData, outCount, outElementSize)
                   : status;
    }

    int GetStringArrayValues(uint32_t field, std::vector<std::string> &out) const {
        out.clear();
        BML_InteropCallContext call{};
        const int status = GetCall(call);
        if (status != BML_OK)
            return status;
        size_t count = 0;
        int getStatus = m_Context->GetInteropRegistry().RecordGetStringArrayCount(&call, m_Record, field, &count);
        if (getStatus != BML_OK)
            return getStatus;
        try {
            std::vector<std::string> values;
            values.reserve(count);
            for (size_t index = 0; index < count; ++index) {
                size_t required = 0;
                getStatus = m_Context->GetInteropRegistry().RecordGetStringArrayItem(
                    &call, m_Record, field, index, nullptr, 0, &required);
                if (getStatus != BML_OK)
                    return getStatus;
                std::string value(required ? required : 1, '\0');
                getStatus = m_Context->GetInteropRegistry().RecordGetStringArrayItem(
                    &call, m_Record, field, index, value.data(), value.size(), &required);
                if (getStatus != BML_OK)
                    return getStatus;
                values.emplace_back(value.data(), required > 0 ? required - 1 : 0);
            }
            out = std::move(values);
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    const std::string &OwnerId() const { return m_Owner; }

private:
    int GetCall(BML_InteropCallContext &out) const {
        return GetHandleCall(m_Context, m_Owner, out);
    }
    void ReleaseNative() {
        if (!m_Context || m_Record.Value == 0)
            return;
        BML_InteropCallContext call{};
        if (GetStoredCall(m_Context, m_Owner, call) == BML_OK)
            (void)m_Context->GetInteropRegistry().ReleaseRecord(&call, m_Record);
        m_Record = {};
    }

    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::string m_Owner;
    BML_RecordRef m_Record{};
};

class ScriptInteropInput final {
public:
    ScriptInteropInput(ModContext *context,
                       std::string owner,
                       uint64_t sessionId,
                       BML_InteropRecordBuilder *builder)
        : m_Context(context), m_Owner(std::move(owner)), m_SessionId(sessionId), m_Builder(builder) {}
    ~ScriptInteropInput() { ReleaseNative(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsActive() const {
        BML_InteropCallContext call{};
        return m_Builder && GetHandleCall(m_Context, m_Owner, call) == BML_OK && call.SessionId == m_SessionId;
    }

    int SetBool(uint32_t field, bool value) {
        const int native = value ? 1 : 0;
        return SetValue(field, BML_INTEROP_FIELD_BOOL, &native, 1);
    }
    int SetInt(uint32_t field, int value) {
        return SetValue(field, BML_INTEROP_FIELD_INT, &value, 1);
    }
    int SetFloat(uint32_t field, float value) {
        return SetValue(field, BML_INTEROP_FIELD_FLOAT, &value, 1);
    }
    int SetString(uint32_t field, const std::string &value) {
        return SetValue(field, BML_INTEROP_FIELD_STRING, value.data(), value.size());
    }
    int SetObject(uint32_t field, const BML_ObjectRef &value) {
        return SetValue(field, BML_INTEROP_FIELD_OBJECT, &value, 1);
    }
    int SetVec2(uint32_t field, const BML_Vec2 &value) {
        return SetValue(field, BML_INTEROP_FIELD_VEC2, &value, 1);
    }
    int SetVec3(uint32_t field, const BML_Vec3 &value) {
        return SetValue(field, BML_INTEROP_FIELD_VEC3, &value, 1);
    }
    int SetMat4(uint32_t field, const BML_Mat4 &value) {
        return SetValue(field, BML_INTEROP_FIELD_MAT4, &value, 1);
    }
    int SetArray(uint32_t field, BML_INTEROP_FIELD_TYPE type, const void *values, size_t count) {
        BML_InteropCallContext call{};
        if (!m_Builder || GetHandleCall(m_Context, m_Owner, call) != BML_OK || call.SessionId != m_SessionId)
            return BML_ERROR_INTEROP_HANDLE_STALE;
        return m_Context->GetInteropRegistry().BuilderSetValue(m_Builder, field, type, values, count);
    }
    int SetStringArray(uint32_t field, const std::vector<std::string> &values) {
        BML_InteropCallContext call{};
        if (!m_Builder || GetHandleCall(m_Context, m_Owner, call) != BML_OK || call.SessionId != m_SessionId)
            return BML_ERROR_INTEROP_HANDLE_STALE;
        std::vector<const char *> strings;
        std::vector<size_t> sizes;
        try {
            strings.reserve(values.size());
            sizes.reserve(values.size());
            for (const std::string &value : values) {
                strings.push_back(value.data());
                sizes.push_back(value.size());
            }
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
        return m_Context->GetInteropRegistry().BuilderSetStringArray(
            m_Builder,
            field,
            strings.empty() ? nullptr : strings.data(),
            sizes.empty() ? nullptr : sizes.data(),
            strings.size());
    }

    BML_InteropRecordBuilder *Builder() const { return m_Builder; }
    const std::string &OwnerId() const { return m_Owner; }
    ModContext *Context() const { return m_Context; }
    uint64_t SessionId() const { return m_SessionId; }

private:
    int SetValue(uint32_t field, BML_INTEROP_FIELD_TYPE type, const void *value, size_t count) {
        BML_InteropCallContext call{};
        if (!m_Builder || GetHandleCall(m_Context, m_Owner, call) != BML_OK || call.SessionId != m_SessionId)
            return BML_ERROR_INTEROP_HANDLE_STALE;
        return m_Context->GetInteropRegistry().BuilderSetValue(m_Builder, field, type, value, count);
    }
    void ReleaseNative() {
        if (m_Context && m_Builder)
            (void)m_Context->GetInteropRegistry().DestroyRecordBuilder(m_Builder);
        m_Builder = nullptr;
    }

    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::string m_Owner;
    uint64_t m_SessionId = 0;
    BML_InteropRecordBuilder *m_Builder = nullptr;
};

class ScriptInteropStream final {
public:
    ScriptInteropStream(ModContext *context, std::string owner, BML_StreamRef stream)
        : m_Context(context), m_Owner(std::move(owner)), m_Stream(stream) {}
    ~ScriptInteropStream() { (void)Close(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsOpen() const { return m_Stream.Value != 0; }
    int Close() {
        if (m_Stream.Value == 0)
            return BML_OK;
        BML_InteropCallContext call{};
        const int contextStatus = GetStoredCall(m_Context, m_Owner, call);
        const int status = contextStatus == BML_OK
                               ? m_Context->GetInteropRegistry().CloseStream(&call, m_Stream)
                               : contextStatus;
        m_Stream = {};
        return status;
    }
    int GetDroppedCount(int &out) const {
        out = 0;
        BML_InteropCallContext call{};
        const int status = GetHandleCall(m_Context, m_Owner, call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().DroppedStreamCount(&call, m_Stream, &out)
                   : status;
    }
    int Poll(ScriptInteropRecord *&out) {
        out = nullptr;
        BML_InteropCallContext call{};
        const int contextStatus = GetHandleCall(m_Context, m_Owner, call);
        if (contextStatus != BML_OK)
            return contextStatus;
        BML_RecordRef record{};
        const int status = m_Context->GetInteropRegistry().PollStream(&call, m_Stream, &record);
        if (status != BML_OK || record.Value == 0)
            return status;
        ScriptInteropRecord *result = new (std::nothrow) ScriptInteropRecord(m_Context, m_Owner, record);
        if (!result) {
            (void)m_Context->GetInteropRegistry().ReleaseRecord(&call, record);
            return BML_ERROR_OUT_OF_MEMORY;
        }
        out = result;
        return BML_OK;
    }

private:
    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::string m_Owner;
    BML_StreamRef m_Stream{};
};

class ScriptInteropCursor final {
public:
    ScriptInteropCursor(ModContext *context, std::string owner, BML_CursorRef cursor)
        : m_Context(context), m_Owner(std::move(owner)), m_Cursor(cursor) {}
    ~ScriptInteropCursor() { (void)Close(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsOpen() const { return m_Cursor.Value != 0; }
    int Close() {
        if (m_Cursor.Value == 0)
            return BML_OK;
        BML_InteropCallContext call{};
        const int contextStatus = GetStoredCall(m_Context, m_Owner, call);
        const int status = contextStatus == BML_OK
                               ? m_Context->GetInteropRegistry().CloseCollection(&call, m_Cursor)
                               : contextStatus;
        m_Cursor = {};
        return status;
    }
    int Next(ScriptInteropRecord *&out, bool &hasValue, bool &complete) {
        out = nullptr;
        hasValue = false;
        complete = false;
        BML_InteropCallContext call{};
        const int contextStatus = GetHandleCall(m_Context, m_Owner, call);
        if (contextStatus != BML_OK)
            return contextStatus;
        BML_RecordRef record{};
        size_t count = 0;
        int nativeComplete = 0;
        const int status = m_Context->GetInteropRegistry().ReadCollectionPage(
            &call, m_Cursor, &record, 1, &count, &nativeComplete);
        if (status != BML_OK)
            return status;
        hasValue = count != 0;
        complete = nativeComplete != 0;
        if (!hasValue)
            return BML_OK;
        ScriptInteropRecord *result = new (std::nothrow) ScriptInteropRecord(m_Context, m_Owner, record);
        if (!result) {
            (void)m_Context->GetInteropRegistry().ReleaseRecord(&call, record);
            return BML_ERROR_OUT_OF_MEMORY;
        }
        out = result;
        return BML_OK;
    }

private:
    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::string m_Owner;
    BML_CursorRef m_Cursor{};
};

template <BML_INTEROP_FIELD_TYPE Type, typename T>
void ReadFixedArray(asIScriptGeneric *gen, const char *elementDeclaration) {
    auto *record = static_cast<ScriptInteropRecord *>(gen ? gen->GetObject() : nullptr);
    int status = record ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    const void *data = nullptr;
    size_t count = 0;
    size_t elementSize = 0;
    if (status == BML_OK)
        status = record->Borrow(gen->GetArgDWord(0), Type, &data, &count, &elementSize);
    if (status == BML_OK && (elementSize != sizeof(T) || (count != 0 && !data)))
        status = BML_ERROR_INTEROP_RECORD_INVALID;
    if (status == BML_OK) {
        ScriptMod *owner = GetArrayOwner(record->OwnerId());
        status = owner ? ScriptInteropArray::WriteFixed(
                             owner, gen, 1, elementDeclaration, static_cast<const T *>(data), count)
                       : BML_ERROR_INTEROP_HANDLE_STALE;
    }
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

void ReadBoolArray(asIScriptGeneric *gen) {
    auto *record = static_cast<ScriptInteropRecord *>(gen ? gen->GetObject() : nullptr);
    int status = record ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    const void *data = nullptr;
    size_t count = 0;
    size_t elementSize = 0;
    if (status == BML_OK)
        status = record->Borrow(gen->GetArgDWord(0), BML_INTEROP_FIELD_BOOL_ARRAY, &data, &count, &elementSize);
    if (status == BML_OK && (elementSize != sizeof(int) || (count != 0 && !data)))
        status = BML_ERROR_INTEROP_RECORD_INVALID;
    if (status == BML_OK) {
        ScriptMod *owner = GetArrayOwner(record->OwnerId());
        status = owner ? ScriptInteropArray::WriteBool(owner, gen, 1, static_cast<const int *>(data), count)
                       : BML_ERROR_INTEROP_HANDLE_STALE;
    }
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

void ReadStringArray(asIScriptGeneric *gen) {
    auto *record = static_cast<ScriptInteropRecord *>(gen ? gen->GetObject() : nullptr);
    int status = record ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<std::string> values;
        if (status == BML_OK)
            status = record->GetStringArrayValues(gen->GetArgDWord(0), values);
        if (status == BML_OK) {
            ScriptMod *owner = GetArrayOwner(record->OwnerId());
            status = owner ? ScriptInteropArray::WriteString(owner, gen, 1, values)
                           : BML_ERROR_INTEROP_HANDLE_STALE;
        }
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

void ReadIntArray(asIScriptGeneric *gen) {
    ReadFixedArray<BML_INTEROP_FIELD_INT_ARRAY, int>(gen, "int");
}
void ReadFloatArray(asIScriptGeneric *gen) {
    ReadFixedArray<BML_INTEROP_FIELD_FLOAT_ARRAY, float>(gen, "float");
}
void ReadObjectArray(asIScriptGeneric *gen) {
    ReadFixedArray<BML_INTEROP_FIELD_OBJECT_ARRAY, BML_ObjectRef>(gen, "BML::Interop::ObjectRef");
}
void ReadVec2Array(asIScriptGeneric *gen) {
    ReadFixedArray<BML_INTEROP_FIELD_VEC2_ARRAY, BML_Vec2>(gen, "BML::Vec2");
}
void ReadVec3Array(asIScriptGeneric *gen) {
    ReadFixedArray<BML_INTEROP_FIELD_VEC3_ARRAY, BML_Vec3>(gen, "BML::Vec3");
}
void ReadMat4Array(asIScriptGeneric *gen) {
    ReadFixedArray<BML_INTEROP_FIELD_MAT4_ARRAY, BML_Mat4>(gen, "BML::Mat4");
}

template <BML_INTEROP_FIELD_TYPE Type, typename T>
void SetFixedArray(asIScriptGeneric *gen, const char *elementDeclaration) {
    auto *input = static_cast<ScriptInteropInput *>(gen ? gen->GetObject() : nullptr);
    int status = input ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<T> values;
        ScriptMod *owner = input ? GetArrayOwner(input->OwnerId()) : nullptr;
        if (status == BML_OK && !owner)
            status = BML_ERROR_INTEROP_HANDLE_STALE;
        if (status == BML_OK)
            status = ScriptInteropArray::ReadFixed(owner, gen, 1, elementDeclaration, values);
        if (status == BML_OK)
            status = input->SetArray(gen->GetArgDWord(0), Type,
                                     values.empty() ? nullptr : values.data(), values.size());
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

void SetBoolArray(asIScriptGeneric *gen) {
    auto *input = static_cast<ScriptInteropInput *>(gen ? gen->GetObject() : nullptr);
    int status = input ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<int> values;
        ScriptMod *owner = input ? GetArrayOwner(input->OwnerId()) : nullptr;
        if (status == BML_OK && !owner)
            status = BML_ERROR_INTEROP_HANDLE_STALE;
        if (status == BML_OK)
            status = ScriptInteropArray::ReadBool(owner, gen, 1, values);
        if (status == BML_OK)
            status = input->SetArray(gen->GetArgDWord(0), BML_INTEROP_FIELD_BOOL_ARRAY,
                                     values.empty() ? nullptr : values.data(), values.size());
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

void SetStringArray(asIScriptGeneric *gen) {
    auto *input = static_cast<ScriptInteropInput *>(gen ? gen->GetObject() : nullptr);
    int status = input ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<std::string> values;
        ScriptMod *owner = input ? GetArrayOwner(input->OwnerId()) : nullptr;
        if (status == BML_OK && !owner)
            status = BML_ERROR_INTEROP_HANDLE_STALE;
        if (status == BML_OK)
            status = ScriptInteropArray::ReadString(owner, gen, 1, values);
        if (status == BML_OK)
            status = input->SetStringArray(gen->GetArgDWord(0), values);
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

void SetIntArray(asIScriptGeneric *gen) {
    SetFixedArray<BML_INTEROP_FIELD_INT_ARRAY, int>(gen, "int");
}
void SetFloatArray(asIScriptGeneric *gen) {
    SetFixedArray<BML_INTEROP_FIELD_FLOAT_ARRAY, float>(gen, "float");
}
void SetObjectArray(asIScriptGeneric *gen) {
    SetFixedArray<BML_INTEROP_FIELD_OBJECT_ARRAY, BML_ObjectRef>(gen, "BML::Interop::ObjectRef");
}
void SetVec2Array(asIScriptGeneric *gen) {
    SetFixedArray<BML_INTEROP_FIELD_VEC2_ARRAY, BML_Vec2>(gen, "BML::Vec2");
}
void SetVec3Array(asIScriptGeneric *gen) {
    SetFixedArray<BML_INTEROP_FIELD_VEC3_ARRAY, BML_Vec3>(gen, "BML::Vec3");
}
void SetMat4Array(asIScriptGeneric *gen) {
    SetFixedArray<BML_INTEROP_FIELD_MAT4_ARRAY, BML_Mat4>(gen, "BML::Mat4");
}

int MakeRecord(const ActiveCall &call, BML_RecordRef native, ScriptInteropRecord *&out) {
    out = nullptr;
    if (native.Value == 0)
        return BML_OK;
    ScriptInteropRecord *record = new (std::nothrow) ScriptInteropRecord(call.Context, call.Owner->GetID(), native);
    if (!record) {
        (void)call.Context->GetInteropRegistry().ReleaseRecord(&call.CallContext, native);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = record;
    return BML_OK;
}

int RequireApi(const std::string &apiId, uint32_t major, uint64_t hash) {
    ActiveCall call;
    const int status = GetActiveCall(call);
    return status == BML_OK
               ? call.Context->GetInteropRegistry().RequireApi(&call.CallContext, apiId.c_str(), major, hash)
               : status;
}

int ReadResource(const std::string &apiId, const std::string &endpoint, ScriptInteropRecord *&out) {
    out = nullptr;
    ActiveCall call;
    int status = GetActiveCall(call);
    BML_RecordRef record{};
    if (status == BML_OK)
        status = call.Context->GetInteropRegistry().ReadResource(
            &call.CallContext, apiId.c_str(), endpoint.c_str(), &record);
    return status == BML_OK ? MakeRecord(call, record, out) : status;
}

int ReadComponent(const std::string &apiId,
                  const std::string &endpoint,
                  const BML_ObjectRef &object,
                  ScriptInteropRecord *&out) {
    out = nullptr;
    ActiveCall call;
    int status = GetActiveCall(call);
    BML_RecordRef record{};
    if (status == BML_OK)
        status = call.Context->GetInteropRegistry().ReadComponent(
            &call.CallContext, apiId.c_str(), endpoint.c_str(), object, &record);
    return status == BML_OK ? MakeRecord(call, record, out) : status;
}

int OpenStream(const std::string &apiId,
               const std::string &endpoint,
               ScriptInteropStream *&out,
               int capacity) {
    out = nullptr;
    ActiveCall call;
    int status = GetActiveCall(call);
    BML_StreamRef stream{};
    if (status == BML_OK)
        status = call.Context->GetInteropRegistry().OpenStream(
            &call.CallContext, apiId.c_str(), endpoint.c_str(), capacity, &stream);
    if (status != BML_OK)
        return status;
    ScriptInteropStream *result = new (std::nothrow) ScriptInteropStream(call.Context, call.Owner->GetID(), stream);
    if (!result) {
        (void)call.Context->GetInteropRegistry().CloseStream(&call.CallContext, stream);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = result;
    return BML_OK;
}

int OpenCollection(const std::string &apiId,
                   const std::string &endpoint,
                   ScriptInteropCursor *&out) {
    out = nullptr;
    ActiveCall call;
    int status = GetActiveCall(call);
    BML_CursorRef cursor{};
    if (status == BML_OK)
        status = call.Context->GetInteropRegistry().OpenCollection(
            &call.CallContext, apiId.c_str(), endpoint.c_str(), &cursor);
    if (status != BML_OK)
        return status;
    ScriptInteropCursor *result = new (std::nothrow) ScriptInteropCursor(call.Context, call.Owner->GetID(), cursor);
    if (!result) {
        (void)call.Context->GetInteropRegistry().CloseCollection(&call.CallContext, cursor);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = result;
    return BML_OK;
}

int CreateInput(const std::string &apiId, uint32_t schema, ScriptInteropInput *&out) {
    out = nullptr;
    ActiveCall call;
    int status = GetActiveCall(call);
    BML_InteropRecordBuilder *builder = nullptr;
    if (status == BML_OK)
        status = call.Context->GetInteropRegistry().CreateInputRecord(
            &call.CallContext, apiId.c_str(), schema, &builder);
    if (status != BML_OK)
        return status;
    ScriptInteropInput *result = new (std::nothrow) ScriptInteropInput(
        call.Context, call.Owner->GetID(), call.CallContext.SessionId, builder);
    if (!result) {
        (void)call.Context->GetInteropRegistry().DestroyRecordBuilder(builder);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = result;
    return BML_OK;
}

int Invoke(const std::string &apiId,
           const std::string &endpoint,
           BML_INTEROP_ENDPOINT_KIND kind,
           ScriptInteropInput *input,
           ScriptInteropRecord *&out) {
    out = nullptr;
    if (!input || !input->Builder())
        return BML_ERROR_INTEROP_RECORD_INVALID;
    ActiveCall call;
    int status = GetActiveCall(call);
    if (status != BML_OK)
        return status;
    if (input->Context() != call.Context || input->OwnerId() != call.Owner->GetID() ||
        input->SessionId() != call.CallContext.SessionId)
        return BML_ERROR_INTEROP_HANDLE_STALE;
    BML_RecordRef record{};
    status = call.Context->GetInteropRegistry().Invoke(
        &call.CallContext, apiId.c_str(), endpoint.c_str(), kind, input->Builder(), &record);
    return status == BML_OK ? MakeRecord(call, record, out) : status;
}

int InvokeQuery(const std::string &apiId,
                const std::string &endpoint,
                ScriptInteropInput *input,
                ScriptInteropRecord *&out) {
    return Invoke(apiId, endpoint, BML_INTEROP_ENDPOINT_QUERY, input, out);
}

int InvokeCommand(const std::string &apiId,
                  const std::string &endpoint,
                  ScriptInteropInput *input,
                  ScriptInteropRecord *&out) {
    return Invoke(apiId, endpoint, BML_INTEROP_ENDPOINT_COMMAND, input, out);
}

} // namespace

int RegisterScriptInteropConsumerBridge(asIScriptEngine *engine, const char **errorMessage) {
    if (!engine) {
        g_ConsumerBridgeRegistrationError = "Interop consumer bridge received a null engine.";
        if (errorMessage)
            *errorMessage = g_ConsumerBridgeRegistrationError.c_str();
        return asERROR;
    }
    if (!Register(engine, engine->SetDefaultNamespace("BML::Interop"), "namespace BML::Interop", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("Record", 0, asOBJ_REF), "Record", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("Stream", 0, asOBJ_REF), "Stream", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("Cursor", 0, asOBJ_REF), "Cursor", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("Input", 0, asOBJ_REF), "Input", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterObjectBehaviour("Record", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropRecord, AddRef), asCALL_THISCALL),
                  "Record::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Record", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropRecord, Release), asCALL_THISCALL),
                  "Record::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Stream", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropStream, AddRef), asCALL_THISCALL),
                  "Stream::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Stream", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropStream, Release), asCALL_THISCALL),
                  "Stream::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Cursor", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropCursor, AddRef), asCALL_THISCALL),
                  "Cursor::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Cursor", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropCursor, Release), asCALL_THISCALL),
                  "Cursor::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Input", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropInput, AddRef), asCALL_THISCALL),
                  "Input::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Input", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropInput, Release), asCALL_THISCALL),
                  "Input::Release", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterObjectMethod("Record", "bool get_IsValid() const",
                                                         asMETHOD(ScriptInteropRecord, IsValid), asCALL_THISCALL),
                  "Record::IsValid", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int get_Status() const",
                                                         asMETHOD(ScriptInteropRecord, Status), asCALL_THISCALL),
                  "Record::Status", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "uint get_Schema() const",
                                                         asMETHOD(ScriptInteropRecord, GetSchema), asCALL_THISCALL),
                  "Record::Schema", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "uint64 get_Sequence() const",
                                                         asMETHOD(ScriptInteropRecord, GetSequence), asCALL_THISCALL),
                  "Record::Sequence", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "uint64 get_Timestamp() const",
                                                         asMETHOD(ScriptInteropRecord, GetTimestamp), asCALL_THISCALL),
                  "Record::Timestamp", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetBool(uint field, bool &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetBool), asCALL_GENERIC),
                  "Record::GetBool", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetInt(uint field, int &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetInt), asCALL_GENERIC),
                  "Record::GetInt", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetFloat(uint field, float &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetFloat), asCALL_GENERIC),
                  "Record::GetFloat", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetString(uint field, string &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetString), asCALL_GENERIC),
                  "Record::GetString", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetObject(uint field, ObjectRef &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetObject), asCALL_GENERIC),
                  "Record::GetObject", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetVec2(uint field, ::BML::Vec2 &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetVec2), asCALL_GENERIC),
                  "Record::GetVec2", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetVec3(uint field, ::BML::Vec3 &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetVec3), asCALL_GENERIC),
                  "Record::GetVec3", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetMat4(uint field, ::BML::Mat4 &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecord::GetMat4), asCALL_GENERIC),
                  "Record::GetMat4", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetBoolArray(uint field, array<bool> &out values) const",
                                                         asFUNCTION(ReadBoolArray), asCALL_GENERIC),
                  "Record::GetBoolArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetIntArray(uint field, array<int> &out values) const",
                                                         asFUNCTION(ReadIntArray), asCALL_GENERIC),
                  "Record::GetIntArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetFloatArray(uint field, array<float> &out values) const",
                                                         asFUNCTION(ReadFloatArray), asCALL_GENERIC),
                  "Record::GetFloatArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetStringArray(uint field, array<string> &out values) const",
                                                         asFUNCTION(ReadStringArray), asCALL_GENERIC),
                  "Record::GetStringArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetObjectArray(uint field, array<ObjectRef> &out values) const",
                                                         asFUNCTION(ReadObjectArray), asCALL_GENERIC),
                  "Record::GetObjectArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetVec2Array(uint field, array<::BML::Vec2> &out values) const",
                                                         asFUNCTION(ReadVec2Array), asCALL_GENERIC),
                  "Record::GetVec2Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetVec3Array(uint field, array<::BML::Vec3> &out values) const",
                                                         asFUNCTION(ReadVec3Array), asCALL_GENERIC),
                  "Record::GetVec3Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Record", "int GetMat4Array(uint field, array<::BML::Mat4> &out values) const",
                                                         asFUNCTION(ReadMat4Array), asCALL_GENERIC),
                  "Record::GetMat4Array", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterObjectMethod("Stream", "bool get_IsOpen() const",
                                                         asMETHOD(ScriptInteropStream, IsOpen), asCALL_THISCALL),
                  "Stream::IsOpen", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Stream", "int Close()",
                                                         asMETHOD(ScriptInteropStream, Close), asCALL_THISCALL),
                  "Stream::Close", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Stream", "int GetDroppedCount(int &out count) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropStream::GetDroppedCount), asCALL_GENERIC),
                  "Stream::GetDroppedCount", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Stream", "int Poll(Record@ &out record)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropStream::Poll), asCALL_GENERIC),
                  "Stream::Poll", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Cursor", "bool get_IsOpen() const",
                                                         asMETHOD(ScriptInteropCursor, IsOpen), asCALL_THISCALL),
                  "Cursor::IsOpen", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Cursor", "int Close()",
                                                         asMETHOD(ScriptInteropCursor, Close), asCALL_THISCALL),
                  "Cursor::Close", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Cursor", "int Next(Record@ &out record, bool &out hasValue, bool &out complete)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropCursor::Next), asCALL_GENERIC),
                  "Cursor::Next", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterObjectMethod("Input", "bool get_IsActive() const",
                                                         asMETHOD(ScriptInteropInput, IsActive), asCALL_THISCALL),
                  "Input::IsActive", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetBool(uint field, bool value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetBool), asCALL_GENERIC),
                  "Input::SetBool", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetInt(uint field, int value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetInt), asCALL_GENERIC),
                  "Input::SetInt", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetFloat(uint field, float value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetFloat), asCALL_GENERIC),
                  "Input::SetFloat", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetString(uint field, const string &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetString), asCALL_GENERIC),
                  "Input::SetString", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetObject(uint field, const ObjectRef &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetObject), asCALL_GENERIC),
                  "Input::SetObject", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetVec2(uint field, const ::BML::Vec2 &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetVec2), asCALL_GENERIC),
                  "Input::SetVec2", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetVec3(uint field, const ::BML::Vec3 &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetVec3), asCALL_GENERIC),
                  "Input::SetVec3", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetMat4(uint field, const ::BML::Mat4 &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropInput::SetMat4), asCALL_GENERIC),
                  "Input::SetMat4", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetBoolArray(uint field, const array<bool> &in values)",
                                                         asFUNCTION(SetBoolArray), asCALL_GENERIC),
                  "Input::SetBoolArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetIntArray(uint field, const array<int> &in values)",
                                                         asFUNCTION(SetIntArray), asCALL_GENERIC),
                  "Input::SetIntArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetFloatArray(uint field, const array<float> &in values)",
                                                         asFUNCTION(SetFloatArray), asCALL_GENERIC),
                  "Input::SetFloatArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetStringArray(uint field, const array<string> &in values)",
                                                         asFUNCTION(SetStringArray), asCALL_GENERIC),
                  "Input::SetStringArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetObjectArray(uint field, const array<ObjectRef> &in values)",
                                                         asFUNCTION(SetObjectArray), asCALL_GENERIC),
                  "Input::SetObjectArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetVec2Array(uint field, const array<::BML::Vec2> &in values)",
                                                         asFUNCTION(SetVec2Array), asCALL_GENERIC),
                  "Input::SetVec2Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetVec3Array(uint field, const array<::BML::Vec3> &in values)",
                                                         asFUNCTION(SetVec3Array), asCALL_GENERIC),
                  "Input::SetVec3Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Input", "int SetMat4Array(uint field, const array<::BML::Mat4> &in values)",
                                                         asFUNCTION(SetMat4Array), asCALL_GENERIC),
                  "Input::SetMat4Array", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterGlobalFunction("int RequireApi(const string &in apiId, uint major, uint64 hash)",
                                                           BML_AS_GENERIC_FUNCTION(&RequireApi), asCALL_GENERIC),
                  "RequireApi", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int ReadResource(const string &in apiId, const string &in endpoint, Record@ &out record)",
                                                           BML_AS_GENERIC_FUNCTION(&ReadResource), asCALL_GENERIC),
                  "ReadResource", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int ReadComponent(const string &in apiId, const string &in endpoint, const ObjectRef &in object, Record@ &out record)",
                                                           BML_AS_GENERIC_FUNCTION(&ReadComponent), asCALL_GENERIC),
                  "ReadComponent", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int OpenStream(const string &in apiId, const string &in endpoint, Stream@ &out stream, int capacity = 256)",
                                                           BML_AS_GENERIC_FUNCTION(&OpenStream), asCALL_GENERIC),
                  "OpenStream", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int OpenCollection(const string &in apiId, const string &in endpoint, Cursor@ &out cursor)",
                                                           BML_AS_GENERIC_FUNCTION(&OpenCollection), asCALL_GENERIC),
                  "OpenCollection", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int CreateInput(const string &in apiId, uint schema, Input@ &out input)",
                                                           BML_AS_GENERIC_FUNCTION(&CreateInput), asCALL_GENERIC),
                  "CreateInput", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int InvokeQuery(const string &in apiId, const string &in endpoint, Input@ input, Record@ &out record)",
                                                           BML_AS_GENERIC_FUNCTION(&InvokeQuery), asCALL_GENERIC),
                  "InvokeQuery", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int InvokeCommand(const string &in apiId, const string &in endpoint, Input@ input, Record@ &out record)",
                                                           BML_AS_GENERIC_FUNCTION(&InvokeCommand), asCALL_GENERIC),
                  "InvokeCommand", errorMessage) ||
        !Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage)) {
        return asERROR;
    }
    return asSUCCESS;
}

} // namespace BML
