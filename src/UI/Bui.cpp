#include "BML/Bui.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_set>

#ifndef BML_UI_AUTOMATION_TEST
#include "CKGroup.h"
#include "CKMaterial.h"
#include "CKMessageManager.h"
#include "CKPathManager.h"
#include "CKTexture.h"
#endif

#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include "UI/BuiInternal.h"
#ifndef BML_UI_AUTOMATION_TEST
#include "BML/InputHook.h"
#include "Loader/ModContext.h"
#include "UI/Overlay.h"
#endif

namespace Bui {
    enum TextureType {
        TEXTURE_BUTTON_DESELECT,
        TEXTURE_BUTTON_SELECT,
        TEXTURE_BUTTON_SPECIAL,
        TEXTURE_FONT,

        TEXTURE_COUNT
    };

    enum MaterialType {
        MATERIAL_BUTTON_UP,
        MATERIAL_BUTTON_OVER,
        MATERIAL_BUTTON_INACTIVE,
        MATERIAL_KEYS_HIGHLIGHT,

        MATERIAL_COUNT
    };

    static const ImRect ButtonUvs[BUTTON_COUNT] = {
        {ImVec2(0.0f, 0.51372f), ImVec2(1.0f, 0.7451f)},
        {ImVec2(0.2392f, 0.75294f), ImVec2(0.8666f, 0.98431f)},
        {ImVec2(0.0f, 0.0f), ImVec2(1.0f, 0.24706f)},
        {ImVec2(0.0f, 0.247f), ImVec2(0.643f, 0.36863f)},
        {ImVec2(0.0f, 0.40785f), ImVec2(1.0f, 0.51f)},
        {ImVec2(0.0f, 0.82353f), ImVec2(0.226f, 0.9098f)},
        {ImVec2(0.6392f, 0.24706f), ImVec2(0.78823f, 0.40392f)},
        {ImVec2(0.7921f, 0.24706f), ImVec2(0.9412f, 0.40392f)},
        {ImVec2(0.88627f, 0.8902f), ImVec2(0.96863f, 0.97255f)},
        {ImVec2(0.88627f, 0.77804f), ImVec2(0.96863f, 0.8594f)},
    };

    static const ImVec2 ButtonSizes[BUTTON_COUNT] = {
        {0.3000f, 0.0938f},
        {0.1875f, 0.0938f},
        {0.3000f, 0.1000f},
        {0.1938f, 0.0500f},
        {0.3000f, 0.0396f},
        {0.0700f, 0.0354f},
        {0.0363f, 0.0517f},
        {0.0363f, 0.0517f},
        {0.0200f, 0.0267f},
        {0.0200f, 0.0267f},
    };

    static constexpr float ButtonIndents[BUTTON_COUNT] = {
        0.055f,
        0.049f,
        0.055f,
        0.037f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
    };

    struct ResourceState {
        std::array<CKTexture *, TEXTURE_COUNT> textures{};
        std::array<CKMaterial *, MATERIAL_COUNT> materials{};
        CKGroup *sounds = nullptr;
        CKMessageManager *messageManager = nullptr;
        CKMessageType menuClickMessage = -1;
    };

    static ResourceState Resources;

#ifndef BML_UI_AUTOMATION_TEST
    struct KeyboardInputBlockState {
        std::uint64_t token = 0;
        unsigned int anonymousUsers = 0;
        std::unordered_set<const void *> owners;
        std::unordered_set<std::uint64_t> pendingReleases;

        bool HasUsers() const {
            return anonymousUsers != 0 || !owners.empty();
        }
    };

    static KeyboardInputBlockState KeyboardInputBlocks;

    static bool AcquireKeyboardInputBlock() {
        if (KeyboardInputBlocks.token != 0)
            return true;

        ModContext *context = BML_GetModContext();
        if (!context)
            return false;
        if (InputHook *input = context->GetInputManager())
            KeyboardInputBlocks.token = input->AcquireBlock(InputHook::INPUT_BLOCK_KEYBOARD);
        return KeyboardInputBlocks.token != 0;
    }

    static void ReleaseKeyboardInputBlockAfterKeysUp() {
        if (KeyboardInputBlocks.HasUsers())
            return;

        ModContext *context = BML_GetModContext();
        if (!context)
            return;

        const std::uint64_t token = KeyboardInputBlocks.token;
        KeyboardInputBlocks.token = 0;
        if (token == 0)
            return;

        KeyboardInputBlocks.pendingReleases.insert(token);
        context->AddTimerLoop(1ul, [context, token] {
            if (!KeyboardInputBlocks.pendingReleases.contains(token))
                return false;

            InputHook *input = context->GetInputManager();
            if (!input)
                return false;
            if (input->oIsKeyDown(CKKEY_ESCAPE) || input->oIsKeyDown(CKKEY_RETURN))
                return true;

            input->ReleaseBlock(token);
            KeyboardInputBlocks.pendingReleases.erase(token);
            return false;
        });
    }

    static void ResetKeyboardInputBlocks() {
        if (ModContext *context = BML_GetModContext()) {
            if (InputHook *input = context->GetInputManager()) {
                if (KeyboardInputBlocks.token != 0)
                    input->ReleaseBlock(KeyboardInputBlocks.token);
                for (std::uint64_t token : KeyboardInputBlocks.pendingReleases)
                    input->ReleaseBlock(token);
            }
        }

        KeyboardInputBlocks = {};
    }
#endif

    ImGuiContext *GetImGuiContext() {
#ifdef BML_UI_AUTOMATION_TEST
        return ImGui::GetCurrentContext();
#else
        return Overlay::GetImGuiContext();
#endif
    }

#ifndef BML_UI_AUTOMATION_TEST
    CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot) {
        if (!context || !id || !filename)
            return nullptr;

        XString path((CKSTRING) filename);
        if (path.Length() == 0)
            return nullptr;

        CKPathManager *pathManager = context->GetPathManager();
        if (!pathManager || pathManager->ResolveFileName(path, BITMAP_PATH_IDX) != CK_OK)
            return nullptr;

        auto *texture = static_cast<CKTexture *>(context->CreateObject(CKCID_TEXTURE, (CKSTRING) id));
        if (!texture)
            return nullptr;

        if (!texture->LoadImage(path.Str(), slot)) {
            context->DestroyObject(texture);
            return nullptr;
        }

        texture->SetDesiredVideoFormat(_32_ARGB8888);
        return texture;
    }

    template <typename Object, std::size_t Size>
    static bool AllObjectsReady(const std::array<Object *, Size> &objects) {
        for (const Object *object : objects) {
            if (!object)
                return false;
        }
        return true;
    }

    template <typename Object, std::size_t Size>
    static void DestroyObjects(CKContext *context, std::array<Object *, Size> &objects) {
        if (context) {
            for (Object *object : objects) {
                if (object)
                    context->DestroyObject(object);
            }
        }
        objects = {};
    }

    static CKMaterial *CreateButtonMaterial(CKContext *context, const char *name, CKTexture *texture,
                                            const VxColor &diffuse, VXBLEND_MODE sourceBlend,
                                            VXBLEND_MODE destinationBlend, VXTEXTURE_ADDRESSMODE addressMode,
                                            bool enablePerspectiveCorrection) {
        auto *material = static_cast<CKMaterial *>(context->CreateObject(CKCID_MATERIAL, (CKSTRING) name));
        if (!material)
            return nullptr;

        material->SetAmbient(VxColor(76, 76, 76, 255));
        material->SetDiffuse(diffuse);
        material->SetSpecular(VxColor(127, 127, 127, 255));
        material->SetPower(0.0f);
        material->SetEmissive(VxColor(255, 255, 255, 0));
        material->SetFillMode(VXFILL_SOLID);
        material->SetShadeMode(VXSHADE_GOURAUD);
        material->EnableZWrite(FALSE);
        material->EnableAlphaBlend();
        material->SetSourceBlend(sourceBlend);
        material->SetDestBlend(destinationBlend);
        material->SetTexture0(texture);
        material->SetTextureBlendMode(VXTEXTUREBLEND_MODULATEALPHA);
        material->SetTextureMinMode(VXTEXTUREFILTER_LINEAR);
        material->SetTextureMagMode(VXTEXTUREFILTER_LINEAR);
        material->SetTextureAddressMode(addressMode);
        if (enablePerspectiveCorrection)
            material->EnablePerspectiveCorrection(TRUE);
        return material;
    }

    bool InitTextures(CKContext *context) {
        if (!context)
            return false;
        if (AllObjectsReady(Resources.textures))
            return true;

        // Materials borrow these textures. Rebuilding a partial texture set
        // invalidates the whole dependent material set as one unit.
        DestroyObjects(context, Resources.materials);
        DestroyObjects(context, Resources.textures);
        std::array<CKTexture *, TEXTURE_COUNT> textures{};
        textures[TEXTURE_BUTTON_DESELECT] = LoadTexture(context, "TEX_Button_Deselect", "Button01_deselect.tga");
        textures[TEXTURE_BUTTON_SELECT] = LoadTexture(context, "TEX_Button_Select", "Button01_select.tga");
        textures[TEXTURE_BUTTON_SPECIAL] = LoadTexture(context, "TEX_Button_Special", "Button01_special.tga");
        textures[TEXTURE_FONT] = LoadTexture(context, "TEX_Font_1", "Font_1.tga");

        if (!AllObjectsReady(textures)) {
            DestroyObjects(context, textures);
            return false;
        }

        Resources.textures = textures;
        return true;
    }

    bool InitMaterials(CKContext *context) {
        if (!context || !AllObjectsReady(Resources.textures))
            return false;
        if (AllObjectsReady(Resources.materials))
            return true;

        DestroyObjects(context, Resources.materials);
        std::array<CKMaterial *, MATERIAL_COUNT> materials{};
        materials[MATERIAL_BUTTON_UP] = CreateButtonMaterial(
            context, "MAT_Button_Up", Resources.textures[TEXTURE_BUTTON_DESELECT],
            VxColor(255, 255, 255, 255), VXBLEND_SRCALPHA, VXBLEND_INVSRCALPHA,
            VXTEXTURE_ADDRESSCLAMP, FALSE);
        materials[MATERIAL_BUTTON_OVER] = CreateButtonMaterial(
            context, "MAT_Button_Over", Resources.textures[TEXTURE_BUTTON_SELECT],
            VxColor(255, 255, 255, 255), VXBLEND_SRCALPHA, VXBLEND_INVSRCALPHA,
            VXTEXTURE_ADDRESSCLAMP, FALSE);
        materials[MATERIAL_BUTTON_INACTIVE] = CreateButtonMaterial(
            context, "MAT_Button_Inactive", Resources.textures[TEXTURE_BUTTON_SPECIAL],
            VxColor(255, 255, 255, 255), VXBLEND_SRCALPHA, VXBLEND_INVSRCALPHA,
            VXTEXTURE_ADDRESSCLAMP, TRUE);
        materials[MATERIAL_KEYS_HIGHLIGHT] = CreateButtonMaterial(
            context, "MAT_Keys_Highlight", Resources.textures[TEXTURE_BUTTON_SPECIAL],
            VxColor(209, 209, 209, 255), VXBLEND_ONE, VXBLEND_ONE,
            VXTEXTURE_ADDRESSWRAP, TRUE);

        if (!AllObjectsReady(materials)) {
            DestroyObjects(context, materials);
            return false;
        }

        Resources.materials = materials;
        return true;
    }

    bool InitSounds(CKContext *context) {
        if (!context)
            return false;

        Resources.sounds = nullptr;
        Resources.messageManager = nullptr;
        Resources.menuClickMessage = -1;

        CKMessageManager *messageManager = context->GetMessageManager();
        if (!messageManager)
            return false;

        const CKMessageType menuClickMessage = messageManager->AddMessageType((CKSTRING) "Menu_Click");
        if (menuClickMessage == -1)
            return false;

        auto *sounds = static_cast<CKGroup *>(
            context->GetObjectByNameAndClass((CKSTRING) "All_Sound", CKCID_GROUP));
        if (!sounds)
            return false;

        Resources.messageManager = messageManager;
        Resources.menuClickMessage = menuClickMessage;
        Resources.sounds = sounds;
        return true;
    }

    void CleanupResources(CKContext *context) {
        ResetKeyboardInputBlocks();
        DestroyObjects(context, Resources.materials);
        DestroyObjects(context, Resources.textures);
        Resources.sounds = nullptr;
        Resources.messageManager = nullptr;
        Resources.menuClickMessage = -1;
    }
#endif

    ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key) {
        switch (key) {
            case CKKEY_TAB: return ImGuiKey_Tab;
            case CKKEY_LEFT: return ImGuiKey_LeftArrow;
            case CKKEY_RIGHT: return ImGuiKey_RightArrow;
            case CKKEY_UP: return ImGuiKey_UpArrow;
            case CKKEY_DOWN: return ImGuiKey_DownArrow;
            case CKKEY_PRIOR: return ImGuiKey_PageUp;
            case CKKEY_NEXT: return ImGuiKey_PageDown;
            case CKKEY_HOME: return ImGuiKey_Home;
            case CKKEY_END: return ImGuiKey_End;
            case CKKEY_INSERT: return ImGuiKey_Insert;
            case CKKEY_DELETE: return ImGuiKey_Delete;
            case CKKEY_BACK: return ImGuiKey_Backspace;
            case CKKEY_SPACE: return ImGuiKey_Space;
            case CKKEY_RETURN: return ImGuiKey_Enter;
            case CKKEY_ESCAPE: return ImGuiKey_Escape;
            case CKKEY_APOSTROPHE: return ImGuiKey_Apostrophe;
            case CKKEY_COMMA: return ImGuiKey_Comma;
            case CKKEY_MINUS: return ImGuiKey_Minus;
            case CKKEY_PERIOD: return ImGuiKey_Period;
            case CKKEY_SLASH: return ImGuiKey_Slash;
            case CKKEY_SEMICOLON: return ImGuiKey_Semicolon;
            case CKKEY_EQUALS: return ImGuiKey_Equal;
            case CKKEY_LBRACKET: return ImGuiKey_LeftBracket;
            case CKKEY_BACKSLASH: return ImGuiKey_Backslash;
            case CKKEY_RBRACKET: return ImGuiKey_RightBracket;
            case CKKEY_GRAVE: return ImGuiKey_GraveAccent;
            case CKKEY_CAPITAL: return ImGuiKey_CapsLock;
            case CKKEY_SCROLL: return ImGuiKey_ScrollLock;
            case CKKEY_NUMLOCK: return ImGuiKey_NumLock;
            case CKKEY_NUMPAD0: return ImGuiKey_Keypad0;
            case CKKEY_NUMPAD1: return ImGuiKey_Keypad1;
            case CKKEY_NUMPAD2: return ImGuiKey_Keypad2;
            case CKKEY_NUMPAD3: return ImGuiKey_Keypad3;
            case CKKEY_NUMPAD4: return ImGuiKey_Keypad4;
            case CKKEY_NUMPAD5: return ImGuiKey_Keypad5;
            case CKKEY_NUMPAD6: return ImGuiKey_Keypad6;
            case CKKEY_NUMPAD7: return ImGuiKey_Keypad7;
            case CKKEY_NUMPAD8: return ImGuiKey_Keypad8;
            case CKKEY_NUMPAD9: return ImGuiKey_Keypad9;
            case CKKEY_DECIMAL: return ImGuiKey_KeypadDecimal;
            case CKKEY_DIVIDE: return ImGuiKey_KeypadDivide;
            case CKKEY_MULTIPLY: return ImGuiKey_KeypadMultiply;
            case CKKEY_SUBTRACT: return ImGuiKey_KeypadSubtract;
            case CKKEY_ADD: return ImGuiKey_KeypadAdd;
            case CKKEY_NUMPADENTER: return ImGuiKey_KeypadEnter;
            case CKKEY_NUMPADEQUALS: return ImGuiKey_KeypadEqual;
            case CKKEY_LCONTROL: return ImGuiKey_LeftCtrl;
            case CKKEY_LSHIFT: return ImGuiKey_LeftShift;
            case CKKEY_LMENU: return ImGuiKey_LeftAlt;
            case CKKEY_LWIN: return ImGuiKey_LeftSuper;
            case CKKEY_RCONTROL: return ImGuiKey_RightCtrl;
            case CKKEY_RSHIFT: return ImGuiKey_RightShift;
            case CKKEY_RMENU: return ImGuiKey_RightAlt;
            case CKKEY_RWIN: return ImGuiKey_RightSuper;
            case CKKEY_APPS: return ImGuiKey_Menu;
            case CKKEY_0: return ImGuiKey_0;
            case CKKEY_1: return ImGuiKey_1;
            case CKKEY_2: return ImGuiKey_2;
            case CKKEY_3: return ImGuiKey_3;
            case CKKEY_4: return ImGuiKey_4;
            case CKKEY_5: return ImGuiKey_5;
            case CKKEY_6: return ImGuiKey_6;
            case CKKEY_7: return ImGuiKey_7;
            case CKKEY_8: return ImGuiKey_8;
            case CKKEY_9: return ImGuiKey_9;
            case CKKEY_A: return ImGuiKey_A;
            case CKKEY_B: return ImGuiKey_B;
            case CKKEY_C: return ImGuiKey_C;
            case CKKEY_D: return ImGuiKey_D;
            case CKKEY_E: return ImGuiKey_E;
            case CKKEY_F: return ImGuiKey_F;
            case CKKEY_G: return ImGuiKey_G;
            case CKKEY_H: return ImGuiKey_H;
            case CKKEY_I: return ImGuiKey_I;
            case CKKEY_J: return ImGuiKey_J;
            case CKKEY_K: return ImGuiKey_K;
            case CKKEY_L: return ImGuiKey_L;
            case CKKEY_M: return ImGuiKey_M;
            case CKKEY_N: return ImGuiKey_N;
            case CKKEY_O: return ImGuiKey_O;
            case CKKEY_P: return ImGuiKey_P;
            case CKKEY_Q: return ImGuiKey_Q;
            case CKKEY_R: return ImGuiKey_R;
            case CKKEY_S: return ImGuiKey_S;
            case CKKEY_T: return ImGuiKey_T;
            case CKKEY_U: return ImGuiKey_U;
            case CKKEY_V: return ImGuiKey_V;
            case CKKEY_W: return ImGuiKey_W;
            case CKKEY_X: return ImGuiKey_X;
            case CKKEY_Y: return ImGuiKey_Y;
            case CKKEY_Z: return ImGuiKey_Z;
            case CKKEY_F1: return ImGuiKey_F1;
            case CKKEY_F2: return ImGuiKey_F2;
            case CKKEY_F3: return ImGuiKey_F3;
            case CKKEY_F4: return ImGuiKey_F4;
            case CKKEY_F5: return ImGuiKey_F5;
            case CKKEY_F6: return ImGuiKey_F6;
            case CKKEY_F7: return ImGuiKey_F7;
            case CKKEY_F8: return ImGuiKey_F8;
            case CKKEY_F9: return ImGuiKey_F9;
            case CKKEY_F10: return ImGuiKey_F10;
            case CKKEY_F11: return ImGuiKey_F11;
            case CKKEY_F12: return ImGuiKey_F12;
            default: return ImGuiKey_None;
        }
    }

    CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key) {
        switch (key) {
            case ImGuiKey_Tab: return CKKEY_TAB;
            case ImGuiKey_LeftArrow: return CKKEY_LEFT;
            case ImGuiKey_RightArrow: return CKKEY_RIGHT;
            case ImGuiKey_UpArrow: return CKKEY_UP;
            case ImGuiKey_DownArrow: return CKKEY_DOWN;
            case ImGuiKey_PageUp: return CKKEY_PRIOR;
            case ImGuiKey_PageDown: return CKKEY_NEXT;
            case ImGuiKey_Home: return CKKEY_HOME;
            case ImGuiKey_End: return CKKEY_END;
            case ImGuiKey_Insert: return CKKEY_INSERT;
            case ImGuiKey_Delete: return CKKEY_DELETE;
            case ImGuiKey_Backspace: return CKKEY_BACK;
            case ImGuiKey_Space: return CKKEY_SPACE;
            case ImGuiKey_Enter: return CKKEY_RETURN;
            case ImGuiKey_Escape: return CKKEY_ESCAPE;
            case ImGuiKey_Apostrophe: return CKKEY_APOSTROPHE;
            case ImGuiKey_Comma: return CKKEY_COMMA;
            case ImGuiKey_Minus: return CKKEY_MINUS;
            case ImGuiKey_Period: return CKKEY_PERIOD;
            case ImGuiKey_Slash: return CKKEY_SLASH;
            case ImGuiKey_Semicolon: return CKKEY_SEMICOLON;
            case ImGuiKey_Equal: return CKKEY_EQUALS;
            case ImGuiKey_LeftBracket: return CKKEY_LBRACKET;
            case ImGuiKey_Backslash: return CKKEY_BACKSLASH;
            case ImGuiKey_RightBracket: return CKKEY_RBRACKET;
            case ImGuiKey_GraveAccent: return CKKEY_GRAVE;
            case ImGuiKey_CapsLock: return CKKEY_CAPITAL;
            case ImGuiKey_ScrollLock: return CKKEY_SCROLL;
            case ImGuiKey_NumLock: return CKKEY_NUMLOCK;
            case ImGuiKey_Keypad0: return CKKEY_NUMPAD0;
            case ImGuiKey_Keypad1: return CKKEY_NUMPAD1;
            case ImGuiKey_Keypad2: return CKKEY_NUMPAD2;
            case ImGuiKey_Keypad3: return CKKEY_NUMPAD3;
            case ImGuiKey_Keypad4: return CKKEY_NUMPAD4;
            case ImGuiKey_Keypad5: return CKKEY_NUMPAD5;
            case ImGuiKey_Keypad6: return CKKEY_NUMPAD6;
            case ImGuiKey_Keypad7: return CKKEY_NUMPAD7;
            case ImGuiKey_Keypad8: return CKKEY_NUMPAD8;
            case ImGuiKey_Keypad9: return CKKEY_NUMPAD9;
            case ImGuiKey_KeypadDecimal: return CKKEY_DECIMAL;
            case ImGuiKey_KeypadDivide: return CKKEY_DIVIDE;
            case ImGuiKey_KeypadMultiply: return CKKEY_MULTIPLY;
            case ImGuiKey_KeypadSubtract: return CKKEY_SUBTRACT;
            case ImGuiKey_KeypadAdd: return CKKEY_ADD;
            case ImGuiKey_KeypadEnter: return CKKEY_NUMPADENTER;
            case ImGuiKey_KeypadEqual: return CKKEY_NUMPADEQUALS;
            case ImGuiKey_LeftCtrl: return CKKEY_LCONTROL;
            case ImGuiKey_LeftShift: return CKKEY_LSHIFT;
            case ImGuiKey_LeftAlt: return CKKEY_LMENU;
            case ImGuiKey_LeftSuper: return CKKEY_LWIN;
            case ImGuiKey_RightCtrl: return CKKEY_RCONTROL;
            case ImGuiKey_RightShift: return CKKEY_RSHIFT;
            case ImGuiKey_RightAlt: return CKKEY_RMENU;
            case ImGuiKey_RightSuper: return CKKEY_RWIN;
            case ImGuiKey_Menu: return CKKEY_APPS;
            case ImGuiKey_0: return CKKEY_0;
            case ImGuiKey_1: return CKKEY_1;
            case ImGuiKey_2: return CKKEY_2;
            case ImGuiKey_3: return CKKEY_3;
            case ImGuiKey_4: return CKKEY_4;
            case ImGuiKey_5: return CKKEY_5;
            case ImGuiKey_6: return CKKEY_6;
            case ImGuiKey_7: return CKKEY_7;
            case ImGuiKey_8: return CKKEY_8;
            case ImGuiKey_9: return CKKEY_9;
            case ImGuiKey_A: return CKKEY_A;
            case ImGuiKey_B: return CKKEY_B;
            case ImGuiKey_C: return CKKEY_C;
            case ImGuiKey_D: return CKKEY_D;
            case ImGuiKey_E: return CKKEY_E;
            case ImGuiKey_F: return CKKEY_F;
            case ImGuiKey_G: return CKKEY_G;
            case ImGuiKey_H: return CKKEY_H;
            case ImGuiKey_I: return CKKEY_I;
            case ImGuiKey_J: return CKKEY_J;
            case ImGuiKey_K: return CKKEY_K;
            case ImGuiKey_L: return CKKEY_L;
            case ImGuiKey_M: return CKKEY_M;
            case ImGuiKey_N: return CKKEY_N;
            case ImGuiKey_O: return CKKEY_O;
            case ImGuiKey_P: return CKKEY_P;
            case ImGuiKey_Q: return CKKEY_Q;
            case ImGuiKey_R: return CKKEY_R;
            case ImGuiKey_S: return CKKEY_S;
            case ImGuiKey_T: return CKKEY_T;
            case ImGuiKey_U: return CKKEY_U;
            case ImGuiKey_V: return CKKEY_V;
            case ImGuiKey_W: return CKKEY_W;
            case ImGuiKey_X: return CKKEY_X;
            case ImGuiKey_Y: return CKKEY_Y;
            case ImGuiKey_Z: return CKKEY_Z;
            case ImGuiKey_F1: return CKKEY_F1;
            case ImGuiKey_F2: return CKKEY_F2;
            case ImGuiKey_F3: return CKKEY_F3;
            case ImGuiKey_F4: return CKKEY_F4;
            case ImGuiKey_F5: return CKKEY_F5;
            case ImGuiKey_F6: return CKKEY_F6;
            case ImGuiKey_F7: return CKKEY_F7;
            case ImGuiKey_F8: return CKKEY_F8;
            case ImGuiKey_F9: return CKKEY_F9;
            case ImGuiKey_F10: return CKKEY_F10;
            case ImGuiKey_F11: return CKKEY_F11;
            case ImGuiKey_F12: return CKKEY_F12;
            default: return static_cast<CKKEYBOARD>(0);
        }
    }

    bool KeyChordToString(ImGuiKeyChord keyChord, char *buffer, std::size_t bufferSize) {
        if (!buffer || bufferSize == 0)
            return false;
        buffer[0] = '\0';
        if (keyChord == 0)
            return false;

        const ImGuiKey key = static_cast<ImGuiKey>(keyChord & ~ImGuiMod_Mask_);
        const int modifiers = keyChord & ImGuiMod_Mask_;

        std::string result;
        if (modifiers & ImGuiMod_Ctrl)
            result += "Ctrl+";
        if (modifiers & ImGuiMod_Shift)
            result += "Shift+";
        if (modifiers & ImGuiMod_Alt)
            result += "Alt+";
        if (modifiers & ImGuiMod_Super)
            result += "Super+";

        const char *keyName = ImGui::GetKeyName(key);
        if (!keyName || *keyName == '\0')
            return false;

        result += keyName;
        ImStrncpy(buffer, result.c_str(), bufferSize);
        return true;
    }

    bool SetKeyChordFromIO(ImGuiKeyChord *keyChord) {
        if (!keyChord)
            return false;

        ImGuiKeyChord chord = 0;
        ImGuiIO &io = ImGui::GetIO();

        if (io.KeyCtrl)
            chord |= ImGuiMod_Ctrl;
        if (io.KeyShift)
            chord |= ImGuiMod_Shift;
        if (io.KeyAlt)
            chord |= ImGuiMod_Alt;
        if (io.KeySuper)
            chord |= ImGuiMod_Super;

        for (int key = ImGuiKey_Tab; key < ImGuiKey_AppBack; ++key) {
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))) {
                if ((chord & ImGuiMod_Ctrl) != 0 && (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl))
                    continue;

                if ((chord & ImGuiMod_Shift) != 0 && (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift))
                    continue;

                if ((chord & ImGuiMod_Alt) != 0 && (key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt))
                    continue;

                if ((chord & ImGuiMod_Super) != 0 && (key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper))
                    continue;

                chord |= key;
                *keyChord = chord;
                return true;
            }
        }

        return false;
    }

    void PlayMenuClickSound() {
#ifndef BML_UI_AUTOMATION_TEST
        if (Resources.messageManager && Resources.sounds && Resources.menuClickMessage != -1)
            Resources.messageManager->SendMessageSingle(Resources.menuClickMessage, Resources.sounds);
#endif
    }

    ImVec2 GetMenuPos() {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        return {viewport->Pos.x + viewport->Size.x * 0.3f, viewport->Pos.y};
    }

    ImVec2 GetMenuSize() {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        return {vpSize.x * 0.4f, vpSize.y};
    }

    ImVec4 GetMenuColor() {
        return {0.0f, 0.0f, 0.0f, 155.0f / 255.0f};
    }

    ImVec2 GetButtonSize(ButtonType type) {
        if (type < 0 || type >= BUTTON_COUNT)
            return ImVec2(0.0f, 0.0f);
        return CoordToPixel(ButtonSizes[type]);
    }

    float GetButtonIndent(ButtonType type) {
        if (type < 0 || type >= BUTTON_COUNT)
            return 0.0f;
        return ButtonIndents[type] * ImGui::GetMainViewport()->Size.x;
    }

    ImVec2 GetButtonSizeInCoord(ButtonType type) {
        if (type < 0 || type >= BUTTON_COUNT)
            return ImVec2(0.0f, 0.0f);
        return ButtonSizes[type];
    }

    float GetButtonIndentInCoord(ButtonType type) {
        if (type < 0 || type >= BUTTON_COUNT)
            return 0.0f;
        return ButtonIndents[type];
    }

    enum TextOverflowMode {
        TextOverflowEllipsis,
        TextOverflowMarqueeWhenActive,
    };

    static TextOverflowMode GetButtonTextOverflowMode(ButtonType type) {
        return type == BUTTON_MAIN || type == BUTTON_LEVEL
            ? TextOverflowMarqueeWhenActive
            : TextOverflowEllipsis;
    }

    static ImRect GetButtonTextRect(const ImRect &bb, ButtonType type) {
        const float indent = GetButtonIndent(type);
        return ImRect(ImVec2(bb.Min.x + indent, bb.Min.y), ImVec2(bb.Max.x - indent, bb.Max.y));
    }

    static ImRect GetButtonOverflowTextRect(const ImRect &bb, ButtonType type) {
        ImRect textRect = GetButtonTextRect(bb, type);

        if (type == BUTTON_LEVEL) {
            const ImVec2 size = bb.GetSize();
            textRect.Min.x = bb.Min.x + size.x * 0.20f;
            textRect.Max.x = bb.Max.x - size.x * 0.08f;
        }

        return textRect;
    }

    static void RenderEllipsisText(ImDrawList *drawList, const ImVec2 &textMin, const ImVec2 &textMax,
                                   const char *text, const ImVec2 *textSize, const ImVec2 &textAlign,
                                   const ImRect *clipRect) {
        if (!drawList || !text || text[0] == '\0' || !textSize)
            return;

        const float availableWidth = textMax.x - textMin.x;
        if (textSize->x <= availableWidth) {
            ImGui::RenderTextClipped(textMin, textMax, text, nullptr, textSize, textAlign, clipRect);
            return;
        }

        float textY = textMin.y + (textMax.y - textMin.y - textSize->y) * textAlign.y;
        if (textY < textMin.y)
            textY = textMin.y;

        ImGui::RenderTextEllipsis(drawList, ImVec2(textMin.x, textY), ImVec2(textMax.x, textMax.y),
                                  textMax.x, text, nullptr, textSize);
    }

    static void RenderMarqueeText(ImDrawList *drawList, const ImVec2 &textMin, const ImVec2 &textMax,
                                  const char *text, const ImVec2 *textSize, bool selected,
                                  float selectedTimer, const ImRect *clipRect) {
        if (!drawList || !text || text[0] == '\0' || !textSize)
            return;

        const float availableWidth = textMax.x - textMin.x;
        if (textSize->x <= availableWidth) {
            ImGui::RenderTextClipped(textMin, textMax, text, nullptr, textSize, ImVec2(0.5f, 0.5f), clipRect);
            return;
        }

        constexpr float ScrollSpeed = 45.0f;
        constexpr float ScrollGap = 36.0f;

        float textY = textMin.y + (textMax.y - textMin.y - textSize->y) * 0.5f;
        if (textY < textMin.y)
            textY = textMin.y;

        if (!selected) {
            RenderEllipsisText(drawList, textMin, textMax, text, textSize, ImVec2(0.5f, 0.5f), clipRect);
            return;
        }

        const float cycleWidth = textSize->x + ScrollGap;
        const float scrollOffset = fmodf(selectedTimer * ScrollSpeed, cycleWidth);
        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
        const float firstX = textMin.x - scrollOffset;

        drawList->PushClipRect(textMin, textMax, true);
        drawList->AddText(ImVec2(firstX, textY), textColor, text);
        drawList->AddText(ImVec2(firstX + cycleWidth, textY), textColor, text);
        drawList->PopClipRect();
    }

    static void RenderButtonText(ImDrawList *drawList, const ImRect &bb, ButtonType type, const char *text,
                                 const ImVec2 &textAlign, bool selected, float selectedTimer) {
        if (!text || text[0] == '\0')
            return;

        const ImRect textRect = GetButtonTextRect(bb, type);
        const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, true);
        if (textSize.x <= textRect.GetWidth()) {
            ImGui::RenderTextClipped(textRect.Min, textRect.Max, text, nullptr, &textSize, textAlign, &bb);
            return;
        }

        const ImRect overflowTextRect = GetButtonOverflowTextRect(bb, type);
        if (selected && GetButtonTextOverflowMode(type) == TextOverflowMarqueeWhenActive) {
            RenderMarqueeText(drawList, overflowTextRect.Min, overflowTextRect.Max, text,
                              &textSize, true, selectedTimer, &overflowTextRect);
            return;
        }

        RenderEllipsisText(drawList, overflowTextRect.Min, overflowTextRect.Max, text,
                           &textSize, textAlign, &overflowTextRect);
    }

    static void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, int state) {
        if (!drawList || type < 0 || type >= BUTTON_COUNT)
            return;

        TextureType texture;
        switch (state) {
            case 1:
                texture = TEXTURE_BUTTON_SELECT;
                break;
            case 2:
                texture = TEXTURE_BUTTON_SPECIAL;
                break;
            default:
                texture = TEXTURE_BUTTON_DESELECT;
                break;
        }

        const ImTextureID textureId = static_cast<ImTextureID>(
            reinterpret_cast<std::uintptr_t>(Resources.textures[texture]));
        drawList->AddImage(textureId, bb.Min, bb.Max, ButtonUvs[type].Min, ButtonUvs[type].Max);
    }

    static void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, int state,
                               const char *text, const ImVec2 &textAlign) {
        AddButtonImage(drawList, bb, type, state);
        RenderButtonText(drawList, bb, type, text, textAlign, false, 0.0f);
    }

    static void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, bool selected) {
        AddButtonImage(drawList, bb, type, selected ? 1 : 0);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state) {
        ImVec2 size = GetButtonSize(type);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        AddButtonImage(drawList, bb, type, state);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected) {
        AddButtonImage(drawList, pos, type, selected ? 1 : 0);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text) {
        AddButtonImage(drawList, pos, type, state, text, ImGui::GetStyle().ButtonTextAlign);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text) {
        AddButtonImage(drawList, pos, type, selected ? 1 : 0, text, ImGui::GetStyle().ButtonTextAlign);
    }

    void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, bool selected,
                        const char *text, const ImVec2 &textAlign) {
        AddButtonImage(drawList, bb, type, selected ? 1 : 0, text, textAlign);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state,
                        const char *text, const ImVec2 &textAlign) {
        const ImVec2 size = GetButtonSize(type);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        AddButtonImage(drawList, bb, type, state, text, textAlign);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected,
                        const char *text, const ImVec2 &textAlign) {
        AddButtonImage(drawList, pos, type, selected ? 1 : 0, text, textAlign);
    }

    static bool TextImageButton(const char *label, const char *text, ButtonType type, ImGuiButtonFlags flags = 0) {
        if (!label || !text || type < 0 || type >= BUTTON_COUNT)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(type);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
        if (pressed)
            PlayMenuClickSound();

        const bool active = pressed || hovered || held;
        ImGuiContext &g = *GImGui;
        const float activeTimer = held ? g.ActiveIdTimer : (hovered ? g.HoveredIdTimer : 0.0f);

        AddButtonImage(window->DrawList, bb, type, active);
        RenderButtonText(window->DrawList, bb, type, text, ImGui::GetStyle().ButtonTextAlign,
                         active, activeTimer);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    static bool SelectableTextImageButton(const char *label, const char *text, ButtonType type, bool *selected,
                                          ImGuiButtonFlags flags = 0) {
        if (!label || !text || type < 0 || type >= BUTTON_COUNT)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(type);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
        if (pressed)
            PlayMenuClickSound();

        int state = (selected && *selected) ? 0 : 2;
        const bool active = pressed || hovered || held;
        if (active) {
            state = 1;
            if (selected)
                *selected = true;
        }

        ImGuiContext &g = *GImGui;
        const float activeTimer = held ? g.ActiveIdTimer : (hovered ? g.HoveredIdTimer : 0.0f);

        AddButtonImage(window->DrawList, bb, type, state);
        RenderButtonText(window->DrawList, bb, type, text, ImGui::GetStyle().ButtonTextAlign,
                         active, activeTimer);

        IMGUI_TEST_ENGINE_ITEM_INFO(
            id, label,
            g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable |
                ((selected && *selected) ? ImGuiItemStatusFlags_Checked : 0));
        return pressed;
    }

    static bool ImageButton(const char *label, ButtonType type, ImGuiButtonFlags flags = 0) {
        if (!label || type < 0 || type >= BUTTON_COUNT)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(label);
        const ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(type);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
        if (pressed)
            PlayMenuClickSound();

        AddButtonImage(window->DrawList, bb, type, pressed || hovered || held);

        ImGuiContext &g = *GImGui;
        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool MainButton(const char *label, ImGuiButtonFlags flags) {
        return TextImageButton(label, label, BUTTON_MAIN, flags);
    }

    bool OkButton(const char *label, ImGuiButtonFlags flags) {
        return TextImageButton(label, "OK", BUTTON_BACK, flags);
    }

    bool BackButton(const char *label, ImGuiButtonFlags flags) {
        return TextImageButton(label, "Back", BUTTON_BACK, flags);
    }

    bool OptionButton(const char *label, ImGuiButtonFlags flags) {
        return TextImageButton(label, label, BUTTON_OPTION, flags);
    }

    bool LevelButton(const char *label, bool *selected, ImGuiButtonFlags flags) {
        return SelectableTextImageButton(label, label, BUTTON_LEVEL, selected, flags);
    }

    bool SmallButton(const char *label, bool *selected, ImGuiButtonFlags flags) {
        return SelectableTextImageButton(label, label, BUTTON_SMALL, selected, flags);
    }

    bool LeftButton(const char *label, ImGuiButtonFlags flags) {
        return ImageButton(label, BUTTON_LEFT, flags);
    }

    bool RightButton(const char *label, ImGuiButtonFlags flags) {
        return ImageButton(label, BUTTON_RIGHT, flags);
    }

    bool PlusButton(const char *label, ImGuiButtonFlags flags) {
        return ImageButton(label, BUTTON_PLUS, flags);
    }

    bool MinusButton(const char *label, ImGuiButtonFlags flags) {
        return ImageButton(label, BUTTON_MINUS, flags);
    }

    bool KeyButton(const char *label, bool *listening, ImGuiKeyChord *keyChord) {
        constexpr float LabelLeftInsetRatio = 0.1055f;
        constexpr float LabelRightInsetRatio = 0.5195f;
        constexpr float ChordLeftInsetRatio = 0.5625f;
        constexpr float ChordRightInsetRatio = 0.0195f;
        constexpr float HighlightLeftInsetRatio = 0.155f;
        constexpr float HighlightWidthRatio = 0.145f;
        constexpr float HighlightHeightRatio = 0.039f;
        constexpr float HighlightUvMinX = 0.005f;
        constexpr float HighlightUvMinY = 0.3850f;
        constexpr float HighlightUvMaxX = 0.4320f;
        constexpr float HighlightUvMaxY = 0.4500f;

        if (!label || !listening || !keyChord)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiStyle &style = ImGui::GetStyle();
        const ImGuiID id = window->GetID(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 size = GetButtonSize(BUTTON_KEY);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        ImGuiContext &g = *GImGui;
        IMGUI_TEST_ENGINE_ITEM_INFO(
            id, label,
            g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Inputable |
                (*listening ? ImGuiItemStatusFlags_Checked : 0));

        bool changed = false;
        if (*listening) {
            if ((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || SetKeyChordFromIO(keyChord)) {
                *listening = false;
                changed = true;
            }
        } else if (pressed) {
            *listening = true;
            PlayMenuClickSound();
        }

        ImDrawList *drawList = window->DrawList;
        AddButtonImage(drawList, bb, BUTTON_KEY, hovered);

        const float labelLeftInset = size.x * LabelLeftInsetRatio;
        const float labelRightInset = size.x * LabelRightInsetRatio;
        const ImVec2 labelMin(bb.Min.x + labelLeftInset, bb.Min.y);
        const ImVec2 labelMax(bb.Max.x - labelRightInset, bb.Max.y);
        RenderEllipsisText(drawList, labelMin, labelMax, label, &textSize, style.ButtonTextAlign, &bb);

        if (*keyChord != 0) {
            const float chordLeftInset = size.x * ChordLeftInsetRatio;
            const float chordRightInset = size.x * ChordRightInsetRatio;
            const ImVec2 chordMin(bb.Min.x + chordLeftInset, bb.Min.y);
            const ImVec2 chordMax(bb.Max.x - chordRightInset, bb.Max.y);

            char keyText[32]{};
            if (KeyChordToString(*keyChord, keyText, sizeof(keyText))) {
                const ImVec2 keyTextSize = ImGui::CalcTextSize(keyText, nullptr, true);
                RenderEllipsisText(drawList, chordMin, chordMax, keyText, &keyTextSize,
                                   style.ButtonTextAlign, &bb);
            }
        }

        if (*listening && Resources.materials[MATERIAL_KEYS_HIGHLIGHT]) {
            const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
            const ImVec2 highlightSize(vpSize.x * HighlightWidthRatio, vpSize.y * HighlightHeightRatio);
            const ImVec2 highlightMin(bb.Min.x + vpSize.x * HighlightLeftInsetRatio, bb.Min.y);
            const ImVec2 highlightMax(highlightMin.x + highlightSize.x, highlightMin.y + highlightSize.y);
            const ImVec2 uvMin(HighlightUvMinX, HighlightUvMinY);
            const ImVec2 uvMax(HighlightUvMaxX, HighlightUvMaxY);

            drawList->AddImage(Resources.materials[MATERIAL_KEYS_HIGHLIGHT], highlightMin, highlightMax, uvMin, uvMax);
        }

        return changed;
    }

    struct OptionRow {
        const char *label = nullptr;
        ImGuiWindow *window = nullptr;
        ImGuiID id = 0;
        ImVec2 position;
        ImVec2 size;
        ImRect bounds;
        ImVec2 restoreCursor;
        bool hovered = false;
        bool held = false;
        bool pressed = false;
        float activeTimer = 0.0f;
    };

    static bool BeginOptionRow(const char *label, OptionRow &row) {
        if (!label)
            return false;

        row.window = ImGui::GetCurrentWindow();
        if (row.window->SkipItems)
            return false;

        row.label = label;
        row.id = row.window->GetID(label);
        row.position = row.window->DC.CursorPos;
        row.size = GetButtonSize(BUTTON_OPTION);
        row.bounds = ImRect(row.position, ImVec2(row.position.x + row.size.x, row.position.y + row.size.y));

        ImGui::BeginGroup();
        ImGui::ItemSize(row.bounds);
        if (!ImGui::ItemAdd(row.bounds, row.id)) {
            ImGui::EndGroup();
            return false;
        }

        row.pressed = ImGui::ButtonBehavior(
            row.bounds, row.id, &row.hovered, &row.held,
            ImGuiButtonFlags_AllowOverlap | ImGuiButtonFlags_FlattenChildren);
        ImGuiContext &context = *GImGui;
        row.activeTimer = row.held ? context.ActiveIdTimer : (row.hovered ? context.HoveredIdTimer : 0.0f);

        AddButtonImage(row.window->DrawList, row.bounds, BUTTON_OPTION, row.hovered);
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);
        const float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 textMin(row.bounds.Min.x + indent, row.bounds.Min.y);
        const ImVec2 textMax(row.bounds.Max.x - indent, row.bounds.Max.y);
        RenderEllipsisText(row.window->DrawList, textMin, textMax, label, &textSize,
                           ImVec2(0.5f, 0.21f), &row.bounds);

        row.restoreCursor = ImGui::GetCursorScreenPos();
        return true;
    }

    static void ReportOptionRow(const OptionRow &row, ImGuiItemStatusFlags extraStatus = 0) {
        ImGuiContext &g = *GImGui;
        IMGUI_TEST_ENGINE_ITEM_INFO(row.id, row.label, g.LastItemData.StatusFlags | extraStatus);
    }

    static void EndOptionRow(const OptionRow &row) {
        ImGui::SetCursorScreenPos(row.restoreCursor);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::EndGroup();
    }

    static void BeginOptionInput(const OptionRow &row) {
        ImGui::SetCursorScreenPos(
            ImVec2(row.position.x + row.size.x * 0.24f, row.position.y + row.size.y * 0.45f));
        ImGui::SetNextItemWidth(row.size.x * 0.6f);
        ImGui::PushID(row.label);
    }

    static void EndOptionInput(const OptionRow &row) {
        ImGui::PopID();
        EndOptionRow(row);
    }

    bool YesNoButton(const char *label, bool *value) {
        if (!value)
            return false;

        OptionRow row;
        if (!BeginOptionRow(label, row))
            return false;

        const bool originalValue = *value;
        if (row.pressed)
            *value = !*value;
        ReportOptionRow(row, ImGuiItemStatusFlags_Checkable |
                             (*value ? ImGuiItemStatusFlags_Checked : 0));

        const float spacing = row.size.x * 0.05f;
        ImGui::SetCursorScreenPos(
            ImVec2(row.position.x + row.size.x * 0.27f, row.position.y + row.size.y * 0.43f));
        ImGui::PushID(label);
        bool yesSelected = *value;
        const bool yesPressed = SmallButton("Yes", &yesSelected);
        ImGui::SameLine(0.0f, spacing);
        bool noSelected = !*value;
        const bool noPressed = SmallButton("No", &noSelected);
        ImGui::PopID();
        EndOptionRow(row);

        if (!yesPressed && !noPressed)
            return false;

        *value = yesPressed;
        return *value != originalValue;
    }

    bool RadioButton(const char *label, int *currentItem, const char *const items[], int itemCount) {
        if (!currentItem || !items || itemCount <= 0)
            return false;

        int selectedItem = *currentItem;
        if (selectedItem < 0 || selectedItem >= itemCount)
            selectedItem = 0;

        OptionRow row;
        if (!BeginOptionRow(label, row))
            return false;
        ReportOptionRow(row);

        const ImVec2 previousPosition(row.position.x + row.size.x * 0.23f,
                                      row.position.y + row.size.y * 0.50f);
        const ImVec2 nextPosition(row.position.x + row.size.x * 0.82f,
                                  row.position.y + row.size.y * 0.50f);
        const ImVec2 textMin(row.position.x + row.size.x * 0.32f,
                             row.position.y + row.size.y * 0.38f);
        const ImVec2 textMax(row.position.x + row.size.x * 0.80f,
                             row.position.y + row.size.y * 0.90f);

        bool changed = false;
        ImGui::PushID(label);
        ImGui::SetCursorScreenPos(previousPosition);
        if (MinusButton("##RadioPrev")) {
            selectedItem = (selectedItem + itemCount - 1) % itemCount;
            changed = true;
        }

        ImGui::SetCursorScreenPos(nextPosition);
        if (PlusButton("##RadioNext")) {
            selectedItem = (selectedItem + 1) % itemCount;
            changed = true;
        }

        const char *currentText = items[selectedItem] ? items[selectedItem] : "";
        const ImVec2 currentTextSize = ImGui::CalcTextSize(currentText, nullptr, true);
        RenderMarqueeText(row.window->DrawList, textMin, textMax, currentText, &currentTextSize,
                          row.hovered || row.held, row.activeTimer, &row.bounds);
        ImGui::PopID();
        EndOptionRow(row);

        if (changed)
            *currentItem = selectedItem;
        return changed;
    }

    bool InputTextButton(const char *label, char *buffer, std::size_t bufferSize,
                         ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *userData) {
        if (!buffer || bufferSize == 0)
            return false;

        OptionRow row;
        if (!BeginOptionRow(label, row))
            return false;
        ReportOptionRow(row, ImGuiItemStatusFlags_Inputable);

        BeginOptionInput(row);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        const bool changed = ImGui::InputText("##InputText", buffer, bufferSize, flags, callback, userData);
        ImGui::PopStyleColor();
        EndOptionInput(row);
        return changed;
    }

    bool InputTextButton(const char *label, std::string *value, ImGuiInputTextFlags flags,
                         ImGuiInputTextCallback callback, void *userData) {
        if (!value)
            return false;

        OptionRow row;
        if (!BeginOptionRow(label, row))
            return false;
        ReportOptionRow(row, ImGuiItemStatusFlags_Inputable);

        BeginOptionInput(row);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        const bool changed = ImGui::InputText("##InputText", value, flags, callback, userData);
        ImGui::PopStyleColor();
        EndOptionInput(row);
        return changed;
    }

    bool InputFloatButton(const char *label, float *value, float step, float stepFast,
                          const char *format, ImGuiInputTextFlags flags) {
        if (!value || !format)
            return false;

        OptionRow row;
        if (!BeginOptionRow(label, row))
            return false;
        ReportOptionRow(row, ImGuiItemStatusFlags_Inputable);

        BeginOptionInput(row);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        const bool changed = ImGui::InputFloat("##InputFloat", value, step, stepFast, format, flags);
        ImGui::PopStyleColor();
        EndOptionInput(row);
        return changed;
    }

    bool InputIntButton(const char *label, int *value, int step, int stepFast, ImGuiInputTextFlags flags) {
        if (!value)
            return false;

        OptionRow row;
        if (!BeginOptionRow(label, row))
            return false;
        ReportOptionRow(row, ImGuiItemStatusFlags_Inputable);

        BeginOptionInput(row);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.63f, 0.32f, 0.18f, 0.57f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.63f, 0.32f, 0.18f, 0.57f));
        const bool changed = ImGui::InputInt("##InputInt", value, step, stepFast, flags);
        ImGui::PopStyleColor(4);
        EndOptionInput(row);
        return changed;
    }

    void WrappedText(const char *text, float width, float baseX, float scale) {
        if (!text || !*text) return;

        const float startX = (fabsf(baseX) < EPSILON) ? ImGui::GetCursorPosX() : baseX;
        const bool doScale = (scale != 1.0f);

        if (doScale)
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * scale);

        if (width > 0.0f) {
            const ImVec2 sz = ImGui::CalcTextSize(text, nullptr, false, width);
            float indent = (width - sz.x) * 0.5f;
            if (indent < 0.0f) indent = 0.0f;

            ImGui::SetCursorPosX(startX + indent);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
        } else {
            const float avail = ImGui::GetContentRegionAvail().x;
            const ImVec2 sz = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosX(startX + (avail - sz.x) * 0.5f);
            ImGui::TextUnformatted(text);
        }

        if (doScale)
            ImGui::PopFont();
    }

    bool NavLeft(float x, float y) {
        return At(x, y, []() {
            return LeftButton("PrevPage") || ImGui::IsKeyPressed(ImGuiKey_PageUp);
        });
    }

    bool NavRight(float x, float y) {
        return At(x, y, []() {
            return RightButton("NextPage") || ImGui::IsKeyPressed(ImGuiKey_PageDown);
        });
    }

    bool NavBack(float x, float y) {
        return At(x, y, []() {
            return BackButton("Back") || ImGui::IsKeyPressed(ImGuiKey_Escape);
        });
    }

#ifndef BML_UI_AUTOMATION_TEST
    void BlockKeyboardInput() {
        if (AcquireKeyboardInputBlock())
            ++KeyboardInputBlocks.anonymousUsers;
    }

    void BlockKeyboardInput(const void *owner) {
        if (!owner) {
            BlockKeyboardInput();
            return;
        }
        if (KeyboardInputBlocks.owners.contains(owner))
            return;
        if (AcquireKeyboardInputBlock())
            KeyboardInputBlocks.owners.insert(owner);
    }

    void ActivateScript(const char *scriptName) {
        if (!scriptName || !*scriptName)
            return;
        ModContext *context = BML_GetModContext();
        CKContext *ckContext = BML_GetCKContext();
        if (!context || !ckContext)
            return;
        CKBehavior *script = context->GetScriptByName(scriptName);
        if (script && ckContext->GetCurrentScene())
            ckContext->GetCurrentScene()->Activate(script, true);
    }

    void UnblockKeyboardAfterRelease() {
        if (KeyboardInputBlocks.anonymousUsers == 0)
            return;
        --KeyboardInputBlocks.anonymousUsers;
        ReleaseKeyboardInputBlockAfterKeysUp();
    }

    void UnblockKeyboardAfterRelease(const void *owner) {
        if (!owner) {
            UnblockKeyboardAfterRelease();
            return;
        }
        if (KeyboardInputBlocks.owners.erase(owner) == 0)
            return;
        ReleaseKeyboardInputBlockAfterKeysUp();
    }

    void TransitionToScriptAndUnblock(const char *scriptName) {
        ActivateScript(scriptName);
        UnblockKeyboardAfterRelease();
    }

    void TransitionToScriptAndUnblock(const char *scriptName, const void *owner) {
        ActivateScript(scriptName);
        UnblockKeyboardAfterRelease(owner);
    }
#endif

    void Title(const char *text, float y, float scale, ImU32 color) {
        if (!text || !*text)
            return;

        ImFont *font = ImGui::GetFont();
        const float size = ImGui::GetFontSize() * (scale > 0.0f ? scale : 1.0f);

        const ImVec2 titleSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);

        const ImGuiViewport *vp = ImGui::GetMainViewport();
        const ImVec2 pos(ImTrunc(vp->Pos.x + (vp->Size.x - titleSize.x) * 0.5f), ImTrunc(vp->Pos.y + vp->Size.y * y));

        ImGui::GetForegroundDrawList()->AddText(font, size, pos, color, text);
    }

    bool SearchBar(char *buffer, std::size_t bufferSize, float x, float y, float width) {
        if (!buffer || bufferSize == 0)
            return false;

        return At(x, y, [=]() {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, GetMenuColor());
            ImGui::SetNextItemWidth(ImGui::GetMainViewport()->Size.x * width);

            const bool changed = ImGui::InputText("##SearchBar", buffer, bufferSize);

            ImGui::PopStyleColor();
            return changed;
        });
    }
}
