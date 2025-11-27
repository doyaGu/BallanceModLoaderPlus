#ifndef BML_VERSION_H
#define BML_VERSION_H

#include "bml_types.h"

BML_BEGIN_CDECLS

#define BML_API_VERSION_MAJOR 0u
#define BML_API_VERSION_MINOR 4u
#define BML_API_VERSION_PATCH 0u

#define BML_API_VERSION_STR "0.4.0"

static inline BML_Version bmlGetApiVersion(void) {
    return bmlMakeVersion(BML_API_VERSION_MAJOR,
                           BML_API_VERSION_MINOR,
                           BML_API_VERSION_PATCH);
}

BML_END_CDECLS

#endif /* BML_VERSION_H */
