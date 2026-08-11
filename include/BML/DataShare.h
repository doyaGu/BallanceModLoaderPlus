// A byte store shared by every Mod, keyed by name, and the way two Mods exchange
// data without either one linking against the other. It is plain C, so what one Mod
// writes another can read whatever language or standard library either was built
// with, as long as both agree on what the bytes mean: nothing here describes a
// value's layout.
//
// BML_GetDataShare(nullptr) is the instance the loader itself uses, where it
// publishes CustomMapName while a custom map loads. Any other name gives a separate
// store, created on the first call for that name, which is how a group of Mods keeps
// its keys out of everyone else's way. An instance is never destroyed by releasing
// the last handle: the registry holds one reference of its own, so a value outlives
// the Mod that wrote it and stays readable for the rest of the process. Remove what
// should not be seen again rather than expecting it to go away with the handle.
//
// Every handle from BML_GetDataShare carries a reference of its own and has to reach
// BML_DataShare_Release, and BML_DataShare_AddRef adds another for a copy of the
// handle. Both return the new count, and 0 for a null handle.
//
// Keys are 1 to 255 bytes and compared byte for byte, case included. A key outside
// that range is refused by every function here, which reports it the same way as a
// key that is simply absent.
//
// Values are copied in and out and may be any size, zero included. There is no
// notification when a value changes, only BML_DataShare_Request for the first time
// one appears, so a Mod that has to follow a value polls it.
//
// Unlike the rest of the SDK this is locked internally and may be used from any
// thread. Callbacks run outside the lock, on whichever thread called
// BML_DataShare_Set, so a Mod that only touches the game from the game thread should
// hand the work to IBML::AddTimer instead of doing it in the callback. An exception
// escaping a callback is swallowed.
#ifndef BML_DATASHARE_H
#define BML_DATASHARE_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

typedef struct BML_DataShare BML_DataShare;

typedef void (*BML_DataShareCallback)(const char *key, const void *data, size_t size, void *userdata);
typedef void (*BML_DataShareCleanupCallback)(const char *key, void *userdata);

// The instance called name, created on the first call for that name, with a
// reference already added for the returned handle. A null or empty name means the
// loader's own instance. Null is returned only when the instance could not be
// created at all.
BML_EXPORT BML_DataShare *BML_GetDataShare(const char *name);
BML_EXPORT uint32_t BML_DataShare_AddRef(BML_DataShare *handle);
BML_EXPORT uint32_t BML_DataShare_Release(BML_DataShare *handle);

// Set copies size bytes out of data and replaces whatever the key held, then fires
// the waiters queued by BML_DataShare_Request for it. It returns 0 for an invalid
// key and for a null data with a non-zero size; a null data with size 0 stores an
// empty value, which reads back the same as an absent one through Get and SizeOf,
// so use BML_DataShare_Has to tell them apart.
//
// Remove drops the key. Waiters still queued for it are fired with a null data and
// a size of 0 rather than left waiting, so a Request callback has to handle that.
// Nothing is reported, and removing a key that was never there is not an error.
BML_EXPORT int BML_DataShare_Set(BML_DataShare *handle, const char *key, const void *data, size_t size);
BML_EXPORT void BML_DataShare_Remove(BML_DataShare *handle, const char *key);

// NOTE: The pointer returned by Get() is BORROWED and only valid until the next
// Set()/Remove() for the same key or until the instance is destroyed. Since another
// thread may be the one doing that Set, prefer BML_DataShare_CopyEx unless the Mod
// knows nobody else writes the key. outSize is written on every path, 0 included,
// and may be null.
BML_EXPORT const void *BML_DataShare_Get(const BML_DataShare *handle, const char *key, size_t *outSize);

// Copies if possible. Returns 1 on success, 0 if not present or invalid key.
// A dst too small for the value is a plain 0 as well, which is what CopyEx tells
// apart.
BML_EXPORT int BML_DataShare_Copy(const BML_DataShare *handle, const char *key, void *dst, size_t dstSize);

// Single-call copy that reports required size: returns 1 on success, 0 if not present,
// or -N (negative required size) if dst is too small. When non-null, outFullSize is
// always set to the full size of the value (0 if key not present). A value larger
// than INT_MAX gives INT_MIN instead of a usable -N, so read the size from
// outFullSize rather than from the return value.
BML_EXPORT int BML_DataShare_CopyEx(const BML_DataShare *handle, const char *key, void *dst, size_t dstSize, size_t *outFullSize);

// Whether the key is there, and how many bytes it holds. SizeOf answers 0 for an
// absent key, an invalid one, and a value that is genuinely empty alike. Neither
// says anything about the next call: another thread can remove the key in between,
// so use the return of a Copy rather than asking first and copying after.
BML_EXPORT int BML_DataShare_Has(const BML_DataShare *handle, const char *key);
BML_EXPORT size_t BML_DataShare_SizeOf(const BML_DataShare *handle, const char *key);

// Enqueue one-shot waiter; (callback, userdata, cleanup) order is intentional.
// If the key already exists, callback fires immediately; otherwise it fires once when set.
// The callback is invoked out-of-lock. Cleanup is always invoked exactly once.
// So this is how one Mod waits for another that may not have loaded yet. The
// callback fires on the calling thread when the key is already there and on the
// thread that called Set when it is not, and the data pointer it gets is only good
// for the length of the call. It fires once and is then forgotten, including when
// BML_DataShare_Remove wakes it with a null data. Do not queue a new waiter from
// inside the callback: the key is present by then, so the new one fires immediately
// and recurses. Poll the value instead to follow one that changes.
//
// Cleanup is where the userdata is released. It runs right after the callback, and
// it runs on its own when the handle, the key, or the callback is invalid, so a Mod
// that allocated the userdata never leaks it.
BML_EXPORT void BML_DataShare_Request(BML_DataShare *handle, const char *key,
                                      BML_DataShareCallback callback, void *userdata,
                                      BML_DataShareCleanupCallback cleanup);

// WARNING: Force-destroys all instances regardless of outstanding references.
// All BML_DataShare* become invalid after this call, including the ones other Mods
// and the loader are holding, and every waiter still queued has its cleanup run.
// This is here for whoever owns the process, not for a Mod: a Mod that wants its own
// keys gone calls BML_DataShare_Remove.
BML_EXPORT void BML_DataShare_DestroyAll(void);

BML_END_CDECLS

#endif // BML_DATASHARE_H
