#ifndef BML_IMGUI_H
#define BML_IMGUI_H
#include "BML/Mod.h"
#define BML_IMGUI_INTERFACE_ID "bml.imgui"
#define BML_IMGUI_INTERFACE_MAJOR 1u
BML_BEGIN_CDECLS
#define BML_IMGUI_INTERFACE_1_0_SIZE                                                               \
    (offsetof(BML_ImGuiInterface, AddButtonImage) +                                                \
     sizeof(((BML_ImGuiInterface *)0)->AddButtonImage))
#pragma pack(push, 8)
typedef struct BML_ImGuiInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *BorrowContext)(BML_ModContextHandle, void **outContext);
    int(BML_CDECL *AddButtonImage)(BML_ModContextHandle, void *drawList, BML_Vec2 position,
                                   uint32_t kind, int32_t state, const char *text,
                                   BML_Vec2 textAlignment);
} BML_ImGuiInterface;

#pragma pack(pop)
BML_END_CDECLS
#endif
