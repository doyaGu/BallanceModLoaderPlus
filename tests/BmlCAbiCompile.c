#include "BML/BML.h"

void BML_TestCAbiMemoryOwnership(char **strings, wchar_t **wideStrings, size_t count) {
    BML_FreeStringArray(strings, count);
    BML_FreeWStringArray(wideStrings, count);
}
