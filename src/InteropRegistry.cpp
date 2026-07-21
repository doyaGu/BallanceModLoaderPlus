#include "InteropRegistry.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "InteropSessionService.h"
#include "CryptoUtils.h"
#include "StringUtils.h"

namespace BML::InteropDetail {

/* Do not store C++ bool in snapshots: the public representation is an int
 * with 0/1 semantics, so the in-memory snapshot remains C-ABI shaped too. */
struct BoolValue {
    int Value = 0;
};

template <typename T, BML_INTEROP_FIELD_TYPE TypeTag>
struct ArrayValue {
    std::vector<T> Values;
    static constexpr BML_INTEROP_FIELD_TYPE Type = TypeTag;
};

using BoolArray = ArrayValue<int, BML_INTEROP_FIELD_BOOL_ARRAY>;
using IntArray = ArrayValue<int, BML_INTEROP_FIELD_INT_ARRAY>;
using FloatArray = ArrayValue<float, BML_INTEROP_FIELD_FLOAT_ARRAY>;
using StringArray = ArrayValue<std::string, BML_INTEROP_FIELD_STRING_ARRAY>;
using ObjectArray = ArrayValue<BML_ObjectRef, BML_INTEROP_FIELD_OBJECT_ARRAY>;
using Vec2Array = ArrayValue<BML_Vec2, BML_INTEROP_FIELD_VEC2_ARRAY>;
using Vec3Array = ArrayValue<BML_Vec3, BML_INTEROP_FIELD_VEC3_ARRAY>;
using Mat4Array = ArrayValue<BML_Mat4, BML_INTEROP_FIELD_MAT4_ARRAY>;

using FieldValue = std::variant<BoolValue,
                                int,
                                float,
                                std::string,
                                BML_ObjectRef,
                                BML_Vec2,
                                BML_Vec3,
                                BML_Mat4,
                                BoolArray,
                                IntArray,
                                FloatArray,
                                StringArray,
                                ObjectArray,
                                Vec2Array,
                                Vec3Array,
                                Mat4Array>;
using FieldMap = std::unordered_map<uint32_t, FieldValue>;

struct SchemaInfo {
    uint32_t Id = 0;
    std::string Name;
    std::vector<std::string> FieldNames;
    std::vector<BML_InteropFieldDescriptor> Fields;
    std::unordered_map<uint32_t, size_t> FieldIndex;
};

struct EndpointInfo {
    std::string Name;
    BML_InteropEndpointDescriptor Descriptor{};
    std::shared_ptr<SchemaInfo> Schema;
    std::shared_ptr<SchemaInfo> InputSchema;
};

} // namespace BML::InteropDetail

/* These opaque C handles are allocated and owned by the runtime.  Their C++
 * layout is deliberately private to this translation unit. */
struct BML_InteropRecordBuilder {
    std::shared_ptr<BML::InteropDetail::SchemaInfo> Schema;
    std::vector<std::pair<uint32_t, BML::InteropDetail::FieldValue>> Fields;
    std::string Owner;
    std::string ApiId;
    std::string Endpoint;
    uint64_t OwnerSessionId = 0;
    bool StandaloneStreamRecord = false;
    bool InputRecord = false;
};

struct BML_InteropRecordView {
    std::shared_ptr<BML::InteropDetail::SchemaInfo> Schema;
    const BML::InteropDetail::FieldMap *Fields = nullptr;
};

struct BML_InteropPageBuilder {
    std::shared_ptr<BML::InteropDetail::SchemaInfo> Schema;
    std::vector<std::unique_ptr<BML_InteropRecordBuilder>> Records;
    bool Complete = true;
};

namespace BML {
namespace {

using InteropDetail::FieldValue;
using InteropDetail::FieldMap;
using InteropDetail::SchemaInfo;
using InteropDetail::EndpointInfo;

struct ApiInfo {
    std::string ApiId;
    std::string Owner;
    uint32_t Major = 0;
    uint32_t Minor = 0;
    uint64_t Hash = 0;
    std::vector<uint64_t> CompatibleApiHashes;
    BML_InteropProviderCallbacks Callbacks{};
    void *Userdata = nullptr;
    std::unordered_map<uint32_t, std::shared_ptr<SchemaInfo>> Schemas;
    std::unordered_map<std::string, std::shared_ptr<EndpointInfo>> Endpoints;
};

const char *CanonicalFieldType(BML_INTEROP_FIELD_TYPE type) {
    switch (type) {
    case BML_INTEROP_FIELD_BOOL: return "bool";
    case BML_INTEROP_FIELD_INT: return "int";
    case BML_INTEROP_FIELD_FLOAT: return "float";
    case BML_INTEROP_FIELD_STRING: return "string";
    case BML_INTEROP_FIELD_OBJECT: return "object";
    case BML_INTEROP_FIELD_VEC2: return "vec2";
    case BML_INTEROP_FIELD_VEC3: return "vec3";
    case BML_INTEROP_FIELD_MAT4: return "mat4";
    case BML_INTEROP_FIELD_BOOL_ARRAY: return "array<bool>";
    case BML_INTEROP_FIELD_INT_ARRAY: return "array<int>";
    case BML_INTEROP_FIELD_FLOAT_ARRAY: return "array<float>";
    case BML_INTEROP_FIELD_STRING_ARRAY: return "array<string>";
    case BML_INTEROP_FIELD_OBJECT_ARRAY: return "array<object>";
    case BML_INTEROP_FIELD_VEC2_ARRAY: return "array<vec2>";
    case BML_INTEROP_FIELD_VEC3_ARRAY: return "array<vec3>";
    case BML_INTEROP_FIELD_MAT4_ARRAY: return "array<mat4>";
    default: return nullptr;
    }
}

const char *CanonicalEndpointKind(BML_INTEROP_ENDPOINT_KIND kind) {
    switch (kind) {
    case BML_INTEROP_ENDPOINT_RESOURCE: return "resource";
    case BML_INTEROP_ENDPOINT_COMPONENT: return "component";
    case BML_INTEROP_ENDPOINT_COLLECTION: return "collection";
    case BML_INTEROP_ENDPOINT_STREAM: return "stream";
    case BML_INTEROP_ENDPOINT_QUERY: return "query";
    case BML_INTEROP_ENDPOINT_COMMAND: return "command";
    default: return nullptr;
    }
}

void AppendCanonicalJsonString(std::string &out, const std::string &value) {
    /* API IDs and endpoint names are validated as [A-Za-z0-9_.-]+, so the JSON string has
     * no characters that require escaping. Keeping this deliberately narrow
     * makes the native form byte-for-byte identical to codegen's JSON. */
    out.push_back('\"');
    out += value;
    out.push_back('\"');
}

uint64_t CanonicalApiHash(const ApiInfo &api) {
    std::vector<const SchemaInfo *> records;
    records.reserve(api.Schemas.size());
    for (const auto &[id, schema] : api.Schemas) {
        (void)id;
        if (!schema)
            return 0;
        records.push_back(schema.get());
    }
    std::sort(records.begin(), records.end(), [](const SchemaInfo *left, const SchemaInfo *right) {
        return left->Id < right->Id;
    });

    std::vector<const EndpointInfo *> endpoints;
    endpoints.reserve(api.Endpoints.size());
    for (const auto &[name, endpoint] : api.Endpoints) {
        (void)name;
        if (!endpoint)
            return 0;
        endpoints.push_back(endpoint.get());
    }
    std::sort(endpoints.begin(), endpoints.end(), [](const EndpointInfo *left, const EndpointInfo *right) {
        return left->Name < right->Name;
    });

    std::string canonical;
    canonical.reserve(256 + records.size() * 96 + endpoints.size() * 80);
    canonical += "{\"api\":";
    AppendCanonicalJsonString(canonical, api.ApiId);
    canonical += ",\"endpoints\":[";
    for (size_t endpointIndex = 0; endpointIndex < endpoints.size(); ++endpointIndex) {
        if (endpointIndex != 0)
            canonical.push_back(',');
        const EndpointInfo &endpoint = *endpoints[endpointIndex];
        const char *kind = CanonicalEndpointKind(endpoint.Descriptor.Kind);
        if (!kind)
            return 0;
        canonical += "{\"input\":" + std::to_string(endpoint.Descriptor.InputSchema) + ",\"kind\":";
        AppendCanonicalJsonString(canonical, kind);
        canonical += ",\"name\":";
        AppendCanonicalJsonString(canonical, endpoint.Name);
        canonical += ",\"output\":" + std::to_string(endpoint.Descriptor.OutputSchema) + ",\"requires_probe\":";
        canonical += endpoint.Descriptor.RequiresProbe ? "true" : "false";
        canonical.push_back('}');
    }
    canonical += "],\"schemas\":[";
    for (size_t schemaIndex = 0; schemaIndex < records.size(); ++schemaIndex) {
        if (schemaIndex != 0)
            canonical.push_back(',');
        const SchemaInfo &schema = *records[schemaIndex];
        canonical += "{\"fields\":[";
        std::vector<const BML_InteropFieldDescriptor *> fields;
        fields.reserve(schema.Fields.size());
        for (const BML_InteropFieldDescriptor &field : schema.Fields)
            fields.push_back(&field);
        std::sort(fields.begin(), fields.end(), [](const auto *left, const auto *right) {
            return left->Id < right->Id;
        });
        for (size_t fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex) {
            if (fieldIndex != 0)
                canonical.push_back(',');
            const BML_InteropFieldDescriptor &field = *fields[fieldIndex];
            const char *type = CanonicalFieldType(field.Type);
            if (!type || !field.Name)
                return 0;
            canonical += "{\"id\":" + std::to_string(field.Id) + ",\"name\":";
            AppendCanonicalJsonString(canonical, field.Name);
            canonical += ",\"optional\":";
            canonical += field.Optional ? "true" : "false";
            canonical += ",\"type\":";
            AppendCanonicalJsonString(canonical, type);
            canonical.push_back('}');
        }
        canonical += "],\"id\":" + std::to_string(schema.Id) + ",\"name\":";
        AppendCanonicalJsonString(canonical, schema.Name);
        canonical.push_back('}');
    }
    canonical += "],\"version\":{\"major\":" + std::to_string(api.Major) +
                 ",\"minor\":" + std::to_string(api.Minor) + "}}";

    std::array<uint8_t, 32> digest{};
    if (!utils::Sha256(reinterpret_cast<const uint8_t *>(canonical.data()), canonical.size(), digest.data()))
        return 0;
    uint64_t result = 0;
    for (size_t index = 0; index < 8; ++index)
        result = (result << 8u) | digest[index];
    return result;
}

struct RecordState {
    uint64_t OwnerSessionId = 0;
    uint32_t RefCount = 1;
    std::shared_ptr<SchemaInfo> Schema;
    std::shared_ptr<const FieldMap> Fields;
    uint64_t Sequence = 0;
    uint64_t Timestamp = 0;
};

/* A borrow must remain usable after the registry mutex is released even if a
 * different thread releases the handle or unloads its owner immediately.
 * The next borrow on this thread advances the hazard slot. */
thread_local std::shared_ptr<const FieldMap> g_BorrowedRecordPayload;

struct StreamState {
    uint64_t OwnerSessionId = 0;
    std::string ApiId;
    std::string Endpoint;
    size_t Capacity = 256;
    uint64_t Dropped = 0;
    bool Closed = false;
    int CloseStatus = BML_OK;
    std::deque<uint64_t> Queue;
};

struct CursorState {
    uint64_t OwnerSessionId = 0;
    std::string ApiId;
    std::string Endpoint;
    uint64_t Offset = 0;
    bool Complete = false;
};

bool IsKeyPart(const char *value) {
    if (!value || !*value)
        return false;
    for (const unsigned char *it = reinterpret_cast<const unsigned char *>(value); *it; ++it) {
        if (!(std::isalnum(*it) || *it == '.' || *it == '_' || *it == '-'))
            return false;
    }
    return true;
}

uint64_t TimestampNow() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

BML_INTEROP_FIELD_TYPE TypeOf(const FieldValue &value) {
    switch (value.index()) {
    case 0: return BML_INTEROP_FIELD_BOOL;
    case 1: return BML_INTEROP_FIELD_INT;
    case 2: return BML_INTEROP_FIELD_FLOAT;
    case 3: return BML_INTEROP_FIELD_STRING;
    case 4: return BML_INTEROP_FIELD_OBJECT;
    case 5: return BML_INTEROP_FIELD_VEC2;
    case 6: return BML_INTEROP_FIELD_VEC3;
    case 7: return BML_INTEROP_FIELD_MAT4;
    case 8: return BML_INTEROP_FIELD_BOOL_ARRAY;
    case 9: return BML_INTEROP_FIELD_INT_ARRAY;
    case 10: return BML_INTEROP_FIELD_FLOAT_ARRAY;
    case 11: return BML_INTEROP_FIELD_STRING_ARRAY;
    case 12: return BML_INTEROP_FIELD_OBJECT_ARRAY;
    case 13: return BML_INTEROP_FIELD_VEC2_ARRAY;
    case 14: return BML_INTEROP_FIELD_VEC3_ARRAY;
    case 15: return BML_INTEROP_FIELD_MAT4_ARRAY;
    default: return BML_INTEROP_FIELD_INT;
    }
}

int CopyString(const std::string &value, char *buffer, size_t bufferSize, size_t *outRequiredSize) {
    if ((!buffer && bufferSize != 0) || (buffer && bufferSize == 0)) {
        if (outRequiredSize)
            *outRequiredSize = value.size() + 1;
        return BML_ERROR_INVALID_PARAMETER;
    }
    return utils::CopyStringToBuffer(value, buffer, bufferSize, outRequiredSize)
               ? BML_OK
               : BML_ERROR_INVALID_PARAMETER;
}

} // namespace

struct InteropRegistry::State {
    explicit State(InteropSessionService &sessions)
        : Sessions(sessions), GameThread(std::this_thread::get_id()) {}

    InteropSessionService &Sessions;
    std::thread::id GameThread;
    std::mutex Mutex;
    uint64_t NextHandle = 1;
    uint64_t NextSequence = 1;
    std::unordered_map<std::string, std::shared_ptr<ApiInfo>> Apis;
    std::unordered_map<uint64_t, RecordState> Records;
    std::unordered_map<uint64_t, StreamState> Streams;
    std::unordered_map<uint64_t, CursorState> Cursors;
    std::unordered_map<BML_InteropRecordBuilder *, std::unique_ptr<BML_InteropRecordBuilder>> OwnedBuilders;
};

namespace {

int RequireGameThread(const InteropRegistry::State &state) {
    return std::this_thread::get_id() == state.GameThread ? BML_OK : BML_ERROR_INTEROP_WRONG_THREAD;
}

int ValidateContext(const InteropRegistry::State &state,
                    const BML_InteropCallContext *context,
                    bool requireSession) {
    return state.Sessions.ValidateContext(context, requireSession);
}

uint64_t SessionId(const InteropRegistry::State &state, const BML_InteropCallContext *context) {
    return state.Sessions.GetSessionId(context);
}

std::shared_ptr<ApiInfo> FindApi(InteropRegistry::State &state, const char *apiId) {
    if (!IsKeyPart(apiId))
        return {};
    const auto found = state.Apis.find(apiId);
    return found == state.Apis.end() ? std::shared_ptr<ApiInfo>() : found->second;
}

std::shared_ptr<EndpointInfo> FindEndpoint(const std::shared_ptr<ApiInfo> &api,
                                       const char *endpoint,
                                       BML_INTEROP_ENDPOINT_KIND expected) {
    if (!api || !IsKeyPart(endpoint))
        return {};
    const auto found = api->Endpoints.find(endpoint);
    if (found == api->Endpoints.end() || !found->second || found->second->Descriptor.Kind != expected)
        return {};
    return found->second;
}

int ValidateBuilder(const std::shared_ptr<SchemaInfo> &schema,
                    const BML_InteropRecordBuilder &builder,
                    FieldMap &out) {
    if (!schema || !builder.Schema || builder.Schema->Id != schema->Id)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    out.clear();
    for (const auto &[fieldId, value] : builder.Fields) {
        const auto known = schema->FieldIndex.find(fieldId);
        if (known == schema->FieldIndex.end() || TypeOf(value) != schema->Fields[known->second].Type)
            return BML_ERROR_INTEROP_RECORD_INVALID;
        if (!out.emplace(fieldId, value).second)
            return BML_ERROR_INTEROP_RECORD_INVALID;
    }
    for (const BML_InteropFieldDescriptor &field : schema->Fields) {
        if (!field.Optional && out.find(field.Id) == out.end())
            return BML_ERROR_INTEROP_RECORD_INVALID;
    }
    return BML_OK;
}

uint64_t StoreRecordPayload(InteropRegistry::State &state,
                            uint64_t ownerSession,
                            const std::shared_ptr<SchemaInfo> &schema,
                            std::shared_ptr<const FieldMap> fields,
                            uint64_t sequence,
                            uint64_t timestamp) {
    const uint64_t token = state.NextHandle++;
    RecordState record;
    record.OwnerSessionId = ownerSession;
    record.Schema = schema;
    record.Fields = std::move(fields);
    record.Sequence = sequence;
    record.Timestamp = timestamp;
    state.Records.emplace(token, std::move(record));
    return token;
}

uint64_t StoreRecord(InteropRegistry::State &state,
                     uint64_t ownerSession,
                     const std::shared_ptr<SchemaInfo> &schema,
                     FieldMap fields,
                     uint64_t sequence,
                     uint64_t timestamp) {
    return StoreRecordPayload(state,
                              ownerSession,
                              schema,
                              std::make_shared<const FieldMap>(std::move(fields)),
                              sequence,
                              timestamp);
}

void DropRecord(InteropRegistry::State &state, uint64_t token) {
    const auto found = state.Records.find(token);
    if (found == state.Records.end())
        return;
    if (found->second.RefCount > 1) {
        --found->second.RefCount;
        return;
    }
    state.Records.erase(found);
}

int RequireRecord(InteropRegistry::State &state,
                  const BML_InteropCallContext *context,
                  BML_RecordRef handle,
                  RecordState **outRecord) {
    if (outRecord)
        *outRecord = nullptr;
    const int contextStatus = ValidateContext(state, context, false);
    if (contextStatus != BML_OK)
        return contextStatus;
    const auto found = state.Records.find(handle.Value);
    if (handle.Value == 0 || found == state.Records.end())
        return BML_ERROR_INTEROP_HANDLE_STALE;
    if (found->second.OwnerSessionId != SessionId(state, context))
        return BML_ERROR_INTEROP_HANDLE_STALE;
    if (outRecord)
        *outRecord = &found->second;
    return BML_OK;
}

int RequireStream(InteropRegistry::State &state,
                  const BML_InteropCallContext *context,
                  BML_StreamRef handle,
                  StreamState **outStream) {
    if (outStream)
        *outStream = nullptr;
    const int contextStatus = ValidateContext(state, context, true);
    if (contextStatus != BML_OK)
        return contextStatus;
    const auto found = state.Streams.find(handle.Value);
    if (handle.Value == 0 || found == state.Streams.end() ||
        found->second.OwnerSessionId != SessionId(state, context)) {
        return BML_ERROR_INTEROP_HANDLE_STALE;
    }
    if (outStream)
        *outStream = &found->second;
    return BML_OK;
}

int RequireCursor(InteropRegistry::State &state,
                  const BML_InteropCallContext *context,
                  BML_CursorRef handle,
                  CursorState **outCursor) {
    if (outCursor)
        *outCursor = nullptr;
    const int contextStatus = ValidateContext(state, context, true);
    if (contextStatus != BML_OK)
        return contextStatus;
    const auto found = state.Cursors.find(handle.Value);
    if (handle.Value == 0 || found == state.Cursors.end() ||
        found->second.OwnerSessionId != SessionId(state, context)) {
        return BML_ERROR_INTEROP_CURSOR_STALE;
    }
    if (outCursor)
        *outCursor = &found->second;
    return BML_OK;
}

BML_InteropProviderRequest MakeRequest(const std::shared_ptr<ApiInfo> &api,
                                       const std::shared_ptr<EndpointInfo> &endpoint,
                                       const BML_InteropCallContext *context,
                                       BML_ObjectRef object = {},
                                       uint64_t offset = 0,
                                       uint32_t limit = 0,
                                       const BML_InteropRecordView *input = nullptr) {
    BML_InteropProviderRequest request{};
    request.Size = sizeof(request);
    request.ApiId = api->ApiId.c_str();
    request.Endpoint = endpoint->Name.c_str();
    request.Kind = endpoint->Descriptor.Kind;
    request.Object = object;
    request.Offset = offset;
    request.Limit = limit;
    request.ConsumerId = context && context->OwnerId ? context->OwnerId : "";
    request.Input = input;
    return request;
}

template <typename Callback, typename... Args>
int InvokeProviderCallback(Callback callback, Args &&...args) noexcept {
    if (!callback)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    try {
        return callback(std::forward<Args>(args)...);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED;
    }
}

int Probe(const std::shared_ptr<ApiInfo> &api,
          const std::shared_ptr<EndpointInfo> &endpoint,
          const BML_InteropProviderRequest &request) {
    if (!endpoint->Descriptor.RequiresProbe)
        return BML_OK;
    if (!api->Callbacks.Probe)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return InvokeProviderCallback(api->Callbacks.Probe, &request, api->Userdata);
}

/* Provider callbacks are allowed to consume another api.  Never hold
 * the registry mutex while invoking them: an AngelScript provider may call a
 * generated consumer facade synchronously on the game thread. */
bool IsLiveApi(InteropRegistry::State &state,
                    const std::shared_ptr<ApiInfo> &api) {
    if (!api)
        return false;
    const auto found = state.Apis.find(api->ApiId);
    return found != state.Apis.end() && found->second == api;
}

int ReadSnapshot(InteropRegistry::State &state,
                 const BML_InteropCallContext *context,
                 const char *apiId,
                 const char *endpointName,
                 BML_INTEROP_ENDPOINT_KIND kind,
                 BML_ObjectRef object,
                 BML_RecordRef *outRecord) {
    if (!outRecord)
        return BML_ERROR_INVALID_PARAMETER;
    *outRecord = {};
    const int threadStatus = RequireGameThread(state);
    if (threadStatus != BML_OK)
        return threadStatus;

    std::shared_ptr<ApiInfo> api;
    std::shared_ptr<EndpointInfo> endpoint;
    uint64_t ownerSessionId = 0;
    {
        std::unique_lock<std::mutex> lock(state.Mutex);
        const int contextStatus = ValidateContext(state, context, false);
        if (contextStatus != BML_OK)
            return contextStatus;
        api = FindApi(state, apiId);
        endpoint = FindEndpoint(api, endpointName, kind);
        if (!api || !endpoint)
            return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
        ownerSessionId = SessionId(state, context);
    }

    const BML_InteropProviderRequest request = MakeRequest(api, endpoint, context, object);
    int status = Probe(api, endpoint, request);
    if (status != BML_OK)
        return status;
    {
        std::unique_lock<std::mutex> lock(state.Mutex);
        if (!IsLiveApi(state, api))
            return BML_ERROR_INTEROP_PROVIDER_UNLOADED;
        const int contextStatus = ValidateContext(state, context, false);
        if (contextStatus != BML_OK)
            return contextStatus;
        if (SessionId(state, context) != ownerSessionId)
            return BML_ERROR_INTEROP_HANDLE_STALE;
    }
    BML_InteropRecordBuilder builder;
    builder.Schema = endpoint->Schema;
    BML_InteropReadCallback callback = kind == BML_INTEROP_ENDPOINT_RESOURCE
                                           ? api->Callbacks.ReadResource
                                           : api->Callbacks.ReadComponent;
    if (!callback)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    status = InvokeProviderCallback(callback, &request, &builder, api->Userdata);
    if (status != BML_OK)
        return status;

    std::unique_lock<std::mutex> lock(state.Mutex);
    if (!IsLiveApi(state, api))
        return BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    const int contextStatus = ValidateContext(state, context, false);
    if (contextStatus != BML_OK)
        return contextStatus;
    if (SessionId(state, context) != ownerSessionId)
        return BML_ERROR_INTEROP_HANDLE_STALE;

    std::unordered_map<uint32_t, FieldValue> fields;
    status = ValidateBuilder(endpoint->Schema, builder, fields);
    if (status != BML_OK)
        return status;
    outRecord->Value = StoreRecord(state,
                                   ownerSessionId,
                                   endpoint->Schema,
                                   std::move(fields),
                                   0,
                                   TimestampNow());
    return BML_OK;
}

template <typename T>
int GetRecordField(InteropRegistry::State &service,
                   const BML_InteropCallContext *context,
                   BML_RecordRef record,
                   uint32_t fieldId,
                   T *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = {};
    RecordState *state = nullptr;
    const int status = RequireRecord(service, context, record, &state);
    if (status != BML_OK)
        return status;
    if (!state->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = state->Fields->find(fieldId);
    if (found == state->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const T *value = std::get_if<T>(&found->second);
    if (!value)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outValue = *value;
    return BML_OK;
}

template <typename T>
int GetViewField(const BML_InteropRecordView *view, uint32_t fieldId, T *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = {};
    if (!view || !view->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = view->Fields->find(fieldId);
    if (found == view->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const T *value = std::get_if<T>(&found->second);
    if (!value)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outValue = *value;
    return BML_OK;
}

int GetViewBool(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = 0;
    if (!view || !view->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = view->Fields->find(fieldId);
    if (found == view->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const InteropDetail::BoolValue *value = std::get_if<InteropDetail::BoolValue>(&found->second);
    if (!value)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outValue = value->Value;
    return BML_OK;
}

template <typename T>
int BorrowRecordScalar(const FieldValue &value,
                       const void **outData,
                       size_t *outCount,
                       size_t *outElementSize) {
    const T *typed = std::get_if<T>(&value);
    if (!typed)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outData = typed;
    *outCount = 1;
    *outElementSize = sizeof(T);
    return BML_OK;
}

int BorrowRecordBool(const FieldValue &value,
                     const void **outData,
                     size_t *outCount,
                     size_t *outElementSize) {
    const InteropDetail::BoolValue *typed = std::get_if<InteropDetail::BoolValue>(&value);
    if (!typed)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outData = &typed->Value;
    *outCount = 1;
    *outElementSize = sizeof(typed->Value);
    return BML_OK;
}

/* ArrayValue intentionally has no public C++ surface.  This helper keeps
 * the ABI conversion table in one place and returns only C layout pointers. */
template <typename T, BML_INTEROP_FIELD_TYPE Tag>
int BorrowRecordArray(const FieldValue &value,
                      const void **outData,
                      size_t *outCount,
                      size_t *outElementSize,
                      InteropDetail::ArrayValue<T, Tag> *) {
    using Array = InteropDetail::ArrayValue<T, Tag>;
    const Array *typed = std::get_if<Array>(&value);
    if (!typed)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outData = typed->Values.empty() ? nullptr : typed->Values.data();
    *outCount = typed->Values.size();
    *outElementSize = sizeof(T);
    return BML_OK;
}

int BorrowFieldValue(const FieldValue &value,
                     BML_INTEROP_FIELD_TYPE expectedType,
                     const void **outData,
                     size_t *outCount,
                     size_t *outElementSize) {
    if (!outData || !outCount || !outElementSize)
        return BML_ERROR_INVALID_PARAMETER;
    *outData = nullptr;
    *outCount = 0;
    *outElementSize = 0;
    if (TypeOf(value) != expectedType)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;

    switch (expectedType) {
    case BML_INTEROP_FIELD_BOOL:
        return BorrowRecordBool(value, outData, outCount, outElementSize);
    case BML_INTEROP_FIELD_INT:
        return BorrowRecordScalar<int>(value, outData, outCount, outElementSize);
    case BML_INTEROP_FIELD_FLOAT:
        return BorrowRecordScalar<float>(value, outData, outCount, outElementSize);
    case BML_INTEROP_FIELD_STRING:
    case BML_INTEROP_FIELD_STRING_ARRAY:
        return BML_ERROR_INTEROP_UNSUPPORTED;
    case BML_INTEROP_FIELD_OBJECT:
        return BorrowRecordScalar<BML_ObjectRef>(value, outData, outCount, outElementSize);
    case BML_INTEROP_FIELD_VEC2:
        return BorrowRecordScalar<BML_Vec2>(value, outData, outCount, outElementSize);
    case BML_INTEROP_FIELD_VEC3:
        return BorrowRecordScalar<BML_Vec3>(value, outData, outCount, outElementSize);
    case BML_INTEROP_FIELD_MAT4:
        return BorrowRecordScalar<BML_Mat4>(value, outData, outCount, outElementSize);
    case BML_INTEROP_FIELD_BOOL_ARRAY:
        return BorrowRecordArray(value, outData, outCount, outElementSize,
                                 static_cast<InteropDetail::BoolArray *>(nullptr));
    case BML_INTEROP_FIELD_INT_ARRAY:
        return BorrowRecordArray(value, outData, outCount, outElementSize,
                                 static_cast<InteropDetail::IntArray *>(nullptr));
    case BML_INTEROP_FIELD_FLOAT_ARRAY:
        return BorrowRecordArray(value, outData, outCount, outElementSize,
                                 static_cast<InteropDetail::FloatArray *>(nullptr));
    case BML_INTEROP_FIELD_OBJECT_ARRAY:
        return BorrowRecordArray(value, outData, outCount, outElementSize,
                                 static_cast<InteropDetail::ObjectArray *>(nullptr));
    case BML_INTEROP_FIELD_VEC2_ARRAY:
        return BorrowRecordArray(value, outData, outCount, outElementSize,
                                 static_cast<InteropDetail::Vec2Array *>(nullptr));
    case BML_INTEROP_FIELD_VEC3_ARRAY:
        return BorrowRecordArray(value, outData, outCount, outElementSize,
                                 static_cast<InteropDetail::Vec3Array *>(nullptr));
    case BML_INTEROP_FIELD_MAT4_ARRAY:
        return BorrowRecordArray(value, outData, outCount, outElementSize,
                                 static_cast<InteropDetail::Mat4Array *>(nullptr));
    default:
        return BML_ERROR_INVALID_PARAMETER;
    }
}

int BorrowViewField(const BML_InteropRecordView *view,
                    uint32_t fieldId,
                    BML_INTEROP_FIELD_TYPE expectedType,
                    const void **outData,
                    size_t *outCount,
                    size_t *outElementSize) {
    if (!outData || !outCount || !outElementSize)
        return BML_ERROR_INVALID_PARAMETER;
    *outData = nullptr;
    *outCount = 0;
    *outElementSize = 0;
    if (!view || !view->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = view->Fields->find(fieldId);
    if (found == view->Fields->end())
        return BML_ERROR_NOT_FOUND;
    return BorrowFieldValue(found->second, expectedType, outData, outCount, outElementSize);
}

int GetViewStringArrayCount(const BML_InteropRecordView *view,
                            uint32_t fieldId,
                            size_t *outCount) {
    if (!outCount)
        return BML_ERROR_INVALID_PARAMETER;
    *outCount = 0;
    if (!view || !view->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = view->Fields->find(fieldId);
    if (found == view->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const auto *array = std::get_if<InteropDetail::StringArray>(&found->second);
    if (!array)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outCount = array->Values.size();
    return BML_OK;
}

int GetViewStringArrayItem(const BML_InteropRecordView *view,
                           uint32_t fieldId,
                           size_t itemIndex,
                           char *buffer,
                           size_t bufferSize,
                           size_t *outRequiredSize) {
    if (outRequiredSize)
        *outRequiredSize = 0;
    if (!view || !view->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = view->Fields->find(fieldId);
    if (found == view->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const auto *array = std::get_if<InteropDetail::StringArray>(&found->second);
    if (!array)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    if (itemIndex >= array->Values.size())
        return BML_ERROR_NOT_FOUND;
    return CopyString(array->Values[itemIndex], buffer, bufferSize, outRequiredSize);
}

} // namespace

InteropRegistry::InteropRegistry(InteropSessionService &sessions)
    : m_State(std::make_unique<State>(sessions)) {}

InteropRegistry::~InteropRegistry() = default;

int InteropRegistry::RegisterProvider(const char *ownerId,
                                             const BML_InteropApiDescriptor *api,
                                             const BML_InteropProviderCallbacks *callbacks,
                                             void *userdata) {
    if (RequireGameThread(*m_State) != BML_OK)
        return BML_ERROR_INTEROP_WRONG_THREAD;
    if (!IsKeyPart(ownerId) || !api || !callbacks ||
        api->Size < sizeof(BML_InteropApiDescriptor) ||
        callbacks->Size < sizeof(BML_InteropProviderCallbacks) ||
        !IsKeyPart(api->ApiId) || api->Major == 0 || api->Hash == 0 ||
        (api->CompatibleApiHashCount > 0 && !api->CompatibleApiHashes) ||
        !api->Schemas || api->SchemaCount == 0 || !api->Endpoints || api->EndpointCount == 0) {
        return BML_ERROR_INTEROP_API_INVALID;
    }

    auto entry = std::make_shared<ApiInfo>();
    entry->ApiId = api->ApiId;
    entry->Owner = ownerId;
    entry->Major = api->Major;
    entry->Minor = api->Minor;
    entry->Hash = api->Hash;
    entry->Callbacks = *callbacks;
    entry->Userdata = userdata;

    try {
        std::unordered_set<std::string> recordNames;
        entry->CompatibleApiHashes.reserve(api->CompatibleApiHashCount);
        for (size_t index = 0; index < api->CompatibleApiHashCount; ++index) {
            const uint64_t compatibleHash = api->CompatibleApiHashes[index];
            if (compatibleHash == 0 || compatibleHash == api->Hash ||
                std::find(entry->CompatibleApiHashes.begin(), entry->CompatibleApiHashes.end(), compatibleHash) !=
                    entry->CompatibleApiHashes.end()) {
                return BML_ERROR_INTEROP_API_INVALID;
            }
            entry->CompatibleApiHashes.push_back(compatibleHash);
        }
        for (size_t recordIndex = 0; recordIndex < api->SchemaCount; ++recordIndex) {
            const BML_InteropSchemaDescriptor &input = api->Schemas[recordIndex];
            if (input.Id == 0 || !IsKeyPart(input.Name) || (input.FieldCount > 0 && !input.Fields) ||
                entry->Schemas.find(input.Id) != entry->Schemas.end() || !recordNames.emplace(input.Name).second) {
                return BML_ERROR_INTEROP_API_INVALID;
            }
            auto schema = std::make_shared<SchemaInfo>();
            schema->Id = input.Id;
            schema->Name = input.Name;
            schema->FieldNames.reserve(input.FieldCount);
            schema->Fields.reserve(input.FieldCount);
            std::unordered_set<std::string> fieldNames;
            for (size_t fieldIndex = 0; fieldIndex < input.FieldCount; ++fieldIndex) {
                const BML_InteropFieldDescriptor &field = input.Fields[fieldIndex];
                if (field.Id == 0 || !IsKeyPart(field.Name) ||
                    field.Type < BML_INTEROP_FIELD_BOOL || field.Type > BML_INTEROP_FIELD_MAT4_ARRAY ||
                    schema->FieldIndex.find(field.Id) != schema->FieldIndex.end() ||
                    !fieldNames.emplace(field.Name).second) {
                    return BML_ERROR_INTEROP_API_INVALID;
                }
                schema->FieldNames.emplace_back(field.Name);
                BML_InteropFieldDescriptor copy = field;
                copy.Name = schema->FieldNames.back().c_str();
                copy.Optional = copy.Optional ? 1 : 0;
                schema->FieldIndex.emplace(copy.Id, schema->Fields.size());
                schema->Fields.push_back(copy);
            }
            entry->Schemas.emplace(schema->Id, std::move(schema));
        }
        for (size_t endpointIndex = 0; endpointIndex < api->EndpointCount; ++endpointIndex) {
            const BML_InteropEndpointDescriptor &input = api->Endpoints[endpointIndex];
            if (!IsKeyPart(input.Name) || input.Kind < BML_INTEROP_ENDPOINT_RESOURCE ||
                input.Kind > BML_INTEROP_ENDPOINT_COMMAND || entry->Endpoints.find(input.Name) != entry->Endpoints.end()) {
                return BML_ERROR_INTEROP_API_INVALID;
            }
            const auto schema = entry->Schemas.find(input.OutputSchema);
            if (schema == entry->Schemas.end() || !schema->second)
                return BML_ERROR_INTEROP_API_INVALID;
            const bool acceptsInput = input.Kind == BML_INTEROP_ENDPOINT_QUERY || input.Kind == BML_INTEROP_ENDPOINT_COMMAND;
            if ((acceptsInput && input.InputSchema == 0) || (!acceptsInput && input.InputSchema != 0))
                return BML_ERROR_INTEROP_API_INVALID;
            std::shared_ptr<SchemaInfo> inputSchema;
            if (acceptsInput) {
                const auto inputIt = entry->Schemas.find(input.InputSchema);
                if (inputIt == entry->Schemas.end() || !inputIt->second)
                    return BML_ERROR_INTEROP_API_INVALID;
                inputSchema = inputIt->second;
            }
            auto endpoint = std::make_shared<EndpointInfo>();
            endpoint->Name = input.Name;
            endpoint->Schema = schema->second;
            endpoint->InputSchema = std::move(inputSchema);
            endpoint->Descriptor = input;
            endpoint->Descriptor.Name = endpoint->Name.c_str();
            endpoint->Descriptor.RequiresProbe = endpoint->Descriptor.RequiresProbe ? 1 : 0;
            entry->Endpoints.emplace(endpoint->Name, std::move(endpoint));
        }
        const uint64_t canonicalHash = CanonicalApiHash(*entry);
        if (canonicalHash == 0 || canonicalHash != api->Hash)
            return BML_ERROR_INTEROP_API_INVALID;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }

    std::lock_guard<std::mutex> lock(m_State->Mutex);
    if (m_State->Apis.find(entry->ApiId) != m_State->Apis.end())
        return BML_ERROR_ALREADY_EXISTS;
    m_State->Apis.emplace(entry->ApiId, std::move(entry));
    return BML_OK;
}

int InteropRegistry::UnregisterProvider(const char *ownerId, const char *apiId) {
    if (RequireGameThread(*m_State) != BML_OK)
        return BML_ERROR_INTEROP_WRONG_THREAD;
    if (!IsKeyPart(ownerId) || !IsKeyPart(apiId))
        return BML_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const auto found = m_State->Apis.find(apiId);
    if (found == m_State->Apis.end())
        return BML_ERROR_NOT_FOUND;
    if (!found->second || found->second->Owner != ownerId)
        return BML_ERROR_ACCESS_DENIED;
    for (auto &[token, stream] : m_State->Streams) {
        if (stream.ApiId == apiId) {
            stream.Closed = true;
            stream.CloseStatus = BML_ERROR_INTEROP_PROVIDER_UNLOADED;
        }
    }
    m_State->Apis.erase(found);
    return BML_OK;
}

int InteropRegistry::RequireApi(const BML_InteropCallContext *context,
                                             const char *apiId,
                                             uint32_t expectedMajor,
                                             uint64_t expectedHash) {
    if (!IsKeyPart(apiId) || expectedMajor == 0 || expectedHash == 0)
        return BML_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const int contextStatus = ValidateContext(*m_State, context, false);
    if (contextStatus != BML_OK)
        return contextStatus;
    const auto api = FindApi(*m_State, apiId);
    if (!api)
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    if (api->Major != expectedMajor)
        return BML_ERROR_INTEROP_API_MISMATCH;
    if (api->Hash == expectedHash ||
        std::find(api->CompatibleApiHashes.begin(), api->CompatibleApiHashes.end(), expectedHash) !=
            api->CompatibleApiHashes.end()) {
        return BML_OK;
    }
    return BML_ERROR_INTEROP_API_MISMATCH;
}

int InteropRegistry::InvalidateOwner(const char *ownerId) {
    /* Owner teardown runs on the same game-thread lifecycle as callbacks.
     * Refusing a foreign-thread teardown is safer than erasing userdata while
     * a provider callback may still be on the stack. */
    if (RequireGameThread(*m_State) != BML_OK)
        return BML_ERROR_INTEROP_WRONG_THREAD;
    if (!ownerId || !*ownerId)
        return BML_ERROR_INVALID_PARAMETER;
    const BML_InteropCallContext context = m_State->Sessions.CreateContextForOwner(ownerId);
    const uint64_t sessionId = context.SessionId;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    for (auto it = m_State->Apis.begin(); it != m_State->Apis.end();) {
        if (it->second && it->second->Owner == ownerId) {
            for (auto &[token, stream] : m_State->Streams) {
                if (stream.ApiId == it->first) {
                    stream.Closed = true;
                    stream.CloseStatus = BML_ERROR_INTEROP_PROVIDER_UNLOADED;
                }
            }
            it = m_State->Apis.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_State->OwnedBuilders.begin(); it != m_State->OwnedBuilders.end();) {
        const BML_InteropRecordBuilder *builder = it->second.get();
        if (builder && (builder->Owner == ownerId ||
                        (sessionId != 0 && builder->OwnerSessionId == sessionId)))
            it = m_State->OwnedBuilders.erase(it);
        else
            ++it;
    }
    if (sessionId == 0)
        return BML_OK;
    for (auto it = m_State->Streams.begin(); it != m_State->Streams.end();) {
        if (it->second.OwnerSessionId == sessionId) {
            for (uint64_t record : it->second.Queue)
                DropRecord(*m_State, record);
            it = m_State->Streams.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_State->Cursors.begin(); it != m_State->Cursors.end();) {
        if (it->second.OwnerSessionId == sessionId)
            it = m_State->Cursors.erase(it);
        else
            ++it;
    }
    for (auto it = m_State->Records.begin(); it != m_State->Records.end();) {
        if (it->second.OwnerSessionId == sessionId)
            it = m_State->Records.erase(it);
        else
            ++it;
    }
    return BML_OK;
}

bool InteropRegistry::HasStreamConsumers(const char *apiId, const char *endpoint) const {
    if (!IsKeyPart(apiId) || !IsKeyPart(endpoint))
        return false;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    for (const auto &[token, stream] : m_State->Streams) {
        (void)token;
        if (!stream.Closed && stream.ApiId == apiId && stream.Endpoint == endpoint)
            return true;
    }
    return false;
}

int InteropRegistry::ReadResource(const BML_InteropCallContext *context,
                                         const char *apiId,
                                         const char *endpoint,
                                         BML_RecordRef *outRecord) {
    return ReadSnapshot(*m_State, context, apiId, endpoint, BML_INTEROP_ENDPOINT_RESOURCE, {}, outRecord);
}

int InteropRegistry::ReadComponent(const BML_InteropCallContext *context,
                                          const char *apiId,
                                           const char *endpoint,
                                           BML_ObjectRef object,
                                           BML_RecordRef *outRecord) {
    return ReadSnapshot(*m_State, context, apiId, endpoint, BML_INTEROP_ENDPOINT_COMPONENT, object, outRecord);
}

int InteropRegistry::OpenStream(const BML_InteropCallContext *context,
                                       const char *apiId,
                                       const char *endpoint,
                                       int capacity,
                                       BML_StreamRef *outStream) {
    if (!outStream)
        return BML_ERROR_INVALID_PARAMETER;
    *outStream = {};
    if (capacity == 0)
        capacity = 256;
    if (capacity < 1 || capacity > 4096)
        return BML_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const int contextStatus = ValidateContext(*m_State, context, true);
    if (contextStatus != BML_OK)
        return contextStatus;
    const auto api = FindApi(*m_State, apiId);
    const auto endpointInfo = FindEndpoint(api, endpoint, BML_INTEROP_ENDPOINT_STREAM);
    if (!api || !endpointInfo)
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    StreamState stream;
    stream.OwnerSessionId = SessionId(*m_State, context);
    stream.ApiId = apiId;
    stream.Endpoint = endpoint;
    stream.Capacity = static_cast<size_t>(capacity);
    const uint64_t token = m_State->NextHandle++;
    m_State->Streams.emplace(token, std::move(stream));
    outStream->Value = token;
    return BML_OK;
}

int InteropRegistry::PollStream(const BML_InteropCallContext *context,
                                       BML_StreamRef streamHandle,
                                       BML_RecordRef *outRecord) {
    if (!outRecord)
        return BML_ERROR_INVALID_PARAMETER;
    *outRecord = {};
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    StreamState *stream = nullptr;
    const int status = RequireStream(*m_State, context, streamHandle, &stream);
    if (status != BML_OK)
        return status;
    if (!stream->Queue.empty()) {
        outRecord->Value = stream->Queue.front();
        stream->Queue.pop_front();
        return BML_OK;
    }
    return stream->Closed ? (stream->CloseStatus == BML_OK ? BML_ERROR_INTEROP_HANDLE_STALE : stream->CloseStatus)
                          : BML_OK;
}

int InteropRegistry::DroppedStreamCount(const BML_InteropCallContext *context,
                                               BML_StreamRef streamHandle,
                                               int *outCount) {
    if (!outCount)
        return BML_ERROR_INVALID_PARAMETER;
    *outCount = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    StreamState *stream = nullptr;
    const int status = RequireStream(*m_State, context, streamHandle, &stream);
    if (status != BML_OK)
        return status;
    *outCount = stream->Dropped > static_cast<uint64_t>((std::numeric_limits<int>::max)())
                    ? (std::numeric_limits<int>::max)()
                    : static_cast<int>(stream->Dropped);
    return BML_OK;
}

int InteropRegistry::CloseStream(const BML_InteropCallContext *context, BML_StreamRef streamHandle) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    StreamState *stream = nullptr;
    const int status = RequireStream(*m_State, context, streamHandle, &stream);
    if (status != BML_OK)
        return status;
    for (uint64_t record : stream->Queue)
        DropRecord(*m_State, record);
    m_State->Streams.erase(streamHandle.Value);
    return BML_OK;
}

int InteropRegistry::OpenCollection(const BML_InteropCallContext *context,
                                           const char *apiId,
                                           const char *endpoint,
                                           BML_CursorRef *outCursor) {
    if (!outCursor)
        return BML_ERROR_INVALID_PARAMETER;
    *outCursor = {};
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const int contextStatus = ValidateContext(*m_State, context, true);
    if (contextStatus != BML_OK)
        return contextStatus;
    const auto api = FindApi(*m_State, apiId);
    const auto endpointInfo = FindEndpoint(api, endpoint, BML_INTEROP_ENDPOINT_COLLECTION);
    if (!api || !endpointInfo)
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    CursorState cursor;
    cursor.OwnerSessionId = SessionId(*m_State, context);
    cursor.ApiId = apiId;
    cursor.Endpoint = endpoint;
    const uint64_t token = m_State->NextHandle++;
    m_State->Cursors.emplace(token, std::move(cursor));
    outCursor->Value = token;
    return BML_OK;
}

int InteropRegistry::ReadCollectionPage(const BML_InteropCallContext *context,
                                               BML_CursorRef cursorHandle,
                                               BML_RecordRef *outRecords,
                                               size_t capacity,
                                               size_t *outCount,
                                               int *outComplete) {
    if (outCount) *outCount = 0;
    if (outComplete) *outComplete = 0;
    if (!outRecords || capacity == 0 || !outCount || !outComplete)
        return BML_ERROR_INVALID_PARAMETER;
    const int threadStatus = RequireGameThread(*m_State);
    if (threadStatus != BML_OK)
        return threadStatus;

    CursorState cursorSnapshot;
    std::shared_ptr<ApiInfo> api;
    std::shared_ptr<EndpointInfo> endpoint;
    BML_InteropPageCallback callback = nullptr;
    {
        std::unique_lock<std::mutex> lock(m_State->Mutex);
        CursorState *cursor = nullptr;
        const int status = RequireCursor(*m_State, context, cursorHandle, &cursor);
        if (status != BML_OK)
            return status;
        if (cursor->Complete) {
            *outComplete = 1;
            return BML_OK;
        }
        cursorSnapshot = *cursor;
        api = FindApi(*m_State, cursorSnapshot.ApiId.c_str());
        endpoint = FindEndpoint(api, cursorSnapshot.Endpoint.c_str(), BML_INTEROP_ENDPOINT_COLLECTION);
        callback = api ? api->Callbacks.ReadCollection : nullptr;
        if (!api || !endpoint || !callback)
            return BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    }

    const BML_InteropProviderRequest request = MakeRequest(
        api, endpoint, context, {}, cursorSnapshot.Offset,
        static_cast<uint32_t>((std::min)(capacity, static_cast<size_t>((std::numeric_limits<uint32_t>::max)()))));
    int status = Probe(api, endpoint, request);
    if (status != BML_OK)
        return status;
    {
        std::unique_lock<std::mutex> lock(m_State->Mutex);
        CursorState *cursor = nullptr;
        status = RequireCursor(*m_State, context, cursorHandle, &cursor);
        if (status != BML_OK)
            return status;
        if (!IsLiveApi(*m_State, api))
            return BML_ERROR_INTEROP_PROVIDER_UNLOADED;
        if (cursor->Offset != cursorSnapshot.Offset ||
            cursor->ApiId != cursorSnapshot.ApiId ||
            cursor->Endpoint != cursorSnapshot.Endpoint) {
            return BML_ERROR_INTEROP_CURSOR_STALE;
        }
    }
    BML_InteropPageBuilder page;
    page.Schema = endpoint->Schema;
    status = InvokeProviderCallback(callback, &request, &page, api->Userdata);
    if (status != BML_OK)
        return status;

    std::unique_lock<std::mutex> lock(m_State->Mutex);
    CursorState *cursor = nullptr;
    status = RequireCursor(*m_State, context, cursorHandle, &cursor);
    if (status != BML_OK)
        return status;
    if (!IsLiveApi(*m_State, api))
        return BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    /* A nested read of this cursor is well-defined as the inner read; do not
     * let the outer callback overwrite the cursor's newer offset. */
    if (cursor->Offset != cursorSnapshot.Offset ||
        cursor->ApiId != cursorSnapshot.ApiId ||
        cursor->Endpoint != cursorSnapshot.Endpoint) {
        return BML_ERROR_INTEROP_CURSOR_STALE;
    }
    if (page.Records.size() > capacity)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const uint64_t timestamp = TimestampNow();
    for (size_t index = 0; index < page.Records.size(); ++index) {
        std::unordered_map<uint32_t, FieldValue> fields;
        status = ValidateBuilder(endpoint->Schema, *page.Records[index], fields);
        if (status != BML_OK) {
            for (size_t rollback = 0; rollback < index; ++rollback) {
                DropRecord(*m_State, outRecords[rollback].Value);
                outRecords[rollback] = {};
            }
            return status;
        }
        outRecords[index].Value = StoreRecord(*m_State,
                                              cursor->OwnerSessionId,
                                              endpoint->Schema,
                                              std::move(fields),
                                              0,
                                              timestamp);
    }
    *outCount = page.Records.size();
    cursor->Offset += page.Records.size();
    cursor->Complete = page.Complete;
    *outComplete = cursor->Complete ? 1 : 0;
    return BML_OK;
}

int InteropRegistry::CloseCollection(const BML_InteropCallContext *context, BML_CursorRef cursorHandle) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    CursorState *cursor = nullptr;
    const int status = RequireCursor(*m_State, context, cursorHandle, &cursor);
    if (status != BML_OK)
        return status;
    m_State->Cursors.erase(cursorHandle.Value);
    return BML_OK;
}

int InteropRegistry::CreateStreamRecord(const char *ownerId,
                                               const char *apiId,
                                               const char *endpointName,
                                               BML_InteropRecordBuilder **outRecord) {
    if (!outRecord)
        return BML_ERROR_INVALID_PARAMETER;
    *outRecord = nullptr;
    if (RequireGameThread(*m_State) != BML_OK)
        return BML_ERROR_INTEROP_WRONG_THREAD;
    if (!IsKeyPart(ownerId))
        return BML_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const auto api = FindApi(*m_State, apiId);
    const auto endpoint = FindEndpoint(api, endpointName, BML_INTEROP_ENDPOINT_STREAM);
    if (!api || !endpoint)
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    if (api->Owner != ownerId)
        return BML_ERROR_ACCESS_DENIED;
    try {
        auto owned = std::make_unique<BML_InteropRecordBuilder>();
        auto *record = owned.get();
        record->Schema = endpoint->Schema;
        record->Owner = ownerId;
        record->ApiId = apiId;
        record->Endpoint = endpointName;
        record->StandaloneStreamRecord = true;
        m_State->OwnedBuilders.emplace(record, std::move(owned));
        *outRecord = record;
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

int InteropRegistry::CreateInputRecord(const BML_InteropCallContext *context,
                                              const char *apiId,
                                              uint32_t schemaId,
                                              BML_InteropRecordBuilder **outRecord) {
    if (!outRecord)
        return BML_ERROR_INVALID_PARAMETER;
    *outRecord = nullptr;
    if (RequireGameThread(*m_State) != BML_OK)
        return BML_ERROR_INTEROP_WRONG_THREAD;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const int contextStatus = ValidateContext(*m_State, context, true);
    if (contextStatus != BML_OK)
        return contextStatus;
    const auto api = FindApi(*m_State, apiId);
    if (!api)
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    const auto schema = api->Schemas.find(schemaId);
    if (schema == api->Schemas.end() || !schema->second)
        return BML_ERROR_INTEROP_SCHEMA_MISMATCH;
    try {
        auto owned = std::make_unique<BML_InteropRecordBuilder>();
        auto *record = owned.get();
        record->Schema = schema->second;
        record->OwnerSessionId = SessionId(*m_State, context);
        record->ApiId = apiId;
        record->InputRecord = true;
        m_State->OwnedBuilders.emplace(record, std::move(owned));
        *outRecord = record;
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

int InteropRegistry::Invoke(const BML_InteropCallContext *context,
                                   const char *apiId,
                                   const char *endpointName,
                                   BML_INTEROP_ENDPOINT_KIND kind,
                                   const BML_InteropRecordBuilder *input,
                                   BML_RecordRef *outRecord) {
    if (!outRecord)
        return BML_ERROR_INVALID_PARAMETER;
    *outRecord = {};
    if (RequireGameThread(*m_State) != BML_OK)
        return BML_ERROR_INTEROP_WRONG_THREAD;

    std::shared_ptr<ApiInfo> api;
    std::shared_ptr<EndpointInfo> endpoint;
    std::unordered_map<uint32_t, FieldValue> inputFields;
    BML_InteropReadCallback callback = nullptr;
    uint64_t ownerSessionId = 0;
    {
        std::unique_lock<std::mutex> lock(m_State->Mutex);
        const int contextStatus = ValidateContext(*m_State, context, true);
        if (contextStatus != BML_OK)
            return contextStatus;
        api = FindApi(*m_State, apiId);
        endpoint = FindEndpoint(api, endpointName, kind);
        if (!api || !endpoint)
            return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
        ownerSessionId = SessionId(*m_State, context);
        if (!input || !input->InputRecord || !endpoint->InputSchema ||
            input->OwnerSessionId != ownerSessionId) {
            return BML_ERROR_INTEROP_HANDLE_STALE;
        }
        if (!input->Schema || input->Schema->Id != endpoint->InputSchema->Id || input->ApiId != apiId)
            return BML_ERROR_INTEROP_SCHEMA_MISMATCH;

        const int inputStatus = ValidateBuilder(endpoint->InputSchema, *input, inputFields);
        if (inputStatus != BML_OK)
            return inputStatus;
        callback = kind == BML_INTEROP_ENDPOINT_QUERY
                       ? api->Callbacks.InvokeQuery
                       : api->Callbacks.InvokeCommand;
        if (!callback)
            return BML_ERROR_INTEROP_UNSUPPORTED;
    }

    BML_InteropRecordView view;
    view.Schema = endpoint->InputSchema;
    view.Fields = &inputFields;
    const BML_InteropProviderRequest request = MakeRequest(api, endpoint, context, {}, 0, 0, &view);
    int status = Probe(api, endpoint, request);
    if (status != BML_OK)
        return status;
    {
        std::unique_lock<std::mutex> lock(m_State->Mutex);
        if (!IsLiveApi(*m_State, api))
            return BML_ERROR_INTEROP_PROVIDER_UNLOADED;
        const int contextStatus = ValidateContext(*m_State, context, true);
        if (contextStatus != BML_OK)
            return contextStatus;
        if (SessionId(*m_State, context) != ownerSessionId)
            return BML_ERROR_INTEROP_HANDLE_STALE;
    }
    BML_InteropRecordBuilder output;
    output.Schema = endpoint->Schema;
    status = InvokeProviderCallback(callback, &request, &output, api->Userdata);
    if (status != BML_OK)
        return status;

    std::unique_lock<std::mutex> lock(m_State->Mutex);
    if (!IsLiveApi(*m_State, api))
        return BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    const int contextStatus = ValidateContext(*m_State, context, true);
    if (contextStatus != BML_OK)
        return contextStatus;
    if (SessionId(*m_State, context) != ownerSessionId)
        return BML_ERROR_INTEROP_HANDLE_STALE;

    std::unordered_map<uint32_t, FieldValue> outputFields;
    status = ValidateBuilder(endpoint->Schema, output, outputFields);
    if (status != BML_OK)
        return status;
    outRecord->Value = StoreRecord(*m_State,
                                   ownerSessionId,
                                   endpoint->Schema,
                                   std::move(outputFields),
                                   0,
                                   TimestampNow());
    return BML_OK;
}

int InteropRegistry::Publish(const char *ownerId, BML_InteropRecordBuilder *record) {
    if (RequireGameThread(*m_State) != BML_OK)
        return BML_ERROR_INTEROP_WRONG_THREAD;
    if (!IsKeyPart(ownerId) || !record)
        return BML_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const auto api = FindApi(*m_State, record->ApiId.c_str());
    const auto endpoint = FindEndpoint(api, record->Endpoint.c_str(), BML_INTEROP_ENDPOINT_STREAM);
    if (!api || !endpoint)
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    if (api->Owner != ownerId || record->Owner != ownerId || !record->StandaloneStreamRecord) {
        return BML_ERROR_ACCESS_DENIED;
    }
    FieldMap fields;
    const int validation = ValidateBuilder(endpoint->Schema, *record, fields);
    if (validation != BML_OK)
        return validation;
    const uint64_t sequence = m_State->NextSequence++;
    const uint64_t timestamp = TimestampNow();
    const auto payload = std::make_shared<const FieldMap>(std::move(fields));
    for (auto &[token, stream] : m_State->Streams) {
        if (stream.Closed || stream.ApiId != record->ApiId || stream.Endpoint != record->Endpoint)
            continue;
        const uint64_t recordToken = StoreRecordPayload(*m_State,
                                                        stream.OwnerSessionId,
                                                        endpoint->Schema,
                                                        payload,
                                                        sequence,
                                                        timestamp);
        if (stream.Queue.size() >= stream.Capacity) {
            DropRecord(*m_State, stream.Queue.front());
            stream.Queue.pop_front();
            ++stream.Dropped;
        }
        stream.Queue.push_back(recordToken);
    }
    return BML_OK;
}

int InteropRegistry::DestroyRecordBuilder(BML_InteropRecordBuilder *record) {
    if (!record)
        return BML_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const auto found = m_State->OwnedBuilders.find(record);
    if (found == m_State->OwnedBuilders.end())
        return BML_ERROR_INVALID_PARAMETER;
    m_State->OwnedBuilders.erase(found);
    return BML_OK;
}

int InteropRegistry::BuilderSetValue(BML_InteropRecordBuilder *record,
                                            uint32_t fieldId,
                                            BML_INTEROP_FIELD_TYPE type,
                                            const void *data,
                                            size_t count) {
    if (!record || fieldId == 0 || type < BML_INTEROP_FIELD_BOOL || type > BML_INTEROP_FIELD_MAT4_ARRAY ||
        (!data && count > 0)) {
        return BML_ERROR_INVALID_PARAMETER;
    }
    try {
        FieldValue value;
        switch (type) {
    case BML_INTEROP_FIELD_BOOL:
            if (!data || count != 1) return BML_ERROR_INVALID_PARAMETER;
            value = InteropDetail::BoolValue{*static_cast<const int *>(data) != 0 ? 1 : 0};
            break;
        case BML_INTEROP_FIELD_INT:
            if (!data || count != 1) return BML_ERROR_INVALID_PARAMETER;
            value = *static_cast<const int *>(data);
            break;
        case BML_INTEROP_FIELD_FLOAT:
            if (!data || count != 1) return BML_ERROR_INVALID_PARAMETER;
            value = *static_cast<const float *>(data);
            break;
        case BML_INTEROP_FIELD_STRING:
            value = std::string(data ? static_cast<const char *>(data) : "", data ? count : 0);
            break;
        case BML_INTEROP_FIELD_OBJECT:
            if (!data || count != 1) return BML_ERROR_INVALID_PARAMETER;
            value = *static_cast<const BML_ObjectRef *>(data);
            break;
        case BML_INTEROP_FIELD_VEC2:
            if (!data || count != 1) return BML_ERROR_INVALID_PARAMETER;
            value = *static_cast<const BML_Vec2 *>(data);
            break;
        case BML_INTEROP_FIELD_VEC3:
            if (!data || count != 1) return BML_ERROR_INVALID_PARAMETER;
            value = *static_cast<const BML_Vec3 *>(data);
            break;
        case BML_INTEROP_FIELD_MAT4:
            if (!data || count != 1) return BML_ERROR_INVALID_PARAMETER;
            value = *static_cast<const BML_Mat4 *>(data);
            break;
        case BML_INTEROP_FIELD_BOOL_ARRAY: {
            InteropDetail::BoolArray values;
            const int *items = static_cast<const int *>(data);
            values.Values.reserve(count);
            for (size_t index = 0; index < count; ++index) values.Values.push_back(items[index] != 0 ? 1 : 0);
            value = std::move(values);
            break;
        }
        case BML_INTEROP_FIELD_INT_ARRAY: {
            InteropDetail::IntArray values;
            if (count > 0) {
                const int *items = static_cast<const int *>(data);
                values.Values.assign(items, items + count);
            }
            value = std::move(values);
            break;
        }
        case BML_INTEROP_FIELD_FLOAT_ARRAY: {
            InteropDetail::FloatArray values;
            if (count > 0) {
                const float *items = static_cast<const float *>(data);
                values.Values.assign(items, items + count);
            }
            value = std::move(values);
            break;
        }
        case BML_INTEROP_FIELD_OBJECT_ARRAY: {
            InteropDetail::ObjectArray values;
            if (count > 0) {
                const BML_ObjectRef *items = static_cast<const BML_ObjectRef *>(data);
                values.Values.assign(items, items + count);
            }
            value = std::move(values);
            break;
        }
        case BML_INTEROP_FIELD_VEC2_ARRAY: {
            InteropDetail::Vec2Array values;
            if (count > 0) {
                const BML_Vec2 *items = static_cast<const BML_Vec2 *>(data);
                values.Values.assign(items, items + count);
            }
            value = std::move(values);
            break;
        }
        case BML_INTEROP_FIELD_VEC3_ARRAY: {
            InteropDetail::Vec3Array values;
            if (count > 0) {
                const BML_Vec3 *items = static_cast<const BML_Vec3 *>(data);
                values.Values.assign(items, items + count);
            }
            value = std::move(values);
            break;
        }
        case BML_INTEROP_FIELD_MAT4_ARRAY: {
            InteropDetail::Mat4Array values;
            if (count > 0) {
                const BML_Mat4 *items = static_cast<const BML_Mat4 *>(data);
                values.Values.assign(items, items + count);
            }
            value = std::move(values);
            break;
        }
        case BML_INTEROP_FIELD_STRING_ARRAY:
            return BML_ERROR_INVALID_PARAMETER;
        }
        for (auto &[existingId, existingValue] : record->Fields) {
            if (existingId == fieldId) {
                existingValue = std::move(value);
                return BML_OK;
            }
        }
        record->Fields.emplace_back(fieldId, std::move(value));
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

int InteropRegistry::BuilderSetStringArray(BML_InteropRecordBuilder *record,
                                                  uint32_t fieldId,
                                                  const char *const *values,
                                                  const size_t *sizes,
                                                  size_t count) {
    if (!record || fieldId == 0 || (count > 0 && (!values || !sizes)))
        return BML_ERROR_INVALID_PARAMETER;
    try {
        InteropDetail::StringArray copy;
        copy.Values.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            if (!values[index] && sizes[index] > 0)
                return BML_ERROR_INVALID_PARAMETER;
            copy.Values.emplace_back(values[index] ? values[index] : "", sizes[index]);
        }
        for (auto &[existingId, existingValue] : record->Fields) {
            if (existingId == fieldId) {
                existingValue = std::move(copy);
                return BML_OK;
            }
        }
        record->Fields.emplace_back(fieldId, std::move(copy));
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

BML_InteropRecordBuilder *InteropRegistry::PageAppend(BML_InteropPageBuilder *page) {
    if (!page || !page->Schema)
        return nullptr;
    try {
        auto record = std::make_unique<BML_InteropRecordBuilder>();
        record->Schema = page->Schema;
        BML_InteropRecordBuilder *raw = record.get();
        page->Records.push_back(std::move(record));
        return raw;
    } catch (...) {
        return nullptr;
    }
}

void InteropRegistry::PageFinish(BML_InteropPageBuilder *page, int complete) {
    if (page)
        page->Complete = complete != 0;
}

int InteropRegistry::RecordViewGetBool(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue) {
    return GetViewBool(view, fieldId, outValue);
}

int InteropRegistry::RecordViewGetInt(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue) {
    return GetViewField(view, fieldId, outValue);
}

int InteropRegistry::RecordViewGetFloat(const BML_InteropRecordView *view, uint32_t fieldId, float *outValue) {
    return GetViewField(view, fieldId, outValue);
}

int InteropRegistry::RecordViewGetString(const BML_InteropRecordView *view,
                                                 uint32_t fieldId,
                                                 char *buffer,
                                                 size_t bufferSize,
                                                 size_t *outRequiredSize) {
    if (outRequiredSize)
        *outRequiredSize = 0;
    if (!view || !view->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = view->Fields->find(fieldId);
    if (found == view->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const std::string *value = std::get_if<std::string>(&found->second);
    return value ? CopyString(*value, buffer, bufferSize, outRequiredSize) : BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
}

int InteropRegistry::RecordViewGetObject(const BML_InteropRecordView *view,
                                                 uint32_t fieldId,
                                                 BML_ObjectRef *outValue) {
    return GetViewField(view, fieldId, outValue);
}

int InteropRegistry::RecordViewGetVec2(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec2 *outValue) {
    return GetViewField(view, fieldId, outValue);
}

int InteropRegistry::RecordViewGetVec3(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec3 *outValue) {
    return GetViewField(view, fieldId, outValue);
}

int InteropRegistry::RecordViewGetMat4(const BML_InteropRecordView *view, uint32_t fieldId, BML_Mat4 *outValue) {
    return GetViewField(view, fieldId, outValue);
}

int InteropRegistry::RecordViewBorrowValue(const BML_InteropRecordView *view,
                                                  uint32_t fieldId,
                                                  BML_INTEROP_FIELD_TYPE expectedType,
                                                  const void **outData,
                                                  size_t *outCount,
                                                  size_t *outElementSize) {
    return BorrowViewField(view, fieldId, expectedType, outData, outCount, outElementSize);
}

int InteropRegistry::RecordViewGetStringArrayItem(const BML_InteropRecordView *view,
                                                         uint32_t fieldId,
                                                         size_t itemIndex,
                                                         char *buffer,
                                                         size_t bufferSize,
                                                         size_t *outRequiredSize) {
    return GetViewStringArrayItem(view, fieldId, itemIndex, buffer, bufferSize, outRequiredSize);
}

int InteropRegistry::RecordViewGetStringArrayCount(const BML_InteropRecordView *view,
                                                          uint32_t fieldId,
                                                          size_t *outCount) {
    return GetViewStringArrayCount(view, fieldId, outCount);
}

int InteropRegistry::RetainRecord(const BML_InteropCallContext *context, BML_RecordRef record) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status == BML_OK)
        ++state->RefCount;
    return status;
}

int InteropRegistry::ReleaseRecord(const BML_InteropCallContext *context, BML_RecordRef record) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    const int status = RequireRecord(*m_State, context, record, nullptr);
    if (status == BML_OK)
        DropRecord(*m_State, record.Value);
    return status;
}

int InteropRegistry::RecordSchema(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t *outSchema) {
    if (!outSchema) return BML_ERROR_INVALID_PARAMETER;
    *outSchema = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status == BML_OK) *outSchema = state->Schema->Id;
    return status;
}

int InteropRegistry::RecordSequence(const BML_InteropCallContext *context, BML_RecordRef record, uint64_t *outSequence) {
    if (!outSequence) return BML_ERROR_INVALID_PARAMETER;
    *outSequence = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status == BML_OK) *outSequence = state->Sequence;
    return status;
}

int InteropRegistry::RecordTimestamp(const BML_InteropCallContext *context, BML_RecordRef record, uint64_t *outTimestamp) {
    if (!outTimestamp) return BML_ERROR_INVALID_PARAMETER;
    *outTimestamp = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status == BML_OK) *outTimestamp = state->Timestamp;
    return status;
}

int InteropRegistry::RecordGetBool(const BML_InteropCallContext *context,
                                          BML_RecordRef record,
                                          uint32_t fieldId,
                                          int *outValue) {
    if (!outValue) return BML_ERROR_INVALID_PARAMETER;
    *outValue = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status != BML_OK)
        return status;
    if (!state->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = state->Fields->find(fieldId);
    if (found == state->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const InteropDetail::BoolValue *value = std::get_if<InteropDetail::BoolValue>(&found->second);
    if (!value)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outValue = value->Value;
    return BML_OK;
}

int InteropRegistry::RecordGetInt(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, int *outValue) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    return GetRecordField(*m_State, context, record, fieldId, outValue);
}

int InteropRegistry::RecordGetFloat(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, float *outValue) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    return GetRecordField(*m_State, context, record, fieldId, outValue);
}

int InteropRegistry::RecordGetString(const BML_InteropCallContext *context,
                                            BML_RecordRef record,
                                            uint32_t fieldId,
                                            char *buffer,
                                            size_t bufferSize,
                                            size_t *outRequiredSize) {
    if (outRequiredSize) *outRequiredSize = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status != BML_OK) return status;
    if (!state->Fields) return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = state->Fields->find(fieldId);
    if (found == state->Fields->end()) return BML_ERROR_NOT_FOUND;
    const std::string *value = std::get_if<std::string>(&found->second);
    return value ? CopyString(*value, buffer, bufferSize, outRequiredSize) : BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
}

int InteropRegistry::RecordBorrowValue(const BML_InteropCallContext *context,
                                              BML_RecordRef record,
                                              uint32_t fieldId,
                                              BML_INTEROP_FIELD_TYPE expectedType,
                                              const void **outData,
                                              size_t *outCount,
                                              size_t *outElementSize) {
    if (!outData || !outCount || !outElementSize)
        return BML_ERROR_INVALID_PARAMETER;
    *outData = nullptr;
    *outCount = 0;
    *outElementSize = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status != BML_OK) {
        g_BorrowedRecordPayload.reset();
        return status;
    }
    if (!state->Fields) {
        g_BorrowedRecordPayload.reset();
        return BML_ERROR_INTEROP_RECORD_INVALID;
    }
    g_BorrowedRecordPayload = state->Fields;
    const auto found = g_BorrowedRecordPayload->find(fieldId);
    if (found == g_BorrowedRecordPayload->end()) {
        g_BorrowedRecordPayload.reset();
        return BML_ERROR_NOT_FOUND;
    }
    const int borrowStatus = BorrowFieldValue(found->second, expectedType,
                                              outData, outCount, outElementSize);
    if (borrowStatus != BML_OK)
        g_BorrowedRecordPayload.reset();
    return borrowStatus;
}

int InteropRegistry::RecordGetStringArrayItem(const BML_InteropCallContext *context,
                                                     BML_RecordRef record,
                                                     uint32_t fieldId,
                                                     size_t itemIndex,
                                                     char *buffer,
                                                     size_t bufferSize,
                                                     size_t *outRequiredSize) {
    if (outRequiredSize)
        *outRequiredSize = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status != BML_OK)
        return status;
    if (!state->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = state->Fields->find(fieldId);
    if (found == state->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const InteropDetail::StringArray *array = std::get_if<InteropDetail::StringArray>(&found->second);
    if (!array)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    if (itemIndex >= array->Values.size())
        return BML_ERROR_NOT_FOUND;
    return CopyString(array->Values[itemIndex], buffer, bufferSize, outRequiredSize);
}

int InteropRegistry::RecordGetStringArrayCount(const BML_InteropCallContext *context,
                                                      BML_RecordRef record,
                                                      uint32_t fieldId,
                                                      size_t *outCount) {
    if (!outCount)
        return BML_ERROR_INVALID_PARAMETER;
    *outCount = 0;
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    RecordState *state = nullptr;
    const int status = RequireRecord(*m_State, context, record, &state);
    if (status != BML_OK)
        return status;
    if (!state->Fields)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const auto found = state->Fields->find(fieldId);
    if (found == state->Fields->end())
        return BML_ERROR_NOT_FOUND;
    const InteropDetail::StringArray *array = std::get_if<InteropDetail::StringArray>(&found->second);
    if (!array)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    *outCount = array->Values.size();
    return BML_OK;
}

int InteropRegistry::RecordGetObject(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, BML_ObjectRef *outValue) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    return GetRecordField(*m_State, context, record, fieldId, outValue);
}

int InteropRegistry::RecordGetVec2(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, BML_Vec2 *outValue) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    return GetRecordField(*m_State, context, record, fieldId, outValue);
}

int InteropRegistry::RecordGetVec3(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, BML_Vec3 *outValue) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    return GetRecordField(*m_State, context, record, fieldId, outValue);
}

int InteropRegistry::RecordGetMat4(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, BML_Mat4 *outValue) {
    std::lock_guard<std::mutex> lock(m_State->Mutex);
    return GetRecordField(*m_State, context, record, fieldId, outValue);
}

} // namespace BML
