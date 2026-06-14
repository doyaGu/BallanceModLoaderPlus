#ifndef BML_INTEROP_H
#define BML_INTEROP_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

typedef struct BML_ModExport BML_ModExport;
typedef struct BML_CallFrame BML_CallFrame;

typedef enum BML_MOD_KIND {
    BML_MOD_KIND_UNKNOWN = 0,
    BML_MOD_KIND_NATIVE = 1,
    BML_MOD_KIND_SCRIPT = 2,
} BML_MOD_KIND;

typedef enum BML_MOD_STATE {
    BML_MOD_STATE_NOT_FOUND = 0,
    BML_MOD_STATE_REGISTERED = 1,
    BML_MOD_STATE_LOADED = 2,
    BML_MOD_STATE_FAILED = 3,
} BML_MOD_STATE;

typedef enum BML_CALL_VALUE_TYPE {
    BML_CALL_VALUE_EMPTY = 0,
    BML_CALL_VALUE_BOOL = 1,
    BML_CALL_VALUE_INT = 2,
    BML_CALL_VALUE_FLOAT = 3,
    BML_CALL_VALUE_STRING = 4,
    BML_CALL_VALUE_BOOL_ARRAY = 16,
    BML_CALL_VALUE_INT_ARRAY = 17,
    BML_CALL_VALUE_FLOAT_ARRAY = 18,
    BML_CALL_VALUE_STRING_ARRAY = 19,
    BML_CALL_VALUE_BUFFER = 20,
    BML_CALL_VALUE_OBJECT_ID = 21,
} BML_CALL_VALUE_TYPE;

typedef int (*BML_NativeExportCallback)(BML_CallFrame *frame, void *userdata);

BML_EXPORT BML_MOD_KIND BML_GetModKind(const char *id);
BML_EXPORT BML_MOD_STATE BML_GetModState(const char *id);
BML_EXPORT int BML_GetModDiagnostic(const char *id,
                                    char *buffer,
                                    size_t bufferSize,
                                    size_t *outRequiredSize);

BML_EXPORT BML_CallFrame *BML_CreateCallFrame(void);
BML_EXPORT void BML_DestroyCallFrame(BML_CallFrame *frame);
BML_EXPORT void BML_CallFrame_Clear(BML_CallFrame *frame);
BML_EXPORT size_t BML_CallFrame_GetArgCount(const BML_CallFrame *frame);
BML_EXPORT BML_CALL_VALUE_TYPE BML_CallFrame_GetArgType(const BML_CallFrame *frame, size_t index);
BML_EXPORT int BML_CallFrame_ClearArg(BML_CallFrame *frame, size_t index);
BML_EXPORT int BML_CallFrame_SetBool(BML_CallFrame *frame, size_t index, int value);
BML_EXPORT int BML_CallFrame_GetBool(const BML_CallFrame *frame, size_t index, int *outValue);
BML_EXPORT int BML_CallFrame_SetInt(BML_CallFrame *frame, size_t index, int value);
BML_EXPORT int BML_CallFrame_GetInt(const BML_CallFrame *frame, size_t index, int *outValue);
BML_EXPORT int BML_CallFrame_SetFloat(BML_CallFrame *frame, size_t index, float value);
BML_EXPORT int BML_CallFrame_GetFloat(const BML_CallFrame *frame, size_t index, float *outValue);
BML_EXPORT int BML_CallFrame_SetString(BML_CallFrame *frame, size_t index, const char *value);
BML_EXPORT int BML_CallFrame_GetString(const BML_CallFrame *frame,
                                       size_t index,
                                       char *buffer,
                                       size_t bufferSize,
                                       size_t *outRequiredSize);
BML_EXPORT int BML_CallFrame_SetBoolArray(BML_CallFrame *frame, size_t index, const int *values, size_t count);
BML_EXPORT size_t BML_CallFrame_GetBoolArrayCount(const BML_CallFrame *frame, size_t index);
BML_EXPORT int BML_CallFrame_CopyBoolArray(const BML_CallFrame *frame,
                                           size_t index,
                                           int *buffer,
                                           size_t bufferCount,
                                           size_t *outRequiredCount);
BML_EXPORT int BML_CallFrame_SetIntArray(BML_CallFrame *frame, size_t index, const int *values, size_t count);
BML_EXPORT size_t BML_CallFrame_GetIntArrayCount(const BML_CallFrame *frame, size_t index);
BML_EXPORT int BML_CallFrame_CopyIntArray(const BML_CallFrame *frame,
                                          size_t index,
                                          int *buffer,
                                          size_t bufferCount,
                                          size_t *outRequiredCount);
BML_EXPORT int BML_CallFrame_SetFloatArray(BML_CallFrame *frame, size_t index, const float *values, size_t count);
BML_EXPORT size_t BML_CallFrame_GetFloatArrayCount(const BML_CallFrame *frame, size_t index);
BML_EXPORT int BML_CallFrame_CopyFloatArray(const BML_CallFrame *frame,
                                            size_t index,
                                            float *buffer,
                                            size_t bufferCount,
                                            size_t *outRequiredCount);
BML_EXPORT int BML_CallFrame_SetStringArray(BML_CallFrame *frame,
                                            size_t index,
                                            const char *const *values,
                                            size_t count);
BML_EXPORT size_t BML_CallFrame_GetStringArrayCount(const BML_CallFrame *frame, size_t index);
BML_EXPORT int BML_CallFrame_GetStringArrayItem(const BML_CallFrame *frame,
                                                size_t index,
                                                size_t itemIndex,
                                                char *buffer,
                                                size_t bufferSize,
                                                size_t *outRequiredSize);
BML_EXPORT int BML_CallFrame_SetBuffer(BML_CallFrame *frame, size_t index, const uint8_t *data, size_t size);
BML_EXPORT size_t BML_CallFrame_GetBufferSize(const BML_CallFrame *frame, size_t index);
BML_EXPORT int BML_CallFrame_CopyBuffer(const BML_CallFrame *frame,
                                        size_t index,
                                        uint8_t *buffer,
                                        size_t bufferSize,
                                        size_t *outRequiredSize);
BML_EXPORT int BML_CallFrame_SetObjectId(BML_CallFrame *frame, size_t index, int objectId);
BML_EXPORT int BML_CallFrame_GetObjectId(const BML_CallFrame *frame, size_t index, int *outObjectId);
BML_EXPORT int BML_CallFrame_SetResultBool(BML_CallFrame *frame, int value);
BML_EXPORT int BML_CallFrame_GetResultBool(const BML_CallFrame *frame, int *outValue);
BML_EXPORT BML_CALL_VALUE_TYPE BML_CallFrame_GetResultType(const BML_CallFrame *frame);
BML_EXPORT int BML_CallFrame_ClearResult(BML_CallFrame *frame);
BML_EXPORT int BML_CallFrame_SetResultInt(BML_CallFrame *frame, int value);
BML_EXPORT int BML_CallFrame_GetResultInt(const BML_CallFrame *frame, int *outValue);
BML_EXPORT int BML_CallFrame_SetResultFloat(BML_CallFrame *frame, float value);
BML_EXPORT int BML_CallFrame_GetResultFloat(const BML_CallFrame *frame, float *outValue);
BML_EXPORT int BML_CallFrame_SetResultString(BML_CallFrame *frame, const char *value);
BML_EXPORT int BML_CallFrame_GetResultString(const BML_CallFrame *frame,
                                             char *buffer,
                                             size_t bufferSize,
                                             size_t *outRequiredSize);
BML_EXPORT int BML_CallFrame_SetResultBoolArray(BML_CallFrame *frame, const int *values, size_t count);
BML_EXPORT size_t BML_CallFrame_GetResultBoolArrayCount(const BML_CallFrame *frame);
BML_EXPORT int BML_CallFrame_CopyResultBoolArray(const BML_CallFrame *frame,
                                                 int *buffer,
                                                 size_t bufferCount,
                                                 size_t *outRequiredCount);
BML_EXPORT int BML_CallFrame_SetResultIntArray(BML_CallFrame *frame, const int *values, size_t count);
BML_EXPORT size_t BML_CallFrame_GetResultIntArrayCount(const BML_CallFrame *frame);
BML_EXPORT int BML_CallFrame_CopyResultIntArray(const BML_CallFrame *frame,
                                                int *buffer,
                                                size_t bufferCount,
                                                size_t *outRequiredCount);
BML_EXPORT int BML_CallFrame_SetResultFloatArray(BML_CallFrame *frame, const float *values, size_t count);
BML_EXPORT size_t BML_CallFrame_GetResultFloatArrayCount(const BML_CallFrame *frame);
BML_EXPORT int BML_CallFrame_CopyResultFloatArray(const BML_CallFrame *frame,
                                                  float *buffer,
                                                  size_t bufferCount,
                                                  size_t *outRequiredCount);
BML_EXPORT int BML_CallFrame_SetResultStringArray(BML_CallFrame *frame,
                                                  const char *const *values,
                                                  size_t count);
BML_EXPORT size_t BML_CallFrame_GetResultStringArrayCount(const BML_CallFrame *frame);
BML_EXPORT int BML_CallFrame_GetResultStringArrayItem(const BML_CallFrame *frame,
                                                      size_t itemIndex,
                                                      char *buffer,
                                                      size_t bufferSize,
                                                      size_t *outRequiredSize);
BML_EXPORT int BML_CallFrame_SetResultBuffer(BML_CallFrame *frame, const uint8_t *data, size_t size);
BML_EXPORT size_t BML_CallFrame_GetResultBufferSize(const BML_CallFrame *frame);
BML_EXPORT int BML_CallFrame_CopyResultBuffer(const BML_CallFrame *frame,
                                              uint8_t *buffer,
                                              size_t bufferSize,
                                              size_t *outRequiredSize);
BML_EXPORT int BML_CallFrame_SetResultObjectId(BML_CallFrame *frame, int objectId);
BML_EXPORT int BML_CallFrame_GetResultObjectId(const BML_CallFrame *frame, int *outObjectId);

BML_EXPORT int BML_RegisterNativeModExport(const char *modId,
                                           const char *name,
                                           const char *signature,
                                           BML_NativeExportCallback callback,
                                           void *userdata);
BML_EXPORT int BML_UnregisterNativeModExport(const char *modId,
                                             const char *name,
                                             const char *signature);
BML_EXPORT int BML_UnregisterNativeModExports(const char *modId);
BML_EXPORT int BML_GetModExportCount(const char *modId);
BML_EXPORT int BML_GetModExportInfo(const char *modId,
                                    size_t index,
                                    char *nameBuffer,
                                    size_t nameBufferSize,
                                    size_t *outNameRequiredSize,
                                    char *signatureBuffer,
                                    size_t signatureBufferSize,
                                    size_t *outSignatureRequiredSize);
BML_EXPORT BML_ModExport *BML_FindModExport(const char *modId,
                                            const char *name,
                                            const char *signature);
BML_EXPORT int BML_FindModExportEx(const char *modId,
                                   const char *name,
                                   const char *signature,
                                   BML_ModExport **outHandle);
BML_EXPORT int BML_IsModExportValid(BML_ModExport *handle);
BML_EXPORT int BML_GetModExportModId(const BML_ModExport *handle,
                                     char *buffer,
                                     size_t bufferSize,
                                     size_t *outRequiredSize);
BML_EXPORT int BML_GetModExportName(const BML_ModExport *handle,
                                    char *buffer,
                                    size_t bufferSize,
                                    size_t *outRequiredSize);
BML_EXPORT int BML_GetModExportSignature(const BML_ModExport *handle,
                                         char *buffer,
                                         size_t bufferSize,
                                         size_t *outRequiredSize);
BML_EXPORT uint32_t BML_AddRefModExport(BML_ModExport *handle);
BML_EXPORT uint32_t BML_ReleaseModExport(BML_ModExport *handle);
BML_EXPORT int BML_CallModExport(BML_ModExport *handle, BML_CallFrame *frame);

BML_END_CDECLS

#endif // BML_INTEROP_H
