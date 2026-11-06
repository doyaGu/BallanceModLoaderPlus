#ifndef BML_ENGINE_H
#define BML_ENGINE_H

#include "BML/Mod.h"
#define BML_ENGINE_INTERFACE_ID "bml.engine"
#define BML_ENGINE_INTERFACE_MAJOR 1u
BML_BEGIN_CDECLS
#define BML_ENGINE_INTERFACE_1_0_SIZE                                                              \
    (offsetof(BML_EngineInterface, BorrowObject) + sizeof(((BML_EngineInterface *)0)->BorrowObject))
#pragma pack(push, 8)

typedef struct BML_EngineInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *BorrowContext)(BML_ModContextHandle context, void **outContext);
    int(BML_CDECL *ReferenceObject)(BML_ModContextHandle context, const void *object,
                                    BML_ObjectRef *out);
    int(BML_CDECL *BorrowObject)(BML_ModContextHandle context, BML_ObjectRef object,
                                 uint32_t expectedClass, void **outObject);
} BML_EngineInterface;

#pragma pack(pop)
BML_END_CDECLS

#endif
