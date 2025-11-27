#ifndef BML_HOT_RELOAD_H
#define BML_HOT_RELOAD_H

#include "bml_types.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

#define BML_MOD_LIFECYCLE_SCHEMA_HASH 0x4d4c4345u /* 'MLCE' */
#define BML_MOD_LIFECYCLE_SCHEMA_VERSION 1u

typedef struct BML_ModLifecycleEvent {
    BML_Version version;
    const char *mod_id;
    size_t mod_id_length;
} BML_ModLifecycleEvent;

#pragma pack(push, 1)
typedef struct BML_ModLifecycleWireHeader {
    BML_Version version;
    uint32_t id_length;
} BML_ModLifecycleWireHeader;
#pragma pack(pop)

static inline BML_Bool bmlParseModLifecycleEvent(const void *payload,
                                                 size_t payload_len,
                                                 BML_ModLifecycleEvent *out_event) {
    if (!payload || !out_event)
        return BML_FALSE;

    if (payload_len < sizeof(BML_ModLifecycleWireHeader))
        return BML_FALSE;

    const BML_ModLifecycleWireHeader *header = (const BML_ModLifecycleWireHeader *)payload;
    size_t required = sizeof(BML_ModLifecycleWireHeader) + (size_t)header->id_length;
    if (required > payload_len)
        return BML_FALSE;

    out_event->version = header->version;
    out_event->mod_id_length = header->id_length;
    out_event->mod_id = (const char *)payload + sizeof(BML_ModLifecycleWireHeader);
    return BML_TRUE;
}

BML_END_CDECLS

#endif /* BML_HOT_RELOAD_H */
