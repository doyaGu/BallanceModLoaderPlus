#ifndef BML_TIME_H
#define BML_TIME_H

#include "BML/Interface.h"

BML_BEGIN_CDECLS

#pragma pack(push, 8)

#define BML_TIME_INTERFACE_ID "bml.time"
#define BML_TIME_INTERFACE_MAJOR 1
#define BML_TIME_INTERFACE_MINOR 0

// CKTimeManager values, in milliseconds. MainTickCount is the raw CKDWORD
// counter and wraps at 2^32; it is not a monotonic 64-bit frame identity.
typedef struct BML_TimeClock {
    float TimeMs;
    float AbsoluteMs;
    float DeltaMs;
    uint32_t MainTickCount;
} BML_TimeClock;

typedef struct BML_TimeInterface {
    BML_InterfaceHeader Header;

    // Game-thread-only. Leaves out untouched on failure; answers
    // BML_ERROR_UNAVAILABLE while the CK time manager is absent.
    int (BML_CDECL *ReadClock)(BML_TimeClock *out);
} BML_TimeInterface;

#pragma pack(pop)

BML_END_CDECLS

#endif // BML_TIME_H
