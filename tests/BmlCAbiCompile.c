#include "BML/BML.h"

void BML_TestCAbiMemoryOwnership(char **strings, wchar_t **wideStrings, size_t count) {
    BML_FreeStringArray(strings, count);
    BML_FreeWStringArray(wideStrings, count);
}

// The loader owns what BML_GetLoaderPath returns and the caller owns what
// BML_GetModRoot returns, so this also checks that the two are spelled apart in C:
// dropping the const or freeing the borrowed one would not compile.
const char *BML_TestCAbiLoaderPath(void) {
    return BML_GetLoaderPathUtf8(BML_DIR_LOADER);
}

void BML_TestCAbiModRoot(void) {
    char *root = BML_GetModRootUtf8(NULL);
    BML_FreeString(root);
}
