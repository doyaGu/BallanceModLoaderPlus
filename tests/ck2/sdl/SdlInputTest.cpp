#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include <SDL3/SDL.h>
#include "CKAll.h"
#include "SdlInputManager.h"
#include "Hooks/InputHook.h"
#include "Hooks/HookLifecycle.h"
#include "Hooks/InputCursor.h"
#include "Hooks/InputVTable.h"
#include "Hooks/VTablePatch.h"

void CKInputManagerInitializeParameterTypes(CKContext *context);
CKERROR CreateKeyEventBehaviorProto(CKBehaviorPrototype **prototype);
int KeyEvent(const CKBehaviorContext &context);
CKERROR CreateGetMouseRelativePositionProto(CKBehaviorPrototype **prototype);
int GetMouseRelativePosition(const CKBehaviorContext &context);

namespace {
class CountingInputManager : public SdlInputManager {
public:
    using SdlInputManager::SdlInputManager;
    int PostProcesses = 0;
    int *CompletedPostProcesses = nullptr;
    CKERROR PostProcess() override {
        ++PostProcesses;
        if (CompletedPostProcesses)
            ++*CompletedPostProcesses;
        return SdlInputManager::PostProcess();
    }
};

class FrameStageManager : public CKBaseManager {
public:
    FrameStageManager(CKContext *context, CKGUID guid, int priority,
                      std::function<void()> action)
        : CKBaseManager(context, guid, "BML scheduling test"),
          Priority(priority), Action(std::move(action)) {
        context->RegisterNewManager(this);
    }
    CKDWORD GetValidFunctionsMask() override { return CKMANAGER_FUNC_PostProcess; }
    int GetFunctionPriority(CKMANAGER_FUNCTIONS) override { return Priority; }
    CKERROR PostProcess() override { Action(); return CK_OK; }
private:
    int Priority;
    std::function<void()> Action;
};

class SdlInputTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    CountingInputManager *Input = nullptr;
    SDL_Window *Window = nullptr;
    std::unique_ptr<InputHook> Hook;
    CKInputManager *Filtered = nullptr;

    void Attach() {
        Hook = std::make_unique<InputHook>(Input);
        ASSERT_TRUE(InputHookOwnsActive(*Hook));
        Filtered = static_cast<CKInputManager *>(Context->GetManagerByGuid(INPUT_MANAGER_GUID));
        ASSERT_EQ(Input, Filtered);
    }
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
        CKInputManagerInitializeParameterTypes(Context);
        ASSERT_EQ(CK_OK, Input->OnCKInit());
        ASSERT_TRUE(Input->SetInputViewport(Window, VxRect(0, 0, 640, 480), 640, 480));
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    }

    void TearDown() override {
        Hook.reset();
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

TEST_F(SdlInputTest, KeyboardBlockPreservesNativeStateStampsAndBuffer) {
    Attach();
    Hook->Block(CK_INPUT_DEVICE_KEYBOARD);
    Key(true, 1000); Frame();
    CKDWORD stamp = 77, key = 99;
    EXPECT_FALSE(Filtered->IsKeyDown(CKKEY_A, &stamp));
    EXPECT_FALSE(Filtered->IsKeyUp(CKKEY_B));
    EXPECT_FALSE(Filtered->IsKeyToggled(CKKEY_A, &stamp));
    EXPECT_EQ(77u, stamp);
    EXPECT_EQ(KS_IDLE, Filtered->GetKeyboardState()[CKKEY_A]);
    EXPECT_EQ(0, Filtered->GetNumberOfKeyInBuffer());
    EXPECT_EQ(NO_KEY, Filtered->GetKeyFromBuffer(0, key, &stamp));
    EXPECT_EQ(99u, key);
    EXPECT_EQ(77u, stamp);
    EXPECT_TRUE(Hook->oIsKeyDown(CKKEY_A, &stamp));
    EXPECT_EQ(1000u, stamp);
    EXPECT_TRUE(Hook->oIsKeyUp(CKKEY_B));
    EXPECT_EQ(KS_PRESSED, Hook->oGetKeyboardState()[CKKEY_A]);
    EXPECT_EQ(1, Hook->oGetNumberOfKeyInBuffer());
    EXPECT_EQ(KEY_PRESSED, Hook->oGetKeyFromBuffer(0, key, &stamp));
    EXPECT_EQ(CKKEY_A, key);
    EXPECT_EQ(1000u, stamp);
    Hook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_TRUE(Filtered->IsKeyDown(CKKEY_A));
    EXPECT_EQ(1, Filtered->GetNumberOfKeyInBuffer());
}

TEST_F(SdlInputTest, CountedBlocksAreIndependentForEachDevice) {
    Attach();
    Hook->Block(CK_INPUT_DEVICE_KEYBOARD);
    Hook->Block(CK_INPUT_DEVICE_KEYBOARD);
    Hook->Block(static_cast<CK_INPUT_DEVICE>(-1));
    Hook->Unblock(CK_INPUT_DEVICE_COUNT);
    Key(true); Button(true); Frame();
    EXPECT_EQ(2, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    EXPECT_EQ(0, Hook->IsBlocked(CK_INPUT_DEVICE_MOUSE));
    EXPECT_FALSE(Filtered->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(Filtered->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    Hook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_FALSE(Filtered->IsKeyDown(CKKEY_A));
    Hook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_TRUE(Filtered->IsKeyDown(CKKEY_A));
    Hook->Block(CK_INPUT_DEVICE_MOUSE);
    EXPECT_TRUE(Filtered->IsKeyDown(CKKEY_A));
    EXPECT_FALSE(Filtered->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Filtered->IsKeyboardAttached());
}

TEST_F(SdlInputTest, NativeBufferRetainsFastTapRepeatsAndReleaseStamp) {
    Attach();
    Input->EnableKeyboardRepetition(TRUE);
    Hook->Block(CK_INPUT_DEVICE_KEYBOARD);
    Key(true, 1000); Key(true, 1100, true); Key(false, 1250); Frame();
    ASSERT_EQ(3, Hook->oGetNumberOfKeyInBuffer());
    CKDWORD key = 0, stamp = 0;
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(i == 2 ? KEY_RELEASED : KEY_PRESSED, Hook->oGetKeyFromBuffer(i, key, &stamp));
        EXPECT_EQ(CKKEY_A, key);
        EXPECT_EQ((std::array<CKDWORD, 3>{1000, 1100, 1250})[i], stamp);
    }
    EXPECT_TRUE(Hook->oIsKeyToggled(CKKEY_A, &stamp));
    EXPECT_EQ(150u, stamp);
    key = 71; stamp = 72;
    EXPECT_EQ(NO_KEY, Hook->oGetKeyFromBuffer(-1, key, &stamp));
    EXPECT_EQ(NO_KEY, Hook->oGetKeyFromBuffer(3, key, &stamp));
    EXPECT_EQ(71u, key); EXPECT_EQ(72u, stamp);
    EXPECT_EQ(0, Filtered->GetNumberOfKeyInBuffer());
}

TEST_F(SdlInputTest, MouseBlockRetainsNativeMotionButtonsAndEdges) {
    Attach(); Hook->Block(CK_INPUT_DEVICE_MOUSE); Button(true);
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
    ASSERT_TRUE(SDL_PushEvent(&wheel)); Frame();
    EXPECT_FALSE(Filtered->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_FALSE(Filtered->IsMouseClicked(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Hook->oIsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Hook->oIsMouseClicked(CK_MOUSEBUTTON_LEFT));
    CKBYTE buttons[4] = {9, 9, 9, 9};
    Filtered->GetMouseButtonsState(buttons);
    for (auto state : buttons) EXPECT_EQ(KS_IDLE, state);
    Hook->oGetMouseButtonsState(buttons);
    EXPECT_EQ(KS_PRESSED, buttons[0]);
    std::array<CKBYTE, 6> guarded{42, 9, 9, 9, 9, 43};
    Hook->oGetMouseButtonsState(guarded.data() + 1);
    EXPECT_EQ(42, guarded.front()); EXPECT_EQ(43, guarded.back());
    EXPECT_EQ(KS_PRESSED, guarded[1]);
    Vx2DVector position(91, 92);
    Filtered->GetMousePosition(position, FALSE);
    EXPECT_EQ(91, position.x); EXPECT_EQ(92, position.y);
    Hook->Unblock(CK_INPUT_DEVICE_MOUSE);
    Input->GetMousePosition(position, FALSE);
    EXPECT_EQ(55, position.x); EXPECT_EQ(33, position.y);
    Hook->Block(CK_INPUT_DEVICE_MOUSE);
    VxVector movement(91, 92, 93);
    Filtered->GetMouseRelativePosition(movement);
    EXPECT_EQ(91, movement.x); EXPECT_EQ(93, movement.z);
    Hook->Unblock(CK_INPUT_DEVICE_MOUSE);
    Input->GetMouseRelativePosition(movement);
    EXPECT_EQ(7, movement.x); EXPECT_EQ(-3, movement.y); EXPECT_NE(0, movement.z);
    Hook->Block(CK_INPUT_DEVICE_MOUSE);
    Input->PostProcess(); Button(false); Frame();
    EXPECT_FALSE(Filtered->IsMouseToggled(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Hook->oIsMouseToggled(CK_MOUSEBUTTON_LEFT));
    Hook->Unblock(CK_INPUT_DEVICE_MOUSE);
    EXPECT_TRUE(Filtered->IsMouseToggled(CK_MOUSEBUTTON_LEFT));
}

TEST_F(SdlInputTest, OriginalReadsStillRespectPauseAndFocusLoss) {
    Attach(); Hook->Block(CK_INPUT_DEVICE_KEYBOARD);
    Key(true); Frame();
    ASSERT_TRUE(Hook->oIsKeyDown(CKKEY_A));
    Filtered->Pause(TRUE);
    EXPECT_EQ(1, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    EXPECT_FALSE(Hook->oIsKeyDown(CKKEY_A));
    EXPECT_EQ(0, Hook->oGetNumberOfKeyInBuffer());
    Key(true, 2000); Frame(); EXPECT_FALSE(Hook->oIsKeyDown(CKKEY_A));
    Filtered->Pause(FALSE);
    Key(true, 3000); Frame(); ASSERT_TRUE(Hook->oIsKeyDown(CKKEY_A));
    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    focus.window.windowID = SDL_GetWindowID(Window);
    ASSERT_TRUE(SDL_PushEvent(&focus)); Frame();
    EXPECT_FALSE(Hook->oIsKeyDown(CKKEY_A));
    EXPECT_EQ(0, Hook->oGetNumberOfKeyInBuffer());
    EXPECT_EQ(1, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    EXPECT_FALSE(Filtered->IsKeyUp(CKKEY_A));
}

TEST_F(SdlInputTest, EngineResetClearsNativeStateWithoutOwningBmlBlocks) {
    Attach(); Hook->Block(CK_INPUT_DEVICE_KEYBOARD); Key(true); Frame();
    ASSERT_EQ(CK_OK, Input->OnCKReset());
    EXPECT_EQ(1, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    EXPECT_EQ(KS_IDLE, Hook->oGetKeyboardState()[CKKEY_A]);
    Key(true, 2000); Frame();
    EXPECT_TRUE(Hook->oIsKeyDown(CKKEY_A));
    EXPECT_FALSE(Filtered->IsKeyDown(CKKEY_A));
}

TEST_F(SdlInputTest, JoystickQueriesAreInterceptedWithoutClearingNativeState) {
    Attach(); Gamepad(); ASSERT_FALSE(HasFatalFailure());
    VxVector rawPosition, rawRotation;
    Vx2DVector rawSliders;
    float rawAngle = 0;
    Input->GetJoystickPosition(0, &rawPosition);
    Input->GetJoystickRotation(0, &rawRotation);
    Input->GetJoystickSliders(0, &rawSliders);
    Input->GetJoystickPointOfViewAngle(0, &rawAngle);
    CKDWORD rawButtons = Input->GetJoystickButtonsState(0);
    ASSERT_GT(rawPosition.x, 0.1f); ASSERT_NE(0u, rawButtons);
    Hook->Block(CK_INPUT_DEVICE_JOYSTICK);
    VxVector position(9, 9, 9), rotation(8, 8, 8);
    Vx2DVector sliders(7, 7);
    float angle = 6;
    Filtered->GetJoystickPosition(0, &position);
    Filtered->GetJoystickRotation(0, &rotation);
    Filtered->GetJoystickSliders(0, &sliders);
    Filtered->GetJoystickPointOfViewAngle(0, &angle);
    EXPECT_EQ(9, position.x); EXPECT_EQ(8, rotation.x);
    EXPECT_EQ(7, sliders.x); EXPECT_EQ(6, angle);
    EXPECT_EQ(0u, Filtered->GetJoystickButtonsState(0));
    EXPECT_FALSE(Filtered->IsJoystickButtonDown(0, SDL_GAMEPAD_BUTTON_SOUTH));
    EXPECT_TRUE(Filtered->IsJoystickAttached(0));
    Hook->Unblock(CK_INPUT_DEVICE_JOYSTICK);
    EXPECT_EQ(rawButtons, Input->GetJoystickButtonsState(0));
    EXPECT_TRUE(Input->IsJoystickButtonDown(0, SDL_GAMEPAD_BUTTON_SOUTH));
    Filtered->Pause(TRUE);
    EXPECT_EQ(0u, Input->GetJoystickButtonsState(0));
    EXPECT_FALSE(Input->IsJoystickButtonDown(0, SDL_GAMEPAD_BUTTON_SOUTH));
    Input->GetJoystickPosition(0, &position);
    Input->GetJoystickRotation(0, &rotation);
    Input->GetJoystickSliders(0, &sliders);
    Input->GetJoystickPointOfViewAngle(0, &angle);
    EXPECT_EQ(9, position.x); EXPECT_EQ(8, rotation.x);
    EXPECT_EQ(7, sliders.x); EXPECT_EQ(6, angle);
    Filtered->Pause(FALSE);
    EXPECT_EQ(rawButtons, Filtered->GetJoystickButtonsState(0));
}

TEST_F(SdlInputTest, EngineLeavesApplicationEventsAndNativeBufferIntact) {
    Attach(); Key(true);
    SDL_Event custom{}; custom.type = SDL_EVENT_USER; custom.user.code = 42;
    ASSERT_TRUE(SDL_PushEvent(&custom)); Frame();
    SDL_Event peek{};
    EXPECT_EQ(1, SDL_PeepEvents(&peek, 1, SDL_PEEKEVENT, SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN));
    EXPECT_EQ(1, SDL_PeepEvents(&peek, 1, SDL_PEEKEVENT, SDL_EVENT_USER, SDL_EVENT_USER));
    EXPECT_EQ(42, peek.user.code);
    Hook->Block(CK_INPUT_DEVICE_KEYBOARD);
    for (int i = 0; i < 3; ++i) {
        CKDWORD key = 0;
        EXPECT_EQ(KEY_PRESSED, Hook->oGetKeyFromBuffer(0, key));
        EXPECT_EQ(1, Hook->oGetNumberOfKeyInBuffer());
        EXPECT_EQ(0, Filtered->GetNumberOfKeyInBuffer());
    }
}

TEST_F(SdlInputTest, TokensRetainExistingBmlSemantics) {
    Attach();
    auto first = Hook->AcquireBlock(InputHook::INPUT_BLOCK_KEYBOARD | InputHook::INPUT_BLOCK_MOUSE);
    auto second = Hook->AcquireBlock(InputHook::INPUT_BLOCK_KEYBOARD);
    Key(true, 1234); Button(true); Frame();
    EXPECT_EQ(2, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    EXPECT_FALSE(Hook->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(Hook->oIsKeyDown(CKKEY_A));
    Hook->ReleaseBlock(first);
    EXPECT_FALSE(Filtered->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(Filtered->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    Hook->ReleaseBlock(first);
    EXPECT_EQ(1, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    Hook->ReleaseBlock(second);
    EXPECT_TRUE(Filtered->IsKeyDown(CKKEY_A));
    EXPECT_EQ(0u, Hook->AcquireBlock(0x80000000u));
}

TEST_F(SdlInputTest, BmlSnapshotDoesNotRunOrClearEnginePostProcess) {
    Attach(); Key(true); Frame();
    EXPECT_TRUE(Hook->oIsKeyPressed(CKKEY_A));
    Hook->Process();
    EXPECT_EQ(0, Input->PostProcesses);
    EXPECT_FALSE(Hook->oIsKeyPressed(CKKEY_A));
    Input->PostProcess(); Key(false, 1500); Frame();
    EXPECT_TRUE(Hook->oIsKeyReleased(CKKEY_A));
    Hook->Process();
    EXPECT_EQ(1, Input->PostProcesses);
    EXPECT_TRUE(Input->IsKeyToggled(CKKEY_A));
    EXPECT_FALSE(Hook->oIsKeyReleased(CKKEY_A));
    Input->PostProcess(); Key(true, 2000); Frame();
    EXPECT_TRUE(Hook->oIsKeyPressed(CKKEY_A));
    EXPECT_FALSE(Hook->oIsKeyPressed(256)); EXPECT_FALSE(Hook->oIsKeyReleased(256));
}

TEST_F(SdlInputTest, DeferredCleanupPreservesKeyboardAndMouseReleaseEdges) {
    Attach();
    DeferInputPostProcess(*Hook, true);
    Key(true); Button(true); Frame();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    EXPECT_EQ(0, Input->PostProcesses);
    EXPECT_TRUE(Hook->oIsKeyPressed(CKKEY_A));
    EXPECT_TRUE(Hook->oIsMouseClicked(CK_MOUSEBUTTON_LEFT));
    Hook->Process();
    EXPECT_EQ(1, Input->PostProcesses);

    Key(false, 1500); Button(false); Frame();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    EXPECT_EQ(1, Input->PostProcesses);
    EXPECT_TRUE(Filtered->IsKeyToggled(CKKEY_A));
    EXPECT_TRUE(Hook->IsKeyReleased(CKKEY_A));
    EXPECT_TRUE(Hook->oIsKeyReleased(CKKEY_A));
    EXPECT_TRUE(Filtered->IsMouseToggled(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(Hook->oIsMouseToggled(CK_MOUSEBUTTON_LEFT));
    Hook->Process();
    EXPECT_EQ(2, Input->PostProcesses);
    EXPECT_FALSE(Filtered->IsKeyToggled(CKKEY_A));
    EXPECT_FALSE(Hook->oIsKeyReleased(CKKEY_A));
    EXPECT_FALSE(Filtered->IsMouseToggled(CK_MOUSEBUTTON_LEFT));

    Hook->Process();
    EXPECT_EQ(2, Input->PostProcesses);
    Frame();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    EXPECT_FALSE(Hook->oIsKeyReleased(CKKEY_A));
    Hook->Process();
    EXPECT_EQ(3, Input->PostProcesses);
}

TEST_F(SdlInputTest, LateModStageReadsInputAfterDefaultManagersAcrossProcessOnlyFrames) {
    Attach();
    auto *level = static_cast<CKLevel *>(Context->CreateObject(CKCID_LEVEL));
    ASSERT_NE(nullptr, level);
    Context->SetCurrentLevel(level);
    int defaultFrames = 0;
    std::vector<bool> releases;
    new FrameStageManager(Context, CKGUID(0x424d4c50, 1), 0, [&] {
        ++defaultFrames;
    });
    new FrameStageManager(Context, CKGUID(0x424d4c50, 2), -1, [&] {
        EXPECT_EQ(releases.size() + 1, static_cast<std::size_t>(defaultFrames));
        EXPECT_EQ(releases.size(), static_cast<std::size_t>(Input->PostProcesses));
        const bool released = Hook->oIsKeyReleased(CKKEY_A) != FALSE;
        releases.push_back(released);
        EXPECT_EQ(released, Hook->oIsMouseToggled(CK_MOUSEBUTTON_LEFT) != FALSE);
        if (Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
            EXPECT_FALSE(Filtered->IsKeyToggled(CKKEY_A));
            EXPECT_FALSE(Hook->IsKeyReleased(CKKEY_A));
        }
        Hook->Process();
    });
    ASSERT_EQ(CK_OK, Context->Play());
    DeferInputPostProcess(*Hook, true);
    Key(true); Button(true);
    ASSERT_EQ(CK_OK, Context->Process());
    Hook->AcquireBlock(InputHook::INPUT_BLOCK_KEYBOARD | InputHook::INPUT_BLOCK_MOUSE);
    Key(false); Button(false);
    ASSERT_EQ(CK_OK, Context->Process());
    ASSERT_EQ(CK_OK, Context->Process());
    EXPECT_EQ((std::vector<bool>{false, true, false}), releases);
    EXPECT_EQ(3, Input->PostProcesses);
    EXPECT_EQ(3, defaultFrames);

    ASSERT_EQ(CK_OK, Context->Pause());
    ASSERT_EQ(CK_OK, Context->Process());
    EXPECT_EQ(3, Input->PostProcesses);
    EXPECT_EQ(3, defaultFrames);
    DeferInputPostProcess(*Hook, false);
    EXPECT_EQ(3, Input->PostProcesses);
}

TEST_F(SdlInputTest, StoppingDeferralFlushesPendingCleanupAndRestoresEngineScheduling) {
    Attach();
    DeferInputPostProcess(*Hook, true);
    Key(true); Frame();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    Hook->Process();
    Key(false); Frame();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    ASSERT_TRUE(Hook->oIsKeyReleased(CKKEY_A));
    ASSERT_EQ(1, Input->PostProcesses);

    DeferInputPostProcess(*Hook, false);
    EXPECT_EQ(2, Input->PostProcesses);
    EXPECT_FALSE(Filtered->IsKeyToggled(CKKEY_A));
    DeferInputPostProcess(*Hook, false);
    Hook->Process();
    EXPECT_EQ(2, Input->PostProcesses);
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    EXPECT_EQ(3, Input->PostProcesses);

    DeferInputPostProcess(*Hook, true);
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    EXPECT_EQ(3, Input->PostProcesses);
    Hook->Process();
    EXPECT_EQ(4, Input->PostProcesses);
}

TEST_F(SdlInputTest, DetachCompletesPendingCleanupBeforeRestoringTheManager) {
    Attach();
    DeferInputPostProcess(*Hook, true);
    Key(false); Button(false); Frame();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    ASSERT_EQ(0, Input->PostProcesses);
    ASSERT_TRUE(Filtered->IsKeyToggled(CKKEY_A));
    ASSERT_TRUE(DetachInputHook(*Hook));
    EXPECT_EQ(1, Input->PostProcesses);
    EXPECT_FALSE(Input->IsKeyToggled(CKKEY_A));
    EXPECT_FALSE(Input->IsMouseToggled(CK_MOUSEBUTTON_LEFT));
    Hook.reset();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    EXPECT_EQ(2, Input->PostProcesses);

    Attach();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    EXPECT_EQ(3, Input->PostProcesses);
}

TEST_F(SdlInputTest, DeferralDoesNotChangeAnotherManagerSharingTheVTable) {
    auto other = std::make_unique<CountingInputManager>(Context);
    Attach();
    DeferInputPostProcess(*Hook, true);
    CKInputManager *foreign = other.get();
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    ASSERT_EQ(CK_OK, foreign->PostProcess());
    EXPECT_EQ(0, Input->PostProcesses);
    EXPECT_EQ(1, other->PostProcesses);
    Hook->Process();
    EXPECT_EQ(1, Input->PostProcesses);
    EXPECT_EQ(1, other->PostProcesses);
}

TEST_F(SdlInputTest, ContextEndFlushesPendingInputBeforeDeletingTheManager) {
    Attach();
    int completed = 0;
    Input->CompletedPostProcesses = &completed;
    DeferInputPostProcess(*Hook, true);
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    ASSERT_EQ(0, completed);
    CKCloseContext(Context);
    Context = nullptr;
    Input = nullptr;
    EXPECT_EQ(1, completed);
    EXPECT_FALSE(InputHookOwnsActive(*Hook));
    Hook.reset();
}

TEST_F(SdlInputTest, DetachClearsBmlBlocksAndAllowsReattachment) {
    Attach();
    Hook->Block(CK_INPUT_DEVICE_KEYBOARD); Hook->AcquireBlock(InputHook::INPUT_BLOCK_ALL);
    InputHook duplicate(Input);
    EXPECT_FALSE(InputHookOwnsActive(duplicate));
    EXPECT_TRUE(DetachInputHook(duplicate));
    Key(true); Button(true); Frame();
    EXPECT_FALSE(Hook->IsKeyDown(CKKEY_A));
    EXPECT_FALSE(Input->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(DetachInputHook(*Hook));
    EXPECT_TRUE(Input->IsKeyDown(CKKEY_A));
    EXPECT_FALSE(InputHookOwnsActive(*Hook));
    EXPECT_EQ(0, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    EXPECT_FALSE(Hook->oIsKeyDown(CKKEY_A));
    Hook.reset();
    Attach();
    EXPECT_TRUE(Hook->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(Hook->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_EQ(0, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
    Hook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_EQ(0, Hook->IsBlocked(CK_INPUT_DEVICE_KEYBOARD));
}
TEST_F(SdlInputTest, AnotherInstanceSharingTheVTableKeepsItsOwnInput) {
    auto other = std::make_unique<CountingInputManager>(Context);
    ASSERT_TRUE(other->SetInputViewport(Window, VxRect(0, 0, 640, 480), 640, 480));
    Attach(); Hook->AcquireBlock(InputHook::INPUT_BLOCK_ALL);
    Key(true); Button(true); Frame();
    ASSERT_EQ(CK_OK, other->PreProcess());
    CKInputManager *foreign = other.get();
    EXPECT_FALSE(Filtered->IsKeyDown(CKKEY_A));
    EXPECT_FALSE(Filtered->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_TRUE(foreign->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(foreign->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT));
    EXPECT_EQ(Input, dynamic_cast<CountingInputManager *>(Filtered));
    EXPECT_EQ(other.get(), dynamic_cast<CountingInputManager *>(foreign));
}

TEST_F(SdlInputTest, NativeCursorRequestsRespectTheOverlayUntilItReleasesTheCursor) {
    Attach(); Filtered->ShowCursor(FALSE);
    EXPECT_FALSE(SDL_CursorVisible());
    SetOverlayCursorVisible(true);
    EXPECT_TRUE(SDL_CursorVisible());
    Filtered->ShowCursor(FALSE);
    EXPECT_FALSE(Filtered->GetCursorVisibility());
    EXPECT_TRUE(SDL_CursorVisible());
    SetOverlayCursorVisible(false);
    EXPECT_FALSE(SDL_CursorVisible());
    SetOverlayCursorVisible(true);
    Filtered->ShowCursor(TRUE);
    Hook.reset();
    EXPECT_TRUE(SDL_CursorVisible());
    EXPECT_TRUE(Input->GetCursorVisibility());
}

struct ForeignInputHook {
    CKBOOL IsKeyDown(CKDWORD, CKDWORD *) { return 123; }
};

TEST_F(SdlInputTest, OwnershipLossDefersDetachAndPreservesTheLaterHook) {
    Attach(); Hook->Block(CK_INPUT_DEVICE_KEYBOARD); Key(true); Frame();
    DeferInputPostProcess(*Hook, true);
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    ASSERT_EQ(0, Input->PostProcesses);
    VTablePatch later;
    const VTablePatch::Request request{BML_INPUT_SLOT(IsKeyDown),
        InputVTable::Address(&ForeignInputHook::IsKeyDown)};
    ASSERT_TRUE(later.Install(Input, &request, 1));
    EXPECT_EQ(123, Filtered->IsKeyDown(CKKEY_A));
    EXPECT_FALSE(DetachInputHook(*Hook));
    EXPECT_EQ(1, Input->PostProcesses);
    EXPECT_TRUE(InputHookOwnsActive(*Hook));
    EXPECT_TRUE(Hook->oIsKeyDown(CKKEY_A));
    EXPECT_EQ(123, Filtered->IsKeyDown(CKKEY_A));
    ASSERT_EQ(CK_OK, Filtered->PostProcess());
    Hook->Process();
    EXPECT_EQ(2, Input->PostProcesses);
    EXPECT_TRUE(later.Remove());
    EXPECT_TRUE(DetachInputHook(*Hook));
    EXPECT_TRUE(Filtered->IsKeyDown(CKKEY_A));
}

TEST_F(SdlInputTest, ContextEndRestoresTheVTableBeforeDeletingTheInputManager) {
    Attach(); Hook->AcquireBlock(InputHook::INPUT_BLOCK_ALL);
    ASSERT_EQ(CK_OK, CKCloseContext(Context));
    Context = nullptr; Input = nullptr; Filtered = nullptr;
    EXPECT_FALSE(InputHookOwnsActive(*Hook));
    EXPECT_FALSE(Hook->oIsKeyDown(CKKEY_A));
    Hook.reset();
}
TEST_F(SdlInputTest, RealKeyEventProcessesReleaseAndOffWhileKeyboardIsBlocked) {
    Attach();
    CKBehaviorPrototype *raw = nullptr;
    ASSERT_EQ(CK_OK, CreateKeyEventBehaviorProto(&raw));
    std::unique_ptr<CKBehaviorPrototype> proto(raw);
    auto *behavior = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    ASSERT_EQ(CK_OK, behavior->InitFromPrototype(proto.get()));
    CKDWORD key = CKKEY_A;
    auto *parameter = Context->CreateCKParameterLocal("key", CKPGUID_KEY);
    ASSERT_NE(nullptr, parameter);
    parameter->SetValue(&key);
    behavior->GetInputParameter(0)->SetDirectSource(parameter);
    CKBehaviorContext frame;
    frame.Context = Context; frame.Behavior = behavior;
    behavior->ActivateInput(0, TRUE);
    Key(true); Frame();
    EXPECT_EQ(CKBR_ACTIVATENEXTFRAME, KeyEvent(frame));
    CKBOOL state = FALSE; behavior->GetOutputParameterValue(0, &state); EXPECT_TRUE(state);
    Hook->Block(CK_INPUT_DEVICE_KEYBOARD);
    EXPECT_EQ(CKBR_ACTIVATENEXTFRAME, KeyEvent(frame));
    behavior->GetOutputParameterValue(0, &state); EXPECT_FALSE(state);
    EXPECT_TRUE(behavior->IsOutputActive(1));
    EXPECT_FALSE(Filtered->IsKeyDown(CKKEY_A));
    EXPECT_TRUE(Hook->oIsKeyDown(CKKEY_A));
    behavior->ActivateInput(1, TRUE);
    EXPECT_EQ(CKBR_OK, KeyEvent(frame));
    EXPECT_FALSE(behavior->IsInputActive(1));
}

TEST_F(SdlInputTest, RealMouseMovementBlockKeepsBehaviorExecutionAndOutputs) {
    Attach();
    CKBehaviorPrototype *raw = nullptr;
    ASSERT_EQ(CK_OK, CreateGetMouseRelativePositionProto(&raw));
    std::unique_ptr<CKBehaviorPrototype> proto(raw);
    auto *behavior = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    ASSERT_EQ(CK_OK, behavior->InitFromPrototype(proto.get()));
    CKBehaviorContext frame;
    frame.Context = Context; frame.Behavior = behavior;
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(Window);
    motion.motion.xrel = 7; motion.motion.yrel = -3;
    ASSERT_TRUE(SDL_PushEvent(&motion)); Frame();
    EXPECT_EQ(CKBR_OK, GetMouseRelativePosition(frame));
    int x = 0; behavior->GetOutputParameterValue(0, &x); EXPECT_EQ(7, x);
    Hook->Block(CK_INPUT_DEVICE_MOUSE); behavior->ActivateInput(0, TRUE);
    EXPECT_EQ(CKBR_OK, GetMouseRelativePosition(frame));
    behavior->GetOutputParameterValue(0, &x); EXPECT_EQ(0, x);
    EXPECT_FALSE(behavior->IsInputActive(0)); EXPECT_TRUE(behavior->IsOutputActive(0));
}
TEST_F(SdlInputTest, ContextEndWithAnOwnershipConflictInvalidatesTheDeletedInstance) {
    Attach(); Hook->AcquireBlock(InputHook::INPUT_BLOCK_ALL);
    SetOverlayCursorVisible(true);
    VTablePatch later;
    const VTablePatch::Request request{BML_INPUT_SLOT(IsKeyDown),
        InputVTable::Address(&ForeignInputHook::IsKeyDown)};
    ASSERT_TRUE(later.Install(Input, &request, 1));
    ASSERT_EQ(CK_OK, CKCloseContext(Context));
    Context = nullptr; Input = nullptr; Filtered = nullptr;
    EXPECT_FALSE(InputHookOwnsActive(*Hook));
    EXPECT_FALSE(Hook->oIsKeyDown(CKKEY_A));
    EXPECT_TRUE(later.Remove());
    EXPECT_TRUE(DetachInputHook(*Hook));
    Hook.reset();
}
} // namespace
