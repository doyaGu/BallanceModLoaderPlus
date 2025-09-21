#ifndef BML_DATASHARE_H
#define BML_DATASHARE_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

typedef struct BML_DataShare BML_DataShare;

typedef void (*BML_DataShareCallback)(const char *key, const void *data, size_t size, void *userdata);
typedef void (*BML_DataShareCleanupCallback)(const char *key, void *userdata);

BML_EXPORT BML_DataShare *BML_GetDataShare(const char *name);
BML_EXPORT uint32_t BML_DataShare_AddRef(BML_DataShare *handle);
BML_EXPORT uint32_t BML_DataShare_Release(BML_DataShare *handle);

BML_EXPORT int BML_DataShare_Set(BML_DataShare *handle, const char *key, const void *data, size_t size);
BML_EXPORT void BML_DataShare_Remove(BML_DataShare *handle, const char *key);

// NOTE: The pointer returned by Get() is BORROWED and only valid until the next
// Set()/Remove() for the same key or until the instance is destroyed.
BML_EXPORT const void *BML_DataShare_Get(const BML_DataShare *handle, const char *key, size_t *outSize);

// Copies if possible. Returns 1 on success, 0 if not present or invalid key.
BML_EXPORT int BML_DataShare_Copy(const BML_DataShare *handle, const char *key, void *dst, size_t dstSize);

// Single-call copy that reports required size: returns 1 on success, 0 if not present,
// or -N (negative required size) if dst is too small. When non-null, outFullSize is
// always set to the full size of the value (0 if key not present).
BML_EXPORT int BML_DataShare_CopyEx(const BML_DataShare *handle, const char *key, void *dst, size_t dstSize, size_t *outFullSize);

BML_EXPORT int BML_DataShare_Has(const BML_DataShare *handle, const char *key);
BML_EXPORT size_t BML_DataShare_SizeOf(const BML_DataShare *handle, const char *key);

// Enqueue one-shot waiter; (callback, userdata, cleanup) order is intentional.
// If the key already exists, callback fires immediately; otherwise it fires once when set.
// The callback is invoked out-of-lock. Cleanup is always invoked exactly once.
BML_EXPORT void BML_DataShare_Request(BML_DataShare *handle, const char *key,
                                      BML_DataShareCallback callback, void *userdata,
                                      BML_DataShareCleanupCallback cleanup);

// WARNING: Force-destroys all instances regardless of outstanding references.
// All BML_DataShare* become invalid after this call.
BML_EXPORT void BML_DataShare_DestroyAll(void);

BML_END_CDECLS

#endif // BML_DATASHARE_H
