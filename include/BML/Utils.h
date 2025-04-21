#ifndef BML_UTILS_H
#define BML_UTILS_H

#include "BML/Defines.h"
#include "BML/Version.h"
#include "BML/Errors.h"

BML_BEGIN_CDECLS

BML_EXPORT void BML_GetVersion(int *major, int *minor, int *patch);
BML_EXPORT const char *BML_GetVersionString();

BML_EXPORT void *BML_Malloc(size_t size);
BML_EXPORT void *BML_Calloc(size_t count, size_t size);
BML_EXPORT void *BML_Realloc(void *ptr, size_t size);
BML_EXPORT void BML_Free(void *ptr);

BML_EXPORT char *BML_Strdup(const char *str);

BML_END_CDECLS

#endif // BML_UTILS_H
