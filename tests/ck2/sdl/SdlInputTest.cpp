#include <gtest/gtest.h>

#include <array>
#include <memory>

#include <SDL3/SDL.h>
#include "CKAll.h"
#include "SdlInputManager.h"
#include "BML/InputHook.h"
#include "Hooks/HookLifecycle.h"

namespace {
class CountingInputManager : public SdlInputManager {
public:
    using SdlInputManager::SdlInputManager;
    int PostProcesses = 0;
    CKERROR PostProcess() override {
        ++PostProcesses;
        return SdlInputManager::PostProcess();
    }
};

class SdlInputTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    CountingInputManager *Input = nullptr;
    SDL_Window *Window = nullptr;
    SDL_JoystickID VirtualId = 0;
    SDL_Joystick *VirtualJoystick = nullptr;

    void SetUp() override {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) << SDL_GetError();
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        Window = SDL_CreateWindow("BML input test", 640, 480, SDL_WINDOW_HIDDEN);
        ASSERT_NE(nullptr, Window) << SDL_GetError();
        Input = new CountingInputManager(Context);
        ASSERT_EQ(CK_OK, Input->OnCKInit());
        ASSERT_TRUE(Input->SetInputViewport(Window, VxRect(0, 0, 640, 480), 640, 480));
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    }

    void TearDown() override {
        if (VirtualJoystick) SDL_CloseJoystick(VirtualJoystick);
        if (VirtualId) SDL_DetachVirtualJoystick(VirtualId);
        if (Input) {
            Input->SetInputViewport(nullptr, VxRect(0, 0, 0, 0), 0, 0);
            delete Input;
        }
        if (Window) SDL_DestroyWindow(Window);
        if (Context) CKCloseContext(Context);
        CKShutdown();
        SDL_Quit();
    }

    void Key(bool down, Uint64 milliseconds = 1000, bool repeat = false) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.windowID = SDL_GetWindowID(Window);
        event.key.scancode = SDL_SCANCODE_A;
        event.key.key = SDLK_A;
        event.key.down = down;
        event.key.repeat = repeat;
        event.key.timestamp = SDL_MS_TO_NS(milliseconds);
        ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
    }

    void Button(bool down) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(Window);
        event.button.button = SDL_BUTTON_LEFT;
        event.button.down = down;
        event.button.x = 55;
        event.button.y = 33;
        ASSERT_TRUE(SDL_PushEvent(&event));
    }

    void Frame() { ASSERT_EQ(CK_OK, Input->PreProcess()); }

    void Gamepad() {
        SDL_VirtualJoystickDesc desc{};
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
        desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        desc.name = "BML virtual gamepad";
        VirtualId = SDL_AttachVirtualJoystick(&desc);
        ASSERT_NE(0u, VirtualId) << SDL_GetError();
        VirtualJoystick = SDL_OpenJoystick(VirtualId);
        ASSERT_NE(nullptr, VirtualJoystick) << SDL_GetError();
        Frame();
        ASSERT_TRUE(Input->IsJoystickAttached(0));
        ASSERT_TRUE(SDL_SetJoystickVirtualAxis(VirtualJoystick, SDL_GAMEPAD_AXIS_LEFTX, 20000));
        ASSERT_TRUE(SDL_SetJoystickVirtualAxis(VirtualJoystick, SDL_GAMEPAD_AXIS_RIGHTX, 16000));
        ASSERT_TRUE(SDL_SetJoystickVirtualButton(VirtualJoystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
        ASSERT_TRUE(SDL_SetJoystickVirtualButton(VirtualJoystick, SDL_GAMEPAD_BUTTON_DPAD_UP, true));
        SDL_UpdateJoysticks();
        Frame();
    }
};

TEST_F(SdlInputTest, KeyboardBlockPreservesRawStateStampsAndBuffer) {
    CKInputManager *sdk = Input;
    sdk->Block(TRUE, CK_INPUT_DEVICE_KEYBOARD);
    Key(true, 1000);
    Frame();
    CKDWORD stamp = 77, key = 99;
    EXPECT_FALSE(sdk->IsKeyDown(CKKEY_A, &stamp));
    EXPECT_FALSE(sdk->IsKeyUp(CKKEY_B));
    EXPECT_FALSE(sdk->IsKeyToggled(CKKEY_A, &stamp));
    EXPECT_EQ(77u, stamp);
    EXPECT_EQ(KS_IDLE, sdk->GetKeyboardState()[CKKEY_A]);
    EXPECT_EQ(0, sdk->GetNumberOfKeyInBuffer());
    EXPECT_EQ(NO_KEY, sdk->GetKeyFromBuffer(0, key, &stamp));
    EXPECT_EQ(99u, key);
    EXPECT_EQ(77u, stamp);
    EXPECT_TRUE(sdk->IsKeyDown(CKKEY_A, &stamp, TRUE));
    EXPECT_EQ(1000u, stamp);
    EXPECT_TRUE(sdk->IsKeyUp(CKKEY_B, TRUE));
    EXPECT_EQ(KS_PRESSED, sdk->GetKeyboardState(TRUE)[CKKEY_A]);
    EXPECT_EQ(1, sdk->GetNumberOfKeyInBuffer(TRUE));
    EXPECT_EQ(KEY_PRESSED, sdk->GetKeyFromBuffer(0, key, &stamp, TRUE));
    EXPECT_EQ(CKKEY_A, key);
    EXPECT_EQ(1000u, stamp);
    sdk->Block(FALSE, CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_TRUE(sdk->IsKeyDown(CKKEY_A));
    EXPECT_EQ(1, sdk->GetNumberOfKeyInBuffer());
}

TEST_F(SdlInputTest, BlockingIsIdempotentAndIndependentForEachDevice) {
    Input->Block(TRUE, CK_INPUT_DEVICE_KEYBOARD);
    Input->Block(TRUE, CK_INPUT_DEVICE_KEYBOARD);
    Input->Block(TRUE, static_cast<CK_INPUT_DEVICE>(-1));
    Input->Block(FALSE, CK_INPUT_DEVICE_COUNT);
    Key(true);
    Button(true);
    Frame();
    EXPECT_FALSE(Input->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(Input->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    Input->Block(FALSE, CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_TRUE(Input->IsKeyDown(CKKEY_A));
    Input->Block(TRUE, CK_INPUT_DEVICE_MOUSE);
    EXPECT_TRUE(Input->IsKeyDown(CKKEY_A));
    EXPECT_FALSE(Input->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Input->IsKeyboardAttached());
}

TEST_F(SdlInputTest, RawBufferRetainsFastTapRepeatsAndReleaseStamp) {
    Input->EnableKeyboardRepetition(TRUE);
    Input->Block(TRUE, CK_INPUT_DEVICE_KEYBOARD);
    Key(true, 1000);
    Key(true, 1100, true);
    Key(false, 1250);
    Frame();
    ASSERT_EQ(3, Input->GetNumberOfKeyInBuffer(TRUE));
    CKDWORD key = 0, stamp = 0;
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(i == 2 ? KEY_RELEASED : KEY_PRESSED,
                  Input->GetKeyFromBuffer(i, key, &stamp, TRUE));
        EXPECT_EQ(CKKEY_A, key);
        EXPECT_EQ((std::array<CKDWORD, 3>{1000, 1100, 1250})[i], stamp);
    }
    EXPECT_TRUE(Input->IsKeyToggled(CKKEY_A, &stamp, TRUE));
    EXPECT_EQ(150u, stamp); // Preserve the engine's existing release-duration value.
    key = 71; stamp = 72;
    EXPECT_EQ(NO_KEY, Input->GetKeyFromBuffer(-1, key, &stamp, TRUE));
    EXPECT_EQ(NO_KEY, Input->GetKeyFromBuffer(3, key, &stamp, TRUE));
    EXPECT_EQ(71u, key);
    EXPECT_EQ(72u, stamp);
    EXPECT_EQ(0, Input->GetNumberOfKeyInBuffer());
}

TEST_F(SdlInputTest, MouseBlockRetainsPositionMotionButtonsAndEdges) {
    Input->Block(TRUE, CK_INPUT_DEVICE_MOUSE);
    Button(true);
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(Window);
    motion.motion.x = 55; motion.motion.y = 33;
    motion.motion.xrel = 7; motion.motion.yrel = -3;
    ASSERT_TRUE(SDL_PushEvent(&motion));
    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.windowID = SDL_GetWindowID(Window);
    wheel.wheel.y = 2;
    ASSERT_TRUE(SDL_PushEvent(&wheel));
    Frame();
    EXPECT_FALSE(Input->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_FALSE(Input->IsMouseClicked(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Input->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT, TRUE));
    EXPECT_TRUE(Input->IsMouseClicked(CK_MOUSEBUTTON_LEFT, TRUE));
    CKBYTE buttons[4] = {9, 9, 9, 9};
    Input->GetMouseButtonsState(buttons);
    for (auto state : buttons) EXPECT_EQ(KS_IDLE, state);
    Input->GetMouseButtonsState(buttons, TRUE);
    EXPECT_EQ(KS_PRESSED, buttons[0]);
    Vx2DVector position(91, 92);
    Input->GetMousePosition(position, FALSE);
    EXPECT_EQ(91, position.x); EXPECT_EQ(92, position.y);
    Input->GetMousePosition(position, FALSE, TRUE);
    EXPECT_EQ(55, position.x); EXPECT_EQ(33, position.y);
    VxVector movement(91, 92, 93);
    Input->GetMouseRelativePosition(movement);
    EXPECT_EQ(91, movement.x); EXPECT_EQ(93, movement.z);
    Input->GetMouseRelativePosition(movement, TRUE);
    EXPECT_EQ(7, movement.x); EXPECT_EQ(-3, movement.y);
    EXPECT_NE(0, movement.z);
    Input->PostProcess();
    Button(false);
    Frame();
    EXPECT_FALSE(Input->IsMouseToggled(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Input->IsMouseToggled(CK_MOUSEBUTTON_LEFT, TRUE));
    Input->Block(FALSE, CK_INPUT_DEVICE_MOUSE);
    EXPECT_TRUE(Input->IsMouseToggled(CK_MOUSEBUTTON_LEFT));
}

TEST_F(SdlInputTest, RawReadsStillRespectPauseAndFocusLoss) {
    Input->Block(TRUE, CK_INPUT_DEVICE_KEYBOARD);
    Key(true);
    Frame();
    ASSERT_TRUE(Input->IsKeyDown(CKKEY_A, nullptr, TRUE));
    Input->Pause(TRUE);
    EXPECT_FALSE(Input->IsKeyDown(CKKEY_A, nullptr, TRUE));
    EXPECT_EQ(0, Input->GetNumberOfKeyInBuffer(TRUE));
    Key(true, 2000);
    Frame();
    EXPECT_FALSE(Input->IsKeyDown(CKKEY_A, nullptr, TRUE));
    Input->Pause(FALSE);
    Key(true, 3000);
    Frame();
    ASSERT_TRUE(Input->IsKeyDown(CKKEY_A, nullptr, TRUE));
    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    focus.window.windowID = SDL_GetWindowID(Window);
    ASSERT_TRUE(SDL_PushEvent(&focus));
    Frame();
    EXPECT_FALSE(Input->IsKeyDown(CKKEY_A, nullptr, TRUE));
    EXPECT_EQ(0, Input->GetNumberOfKeyInBuffer(TRUE));
    EXPECT_FALSE(Input->IsKeyUp(CKKEY_A)); // The device is still blocked.
}

TEST_F(SdlInputTest, ResetClearsInputWithoutCancellingTheBlock) {
    Input->Block(TRUE, CK_INPUT_DEVICE_KEYBOARD);
    Key(true);
    Frame();
    ASSERT_EQ(CK_OK, Input->OnCKReset());
    EXPECT_EQ(KS_IDLE, Input->GetKeyboardState(TRUE)[CKKEY_A]);
    Key(true, 2000);
    Frame();
    EXPECT_TRUE(Input->IsKeyDown(CKKEY_A, nullptr, TRUE));
    EXPECT_FALSE(Input->IsKeyDown(CKKEY_A));
}

TEST_F(SdlInputTest, JoystickQueriesUseTheSameBlockAndRawRules) {
    Gamepad();
    ASSERT_FALSE(HasFatalFailure());
    CKInputManager *sdk = Input;
    VxVector rawPosition(0, 0, 0), rawRotation(0, 0, 0);
    Vx2DVector rawSliders(0, 0);
    float rawAngle = 0;
    sdk->GetJoystickPosition(0, &rawPosition);
    sdk->GetJoystickRotation(0, &rawRotation);
    sdk->GetJoystickSliders(0, &rawSliders);
    sdk->GetJoystickPointOfViewAngle(0, &rawAngle);
    CKDWORD rawButtons = sdk->GetJoystickButtonsState(0);
    ASSERT_GT(rawPosition.x, 0.1f);
    ASSERT_NE(0u, rawButtons);
    ASSERT_TRUE(sdk->IsJoystickButtonDown(0, SDL_GAMEPAD_BUTTON_SOUTH));
    sdk->Block(TRUE, CK_INPUT_DEVICE_JOYSTICK);
    VxVector position(9, 9, 9), rotation(8, 8, 8);
    Vx2DVector sliders(7, 7);
    float angle = 6;
    sdk->GetJoystickPosition(0, &position);
    sdk->GetJoystickRotation(0, &rotation);
    sdk->GetJoystickSliders(0, &sliders);
    sdk->GetJoystickPointOfViewAngle(0, &angle);
    EXPECT_EQ(9, position.x); EXPECT_EQ(8, rotation.x);
    EXPECT_EQ(7, sliders.x); EXPECT_EQ(6, angle);
    EXPECT_EQ(0u, sdk->GetJoystickButtonsState(0));
    EXPECT_FALSE(sdk->IsJoystickButtonDown(0, SDL_GAMEPAD_BUTTON_SOUTH));
    EXPECT_TRUE(sdk->IsJoystickAttached(0));
    sdk->GetJoystickPosition(0, &position, TRUE);
    sdk->GetJoystickRotation(0, &rotation, TRUE);
    sdk->GetJoystickSliders(0, &sliders, TRUE);
    sdk->GetJoystickPointOfViewAngle(0, &angle, TRUE);
    EXPECT_EQ(rawPosition.x, position.x); EXPECT_EQ(rawRotation.x, rotation.x);
    EXPECT_EQ(rawSliders.x, sliders.x); EXPECT_EQ(rawAngle, angle);
    EXPECT_EQ(rawButtons, sdk->GetJoystickButtonsState(0, TRUE));
    EXPECT_TRUE(sdk->IsJoystickButtonDown(0, SDL_GAMEPAD_BUTTON_SOUTH, TRUE));
    sdk->Pause(TRUE);
    EXPECT_EQ(0u, sdk->GetJoystickButtonsState(0, TRUE));
    EXPECT_FALSE(sdk->IsJoystickButtonDown(0, SDL_GAMEPAD_BUTTON_SOUTH, TRUE));
    sdk->Block(FALSE, CK_INPUT_DEVICE_JOYSTICK);
    sdk->Pause(FALSE);
    EXPECT_EQ(rawButtons, sdk->GetJoystickButtonsState(0));
}

TEST_F(SdlInputTest, EngineLeavesTheApplicationEventQueueAndRawBufferIntact) {
    Key(true);
    SDL_Event custom{};
    custom.type = SDL_EVENT_USER;
    custom.user.code = 42;
    ASSERT_TRUE(SDL_PushEvent(&custom));
    Frame();
    SDL_Event peek{};
    EXPECT_EQ(1, SDL_PeepEvents(&peek, 1, SDL_PEEKEVENT, SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN));
    EXPECT_EQ(1, SDL_PeepEvents(&peek, 1, SDL_PEEKEVENT, SDL_EVENT_USER, SDL_EVENT_USER));
    EXPECT_EQ(42, peek.user.code);
    Input->Block(TRUE, CK_INPUT_DEVICE_KEYBOARD);
    for (int i = 0; i < 3; ++i) {
        CKDWORD key = 0;
        EXPECT_EQ(KEY_PRESSED, Input->GetKeyFromBuffer(0, key, nullptr, TRUE));
        EXPECT_EQ(1, Input->GetNumberOfKeyInBuffer(TRUE));
        EXPECT_EQ(0, Input->GetNumberOfKeyInBuffer());
    }
}

TEST_F(SdlInputTest, BmlTokensCountRequestsAndOriginalReadersUseRawQueries) {
    InputHook hook(Input);
    ASSERT_TRUE(InputHookOwnsActive(hook));
    auto first = hook.AcquireBlock(InputHook::INPUT_BLOCK_KEYBOARD | InputHook::INPUT_BLOCK_MOUSE);
    auto second = hook.AcquireBlock(InputHook::INPUT_BLOCK_KEYBOARD);
    ASSERT_NE(0u, first); ASSERT_NE(0u, second);
    Key(true, 1234); Button(true); Frame();
    EXPECT_EQ(2, hook.IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    EXPECT_FALSE(hook.IsKeyDown(CKKEY_A));
    CKDWORD stamp = 0, key = 0;
    EXPECT_TRUE(hook.oIsKeyDown(CKKEY_A, &stamp));
    EXPECT_EQ(1234u, stamp);
    EXPECT_TRUE(hook.oIsKeyUp(CKKEY_B));
    EXPECT_EQ(KS_PRESSED, hook.oGetKeyboardState()[CKKEY_A]);
    EXPECT_EQ(1, hook.oGetNumberOfKeyInBuffer());
    EXPECT_EQ(KEY_PRESSED, hook.oGetKeyFromBuffer(0, key, &stamp));
    EXPECT_EQ(CKKEY_A, key); EXPECT_EQ(1234u, stamp);
    EXPECT_FALSE(hook.IsMouseClicked(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(hook.oIsMouseClicked(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(hook.oIsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    CKBYTE buttons[4]{}; hook.oGetMouseButtonsState(buttons);
    EXPECT_EQ(KS_PRESSED, buttons[0]);
    hook.ReleaseBlock(first);
    EXPECT_FALSE(hook.IsKeyDown(CKKEY_A));
    EXPECT_TRUE(hook.IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    hook.ReleaseBlock(first);
    EXPECT_EQ(1, hook.IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    hook.ReleaseBlock(second);
    EXPECT_TRUE(hook.IsKeyDown(CKKEY_A));
    EXPECT_EQ(0u, hook.AcquireBlock(0x80000000u));
}

TEST_F(SdlInputTest, BmlSnapshotDoesNotRunOrClearEnginePostProcess) {
    InputHook hook(Input);
    Key(true); Frame();
    EXPECT_TRUE(hook.oIsKeyPressed(CKKEY_A));
    hook.Process();
    EXPECT_EQ(0, Input->PostProcesses);
    EXPECT_FALSE(hook.oIsKeyPressed(CKKEY_A));
    Input->PostProcess();
    Key(false, 1500); Frame();
    EXPECT_TRUE(hook.oIsKeyReleased(CKKEY_A));
    hook.Process();
    EXPECT_EQ(1, Input->PostProcesses);
    EXPECT_TRUE(Input->IsKeyToggled(CKKEY_A, nullptr, TRUE));
    EXPECT_FALSE(hook.oIsKeyReleased(CKKEY_A));
    Input->PostProcess();
    Key(true, 2000); Frame();
    EXPECT_TRUE(hook.oIsKeyPressed(CKKEY_A));
    EXPECT_FALSE(hook.oIsKeyPressed(256));
    EXPECT_FALSE(hook.oIsKeyReleased(256));
}

TEST_F(SdlInputTest, DetachReleasesManualAndTokenBlocksAndAllowsReattachment) {
    {
        InputHook hook(Input);
        hook.Block(CK_INPUT_DEVICE_KEYBOARD);
        hook.AcquireBlock(InputHook::INPUT_BLOCK_ALL);
        InputHook duplicate(Input);
        EXPECT_FALSE(InputHookOwnsActive(duplicate));
        EXPECT_TRUE(DetachInputHook(duplicate));
        EXPECT_TRUE(InputHookOwnsActive(hook));
        Key(true); Button(true); Frame();
        EXPECT_FALSE(Input->IsKeyDown(CKKEY_A));
        EXPECT_TRUE(DetachInputHook(hook));
        EXPECT_FALSE(InputHookOwnsActive(hook));
        EXPECT_TRUE(Input->IsKeyDown(CKKEY_A));
        EXPECT_TRUE(Input->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    }
    InputHook next(Input);
    EXPECT_TRUE(InputHookOwnsActive(next));
    EXPECT_EQ(0, next.IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    next.Unblock(CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_EQ(0, next.IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
}
} // namespace
