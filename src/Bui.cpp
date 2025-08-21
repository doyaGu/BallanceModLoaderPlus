#include "BML/Bui.h"

#include <cstring>

#include "CKMessageManager.h"
#include "CKPathManager.h"
#include "CKTexture.h"
#include "CKMaterial.h"
#include "CKGroup.h"

#include "imgui_internal.h"

#include "Overlay.h"

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

    ImRect g_ButtonUVs[BUTTON_COUNT] = {
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

    ImVec2 g_ButtonSizes[BUTTON_COUNT] = {
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

    float g_ButtonIndent[BUTTON_COUNT] = {
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

    CKTexture *g_Textures[TEXTURE_COUNT] = {};
    CKMaterial *g_Materials[MATERIAL_COUNT] = {};
    CKGroup *g_Sounds = nullptr;
    CKMessageManager *g_MessageManager = nullptr;
    CKMessageType g_MenuClickMessageType = -1;

    ImGuiContext *GetImGuiContext() {
        return Overlay::GetImGuiContext();
    }

    CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot) {
        if (!context || !filename)
            return nullptr;

        XString fname((CKSTRING) filename);
        CKERROR res = CK_OK;
        if (fname.Length() == 0)
            return nullptr;

        CKPathManager *pm = context->GetPathManager();
        if (pm->ResolveFileName(fname, BITMAP_PATH_IDX) != CK_OK)
            return nullptr;

        auto *tex = (CKTexture *) context->CreateObject(CKCID_TEXTURE, (CKSTRING) id);
        if (!tex)
            return nullptr;

        if (!tex->LoadImage(fname.Str(), slot)) {
            context->DestroyObject(tex);
            return nullptr;
        }

        tex->SetDesiredVideoFormat(_32_ARGB8888);

        return tex;
    }

    bool InitTextures(CKContext *context) {
        if (!context)
            return false;

        g_Textures[TEXTURE_BUTTON_DESELECT] = LoadTexture(context, "TEX_Button_Deselect", "Button01_deselect.tga");
        g_Textures[TEXTURE_BUTTON_SELECT] = LoadTexture(context, "TEX_Button_Select", "Button01_select.tga");
        g_Textures[TEXTURE_BUTTON_SPECIAL] = LoadTexture(context, "TEX_Button_Special", "Button01_special.tga");
        g_Textures[TEXTURE_FONT] = LoadTexture(context, "TEX_Font_1", "Font_1.tga");

        for (auto *texture: g_Textures)
            if (texture == nullptr)
                return false;

        return true;
    }

    bool InitMaterials(CKContext *context) {
        if (!context)
            return false;

        auto *mbu = (CKMaterial *) context->CreateObject(CKCID_MATERIAL, (CKSTRING) "MAT_Button_Up");
        if (!mbu)
            return false;

        mbu->SetAmbient(VxColor(76, 76, 76, 255));
        mbu->SetDiffuse(VxColor(255, 255, 255, 255));
        mbu->SetSpecular(VxColor(127, 127, 127, 255));
        mbu->SetPower(0.0f);
        mbu->SetEmissive(VxColor(255, 255, 255, 0));

        mbu->SetFillMode(VXFILL_SOLID);
        mbu->SetShadeMode(VXSHADE_GOURAUD);

        mbu->EnableZWrite(FALSE);

        mbu->EnableAlphaBlend();
        mbu->SetSourceBlend(VXBLEND_SRCALPHA);
        mbu->SetDestBlend(VXBLEND_INVSRCALPHA);

        mbu->SetTexture0(g_Textures[TEXTURE_BUTTON_DESELECT]);
        mbu->SetTextureBlendMode(VXTEXTUREBLEND_MODULATEALPHA);
        mbu->SetTextureMinMode(VXTEXTUREFILTER_LINEAR);
        mbu->SetTextureMagMode(VXTEXTUREFILTER_LINEAR);
        mbu->SetTextureAddressMode(VXTEXTURE_ADDRESSCLAMP);

        g_Materials[MATERIAL_BUTTON_UP] = mbu;

        auto *mbo = (CKMaterial *) context->CreateObject(CKCID_MATERIAL, (CKSTRING) "MAT_Button_Over");
        if (!mbo)
            return false;

        mbo->SetAmbient(VxColor(76, 76, 76, 255));
        mbo->SetDiffuse(VxColor(255, 255, 255, 255));
        mbo->SetSpecular(VxColor(127, 127, 127, 255));
        mbo->SetPower(0.0f);
        mbo->SetEmissive(VxColor(255, 255, 255, 0));

        mbo->SetFillMode(VXFILL_SOLID);
        mbo->SetShadeMode(VXSHADE_GOURAUD);

        mbo->EnableZWrite(FALSE);

        mbo->EnableAlphaBlend();
        mbo->SetSourceBlend(VXBLEND_SRCALPHA);
        mbo->SetDestBlend(VXBLEND_INVSRCALPHA);

        mbo->SetTexture0(g_Textures[TEXTURE_BUTTON_SELECT]);
        mbo->SetTextureBlendMode(VXTEXTUREBLEND_MODULATEALPHA);
        mbo->SetTextureMinMode(VXTEXTUREFILTER_LINEAR);
        mbo->SetTextureMagMode(VXTEXTUREFILTER_LINEAR);
        mbo->SetTextureAddressMode(VXTEXTURE_ADDRESSCLAMP);

        g_Materials[MATERIAL_BUTTON_OVER] = mbo;

        auto *mbi = (CKMaterial *) context->CreateObject(CKCID_MATERIAL, (CKSTRING) "MAT_Button_Inactive");
        if (!mbi)
            return false;

        mbi->SetAmbient(VxColor(76, 76, 76, 255));
        mbi->SetDiffuse(VxColor(255, 255, 255, 255));
        mbi->SetSpecular(VxColor(127, 127, 127, 255));
        mbi->SetPower(0.0f);
        mbi->SetEmissive(VxColor(255, 255, 255, 0));

        mbi->SetFillMode(VXFILL_SOLID);
        mbi->SetShadeMode(VXSHADE_GOURAUD);

        mbi->EnableZWrite(FALSE);

        mbi->EnableAlphaBlend();
        mbi->SetSourceBlend(VXBLEND_SRCALPHA);
        mbi->SetDestBlend(VXBLEND_INVSRCALPHA);

        mbi->SetTexture0(g_Textures[TEXTURE_BUTTON_SPECIAL]);
        mbi->SetTextureBlendMode(VXTEXTUREBLEND_MODULATEALPHA);
        mbi->SetTextureMinMode(VXTEXTUREFILTER_LINEAR);
        mbi->SetTextureMagMode(VXTEXTUREFILTER_LINEAR);
        mbi->SetTextureAddressMode(VXTEXTURE_ADDRESSCLAMP);

        mbi->EnablePerspectiveCorrection(TRUE);

        g_Materials[MATERIAL_BUTTON_INACTIVE] = mbi;

        auto *mkh = (CKMaterial *) context->CreateObject(CKCID_MATERIAL, (CKSTRING) "MAT_Keys_Highlight");
        if (!mkh)
            return false;

        mkh->SetAmbient(VxColor(76, 76, 76, 255));
        mkh->SetDiffuse(VxColor(209, 209, 209, 255));
        mkh->SetSpecular(VxColor(127, 127, 127, 255));
        mkh->SetPower(0.0f);
        mkh->SetEmissive(VxColor(255, 255, 255, 0));

        mkh->SetFillMode(VXFILL_SOLID);
        mkh->SetShadeMode(VXSHADE_GOURAUD);

        mkh->EnableZWrite(FALSE);

        mkh->EnableAlphaBlend();
        mkh->SetSourceBlend(VXBLEND_ONE);
        mkh->SetDestBlend(VXBLEND_ONE);

        mkh->SetTexture0(g_Textures[TEXTURE_BUTTON_SPECIAL]);
        mkh->SetTextureBlendMode(VXTEXTUREBLEND_MODULATEALPHA);
        mkh->SetTextureMinMode(VXTEXTUREFILTER_LINEAR);
        mkh->SetTextureMagMode(VXTEXTUREFILTER_LINEAR);
        mkh->SetTextureAddressMode(VXTEXTURE_ADDRESSWRAP);

        mkh->EnablePerspectiveCorrection(TRUE);

        g_Materials[MATERIAL_KEYS_HIGHLIGHT] = mkh;

        return true;
    }

    bool InitSounds(CKContext *context) {
        if (!context)
            return false;

        g_MessageManager = context->GetMessageManager();
        if (!g_MessageManager)
            return false;

        g_MenuClickMessageType = g_MessageManager->AddMessageType((CKSTRING) "Menu_Click");
        if (g_MenuClickMessageType == -1)
            return false;

        g_Sounds = (CKGroup *) context->GetObjectByNameAndClass((CKSTRING) "All_Sound", CKCID_GROUP);
        if (!g_Sounds)
            return false;

        return true;
    }

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
            default: return (CKKEYBOARD) 0;
        }
    }

    bool KeyChordToString(ImGuiKeyChord key_chord, char *buf, size_t size) {
        if (!buf || size == 0 || key_chord == 0)
            return false;

        const char *s = ImGui::GetKeyChordName(key_chord);
        if (!s || *s == '\0')
            return false;
        ImStrncpy(buf, s, (int) size);
        return true;
    }

    bool SetKeyChordFromIO(ImGuiKeyChord *key_chord) {
        ImGuiKeyChord chord = 0;

        ImGuiIO &io = ImGui::GetIO();

        // Use ImGuiMod_* flags for chord modifiers (unified since 1.89+).
        if (io.KeyCtrl)  chord |= ImGuiMod_Ctrl;
        if (io.KeyShift) chord |= ImGuiMod_Shift;
        if (io.KeyAlt)   chord |= ImGuiMod_Alt;
        if (io.KeySuper) chord |= ImGuiMod_Super;

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

                *key_chord = chord;
                return true;
            }
        }

        return false;
    }

    void PlayMenuClickSound() {
        if (g_Sounds)
            g_MessageManager->SendMessageSingle(g_MenuClickMessageType, g_Sounds);
    }

    ImVec2 GetMenuPos() {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        return {vpSize.x * 0.3f, 0.0f};
    }

    ImVec2 GetMenuSize() {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        return {vpSize.x * 0.4f, vpSize.y};
    }

    ImVec4 GetMenuColor() {
        return {0.0f, 0.0f, 0.0f, 0.57f};
    }

    ImVec2 GetButtonSize(ButtonType type) {
        return CoordToPixel(g_ButtonSizes[type]);
    }

    float GetButtonIndent(ButtonType type) {
        return g_ButtonIndent[type] * ImGui::GetMainViewport()->Size.x;
    }

    void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, int state) {
        assert(drawList != nullptr);

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

        drawList->AddImage((ImTextureID) g_Textures[texture], bb.Min, bb.Max, g_ButtonUVs[type].Min, g_ButtonUVs[type].Max);
    }

    void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, int state, const char *text, const ImVec2 &text_align) {
        AddButtonImage(drawList, bb, type, state);
        if (text && text[0] != '\0') {
            float indent = GetButtonIndent(type);
            const ImVec2 min(bb.Min.x + indent, bb.Min.y);
            const ImVec2 max(bb.Max.x - indent, bb.Max.y);
            const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, true);
            ImGui::RenderTextClipped(min, max, text, nullptr, &textSize, text_align, &bb);
        }
    }

    void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, bool selected) {
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

    void AddButtonImage(ImDrawList *drawList, const ImRect &bb, ButtonType type, bool selected, const char *text, const ImVec2 &text_align) {
        AddButtonImage(drawList, bb, type, selected ? 1 : 0, text, text_align);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text, const ImVec2 &text_align) {
        ImVec2 size = GetButtonSize(type);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        AddButtonImage(drawList, bb, type, state, text, text_align);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text, const ImVec2 &text_align) {
        AddButtonImage(drawList, pos, type, selected ? 1 : 0, text, text_align);
    }

    bool TextImageButton(const char *label, const char *text, ButtonType type, ImGuiButtonFlags flags = 0) {
        assert(label != nullptr);
        assert(text != nullptr);

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiStyle &style = ImGui::GetStyle();
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

        AddButtonImage(window->DrawList, bb, type, pressed || hovered || held, text, ImGui::GetStyle().ButtonTextAlign);

        return pressed;
    }

    bool TextImageButtonV(const char *label, const char *text, ButtonType type, bool *v, ImGuiButtonFlags flags = 0) {
        assert(label != nullptr);
        assert(text != nullptr);

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiStyle &style = ImGui::GetStyle();
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

        int state = (v && *v) ? 0 : 2;
        if (pressed || hovered || held) {
            state = 1;
            if (v) *v = true;
        }

        AddButtonImage(window->DrawList, bb, type, state, text, ImGui::GetStyle().ButtonTextAlign);

        return pressed;
    }

    bool ImageButton(const char *label, ButtonType type, ImGuiButtonFlags flags = 0) {
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

    bool LevelButton(const char *label, bool *v, ImGuiButtonFlags flags) {
        return TextImageButtonV(label, label, BUTTON_LEVEL, v, flags);
    }

    bool SmallButton(const char *label, bool *v, ImGuiButtonFlags flags) {
        return TextImageButtonV(label, label, BUTTON_SMALL, v, flags);
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

    bool KeyButton(const char *label, bool *toggled, ImGuiKeyChord *key_chord) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiStyle &style = ImGui::GetStyle();
        const ImGuiID id = window->GetID(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(BUTTON_KEY);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        bool changed = false;
        if (*toggled) {
            if ((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || SetKeyChordFromIO(key_chord)) {
                *toggled = false;
                changed = true;
            }
        } else if (pressed) {
            *toggled = !*toggled;
            PlayMenuClickSound();
        }

        auto *drawList = ImGui::GetWindowDrawList();

        AddButtonImage(drawList, bb, BUTTON_KEY, hovered);

        float xl1 = size.x * 0.1055f, xr1 = size.x * 0.5195f;
        const ImVec2 min1(bb.Min.x + xl1, bb.Min.y);
        const ImVec2 max1(bb.Max.x - xr1, bb.Max.y);
        ImGui::RenderTextClipped(min1, max1, label, nullptr, &textSize, style.ButtonTextAlign, &bb);

        if (*key_chord != 0) {
            float xl2 = size.x * 0.5625f, xr2 = size.x * 0.0195f;
            const ImVec2 min2(bb.Min.x + xl2, bb.Min.y);
            const ImVec2 max2(bb.Max.x, bb.Max.y);

            char keyText[32];
            KeyChordToString(*key_chord, keyText, sizeof(keyText));
            const ImVec2 keyTextSize = ImGui::CalcTextSize(keyText, nullptr, true);
            ImGui::RenderTextClipped(min2, max2, keyText, nullptr, &keyTextSize, style.ButtonTextAlign, &bb);
        }

        if (*toggled) {
            ImVec2 vpSize = ImGui::GetMainViewport()->Size;
            const ImVec2 size0(vpSize.x * 0.145f, vpSize.y * 0.039f);
            const ImVec2 min0(bb.Min.x + vpSize.x * 0.155f, bb.Min.y);
            const ImVec2 max0(min0.x + size0.x, min0.y + size0.y);

            const ImVec2 uv0(0.005f, 0.3850f);
            const ImVec2 uv1(0.4320f, 0.4500f);

            drawList->AddImage(g_Materials[MATERIAL_KEYS_HIGHLIGHT], min0, max0, uv0, uv1);
        }

        return changed;
    }

    bool YesNoButton(const char *label, bool *v) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

        ImGui::BeginGroup();

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) {
            ImGui::EndGroup();
            return false;
        }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_AllowOverlap | ImGuiButtonFlags_FlattenChildren);
        if (pressed)
            *v = !*v;

        AddButtonImage(window->DrawList, bb, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bb.Min.x + indent, bb.Min.y);
        const ImVec2 max(bb.Max.x - indent, bb.Max.y);
        ImGui::RenderTextClipped(min, max, label, nullptr, &textSize, ImVec2(0.5f, 0.21f), &bb);

        // Inline "Yes / No"
        ImVec2 backup = ImGui::GetCursorScreenPos();
        float spacing = size.x * 0.05f;
        ImVec2 smPos(pos.x + size.x * 0.27f, pos.y + size.y * 0.43f);
        ImGui::SetCursorScreenPos(smPos);

        ImGui::PushID(label);
        bool yf = *v;
        bool yes = SmallButton("Yes", &yf);
        ImGui::SameLine(0, spacing);
        bool nf = !*v;
        bool no = SmallButton("No", &nf);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(backup);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::EndGroup();

        if (yes || no) {
            *v = yes;
            return true;
        }
        return false;
    }

    bool RadioButton(const char *label, int *current_item, const char *const items[], int items_count) {
        return false;
    }

    bool InputTextButton(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

        ImGui::BeginGroup();

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) {
            ImGui::EndGroup();
            return false;
        }

        bool hovered, held;
        ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_AllowOverlap | ImGuiButtonFlags_FlattenChildren);

        AddButtonImage(window->DrawList, bb, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bb.Min.x + indent, bb.Min.y);
        const ImVec2 max(bb.Max.x - indent, bb.Max.y);
        ImGui::RenderTextClipped(min, max, label, nullptr, &textSize, ImVec2(0.5f, 0.21f), &bb);

        ImVec2 backup = ImGui::GetCursorScreenPos();
        ImVec2 smPos(pos.x + size.x * 0.24f, pos.y + size.y * 0.45f);
        ImGui::SetCursorScreenPos(smPos);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        ImGui::SetNextItemWidth(size.x * 0.6f);

        ImGui::PushID(label);
        bool changed = ImGui::InputText("##InputText", buf, buf_size, flags, callback, user_data);
        ImGui::PopID();

        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(backup);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::EndGroup();

        return changed;
    }

    bool InputFloatButton(const char *label, float *v, float step, float step_fast, const char *format, ImGuiInputTextFlags flags) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

        ImGui::BeginGroup();

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) {
            ImGui::EndGroup();
            return false;
        }

        bool hovered, held;
        ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_AllowOverlap | ImGuiButtonFlags_FlattenChildren);

        AddButtonImage(window->DrawList, bb, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bb.Min.x + indent, bb.Min.y);
        const ImVec2 max(bb.Max.x - indent, bb.Max.y);
        ImGui::RenderTextClipped(min, max, label, nullptr, &textSize, ImVec2(0.5f, 0.21f), &bb);

        ImVec2 backup = ImGui::GetCursorScreenPos();
        ImVec2 smPos(pos.x + size.x * 0.24f, pos.y + size.y * 0.45f);
        ImGui::SetCursorScreenPos(smPos);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        ImGui::SetNextItemWidth(size.x * 0.6f);

        ImGui::PushID(label);
        bool changed = ImGui::InputFloat("##InputFloat", v, step, step_fast, format, flags);
        ImGui::PopID();

        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(backup);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::EndGroup();

        return changed;
    }

    bool InputIntButton(const char *label, int *v, int step, int step_fast, ImGuiInputTextFlags flags) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

        ImGui::BeginGroup();

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) {
            ImGui::EndGroup();
            return false;
        }

        bool hovered, held;
        ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_AllowOverlap | ImGuiButtonFlags_FlattenChildren);

        AddButtonImage(window->DrawList, bb, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bb.Min.x + indent, bb.Min.y);
        const ImVec2 max(bb.Max.x - indent, bb.Max.y);
        ImGui::RenderTextClipped(min, max, label, nullptr, &textSize, ImVec2(0.5f, 0.21f), &bb);

        ImVec2 backup = ImGui::GetCursorScreenPos();
        ImVec2 smPos(pos.x + size.x * 0.24f, pos.y + size.y * 0.45f);
        ImGui::SetCursorScreenPos(smPos);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.63f, 0.32f, 0.18f, 0.57f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.63f, 0.32f, 0.18f, 0.57f));

        ImGui::SetNextItemWidth(size.x * 0.6f);

        ImGui::PushID(label);
        bool changed = ImGui::InputInt("##InputInt", v, step, step_fast, flags);
        ImGui::PopID();

        ImGui::PopStyleColor(4);

        ImGui::SetCursorScreenPos(backup);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::EndGroup();

        return changed;
    }

    void WrappedText(const char *text, float width, float scale) {
        if (!text || !*text) return;

        const float startX = ImGui::GetCursorPosX();
        const bool doScale = (scale != 1.0f);

        if (doScale)
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * scale);

        if (width > 0.0f) {
            const float textW = ImGui::CalcTextSize(text).x;
            const float indent = (width - textW) * 0.5f;
            if (indent > 0.0f)
                ImGui::SetCursorPosX(startX + indent);

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
        } else {
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

    void Title(const char *text, float y, float scale, ImU32 color) {
        if (!text || !*text) return;

        const float sizeBase = ImGui::GetStyle().FontSizeBase * (scale > 0.0f ? scale : 1.0f);

        // Measure at the desired size
        ImGui::PushFont(nullptr, sizeBase);
        const ImVec2 titleSize = ImGui::CalcTextSize(text);
        ImGui::PopFont();

        // Center on main viewport (absolute coords)
        const ImGuiViewport *vp = ImGui::GetMainViewport();
        const ImVec2 pos(vp->Pos.x + (vp->Size.x - titleSize.x) * 0.5f,
                         vp->Pos.y + vp->Size.y * y);

        // Draw without disturbing the current font stack
        ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(), sizeBase, pos, color, text);
    }

    bool SearchBar(char *buffer, size_t bufferSize, float x, float y, float width) {
        return At(x, y, [=]() {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, GetMenuColor());
            ImGui::SetNextItemWidth(ImGui::GetMainViewport()->Size.x * width);

            bool changed = ImGui::InputText("##SearchBar", buffer, bufferSize);

            ImGui::PopStyleColor();
            return changed;
        });
    }
}
