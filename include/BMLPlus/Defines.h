#ifndef BML_DEFINES_H
#define BML_DEFINES_H

#include "Version.h"
#include "Export.h"
#include "Guids.h"

#include "CKDefines.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define TOCKSTRING(str) const_cast<CKSTRING>(str)

#endif // BML_DEFINES_H
