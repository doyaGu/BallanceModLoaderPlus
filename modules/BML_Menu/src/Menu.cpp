#define BML_MENU_PROVIDER_IMPLEMENTATION
#include "bml_menu.hpp"

#include <cstring>
#include <string>
#include <cmath>

#include "CKMessageManager.h"
#include "CKPathManager.h"
#include "CKTexture.h"
#include "CKMaterial.h"
#include "CKGroup.h"

// Internal API usage removed - using public ImGui API only

namespace Menu {
    namespace {
        [[nodiscard]] const BML_ImGuiApi *GetImGuiApi() {
            const BML_ImGuiApi *api = bml::imgui::GetCurrentApi();
            assert(api != nullptr && "Menu provider requires an active ImGui API scope");
            return api;
        }

        [[nodiscard]] ImTextureRef_c MakeTextureRef(ImTextureID textureId) {
            ImTextureRef_c textureRef{};
            textureRef._TexID = textureId;
            return textureRef;
        }

        void CopyString(char *dst, size_t dstSize, const char *src) {
            if (!dst || dstSize == 0 || !src) {
                return;
            }

            std::strncpy(dst, src, dstSize - 1);
            dst[dstSize - 1] = '\0';
        }
    } // namespace

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

    // Button UV coordinates (min, max pairs)
    struct ButtonUV {
        ImVec2 min;
        ImVec2 max;
    };

    ButtonUV g_ButtonUVs[BUTTON_COUNT] = {
        {{0.0f, 0.51372f}, {1.0f, 0.7451f}},
        {{0.2392f, 0.75294f}, {0.8666f, 0.98431f}},
        {{0.0f, 0.0f}, {1.0f, 0.24706f}},
        {{0.0f, 0.247f}, {0.643f, 0.36863f}},
        {{0.0f, 0.40785f}, {1.0f, 0.51f}},
        {{0.0f, 0.82353f}, {0.226f, 0.9098f}},
        {{0.6392f, 0.24706f}, {0.78823f, 0.40392f}},
        {{0.7921f, 0.24706f}, {0.9412f, 0.40392f}},
        {{0.88627f, 0.8902f}, {0.96863f, 0.97255f}},
        {{0.88627f, 0.77804f}, {0.96863f, 0.8594f}},
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

    CKContext *g_Context = nullptr;
    CKTexture *g_Textures[TEXTURE_COUNT] = {};
    CKMaterial *g_Materials[MATERIAL_COUNT] = {};
    CKGroup *g_Sounds = nullptr;
    CKMessageManager *g_MessageManager = nullptr;
    CKMessageType g_MenuClickMessageType = -1;

    CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot) {
        if (!context || !filename)
            return nullptr;

        XString fname((CKSTRING) filename);
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

        g_Context = context;

        if (!g_Textures[TEXTURE_BUTTON_DESELECT])
            g_Textures[TEXTURE_BUTTON_DESELECT] = LoadTexture(context, "TEX_Button_Deselect", "Button01_deselect.tga");
        if (!g_Textures[TEXTURE_BUTTON_SELECT])
            g_Textures[TEXTURE_BUTTON_SELECT] = LoadTexture(context, "TEX_Button_Select", "Button01_select.tga");
        if (!g_Textures[TEXTURE_BUTTON_SPECIAL])
            g_Textures[TEXTURE_BUTTON_SPECIAL] = LoadTexture(context, "TEX_Button_Special", "Button01_special.tga");
        if (!g_Textures[TEXTURE_FONT])
            g_Textures[TEXTURE_FONT] = LoadTexture(context, "TEX_Font_1", "Font_1.tga");

        for (auto *texture: g_Textures)
            if (texture == nullptr)
                return false;

        return true;
    }

    struct MaterialDesc {
        const char *name;
        VxColor diffuse;
        VXBLEND_MODE srcBlend;
        VXBLEND_MODE dstBlend;
        TextureType texture;
        VXTEXTURE_ADDRESSMODE addressMode;
        bool perspectiveCorrection;
    };

    static CKMaterial *CreateButtonMaterial(CKContext *context, const MaterialDesc &desc) {
        auto *mat = (CKMaterial *) context->CreateObject(CKCID_MATERIAL, (CKSTRING) desc.name);
        if (!mat) return nullptr;

        mat->SetAmbient(VxColor(76, 76, 76, 255));
        mat->SetDiffuse(desc.diffuse);
        mat->SetSpecular(VxColor(127, 127, 127, 255));
        mat->SetPower(0.0f);
        mat->SetEmissive(VxColor(255, 255, 255, 0));
        mat->SetFillMode(VXFILL_SOLID);
        mat->SetShadeMode(VXSHADE_GOURAUD);
        mat->EnableZWrite(FALSE);
        mat->EnableAlphaBlend();
        mat->SetSourceBlend(desc.srcBlend);
        mat->SetDestBlend(desc.dstBlend);
        mat->SetTexture0(g_Textures[desc.texture]);
        mat->SetTextureBlendMode(VXTEXTUREBLEND_MODULATEALPHA);
        mat->SetTextureMinMode(VXTEXTUREFILTER_LINEAR);
        mat->SetTextureMagMode(VXTEXTUREFILTER_LINEAR);
        mat->SetTextureAddressMode(desc.addressMode);
        if (desc.perspectiveCorrection) mat->EnablePerspectiveCorrection(TRUE);

        return mat;
    }

    bool InitMaterials(CKContext *context) {
        if (!context)
            return false;

        static const MaterialDesc kMaterialDescs[MATERIAL_COUNT] = {
            {"MAT_Button_Up",       VxColor(255, 255, 255, 255), VXBLEND_SRCALPHA, VXBLEND_INVSRCALPHA, TEXTURE_BUTTON_DESELECT, VXTEXTURE_ADDRESSCLAMP, false},
            {"MAT_Button_Over",     VxColor(255, 255, 255, 255), VXBLEND_SRCALPHA, VXBLEND_INVSRCALPHA, TEXTURE_BUTTON_SELECT,   VXTEXTURE_ADDRESSCLAMP, false},
            {"MAT_Button_Inactive", VxColor(255, 255, 255, 255), VXBLEND_SRCALPHA, VXBLEND_INVSRCALPHA, TEXTURE_BUTTON_SPECIAL,  VXTEXTURE_ADDRESSCLAMP, true},
            {"MAT_Keys_Highlight",  VxColor(209, 209, 209, 255), VXBLEND_ONE,      VXBLEND_ONE,         TEXTURE_BUTTON_SPECIAL,  VXTEXTURE_ADDRESSWRAP,  true},
        };

        for (int i = 0; i < MATERIAL_COUNT; ++i) {
            if (!g_Materials[i]) {
                g_Materials[i] = CreateButtonMaterial(context, kMaterialDescs[i]);
                if (!g_Materials[i]) return false;
            }
        }

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

    void CleanupResources(CKContext *context) {
        CKContext *ctx = context ? context : g_Context;
        if (ctx) {
            for (auto &tex : g_Textures) {
                if (tex) {
                    ctx->DestroyObject(tex);
                    tex = nullptr;
                }
            }
            for (auto &mat : g_Materials) {
                if (mat) {
                    ctx->DestroyObject(mat);
                    mat = nullptr;
                }
            }
        }
        g_Sounds = nullptr;
        g_MessageManager = nullptr;
        g_MenuClickMessageType = -1;
        g_Context = nullptr;
    }

    struct KeyMapping { CKKEYBOARD ck; ImGuiKey imgui; };
    static const KeyMapping kKeyMappings[] = {
        {CKKEY_TAB, ImGuiKey_Tab}, {CKKEY_LEFT, ImGuiKey_LeftArrow}, {CKKEY_RIGHT, ImGuiKey_RightArrow},
        {CKKEY_UP, ImGuiKey_UpArrow}, {CKKEY_DOWN, ImGuiKey_DownArrow}, {CKKEY_PRIOR, ImGuiKey_PageUp},
        {CKKEY_NEXT, ImGuiKey_PageDown}, {CKKEY_HOME, ImGuiKey_Home}, {CKKEY_END, ImGuiKey_End},
        {CKKEY_INSERT, ImGuiKey_Insert}, {CKKEY_DELETE, ImGuiKey_Delete}, {CKKEY_BACK, ImGuiKey_Backspace},
        {CKKEY_SPACE, ImGuiKey_Space}, {CKKEY_RETURN, ImGuiKey_Enter}, {CKKEY_ESCAPE, ImGuiKey_Escape},
        {CKKEY_APOSTROPHE, ImGuiKey_Apostrophe}, {CKKEY_COMMA, ImGuiKey_Comma}, {CKKEY_MINUS, ImGuiKey_Minus},
        {CKKEY_PERIOD, ImGuiKey_Period}, {CKKEY_SLASH, ImGuiKey_Slash}, {CKKEY_SEMICOLON, ImGuiKey_Semicolon},
        {CKKEY_EQUALS, ImGuiKey_Equal}, {CKKEY_LBRACKET, ImGuiKey_LeftBracket}, {CKKEY_BACKSLASH, ImGuiKey_Backslash},
        {CKKEY_RBRACKET, ImGuiKey_RightBracket}, {CKKEY_GRAVE, ImGuiKey_GraveAccent},
        {CKKEY_CAPITAL, ImGuiKey_CapsLock}, {CKKEY_SCROLL, ImGuiKey_ScrollLock}, {CKKEY_NUMLOCK, ImGuiKey_NumLock},
        {CKKEY_NUMPAD0, ImGuiKey_Keypad0}, {CKKEY_NUMPAD1, ImGuiKey_Keypad1}, {CKKEY_NUMPAD2, ImGuiKey_Keypad2},
        {CKKEY_NUMPAD3, ImGuiKey_Keypad3}, {CKKEY_NUMPAD4, ImGuiKey_Keypad4}, {CKKEY_NUMPAD5, ImGuiKey_Keypad5},
        {CKKEY_NUMPAD6, ImGuiKey_Keypad6}, {CKKEY_NUMPAD7, ImGuiKey_Keypad7}, {CKKEY_NUMPAD8, ImGuiKey_Keypad8},
        {CKKEY_NUMPAD9, ImGuiKey_Keypad9}, {CKKEY_DECIMAL, ImGuiKey_KeypadDecimal},
        {CKKEY_DIVIDE, ImGuiKey_KeypadDivide}, {CKKEY_MULTIPLY, ImGuiKey_KeypadMultiply},
        {CKKEY_SUBTRACT, ImGuiKey_KeypadSubtract}, {CKKEY_ADD, ImGuiKey_KeypadAdd},
        {CKKEY_NUMPADENTER, ImGuiKey_KeypadEnter}, {CKKEY_NUMPADEQUALS, ImGuiKey_KeypadEqual},
        {CKKEY_LCONTROL, ImGuiKey_LeftCtrl}, {CKKEY_LSHIFT, ImGuiKey_LeftShift},
        {CKKEY_LMENU, ImGuiKey_LeftAlt}, {CKKEY_LWIN, ImGuiKey_LeftSuper},
        {CKKEY_RCONTROL, ImGuiKey_RightCtrl}, {CKKEY_RSHIFT, ImGuiKey_RightShift},
        {CKKEY_RMENU, ImGuiKey_RightAlt}, {CKKEY_RWIN, ImGuiKey_RightSuper}, {CKKEY_APPS, ImGuiKey_Menu},
        {CKKEY_0, ImGuiKey_0}, {CKKEY_1, ImGuiKey_1}, {CKKEY_2, ImGuiKey_2}, {CKKEY_3, ImGuiKey_3},
        {CKKEY_4, ImGuiKey_4}, {CKKEY_5, ImGuiKey_5}, {CKKEY_6, ImGuiKey_6}, {CKKEY_7, ImGuiKey_7},
        {CKKEY_8, ImGuiKey_8}, {CKKEY_9, ImGuiKey_9},
        {CKKEY_A, ImGuiKey_A}, {CKKEY_B, ImGuiKey_B}, {CKKEY_C, ImGuiKey_C}, {CKKEY_D, ImGuiKey_D},
        {CKKEY_E, ImGuiKey_E}, {CKKEY_F, ImGuiKey_F}, {CKKEY_G, ImGuiKey_G}, {CKKEY_H, ImGuiKey_H},
        {CKKEY_I, ImGuiKey_I}, {CKKEY_J, ImGuiKey_J}, {CKKEY_K, ImGuiKey_K}, {CKKEY_L, ImGuiKey_L},
        {CKKEY_M, ImGuiKey_M}, {CKKEY_N, ImGuiKey_N}, {CKKEY_O, ImGuiKey_O}, {CKKEY_P, ImGuiKey_P},
        {CKKEY_Q, ImGuiKey_Q}, {CKKEY_R, ImGuiKey_R}, {CKKEY_S, ImGuiKey_S}, {CKKEY_T, ImGuiKey_T},
        {CKKEY_U, ImGuiKey_U}, {CKKEY_V, ImGuiKey_V}, {CKKEY_W, ImGuiKey_W}, {CKKEY_X, ImGuiKey_X},
        {CKKEY_Y, ImGuiKey_Y}, {CKKEY_Z, ImGuiKey_Z},
        {CKKEY_F1, ImGuiKey_F1}, {CKKEY_F2, ImGuiKey_F2}, {CKKEY_F3, ImGuiKey_F3}, {CKKEY_F4, ImGuiKey_F4},
        {CKKEY_F5, ImGuiKey_F5}, {CKKEY_F6, ImGuiKey_F6}, {CKKEY_F7, ImGuiKey_F7}, {CKKEY_F8, ImGuiKey_F8},
        {CKKEY_F9, ImGuiKey_F9}, {CKKEY_F10, ImGuiKey_F10}, {CKKEY_F11, ImGuiKey_F11}, {CKKEY_F12, ImGuiKey_F12},
    };

    ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key) {
        for (const auto &m : kKeyMappings) {
            if (m.ck == key) return m.imgui;
        }
        return ImGuiKey_None;
    }

    CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key) {
        for (const auto &m : kKeyMappings) {
            if (m.imgui == key) return m.ck;
        }
        return (CKKEYBOARD) 0;
    }

    bool KeyChordToString(ImGuiKeyChord key_chord, char *buf, size_t size) {
        if (!buf || size == 0 || key_chord == 0)
            return false;

        // Extract modifiers and key from chord
        ImGuiKey key = (ImGuiKey)(key_chord & ~ImGuiMod_Mask_);
        int mods = key_chord & ImGuiMod_Mask_;

        // Build string manually since GetKeyChordName is internal
        std::string result;
        if (mods & ImGuiMod_Ctrl)  result += "Ctrl+";
        if (mods & ImGuiMod_Shift) result += "Shift+";
        if (mods & ImGuiMod_Alt)   result += "Alt+";
        if (mods & ImGuiMod_Super) result += "Super+";

        // Get key name
        const char *keyName = ImGui::GetKeyName(key);
        if (keyName && *keyName != '\0') {
            result += keyName;
            CopyString(buf, size, result.c_str());
            return true;
        }
        return false;
    }

    bool SetKeyChordFromIO(ImGuiKeyChord *key_chord) {
        ImGuiKeyChord chord = 0;

        ImGuiIO &io = ImGui::GetIO();

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
        if (g_Sounds && g_MessageManager && g_MenuClickMessageType != -1)
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
        return {0.0f, 0.0f, 0.0f, 155.0f / 255.0f};
    }

    ImVec2 GetButtonSize(ButtonType type) {
        return CoordToPixel(g_ButtonSizes[type]);
    }

    float GetButtonIndent(ButtonType type) {
        return g_ButtonIndent[type] * ImGui::GetMainViewport()->Size.x;
    }

    ImVec2 GetButtonSizeInCoord(ButtonType type) {
        return g_ButtonSizes[type];
    }

    float GetButtonIndentInCoord(ButtonType type) {
        return g_ButtonIndent[type];
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &bbMin, const ImVec2 &bbMax, ButtonType type, int state) {
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

        const BML_ImGuiApi *api = GetImGuiApi();
        if (!api->draw_list || !api->draw_list->AddImage)
            return;

        api->draw_list->AddImage(
            drawList,
            MakeTextureRef((ImTextureID) g_Textures[texture]),
            bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(bbMin),
            bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(bbMax),
            bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(g_ButtonUVs[type].min),
            bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(g_ButtonUVs[type].max),
            IM_COL32_WHITE);
    }

    // Helper to render clipped text using C API wrapper
    void RenderClippedText(ImDrawList *drawList, const ImVec2 &min, const ImVec2 &max, const char *text, const ImVec2 &textSize, const ImVec2 &align) {
        if (!text || !text[0]) return;

        // Calculate aligned position
        ImVec2 pos = min;
        const ImVec2 availSize(max.x - min.x, max.y - min.y);

        if (align.x > 0.0f) pos.x += availSize.x * align.x - textSize.x * align.x;
        if (align.y > 0.0f) pos.y += availSize.y * align.y - textSize.y * align.y;

        // Clamp position to bounding box
        if (pos.x < min.x) pos.x = min.x;
        if (pos.y < min.y) pos.y = min.y;

        // Use the C API wrapper to add text with clipping
        const BML_ImGuiApi *api = GetImGuiApi();
        if (!api) return;

        // Push clip rect if text extends beyond bounds
        bool needsClip = (pos.x + textSize.x > max.x) || (pos.y + textSize.y > max.y);
        if (needsClip && api->draw_list->PushClipRect) {
            api->draw_list->PushClipRect(
                drawList,
                bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(min),
                bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(max),
                true);
        }

        // Add text
        if (api->draw_list->AddText_Vec2) {
            api->draw_list->AddText_Vec2(
                drawList,
                bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(pos),
                ImGui::GetColorU32(ImGuiCol_Text),
                text,
                nullptr);
        }

        // Pop clip rect
        if (needsClip && api->draw_list->PopClipRect) {
            api->draw_list->PopClipRect(drawList);
        }
    }

    void RenderMarqueeText(
        ImDrawList *drawList,
        const ImVec2 &min,
        const ImVec2 &max,
        const char *text,
        const ImVec2 &textSize,
        bool active) {
        if (!text || !text[0]) {
            return;
        }

        const float width = max.x - min.x;
        if (!active || textSize.x <= width) {
            RenderClippedText(drawList, min, max, text, textSize, ImVec2(0.5f, 0.5f));
            return;
        }

        const BML_ImGuiApi *api = GetImGuiApi();
        if (!api || !api->draw_list->AddText_Vec2) {
            return;
        }

        const float overflow = textSize.x - width;
        const float gap = ImGui::GetFontSize() * 2.0f;
        const float cycleWidth = overflow + gap;
        const float scrollSpeed = 45.0f;
        const float offset = std::fmod(static_cast<float>(ImGui::GetTime()) * scrollSpeed, cycleWidth);
        const float posY = min.y + ((max.y - min.y) - textSize.y) * 0.5f;

        if (api->draw_list->PushClipRect) {
            api->draw_list->PushClipRect(
                drawList,
                bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(min),
                bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(max),
                true);
        }

        const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
        const ImVec2 firstPos(min.x - offset, posY);
        api->draw_list->AddText_Vec2(
            drawList,
            bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(firstPos),
            color,
            text,
            nullptr);

        const ImVec2 secondPos(firstPos.x + textSize.x + gap, posY);
        api->draw_list->AddText_Vec2(
            drawList,
            bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(secondPos),
            color,
            text,
            nullptr);

        if (api->draw_list->PopClipRect) {
            api->draw_list->PopClipRect(drawList);
        }
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &bbMin, const ImVec2 &bbMax, ButtonType type, int state, const char *text, const ImVec2 &text_align) {
        AddButtonImage(drawList, bbMin, bbMax, type, state);
        if (text && text[0] != '\0') {
            float indent = GetButtonIndent(type);
            const ImVec2 min(bbMin.x + indent, bbMin.y);
            const ImVec2 max(bbMax.x - indent, bbMax.y);
            const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, true);
            RenderClippedText(drawList, min, max, text, textSize, text_align);
        }
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state) {
        ImVec2 size = GetButtonSize(type);
        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(drawList, bbMin, bbMax, type, state);
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

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text, const ImVec2 &text_align) {
        ImVec2 size = GetButtonSize(type);
        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(drawList, bbMin, bbMax, type, state, text, text_align);
    }

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text, const ImVec2 &text_align) {
        AddButtonImage(drawList, pos, type, selected ? 1 : 0, text, text_align);
    }

    bool TextImageButton(const char *label, const char *text, ButtonType type, ImGuiButtonFlags flags = 0) {
        assert(label != nullptr);
        assert(text != nullptr);

        ImVec2 size = GetButtonSize(type);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        // Use InvisibleButton for interaction
        bool pressed = ImGui::InvisibleButton(label, size, flags);
        if (pressed)
            PlayMenuClickSound();

        // Check hover/active state
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();

        // Draw button image on top
        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, type, pressed || hovered || held, text, ImGui::GetStyle().ButtonTextAlign);

        return pressed;
    }

    bool TextImageButtonV(const char *label, const char *text, ButtonType type, bool *v, ImGuiButtonFlags flags = 0) {
        assert(label != nullptr);
        assert(text != nullptr);

        ImVec2 size = GetButtonSize(type);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        // Use InvisibleButton for interaction
        bool pressed = ImGui::InvisibleButton(label, size, flags);
        if (pressed)
            PlayMenuClickSound();

        // Check hover/active state
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();

        int state = (v && *v) ? 0 : 2;
        if (pressed || hovered || held) {
            state = 1;
            if (v) *v = true;
        }

        // Draw button image on top
        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, type, state, text, ImGui::GetStyle().ButtonTextAlign);

        return pressed;
    }

    bool ImageButton(const char *label, ButtonType type, ImGuiButtonFlags flags = 0) {
        ImVec2 size = GetButtonSize(type);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        // Use InvisibleButton for interaction
        bool pressed = ImGui::InvisibleButton(label, size, flags);
        if (pressed)
            PlayMenuClickSound();

        // Check hover/active state
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();

        // Draw button image on top
        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, type, pressed || hovered || held);

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
        const ImGuiStyle &style = ImGui::GetStyle();
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 size = GetButtonSize(BUTTON_KEY);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        // Use InvisibleButton for interaction
        bool pressed = ImGui::InvisibleButton(label, size);

        // Check hover state
        bool hovered = ImGui::IsItemHovered();

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

        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(drawList, bbMin, bbMax, BUTTON_KEY, hovered);

        float xl1 = size.x * 0.1055f, xr1 = size.x * 0.5195f;
        const ImVec2 min1(bbMin.x + xl1, bbMin.y);
        const ImVec2 max1(bbMax.x - xr1, bbMax.y);
        RenderClippedText(drawList, min1, max1, label, textSize, style.ButtonTextAlign);

        if (*key_chord != 0) {
            float xl2 = size.x * 0.5625f, xr2 = size.x * 0.0195f;
            const ImVec2 min2(bbMin.x + xl2, bbMin.y);
            const ImVec2 max2(bbMax.x, bbMax.y);

            char keyText[32];
            KeyChordToString(*key_chord, keyText, sizeof(keyText));
            const ImVec2 keyTextSize = ImGui::CalcTextSize(keyText, nullptr, true);
            RenderClippedText(drawList, min2, max2, keyText, keyTextSize, style.ButtonTextAlign);
        }

        if (*toggled) {
            const BML_ImGuiApi *api = GetImGuiApi();
            if (api->draw_list && api->draw_list->AddImage) {
                ImVec2 vpSize = ImGui::GetMainViewport()->Size;
                const ImVec2 size0(vpSize.x * 0.145f, vpSize.y * 0.039f);
                const ImVec2 min0(bbMin.x + vpSize.x * 0.155f, bbMin.y);
                const ImVec2 max0(min0.x + size0.x, min0.y + size0.y);

                const ImVec2 uv0(0.005f, 0.3850f);
                const ImVec2 uv1(0.4320f, 0.4500f);

                api->draw_list->AddImage(
                    drawList,
                    MakeTextureRef((ImTextureID) g_Materials[MATERIAL_KEYS_HIGHLIGHT]),
                    bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(min0),
                    bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(max0),
                    bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(uv0),
                    bml::imgui::detail::ToCValue<ImVec2, ImVec2_c>(uv1),
                    IM_COL32_WHITE);
            }
        }

        return changed;
    }

    bool YesNoButton(const char *label, bool *v) {
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::BeginGroup();

        // Use InvisibleButton for interaction
        bool pressed = ImGui::InvisibleButton(label, size);
        if (pressed)
            *v = !*v;

        // Check hover state
        bool hovered = ImGui::IsItemHovered();

        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bbMin.x + indent, bbMin.y);
        const ImVec2 max(bbMax.x - indent, bbMax.y);
        RenderClippedText(ImGui::GetWindowDrawList(), min, max, label, textSize, ImVec2(0.5f, 0.21f));

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
        if (!label || !current_item || !items || items_count <= 0) {
            return false;
        }

        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        int selectedItem = *current_item;
        if (selectedItem < 0 || selectedItem >= items_count) {
            selectedItem = 0;
        }

        ImGui::BeginGroup();

        ImGui::InvisibleButton(label, size);
        const bool hovered = ImGui::IsItemHovered();

        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bbMin.x + indent, bbMin.y);
        const ImVec2 max(bbMax.x - indent, bbMax.y);
        RenderClippedText(ImGui::GetWindowDrawList(), min, max, label, textSize, ImVec2(0.5f, 0.21f));

        ImVec2 backup = ImGui::GetCursorScreenPos();
        bool changed = false;
        const char *currentText = items[selectedItem] ? items[selectedItem] : "";

        const ImVec2 leftPos(pos.x + size.x * 0.15f, pos.y + size.y * 0.43f);
        const ImVec2 rightPos(pos.x + size.x * 0.75f, pos.y + size.y * 0.43f);
        const ImVec2 textMin(pos.x + size.x * 0.25f, pos.y + size.y * 0.39f);
        const ImVec2 textMax(pos.x + size.x * 0.73f, pos.y + size.y * 0.92f);
        const ImVec2 currentTextSize = ImGui::CalcTextSize(currentText, nullptr, true);
        bool marqueeActive = hovered;

        ImGui::PushID(label);
        ImGui::SetCursorScreenPos(leftPos);
        if (MinusButton("##RadioPrev")) {
            selectedItem = (selectedItem + items_count - 1) % items_count;
            changed = true;
        }
        marqueeActive = marqueeActive || ImGui::IsItemHovered() || ImGui::IsItemActive() || ImGui::IsItemFocused();

        ImGui::SetCursorScreenPos(rightPos);
        if (PlusButton("##RadioNext")) {
            selectedItem = (selectedItem + 1) % items_count;
            changed = true;
        }
        marqueeActive = marqueeActive || ImGui::IsItemHovered() || ImGui::IsItemActive() || ImGui::IsItemFocused();

        RenderMarqueeText(
            ImGui::GetWindowDrawList(),
            textMin,
            textMax,
            currentText,
            currentTextSize,
            marqueeActive);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(backup);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::EndGroup();

        if (changed) {
            *current_item = selectedItem;
        }

        return changed;
    }

    bool InputTextButton(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data) {
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::BeginGroup();

        // Use InvisibleButton for interaction
        ImGui::InvisibleButton(label, size);

        // Check hover state
        bool hovered = ImGui::IsItemHovered();

        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bbMin.x + indent, bbMin.y);
        const ImVec2 max(bbMax.x - indent, bbMax.y);
        RenderClippedText(ImGui::GetWindowDrawList(), min, max, label, textSize, ImVec2(0.5f, 0.21f));

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
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::BeginGroup();

        // Use InvisibleButton for interaction
        ImGui::InvisibleButton(label, size);

        // Check hover state
        bool hovered = ImGui::IsItemHovered();

        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bbMin.x + indent, bbMin.y);
        const ImVec2 max(bbMax.x - indent, bbMax.y);
        RenderClippedText(ImGui::GetWindowDrawList(), min, max, label, textSize, ImVec2(0.5f, 0.21f));

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
        const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImVec2 size = GetButtonSize(BUTTON_OPTION);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::BeginGroup();

        // Use InvisibleButton for interaction
        ImGui::InvisibleButton(label, size);

        // Check hover state
        bool hovered = ImGui::IsItemHovered();

        const ImVec2 bbMin = pos;
        const ImVec2 bbMax(pos.x + size.x, pos.y + size.y);
        AddButtonImage(ImGui::GetWindowDrawList(), bbMin, bbMax, BUTTON_OPTION, hovered);

        float indent = GetButtonIndent(BUTTON_OPTION);
        const ImVec2 min(bbMin.x + indent, bbMin.y);
        const ImVec2 max(bbMax.x - indent, bbMax.y);
        RenderClippedText(ImGui::GetWindowDrawList(), min, max, label, textSize, ImVec2(0.5f, 0.21f));

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

    void Title(const char *text, float y, float scale, ImU32 color) {
        if (!text || !*text) return;

        ImFont *font = ImGui::GetFont();
        const float size = ImGui::GetFontSize() * (scale > 0.0f ? scale : 1.0f);

        const ImVec2 titleSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);

        const ImGuiViewport *vp = ImGui::GetMainViewport();
        const ImVec2 pos(std::floor(vp->Pos.x + (vp->Size.x - titleSize.x) * 0.5f), std::floor(vp->Pos.y + vp->Size.y * y));

        ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddText(font, size, pos, color, text);
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

