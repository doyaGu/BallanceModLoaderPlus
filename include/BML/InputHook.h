// Reads input for a Mod, and blocks the game from reading it. Get the single
// instance from IBML::GetInputManager; do not construct one.
//
// Most functions here just forward to the game's CKInputManager, but the loader
// has replaced several slots of that manager's vtable, so a forwarded call also
// runs the loader's blocking check. That check is what lets a Mod take the
// keyboard while a text field is focused: the game's own scripts read through the
// same patched slots and see nothing.
//
// Three groups of functions behave differently while a device is blocked:
//   - the plain readers below return an empty result, because they go through the
//     patched slots;
//   - the o-prefixed readers further down call the original slots and so keep
//     reading the real device;
//   - the functions that are not device reads, meaning GetKeyName,
//     GetKeyFromName, the repetition and attachment queries, the cursor
//     functions, and Pause, are not affected by the block at all.
//
// Blocked reads with an out parameter differ in how they report emptiness.
// GetKeyboardState hands back a zeroed 256-byte buffer and GetMouseButtonsState
// fills oStates with KS_IDLE, but GetMousePosition and GetMouseRelativePosition
// return without touching the vector, so initialize it before the call rather
// than expecting a zero.
//
// Everything here is for the game thread only, from inside a Mod callback. There
// is no locking, and the snapshots the edge detection compares against advance
// once per frame.
#ifndef BML_INPUTHOOK_H
#define BML_INPUTHOOK_H

#include <cstdint>

#include "CKInputManager.h"

#include "BML/Defines.h"

typedef enum CK_INPUT_DEVICE {
    CK_INPUT_DEVICE_KEYBOARD = 0,
    CK_INPUT_DEVICE_MOUSE    = 1,
    CK_INPUT_DEVICE_JOYSTICK = 2,
    CK_INPUT_DEVICE_COUNT    = 3,
} CK_INPUT_DEVICE;

class BML_EXPORT InputHook {
public:
    explicit InputHook(CKInputManager *input);
    ~InputHook();

    void EnableKeyboardRepetition(CKBOOL iEnable = TRUE);
    CKBOOL IsKeyboardRepetitionEnabled();

    // iKey is a CKKEYBOARD value, not a Win32 virtual key code. oStamp, when
    // given, receives the time stamp of the press or of the toggle, not a
    // duration.
    CKBOOL IsKeyDown(CKDWORD iKey, CKDWORD *oStamp = nullptr);
    CKBOOL IsKeyUp(CKDWORD iKey);
    CKBOOL IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp = nullptr);

    // Edge detection the loader adds on top of CKInputManager, which has no
    // equivalent: these compare the key against the snapshot taken at the end of
    // the previous frame and are true for exactly one frame per keystroke. Both
    // work only while the calling Mod runs once per frame; a callback that skips
    // frames will miss edges. They index a 256-entry snapshot by iKey without
    // checking it, so keep iKey inside the CKKEYBOARD range.
    CKBOOL IsKeyPressed(CKDWORD iKey);
    CKBOOL IsKeyReleased(CKDWORD iKey);

    // oKeyName must point to a buffer the caller owns; CKInputManager writes the
    // name into it and does not allocate.
    void GetKeyName(CKDWORD iKey, CKSTRING oKeyName);
    CKDWORD GetKeyFromName(CKSTRING iKeyName);

    // Points at the manager's own 256-byte array, indexed by CKKEYBOARD value,
    // where a nonzero entry means the key is down. The pointer stays valid, but
    // the bytes behind it change every frame, so read them, do not keep them.
    // Returns nullptr only when the input manager is gone, during shutdown.
    unsigned char *GetKeyboardState();
    CKBOOL IsKeyboardAttached();

    // The keyboard events of this frame, which is what to read when a Mod must
    // not miss a fast double tap that per-frame polling would merge.
    // GetKeyFromBuffer returns KEY_PRESSED or KEY_RELEASED for the event at i,
    // and NO_KEY when i is out of range.
    int GetNumberOfKeyInBuffer();
    int GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp = nullptr);

    // IsMouseClicked and IsMouseToggled are the CKInputManager edges: clicked
    // means up last frame and down now, toggled means down last frame and up now.
    // The names do not follow IsKeyPressed and IsKeyReleased, but the meanings
    // line up with them.
    CKBOOL IsMouseButtonDown(CK_MOUSEBUTTON iButton);
    CKBOOL IsMouseClicked(CK_MOUSEBUTTON iButton);
    CKBOOL IsMouseToggled(CK_MOUSEBUTTON iButton);

    // oStates gets one byte per button, carrying the KS_PRESSED mask when down
    // and KS_RELEASED when up.
    void GetMouseButtonsState(CKBYTE oStates[4]);

    // iAbsolute TRUE asks for screen coordinates, FALSE for coordinates relative
    // to the render window.
    void GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute = TRUE);

    // Movement since the previous frame, with the wheel displacement in z.
    void GetMouseRelativePosition(VxVector &oPosition);

    // The window-relative position as of the end of the previous frame, which is
    // what to subtract from GetMousePosition to get this frame's movement. The
    // loader takes that snapshot through the patched slots, so while the mouse is
    // blocked it stops being refreshed and keeps reporting where the cursor was
    // when the block went up.
    void GetLastMousePosition(Vx2DVector &position);
    CKBOOL IsMouseAttached();

    // iJoystick runs from CKJOY_1 to CKMAX_JOY - 1. Nothing here range-checks it,
    // unlike the script bindings, so an out-of-range index reaches CKInputManager
    // as it stands. Check IsJoystickAttached before reading a device.
    CKBOOL IsJoystickAttached(int iJoystick);
    void GetJoystickPosition(int iJoystick, VxVector *oPosition);
    void GetJoystickRotation(int iJoystick, VxVector *oRotation);
    void GetJoystickSliders(int iJoystick, Vx2DVector *oPosition);
    void GetJoystickPointOfViewAngle(int iJoystick, float *oAngle);
    CKDWORD GetJoystickButtonsState(int iJoystick);
    CKBOOL IsJoystickButtonDown(int iJoystick, int iButton);

    // Suspends the input manager itself, so no reader here sees fresh input while
    // it is paused, the o-prefixed ones included. This is neither the per-device
    // block below nor a pause of the game. There is no counter behind it, so two
    // Mods pausing and unpausing on their own schedules will undo each other, and
    // whoever pauses is responsible for unpausing. Prefer AcquireBlock.
    void Pause(CKBOOL pause);

    // The system cursor, which Ballance hides during play. The loader shows it
    // again by itself whenever ImGui wants the mouse and hides it when ImGui is
    // done, so a Mod that leaves the cursor in a state of its own choosing will
    // see the loader override it on the next frame.
    void ShowCursor(CKBOOL iShow);
    CKBOOL GetCursorVisibility();
    VXCURSOR_POINTER GetSystemCursor();
    void SetSystemCursor(VXCURSOR_POINTER cursor);

    // The o prefix stands for original: these call the input manager slots the
    // loader replaced, so they report the real device even while it is blocked.
    // They exist for the code that puts a block up, which still has to read the
    // input it took: the command bar reads the keyboard this way while everything
    // else sees nothing. Do not use them to work around another Mod's block.
    CKBOOL oIsKeyDown(CKDWORD iKey, CKDWORD *oStamp = nullptr);
    CKBOOL oIsKeyUp(CKDWORD iKey);
    CKBOOL oIsKeyToggled(CKDWORD iKey, CKDWORD *oStamp = nullptr);

    // The block-ignoring form of IsKeyPressed and IsKeyReleased, against the same
    // end-of-previous-frame snapshot.
    CKBOOL oIsKeyPressed(CKDWORD iKey);
    CKBOOL oIsKeyReleased(CKDWORD iKey);

    unsigned char *oGetKeyboardState();
    int oGetNumberOfKeyInBuffer();
    int oGetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp = nullptr);

    CKBOOL oIsMouseButtonDown(CK_MOUSEBUTTON iButton);
    CKBOOL oIsMouseClicked(CK_MOUSEBUTTON iButton);
    CKBOOL oIsMouseToggled(CK_MOUSEBUTTON iButton);
    void oGetMouseButtonsState(CKBYTE oStates[4]);

    // Kept for the Mods built against older loader releases. They are the
    // keyboard-only case of Block and Unblock, sharing the one counter with them,
    // so SetBlock(true) raises the count and SetBlock(false) lowers it rather than
    // assigning the state its signature suggests. Two SetBlock(true) calls need
    // two SetBlock(false) calls to lift the block. Use AcquireBlock instead.
    bool IsBlock();
    void SetBlock(bool block);

    // A block per device, counted rather than boolean, so several Mods can hold
    // the same device at once and the device stays blocked until the last of them
    // lets go. IsBlocked returns that count, which is nonzero while blocked.
    // Unblock stops at zero, so an unmatched Unblock cannot make the count go
    // negative, but it does cancel somebody else's Block. Pair every Block with
    // exactly one Unblock, and reach for AcquireBlock when the code that unblocks
    // is not obviously the code that blocked.
    int IsBlocked(CK_INPUT_DEVICE device);
    void Block(CK_INPUT_DEVICE device);
    void Unblock(CK_INPUT_DEVICE device);

    enum InputBlockMask : uint32_t {
        INPUT_BLOCK_KEYBOARD = 1u << CK_INPUT_DEVICE_KEYBOARD,
        INPUT_BLOCK_MOUSE = 1u << CK_INPUT_DEVICE_MOUSE,
        INPUT_BLOCK_JOYSTICK = 1u << CK_INPUT_DEVICE_JOYSTICK,
        INPUT_BLOCK_ALL = INPUT_BLOCK_KEYBOARD | INPUT_BLOCK_MOUSE | INPUT_BLOCK_JOYSTICK,
    };

    // Blocks every device in deviceMask in one step and returns a token that
    // remembers which ones, which is the form to prefer: releasing the token
    // lowers exactly the counts this call raised, and releasing it twice or
    // releasing a token that was never handed out does nothing, so a mistake here
    // cannot unblock a device somebody else is holding. AcquireBlock returns 0
    // when deviceMask names no known device, and ReleaseBlock ignores 0, so an
    // unchecked failure leaves the state alone. Bits outside INPUT_BLOCK_ALL are
    // dropped. The loader uses this for its own command bar.
    uint64_t AcquireBlock(uint32_t deviceMask);
    void ReleaseBlock(uint64_t token);

    // Loader-internal. It runs the input manager's own per-frame work and then
    // takes the keyboard and mouse-position snapshots that IsKeyPressed,
    // IsKeyReleased, and GetLastMousePosition compare against. The loader calls
    // it once per frame, at the very end of the frame, after every Mod callback
    // has run. A Mod that calls it advances those snapshots early and makes the
    // edge detection of every other Mod in the same frame miss its keystrokes.
    void Process();

private:
    static bool IsValid();
    struct Impl;
    Impl *m_Impl;
};

#endif // BML_INPUTHOOK_H
