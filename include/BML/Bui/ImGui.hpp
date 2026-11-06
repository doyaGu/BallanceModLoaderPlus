#ifndef BML_BUI_IMGUI_HPP
#define BML_BUI_IMGUI_HPP
// Optional native ImGui/CK ABI; link BML::ImGui and BML::Engine.
#include "BML/Bui.hpp"
#include "BML/ImGui.hpp"
#include "BML/Engine.hpp"
#include <imgui_internal.h>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
namespace Bui {
inline ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key) {
    switch (key) {
    case CKKEY_TAB:
        return ImGuiKey_Tab;
    case CKKEY_LEFT:
        return ImGuiKey_LeftArrow;
    case CKKEY_RIGHT:
        return ImGuiKey_RightArrow;
    case CKKEY_UP:
        return ImGuiKey_UpArrow;
    case CKKEY_DOWN:
        return ImGuiKey_DownArrow;
    case CKKEY_PRIOR:
        return ImGuiKey_PageUp;
    case CKKEY_NEXT:
        return ImGuiKey_PageDown;
    case CKKEY_HOME:
        return ImGuiKey_Home;
    case CKKEY_END:
        return ImGuiKey_End;
    case CKKEY_INSERT:
        return ImGuiKey_Insert;
    case CKKEY_DELETE:
        return ImGuiKey_Delete;
    case CKKEY_BACK:
        return ImGuiKey_Backspace;
    case CKKEY_SPACE:
        return ImGuiKey_Space;
    case CKKEY_RETURN:
        return ImGuiKey_Enter;
    case CKKEY_ESCAPE:
        return ImGuiKey_Escape;
    case CKKEY_APOSTROPHE:
        return ImGuiKey_Apostrophe;
    case CKKEY_COMMA:
        return ImGuiKey_Comma;
    case CKKEY_MINUS:
        return ImGuiKey_Minus;
    case CKKEY_PERIOD:
        return ImGuiKey_Period;
    case CKKEY_SLASH:
        return ImGuiKey_Slash;
    case CKKEY_SEMICOLON:
        return ImGuiKey_Semicolon;
    case CKKEY_EQUALS:
        return ImGuiKey_Equal;
    case CKKEY_LBRACKET:
        return ImGuiKey_LeftBracket;
    case CKKEY_BACKSLASH:
        return ImGuiKey_Backslash;
    case CKKEY_RBRACKET:
        return ImGuiKey_RightBracket;
    case CKKEY_GRAVE:
        return ImGuiKey_GraveAccent;
    case CKKEY_CAPITAL:
        return ImGuiKey_CapsLock;
    case CKKEY_SCROLL:
        return ImGuiKey_ScrollLock;
    case CKKEY_NUMLOCK:
        return ImGuiKey_NumLock;
    case CKKEY_NUMPAD0:
        return ImGuiKey_Keypad0;
    case CKKEY_NUMPAD1:
        return ImGuiKey_Keypad1;
    case CKKEY_NUMPAD2:
        return ImGuiKey_Keypad2;
    case CKKEY_NUMPAD3:
        return ImGuiKey_Keypad3;
    case CKKEY_NUMPAD4:
        return ImGuiKey_Keypad4;
    case CKKEY_NUMPAD5:
        return ImGuiKey_Keypad5;
    case CKKEY_NUMPAD6:
        return ImGuiKey_Keypad6;
    case CKKEY_NUMPAD7:
        return ImGuiKey_Keypad7;
    case CKKEY_NUMPAD8:
        return ImGuiKey_Keypad8;
    case CKKEY_NUMPAD9:
        return ImGuiKey_Keypad9;
    case CKKEY_DECIMAL:
        return ImGuiKey_KeypadDecimal;
    case CKKEY_DIVIDE:
        return ImGuiKey_KeypadDivide;
    case CKKEY_MULTIPLY:
        return ImGuiKey_KeypadMultiply;
    case CKKEY_SUBTRACT:
        return ImGuiKey_KeypadSubtract;
    case CKKEY_ADD:
        return ImGuiKey_KeypadAdd;
    case CKKEY_NUMPADENTER:
        return ImGuiKey_KeypadEnter;
    case CKKEY_NUMPADEQUALS:
        return ImGuiKey_KeypadEqual;
    case CKKEY_LCONTROL:
        return ImGuiKey_LeftCtrl;
    case CKKEY_LSHIFT:
        return ImGuiKey_LeftShift;
    case CKKEY_LMENU:
        return ImGuiKey_LeftAlt;
    case CKKEY_LWIN:
        return ImGuiKey_LeftSuper;
    case CKKEY_RCONTROL:
        return ImGuiKey_RightCtrl;
    case CKKEY_RSHIFT:
        return ImGuiKey_RightShift;
    case CKKEY_RMENU:
        return ImGuiKey_RightAlt;
    case CKKEY_RWIN:
        return ImGuiKey_RightSuper;
    case CKKEY_APPS:
        return ImGuiKey_Menu;
    case CKKEY_0:
        return ImGuiKey_0;
    case CKKEY_1:
        return ImGuiKey_1;
    case CKKEY_2:
        return ImGuiKey_2;
    case CKKEY_3:
        return ImGuiKey_3;
    case CKKEY_4:
        return ImGuiKey_4;
    case CKKEY_5:
        return ImGuiKey_5;
    case CKKEY_6:
        return ImGuiKey_6;
    case CKKEY_7:
        return ImGuiKey_7;
    case CKKEY_8:
        return ImGuiKey_8;
    case CKKEY_9:
        return ImGuiKey_9;
    case CKKEY_A:
        return ImGuiKey_A;
    case CKKEY_B:
        return ImGuiKey_B;
    case CKKEY_C:
        return ImGuiKey_C;
    case CKKEY_D:
        return ImGuiKey_D;
    case CKKEY_E:
        return ImGuiKey_E;
    case CKKEY_F:
        return ImGuiKey_F;
    case CKKEY_G:
        return ImGuiKey_G;
    case CKKEY_H:
        return ImGuiKey_H;
    case CKKEY_I:
        return ImGuiKey_I;
    case CKKEY_J:
        return ImGuiKey_J;
    case CKKEY_K:
        return ImGuiKey_K;
    case CKKEY_L:
        return ImGuiKey_L;
    case CKKEY_M:
        return ImGuiKey_M;
    case CKKEY_N:
        return ImGuiKey_N;
    case CKKEY_O:
        return ImGuiKey_O;
    case CKKEY_P:
        return ImGuiKey_P;
    case CKKEY_Q:
        return ImGuiKey_Q;
    case CKKEY_R:
        return ImGuiKey_R;
    case CKKEY_S:
        return ImGuiKey_S;
    case CKKEY_T:
        return ImGuiKey_T;
    case CKKEY_U:
        return ImGuiKey_U;
    case CKKEY_V:
        return ImGuiKey_V;
    case CKKEY_W:
        return ImGuiKey_W;
    case CKKEY_X:
        return ImGuiKey_X;
    case CKKEY_Y:
        return ImGuiKey_Y;
    case CKKEY_Z:
        return ImGuiKey_Z;
    case CKKEY_F1:
        return ImGuiKey_F1;
    case CKKEY_F2:
        return ImGuiKey_F2;
    case CKKEY_F3:
        return ImGuiKey_F3;
    case CKKEY_F4:
        return ImGuiKey_F4;
    case CKKEY_F5:
        return ImGuiKey_F5;
    case CKKEY_F6:
        return ImGuiKey_F6;
    case CKKEY_F7:
        return ImGuiKey_F7;
    case CKKEY_F8:
        return ImGuiKey_F8;
    case CKKEY_F9:
        return ImGuiKey_F9;
    case CKKEY_F10:
        return ImGuiKey_F10;
    case CKKEY_F11:
        return ImGuiKey_F11;
    case CKKEY_F12:
        return ImGuiKey_F12;
    default:
        return ImGuiKey_None;
    }
}

inline CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key) {
    switch (key) {
    case ImGuiKey_Tab:
        return CKKEY_TAB;
    case ImGuiKey_LeftArrow:
        return CKKEY_LEFT;
    case ImGuiKey_RightArrow:
        return CKKEY_RIGHT;
    case ImGuiKey_UpArrow:
        return CKKEY_UP;
    case ImGuiKey_DownArrow:
        return CKKEY_DOWN;
    case ImGuiKey_PageUp:
        return CKKEY_PRIOR;
    case ImGuiKey_PageDown:
        return CKKEY_NEXT;
    case ImGuiKey_Home:
        return CKKEY_HOME;
    case ImGuiKey_End:
        return CKKEY_END;
    case ImGuiKey_Insert:
        return CKKEY_INSERT;
    case ImGuiKey_Delete:
        return CKKEY_DELETE;
    case ImGuiKey_Backspace:
        return CKKEY_BACK;
    case ImGuiKey_Space:
        return CKKEY_SPACE;
    case ImGuiKey_Enter:
        return CKKEY_RETURN;
    case ImGuiKey_Escape:
        return CKKEY_ESCAPE;
    case ImGuiKey_Apostrophe:
        return CKKEY_APOSTROPHE;
    case ImGuiKey_Comma:
        return CKKEY_COMMA;
    case ImGuiKey_Minus:
        return CKKEY_MINUS;
    case ImGuiKey_Period:
        return CKKEY_PERIOD;
    case ImGuiKey_Slash:
        return CKKEY_SLASH;
    case ImGuiKey_Semicolon:
        return CKKEY_SEMICOLON;
    case ImGuiKey_Equal:
        return CKKEY_EQUALS;
    case ImGuiKey_LeftBracket:
        return CKKEY_LBRACKET;
    case ImGuiKey_Backslash:
        return CKKEY_BACKSLASH;
    case ImGuiKey_RightBracket:
        return CKKEY_RBRACKET;
    case ImGuiKey_GraveAccent:
        return CKKEY_GRAVE;
    case ImGuiKey_CapsLock:
        return CKKEY_CAPITAL;
    case ImGuiKey_ScrollLock:
        return CKKEY_SCROLL;
    case ImGuiKey_NumLock:
        return CKKEY_NUMLOCK;
    case ImGuiKey_Keypad0:
        return CKKEY_NUMPAD0;
    case ImGuiKey_Keypad1:
        return CKKEY_NUMPAD1;
    case ImGuiKey_Keypad2:
        return CKKEY_NUMPAD2;
    case ImGuiKey_Keypad3:
        return CKKEY_NUMPAD3;
    case ImGuiKey_Keypad4:
        return CKKEY_NUMPAD4;
    case ImGuiKey_Keypad5:
        return CKKEY_NUMPAD5;
    case ImGuiKey_Keypad6:
        return CKKEY_NUMPAD6;
    case ImGuiKey_Keypad7:
        return CKKEY_NUMPAD7;
    case ImGuiKey_Keypad8:
        return CKKEY_NUMPAD8;
    case ImGuiKey_Keypad9:
        return CKKEY_NUMPAD9;
    case ImGuiKey_KeypadDecimal:
        return CKKEY_DECIMAL;
    case ImGuiKey_KeypadDivide:
        return CKKEY_DIVIDE;
    case ImGuiKey_KeypadMultiply:
        return CKKEY_MULTIPLY;
    case ImGuiKey_KeypadSubtract:
        return CKKEY_SUBTRACT;
    case ImGuiKey_KeypadAdd:
        return CKKEY_ADD;
    case ImGuiKey_KeypadEnter:
        return CKKEY_NUMPADENTER;
    case ImGuiKey_KeypadEqual:
        return CKKEY_NUMPADEQUALS;
    case ImGuiKey_LeftCtrl:
        return CKKEY_LCONTROL;
    case ImGuiKey_LeftShift:
        return CKKEY_LSHIFT;
    case ImGuiKey_LeftAlt:
        return CKKEY_LMENU;
    case ImGuiKey_LeftSuper:
        return CKKEY_LWIN;
    case ImGuiKey_RightCtrl:
        return CKKEY_RCONTROL;
    case ImGuiKey_RightShift:
        return CKKEY_RSHIFT;
    case ImGuiKey_RightAlt:
        return CKKEY_RMENU;
    case ImGuiKey_RightSuper:
        return CKKEY_RWIN;
    case ImGuiKey_Menu:
        return CKKEY_APPS;
    case ImGuiKey_0:
        return CKKEY_0;
    case ImGuiKey_1:
        return CKKEY_1;
    case ImGuiKey_2:
        return CKKEY_2;
    case ImGuiKey_3:
        return CKKEY_3;
    case ImGuiKey_4:
        return CKKEY_4;
    case ImGuiKey_5:
        return CKKEY_5;
    case ImGuiKey_6:
        return CKKEY_6;
    case ImGuiKey_7:
        return CKKEY_7;
    case ImGuiKey_8:
        return CKKEY_8;
    case ImGuiKey_9:
        return CKKEY_9;
    case ImGuiKey_A:
        return CKKEY_A;
    case ImGuiKey_B:
        return CKKEY_B;
    case ImGuiKey_C:
        return CKKEY_C;
    case ImGuiKey_D:
        return CKKEY_D;
    case ImGuiKey_E:
        return CKKEY_E;
    case ImGuiKey_F:
        return CKKEY_F;
    case ImGuiKey_G:
        return CKKEY_G;
    case ImGuiKey_H:
        return CKKEY_H;
    case ImGuiKey_I:
        return CKKEY_I;
    case ImGuiKey_J:
        return CKKEY_J;
    case ImGuiKey_K:
        return CKKEY_K;
    case ImGuiKey_L:
        return CKKEY_L;
    case ImGuiKey_M:
        return CKKEY_M;
    case ImGuiKey_N:
        return CKKEY_N;
    case ImGuiKey_O:
        return CKKEY_O;
    case ImGuiKey_P:
        return CKKEY_P;
    case ImGuiKey_Q:
        return CKKEY_Q;
    case ImGuiKey_R:
        return CKKEY_R;
    case ImGuiKey_S:
        return CKKEY_S;
    case ImGuiKey_T:
        return CKKEY_T;
    case ImGuiKey_U:
        return CKKEY_U;
    case ImGuiKey_V:
        return CKKEY_V;
    case ImGuiKey_W:
        return CKKEY_W;
    case ImGuiKey_X:
        return CKKEY_X;
    case ImGuiKey_Y:
        return CKKEY_Y;
    case ImGuiKey_Z:
        return CKKEY_Z;
    case ImGuiKey_F1:
        return CKKEY_F1;
    case ImGuiKey_F2:
        return CKKEY_F2;
    case ImGuiKey_F3:
        return CKKEY_F3;
    case ImGuiKey_F4:
        return CKKEY_F4;
    case ImGuiKey_F5:
        return CKKEY_F5;
    case ImGuiKey_F6:
        return CKKEY_F6;
    case ImGuiKey_F7:
        return CKKEY_F7;
    case ImGuiKey_F8:
        return CKKEY_F8;
    case ImGuiKey_F9:
        return CKKEY_F9;
    case ImGuiKey_F10:
        return CKKEY_F10;
    case ImGuiKey_F11:
        return CKKEY_F11;
    case ImGuiKey_F12:
        return CKKEY_F12;
    default:
        return static_cast<CKKEYBOARD>(0);
    }
}

namespace ImGuiDetail {
struct ColorPickerArea {
    ImVec2 minimum{}, maximum{};
};
inline ImVec2 ButtonSize(const BML::Mod::Context &c, ButtonType kind) {
    auto s = Bui::GetButtonSize(c, kind);
    return {s.x, s.y};
}
inline float ButtonIndent(const BML::Mod::Context &c, ButtonType kind) {
    return Bui::GetButtonIndent(c, kind);
}
inline void ClickSound(const BML::Mod::Context &c) { Bui::PlayMenuClickSound(c); }
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImRect &bb,
                           ButtonType kind, int state) {
    auto *api = BML::ImGui::Detail::Api();
    if (api)
        Bui::Detail::Report(c, api->AddButtonImage(c.Handle(), list, {bb.Min.x, bb.Min.y}, kind,
                                                   state, "", {.5f, .5f}));
}
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImRect &bb,
                           ButtonType kind, bool selected) {
    AddButtonImage(c, list, bb, kind, selected ? 1 : 0);
}
inline bool KeyChordToString(const BML::Mod::Context &context, ImGuiKeyChord keyChord, char *buffer,
                             std::size_t bufferSize) {
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

inline bool SetKeyChordFromIO(const BML::Mod::Context &context, ImGuiKeyChord *keyChord) {
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
            if ((chord & ImGuiMod_Ctrl) != 0 &&
                (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl))
                continue;

            if ((chord & ImGuiMod_Shift) != 0 &&
                (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift))
                continue;

            if ((chord & ImGuiMod_Alt) != 0 &&
                (key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt))
                continue;

            if ((chord & ImGuiMod_Super) != 0 &&
                (key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper))
                continue;

            chord |= key;
            *keyChord = chord;
            return true;
        }
    }

    return false;
}

enum TextOverflowMode {
    TextOverflowEllipsis,
    TextOverflowMarqueeWhenActive,
};

static TextOverflowMode GetButtonTextOverflowMode(const BML::Mod::Context &context,
                                                  ButtonType type) {
    return type == BUTTON_MAIN || type == BUTTON_LEVEL ? TextOverflowMarqueeWhenActive
                                                       : TextOverflowEllipsis;
}

static ImRect GetButtonTextRect(const BML::Mod::Context &context, const ImRect &bb,
                                ButtonType type) {
    const float indent = ButtonIndent(context, type);
    return ImRect(ImVec2(bb.Min.x + indent, bb.Min.y), ImVec2(bb.Max.x - indent, bb.Max.y));
}

static ImRect GetButtonOverflowTextRect(const BML::Mod::Context &context, const ImRect &bb,
                                        ButtonType type) {
    ImRect textRect = GetButtonTextRect(context, bb, type);

    if (type == BUTTON_LEVEL) {
        const ImVec2 size = bb.GetSize();
        textRect.Min.x = bb.Min.x + size.x * 0.20f;
        textRect.Max.x = bb.Max.x - size.x * 0.08f;
    }

    return textRect;
}

static void RenderEllipsisText(const BML::Mod::Context &context, ImDrawList *drawList,
                               const ImVec2 &textMin, const ImVec2 &textMax, const char *text,
                               const ImVec2 *textSize, const ImVec2 &textAlign,
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

static void RenderMarqueeText(const BML::Mod::Context &context, ImDrawList *drawList,
                              const ImVec2 &textMin, const ImVec2 &textMax, const char *text,
                              const ImVec2 *textSize, bool selected, float selectedTimer,
                              const ImRect *clipRect) {
    if (!drawList || !text || text[0] == '\0' || !textSize)
        return;

    const float availableWidth = textMax.x - textMin.x;
    if (textSize->x <= availableWidth) {
        ImGui::RenderTextClipped(textMin, textMax, text, nullptr, textSize, ImVec2(0.5f, 0.5f),
                                 clipRect);
        return;
    }

    constexpr float ScrollSpeed = 45.0f;
    constexpr float ScrollGap = 36.0f;

    float textY = textMin.y + (textMax.y - textMin.y - textSize->y) * 0.5f;
    if (textY < textMin.y)
        textY = textMin.y;

    if (!selected) {
        RenderEllipsisText(context, drawList, textMin, textMax, text, textSize, ImVec2(0.5f, 0.5f),
                           clipRect);
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

static void RenderButtonText(const BML::Mod::Context &context, ImDrawList *drawList,
                             const ImRect &bb, ButtonType type, const char *text,
                             const ImVec2 &textAlign, bool selected, float selectedTimer) {
    if (!text || text[0] == '\0')
        return;

    const ImRect textRect = GetButtonTextRect(context, bb, type);
    const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, true);
    if (textSize.x <= textRect.GetWidth()) {
        ImGui::RenderTextClipped(textRect.Min, textRect.Max, text, nullptr, &textSize, textAlign,
                                 &bb);
        return;
    }

    const ImRect overflowTextRect = GetButtonOverflowTextRect(context, bb, type);
    if (selected && GetButtonTextOverflowMode(context, type) == TextOverflowMarqueeWhenActive) {
        RenderMarqueeText(context, drawList, overflowTextRect.Min, overflowTextRect.Max, text,
                          &textSize, true, selectedTimer, &overflowTextRect);
        return;
    }

    RenderEllipsisText(context, drawList, overflowTextRect.Min, overflowTextRect.Max, text,
                       &textSize, textAlign, &overflowTextRect);
}

static bool TextImageButton(const BML::Mod::Context &context, const char *label, const char *text,
                            ButtonType type, ImGuiButtonFlags flags = 0) {
    if (!label || !text || type < 0 || type >= BUTTON_COUNT)
        return false;

    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ButtonSize(context, type);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
    if (pressed)
        ClickSound(context);

    const bool active = pressed || hovered || held;
    ImGuiContext &g = *GImGui;
    const float activeTimer = held ? g.ActiveIdTimer : (hovered ? g.HoveredIdTimer : 0.0f);

    AddButtonImage(context, window->DrawList, bb, type, active);
    RenderButtonText(context, window->DrawList, bb, type, text, ImGui::GetStyle().ButtonTextAlign,
                     active, activeTimer);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed;
}

static bool SelectableTextImageButton(const BML::Mod::Context &context, const char *label,
                                      const char *text, ButtonType type, bool *selected,
                                      ImGuiButtonFlags flags = 0) {
    if (!label || !text || type < 0 || type >= BUTTON_COUNT)
        return false;

    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ButtonSize(context, type);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
    if (pressed)
        ClickSound(context);

    int state = (selected && *selected) ? 0 : 2;
    const bool active = pressed || hovered || held;
    if (active) {
        state = 1;
        if (selected)
            *selected = true;
    }

    ImGuiContext &g = *GImGui;
    const float activeTimer = held ? g.ActiveIdTimer : (hovered ? g.HoveredIdTimer : 0.0f);

    AddButtonImage(context, window->DrawList, bb, type, state);
    RenderButtonText(context, window->DrawList, bb, type, text, ImGui::GetStyle().ButtonTextAlign,
                     active, activeTimer);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable |
                                    ((selected && *selected) ? ImGuiItemStatusFlags_Checked : 0));
    return pressed;
}

static bool ImageButton(const BML::Mod::Context &context, const char *label, ButtonType type,
                        ImGuiButtonFlags flags = 0) {
    if (!label || type < 0 || type >= BUTTON_COUNT)
        return false;

    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ButtonSize(context, type);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
    if (pressed)
        ClickSound(context);

    AddButtonImage(context, window->DrawList, bb, type, pressed || hovered || held);

    ImGuiContext &g = *GImGui;
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
    return pressed;
}

inline bool KeyButton(const BML::Mod::Context &context, const char *label, bool *listening,
                      ImGuiKeyChord *keyChord) {
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
    const ImVec2 size = ButtonSize(context, BUTTON_KEY);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImGuiContext &g = *GImGui;
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Inputable |
                                    (*listening ? ImGuiItemStatusFlags_Checked : 0));

    const bool wasListening = *listening;
    bool changed = false;
    if (wasListening) {
        if ((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) ||
            SetKeyChordFromIO(context, keyChord)) {
            *listening = false;
            changed = true;
        }
    } else if (pressed) {
        *listening = true;
        ClickSound(context);
    }
    if (*listening || wasListening) {
        if (g.ActiveId != id)
            ImGui::SetActiveID(id, window);
        ImGui::SetActiveIdUsingAllKeyboardKeys();
    }

    ImDrawList *drawList = window->DrawList;
    AddButtonImage(context, drawList, bb, BUTTON_KEY, hovered);

    const float labelLeftInset = size.x * LabelLeftInsetRatio;
    const float labelRightInset = size.x * LabelRightInsetRatio;
    const ImVec2 labelMin(bb.Min.x + labelLeftInset, bb.Min.y);
    const ImVec2 labelMax(bb.Max.x - labelRightInset, bb.Max.y);
    RenderEllipsisText(context, drawList, labelMin, labelMax, label, &textSize,
                       style.ButtonTextAlign, &bb);

    if (*keyChord != 0) {
        const float chordLeftInset = size.x * ChordLeftInsetRatio;
        const float chordRightInset = size.x * ChordRightInsetRatio;
        const ImVec2 chordMin(bb.Min.x + chordLeftInset, bb.Min.y);
        const ImVec2 chordMax(bb.Max.x - chordRightInset, bb.Max.y);

        char keyText[32]{};
        if (KeyChordToString(context, *keyChord, keyText, sizeof(keyText))) {
            const ImVec2 keyTextSize = ImGui::CalcTextSize(keyText, nullptr, true);
            RenderEllipsisText(context, drawList, chordMin, chordMax, keyText, &keyTextSize,
                               style.ButtonTextAlign, &bb);
        }
    }

    if (*listening) {
        auto ck = BML::Engine::BorrowContext(context);
        auto *material = ck ? static_cast<CKMaterial *>(ck.Value()->GetObjectByNameAndClass(
                                  (CKSTRING) "M_Keys_Highlight", CKCID_MATERIAL))
                            : nullptr;
        if (material) {
            const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
            const ImVec2 minimum(bb.Min.x + vpSize.x * HighlightLeftInsetRatio, bb.Min.y);
            const ImVec2 maximum(minimum.x + vpSize.x * HighlightWidthRatio,
                                 minimum.y + vpSize.y * HighlightHeightRatio);
            drawList->AddImage(material, minimum, maximum, {HighlightUvMinX, HighlightUvMinY},
                               {HighlightUvMaxX, HighlightUvMaxY});
        }
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
    bool focused = false;
    float activeTimer = 0.0f;
};

static bool BeginOptionRow(const BML::Mod::Context &context, const char *label, OptionRow &row,
                           bool navigation = true) {
    if (!label)
        return false;

    row.window = ImGui::GetCurrentWindow();
    if (row.window->SkipItems)
        return false;

    row.label = label;
    row.id = row.window->GetID(label);
    row.position = row.window->DC.CursorPos;
    row.size = ButtonSize(context, BUTTON_OPTION);
    row.bounds =
        ImRect(row.position, ImVec2(row.position.x + row.size.x, row.position.y + row.size.y));

    ImGui::BeginGroup();
    ImGui::PushFocusScope(row.id);
    ImGui::ItemSize(row.bounds);
    const ImGuiItemFlags itemFlags = navigation ? ImGuiItemFlags_None : ImGuiItemFlags_NoNav;
    if (!ImGui::ItemAdd(row.bounds, row.id, nullptr, itemFlags)) {
        ImGui::PopFocusScope();
        ImGui::EndGroup();
        return false;
    }

    row.pressed = ImGui::ButtonBehavior(row.bounds, row.id, &row.hovered, &row.held,
                                        ImGuiButtonFlags(ImGuiButtonFlags_AllowOverlap) |
                                            ImGuiButtonFlags(ImGuiButtonFlags_FlattenChildren));
    row.focused = ImGui::IsItemFocused();
    ImGuiContext &imgui = *GImGui;
    const bool navigationVisible = imgui.NavCursorVisible && imgui.NavHighlightItemUnderNav;
    const bool controlFocused = !navigation && imgui.NavId != 0 && imgui.NavFocusScopeId == row.id;
    const bool controlSelected =
        controlFocused && (navigationVisible || imgui.ActiveId == imgui.NavId);
    const bool selected = row.pressed || row.hovered || row.held ||
                          (navigationVisible && row.focused) || controlSelected;
    row.activeTimer = row.held ? imgui.ActiveIdTimer : (row.hovered ? imgui.HoveredIdTimer : 0.0f);

    AddButtonImage(context, row.window->DrawList, row.bounds, BUTTON_OPTION, selected);
    const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);
    const float indent = ButtonIndent(context, BUTTON_OPTION);
    const ImVec2 textMin(row.bounds.Min.x + indent, row.bounds.Min.y);
    const ImVec2 textMax(row.bounds.Max.x - indent, row.bounds.Max.y);
    RenderEllipsisText(context, row.window->DrawList, textMin, textMax, label, &textSize,
                       ImVec2(0.5f, 0.21f), &row.bounds);

    row.restoreCursor = ImGui::GetCursorScreenPos();
    return true;
}

static void ReportOptionRow(const BML::Mod::Context &context, const OptionRow &row,
                            ImGuiItemStatusFlags extraStatus = 0) {
    ImGuiContext &g = *GImGui;
    IMGUI_TEST_ENGINE_ITEM_INFO(row.id, row.label, g.LastItemData.StatusFlags | extraStatus);
}

static ImGuiItemStatusFlags CheckableStatus(const BML::Mod::Context &context, bool checked) {
#ifdef IMGUI_ENABLE_TEST_ENGINE
    return ImGuiItemStatusFlags_Checkable | (checked ? ImGuiItemStatusFlags_Checked : 0);
#else
    (void)checked;
    return 0;
#endif
}

static ImGuiItemStatusFlags InputableStatus(const BML::Mod::Context &context) {
#ifdef IMGUI_ENABLE_TEST_ENGINE
    return ImGuiItemStatusFlags_Inputable;
#else
    return 0;
#endif
}

static void EndOptionRow(const BML::Mod::Context &context, const OptionRow &row) {
    ImGui::SetCursorScreenPos(row.restoreCursor);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PopFocusScope();
    ImGui::EndGroup();
}

static void BeginOptionInput(const BML::Mod::Context &context, const OptionRow &row) {
    ImGui::SetCursorScreenPos(
        ImVec2(row.position.x + row.size.x * 0.24f, row.position.y + row.size.y * 0.45f));
    ImGui::SetNextItemWidth(row.size.x * 0.6f);
    ImGui::PushID(row.label);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, IM_COL32(0, 0, 0, 0));
}

static void EndOptionInput(const BML::Mod::Context &context, const OptionRow &row) {
    ImGui::PopStyleColor();
    ImGui::PopID();
    EndOptionRow(context, row);
}

inline bool InputTextButton(const BML::Mod::Context &context, const char *label, char *buffer,
                            std::size_t bufferSize, ImGuiInputTextFlags flags,
                            ImGuiInputTextCallback callback, void *userData) {
    if (!buffer || bufferSize == 0)
        return false;

    OptionRow row;
    if (!BeginOptionRow(context, label, row, false))
        return false;
    ReportOptionRow(context, row, InputableStatus(context));

    BeginOptionInput(context, row);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
    const bool changed =
        ImGui::InputText("##InputText", buffer, bufferSize, flags, callback, userData);
    ImGui::PopStyleColor();
    EndOptionInput(context, row);
    return changed;
}

inline bool InputFloatButton(const BML::Mod::Context &context, const char *label, float *value,
                             float step, float stepFast, const char *format,
                             ImGuiInputTextFlags flags) {
    if (!value || !format)
        return false;

    OptionRow row;
    if (!BeginOptionRow(context, label, row, false))
        return false;
    ReportOptionRow(context, row, InputableStatus(context));

    BeginOptionInput(context, row);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
    const bool changed = ImGui::InputFloat("##InputFloat", value, step, stepFast, format, flags);
    ImGui::PopStyleColor();
    EndOptionInput(context, row);
    return changed;
}

inline bool InputIntButton(const BML::Mod::Context &context, const char *label, int *value,
                           int step, int stepFast, ImGuiInputTextFlags flags) {
    if (!value)
        return false;

    OptionRow row;
    if (!BeginOptionRow(context, label, row, false))
        return false;
    ReportOptionRow(context, row, InputableStatus(context));

    BeginOptionInput(context, row);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.63f, 0.32f, 0.18f, 0.57f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.63f, 0.32f, 0.18f, 0.57f));
    const bool changed = ImGui::InputInt("##InputInt", value, step, stepFast, flags);
    ImGui::PopStyleColor(4);
    EndOptionInput(context, row);
    return changed;
}

struct ColorPickerLayout {
    ImVec2 position;
    ImVec2 size;
    float contentWidth = 0.0f;
};

constexpr ImGuiColorEditFlags DefaultColorEditFlags =
    ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_PickerHueBar |
    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;

static ColorPickerLayout FitColorPicker(const BML::Mod::Context &context, const ImRect &anchor,
                                        ImGuiColorEditFlags flags,
                                        const ColorPickerArea *pickerArea) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 areaMin = viewport->WorkPos;
    ImVec2 areaMax(viewport->WorkPos.x + viewport->WorkSize.x,
                   viewport->WorkPos.y + viewport->WorkSize.y);
    if (pickerArea && pickerArea->maximum.x > pickerArea->minimum.x &&
        pickerArea->maximum.y > pickerArea->minimum.y) {
        const ImVec2 clippedMin(std::max(areaMin.x, pickerArea->minimum.x),
                                std::max(areaMin.y, pickerArea->minimum.y));
        const ImVec2 clippedMax(std::min(areaMax.x, pickerArea->maximum.x),
                                std::min(areaMax.y, pickerArea->maximum.y));
        if (clippedMax.x > clippedMin.x && clippedMax.y > clippedMin.y) {
            areaMin = clippedMin;
            areaMax = clippedMax;
        }
    }

    constexpr float margin = 8.0f;
    if (areaMax.x - areaMin.x > margin * 2.0f) {
        areaMin.x += margin;
        areaMax.x -= margin;
    }
    if (areaMax.y - areaMin.y > margin * 2.0f) {
        areaMin.y += margin;
        areaMax.y -= margin;
    }

    const ImGuiStyle &style = ImGui::GetStyle();
    const float frameHeight = ImGui::GetFrameHeight();
    const float availableWidth = std::max(1.0f, areaMax.x - areaMin.x);
    const float availableHeight = std::max(1.0f, areaMax.y - areaMin.y);
    const float horizontalPadding = style.WindowPadding.x * 2.0f;
    const float verticalPadding = style.WindowPadding.y * 2.0f;
    const bool alphaBar =
        ((flags & ImGuiColorEditFlags_AlphaBar) != 0 ||
         ((flags & ImGuiColorEditFlags_NoOptions) == 0 &&
          (ImGui::GetIO().ConfigColorEditFlags & ImGuiColorEditFlags_AlphaBar) != 0)) &&
        (flags & ImGuiColorEditFlags_NoAlpha) == 0;
    const float barSpace = (alphaBar ? 2.0f : 1.0f) * (frameHeight + style.ItemInnerSpacing.x);
    const ImGuiColorEditFlags display = flags & ImGuiColorEditFlags_DisplayMask_;
    const int inputRows = display == 0 ? 3 : 1;
    const float inputHeight = inputRows * frameHeight + inputRows * style.ItemSpacing.y;

    float contentWidth =
        std::min(frameHeight * 11.0f, std::max(1.0f, availableWidth - horizontalPadding));
    const float maximumSquareHeight =
        std::max(frameHeight, availableHeight - verticalPadding - inputHeight);
    contentWidth = std::min(contentWidth, maximumSquareHeight + barSpace);
    const float squareHeight = std::max(frameHeight, contentWidth - barSpace);
    const float contentHeight = squareHeight + inputHeight;

    ColorPickerLayout layout;
    layout.size.x = std::min(availableWidth, contentWidth + horizontalPadding);
    layout.size.y = std::min(availableHeight, contentHeight + verticalPadding);
    layout.contentWidth = std::max(1.0f, layout.size.x - horizontalPadding);

    const float maximumX = std::max(areaMin.x, areaMax.x - layout.size.x);
    layout.position.x =
        std::clamp(anchor.GetCenter().x - layout.size.x * 0.5f, areaMin.x, maximumX);
    const float gap = style.ItemSpacing.y;
    const float below = anchor.Max.y + gap;
    const float above = anchor.Min.y - gap - layout.size.y;
    if (below + layout.size.y <= areaMax.y)
        layout.position.y = below;
    else if (above >= areaMin.y)
        layout.position.y = above;
    else
        layout.position.y = std::clamp(anchor.GetCenter().y - layout.size.y * 0.5f, areaMin.y,
                                       std::max(areaMin.y, areaMax.y - layout.size.y));
    return layout;
}

static bool DrawColorButton(const BML::Mod::Context &context, const char *label, ImVec4 *color,
                            ImGuiColorEditFlags flags, const ColorPickerArea *pickerArea) {
    if (!color)
        return false;

    OptionRow row;
    if (!BeginOptionRow(context, label, row, false))
        return false;
    ReportOptionRow(context, row, InputableStatus(context));

    BeginOptionInput(context, row);
    const bool showEditor = (flags & ImGuiColorEditFlags_NoInputs) == 0;
    const bool showSwatch = (flags & ImGuiColorEditFlags_NoSmallPreview) == 0;
    const float swatchSize = ImGui::GetFrameHeight();
    const float editorWidth = row.size.x * 0.6f;
    if (showEditor && showSwatch) {
        ImGui::SetNextItemWidth(
            std::max(1.0f, editorWidth - swatchSize - ImGui::GetStyle().ItemInnerSpacing.x));
    } else if (showEditor) {
        ImGui::SetNextItemWidth(editorWidth);
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));
    bool changed = showEditor && ImGui::ColorEdit4("##ColorValue", &color->x,
                                                   flags | ImGuiColorEditFlags_NoLabel |
                                                       ImGuiColorEditFlags_NoPicker |
                                                       ImGuiColorEditFlags_NoSmallPreview);
    ImGui::PopStyleColor();

    ImRect swatch;
    bool swatchPressed = false;
    if (showSwatch) {
        if (showEditor)
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        swatchPressed =
            ImGui::ColorButton("##ColorSwatch", *color, flags, ImVec2(swatchSize, swatchSize));
        swatch = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }

    constexpr const char *popupId = "##ColorPicker";
    if (swatchPressed && (flags & ImGuiColorEditFlags_NoPicker) == 0)
        ImGui::OpenPopup(popupId);
    if (ImGui::IsPopupOpen(popupId)) {
        const ColorPickerLayout layout = FitColorPicker(context, swatch, flags, pickerArea);
        ImGui::SetNextWindowPos(layout.position, ImGuiCond_Always);
        ImGui::SetNextWindowSize(layout.size, ImGuiCond_Always);
        if (ImGui::BeginPopup(popupId, ImGuiWindowFlags_NoSavedSettings)) {
            constexpr ImGuiColorEditFlags forwarded =
                ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_PickerMask_ |
                ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_DisplayMask_ |
                ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha |
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoOptions |
                ImGuiColorEditFlags_NoTooltip;
            ImGui::SetNextItemWidth(layout.contentWidth);
            changed |= ImGui::ColorPicker4("##Picker", &color->x,
                                           (flags & forwarded) | ImGuiColorEditFlags_NoLabel |
                                               ImGuiColorEditFlags_NoSidePreview);
            ImGui::EndPopup();
        }
    }

#ifdef IMGUI_ENABLE_TEST_ENGINE
    const ImGuiID swatchId = ImGui::GetID("##ColorSwatch");
    const bool pickerOpen = ImGui::IsPopupOpen(popupId);
    ImGuiContext &g = *GImGui;
    IMGUI_TEST_ENGINE_ITEM_INFO(swatchId, "ColorSwatch",
                                ImGuiItemStatusFlags_Openable |
                                    (pickerOpen ? ImGuiItemStatusFlags_Opened : 0));
#endif
    EndOptionInput(context, row);
    return changed;
}

inline bool ColorButton(const BML::Mod::Context &context, const char *label, ImVec4 *color,
                        ImGuiColorEditFlags flags) {
    return DrawColorButton(context, label, color, flags, nullptr);
}

} // namespace ImGuiDetail
inline bool KeyChordToString(const BML::Mod::Context &c, ImGuiKeyChord chord, char *b, size_t n) {
    return ImGuiDetail::KeyChordToString(c, chord, b, n);
}
inline bool SetKeyChordFromIO(const BML::Mod::Context &c, ImGuiKeyChord *chord) {
    return ImGuiDetail::SetKeyChordFromIO(c, chord);
}
inline bool KeyButton(const BML::Mod::Context &c, const char *label, bool *listening,
                      ImGuiKeyChord *chord) {
    return ImGuiDetail::KeyButton(c, label, listening, chord);
}
inline bool MainButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::TextImageButton(c, label, label, BUTTON_MAIN, flags);
}
inline bool OkButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::TextImageButton(c, label, "OK", BUTTON_BACK, flags);
}
inline bool BackButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::TextImageButton(c, label, "Back", BUTTON_BACK, flags);
}
inline bool OptionButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::TextImageButton(c, label, label, BUTTON_OPTION, flags);
}
inline bool LevelButton(const BML::Mod::Context &c, const char *label, bool *selected,
                        ImGuiButtonFlags flags) {
    return ImGuiDetail::SelectableTextImageButton(c, label, label, BUTTON_LEVEL, selected, flags);
}
inline bool SmallButton(const BML::Mod::Context &c, const char *label, bool *selected,
                        ImGuiButtonFlags flags) {
    return ImGuiDetail::SelectableTextImageButton(c, label, label, BUTTON_SMALL, selected, flags);
}
inline bool LeftButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::ImageButton(c, label, BUTTON_LEFT, flags);
}
inline bool RightButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::ImageButton(c, label, BUTTON_RIGHT, flags);
}
inline bool PlusButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::ImageButton(c, label, BUTTON_PLUS, flags);
}
inline bool MinusButton(const BML::Mod::Context &c, const char *label, ImGuiButtonFlags flags) {
    return ImGuiDetail::ImageButton(c, label, BUTTON_MINUS, flags);
}
inline bool InputTextButton(const BML::Mod::Context &c, const char *label, char *b, size_t n,
                            ImGuiInputTextFlags flags, ImGuiInputTextCallback callback = nullptr,
                            void *userdata = nullptr) {
    return ImGuiDetail::InputTextButton(c, label, b, n, flags, callback, userdata);
}
inline bool InputFloatButton(const BML::Mod::Context &c, const char *label, float *v, float step,
                             float fast, const char *format, ImGuiInputTextFlags flags) {
    return ImGuiDetail::InputFloatButton(c, label, v, step, fast, format, flags);
}
inline bool InputIntButton(const BML::Mod::Context &c, const char *label, int *v, int step,
                           int fast, ImGuiInputTextFlags flags) {
    return ImGuiDetail::InputIntButton(c, label, v, step, fast, flags);
}
inline bool ColorButton(const BML::Mod::Context &c, const char *label, ImVec4 *v,
                        ImGuiColorEditFlags flags = ImGuiDetail::DefaultColorEditFlags) {
    return ImGuiDetail::ColorButton(c, label, v, flags);
}
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImVec2 &p,
                           ButtonType t, int state, const char *text, const ImVec2 &align) {
    auto *api = BML::ImGui::Detail::Api();
    if (api)
        Bui::Detail::Report(c, api->AddButtonImage(c.Handle(), list, {p.x, p.y}, t, state,
                                                   text ? text : "", {align.x, align.y}));
}
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImVec2 &p,
                           ButtonType t, int state, const char *text) {
    AddButtonImage(c, list, p, t, state, text, ImGui::GetStyle().ButtonTextAlign);
}
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImVec2 &p,
                           ButtonType t, int state) {
    AddButtonImage(c, list, p, t, state, "");
}
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImVec2 &p,
                           ButtonType t, bool selected) {
    AddButtonImage(c, list, p, t, selected ? 1 : 0);
}
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImVec2 &p,
                           ButtonType t, bool selected, const char *text) {
    AddButtonImage(c, list, p, t, selected ? 1 : 0, text);
}
inline void AddButtonImage(const BML::Mod::Context &c, ImDrawList *list, const ImVec2 &p,
                           ButtonType t, bool selected, const char *text, const ImVec2 &align) {
    AddButtonImage(c, list, p, t, selected ? 1 : 0, text, align);
}
class ImGuiContextScope {
  public:
    explicit ImGuiContextScope(const BML::Mod::Context &c, ImGuiContext *context = nullptr)
        : m_Previous(ImGui::GetCurrentContext()) {
        auto r = BML::ImGui::BorrowContext(c);
        ImGui::SetCurrentContext(context ? context : r ? r.Value() : nullptr);
    }
    ~ImGuiContextScope() { ImGui::SetCurrentContext(m_Previous); }
    ImGuiContextScope(const ImGuiContextScope &) = delete;
    ImGuiContextScope &operator=(const ImGuiContextScope &) = delete;

  private:
    ImGuiContext *m_Previous;
};
inline CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename,
                              int slot = 0) {
    if (!context || !id || !filename)
        return nullptr;
    XString path((CKSTRING)filename);
    auto *paths = context->GetPathManager();
    if (!paths || paths->ResolveFileName(path, BITMAP_PATH_IDX) != CK_OK)
        return nullptr;
    auto *texture = static_cast<CKTexture *>(context->CreateObject(CKCID_TEXTURE, (CKSTRING)id));
    if (!texture)
        return nullptr;
    if (!texture->LoadImage(path.Str(), slot)) {
        context->DestroyObject(texture);
        return nullptr;
    }
    texture->SetDesiredVideoFormat(_32_ARGB8888);
    return texture;
}
} // namespace Bui
#endif
