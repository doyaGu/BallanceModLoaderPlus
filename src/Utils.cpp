#include "BML/Utils.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

void BML_GetVersion(int *major, int *minor, int *patch) {
    if (major) *major = BML_MAJOR_VERSION;
    if (minor) *minor = BML_MINOR_VERSION;
    if (patch) *patch = BML_PATCH_VERSION;
}

void BML_GetVersionString(char *version, size_t size) {
    if (version && size > 0) {
        snprintf(version, size, "%d.%d.%d", BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION);
    }
}

void *BML_Malloc(size_t size) {
    return malloc(size);
}

void *BML_Calloc(size_t num, size_t size) {
    return calloc(num, size);
}

void *BML_Realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void BML_Free(void *ptr) {
    return free(ptr);
}

char *BML_Strdup(const char *str) {
    if (!str)
        return nullptr;

    size_t len = strlen(str);
    char *dup = static_cast<char *>(malloc(len + 1));
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}