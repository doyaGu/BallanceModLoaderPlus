// Pure C interface for Native Mod pages in the loader's built-in Mods
// menu. ModMenu.hpp is the corresponding type-safe C++ authoring layer.
// A visible page adds one Ballance-styled entry to its Mod's details page.
// Hidden pages can only be reached from another page of the same Mod.
//
// Draw runs on the game thread inside the loader's central, scrollable ImGui
// content region. Draw widgets directly; do not begin or end an ImGui frame.
// The loader draws the native Back control outside this region. The loader
// copies Id, Label, and Description during registration. UserData and the
// callbacks remain owned by the Mod and must stay valid until the page is
// unregistered; every callback address must belong to the owner DLL. When
// Release is non-null, successful registration transfers one UserData reference
// to the loader and Release returns it after the last active callback during
// unregistration or owner cleanup. A page may unregister itself during Draw,
// but must not destroy its UserData or callback code before Draw returns.
// Remaining pages are removed before the owner DLL is released.
#ifndef BML_MOD_MENU_H
#define BML_MOD_MENU_H

#include "BML/Interface.h"

BML_BEGIN_CDECLS

#pragma pack(push, 8)

#define BML_MOD_MENU_INTERFACE_ID "bml.mod-menu"
#define BML_MOD_MENU_INTERFACE_MAJOR 1
#define BML_MOD_MENU_INTERFACE_MINOR 0
#define BML_MOD_MENU_PAGE_ID_CAPACITY 256

typedef enum BML_ModMenuPageFlags {
    BML_MOD_MENU_PAGE_VISIBLE = 0,
    BML_MOD_MENU_PAGE_HIDDEN = 1,
    BML_MOD_MENU_PAGE_FLAGS_FORCE_32BIT = 0x7fffffff
} BML_ModMenuPageFlags;

typedef enum BML_ModMenuPageAction {
    BML_MOD_MENU_PAGE_NONE = 0,
    BML_MOD_MENU_PAGE_BACK = 1,
    BML_MOD_MENU_PAGE_CLOSE = 2,
    BML_MOD_MENU_PAGE_PUSH = 3,
    BML_MOD_MENU_PAGE_REPLACE = 4,
    BML_MOD_MENU_PAGE_ACTION_FORCE_32BIT = 0x7fffffff
} BML_ModMenuPageAction;

typedef enum BML_ModMenuPageEnterReason {
    BML_MOD_MENU_PAGE_ENTER_PUSH = 0,
    BML_MOD_MENU_PAGE_ENTER_REPLACE = 1,
    BML_MOD_MENU_PAGE_ENTER_BACK = 2,
    BML_MOD_MENU_PAGE_ENTER_REASON_FORCE_32BIT = 0x7fffffff
} BML_ModMenuPageEnterReason;

typedef enum BML_ModMenuPageLeaveReason {
    BML_MOD_MENU_PAGE_LEAVE_BACK = 0,
    BML_MOD_MENU_PAGE_LEAVE_CLOSE = 1,
    BML_MOD_MENU_PAGE_LEAVE_PUSH = 2,
    BML_MOD_MENU_PAGE_LEAVE_REPLACE = 3,
    BML_MOD_MENU_PAGE_LEAVE_REASON_FORCE_32BIT = 0x7fffffff
} BML_ModMenuPageLeaveReason;

// The loader initializes this frame before Draw. For Push/Replace, write a
// NUL-terminated page id to TargetPageId (at most 255 bytes). Routing resolves
// that id only within the current owner and runs after Draw returns. Draw,
// Enter and Leave return BML status codes.
typedef struct BML_ModMenuPageFrame {
    size_t StructSize;
    BML_ModMenuPageAction Action;
    char TargetPageId[BML_MOD_MENU_PAGE_ID_CAPACITY];
} BML_ModMenuPageFrame;

#define BML_MOD_MENU_PAGE_FRAME_1_0_SIZE                                    \
    (offsetof(BML_ModMenuPageFrame, TargetPageId) +                         \
     sizeof(((BML_ModMenuPageFrame *) 0)->TargetPageId))

typedef int (BML_CDECL *BML_ModMenuPageDraw)(
    void *userData, BML_ModMenuPageFrame *frame);
typedef int (BML_CDECL *BML_ModMenuPageEnter)(
    void *userData, BML_ModMenuPageEnterReason reason);
typedef int (BML_CDECL *BML_ModMenuPageLeave)(
    void *userData, BML_ModMenuPageLeaveReason reason);
typedef void (BML_CDECL *BML_ModMenuPageRelease)(void *userData);

typedef struct BML_ModMenuPage {
    size_t StructSize;
    const char *Id;
    const char *Label;
    const char *Description;
    void *UserData;
    BML_ModMenuPageDraw Draw;
    BML_ModMenuPageEnter Enter;
    BML_ModMenuPageLeave Leave;
    BML_ModMenuPageRelease Release;
    unsigned int Flags;
} BML_ModMenuPage;

#define BML_MOD_MENU_PAGE_1_0_SIZE                                           \
    (offsetof(BML_ModMenuPage, Flags) +                                      \
     sizeof(((BML_ModMenuPage *) 0)->Flags))

typedef struct BML_ModMenuInterface {
    BML_InterfaceHeader Header;

    // ownerId may be null when the calling DLL owns exactly one Mod. Page ids
    // are unique only within one owner. Registration and removal are main-thread
    // operations; registering the same owner/id twice is rejected.
    int (BML_CDECL *RegisterPage)(const char *ownerId, const BML_ModMenuPage *page);
    int (BML_CDECL *UnregisterPage)(const char *ownerId, const char *pageId);
} BML_ModMenuInterface;

#pragma pack(pop)

BML_END_CDECLS

#endif // BML_MOD_MENU_H
