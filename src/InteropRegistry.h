#ifndef BML_INTEROPREGISTRY_H
#define BML_INTEROPREGISTRY_H

#include "BML/InteropApi.h"
#include "InteropContextInternal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace BML {

class InteropSessionService;

class InteropRegistry {
public:
    struct State;

    explicit InteropRegistry(InteropSessionService &sessions);
    ~InteropRegistry();

    InteropRegistry(const InteropRegistry &) = delete;
    InteropRegistry &operator=(const InteropRegistry &) = delete;

    int RegisterProvider(const char *ownerId,
                         const BML_InteropApiDescriptor *api,
                         const BML_InteropProviderCallbacks *callbacks,
                         void *userdata);
    int UnregisterProvider(const char *ownerId, const char *apiId);
    int RequireApi(const BML_InteropCallContext *context,
                         const char *apiId,
                         uint32_t expectedMajor,
                         uint64_t expectedHash);
    /* Lifecycle teardown must run on the game thread, just like provider
     * callbacks.  Returning a status lets module unload refuse an unsafe
     * foreign-thread teardown instead of freeing provider userdata in flight. */
    int InvalidateOwner(const char *ownerId);
    bool HasStreamConsumers(const char *apiId, const char *endpoint) const;

    int ReadResource(const BML_InteropCallContext *context,
                     const char *apiId,
                     const char *endpoint,
                     BML_RecordRef *outRecord);
    int ReadComponent(const BML_InteropCallContext *context,
                      const char *apiId,
                      const char *endpoint,
                      BML_ObjectRef object,
                      BML_RecordRef *outRecord);
    int OpenStream(const BML_InteropCallContext *context,
                   const char *apiId,
                   const char *endpoint,
                   int capacity,
                   BML_StreamRef *outStream);
    int PollStream(const BML_InteropCallContext *context, BML_StreamRef stream, BML_RecordRef *outRecord);
    int DroppedStreamCount(const BML_InteropCallContext *context, BML_StreamRef stream, int *outCount);
    int CloseStream(const BML_InteropCallContext *context, BML_StreamRef stream);

    int OpenCollection(const BML_InteropCallContext *context,
                       const char *apiId,
                       const char *endpoint,
                       BML_CursorRef *outCursor);
    int ReadCollectionPage(const BML_InteropCallContext *context,
                           BML_CursorRef cursor,
                           BML_RecordRef *outRecords,
                           size_t capacity,
                           size_t *outCount,
                           int *outComplete);
    int CloseCollection(const BML_InteropCallContext *context, BML_CursorRef cursor);

    int Publish(const char *ownerId, BML_InteropRecordBuilder *record);

    int CreateStreamRecord(const char *ownerId,
                           const char *apiId,
                           const char *endpoint,
                           BML_InteropRecordBuilder **outRecord);
    int CreateInputRecord(const BML_InteropCallContext *context,
                          const char *apiId,
                          uint32_t schema,
                          BML_InteropRecordBuilder **outRecord);
    int Invoke(const BML_InteropCallContext *context,
               const char *apiId,
               const char *endpoint,
               BML_INTEROP_ENDPOINT_KIND kind,
               const BML_InteropRecordBuilder *input,
               BML_RecordRef *outRecord);
    int DestroyRecordBuilder(BML_InteropRecordBuilder *record);
    int BuilderSetValue(BML_InteropRecordBuilder *record,
                        uint32_t fieldId,
                        BML_INTEROP_FIELD_TYPE type,
                        const void *data,
                        size_t count);
    int BuilderSetStringArray(BML_InteropRecordBuilder *record,
                              uint32_t fieldId,
                              const char *const *values,
                              const size_t *sizes,
                              size_t count);
    BML_InteropRecordBuilder *PageAppend(BML_InteropPageBuilder *page);
    void PageFinish(BML_InteropPageBuilder *page, int complete);

    int RecordViewGetBool(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue);
    int RecordViewGetInt(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue);
    int RecordViewGetFloat(const BML_InteropRecordView *view, uint32_t fieldId, float *outValue);
    int RecordViewGetString(const BML_InteropRecordView *view,
                            uint32_t fieldId,
                            char *buffer,
                            size_t bufferSize,
                            size_t *outRequiredSize);
    int RecordViewGetObject(const BML_InteropRecordView *view, uint32_t fieldId, BML_ObjectRef *outValue);
    int RecordViewGetVec2(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec2 *outValue);
    int RecordViewGetVec3(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec3 *outValue);
    int RecordViewGetMat4(const BML_InteropRecordView *view, uint32_t fieldId, BML_Mat4 *outValue);
    int RecordViewBorrowValue(const BML_InteropRecordView *view,
                              uint32_t fieldId,
                              BML_INTEROP_FIELD_TYPE expectedType,
                              const void **outData,
                              size_t *outCount,
                              size_t *outElementSize);
    int RecordViewGetStringArrayItem(const BML_InteropRecordView *view,
                                     uint32_t fieldId,
                                     size_t itemIndex,
                                     char *buffer,
                                     size_t bufferSize,
                                     size_t *outRequiredSize);
    int RecordViewGetStringArrayCount(const BML_InteropRecordView *view,
                                      uint32_t fieldId,
                                      size_t *outCount);

    int RetainRecord(const BML_InteropCallContext *context, BML_RecordRef record);
    int ReleaseRecord(const BML_InteropCallContext *context, BML_RecordRef record);
    int RecordSchema(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t *outSchema);
    int RecordSequence(const BML_InteropCallContext *context, BML_RecordRef record, uint64_t *outSequence);
    int RecordTimestamp(const BML_InteropCallContext *context, BML_RecordRef record, uint64_t *outTimestamp);
    int RecordGetBool(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, int *outValue);
    int RecordGetInt(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, int *outValue);
    int RecordGetFloat(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, float *outValue);
    int RecordGetString(const BML_InteropCallContext *context,
                        BML_RecordRef record,
                        uint32_t fieldId,
                        char *buffer,
                        size_t bufferSize,
                        size_t *outRequiredSize);
    int RecordBorrowValue(const BML_InteropCallContext *context,
                          BML_RecordRef record,
                          uint32_t fieldId,
                          BML_INTEROP_FIELD_TYPE expectedType,
                          const void **outData,
                          size_t *outCount,
                          size_t *outElementSize);
    int RecordGetStringArrayItem(const BML_InteropCallContext *context,
                                 BML_RecordRef record,
                                 uint32_t fieldId,
                                 size_t itemIndex,
                                 char *buffer,
                                 size_t bufferSize,
                                 size_t *outRequiredSize);
    int RecordGetStringArrayCount(const BML_InteropCallContext *context,
                                  BML_RecordRef record,
                                  uint32_t fieldId,
                                  size_t *outCount);
    int RecordGetObject(const BML_InteropCallContext *context,
                        BML_RecordRef record,
                        uint32_t fieldId,
                        BML_ObjectRef *outValue);
    int RecordGetVec2(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, BML_Vec2 *outValue);
    int RecordGetVec3(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, BML_Vec3 *outValue);
    int RecordGetMat4(const BML_InteropCallContext *context, BML_RecordRef record, uint32_t fieldId, BML_Mat4 *outValue);

private:
    std::unique_ptr<State> m_State;
};

} // namespace BML

#endif // BML_INTEROPREGISTRY_H
