// Frozen 1.0 layouts, compiled for every supported target by NativeApiLayoutTest.
#pragma pack(push, 1)
#include "BML/Mod.h"
#include "BML/Config.h"
#include "BML/Timer.h"
#include "BML/Input.h"
#include "BML/Bui.h"
#include "BML/Gui.h"
#include "BML/Engine.h"
#include "BML/ImGui.h"
#include "BML/Gameplay.h"
#include "BML/Scene.h"
#include "BML/UI.h"
struct CallerPacking {
    char Byte;
    uint64_t Integer;
};
_Static_assert(offsetof(struct CallerPacking, Integer) == 1, "headers restore caller packing");
#pragma pack(pop)
#if UINTPTR_MAX == UINT32_MAX
#define BML_ABI_SIZE(x32, x64) (x32)
#else
#define BML_ABI_SIZE(x32, x64) (x64)
#endif
#define BML_RECORD(type, x32, x64)                                                                 \
    _Static_assert(sizeof(type) == BML_ABI_SIZE(x32, x64), #type " layout")
BML_RECORD(BML_InterfaceHeader, 12, 24);
BML_RECORD(BML_ModDependency, 16, 32);
BML_RECORD(BML_ModDefinition, 40, 80);
BML_RECORD(BML_ModInfo, 32, 40);
BML_RECORD(BML_ModInterface, 28, 56);
BML_RECORD(BML_ModContextInterface, 48, 96);
BML_RECORD(BML_ConfigValue, 12, 24);
BML_RECORD(BML_ConfigInterface, 56, 112);
BML_RECORD(BML_TimerDefinition, 48, 56);
BML_RECORD(BML_TimerEvent, 56, 64);
BML_RECORD(BML_TimerInterface, 32, 64);
BML_RECORD(BML_InputKeyboardState, 28, 48);
BML_RECORD(BML_InputMouseState, 56, 64);
BML_RECORD(BML_InputJoystickState, 48, 56);
BML_RECORD(BML_InputInterface, 64, 128);
BML_RECORD(BML_GuiElementInfo, 96, 104);
BML_RECORD(BML_GuiInterface, 100, 200);
BML_RECORD(BML_BuiInterface, 100, 200);
_Static_assert(BML_TIMER_EVENT_1_0_SIZE == BML_ABI_SIZE(52, 60), "timer prefix excludes padding");
_Static_assert(BML_GUI_ELEMENT_INFO_1_0_SIZE == BML_ABI_SIZE(96, 100),
               "GUI prefix excludes padding");
static int BML_CDECL Notify(void *data, BML_ModContextHandle context) {
    (void)data;
    (void)context;
    return BML_OK;
}
static const BML_ModCallbacks Callbacks = {.StructSize = sizeof(BML_ModCallbacks),
                                           .OnLoad = Notify};
const BML_ModCallbacks *BML_TestNativeCallingConvention(void) { return &Callbacks; }
