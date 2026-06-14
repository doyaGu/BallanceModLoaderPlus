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
