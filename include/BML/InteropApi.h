#ifndef BML_INTEROP_API_H
#define BML_INTEROP_API_H

#include "BML/InteropTypes.h"

/*
 * API ABI
 * ------------
 * Every symbol that crosses a DLL boundary below is C ABI and fixed-layout.
 * C++ conveniences live in BML/InteropClient.h as inline wrappers only; they
 * do not export classes, vtables, allocators, STL containers, or C++
 * functions.
 */

BML_BEGIN_CDECLS

typedef enum BML_INTEROP_FIELD_TYPE {
    BML_INTEROP_FIELD_BOOL = 1,
    BML_INTEROP_FIELD_INT,
    BML_INTEROP_FIELD_FLOAT,
    BML_INTEROP_FIELD_STRING,
    BML_INTEROP_FIELD_OBJECT,
    BML_INTEROP_FIELD_VEC2,
    BML_INTEROP_FIELD_VEC3,
    BML_INTEROP_FIELD_MAT4,
    BML_INTEROP_FIELD_BOOL_ARRAY,
    BML_INTEROP_FIELD_INT_ARRAY,
    BML_INTEROP_FIELD_FLOAT_ARRAY,
    BML_INTEROP_FIELD_STRING_ARRAY,
    BML_INTEROP_FIELD_OBJECT_ARRAY,
    BML_INTEROP_FIELD_VEC2_ARRAY,
    BML_INTEROP_FIELD_VEC3_ARRAY,
    BML_INTEROP_FIELD_MAT4_ARRAY,
} BML_INTEROP_FIELD_TYPE;

typedef enum BML_INTEROP_ENDPOINT_KIND {
    BML_INTEROP_ENDPOINT_RESOURCE = 1,
    BML_INTEROP_ENDPOINT_COMPONENT,
    BML_INTEROP_ENDPOINT_COLLECTION,
    BML_INTEROP_ENDPOINT_STREAM,
    BML_INTEROP_ENDPOINT_QUERY,
    BML_INTEROP_ENDPOINT_COMMAND,
} BML_INTEROP_ENDPOINT_KIND;

/* Domain reserved by BML's scene provider for Virtools object identities.
 * Other providers choose their own non-zero domains. */
#define BML_INTEROP_OBJECT_DOMAIN_VIRTOOLS 1u

typedef struct BML_InteropFieldDescriptor {
    uint32_t Id;
    const char *Name;
    BML_INTEROP_FIELD_TYPE Type;
    int Optional;
} BML_InteropFieldDescriptor;

typedef struct BML_InteropSchemaDescriptor {
    uint32_t Id;
    const char *Name;
    const BML_InteropFieldDescriptor *Fields;
    size_t FieldCount;
} BML_InteropSchemaDescriptor;

typedef struct BML_InteropEndpointDescriptor {
    const char *Name;
    BML_INTEROP_ENDPOINT_KIND Kind;
    /* Zero for endpoints without input. QUERY and COMMAND require an input
     * record schema; the other built-in endpoint kinds must use zero. */
    uint32_t InputSchema;
    uint32_t OutputSchema;
    int RequiresProbe;
} BML_InteropEndpointDescriptor;

typedef struct BML_InteropApiDescriptor {
    size_t Size;
    const char *ApiId;
    uint32_t Major;
    uint32_t Minor;
    uint64_t Hash;
    const BML_InteropSchemaDescriptor *Schemas;
    size_t SchemaCount;
    const BML_InteropEndpointDescriptor *Endpoints;
    size_t EndpointCount;
    /* Hashes of older, append-only-compatible descriptors from this major.
     * This is explicit rollout metadata, not a C++ ABI promise. */
    const uint64_t *CompatibleApiHashes;
    size_t CompatibleApiHashCount;
} BML_InteropApiDescriptor;

typedef struct BML_InteropProviderRequest {
    size_t Size;
    const char *ApiId;
    const char *Endpoint;
    BML_INTEROP_ENDPOINT_KIND Kind;
    BML_ObjectRef Object;
    uint64_t Offset;
    uint32_t Limit;
    const char *ConsumerId;
    /* Non-null only while a QUERY or COMMAND provider callback runs. */
    const struct BML_InteropRecordView *Input;
} BML_InteropProviderRequest;

typedef struct BML_InteropRecordBuilder BML_InteropRecordBuilder;
typedef struct BML_InteropPageBuilder BML_InteropPageBuilder;
typedef struct BML_InteropRecordView BML_InteropRecordView;

typedef int (*BML_InteropProbeCallback)(const BML_InteropProviderRequest *request, void *userdata);
typedef int (*BML_InteropReadCallback)(const BML_InteropProviderRequest *request,
                                       BML_InteropRecordBuilder *record,
                                       void *userdata);
typedef int (*BML_InteropPageCallback)(const BML_InteropProviderRequest *request,
                                       BML_InteropPageBuilder *page,
                                       void *userdata);

typedef struct BML_InteropProviderCallbacks {
    size_t Size;
    BML_InteropProbeCallback Probe;
    BML_InteropReadCallback ReadResource;
    BML_InteropReadCallback ReadComponent;
    BML_InteropPageCallback ReadCollection;
    BML_InteropReadCallback InvokeQuery;
    BML_InteropReadCallback InvokeCommand;
} BML_InteropProviderCallbacks;

/* The owner is inferred from the calling DLL and must be an active native
 * mod. Registration and unregistration run on BML's game thread; a foreign
 * thread receives BML_ERROR_INTEROP_WRONG_THREAD.  BML recomputes the
 * canonical descriptor hash at registration and rejects a mismatched Hash.
 * All callbacks and userdata are revoked before that DLL unloads. */
BML_EXPORT int BML_Interop_RegisterProvider(const BML_InteropApiDescriptor *api,
                                            const BML_InteropProviderCallbacks *callbacks,
                                            void *userdata);
BML_EXPORT int BML_Interop_UnregisterProvider(const char *apiId);

/* Generated consumers call this before using a typed facade. Matching uses
 * API ID, major version, and canonical descriptor hash. A provider may
 * explicitly accept an older append-only-compatible hash through its fixed C
 * descriptor; no C++ ABI compatibility is assumed. */
BML_EXPORT int BML_Interop_RequireApi(const char *apiId,
                                      uint32_t expectedMajor,
                                      uint64_t expectedHash);

/* Builders are valid only for their callback/publish operation. */
BML_EXPORT int BML_Interop_RecordBuilder_SetValue(BML_InteropRecordBuilder *record,
                                                  uint32_t fieldId,
                                                  BML_INTEROP_FIELD_TYPE type,
                                                  const void *data,
                                                  size_t count);
BML_EXPORT int BML_Interop_RecordBuilder_SetStringArray(BML_InteropRecordBuilder *record,
                                                        uint32_t fieldId,
                                                        const char *const *values,
                                                        const size_t *sizes,
                                                        size_t count);
BML_EXPORT BML_InteropRecordBuilder *BML_Interop_PageBuilder_Append(BML_InteropPageBuilder *page);
BML_EXPORT void BML_Interop_PageBuilder_Finish(BML_InteropPageBuilder *page, int complete);

/* A stream provider creates an independent snapshot, fills it with the same
 * builder functions, then publishes it. */
BML_EXPORT int BML_Interop_CreateStreamRecord(const char *apiId,
                                              const char *endpoint,
                                              BML_InteropRecordBuilder **outRecord);
BML_EXPORT int BML_Interop_PublishStreamRecord(BML_InteropRecordBuilder *record);
BML_EXPORT int BML_Interop_CreateInputRecord(const char *apiId,
                                             uint32_t schema,
                                             BML_InteropRecordBuilder **outRecord);
BML_EXPORT void BML_Interop_DestroyRecordBuilder(BML_InteropRecordBuilder *record);

/* Query and command input is an API-defined record. A command does not
 * imply game mutation: the built-in API currently uses it only for
 * existing UI controls. */
BML_EXPORT int BML_Interop_InvokeQuery(const char *apiId,
                                       const char *endpoint,
                                       const BML_InteropRecordBuilder *input,
                                       BML_RecordRef *outRecord);
BML_EXPORT int BML_Interop_InvokeCommand(const char *apiId,
                                         const char *endpoint,
                                         const BML_InteropRecordBuilder *input,
                                         BML_RecordRef *outRecord);

/* An input view belongs to the synchronous provider callback. It is read-only
 * and must not be retained after that callback returns. */
BML_EXPORT int BML_Interop_RecordViewGetBool(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue);
BML_EXPORT int BML_Interop_RecordViewGetInt(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue);
BML_EXPORT int BML_Interop_RecordViewGetFloat(const BML_InteropRecordView *view, uint32_t fieldId, float *outValue);
BML_EXPORT int BML_Interop_RecordViewGetString(const BML_InteropRecordView *view,
                                                uint32_t fieldId,
                                                char *buffer,
                                                size_t bufferSize,
                                                size_t *outRequiredSize);
BML_EXPORT int BML_Interop_RecordViewGetObject(const BML_InteropRecordView *view,
                                                uint32_t fieldId,
                                                BML_ObjectRef *outValue);
BML_EXPORT int BML_Interop_RecordViewGetVec2(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec2 *outValue);
BML_EXPORT int BML_Interop_RecordViewGetVec3(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec3 *outValue);
BML_EXPORT int BML_Interop_RecordViewGetMat4(const BML_InteropRecordView *view, uint32_t fieldId, BML_Mat4 *outValue);
/* Same borrowed-value convention as RecordBorrowValue below.  A view belongs
 * to one synchronous query/command callback and must never be retained. */
BML_EXPORT int BML_Interop_RecordViewBorrowValue(const BML_InteropRecordView *view,
                                                 uint32_t fieldId,
                                                 BML_INTEROP_FIELD_TYPE expectedType,
                                                 const void **outData,
                                                 size_t *outCount,
                                                 size_t *outElementSize);
BML_EXPORT int BML_Interop_RecordViewGetStringArrayItem(const BML_InteropRecordView *view,
                                                         uint32_t fieldId,
                                                         size_t itemIndex,
                                                         char *buffer,
                                                         size_t bufferSize,
                                                         size_t *outRequiredSize);
BML_EXPORT int BML_Interop_RecordViewGetStringArrayCount(const BML_InteropRecordView *view,
                                                          uint32_t fieldId,
                                                          size_t *outCount);

BML_EXPORT int BML_Interop_ReadResource(const char *apiId, const char *endpoint, BML_RecordRef *outRecord);
BML_EXPORT int BML_Interop_ReadComponent(const char *apiId,
                                         const char *endpoint,
                                         BML_ObjectRef object,
                                         BML_RecordRef *outRecord);
BML_EXPORT int BML_Interop_OpenStream(const char *apiId,
                                      const char *endpoint,
                                      int capacity,
                                      BML_StreamRef *outStream);
BML_EXPORT int BML_Interop_PollStream(BML_StreamRef stream, BML_RecordRef *outRecord);
BML_EXPORT int BML_Interop_GetDroppedStreamCount(BML_StreamRef stream, int *outCount);
BML_EXPORT int BML_Interop_CloseStream(BML_StreamRef stream);
BML_EXPORT int BML_Interop_OpenCollection(const char *apiId, const char *endpoint, BML_CursorRef *outCursor);
BML_EXPORT int BML_Interop_ReadCollectionPage(BML_CursorRef cursor,
                                              BML_RecordRef *outRecords,
                                              size_t capacity,
                                              size_t *outCount,
                                              int *outComplete);
BML_EXPORT int BML_Interop_CloseCollection(BML_CursorRef cursor);
BML_EXPORT int BML_Interop_RetainRecord(BML_RecordRef record);
BML_EXPORT int BML_Interop_ReleaseRecord(BML_RecordRef record);
BML_EXPORT int BML_Interop_RecordSchema(BML_RecordRef record, uint32_t *outSchema);
BML_EXPORT int BML_Interop_RecordSequence(BML_RecordRef record, uint64_t *outSequence);
BML_EXPORT int BML_Interop_RecordTimestamp(BML_RecordRef record, uint64_t *outTimestamp);
BML_EXPORT int BML_Interop_RecordGetBool(BML_RecordRef record, uint32_t fieldId, int *outValue);
BML_EXPORT int BML_Interop_RecordGetInt(BML_RecordRef record, uint32_t fieldId, int *outValue);
BML_EXPORT int BML_Interop_RecordGetFloat(BML_RecordRef record, uint32_t fieldId, float *outValue);
BML_EXPORT int BML_Interop_RecordGetString(BML_RecordRef record,
                                            uint32_t fieldId,
                                            char *buffer,
                                            size_t bufferSize,
                                            size_t *outRequiredSize);
/* Borrows a non-string record value or array.  The returned pointer stays
 * valid through concurrent record release/owner invalidation and until the
 * next RecordBorrowValue call on the same thread. Copy it before borrowing
 * another field. STRING and STRING_ARRAY use the dedicated string accessors
 * because neither has a single fixed-width element layout. */
BML_EXPORT int BML_Interop_RecordBorrowValue(BML_RecordRef record,
                                             uint32_t fieldId,
                                             BML_INTEROP_FIELD_TYPE expectedType,
                                             const void **outData,
                                             size_t *outCount,
                                             size_t *outElementSize);
BML_EXPORT int BML_Interop_RecordGetStringArrayItem(BML_RecordRef record,
                                                     uint32_t fieldId,
                                                     size_t itemIndex,
                                                     char *buffer,
                                                     size_t bufferSize,
                                                     size_t *outRequiredSize);
BML_EXPORT int BML_Interop_RecordGetStringArrayCount(BML_RecordRef record,
                                                      uint32_t fieldId,
                                                      size_t *outCount);
BML_EXPORT int BML_Interop_RecordGetObject(BML_RecordRef record, uint32_t fieldId, BML_ObjectRef *outValue);
BML_EXPORT int BML_Interop_RecordGetVec2(BML_RecordRef record, uint32_t fieldId, BML_Vec2 *outValue);
BML_EXPORT int BML_Interop_RecordGetVec3(BML_RecordRef record, uint32_t fieldId, BML_Vec3 *outValue);
BML_EXPORT int BML_Interop_RecordGetMat4(BML_RecordRef record, uint32_t fieldId, BML_Mat4 *outValue);

BML_END_CDECLS

#endif // BML_INTEROP_API_H
