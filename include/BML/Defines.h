#ifndef BML_DEFINES_H
#define BML_DEFINES_H

#include "BML/Version.h"
#include "BML/Export.h"

#ifndef BML_BEGIN_CDECLS
#   ifdef __cplusplus
#       define BML_BEGIN_CDECLS extern "C" {
#   else
#       define BML_BEGIN_CDECLS
#   endif
#endif // !BML_BEGIN_CDECLS

#ifndef BML_END_CDECLS
#   ifdef __cplusplus
#       define BML_END_CDECLS }
#   else
#       define BML_END_CDECLS
#   endif
#endif // !BML_END_CDECLS

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#endif // BML_DEFINES_H
