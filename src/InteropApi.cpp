#include "BML/InteropApi.h"

#include <intrin.h>
#include <new>
#include <string>

#include "InteropContextInternal.h"
#include "InteropRegistry.h"
#include "InteropSessionService.h"
#include "ModContext.h"

namespace {

template <typename Function>
int GuardInteropMutation(Function function) {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

struct NativeInteropCaller {
    ModContext *Context = nullptr;
    BML::ModInvocationGate::CallLock InvocationLock;
    std::string OwnerId;
    BML_InteropCallContext CallContext{};
};

/* Resolve ownership at the public C entry point.  The return address then
 * belongs to the native mod rather than to an inline C++ convenience wrapper. */
NativeInteropCaller ResolveNativeInteropCaller(const void *returnAddress) {
    NativeInteropCaller caller;
    caller.Context = BML_GetModContext();
    if (!caller.Context)
        return caller;
    caller.InvocationLock = caller.Context->LockModInvocation();
    caller.OwnerId = caller.Context->GetNativeInteropOwnerId(returnAddress);
    caller.CallContext = caller.Context->GetInteropSessions().CreateContextForOwner(caller.OwnerId);
    return caller;
}

int RequireNativeInteropProvider(const NativeInteropCaller &caller) {
    if (!caller.Context || caller.OwnerId.empty() || caller.CallContext.SessionId == 0)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return BML_OK;
}

} // namespace

BML_BEGIN_CDECLS

BML_EXPORT int BML_Interop_RegisterProvider(const BML_InteropApiDescriptor *api,
                                            const BML_InteropProviderCallbacks *callbacks,
                                            void *userdata) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    const int ownerStatus = RequireNativeInteropProvider(caller);
    if (ownerStatus != BML_OK)
        return ownerStatus;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RegisterProvider(caller.OwnerId.c_str(), api, callbacks, userdata);
    });
}

BML_EXPORT int BML_Interop_UnregisterProvider(const char *apiId) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    const int ownerStatus = RequireNativeInteropProvider(caller);
    if (ownerStatus != BML_OK)
        return ownerStatus;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().UnregisterProvider(caller.OwnerId.c_str(), apiId);
    });
}

BML_EXPORT int BML_Interop_RequireApi(const char *apiId,
                                            uint32_t expectedMajor,
                                            uint64_t expectedHash) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RequireApi(&caller.CallContext,
                                                                        apiId,
                                                                        expectedMajor,
                                                                        expectedHash);
    });
}

BML_EXPORT int BML_Interop_RecordBuilder_SetValue(BML_InteropRecordBuilder *record,
                                                  uint32_t fieldId,
                                                  BML_INTEROP_FIELD_TYPE type,
                                                  const void *data,
                                                  size_t count) {
    ModContext *context = BML_GetModContext();
    if (!context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return context->GetInteropRegistry().BuilderSetValue(record, fieldId, type, data, count);
    });
}

BML_EXPORT int BML_Interop_RecordBuilder_SetStringArray(BML_InteropRecordBuilder *record,
                                                        uint32_t fieldId,
                                                        const char *const *values,
                                                        const size_t *sizes,
                                                        size_t count) {
    ModContext *context = BML_GetModContext();
    if (!context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return context->GetInteropRegistry().BuilderSetStringArray(record, fieldId, values, sizes, count);
    });
}

BML_EXPORT BML_InteropRecordBuilder *BML_Interop_PageBuilder_Append(BML_InteropPageBuilder *page) {
    try {
        ModContext *context = BML_GetModContext();
        return context ? context->GetInteropRegistry().PageAppend(page) : nullptr;
    } catch (...) {
        return nullptr;
    }
}

BML_EXPORT void BML_Interop_PageBuilder_Finish(BML_InteropPageBuilder *page, int complete) {
    try {
        if (ModContext *context = BML_GetModContext())
            context->GetInteropRegistry().PageFinish(page, complete);
    } catch (...) {
    }
}

BML_EXPORT int BML_Interop_CreateStreamRecord(const char *apiId,
                                              const char *endpoint,
                                              BML_InteropRecordBuilder **outRecord) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    const int ownerStatus = RequireNativeInteropProvider(caller);
    if (ownerStatus != BML_OK)
        return ownerStatus;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().CreateStreamRecord(caller.OwnerId.c_str(), apiId, endpoint, outRecord);
    });
}

BML_EXPORT int BML_Interop_PublishStreamRecord(BML_InteropRecordBuilder *record) {
    if (!record)
        return BML_ERROR_INVALID_PARAMETER;
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    const int ownerStatus = RequireNativeInteropProvider(caller);
    if (ownerStatus != BML_OK)
        return ownerStatus;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().Publish(caller.OwnerId.c_str(), record);
    });
}

BML_EXPORT int BML_Interop_CreateInputRecord(const char *apiId,
                                             uint32_t schema,
                                             BML_InteropRecordBuilder **outRecord) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().CreateInputRecord(&caller.CallContext, apiId, schema, outRecord);
    });
}

BML_EXPORT void BML_Interop_DestroyRecordBuilder(BML_InteropRecordBuilder *record) {
    try {
        if (ModContext *context = BML_GetModContext())
            (void)context->GetInteropRegistry().DestroyRecordBuilder(record);
    } catch (...) {
    }
}

BML_EXPORT int BML_Interop_InvokeQuery(const char *apiId,
                                       const char *endpoint,
                                       const BML_InteropRecordBuilder *input,
                                       BML_RecordRef *outRecord) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().Invoke(&caller.CallContext, apiId, endpoint,
                                                            BML_INTEROP_ENDPOINT_QUERY, input, outRecord);
    });
}

BML_EXPORT int BML_Interop_InvokeCommand(const char *apiId,
                                         const char *endpoint,
                                         const BML_InteropRecordBuilder *input,
                                         BML_RecordRef *outRecord) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().Invoke(&caller.CallContext, apiId, endpoint,
                                                            BML_INTEROP_ENDPOINT_COMMAND, input, outRecord);
    });
}

BML_EXPORT int BML_Interop_RecordViewGetBool(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetBool(view, fieldId, outValue)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetInt(const BML_InteropRecordView *view, uint32_t fieldId, int *outValue) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetInt(view, fieldId, outValue)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetFloat(const BML_InteropRecordView *view, uint32_t fieldId, float *outValue) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetFloat(view, fieldId, outValue)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetString(const BML_InteropRecordView *view,
                                                uint32_t fieldId,
                                                char *buffer,
                                                size_t bufferSize,
                                                size_t *outRequiredSize) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetString(view, fieldId, buffer, bufferSize, outRequiredSize)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetObject(const BML_InteropRecordView *view,
                                                uint32_t fieldId,
                                                BML_ObjectRef *outValue) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetObject(view, fieldId, outValue)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetVec2(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec2 *outValue) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetVec2(view, fieldId, outValue)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetVec3(const BML_InteropRecordView *view, uint32_t fieldId, BML_Vec3 *outValue) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetVec3(view, fieldId, outValue)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetMat4(const BML_InteropRecordView *view, uint32_t fieldId, BML_Mat4 *outValue) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetMat4(view, fieldId, outValue)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewBorrowValue(const BML_InteropRecordView *view,
                                                 uint32_t fieldId,
                                                 BML_INTEROP_FIELD_TYPE expectedType,
                                                 const void **outData,
                                                 size_t *outCount,
                                                 size_t *outElementSize) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewBorrowValue(view,
                                                                            fieldId,
                                                                            expectedType,
                                                                            outData,
                                                                            outCount,
                                                                            outElementSize)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetStringArrayItem(const BML_InteropRecordView *view,
                                                        uint32_t fieldId,
                                                        size_t itemIndex,
                                                        char *buffer,
                                                        size_t bufferSize,
                                                        size_t *outRequiredSize) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetStringArrayItem(view,
                                                                                   fieldId,
                                                                                   itemIndex,
                                                                                   buffer,
                                                                                   bufferSize,
                                                                                   outRequiredSize)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_RecordViewGetStringArrayCount(const BML_InteropRecordView *view,
                                                         uint32_t fieldId,
                                                         size_t *outCount) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetInteropRegistry().RecordViewGetStringArrayCount(view, fieldId, outCount)
                   : BML_ERROR_INTEROP_UNSUPPORTED;
}

BML_EXPORT int BML_Interop_ReadResource(const char *apiId, const char *endpoint, BML_RecordRef *outRecord) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().ReadResource(&caller.CallContext, apiId, endpoint, outRecord);
    });
}

BML_EXPORT int BML_Interop_ReadComponent(const char *apiId,
                                         const char *endpoint,
                                         BML_ObjectRef object,
                                         BML_RecordRef *outRecord) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().ReadComponent(&caller.CallContext, apiId, endpoint, object, outRecord);
    });
}

BML_EXPORT int BML_Interop_OpenStream(const char *apiId,
                                      const char *endpoint,
                                      int capacity,
                                      BML_StreamRef *outStream) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().OpenStream(&caller.CallContext, apiId, endpoint, capacity, outStream);
    });
}

BML_EXPORT int BML_Interop_PollStream(BML_StreamRef stream, BML_RecordRef *outRecord) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().PollStream(&caller.CallContext, stream, outRecord);
    });
}

BML_EXPORT int BML_Interop_GetDroppedStreamCount(BML_StreamRef stream, int *outCount) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().DroppedStreamCount(&caller.CallContext, stream, outCount);
    });
}

BML_EXPORT int BML_Interop_CloseStream(BML_StreamRef stream) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().CloseStream(&caller.CallContext, stream);
    });
}

BML_EXPORT int BML_Interop_OpenCollection(const char *apiId, const char *endpoint, BML_CursorRef *outCursor) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().OpenCollection(&caller.CallContext, apiId, endpoint, outCursor);
    });
}

BML_EXPORT int BML_Interop_ReadCollectionPage(BML_CursorRef cursor,
                                              BML_RecordRef *outRecords,
                                              size_t capacity,
                                              size_t *outCount,
                                              int *outComplete) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().ReadCollectionPage(&caller.CallContext, cursor,
                                                                         outRecords, capacity, outCount, outComplete);
    });
}

BML_EXPORT int BML_Interop_CloseCollection(BML_CursorRef cursor) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().CloseCollection(&caller.CallContext, cursor);
    });
}

BML_EXPORT int BML_Interop_RetainRecord(BML_RecordRef record) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RetainRecord(&caller.CallContext, record);
    });
}

BML_EXPORT int BML_Interop_ReleaseRecord(BML_RecordRef record) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().ReleaseRecord(&caller.CallContext, record);
    });
}

BML_EXPORT int BML_Interop_RecordSchema(BML_RecordRef record, uint32_t *outSchema) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordSchema(&caller.CallContext, record, outSchema);
    });
}

BML_EXPORT int BML_Interop_RecordSequence(BML_RecordRef record, uint64_t *outSequence) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordSequence(&caller.CallContext, record, outSequence);
    });
}

BML_EXPORT int BML_Interop_RecordTimestamp(BML_RecordRef record, uint64_t *outTimestamp) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordTimestamp(&caller.CallContext, record, outTimestamp);
    });
}

BML_EXPORT int BML_Interop_RecordGetBool(BML_RecordRef record, uint32_t fieldId, int *outValue) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetBool(&caller.CallContext, record, fieldId, outValue);
    });
}

BML_EXPORT int BML_Interop_RecordGetInt(BML_RecordRef record, uint32_t fieldId, int *outValue) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetInt(&caller.CallContext, record, fieldId, outValue);
    });
}

BML_EXPORT int BML_Interop_RecordGetFloat(BML_RecordRef record, uint32_t fieldId, float *outValue) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetFloat(&caller.CallContext, record, fieldId, outValue);
    });
}

BML_EXPORT int BML_Interop_RecordGetString(BML_RecordRef record,
                                            uint32_t fieldId,
                                            char *buffer,
                                            size_t bufferSize,
                                            size_t *outRequiredSize) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetString(&caller.CallContext, record, fieldId,
                                                                      buffer, bufferSize, outRequiredSize);
    });
}

BML_EXPORT int BML_Interop_RecordBorrowValue(BML_RecordRef record,
                                             uint32_t fieldId,
                                             BML_INTEROP_FIELD_TYPE expectedType,
                                             const void **outData,
                                             size_t *outCount,
                                             size_t *outElementSize) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordBorrowValue(&caller.CallContext, record, fieldId, expectedType,
                                                                        outData, outCount, outElementSize);
    });
}

BML_EXPORT int BML_Interop_RecordGetStringArrayItem(BML_RecordRef record,
                                                     uint32_t fieldId,
                                                     size_t itemIndex,
                                                     char *buffer,
                                                     size_t bufferSize,
                                                     size_t *outRequiredSize) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetStringArrayItem(&caller.CallContext, record, fieldId,
                                                                                itemIndex, buffer, bufferSize, outRequiredSize);
    });
}

BML_EXPORT int BML_Interop_RecordGetStringArrayCount(BML_RecordRef record,
                                                      uint32_t fieldId,
                                                      size_t *outCount) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetStringArrayCount(&caller.CallContext, record, fieldId, outCount);
    });
}

BML_EXPORT int BML_Interop_RecordGetObject(BML_RecordRef record, uint32_t fieldId, BML_ObjectRef *outValue) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetObject(&caller.CallContext, record, fieldId, outValue);
    });
}

BML_EXPORT int BML_Interop_RecordGetVec2(BML_RecordRef record, uint32_t fieldId, BML_Vec2 *outValue) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetVec2(&caller.CallContext, record, fieldId, outValue);
    });
}

BML_EXPORT int BML_Interop_RecordGetVec3(BML_RecordRef record, uint32_t fieldId, BML_Vec3 *outValue) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetVec3(&caller.CallContext, record, fieldId, outValue);
    });
}

BML_EXPORT int BML_Interop_RecordGetMat4(BML_RecordRef record, uint32_t fieldId, BML_Mat4 *outValue) {
    const NativeInteropCaller caller = ResolveNativeInteropCaller(_ReturnAddress());
    if (!caller.Context)
        return BML_ERROR_INTEROP_UNSUPPORTED;
    return GuardInteropMutation([&]() {
        return caller.Context->GetInteropRegistry().RecordGetMat4(&caller.CallContext, record, fieldId, outValue);
    });
}

BML_END_CDECLS
