// Pure C interface for optional Native Mod pages in the loader's built-in Mods
// menu. ModMenu.hpp is the corresponding type-safe C++ authoring layer.
// Registering a page adds one Ballance-styled entry to that Mod's details page;
// activating the entry routes to the page's Draw callback inside the loader's
// ImGui frame.
//
// Draw runs on the game thread inside the loader's active full-viewport ImGui
// page. Draw widgets directly; do not begin or end an ImGui frame. The loader
// copies Id, Label, and Description during registration. UserData and the
// callbacks remain owned by the Mod and must stay valid until the page is
// unregistered; every callback address must belong to the owner DLL. Do not
// destroy a page from one of its own callbacks. Remaining pages are removed
// before the owner DLL is released.
#ifndef BML_MOD_MENU_H
#define BML_MOD_MENU_H

#include "BML/Interface.h"

BML_BEGIN_CDECLS

#define BML_MOD_MENU_INTERFACE_ID "bml.mod-menu"
#define BML_MOD_MENU_INTERFACE_MAJOR 1
#define BML_MOD_MENU_INTERFACE_MINOR 0

typedef enum BML_ModMenuPageAction {
    BML_MOD_MENU_PAGE_NONE = 0,
    BML_MOD_MENU_PAGE_BACK = 1,
    BML_MOD_MENU_PAGE_CLOSE = 2,
    _BML_MOD_MENU_PAGE_ACTION_FORCE_32BIT = 0x7fffffff
} BML_ModMenuPageAction;

typedef enum BML_ModMenuPageLeaveReason {
    BML_MOD_MENU_PAGE_LEAVE_BACK = 0,
    BML_MOD_MENU_PAGE_LEAVE_CLOSE = 1,
    _BML_MOD_MENU_PAGE_LEAVE_REASON_FORCE_32BIT = 0x7fffffff
} BML_ModMenuPageLeaveReason;

// The loader initializes this frame before every Draw call. Draw returns a
// BML status and writes only Action. Later minor versions may append fields.
typedef struct BML_ModMenuPageFrame {
    size_t StructSize;
    BML_ModMenuPageAction Action;
} BML_ModMenuPageFrame;

#define BML_MOD_MENU_PAGE_FRAME_1_0_SIZE                                    \
    (offsetof(BML_ModMenuPageFrame, Action) +                               \
     sizeof(((BML_ModMenuPageFrame *) 0)->Action))

typedef int (BML_CDECL *BML_ModMenuPageDraw)(
    void *userData, BML_ModMenuPageFrame *frame);
typedef void (BML_CDECL *BML_ModMenuPageEnter)(void *userData);
typedef void (BML_CDECL *BML_ModMenuPageLeave)(
    void *userData, BML_ModMenuPageLeaveReason reason);

typedef struct BML_ModMenuPage {
    size_t StructSize;
    const char *Id;
    const char *Label;
    const char *Description;
    void *UserData;
    BML_ModMenuPageDraw Draw;
    BML_ModMenuPageEnter Enter;
    BML_ModMenuPageLeave Leave;
} BML_ModMenuPage;

#define BML_MOD_MENU_PAGE_1_0_SIZE                                           \
    (offsetof(BML_ModMenuPage, Leave) + sizeof(((BML_ModMenuPage *) 0)->Leave))

typedef struct BML_ModMenuInterface {
    BML_InterfaceHeader Header;

    // ownerId may be null when the calling DLL owns exactly one Mod. Page ids
    // are unique only within one owner. Registration and removal are main-thread
    // operations; registering the same owner/id twice is rejected.
    int (BML_CDECL *RegisterPage)(const char *ownerId, const BML_ModMenuPage *page);
    int (BML_CDECL *UnregisterPage)(const char *ownerId, const char *pageId);
} BML_ModMenuInterface;

BML_END_CDECLS

#endif // BML_MOD_MENU_H
