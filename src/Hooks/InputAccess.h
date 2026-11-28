#ifndef BML_HOOKS_INPUTACCESS_H
#define BML_HOOKS_INPUTACCESS_H
#include "BML/Input.h"
class InputHook;
// Loader-private access to methods already saved by the input hook.
struct InputHookAccess {
    static uint64_t Revision(InputHook &);
    static void ReadRawMouse(InputHook &, BML_InputMouseState &);
    static void ReadRawJoystick(InputHook &, uint32_t, BML_InputJoystickState &);
};
#endif
