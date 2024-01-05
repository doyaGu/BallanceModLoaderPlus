#include "BML/Bui.h"

#include <string>

#include "CKPathManager.h"
#include "CKTexture.h"

#include "imgui_internal.h"

#include "Overlay.h"

namespace Bui {
    enum TextureType {
        TEXTURE_BUTTON_SELECT,
        TEXTURE_BUTTON_DESELECT,
        TEXTURE_BUTTON_SPECIAL,
        TEXTURE_COUNT
    };

    enum ButtonType {
        BUTTON_NORMAL,
        BUTTON_BACK,
        BUTTON_SETTING,
        BUTTON_LEVEL,
        BUTTON_KEY,
        BUTTON_SMALL,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_PLUS,
        BUTTON_MINUS,
    };

    struct UI {
        CKTexture *Textures[TEXTURE_COUNT] = {};
        CKGroup *Sounds = nullptr;
    } g_UI;

    ImGuiContext *GetImGuiContext() {
        return Overlay::GetImGuiContext();
    }

    CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot) {
        if (!context || !filename)
            return nullptr;

        XString fname((CKSTRING)filename);
        CKERROR res = CK_OK;
        if (fname.Length() == 0)
            return nullptr;

        CKPathManager *pm = context->GetPathManager();
        if (pm->ResolveFileName(fname, BITMAP_PATH_IDX) != CK_OK)
            return nullptr;

        auto *tex = (CKTexture *)context->CreateObject(CKCID_TEXTURE, (CKSTRING)id);
        if (!tex)
            return nullptr;

        if (!tex->LoadImage(fname.Str(), slot)) {
            context->DestroyObject(tex);
            return nullptr;
        }

        return tex;
    }

    bool InitTextures(CKContext *context) {
        if (!context)
            return false;

        g_UI.Textures[TEXTURE_BUTTON_SELECT] = LoadTexture(context, "T_Button_Select", "Button01_select.tga");
        g_UI.Textures[TEXTURE_BUTTON_DESELECT] = LoadTexture(context, "T_Button_Deselect", "Button01_deselect.tga");
        g_UI.Textures[TEXTURE_BUTTON_SPECIAL] = LoadTexture(context, "T_Button_Special", "Button01_special.tga");

        for (auto *texture : g_UI.Textures)
            if (texture == nullptr)
                return false;

        return true;
    }

    ImVec2 CalcButtonSize(ButtonType type) {
        const ImVec2 vpSize = ImGui::GetMainViewport()->Size;

        switch (type) {
            case BUTTON_NORMAL:
                return {vpSize.x * 0.3f, vpSize.y * 0.0938f};
            case BUTTON_BACK:
                return {vpSize.x * 0.1875f, vpSize.y * 0.0938f};
            case BUTTON_SETTING:
                return {vpSize.x * 0.3f, vpSize.y * 0.1f};
            case BUTTON_LEVEL:
                return {vpSize.x * 0.1938f, vpSize.y * 0.05f};
            case BUTTON_KEY:
                return {vpSize.x * 0.3f, vpSize.y * 0.0396f};
            case BUTTON_SMALL:
                return {vpSize.x * 0.0700f, vpSize.y * 0.0354f};
            case BUTTON_LEFT:
            case BUTTON_RIGHT:
                return {vpSize.x * 0.0363f, vpSize.y * 0.0517f};
            case BUTTON_PLUS:
            case BUTTON_MINUS:
                return {vpSize.x * 0.0200f, vpSize.y * 0.0267f};
        }
    }

    float CalcButtonIndent(ButtonType type) {
        const ImVec2 vpSize = ImGui::GetMainViewport()->Size;

        switch (type) {
            case BUTTON_NORMAL:
                return vpSize.x * 0.05f;
            case BUTTON_BACK:
                return vpSize.x * 0.044f;
            case BUTTON_SETTING:
                return vpSize.x * 0.05f;
            case BUTTON_LEVEL:
                return vpSize.x * 0.032f;
            case BUTTON_KEY:
            case BUTTON_SMALL:
            case BUTTON_LEFT:
            case BUTTON_RIGHT:
            case BUTTON_PLUS:
            case BUTTON_MINUS:
                return 0.0f;
        }
    }

    void AddButtonImage(ImDrawList *drawList, ButtonType type, const ImRect &bb, bool pressed = false) {
        assert(drawList != nullptr);
 
        ImTextureID textureId;
        if (pressed) {
            textureId = g_UI.Textures[TEXTURE_BUTTON_SELECT];
        } else {
            textureId = g_UI.Textures[TEXTURE_BUTTON_DESELECT];
        }

        ImVec2 uv0;
        ImVec2 uv1;
        switch (type) {
            case BUTTON_NORMAL:
                uv0 = ImVec2(0.0f, 0.51372f);
                uv1 = ImVec2(1.0f, 0.7451f);
                break;
            case BUTTON_BACK:
                uv0 = ImVec2(0.2392f, 0.75294f);
                uv1 = ImVec2(0.8666f, 0.98431f);
                break;
            case BUTTON_SETTING:
                uv0 = ImVec2(0.0f, 0.0f);
                uv1 = ImVec2(1.0f, 0.24706f);
                break;
            case BUTTON_LEVEL:
                uv0 = ImVec2(0.0f, 0.247f);
                uv1 = ImVec2(0.643f, 0.36863f);
                break;
            case BUTTON_KEY:
                uv0 = ImVec2(0.0f, 0.40785f);
                uv1 = ImVec2(1.0f, 0.51f);
                break;
            case BUTTON_SMALL:
                uv0 = ImVec2(0.0f, 0.82353f);
                uv1 = ImVec2(0.226f, 0.9098f);
                break;
            case BUTTON_LEFT:
                uv0 = ImVec2(0.6392f, 0.24706f);
                uv1 = ImVec2(0.78823f, 0.40392f);
                break;
            case BUTTON_RIGHT:
                uv0 = ImVec2(0.7921f, 0.24706f);
                uv1 = ImVec2(0.9412f, 0.40392f);
                break;
            case BUTTON_PLUS:
                uv0 = ImVec2(0.88627f, 0.8902f);
                uv1 = ImVec2(0.96863f, 0.97255f);
                break;
            case BUTTON_MINUS:
                uv0 = ImVec2(0.88627f, 0.77804f);
                uv1 = ImVec2(0.96863f, 0.8594f);
                break;
        }

        drawList->AddImage(textureId, bb.Min, bb.Max, uv0, uv1);
    }

    bool TextImageButton(const char *name, const char *text, ButtonType type, ImGuiButtonFlags flags = 0) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiStyle& style = ImGui::GetStyle();
        const ImGuiID id = window->GetID(name);
        const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcButtonSize(type);
        const ImVec2 padding = style.FramePadding;
        const ImRect bb(pos, ImVec2(pos.x + size.x + padding.x * 2.0f, pos.y + size.y + padding.y * 2.0f));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

        ImGui::RenderNavHighlight(bb, id);

        AddButtonImage(window->DrawList, type, bb, pressed || hovered || held);

        float indent = CalcButtonIndent(type);
        const ImVec2 min(bb.Min.x + indent + padding.x, bb.Min.y + padding.y);
        const ImVec2 max(bb.Max.x - indent - padding.x, bb.Max.y - padding.y);
        ImGui::RenderTextClipped(min, max, text, nullptr, &textSize, style.ButtonTextAlign, &bb);

        return pressed;
    }

    bool ImageButton(const char *name, ButtonType type, ImGuiButtonFlags flags = 0) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiID id = window->GetID(name);
        const ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcButtonSize(type);
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        const ImRect bb(pos, ImVec2(pos.x + size.x + padding.x * 2.0f, pos.y + size.y + padding.y * 2.0f));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

        ImGui::RenderNavHighlight(bb, id);

        AddButtonImage(window->DrawList, type, bb, pressed || hovered || held);

        return pressed;
    }

    bool NormalButton(const char *name, const char *text, ImGuiButtonFlags flags) {
        return TextImageButton(name, text, BUTTON_NORMAL, flags);
    }

    bool BackButton(const char *name, const char *text, ImGuiButtonFlags flags) {
        return TextImageButton(name, text, BUTTON_BACK, flags);
    }

    bool SettingButton(const char *name, const char *text, ImGuiButtonFlags flags) {
        return TextImageButton(name, text, BUTTON_SETTING, flags);
    }

    bool LevelButton(const char *name, const char *text, ImGuiButtonFlags flags) {
        return TextImageButton(name, text, BUTTON_LEVEL, flags);
    }

    bool SmallButton(const char *name, const char *text, ImGuiButtonFlags flags) {
        return TextImageButton(name, text, BUTTON_SMALL, flags);
    }

    bool LeftButton(const char *name, ImGuiButtonFlags flags) {
        return ImageButton(name, BUTTON_LEFT, flags);
    }

    bool RightButton(const char *name, ImGuiButtonFlags flags) {
        return ImageButton(name, BUTTON_RIGHT, flags);
    }

    bool PlusButton(const char *name, ImGuiButtonFlags flags) {
        return ImageButton(name, BUTTON_PLUS, flags);
    }

    bool MinusButton(const char *name, ImGuiButtonFlags flags) {
        return ImageButton(name, BUTTON_MINUS, flags);
    }
}