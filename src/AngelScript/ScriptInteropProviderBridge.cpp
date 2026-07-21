#include "ScriptInteropProviderBridge.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "BML/InteropApi.h"

#include "BuiltinInteropApis.h"
#include "InteropRegistry.h"
#include "ModContext.h"
#include "ScriptInteropArray.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"
#include "ScriptStringInterop.h"

#ifdef GetObject
#undef GetObject
#endif

namespace BML {
namespace {

std::string g_ProviderBridgeRegistrationError;

bool IsInteropKey(const std::string &value) {
    if (value.empty())
        return false;
    for (const unsigned char ch : value) {
        if (!(std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-'))
            return false;
    }
    return true;
}

bool Register(asIScriptEngine *engine, int status, const char *declaration, const char **errorMessage) {
    if (status >= 0)
        return true;
    g_ProviderBridgeRegistrationError = "Failed to register Interop provider declaration: ";
    g_ProviderBridgeRegistrationError += declaration ? declaration : "";
    g_ProviderBridgeRegistrationError += " returned ";
    g_ProviderBridgeRegistrationError += std::to_string(status);
    if (engine)
        engine->SetDefaultNamespace("");
    if (errorMessage)
        *errorMessage = g_ProviderBridgeRegistrationError.c_str();
    return false;
}

template <typename T>
void ConstructValue(T *self) {
    new (self) T();
}

template <typename T>
void CopyConstructValue(const T &other, T *self) {
    new (self) T(other);
}

template <typename T>
void DestructValue(T *self) {
    self->~T();
}

template <typename T>
T &AssignValue(const T &other, T *self) {
    *self = other;
    return *self;
}

void ConstructObjectRef(BML_ObjectRef *self) { ConstructValue(self); }
void CopyConstructObjectRef(const BML_ObjectRef &other, BML_ObjectRef *self) { CopyConstructValue(other, self); }
void DestructObjectRef(BML_ObjectRef *self) { DestructValue(self); }
BML_ObjectRef &AssignObjectRef(const BML_ObjectRef &other, BML_ObjectRef *self) { return AssignValue(other, self); }

class ScriptInteropApiBuilder final {
public:
    struct EndpointStorage;

    ScriptInteropApiBuilder(std::string apiId, uint32_t major, uint32_t minor, uint64_t hash)
        : m_ApiId(std::move(apiId)), m_Major(major), m_Minor(minor), m_Hash(hash) {}

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    int AddSchema(uint32_t id, const std::string &name) {
        if (m_Frozen)
            return BML_ERROR_FROZEN;
        if (id == 0 || !IsInteropKey(name))
            return BML_ERROR_INVALID_PARAMETER;
        const auto duplicate = std::find_if(m_Schemas.begin(), m_Schemas.end(), [&](const SchemaStorage &record) {
            return record.Id == id || record.Name == name;
        });
        if (duplicate != m_Schemas.end())
            return BML_ERROR_ALREADY_EXISTS;
        try {
            m_Schemas.push_back({id, name});
            m_DescriptorReady = false;
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    int AddField(uint32_t schemaId,
                 uint32_t id,
                 const std::string &name,
                 int type,
                 bool optional) {
        if (m_Frozen)
            return BML_ERROR_FROZEN;
        if (id == 0 || !IsInteropKey(name) ||
            type < BML_INTEROP_FIELD_BOOL || type > BML_INTEROP_FIELD_MAT4_ARRAY) {
            return BML_ERROR_INVALID_PARAMETER;
        }
        SchemaStorage *record = FindSchema(schemaId);
        if (!record)
            return BML_ERROR_NOT_FOUND;
        const auto duplicate = std::find_if(record->Fields.begin(), record->Fields.end(), [&](const FieldStorage &field) {
            return field.Id == id || field.Name == name;
        });
        if (duplicate != record->Fields.end())
            return BML_ERROR_ALREADY_EXISTS;
        try {
            record->Fields.push_back({id, name, static_cast<BML_INTEROP_FIELD_TYPE>(type), optional});
            m_DescriptorReady = false;
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    int AddEndpoint(const std::string &name,
                  int kind,
                  uint32_t inputSchema,
                  uint32_t outputSchema,
                  bool requiresProbe) {
        if (m_Frozen)
            return BML_ERROR_FROZEN;
        if (!IsInteropKey(name) || kind < BML_INTEROP_ENDPOINT_RESOURCE || kind > BML_INTEROP_ENDPOINT_COMMAND ||
            outputSchema == 0 || !FindSchema(outputSchema)) {
            return BML_ERROR_INVALID_PARAMETER;
        }
        const BML_INTEROP_ENDPOINT_KIND endpointKind = static_cast<BML_INTEROP_ENDPOINT_KIND>(kind);
        const bool acceptsInput = endpointKind == BML_INTEROP_ENDPOINT_QUERY || endpointKind == BML_INTEROP_ENDPOINT_COMMAND;
        if (acceptsInput != (inputSchema != 0) || (inputSchema != 0 && !FindSchema(inputSchema)))
            return BML_ERROR_INVALID_PARAMETER;
        const auto duplicate = std::find_if(m_Endpoints.begin(), m_Endpoints.end(), [&](const EndpointStorage &endpoint) {
            return endpoint.Name == name;
        });
        if (duplicate != m_Endpoints.end())
            return BML_ERROR_ALREADY_EXISTS;
        try {
            m_Endpoints.push_back({name, endpointKind, inputSchema, outputSchema, requiresProbe});
            m_DescriptorReady = false;
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    int AddCompatibleApiHash(uint64_t hash) {
        if (m_Frozen)
            return BML_ERROR_FROZEN;
        if (hash == 0 || hash == m_Hash)
            return BML_ERROR_INVALID_PARAMETER;
        if (std::find(m_CompatibleApiHashes.begin(), m_CompatibleApiHashes.end(), hash) != m_CompatibleApiHashes.end())
            return BML_ERROR_ALREADY_EXISTS;
        try {
            m_CompatibleApiHashes.push_back(hash);
            m_DescriptorReady = false;
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    bool IsFrozen() const { return m_Frozen; }
    const std::string &ApiId() const { return m_ApiId; }
    const std::vector<EndpointStorage> &Endpoints() const { return m_Endpoints; }
    const BML_InteropApiDescriptor *Descriptor() const {
        return m_DescriptorReady ? &m_Descriptor : nullptr;
    }

    int Prepare() {
        if (m_DescriptorReady)
            return BML_OK;
        if (!IsInteropKey(m_ApiId) || m_Major == 0 || m_Hash == 0 || m_Schemas.empty() || m_Endpoints.empty())
            return BML_ERROR_INTEROP_API_INVALID;
        try {
            m_RawSchemas.clear();
            m_RawSchemas.reserve(m_Schemas.size());
            for (SchemaStorage &record : m_Schemas) {
                record.RawFields.clear();
                record.RawFields.reserve(record.Fields.size());
                for (const FieldStorage &field : record.Fields) {
                    record.RawFields.push_back({field.Id, field.Name.c_str(), field.Type, field.Optional ? 1 : 0});
                }
                m_RawSchemas.push_back({record.Id,
                                        record.Name.c_str(),
                                        record.RawFields.empty() ? nullptr : record.RawFields.data(),
                                        record.RawFields.size()});
            }
            m_RawEndpoints.clear();
            m_RawEndpoints.reserve(m_Endpoints.size());
            for (const EndpointStorage &endpoint : m_Endpoints) {
                m_RawEndpoints.push_back({endpoint.Name.c_str(),
                                        endpoint.Kind,
                                        endpoint.InputSchema,
                                        endpoint.OutputSchema,
                                        endpoint.RequiresProbe ? 1 : 0});
            }
            m_Descriptor = {sizeof(BML_InteropApiDescriptor),
                            m_ApiId.c_str(),
                            m_Major,
                            m_Minor,
                            m_Hash,
                            m_RawSchemas.data(),
                            m_RawSchemas.size(),
                            m_RawEndpoints.data(),
                            m_RawEndpoints.size(),
                            m_CompatibleApiHashes.empty() ? nullptr : m_CompatibleApiHashes.data(),
                            m_CompatibleApiHashes.size()};
            m_DescriptorReady = true;
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    int Freeze() {
        const int status = Prepare();
        if (status == BML_OK)
            m_Frozen = true;
        return status;
    }

private:
    struct FieldStorage {
        uint32_t Id = 0;
        std::string Name;
        BML_INTEROP_FIELD_TYPE Type = BML_INTEROP_FIELD_INT;
        bool Optional = false;
    };

public:
    struct EndpointStorage {
        std::string Name;
        BML_INTEROP_ENDPOINT_KIND Kind = BML_INTEROP_ENDPOINT_RESOURCE;
        uint32_t InputSchema = 0;
        uint32_t OutputSchema = 0;
        bool RequiresProbe = false;
    };

private:
    struct SchemaStorage {
        uint32_t Id = 0;
        std::string Name;
        std::vector<FieldStorage> Fields;
        std::vector<BML_InteropFieldDescriptor> RawFields;
    };

    SchemaStorage *FindSchema(uint32_t id) {
        const auto found = std::find_if(m_Schemas.begin(), m_Schemas.end(), [id](const SchemaStorage &record) {
            return record.Id == id;
        });
        return found == m_Schemas.end() ? nullptr : &*found;
    }

    int m_RefCount = 1;
    std::string m_ApiId;
    uint32_t m_Major = 0;
    uint32_t m_Minor = 0;
    uint64_t m_Hash = 0;
    bool m_Frozen = false;
    bool m_DescriptorReady = false;
    std::vector<SchemaStorage> m_Schemas;
    std::vector<EndpointStorage> m_Endpoints;
    std::vector<BML_InteropSchemaDescriptor> m_RawSchemas;
    std::vector<BML_InteropEndpointDescriptor> m_RawEndpoints;
    std::vector<uint64_t> m_CompatibleApiHashes;
    BML_InteropApiDescriptor m_Descriptor{};
};

class ScriptInteropRecordWriter;

class ScriptInteropRequest final {
public:
    ScriptInteropRequest(ScriptMod *owner, const BML_InteropProviderRequest *request)
        : m_Owner(owner) {
        if (!request)
            return;
        m_ApiId = request->ApiId ? request->ApiId : "";
        m_Endpoint = request->Endpoint ? request->Endpoint : "";
        m_Kind = static_cast<int>(request->Kind);
        m_Object = request->Object;
        m_Offset = request->Offset;
        m_Limit = request->Limit;
        m_ConsumerId = request->ConsumerId ? request->ConsumerId : "";
        m_Input = request->Input;
    }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    void Invalidate() { m_Active = false; m_Input = nullptr; }
    bool IsActive() const { return m_Active; }
    const std::string &GetApiId() const { return m_ApiId; }
    const std::string &GetEndpoint() const { return m_Endpoint; }
    const std::string &GetConsumerId() const { return m_ConsumerId; }
    int GetKind() const { return m_Kind; }
    BML_ObjectRef GetObject() const { return m_Object; }
    uint64_t GetOffset() const { return m_Offset; }
    uint32_t GetLimit() const { return m_Limit; }

    int GetInputBool(uint32_t field, bool &out) const;
    int GetInputInt(uint32_t field, int &out) const;
    int GetInputFloat(uint32_t field, float &out) const;
    int GetInputString(uint32_t field, std::string &out) const;
    int GetInputObject(uint32_t field, BML_ObjectRef &out) const;
    int GetInputVec2(uint32_t field, BML_Vec2 &out) const;
    int GetInputVec3(uint32_t field, BML_Vec3 &out) const;
    int GetInputMat4(uint32_t field, BML_Mat4 &out) const;
    int BorrowInput(uint32_t field,
                    BML_INTEROP_FIELD_TYPE expectedType,
                    const void **outData,
                    size_t *outCount,
                    size_t *outElementSize) const;
    int InputStringArrayCount(uint32_t field, size_t *outCount) const;
    int InputStringArrayItem(uint32_t field,
                             size_t itemIndex,
                             char *buffer,
                             size_t bufferSize,
                             size_t *outRequiredSize) const;
    ScriptMod *Owner() const { return m_Owner; }

private:
    InteropRegistry *Apis() const {
        return m_Owner && m_Owner->GetModContext() ? &m_Owner->GetModContext()->GetInteropRegistry() : nullptr;
    }

    int m_RefCount = 1;
    ScriptMod *m_Owner = nullptr;
    bool m_Active = true;
    std::string m_ApiId;
    std::string m_Endpoint;
    std::string m_ConsumerId;
    int m_Kind = 0;
    BML_ObjectRef m_Object{};
    uint64_t m_Offset = 0;
    uint32_t m_Limit = 0;
    const BML_InteropRecordView *m_Input = nullptr;
};

class ScriptInteropRecordWriter final {
public:
    ScriptInteropRecordWriter(ScriptMod *owner, BML_InteropRecordBuilder *record, bool ownsRecord)
        : m_Owner(owner), m_Record(record), m_OwnsRecord(ownsRecord) {}
    ~ScriptInteropRecordWriter() { DestroyOwnedRecord(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    void Invalidate() {
        if (!m_OwnsRecord)
            m_Record = nullptr;
        m_Active = false;
    }
    bool IsActive() const { return m_Active && m_Record != nullptr; }

    int SetBool(uint32_t field, bool value) {
        const int raw = value ? 1 : 0;
        return Set(field, BML_INTEROP_FIELD_BOOL, &raw, 1);
    }
    int SetInt(uint32_t field, int value) { return Set(field, BML_INTEROP_FIELD_INT, &value, 1); }
    int SetFloat(uint32_t field, float value) { return Set(field, BML_INTEROP_FIELD_FLOAT, &value, 1); }
    int SetString(uint32_t field, const std::string &value) {
        return Set(field, BML_INTEROP_FIELD_STRING, value.data(), value.size());
    }
    int SetObject(uint32_t field, const BML_ObjectRef &value) {
        return Set(field, BML_INTEROP_FIELD_OBJECT, &value, 1);
    }
    int SetVec2(uint32_t field, const BML_Vec2 &value) { return Set(field, BML_INTEROP_FIELD_VEC2, &value, 1); }
    int SetVec3(uint32_t field, const BML_Vec3 &value) { return Set(field, BML_INTEROP_FIELD_VEC3, &value, 1); }
    int SetMat4(uint32_t field, const BML_Mat4 &value) { return Set(field, BML_INTEROP_FIELD_MAT4, &value, 1); }
    int SetArray(uint32_t field, BML_INTEROP_FIELD_TYPE type, const void *data, size_t count) {
        return Set(field, type, data, count);
    }
    int SetStringArray(uint32_t field, const std::vector<std::string> &values) {
        if (!IsActive() || !m_Owner || !m_Owner->GetModContext())
            return BML_ERROR_INTEROP_RECORD_INVALID;
        try {
            std::vector<const char *> pointers;
            std::vector<size_t> sizes;
            pointers.reserve(values.size());
            sizes.reserve(values.size());
            for (const std::string &value : values) {
                pointers.push_back(value.data());
                sizes.push_back(value.size());
            }
            return m_Owner->GetModContext()->GetInteropRegistry().BuilderSetStringArray(
                m_Record,
                field,
                pointers.empty() ? nullptr : pointers.data(),
                sizes.empty() ? nullptr : sizes.data(),
                values.size());
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }
    ScriptMod *Owner() const { return m_Owner; }

    int Publish() {
        if (RejectScriptRestrictedHostCall("BML::Interop::RecordWriter::Publish"))
            return BML_ERROR_FROZEN;
        if (!m_OwnsRecord || !IsActive() || !m_Owner || !m_Owner->GetModContext())
            return BML_ERROR_INTEROP_RECORD_INVALID;
        const int status = m_Owner->GetModContext()->GetInteropRegistry().Publish(m_Owner->GetID(), m_Record);
        if (status == BML_OK) {
            DestroyOwnedRecord();
            m_Active = false;
        }
        return status;
    }

private:
    int Set(uint32_t field, BML_INTEROP_FIELD_TYPE type, const void *data, size_t count) {
        if (!IsActive() || !m_Owner || !m_Owner->GetModContext())
            return BML_ERROR_INTEROP_RECORD_INVALID;
        return m_Owner->GetModContext()->GetInteropRegistry().BuilderSetValue(m_Record, field, type, data, count);
    }

    void DestroyOwnedRecord() {
        if (!m_OwnsRecord || !m_Record)
            return;
        if (m_Owner && m_Owner->GetModContext())
            (void)m_Owner->GetModContext()->GetInteropRegistry().DestroyRecordBuilder(m_Record);
        m_Record = nullptr;
    }

    int m_RefCount = 1;
    ScriptMod *m_Owner = nullptr;
    BML_InteropRecordBuilder *m_Record = nullptr;
    bool m_OwnsRecord = false;
    bool m_Active = true;
};

class ScriptInteropPageWriter final {
public:
    ScriptInteropPageWriter(ScriptMod *owner, BML_InteropPageBuilder *page)
        : m_Owner(owner), m_Page(page) {}
    ~ScriptInteropPageWriter() { Invalidate(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsActive() const { return m_Active && m_Page != nullptr; }
    ScriptInteropRecordWriter *Append() {
        if (!IsActive() || !m_Owner || !m_Owner->GetModContext())
            return nullptr;
        BML_InteropRecordBuilder *record = m_Owner->GetModContext()->GetInteropRegistry().PageAppend(m_Page);
        if (!record)
            return nullptr;
        auto *writer = new (std::nothrow) ScriptInteropRecordWriter(m_Owner, record, false);
        if (!writer)
            return nullptr;
        try {
            writer->AddRef();
            m_Children.push_back(writer);
            return writer;
        } catch (const std::bad_alloc &) {
            writer->Release();
            return nullptr;
        }
    }
    void SetComplete(bool complete) {
        if (IsActive() && m_Owner && m_Owner->GetModContext())
            m_Owner->GetModContext()->GetInteropRegistry().PageFinish(m_Page, complete ? 1 : 0);
    }
    void Invalidate() {
        if (!m_Active)
            return;
        m_Active = false;
        m_Page = nullptr;
        for (ScriptInteropRecordWriter *writer : m_Children) {
            writer->Invalidate();
            writer->Release();
        }
        m_Children.clear();
    }

private:
    int m_RefCount = 1;
    ScriptMod *m_Owner = nullptr;
    BML_InteropPageBuilder *m_Page = nullptr;
    bool m_Active = true;
    std::vector<ScriptInteropRecordWriter *> m_Children;
};

int ScriptInteropRequest::GetInputBool(uint32_t field, bool &out) const {
    out = false;
    int value = 0;
    InteropRegistry *registry = Apis();
    const int status = m_Active && m_Input && registry
                           ? registry->RecordViewGetBool(m_Input, field, &value)
                           : BML_ERROR_INTEROP_RECORD_INVALID;
    if (status == BML_OK)
        out = value != 0;
    return status;
}

int ScriptInteropRequest::GetInputInt(uint32_t field, int &out) const {
    out = 0;
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetInt(m_Input, field, &out)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::GetInputFloat(uint32_t field, float &out) const {
    out = 0.0f;
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetFloat(m_Input, field, &out)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::GetInputString(uint32_t field, std::string &out) const {
    out.clear();
    InteropRegistry *registry = Apis();
    if (!m_Active || !m_Input || !registry)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    size_t required = 0;
    int status = registry->RecordViewGetString(m_Input, field, nullptr, 0, &required);
    if (status != BML_OK)
        return status;
    try {
        std::string storage(required ? required : 1, '\0');
        status = registry->RecordViewGetString(m_Input, field, storage.data(), storage.size(), &required);
        if (status == BML_OK)
            out.assign(storage.data(), required > 0 ? required - 1 : 0);
        return status;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

int ScriptInteropRequest::GetInputObject(uint32_t field, BML_ObjectRef &out) const {
    out = {};
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetObject(m_Input, field, &out)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::GetInputVec2(uint32_t field, BML_Vec2 &out) const {
    out = {};
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetVec2(m_Input, field, &out)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::GetInputVec3(uint32_t field, BML_Vec3 &out) const {
    out = {};
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetVec3(m_Input, field, &out)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::GetInputMat4(uint32_t field, BML_Mat4 &out) const {
    out = {};
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetMat4(m_Input, field, &out)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::BorrowInput(uint32_t field,
                                      BML_INTEROP_FIELD_TYPE expectedType,
                                      const void **outData,
                                      size_t *outCount,
                                      size_t *outElementSize) const {
    if (outData) *outData = nullptr;
    if (outCount) *outCount = 0;
    if (outElementSize) *outElementSize = 0;
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewBorrowValue(m_Input,
                                                   field,
                                                   expectedType,
                                                   outData,
                                                   outCount,
                                                   outElementSize)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::InputStringArrayCount(uint32_t field, size_t *outCount) const {
    if (outCount) *outCount = 0;
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetStringArrayCount(m_Input, field, outCount)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

int ScriptInteropRequest::InputStringArrayItem(uint32_t field,
                                                size_t itemIndex,
                                                char *buffer,
                                                size_t bufferSize,
                                                size_t *outRequiredSize) const {
    if (outRequiredSize) *outRequiredSize = 0;
    InteropRegistry *registry = Apis();
    return m_Active && m_Input && registry
               ? registry->RecordViewGetStringArrayItem(m_Input,
                                                          field,
                                                          itemIndex,
                                                          buffer,
                                                          bufferSize,
                                                          outRequiredSize)
               : BML_ERROR_INTEROP_RECORD_INVALID;
}

void ReturnInteropStatus(asIScriptGeneric *gen, int status) {
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

template <typename T>
int ReadFixedScriptArray(ScriptMod *owner,
                         asIScriptGeneric *gen,
                         asUINT argumentIndex,
                         const char *elementDeclaration,
                         std::vector<T> &out) {
    return ScriptInteropArray::ReadFixed(owner, gen, argumentIndex, elementDeclaration, out);
}

int ReadBoolScriptArray(ScriptMod *owner,
                        asIScriptGeneric *gen,
                        asUINT argumentIndex,
                        std::vector<int> &out) {
    return ScriptInteropArray::ReadBool(owner, gen, argumentIndex, out);
}

int ReadStringScriptArray(ScriptMod *owner,
                          asIScriptGeneric *gen,
                          asUINT argumentIndex,
                          std::vector<std::string> &out) {
    return ScriptInteropArray::ReadString(owner, gen, argumentIndex, out);
}

template <typename T>
int WriteFixedScriptArray(ScriptMod *owner,
                          asIScriptGeneric *gen,
                          asUINT argumentIndex,
                          const char *elementDeclaration,
                          const T *values,
                          size_t count) {
    return ScriptInteropArray::WriteFixed(owner, gen, argumentIndex, elementDeclaration, values, count);
}

int WriteBoolScriptArray(ScriptMod *owner,
                         asIScriptGeneric *gen,
                         asUINT argumentIndex,
                         const int *values,
                         size_t count) {
    return ScriptInteropArray::WriteBool(owner, gen, argumentIndex, values, count);
}

int WriteStringScriptArray(ScriptMod *owner,
                           asIScriptGeneric *gen,
                           asUINT argumentIndex,
                           const std::vector<std::string> &values) {
    return ScriptInteropArray::WriteString(owner, gen, argumentIndex, values);
}

int ReadInputStringArray(const ScriptInteropRequest &request,
                         uint32_t field,
                         std::vector<std::string> &out) {
    size_t count = 0;
    int status = request.InputStringArrayCount(field, &count);
    if (status != BML_OK)
        return status;
    try {
        std::vector<std::string> values;
        values.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            size_t required = 0;
            status = request.InputStringArrayItem(field, index, nullptr, 0, &required);
            if (status != BML_OK)
                return status;
            std::string storage(required ? required : 1, '\0');
            status = request.InputStringArrayItem(field,
                                                  index,
                                                  storage.data(),
                                                  storage.size(),
                                                  &required);
            if (status != BML_OK)
                return status;
            values.emplace_back(storage.c_str());
        }
        out = std::move(values);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

template <BML_INTEROP_FIELD_TYPE FieldType, typename T>
void WriterSetFixedArray(asIScriptGeneric *gen, const char *elementDeclaration) {
    auto *writer = static_cast<ScriptInteropRecordWriter *>(gen ? gen->GetObject() : nullptr);
    int status = writer ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<T> values;
        if (status == BML_OK)
            status = ReadFixedScriptArray(writer->Owner(), gen, 1, elementDeclaration, values);
        if (status == BML_OK)
            status = writer->SetArray(gen->GetArgDWord(0), FieldType,
                                      values.empty() ? nullptr : values.data(), values.size());
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    ReturnInteropStatus(gen, status);
}

void WriterSetBoolArray(asIScriptGeneric *gen) {
    auto *writer = static_cast<ScriptInteropRecordWriter *>(gen ? gen->GetObject() : nullptr);
    int status = writer ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<int> values;
        if (status == BML_OK)
            status = ReadBoolScriptArray(writer->Owner(), gen, 1, values);
        if (status == BML_OK)
            status = writer->SetArray(gen->GetArgDWord(0), BML_INTEROP_FIELD_BOOL_ARRAY,
                                      values.empty() ? nullptr : values.data(), values.size());
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    ReturnInteropStatus(gen, status);
}

void WriterSetStringArray(asIScriptGeneric *gen) {
    auto *writer = static_cast<ScriptInteropRecordWriter *>(gen ? gen->GetObject() : nullptr);
    int status = writer ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<std::string> values;
        if (status == BML_OK)
            status = ReadStringScriptArray(writer->Owner(), gen, 1, values);
        if (status == BML_OK)
            status = writer->SetStringArray(gen->GetArgDWord(0), values);
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    ReturnInteropStatus(gen, status);
}

template <BML_INTEROP_FIELD_TYPE FieldType, typename T>
void RequestGetFixedArray(asIScriptGeneric *gen, const char *elementDeclaration) {
    auto *request = static_cast<ScriptInteropRequest *>(gen ? gen->GetObject() : nullptr);
    int status = request ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    const void *data = nullptr;
    size_t count = 0;
    size_t elementSize = 0;
    if (status == BML_OK)
        status = request->BorrowInput(gen->GetArgDWord(0), FieldType, &data, &count, &elementSize);
    if (status == BML_OK && (elementSize != sizeof(T) || (count != 0 && !data)))
        status = BML_ERROR_INTEROP_RECORD_INVALID;
    if (status == BML_OK)
        status = WriteFixedScriptArray(request->Owner(), gen, 1, elementDeclaration,
                                       static_cast<const T *>(data), count);
    ReturnInteropStatus(gen, status);
}

void RequestGetBoolArray(asIScriptGeneric *gen) {
    auto *request = static_cast<ScriptInteropRequest *>(gen ? gen->GetObject() : nullptr);
    int status = request ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    const void *data = nullptr;
    size_t count = 0;
    size_t elementSize = 0;
    if (status == BML_OK)
        status = request->BorrowInput(gen->GetArgDWord(0), BML_INTEROP_FIELD_BOOL_ARRAY,
                                      &data, &count, &elementSize);
    if (status == BML_OK && (elementSize != sizeof(int) || (count != 0 && !data)))
        status = BML_ERROR_INTEROP_RECORD_INVALID;
    if (status == BML_OK)
        status = WriteBoolScriptArray(request->Owner(), gen, 1, static_cast<const int *>(data), count);
    ReturnInteropStatus(gen, status);
}

void RequestGetStringArray(asIScriptGeneric *gen) {
    auto *request = static_cast<ScriptInteropRequest *>(gen ? gen->GetObject() : nullptr);
    int status = request ? BML_OK : BML_ERROR_INTEROP_RECORD_INVALID;
    try {
        std::vector<std::string> values;
        if (status == BML_OK)
            status = ReadInputStringArray(*request, gen->GetArgDWord(0), values);
        if (status == BML_OK)
            status = WriteStringScriptArray(request->Owner(), gen, 1, values);
    } catch (const std::bad_alloc &) {
        status = BML_ERROR_OUT_OF_MEMORY;
    }
    ReturnInteropStatus(gen, status);
}

void WriterSetIntArray(asIScriptGeneric *gen) {
    WriterSetFixedArray<BML_INTEROP_FIELD_INT_ARRAY, int>(gen, "int");
}
void WriterSetFloatArray(asIScriptGeneric *gen) {
    WriterSetFixedArray<BML_INTEROP_FIELD_FLOAT_ARRAY, float>(gen, "float");
}
void WriterSetObjectArray(asIScriptGeneric *gen) {
    WriterSetFixedArray<BML_INTEROP_FIELD_OBJECT_ARRAY, BML_ObjectRef>(gen, "BML::Interop::ObjectRef");
}
void WriterSetVec2Array(asIScriptGeneric *gen) {
    WriterSetFixedArray<BML_INTEROP_FIELD_VEC2_ARRAY, BML_Vec2>(gen, "BML::Vec2");
}
void WriterSetVec3Array(asIScriptGeneric *gen) {
    WriterSetFixedArray<BML_INTEROP_FIELD_VEC3_ARRAY, BML_Vec3>(gen, "BML::Vec3");
}
void WriterSetMat4Array(asIScriptGeneric *gen) {
    WriterSetFixedArray<BML_INTEROP_FIELD_MAT4_ARRAY, BML_Mat4>(gen, "BML::Mat4");
}

void RequestGetIntArray(asIScriptGeneric *gen) {
    RequestGetFixedArray<BML_INTEROP_FIELD_INT_ARRAY, int>(gen, "int");
}
void RequestGetFloatArray(asIScriptGeneric *gen) {
    RequestGetFixedArray<BML_INTEROP_FIELD_FLOAT_ARRAY, float>(gen, "float");
}
void RequestGetObjectArray(asIScriptGeneric *gen) {
    RequestGetFixedArray<BML_INTEROP_FIELD_OBJECT_ARRAY, BML_ObjectRef>(gen, "BML::Interop::ObjectRef");
}
void RequestGetVec2Array(asIScriptGeneric *gen) {
    RequestGetFixedArray<BML_INTEROP_FIELD_VEC2_ARRAY, BML_Vec2>(gen, "BML::Vec2");
}
void RequestGetVec3Array(asIScriptGeneric *gen) {
    RequestGetFixedArray<BML_INTEROP_FIELD_VEC3_ARRAY, BML_Vec3>(gen, "BML::Vec3");
}
void RequestGetMat4Array(asIScriptGeneric *gen) {
    RequestGetFixedArray<BML_INTEROP_FIELD_MAT4_ARRAY, BML_Mat4>(gen, "BML::Mat4");
}

struct ProviderCallArgs {
    ScriptInteropRequest *Request = nullptr;
    ScriptInteropRecordWriter *Record = nullptr;
    ScriptInteropPageWriter *Page = nullptr;
    int Result = BML_ERROR_SCRIPT_EXECUTION;
};

int WriteProbeArgs(asIScriptContext *context, void *userdata) {
    const auto *args = static_cast<const ProviderCallArgs *>(userdata);
    return context->SetArgObject(0, args ? args->Request : nullptr);
}

int WriteReadArgs(asIScriptContext *context, void *userdata) {
    const auto *args = static_cast<const ProviderCallArgs *>(userdata);
    int status = context->SetArgObject(0, args ? args->Request : nullptr);
    if (status >= 0)
        status = context->SetArgObject(1, args ? args->Record : nullptr);
    return status;
}

int WritePageArgs(asIScriptContext *context, void *userdata) {
    const auto *args = static_cast<const ProviderCallArgs *>(userdata);
    int status = context->SetArgObject(0, args ? args->Request : nullptr);
    if (status >= 0)
        status = context->SetArgObject(1, args ? args->Page : nullptr);
    return status;
}

int ReadProviderResult(asIScriptContext *context, void *userdata) {
    auto *args = static_cast<ProviderCallArgs *>(userdata);
    if (!args || !context)
        return asERROR;
    args->Result = static_cast<int>(context->GetReturnDWord());
    return asSUCCESS;
}

class ScriptInteropProvider final {
public:
    explicit ScriptInteropProvider(ScriptMod *owner) : m_Owner(owner) {}
    ~ScriptInteropProvider() { Deactivate(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsRegistered() const { return m_Registered; }
    ScriptMod *Owner() const { return m_Owner; }
    const char *ApiId() const { return m_Api ? m_Api->ApiId().c_str() : nullptr; }

    int SetProbe(asIScriptFunction *callback) {
        if (m_Registered)
            return BML_ERROR_FROZEN;
        return ReplaceCallback(m_Probe, callback);
    }

    int SetRead(const std::string &endpoint, asIScriptFunction *callback) {
        return SetEndpointCallback(m_ReadCallbacks, endpoint, callback);
    }

    int SetPage(const std::string &endpoint, asIScriptFunction *callback) {
        return SetEndpointCallback(m_PageCallbacks, endpoint, callback);
    }

    ScriptInteropRecordWriter *BeginStreamRecord(const std::string &endpoint) {
        if (RejectScriptRestrictedHostCall("BML::Interop::Provider::BeginStreamRecord"))
            return nullptr;
        if (!m_Registered || !m_Api || !m_Owner || !m_Owner->GetModContext() || !IsInteropKey(endpoint))
            return nullptr;
        BML_InteropRecordBuilder *record = nullptr;
        const int status = m_Owner->GetModContext()->GetInteropRegistry().CreateStreamRecord(
            m_Owner->GetID(), m_Api->ApiId().c_str(), endpoint.c_str(), &record);
        if (status != BML_OK || !record)
            return nullptr;
        auto *writer = new (std::nothrow) ScriptInteropRecordWriter(m_Owner, record, true);
        if (!writer)
            (void)m_Owner->GetModContext()->GetInteropRegistry().DestroyRecordBuilder(record);
        return writer;
    }

    int Validate(const ScriptInteropApiBuilder &api) const {
        if (!m_Owner || !m_Owner->GetModContext())
            return BML_ERROR_INTEROP_UNSUPPORTED;
        for (const ScriptInteropApiBuilder::EndpointStorage &endpoint : api.Endpoints()) {
            if (endpoint.RequiresProbe && !m_Probe)
                return BML_ERROR_INTEROP_UNSUPPORTED;
            switch (endpoint.Kind) {
            case BML_INTEROP_ENDPOINT_RESOURCE:
            case BML_INTEROP_ENDPOINT_COMPONENT:
            case BML_INTEROP_ENDPOINT_QUERY:
            case BML_INTEROP_ENDPOINT_COMMAND:
                if (m_ReadCallbacks.find(endpoint.Name) == m_ReadCallbacks.end())
                    return BML_ERROR_INTEROP_UNSUPPORTED;
                break;
            case BML_INTEROP_ENDPOINT_COLLECTION:
                if (m_PageCallbacks.find(endpoint.Name) == m_PageCallbacks.end())
                    return BML_ERROR_INTEROP_UNSUPPORTED;
                break;
            case BML_INTEROP_ENDPOINT_STREAM:
                break;
            default:
                return BML_ERROR_INTEROP_API_INVALID;
            }
        }
        return BML_OK;
    }

    void Activate(ScriptInteropApiBuilder *api) {
        if (m_Api == api)
            return;
        if (m_Api)
            m_Api->Release();
        m_Api = api;
        if (m_Api)
            m_Api->AddRef();
        m_Registered = m_Api != nullptr;
    }

    void Deactivate() {
        m_Registered = false;
        if (m_Api) {
            m_Api->Release();
            m_Api = nullptr;
        }
        ReleaseCallback(m_Probe);
        ReleaseCallbackMap(m_ReadCallbacks);
        ReleaseCallbackMap(m_PageCallbacks);
    }

    static int ProbeCallback(const BML_InteropProviderRequest *request, void *userdata) {
        auto *provider = static_cast<ScriptInteropProvider *>(userdata);
        return provider ? provider->CallProbe(request) : BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    }
    static int ReadCallback(const BML_InteropProviderRequest *request,
                            BML_InteropRecordBuilder *record,
                            void *userdata) {
        auto *provider = static_cast<ScriptInteropProvider *>(userdata);
        return provider ? provider->CallRead(request, record) : BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    }
    static int PageCallback(const BML_InteropProviderRequest *request,
                            BML_InteropPageBuilder *page,
                            void *userdata) {
        auto *provider = static_cast<ScriptInteropProvider *>(userdata);
        return provider ? provider->CallPage(request, page) : BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    }

    static BML_InteropProviderCallbacks Callbacks() {
        BML_InteropProviderCallbacks callbacks{};
        callbacks.Size = sizeof(callbacks);
        callbacks.Probe = &ProbeCallback;
        callbacks.ReadResource = &ReadCallback;
        callbacks.ReadComponent = &ReadCallback;
        callbacks.ReadCollection = &PageCallback;
        callbacks.InvokeQuery = &ReadCallback;
        callbacks.InvokeCommand = &ReadCallback;
        return callbacks;
    }

private:
    int SetEndpointCallback(std::unordered_map<std::string, asIScriptFunction *> &callbacks,
                          const std::string &endpoint,
                          asIScriptFunction *callback) {
        if (m_Registered)
            return BML_ERROR_FROZEN;
        if (!IsInteropKey(endpoint) || !callback)
            return BML_ERROR_INVALID_PARAMETER;
        try {
            auto found = callbacks.find(endpoint);
            if (found == callbacks.end()) {
                callback->AddRef();
                callbacks.emplace(endpoint, callback);
            } else {
                ReplaceCallback(found->second, callback);
            }
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    static int ReplaceCallback(asIScriptFunction *&target, asIScriptFunction *callback) {
        if (!callback)
            return BML_ERROR_INVALID_PARAMETER;
        callback->AddRef();
        ReleaseCallback(target);
        target = callback;
        return BML_OK;
    }

    static void ReleaseCallback(asIScriptFunction *&callback) {
        if (callback)
            callback->Release();
        callback = nullptr;
    }

    static void ReleaseCallbackMap(std::unordered_map<std::string, asIScriptFunction *> &callbacks) {
        for (auto &[endpoint, callback] : callbacks)
            ReleaseCallback(callback);
        callbacks.clear();
    }

    asIScriptFunction *FindRead(const char *endpoint) const {
        const auto found = m_ReadCallbacks.find(endpoint ? endpoint : "");
        return found == m_ReadCallbacks.end() ? nullptr : found->second;
    }

    asIScriptFunction *FindPage(const char *endpoint) const {
        const auto found = m_PageCallbacks.find(endpoint ? endpoint : "");
        return found == m_PageCallbacks.end() ? nullptr : found->second;
    }

    int Execute(asIScriptFunction *function,
                const BML_InteropProviderRequest *request,
                BML_InteropRecordBuilder *record,
                BML_InteropPageBuilder *page,
                ScriptFunctionArgWriter writeArgs) {
        if (!m_Registered || !function || !request || !m_Owner)
            return BML_ERROR_INTEROP_PROVIDER_UNLOADED;

        auto *scriptRequest = new (std::nothrow) ScriptInteropRequest(m_Owner, request);
        if (!scriptRequest)
            return BML_ERROR_OUT_OF_MEMORY;
        auto *writer = record ? new (std::nothrow) ScriptInteropRecordWriter(m_Owner, record, false) : nullptr;
        auto *pageWriter = page ? new (std::nothrow) ScriptInteropPageWriter(m_Owner, page) : nullptr;
        if ((record && !writer) || (page && !pageWriter)) {
            if (writer) writer->Release();
            if (pageWriter) pageWriter->Release();
            scriptRequest->Release();
            return BML_ERROR_OUT_OF_MEMORY;
        }

        ProviderCallArgs args{scriptRequest, writer, pageWriter, BML_ERROR_SCRIPT_EXECUTION};
        ScriptFunctionCall call;
        call.Function = function;
        call.Owner = m_Owner;
        call.Phase = ScriptDiagnosticPhase::Callback;
        call.FailurePrefix = "Interop provider callback failed";
        call.InvalidStateMessage = "Interop provider callback is no longer active.";
        call.ContextFailureMessage = "Unable to create an AngelScript context for an Interop provider callback.";
        call.SuspendedMessage = "Interop provider callbacks cannot suspend.";
        call.WriteArgs = writeArgs;
        call.ReadResult = &ReadProviderResult;
        call.UserData = &args;
        ScriptDiagnostic diagnostic;
        const bool succeeded = ExecuteScriptFunction(call, diagnostic);
        if (!succeeded)
            m_Owner->RecordScriptDiagnostic(diagnostic);

        scriptRequest->Invalidate();
        scriptRequest->Release();
        if (writer) {
            writer->Invalidate();
            writer->Release();
        }
        if (pageWriter) {
            pageWriter->Invalidate();
            pageWriter->Release();
        }
        return succeeded ? args.Result : BML_ERROR_SCRIPT_EXECUTION;
    }

    int CallProbe(const BML_InteropProviderRequest *request) {
        return Execute(m_Probe, request, nullptr, nullptr, &WriteProbeArgs);
    }

    int CallRead(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record) {
        return Execute(FindRead(request ? request->Endpoint : nullptr), request, record, nullptr, &WriteReadArgs);
    }

    int CallPage(const BML_InteropProviderRequest *request, BML_InteropPageBuilder *page) {
        return Execute(FindPage(request ? request->Endpoint : nullptr), request, nullptr, page, &WritePageArgs);
    }

    int m_RefCount = 1;
    ScriptMod *m_Owner = nullptr;
    ScriptInteropApiBuilder *m_Api = nullptr;
    bool m_Registered = false;
    asIScriptFunction *m_Probe = nullptr;
    std::unordered_map<std::string, asIScriptFunction *> m_ReadCallbacks;
    std::unordered_map<std::string, asIScriptFunction *> m_PageCallbacks;
};

ScriptInteropApiBuilder *CreateApi(const std::string &apiId,
                                      uint32_t major,
                                      uint32_t minor,
                                      uint64_t hash) {
    if (!IsInteropKey(apiId) || major == 0 || hash == 0)
        return nullptr;
    return new (std::nothrow) ScriptInteropApiBuilder(apiId, major, minor, hash);
}

ScriptInteropProvider *CreateProvider() {
    ScriptMod *owner = ScriptModRuntime::GetCurrentScriptMod();
    return owner ? static_cast<ScriptInteropProvider *>(owner->GetInteropProviderService().CreateProvider()) : nullptr;
}

BML_ObjectRef MakeScriptObjectRef(CKObject *object) {
    ScriptMod *owner = ScriptModRuntime::GetCurrentScriptMod();
    return owner && owner->GetModContext() ? MakeBuiltinObjectRef(*owner->GetModContext(), object) : BML_ObjectRef{};
}

CKObject *BorrowScriptObjectRef(const BML_ObjectRef &reference) {
    ScriptMod *owner = ScriptModRuntime::GetCurrentScriptMod();
    return owner && owner->GetModContext() ? ResolveBuiltinObjectRef(*owner->GetModContext(), reference) : nullptr;
}

void ProviderSetProbe(asIScriptGeneric *gen) {
    auto *provider = static_cast<ScriptInteropProvider *>(gen ? gen->GetObject() : nullptr);
    asIScriptFunction *callback = gen ? static_cast<asIScriptFunction *>(gen->GetArgObject(0)) : nullptr;
    const int status = provider ? provider->SetProbe(callback) : BML_ERROR_INVALID_PARAMETER;
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

template <bool Page>
void ProviderSetEndpointCallback(asIScriptGeneric *gen) {
    auto *provider = static_cast<ScriptInteropProvider *>(gen ? gen->GetObject() : nullptr);
    bool ok = provider != nullptr;
    ScriptStringInterop::GenericArg<const std::string &> endpoint(gen, 0, ok);
    asIScriptFunction *callback = gen ? static_cast<asIScriptFunction *>(gen->GetArgObject(1)) : nullptr;
    const int status = ok
                           ? (Page ? provider->SetPage(endpoint.Get(), callback) : provider->SetRead(endpoint.Get(), callback))
                           : BML_ERROR_INVALID_PARAMETER;
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

void ProviderBeginStreamRecord(asIScriptGeneric *gen) {
    auto *provider = static_cast<ScriptInteropProvider *>(gen ? gen->GetObject() : nullptr);
    bool ok = provider != nullptr;
    ScriptStringInterop::GenericArg<const std::string &> endpoint(gen, 0, ok);
    ScriptInteropRecordWriter *writer = ok ? provider->BeginStreamRecord(endpoint.Get()) : nullptr;
    if (!gen || gen->SetReturnAddress(writer) < 0) {
        if (writer)
            writer->Release();
    }
}

void RegisterScriptProvider(asIScriptGeneric *gen) {
    auto *api = static_cast<ScriptInteropApiBuilder *>(gen ? gen->GetArgObject(0) : nullptr);
    auto *provider = static_cast<ScriptInteropProvider *>(gen ? gen->GetArgObject(1) : nullptr);
    ScriptMod *owner = ScriptModRuntime::GetCurrentScriptMod();
    const int status = owner ? owner->GetInteropProviderService().Register(api, provider)
                             : BML_ERROR_INTEROP_UNSUPPORTED;
    if (gen)
        gen->SetReturnDWord(static_cast<asDWORD>(status));
}

} // namespace

struct ScriptInteropProviderService::State {
    ::ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    bool Active = false;
    std::vector<ScriptInteropProvider *> Providers;
};

ScriptInteropProviderService::ScriptInteropProviderService()
    : m_State(std::make_shared<State>()) {}

ScriptInteropProviderService::~ScriptInteropProviderService() {
    try {
        Release();
    } catch (...) {
    }
}

bool ScriptInteropProviderService::Bind(::ModContext *context, ScriptMod *owner) {
    if (!m_State)
        m_State = std::make_shared<State>();
    m_State->Context = context;
    m_State->Owner = owner;
    m_State->Active = context != nullptr && owner != nullptr;
    return m_State->Active;
}

void *ScriptInteropProviderService::CreateProvider() {
    if (!m_State || !m_State->Active || !m_State->Owner)
        return nullptr;
    return new (std::nothrow) ScriptInteropProvider(m_State->Owner);
}

int ScriptInteropProviderService::Register(void *rawApi, void *rawProvider) {
    auto *api = static_cast<ScriptInteropApiBuilder *>(rawApi);
    auto *provider = static_cast<ScriptInteropProvider *>(rawProvider);
    if (!m_State || !m_State->Active || !m_State->Context || !m_State->Owner || !api || !provider)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    if (!m_State->Owner->IsInLoadCallback() || provider->Owner() != m_State->Owner || provider->IsRegistered())
        return BML_ERROR_FROZEN;

    int status = provider->Validate(*api);
    if (status != BML_OK)
        return status;
    status = api->Prepare();
    if (status != BML_OK)
        return status;

    const BML_InteropProviderCallbacks callbacks = ScriptInteropProvider::Callbacks();
    status = m_State->Context->GetInteropRegistry().RegisterProvider(m_State->Owner->GetID(),
                                                                        api->Descriptor(),
                                                                        &callbacks,
                                                                        provider);
    if (status != BML_OK)
        return status;

    try {
        provider->AddRef();
        try {
            m_State->Providers.push_back(provider);
        } catch (...) {
            provider->Release();
            throw;
        }
        provider->Activate(api);
        (void)api->Freeze();
        return BML_OK;
    } catch (const std::bad_alloc &) {
        (void)m_State->Context->GetInteropRegistry().UnregisterProvider(m_State->Owner->GetID(),
                                                                           api->ApiId().c_str());
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

void ScriptInteropProviderService::Release() {
    if (!m_State)
        return;
    for (ScriptInteropProvider *provider : m_State->Providers) {
        if (m_State->Context && m_State->Owner && provider->IsRegistered() && provider->ApiId()) {
            (void)m_State->Context->GetInteropRegistry().UnregisterProvider(m_State->Owner->GetID(),
                                                                               provider->ApiId());
        }
        provider->Deactivate();
        provider->Release();
    }
    m_State->Providers.clear();
    m_State->Active = false;
    m_State->Context = nullptr;
    m_State->Owner = nullptr;
}

size_t ScriptInteropProviderService::GetActiveCount() const {
    return m_State ? m_State->Providers.size() : 0;
}

int RegisterScriptInteropProviderBridge(asIScriptEngine *engine, const char **errorMessage) {
    if (!engine) {
        g_ProviderBridgeRegistrationError = "Interop provider bridge received a null engine.";
        if (errorMessage)
            *errorMessage = g_ProviderBridgeRegistrationError.c_str();
        return asERROR;
    }

    if (!Register(engine, engine->SetDefaultNamespace("BML::Interop"), "namespace BML::Interop", errorMessage) ||
        !Register(engine, engine->RegisterEnum("FieldType"), "enum FieldType", errorMessage)) {
        return asERROR;
    }

#define BML_INTEROP_FIELD_ENUM(Name, Value) \
    if (!Register(engine, engine->RegisterEnumValue("FieldType", Name, Value), "FieldType::" Name, errorMessage)) \
        return asERROR
    BML_INTEROP_FIELD_ENUM("FIELD_BOOL", BML_INTEROP_FIELD_BOOL);
    BML_INTEROP_FIELD_ENUM("FIELD_INT", BML_INTEROP_FIELD_INT);
    BML_INTEROP_FIELD_ENUM("FIELD_FLOAT", BML_INTEROP_FIELD_FLOAT);
    BML_INTEROP_FIELD_ENUM("FIELD_STRING", BML_INTEROP_FIELD_STRING);
    BML_INTEROP_FIELD_ENUM("FIELD_OBJECT", BML_INTEROP_FIELD_OBJECT);
    BML_INTEROP_FIELD_ENUM("FIELD_VEC2", BML_INTEROP_FIELD_VEC2);
    BML_INTEROP_FIELD_ENUM("FIELD_VEC3", BML_INTEROP_FIELD_VEC3);
    BML_INTEROP_FIELD_ENUM("FIELD_MAT4", BML_INTEROP_FIELD_MAT4);
    BML_INTEROP_FIELD_ENUM("FIELD_BOOL_ARRAY", BML_INTEROP_FIELD_BOOL_ARRAY);
    BML_INTEROP_FIELD_ENUM("FIELD_INT_ARRAY", BML_INTEROP_FIELD_INT_ARRAY);
    BML_INTEROP_FIELD_ENUM("FIELD_FLOAT_ARRAY", BML_INTEROP_FIELD_FLOAT_ARRAY);
    BML_INTEROP_FIELD_ENUM("FIELD_STRING_ARRAY", BML_INTEROP_FIELD_STRING_ARRAY);
    BML_INTEROP_FIELD_ENUM("FIELD_OBJECT_ARRAY", BML_INTEROP_FIELD_OBJECT_ARRAY);
    BML_INTEROP_FIELD_ENUM("FIELD_VEC2_ARRAY", BML_INTEROP_FIELD_VEC2_ARRAY);
    BML_INTEROP_FIELD_ENUM("FIELD_VEC3_ARRAY", BML_INTEROP_FIELD_VEC3_ARRAY);
    BML_INTEROP_FIELD_ENUM("FIELD_MAT4_ARRAY", BML_INTEROP_FIELD_MAT4_ARRAY);
#undef BML_INTEROP_FIELD_ENUM

    if (!Register(engine, engine->RegisterEnum("EndpointKind"), "enum EndpointKind", errorMessage)) return asERROR;
#define BML_INTEROP_ENDPOINT_ENUM(Name, Value) \
    if (!Register(engine, engine->RegisterEnumValue("EndpointKind", Name, Value), "EndpointKind::" Name, errorMessage)) \
        return asERROR
    BML_INTEROP_ENDPOINT_ENUM("ENDPOINT_RESOURCE", BML_INTEROP_ENDPOINT_RESOURCE);
    BML_INTEROP_ENDPOINT_ENUM("ENDPOINT_COMPONENT", BML_INTEROP_ENDPOINT_COMPONENT);
    BML_INTEROP_ENDPOINT_ENUM("ENDPOINT_COLLECTION", BML_INTEROP_ENDPOINT_COLLECTION);
    BML_INTEROP_ENDPOINT_ENUM("ENDPOINT_STREAM", BML_INTEROP_ENDPOINT_STREAM);
    BML_INTEROP_ENDPOINT_ENUM("ENDPOINT_QUERY", BML_INTEROP_ENDPOINT_QUERY);
    BML_INTEROP_ENDPOINT_ENUM("ENDPOINT_COMMAND", BML_INTEROP_ENDPOINT_COMMAND);
#undef BML_INTEROP_ENDPOINT_ENUM

    if (!Register(engine, engine->RegisterObjectType("ObjectRef", sizeof(BML_ObjectRef),
                                                       asOBJ_VALUE | asGetTypeTraits<BML_ObjectRef>()),
                  "ObjectRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("ObjectRef", asBEHAVE_CONSTRUCT, "void f()",
                                                            asFUNCTION(ConstructObjectRef), asCALL_CDECL_OBJLAST),
                  "ObjectRef::construct", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("ObjectRef", asBEHAVE_CONSTRUCT, "void f(const ObjectRef &in)",
                                                            asFUNCTION(CopyConstructObjectRef), asCALL_CDECL_OBJLAST),
                  "ObjectRef::copy", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("ObjectRef", asBEHAVE_DESTRUCT, "void f()",
                                                            asFUNCTION(DestructObjectRef), asCALL_CDECL_OBJLAST),
                  "ObjectRef::destruct", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("ObjectRef", "ObjectRef &opAssign(const ObjectRef &in)",
                                                         asFUNCTION(AssignObjectRef), asCALL_CDECL_OBJLAST),
                  "ObjectRef::assign", errorMessage) ||
        !Register(engine, engine->RegisterObjectProperty("ObjectRef", "uint Domain", asOFFSET(BML_ObjectRef, Domain)),
                  "ObjectRef::Domain", errorMessage) ||
        !Register(engine, engine->RegisterObjectProperty("ObjectRef", "uint Slot", asOFFSET(BML_ObjectRef, Slot)),
                  "ObjectRef::Slot", errorMessage) ||
        !Register(engine, engine->RegisterObjectProperty("ObjectRef", "uint Generation", asOFFSET(BML_ObjectRef, Generation)),
                  "ObjectRef::Generation", errorMessage)) {
        return asERROR;
    }

    const char *referenceTypes[] = {"ApiBuilder", "Provider", "Request", "RecordWriter", "PageWriter"};
    for (const char *type : referenceTypes) {
        if (!Register(engine, engine->RegisterObjectType(type, 0, asOBJ_REF), type, errorMessage))
            return asERROR;
    }

    if (!Register(engine, engine->RegisterObjectBehaviour("ApiBuilder", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropApiBuilder, AddRef), asCALL_THISCALL),
                  "ApiBuilder::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("ApiBuilder", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropApiBuilder, Release), asCALL_THISCALL),
                  "ApiBuilder::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("ApiBuilder", "int AddSchema(uint id, const string &in name)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropApiBuilder::AddSchema), asCALL_GENERIC),
                  "ApiBuilder::AddSchema", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("ApiBuilder", "int AddField(uint schemaId, uint id, const string &in name, FieldType type, bool optional = false)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropApiBuilder::AddField), asCALL_GENERIC),
                  "ApiBuilder::AddField", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("ApiBuilder", "int AddEndpoint(const string &in name, EndpointKind kind, uint inputSchema, uint outputSchema, bool requiresProbe = false)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropApiBuilder::AddEndpoint), asCALL_GENERIC),
                  "ApiBuilder::AddEndpoint", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("ApiBuilder", "int AddCompatibleApiHash(uint64 hash)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropApiBuilder::AddCompatibleApiHash), asCALL_GENERIC),
                  "ApiBuilder::AddCompatibleApiHash", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("ApiBuilder", "bool get_IsFrozen() const",
                                                         asMETHOD(ScriptInteropApiBuilder, IsFrozen), asCALL_THISCALL),
                  "ApiBuilder::IsFrozen", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Provider", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropProvider, AddRef), asCALL_THISCALL),
                  "Provider::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Provider", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropProvider, Release), asCALL_THISCALL),
                  "Provider::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Provider", "int SetProbe(ProbeCallback@+ callback)",
                                                         asFUNCTION(ProviderSetProbe), asCALL_GENERIC),
                  "Provider::SetProbe", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Provider", "int SetRead(const string &in endpoint, ReadCallback@+ callback)",
                                                         asFUNCTION(ProviderSetEndpointCallback<false>), asCALL_GENERIC),
                  "Provider::SetRead", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Provider", "int SetPage(const string &in endpoint, PageCallback@+ callback)",
                                                         asFUNCTION(ProviderSetEndpointCallback<true>), asCALL_GENERIC),
                  "Provider::SetPage", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Provider", "RecordWriter@ BeginStreamRecord(const string &in endpoint)",
                                                         asFUNCTION(ProviderBeginStreamRecord), asCALL_GENERIC),
                  "Provider::BeginStreamRecord", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Provider", "bool get_IsRegistered() const",
                                                         asMETHOD(ScriptInteropProvider, IsRegistered), asCALL_THISCALL),
                  "Provider::IsRegistered", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterObjectBehaviour("Request", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropRequest, AddRef), asCALL_THISCALL),
                  "Request::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("Request", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropRequest, Release), asCALL_THISCALL),
                  "Request::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "bool get_IsActive() const",
                                                         asMETHOD(ScriptInteropRequest, IsActive), asCALL_THISCALL),
                  "Request::IsActive", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "string get_ApiId() const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetApiId), asCALL_GENERIC),
                  "Request::ApiId", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "string get_Endpoint() const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetEndpoint), asCALL_GENERIC),
                  "Request::Endpoint", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "string get_ConsumerId() const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetConsumerId), asCALL_GENERIC),
                  "Request::ConsumerId", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int get_Kind() const",
                                                         asMETHOD(ScriptInteropRequest, GetKind), asCALL_THISCALL),
                  "Request::Kind", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "ObjectRef get_Object() const",
                                                         asMETHOD(ScriptInteropRequest, GetObject), asCALL_THISCALL),
                  "Request::Object", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "uint64 get_Offset() const",
                                                         asMETHOD(ScriptInteropRequest, GetOffset), asCALL_THISCALL),
                  "Request::Offset", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "uint get_Limit() const",
                                                         asMETHOD(ScriptInteropRequest, GetLimit), asCALL_THISCALL),
                  "Request::Limit", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputBool(uint field, bool &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputBool), asCALL_GENERIC),
                  "Request::GetInputBool", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputInt(uint field, int &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputInt), asCALL_GENERIC),
                  "Request::GetInputInt", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputFloat(uint field, float &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputFloat), asCALL_GENERIC),
                  "Request::GetInputFloat", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputString(uint field, string &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputString), asCALL_GENERIC),
                  "Request::GetInputString", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputObject(uint field, ObjectRef &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputObject), asCALL_GENERIC),
                  "Request::GetInputObject", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputVec2(uint field, ::BML::Vec2 &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputVec2), asCALL_GENERIC),
                  "Request::GetInputVec2", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputVec3(uint field, ::BML::Vec3 &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputVec3), asCALL_GENERIC),
                  "Request::GetInputVec3", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputMat4(uint field, ::BML::Mat4 &out value) const",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRequest::GetInputMat4), asCALL_GENERIC),
                  "Request::GetInputMat4", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputBoolArray(uint field, array<bool> &out values) const",
                                                         asFUNCTION(RequestGetBoolArray), asCALL_GENERIC),
                  "Request::GetInputBoolArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputIntArray(uint field, array<int> &out values) const",
                                                         asFUNCTION(RequestGetIntArray), asCALL_GENERIC),
                  "Request::GetInputIntArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputFloatArray(uint field, array<float> &out values) const",
                                                         asFUNCTION(RequestGetFloatArray), asCALL_GENERIC),
                  "Request::GetInputFloatArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputStringArray(uint field, array<string> &out values) const",
                                                         asFUNCTION(RequestGetStringArray), asCALL_GENERIC),
                  "Request::GetInputStringArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputObjectArray(uint field, array<ObjectRef> &out values) const",
                                                         asFUNCTION(RequestGetObjectArray), asCALL_GENERIC),
                  "Request::GetInputObjectArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputVec2Array(uint field, array<::BML::Vec2> &out values) const",
                                                         asFUNCTION(RequestGetVec2Array), asCALL_GENERIC),
                  "Request::GetInputVec2Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputVec3Array(uint field, array<::BML::Vec3> &out values) const",
                                                         asFUNCTION(RequestGetVec3Array), asCALL_GENERIC),
                  "Request::GetInputVec3Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("Request", "int GetInputMat4Array(uint field, array<::BML::Mat4> &out values) const",
                                                         asFUNCTION(RequestGetMat4Array), asCALL_GENERIC),
                  "Request::GetInputMat4Array", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterObjectBehaviour("RecordWriter", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropRecordWriter, AddRef), asCALL_THISCALL),
                  "RecordWriter::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("RecordWriter", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropRecordWriter, Release), asCALL_THISCALL),
                  "RecordWriter::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "bool get_IsActive() const",
                                                         asMETHOD(ScriptInteropRecordWriter, IsActive), asCALL_THISCALL),
                  "RecordWriter::IsActive", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetBool(uint field, bool value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetBool), asCALL_GENERIC),
                  "RecordWriter::SetBool", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetInt(uint field, int value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetInt), asCALL_GENERIC),
                  "RecordWriter::SetInt", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetFloat(uint field, float value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetFloat), asCALL_GENERIC),
                  "RecordWriter::SetFloat", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetString(uint field, const string &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetString), asCALL_GENERIC),
                  "RecordWriter::SetString", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetObject(uint field, const ObjectRef &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetObject), asCALL_GENERIC),
                  "RecordWriter::SetObject", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetVec2(uint field, const ::BML::Vec2 &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetVec2), asCALL_GENERIC),
                  "RecordWriter::SetVec2", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetVec3(uint field, const ::BML::Vec3 &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetVec3), asCALL_GENERIC),
                  "RecordWriter::SetVec3", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetMat4(uint field, const ::BML::Mat4 &in value)",
                                                         BML_AS_GENERIC_METHOD(&ScriptInteropRecordWriter::SetMat4), asCALL_GENERIC),
                  "RecordWriter::SetMat4", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetBoolArray(uint field, const array<bool> &in values)",
                                                         asFUNCTION(WriterSetBoolArray), asCALL_GENERIC),
                  "RecordWriter::SetBoolArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetIntArray(uint field, const array<int> &in values)",
                                                         asFUNCTION(WriterSetIntArray), asCALL_GENERIC),
                  "RecordWriter::SetIntArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetFloatArray(uint field, const array<float> &in values)",
                                                         asFUNCTION(WriterSetFloatArray), asCALL_GENERIC),
                  "RecordWriter::SetFloatArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetStringArray(uint field, const array<string> &in values)",
                                                         asFUNCTION(WriterSetStringArray), asCALL_GENERIC),
                  "RecordWriter::SetStringArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetObjectArray(uint field, const array<ObjectRef> &in values)",
                                                         asFUNCTION(WriterSetObjectArray), asCALL_GENERIC),
                  "RecordWriter::SetObjectArray", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetVec2Array(uint field, const array<::BML::Vec2> &in values)",
                                                         asFUNCTION(WriterSetVec2Array), asCALL_GENERIC),
                  "RecordWriter::SetVec2Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetVec3Array(uint field, const array<::BML::Vec3> &in values)",
                                                         asFUNCTION(WriterSetVec3Array), asCALL_GENERIC),
                  "RecordWriter::SetVec3Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int SetMat4Array(uint field, const array<::BML::Mat4> &in values)",
                                                         asFUNCTION(WriterSetMat4Array), asCALL_GENERIC),
                  "RecordWriter::SetMat4Array", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("RecordWriter", "int Publish()",
                                                         asMETHOD(ScriptInteropRecordWriter, Publish), asCALL_THISCALL),
                  "RecordWriter::Publish", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("PageWriter", asBEHAVE_ADDREF, "void f()",
                                                            asMETHOD(ScriptInteropPageWriter, AddRef), asCALL_THISCALL),
                  "PageWriter::AddRef", errorMessage) ||
        !Register(engine, engine->RegisterObjectBehaviour("PageWriter", asBEHAVE_RELEASE, "void f()",
                                                            asMETHOD(ScriptInteropPageWriter, Release), asCALL_THISCALL),
                  "PageWriter::Release", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("PageWriter", "bool get_IsActive() const",
                                                         asMETHOD(ScriptInteropPageWriter, IsActive), asCALL_THISCALL),
                  "PageWriter::IsActive", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("PageWriter", "RecordWriter@ Append()",
                                                         asMETHOD(ScriptInteropPageWriter, Append), asCALL_THISCALL),
                  "PageWriter::Append", errorMessage) ||
        !Register(engine, engine->RegisterObjectMethod("PageWriter", "void SetComplete(bool complete)",
                                                         asMETHOD(ScriptInteropPageWriter, SetComplete), asCALL_THISCALL),
                  "PageWriter::SetComplete", errorMessage)) {
        return asERROR;
    }

    if (!Register(engine, engine->RegisterFuncdef("int ProbeCallback(const Request &in request)"),
                  "funcdef ProbeCallback", errorMessage) ||
        !Register(engine, engine->RegisterFuncdef("int ReadCallback(const Request &in request, RecordWriter@ writer)"),
                  "funcdef ReadCallback", errorMessage) ||
        !Register(engine, engine->RegisterFuncdef("int PageCallback(const Request &in request, PageWriter@ page)"),
                  "funcdef PageCallback", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("ApiBuilder@ CreateApi(const string &in apiId, uint major, uint minor, uint64 hash)",
                                                           BML_AS_GENERIC_FUNCTION(&CreateApi), asCALL_GENERIC),
                  "CreateApi", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("Provider@ CreateProvider()",
                                                           asFUNCTION(CreateProvider), asCALL_CDECL),
                  "CreateProvider", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("int RegisterProvider(ApiBuilder@ api, Provider@ provider)",
                                                           asFUNCTION(RegisterScriptProvider), asCALL_GENERIC),
                  "RegisterProvider", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("ObjectRef MakeObjectRef(CKObject@ object)",
                                                           asFUNCTION(MakeScriptObjectRef), asCALL_CDECL),
                  "MakeObjectRef", errorMessage) ||
        !Register(engine, engine->RegisterGlobalFunction("CKObject@ BorrowObject(const ObjectRef &in object)",
                                                           BML_AS_GENERIC_FUNCTION(&BorrowScriptObjectRef), asCALL_GENERIC),
                  "BorrowObject", errorMessage) ||
        !Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage)) {
        return asERROR;
    }
    return asSUCCESS;
}

} // namespace BML
