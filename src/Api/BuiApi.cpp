#include "Api/BuiApi.h"
#include "Api/ModApi.h"
#include "UI/BuiInternal.h"
#include "UI/Overlay.h"
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include "imgui_internal.h"

namespace BML::Api {
namespace {
bool Finite(BML_Vec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
ImGuiInputTextFlags InputFlags(uint32_t flags) {
    return ((flags & BML_BUI_INPUT_READ_ONLY) ? ImGuiInputTextFlags_ReadOnly : 0) |
           ((flags & BML_BUI_INPUT_PASSWORD) ? ImGuiInputTextFlags_Password : 0) |
           ((flags & BML_BUI_INPUT_ENTER_RETURNS_TRUE) ? ImGuiInputTextFlags_EnterReturnsTrue : 0);
}
template <class F> int WithUi(BML_ModContextHandle h, F &&body) {
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        auto *imgui = Overlay::GetImGuiContext();
        if (!imgui || !imgui->WithinFrameScope)
            return BML_ERROR_FROZEN;
        Overlay::ImGuiContextScope scope;
        return body(ctx, owner);
    });
}
int BML_CDECL MenuStyle(BML_ModContextHandle h, BML_Vec2 *p, BML_Vec2 *s, BML_Color *c) {
    if (!p || !s || !c)
        return BML_ERROR_INVALID_PARAMETER;
    return WithUi(h, [&](ModContext &, IMod &) {
        auto pos = BuiHost::GetMenuPos();
        auto size = BuiHost::GetMenuSize();
        auto color = BuiHost::GetMenuColor();
        *p = {pos.x, pos.y};
        *s = {size.x, size.y};
        *c = {color.x, color.y, color.z, color.w};
        return BML_OK;
    });
}
int BML_CDECL ButtonStyle(BML_ModContextHandle h, uint32_t kind, BML_Vec2 *s, float *indent) {
    if (!s || !indent || kind >= BuiHost::BUTTON_COUNT)
        return BML_ERROR_INVALID_PARAMETER;
    return WithUi(h, [&](ModContext &, IMod &) {
        auto type = static_cast<BuiHost::ButtonType>(kind);
        auto size = BuiHost::GetButtonSize(type);
        *s = {size.x, size.y};
        *indent = BuiHost::GetButtonIndent(type);
        return BML_OK;
    });
}
int BML_CDECL Begin(BML_ModContextHandle h, const char *id, const char *title, uint32_t *open,
                    uint32_t flags, uint32_t *draw) {
    if (!id || !*id || !title || !draw || (open && *open > 1) || (flags & ~511u))
        return BML_ERROR_INVALID_PARAMETER;
    *draw = 0;
    return WithUi(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetBuiApi().Begin(ctx, owner, id, title, open, flags, draw);
    });
}
int BML_CDECL End(BML_ModContextHandle h) {
    return WithUi(h, [&](ModContext &ctx, IMod &owner) { return ctx.GetBuiApi().End(ctx, owner); });
}
int BML_CDECL At(BML_ModContextHandle h, BML_Vec2 pos, uint32_t pixels, BML_BuiDrawCallback cb,
                 void *data) {
    if (!cb || pixels > 1 || !Finite(pos))
        return BML_ERROR_INVALID_PARAMETER;
    return WithUi(h, [&](ModContext &, IMod &) {
        auto *g = ImGui::GetCurrentContext();
        auto *window = ImGui::GetCurrentWindow();
        const auto saved = ImGui::GetCursorScreenPos();
        const auto *vp = ImGui::GetMainViewport();
        const int depth = g->CurrentWindowStack.Size;
        const auto before = Overlay::CaptureImGuiState();
        ImGui::SetCursorScreenPos(
            pixels ? ImVec2(pos.x, pos.y)
                   : ImVec2(vp->Pos.x + vp->Size.x * pos.x, vp->Pos.y + vp->Size.y * pos.y));
        int code = BML_ERROR_FAIL;
        try {
            code = cb(data);
        } catch (...) {
            code = BML_ERROR_FAIL;
        }
        // The callback may use nested, paired windows. Never write a cursor into
        // an unrelated window after a malformed C callback.
        if (g->CurrentWindowStack.Size != depth || ImGui::GetCurrentWindow() != window) {
            Overlay::RecoverImGuiState(before);
            if (g->CurrentWindowStack.Size == depth && ImGui::GetCurrentWindow() == window) {
                ImGui::SetCursorScreenPos(saved);
                ImGui::Dummy({0, 0});
            }
            return BML_ERROR_FAIL;
        }
        ImGui::SetCursorScreenPos(saved);
        ImGui::Dummy({0, 0});
        return code;
    });
}
int BML_CDECL Button(BML_ModContextHandle h, uint32_t kind, const char *label, uint32_t flags,
                     uint32_t *selected, uint32_t *clicked) {
    if (!label || !clicked || kind >= BuiHost::BUTTON_COUNT || (flags & ~7u) ||
        (flags & 3u) == 3u || (selected && *selected > 1))
        return BML_ERROR_INVALID_PARAMETER;
    *clicked = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        ImGuiButtonFlags f = ((flags & 1u) ? ImGuiButtonFlags_PressedOnClick : 0) |
                             ((flags & 2u) ? ImGuiButtonFlags_PressedOnRelease : 0);
        bool choice = selected && *selected;
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, (flags & 4u) != 0);
        const bool result = BuiHost::DrawButton(static_cast<BuiHost::ButtonType>(kind), label,
                                                selected ? &choice : nullptr, f);
        ImGui::PopItemFlag();
        if (selected)
            *selected = choice;
        *clicked = result;
        return BML_OK;
    });
}
int BML_CDECL Navigate(BML_ModContextHandle h, uint32_t kind, BML_Vec2 p, uint32_t *clicked) {
    if (!clicked || !Finite(p) ||
        (kind != BML_BUI_LEFT && kind != BML_BUI_RIGHT && kind != BML_BUI_BACK))
        return BML_ERROR_INVALID_PARAMETER;
    *clicked = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        *clicked = kind == BML_BUI_LEFT    ? BuiHost::NavLeft(p.x, p.y)
                   : kind == BML_BUI_RIGHT ? BuiHost::NavRight(p.x, p.y)
                                           : BuiHost::NavBack(p.x, p.y);
        return BML_OK;
    });
}
int BML_CDECL Key(BML_ModContextHandle h, const char *label, uint32_t *listening, BML_KeyChord *key,
                  uint32_t *changed) {
    if (!label || !listening || !key || !changed || *listening > 1 || key->Key > 255 ||
        (key->Modifiers & ~15u))
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        bool active = *listening != 0;
        ImGuiKeyChord chord = BuiHost::CKKeyToImGuiKey(static_cast<CKKEYBOARD>(key->Key));
        if (key->Modifiers & 1)
            chord |= ImGuiMod_Shift;
        if (key->Modifiers & 2)
            chord |= ImGuiMod_Ctrl;
        if (key->Modifiers & 4)
            chord |= ImGuiMod_Alt;
        if (key->Modifiers & 8)
            chord |= ImGuiMod_Super;
        *changed = BuiHost::KeyButton(label, &active, &chord);
        *listening = active;
        key->Key = BuiHost::ImGuiKeyToCKKey(static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_));
        key->Modifiers = ((chord & ImGuiMod_Shift) ? 1u : 0u) |
                         ((chord & ImGuiMod_Ctrl) ? 2u : 0u) | ((chord & ImGuiMod_Alt) ? 4u : 0u) |
                         ((chord & ImGuiMod_Super) ? 8u : 0u);
        return BML_OK;
    });
}
int BML_CDECL YesNo(BML_ModContextHandle h, const char *label, uint32_t *v, uint32_t *changed) {
    if (!label || !v || *v > 1 || !changed)
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        bool value = *v != 0;
        *changed = BuiHost::YesNoButton(label, &value);
        *v = value;
        return BML_OK;
    });
}
int BML_CDECL Radio(BML_ModContextHandle h, const char *label, int32_t *v, const char *const *items,
                    size_t count, uint32_t *changed) {
    if (!label || !v || !changed || (!items && count) || count > INT_MAX)
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        *changed = BuiHost::RadioButton(label, v, items, static_cast<int>(count));
        return BML_OK;
    });
}
int BML_CDECL Text(BML_ModContextHandle h, const char *label, char *b, size_t n, uint32_t flags,
                   uint32_t *changed) {
    if (!label || !b || !n || !changed || (flags & ~7u) || !std::memchr(b, 0, n))
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        *changed = BuiHost::InputTextButton(label, b, n, InputFlags(flags));
        return BML_OK;
    });
}
int BML_CDECL Float(BML_ModContextHandle h, const char *label, float *v, float step, float fast,
                    const char *format, uint32_t flags, uint32_t *changed) {
    if (!label || !v || !format || !changed || (flags & ~5u) || !std::isfinite(*v) ||
        !std::isfinite(step) || !std::isfinite(fast))
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        *changed = BuiHost::InputFloatButton(label, v, step, fast, format, InputFlags(flags));
        return BML_OK;
    });
}
int BML_CDECL Int(BML_ModContextHandle h, const char *label, int32_t *v, int32_t step, int32_t fast,
                  uint32_t flags, uint32_t *changed) {
    if (!label || !v || !changed || (flags & ~5u))
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        *changed = BuiHost::InputIntButton(label, v, step, fast, InputFlags(flags));
        return BML_OK;
    });
}
int BML_CDECL Color(BML_ModContextHandle h, const char *label, BML_Color *v, uint32_t flags,
                    uint32_t *changed) {
    if (!label || !v || !changed || (flags & ~1u) || !std::isfinite(v->r) || !std::isfinite(v->g) ||
        !std::isfinite(v->b) || !std::isfinite(v->a))
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        ImVec4 color(v->r, v->g, v->b, v->a);
        const int f = ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Uint8 |
                      ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_AlphaBar |
                      ImGuiColorEditFlags_AlphaPreviewHalf |
                      ((flags & 1) ? ImGuiColorEditFlags_NoAlpha : 0);
        *changed = BuiHost::ColorButton(label, &color, f);
        *v = {color.x, color.y, color.z, color.w};
        return BML_OK;
    });
}
int BML_CDECL Wrapped(BML_ModContextHandle h, const char *text, float width, float base,
                      float scale) {
    if (!text || !std::isfinite(width) || !std::isfinite(base) || !std::isfinite(scale))
        return BML_ERROR_INVALID_PARAMETER;
    return WithUi(h, [&](ModContext &, IMod &) {
        BuiHost::WrappedText(text, width, base, scale);
        return BML_OK;
    });
}
int BML_CDECL Title(BML_ModContextHandle h, const char *text, float y, float scale,
                    uint32_t color) {
    if (!text || !std::isfinite(y) || !std::isfinite(scale))
        return BML_ERROR_INVALID_PARAMETER;
    return WithUi(h, [&](ModContext &, IMod &) {
        BuiHost::Title(text, y, scale, color);
        return BML_OK;
    });
}
int BML_CDECL Search(BML_ModContextHandle h, char *b, size_t n, BML_Vec2 p, float width,
                     uint32_t *changed) {
    if (!b || !n || !changed || !std::memchr(b, 0, n) || !Finite(p) || !std::isfinite(width))
        return BML_ERROR_INVALID_PARAMETER;
    *changed = 0;
    return WithUi(h, [&](ModContext &, IMod &) {
        *changed = BuiHost::SearchBar(b, n, p.x, p.y, width);
        return BML_OK;
    });
}
int BML_CDECL Sound(BML_ModContextHandle h) {
    return WithUi(h, [](ModContext &, IMod &) {
        BuiHost::PlayMenuClickSound();
        return BML_OK;
    });
}
int BML_CDECL Activate(BML_ModContextHandle h, const char *name) {
    if (!name || !*name)
        return BML_ERROR_INVALID_PARAMETER;
    return WithUi(h, [&](ModContext &, IMod &) {
        BuiHost::ActivateScript(name);
        return BML_OK;
    });
}
int BML_CDECL Block(BML_ModContextHandle h) {
    return WithUi(h, [](ModContext &ctx, IMod &owner) {
        if (!AcceptsModWork(ctx, owner))
            return BML_ERROR_FROZEN;
        return ctx.GetBuiApi().Block(owner);
    });
}
int BML_CDECL Unblock(BML_ModContextHandle h) {
    return WithUi(h, [](ModContext &ctx, IMod &owner) { return ctx.GetBuiApi().Unblock(owner); });
}
int BML_CDECL Transition(BML_ModContextHandle h, const char *name) {
    if (!name || !*name)
        return BML_ERROR_INVALID_PARAMETER;
    return WithUi(h, [&](ModContext &ctx, IMod &owner) {
        BuiHost::ActivateScript(name);
        return ctx.GetBuiApi().Unblock(owner);
    });
}
const BML_BuiInterface table{BML_IFACE_HEADER(BML_BuiInterface, BML_BUI_INTERFACE_ID, 1, 0),
                             MenuStyle,
                             ButtonStyle,
                             Begin,
                             End,
                             At,
                             Button,
                             Navigate,
                             Key,
                             YesNo,
                             Radio,
                             Text,
                             Float,
                             Int,
                             Color,
                             Wrapped,
                             Title,
                             Search,
                             Sound,
                             Activate,
                             Block,
                             Unblock,
                             Transition};
} // namespace
const BML_BuiInterface &BuiInterface() { return table; }
void BuiApi::Trim() {
    auto &g = *ImGui::GetCurrentContext();
    while (m_WindowCount) {
        const auto &w = m_Windows[m_WindowCount - 1];
        if (w.Frame == g.FrameCount && w.Depth <= g.CurrentWindowStack.Size &&
            g.CurrentWindowStack[w.Depth - 1].Window == w.Native)
            break;
        --m_WindowCount;
    }
}
int BuiApi::Begin(ModContext &ctx, IMod &owner, const char *id, const char *title, uint32_t *open,
                  uint32_t flags, uint32_t *draw) {
    Trim();
    if (m_WindowCount == m_Windows.size())
        m_Windows.emplace_back();
    auto &w = m_Windows[m_WindowCount];
    w.Name = title;
    w.Name += "###BML/";
    w.Name += owner.GetID();
    w.Name += "/";
    w.Name += id;
    ImGuiWindowFlags f = ((flags & 1) ? ImGuiWindowFlags_NoTitleBar : 0) |
                         ((flags & 2) ? ImGuiWindowFlags_NoMove : 0) |
                         ((flags & 4) ? ImGuiWindowFlags_NoResize : 0) |
                         ((flags & 8) ? ImGuiWindowFlags_AlwaysAutoResize : 0) |
                         ((flags & 16) ? ImGuiWindowFlags_NoBackground : 0) |
                         ((flags & 32) ? ImGuiWindowFlags_NoScrollbar : 0) |
                         ((flags & 64) ? ImGuiWindowFlags_NoCollapse : 0) |
                         ((flags & 128) ? ImGuiWindowFlags_NoSavedSettings : 0);
    if (flags & BML_BUI_WINDOW_MENU_BOUNDS) {
        auto *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        f |= ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    }
    bool visible = !open || *open;
    w.Before = Overlay::CaptureImGuiState();
    w.CallDepth = ctx.GetModLoader().InvocationGate().CallDepth();
    *draw = ImGui::Begin(w.Name.c_str(), open ? &visible : nullptr, f);
    if (open)
        *open = visible;
    w.Owner = &owner;
    w.Native = ImGui::GetCurrentWindow();
    w.Depth = ImGui::GetCurrentContext()->CurrentWindowStack.Size;
    w.Frame = ImGui::GetFrameCount();
    ++m_WindowCount;
    if ((flags & BML_BUI_WINDOW_MENU_BOUNDS) && *draw && ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNavCursorVisible(true);
    }
    return BML_OK;
}
int BuiApi::End(ModContext &ctx, IMod &owner) {
    Trim();
    if (!m_WindowCount)
        return BML_ERROR_INVALID_PARAMETER;
    auto &w = m_Windows[m_WindowCount - 1];
    auto &g = *ImGui::GetCurrentContext();
    if (w.Owner != &owner || w.CallDepth != ctx.GetModLoader().InvocationGate().CallDepth())
        return BML_ERROR_INVALID_PARAMETER;
    if (w.Depth != g.CurrentWindowStack.Size || w.Native != g.CurrentWindow) {
        Overlay::RecoverImGuiState(w.Before);
        --m_WindowCount;
        return BML_ERROR_FAIL;
    }
    ImGui::End();
    --m_WindowCount;
    return BML_OK;
}
int BuiApi::Block(IMod &owner) {
    auto &s = m_Blocks[&owner];
    if (!s)
        s = std::make_unique<BlockShare>();
    if (s->Count == SIZE_MAX)
        return BML_ERROR_BUSY;
    if (!s->Count && !BuiHost::BlockKeyboardInput(s.get()))
        return BML_ERROR_UNAVAILABLE;
    ++s->Count;
    return BML_OK;
}
int BuiApi::Unblock(IMod &owner) {
    auto it = m_Blocks.find(&owner);
    if (it == m_Blocks.end() || !it->second->Count)
        return BML_ERROR_INVALID_PARAMETER;
    if (!--it->second->Count) {
        BuiHost::UnblockKeyboardAfterRelease(it->second.get());
        m_Blocks.erase(it);
    }
    return BML_OK;
}
void BuiApi::CleanupOwner(IMod &owner) noexcept {
    try {
        auto it = m_Blocks.find(&owner);
        if (it != m_Blocks.end()) {
            BuiHost::UnblockKeyboardAfterRelease(it->second.get());
            m_Blocks.erase(it);
        }
    } catch (...) {
    }
}
void BuiApi::Reset() noexcept {
    m_WindowCount = 0;
    for (const auto &[owner, share] : m_Blocks) {
        try {
            BuiHost::UnblockKeyboardAfterRelease(share.get());
        } catch (...) {
        }
    }
    m_Blocks.clear();
}
} // namespace BML::Api
