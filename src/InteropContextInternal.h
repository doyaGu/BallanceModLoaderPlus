#ifndef BML_INTEROPCONTEXTINTERNAL_H
#define BML_INTEROPCONTEXTINTERNAL_H

#include <cstdint>

/* Private, immutable metadata passed to every v2 provider callback. */
struct BML_InteropCallContext {
    const char *OwnerId = nullptr;
    // Private runtime identity.  A context is only valid for the service that
    // created it; matching a session number in another ModContext is not
    // sufficient.
    uint64_t ServiceId = 0;
    uint64_t SessionId = 0;
};

#endif
