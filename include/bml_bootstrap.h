#ifndef BML_BOOTSTRAP_H
#define BML_BOOTSTRAP_H

#include "bml_types.h"

BML_BEGIN_CDECLS

typedef enum BML_BootstrapState {
    BML_BOOTSTRAP_STATE_NOT_STARTED = 0,
    BML_BOOTSTRAP_STATE_CORE_INITIALIZED = 1,
    BML_BOOTSTRAP_STATE_MODULES_DISCOVERED = 2,
    BML_BOOTSTRAP_STATE_MODULES_LOADED = 3,
    BML_BOOTSTRAP_STATE_READY = 4,
    BML_BOOTSTRAP_STATE_SHUTDOWN = 5,
    _BML_BOOTSTRAP_STATE_FORCE_32BIT = 0x7FFFFFFF
} BML_BootstrapState;

#define BML_BOOTSTRAP_FLAG_NONE            0x00000000u
#define BML_BOOTSTRAP_FLAG_SKIP_DISCOVERY  0x00000001u
#define BML_BOOTSTRAP_FLAG_SKIP_LOAD       0x00000002u

typedef struct BML_BootstrapConfig {
    size_t struct_size;
    uint32_t flags;
    const char *mods_dir_utf8;
    void *reserved;
} BML_BootstrapConfig;

#define BML_BOOTSTRAP_CONFIG_INIT { sizeof(BML_BootstrapConfig), BML_BOOTSTRAP_FLAG_NONE, NULL, NULL }

BML_END_CDECLS

#endif /* BML_BOOTSTRAP_H */