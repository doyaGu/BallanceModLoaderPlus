#include "Api/GuiApi.h"
#include "Api/ModApi.h"
#include "UI/GuiHost.h"
#include "UI/Gui/LegacyTextFont.h"
#include "Behavior/Blocks/Text2DView.h"
#include <cmath>
#include <unordered_map>
#include "CKSpriteText.h"
#include "CKMaterial.h"

namespace BML::Api {
namespace Host = BGuiHost;

namespace {
bool Equal(BML_ObjectRef a, BML_ObjectRef b) {
    return a.Domain == b.Domain && a.Slot == b.Slot && a.Generation == b.Generation;
}

bool Finite(BML_Vec2 v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

bool Finite(BML_Color c) {
    return std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b) && std::isfinite(c.a);
}

uint32_t Packed(BML_Color c) {
    return VxColor(c.r, c.g, c.b, c.a).GetRGBA();
}

BML_Color Unpacked(uint32_t c) {
    VxColor v(c);
    return {v.r, v.g, v.b, v.a};
}
} // namespace

struct GuiApi::Record {
    ModContext *Context = nullptr;
    IMod *Owner = nullptr;
    uint32_t Kind = 0;
    BML_ObjectRef Primary{}, Base{}, Material{}, Behavior{}, Sprite{};
    bool Invalid = false;
    uint32_t Align = 0;
    BML_Vec2 Offset{};
    bool Focus = false;
    int Width = 0, Height = 0;
    Host::Text *SpriteText = nullptr;
    Host::Panel *Panel = nullptr;
    Host::Label *Label = nullptr;
    Host::Button *Button = nullptr;
    Host::Input *Input = nullptr;
    Host::KeyInput *KeyInput = nullptr;
    BML_GuiEvent Event = BML_GUI_EVENT_INIT;
    std::unique_ptr<Host::Element> Item;

    ~Record() {
        Sanitize(*this);
        Item.reset();
    }
};

struct GuiApi::Storage {
    std::unordered_map<uint32_t, std::unique_ptr<Record>> Elements;
};

GuiApi::GuiApi() = default;
GuiApi::~GuiApi() = default;

void GuiApi::Sanitize(Record &r, bool all) noexcept {
    if (!r.Item)
        return;
    auto alive = [&](BML_ObjectRef ref) { return !all && r.Context->ObjectRefs().Resolve(ref); };
    if (!alive(r.Base))
        r.Item->m_2dEntity = nullptr;
    if (r.Panel && !alive(r.Material))
        r.Panel->m_Material = nullptr;
    if (r.Label && !alive(r.Behavior))
        r.Label->m_Text2d = nullptr;
    if (r.SpriteText && !alive(r.Sprite)) {
        Host::ForgetLegacyTextFont(r.SpriteText->m_Sprite);
        r.SpriteText->m_Sprite = nullptr;
    }
}

GuiApi::Record *GuiApi::Find(ModContext &ctx, IMod &owner, BML_ObjectRef ref) {
    if (!m_Storage)
        return nullptr;
    auto it = m_Storage->Elements.find(ref.Slot);
    if (it == m_Storage->Elements.end() || it->second->Owner != &owner ||
        !Equal(it->second->Primary, ref))
        return nullptr;
    auto &r = *it->second;
    if (r.Invalid || !ctx.ObjectRefs().Resolve(r.Primary) || !ctx.ObjectRefs().Resolve(r.Base) ||
        (r.Behavior.Domain && !ctx.ObjectRefs().Resolve(r.Behavior)) ||
        (r.Material.Domain && !ctx.ObjectRefs().Resolve(r.Material))) {
        r.Invalid = true;
        return nullptr;
    }
    return &r;
}

int GuiApi::Create(ModContext &ctx, IMod &owner, uint32_t kind, const char *name,
                   BML_ObjectRef *out) {
    if (!AcceptsModWork(ctx, owner))
        return BML_ERROR_FROZEN;
    if (!ctx.GetCKContext() || !ctx.GetCKContext()->GetCurrentLevel())
        return BML_ERROR_UNAVAILABLE;
    Collect();
    auto r = std::make_unique<Record>();
    r->Context = &ctx;
    r->Owner = &owner;
    r->Kind = kind;
    // The record takes ownership before any fallible publication step.
    switch (kind) {
    case BML_GUI_ELEMENT:
        r->Item = std::make_unique<Host::Element>(name);
        break;
    case BML_GUI_PANEL: {
        auto p = std::make_unique<Host::Panel>(name);
        r->Panel = p.get();
        r->Item = std::move(p);
        break;
    }
    case BML_GUI_TEXT: {
        auto p = std::make_unique<Host::Text>(name);
        r->SpriteText = p.get();
        r->Item.reset(static_cast<Host::Element *>(p.release()));
        break;
    }
    case BML_GUI_LABEL: {
        auto p = std::make_unique<Host::Label>(name);
        r->Label = p.get();
        r->Item = std::move(p);
        break;
    }
    case BML_GUI_BUTTON: {
        auto p = std::make_unique<Host::Button>(name);
        r->Button = p.get();
        r->Label = p.get();
        r->Item = std::move(p);
        break;
    }
    case BML_GUI_INPUT: {
        auto p = std::make_unique<Host::Input>(name);
        r->Input = p.get();
        r->Label = p.get();
        r->Item = std::move(p);
        break;
    }
    case BML_GUI_KEY_INPUT: {
        auto p = std::make_unique<Host::KeyInput>(name);
        r->KeyInput = p.get();
        r->Input = p.get();
        r->Label = p.get();
        r->Item = std::move(p);
        break;
    }
    default:
        return BML_ERROR_INVALID_PARAMETER;
    }
    auto issue = [&](CKObject *p) {
        auto ref = ctx.ObjectRefs().Issue(p);
        if (p && !ref.Domain)
            throw std::bad_alloc();
        return ref;
    };
    // Retain raw ownership during Issue failure; zero refs have not been published.
    try {
        r->Base = issue(r->Item->m_2dEntity);
        if (r->Panel)
            r->Material = issue(r->Panel->m_Material);
        if (r->Label)
            r->Behavior = issue(r->Label->m_Text2d);
        if (r->SpriteText)
            r->Sprite = issue(r->SpriteText->m_Sprite);
    } catch (...) {
        // No external code has run, so all constructor-owned pointers are live.
        r->Item.reset();
        throw;
    }
    r->Primary = r->SpriteText ? r->Sprite : r->Base;
    if (r->Input) {
        auto *record = r.get();
        r->Input->SetCallback([record](CKDWORD key) {
            record->Event.Key = key;
            record->Event.NotifyCallback = 1;
        });
    }
    if (r->Button)
        r->Button->SetCallback([] {});
    if (!m_Storage)
        m_Storage = std::make_unique<Storage>();
    auto ref = r->Primary;
    m_Storage->Elements.emplace(ref.Slot, std::move(r));
    *out = ref;
    return BML_OK;
}

int GuiApi::Destroy(ModContext &, IMod &owner, BML_ObjectRef ref) {
    if (!m_Storage)
        return BML_ERROR_OBJECT_INVALID;
    auto it = m_Storage->Elements.find(ref.Slot);
    if (it == m_Storage->Elements.end() || it->second->Owner != &owner ||
        !Equal(it->second->Primary, ref))
        return BML_ERROR_OBJECT_INVALID;
    auto record = std::move(it->second);
    m_Storage->Elements.erase(it);
    record.reset();
    return BML_OK;
}

int GuiApi::Read(ModContext &ctx, IMod &owner, BML_ObjectRef ref, BML_GuiElementInfo *out) {
    auto *r = Find(ctx, owner, ref);
    if (!r)
        return BML_ERROR_OBJECT_INVALID;
    BML_GuiElementInfo v = BML_GUI_ELEMENT_INFO_INIT;
    v.Object = ref;
    v.Kind = r->Kind;
    v.Active = 1;
    auto p = r->Item->GetPosition(), s = r->Item->GetSize();
    v.Position = {p.x, p.y};
    v.Size = {s.x, s.y};
    v.Visible = r->Item->IsVisible();
    v.ZOrder = r->Item->GetZOrder();
    if (r->Panel) {
        auto c = r->Panel->GetColor();
        v.Color = {c.r, c.g, c.b, c.a};
    }
    if (r->SpriteText) {
        v.Color = Unpacked(r->SpriteText->GetTextColor());
        v.Alignment = r->SpriteText->m_Sprite->GetAlign();
    }
    if (r->Label) {
        v.GameFont = static_cast<uint32_t>(r->Label->GetFont());
        v.Alignment = r->Align;
        v.TextFlags = r->Label->GetTextFlags();
        v.Offset = r->Offset;
    }
    if (r->Button) {
        v.Active = r->Button->IsActive();
        v.ButtonType = r->Button->GetType();
    }
    if (r->Input)
        v.Focused = r->Focus;
    if (r->KeyInput)
        v.Key = r->KeyInput->GetKey();
    WriteOutput(out, v, BML_GUI_ELEMENT_INFO_1_0_SIZE);
    return BML_OK;
}

#define BML_GUI_FIND                                                                               \
    auto *r = Find(ctx, owner, ref);                                                               \
    if (!r)                                                                                        \
    return BML_ERROR_OBJECT_INVALID

int GuiApi::SetPosition(ModContext &ctx, IMod &owner, BML_ObjectRef ref, BML_Vec2 v) {
    BML_GUI_FIND;
    if (!Finite(v))
        return BML_ERROR_INVALID_PARAMETER;
    r->Item->SetPosition({v.x, v.y});
    return BML_OK;
}

int GuiApi::SetSize(ModContext &ctx, IMod &owner, BML_ObjectRef ref, BML_Vec2 v) {
    BML_GUI_FIND;
    if (!Finite(v) || v.x < 0 || v.y < 0)
        return BML_ERROR_INVALID_PARAMETER;
    if (r->SpriteText) {
        auto *rc = ctx.GetRenderContext();
        if (!rc)
            return BML_ERROR_UNAVAILABLE;
        if (double(rc->GetWidth()) * v.x > INT_MAX || double(rc->GetHeight()) * v.y > INT_MAX)
            return BML_ERROR_INVALID_PARAMETER;
    }
    r->Item->SetSize({v.x, v.y});
    return BML_OK;
}

int GuiApi::SetZOrder(ModContext &ctx, IMod &owner, BML_ObjectRef ref, int32_t v) {
    BML_GUI_FIND;
    r->Item->SetZOrder(v);
    return BML_OK;
}

int GuiApi::SetVisible(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (v > 1)
        return BML_ERROR_INVALID_PARAMETER;
    r->Item->SetVisible(v != 0);
    return BML_OK;
}

int GuiApi::SetActive(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (!r->Button)
        return BML_ERROR_TYPE_MISMATCH;
    if (v > 1)
        return BML_ERROR_INVALID_PARAMETER;
    r->Button->SetActive(v != 0);
    return BML_OK;
}

int GuiApi::SetColor(ModContext &ctx, IMod &owner, BML_ObjectRef ref, BML_Color v) {
    BML_GUI_FIND;
    if (!Finite(v))
        return BML_ERROR_INVALID_PARAMETER;
    if (r->Panel)
        r->Panel->SetColor({v.r, v.g, v.b, v.a});
    else if (r->SpriteText)
        r->SpriteText->SetTextColor(Packed(v));
    else
        return BML_ERROR_TYPE_MISMATCH;
    return BML_OK;
}

int GuiApi::SetText(ModContext &ctx, IMod &owner, BML_ObjectRef ref, const char *v) {
    BML_GUI_FIND;
    if (r->SpriteText)
        r->SpriteText->SetText(v);
    else if (r->Label)
        r->Label->SetText(v);
    else
        return BML_ERROR_TYPE_MISMATCH;
    return BML_OK;
}

int GuiApi::GetText(ModContext &ctx, IMod &owner, BML_ObjectRef ref, char *b, size_t n,
                    size_t *required) {
    BML_GUI_FIND;
    const char *s = r->SpriteText ? r->SpriteText->GetText()
                    : r->Label    ? r->Label->GetText()
                                  : nullptr;
    if (!r->SpriteText && !r->Label)
        return BML_ERROR_TYPE_MISMATCH;
    return CopyText(s ? s : "", b, n, required);
}

int GuiApi::SetFont(ModContext &ctx, IMod &owner, BML_ObjectRef ref, const char *face, int32_t size,
                    int32_t weight, uint32_t flags) {
    BML_GUI_FIND;
    if (!r->SpriteText)
        return BML_ERROR_TYPE_MISMATCH;
    if (!face || !*face || size <= 0 || weight < 0 || (flags & ~3u))
        return BML_ERROR_INVALID_PARAMETER;
    r->SpriteText->SetFont(face, size, weight, flags & 1, flags & 2);
    return BML_OK;
}

int GuiApi::SetAlignment(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (r->SpriteText) {
        if (v & ~127u)
            return BML_ERROR_INVALID_PARAMETER;
        r->SpriteText->SetAlignment(static_cast<CKSPRITETEXT_ALIGNMENT>(v));
    } else if (r->Label) {
        if (v > 10 || v == 3 || v == 7)
            return BML_ERROR_INVALID_PARAMETER;
        r->Label->SetAlignment(v);
        r->Align = v;
    } else
        return BML_ERROR_TYPE_MISMATCH;
    return BML_OK;
}

int GuiApi::SetButtonType(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (!r->Button)
        return BML_ERROR_TYPE_MISMATCH;
    if (v > BML_GUI_BUTTON_MINUS)
        return BML_ERROR_INVALID_PARAMETER;
    r->Button->SetType(static_cast<Host::ButtonType>(v));
    return BML_OK;
}

int GuiApi::SetHovered(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (!r->Button)
        return BML_ERROR_TYPE_MISMATCH;
    if (v > 1)
        return BML_ERROR_INVALID_PARAMETER;
    if (v)
        r->Button->OnMouseEnter();
    else
        r->Button->OnMouseLeave();
    return BML_OK;
}

int GuiApi::SetGameFont(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (!r->Label)
        return BML_ERROR_TYPE_MISMATCH;
    if (v > BML_GUI_GAMEFONT_CREDITS_BIG)
        return BML_ERROR_INVALID_PARAMETER;
    r->Label->SetFont(static_cast<BML::GameFont>(v));
    return BML_OK;
}

int GuiApi::SetTextFlags(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (!r->Label)
        return BML_ERROR_TYPE_MISMATCH;
    if (v & ~4095u)
        return BML_ERROR_INVALID_PARAMETER;
    r->Label->SetTextFlags(v);
    return BML_OK;
}

int GuiApi::SetOffset(ModContext &ctx, IMod &owner, BML_ObjectRef ref, BML_Vec2 v) {
    BML_GUI_FIND;
    if (!r->Label)
        return BML_ERROR_TYPE_MISMATCH;
    if (!Finite(v))
        return BML_ERROR_INVALID_PARAMETER;
    r->Label->SetOffset({v.x, v.y});
    r->Offset = v;
    return BML_OK;
}

int GuiApi::SetFocused(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (!r->Input)
        return BML_ERROR_TYPE_MISMATCH;
    if (v > 1)
        return BML_ERROR_INVALID_PARAMETER;
    if (v)
        r->Input->GetFocus();
    else
        r->Input->LoseFocus();
    r->Focus = v;
    return BML_OK;
}

int GuiApi::SetKey(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t v) {
    BML_GUI_FIND;
    if (!r->KeyInput)
        return BML_ERROR_TYPE_MISMATCH;
    if (v > 255)
        return BML_ERROR_INVALID_PARAMETER;
    r->KeyInput->SetKey(static_cast<CKKEYBOARD>(v));
    return BML_OK;
}

int GuiApi::CharTyped(ModContext &ctx, IMod &owner, BML_ObjectRef ref, uint32_t key,
                      BML_GuiEvent *out) {
    BML_GUI_FIND;
    if (!r->Input)
        return BML_ERROR_TYPE_MISMATCH;
    if (key > 255)
        return BML_ERROR_INVALID_PARAMETER;
    const std::string before = r->Input->GetText();
    r->Event = BML_GUI_EVENT_INIT;
    r->Event.Key = key;
    r->Input->OnCharTyped(key);
    r->Event.Changed = before != r->Input->GetText();
    WriteOutput(out, r->Event, BML_GUI_EVENT_1_0_SIZE);
    return BML_OK;
}

int GuiApi::Process(ModContext &ctx, IMod &owner, BML_ObjectRef ref) {
    BML_GUI_FIND;
    if (r->SpriteText) {
        r->SpriteText->UpdateFont();
        auto *rc = ctx.GetRenderContext();
        if (rc && (r->Width != rc->GetWidth() || r->Height != rc->GetHeight())) {
            auto size = r->Item->GetSize();
            r->Item->SetSize(size);
            r->Width = rc->GetWidth();
            r->Height = rc->GetHeight();
        }
    } else
        r->Item->Process();
    return BML_OK;
}

#undef BML_GUI_FIND

void GuiApi::ObjectsToBeDeleted(const uint32_t *ids, int count) noexcept {
    if (!m_Storage || !ids || count <= 0)
        return;
    for (auto &[slot, p] : m_Storage->Elements) {
        auto &r = *p;
        auto doomed = [&](BML_ObjectRef ref) {
            for (int i = 0; i < count; ++i)
                if (ref.Domain && ref.Slot == ids[i])
                    return true;
            return false;
        };
        if (doomed(r.Base)) {
            r.Item->m_2dEntity = nullptr;
            r.Invalid = true;
        }
        if (r.Panel && doomed(r.Material)) {
            r.Panel->m_Material = nullptr;
            r.Invalid = true;
        }
        if (r.Label && doomed(r.Behavior)) {
            r.Label->m_Text2d = nullptr;
            r.Invalid = true;
        }
        if (r.SpriteText && doomed(r.Sprite)) {
            Host::ForgetLegacyTextFont(r.SpriteText->m_Sprite);
            r.SpriteText->m_Sprite = nullptr;
            r.Invalid = true;
        }
    }
}

void GuiApi::ResetWorld() noexcept {
    if (!m_Storage)
        return;
    for (auto &[slot, r] : m_Storage->Elements)
        Sanitize(*r, true);
    m_Storage->Elements.clear();
}

void GuiApi::Collect() noexcept {
    if (!m_Storage)
        return;
    for (auto it = m_Storage->Elements.begin(); it != m_Storage->Elements.end();)
        if (it->second->Invalid) {
            auto r = std::move(it->second);
            it = m_Storage->Elements.erase(it);
            r.reset();
        } else
            ++it;
}

bool GuiApi::CleanupOwner(IMod &owner) noexcept {
    if (!m_Storage)
        return true;
    for (auto it = m_Storage->Elements.begin(); it != m_Storage->Elements.end();)
        if (it->second->Owner == &owner) {
            auto r = std::move(it->second);
            it = m_Storage->Elements.erase(it);
            r.reset();
        } else
            ++it;
    return true;
}

namespace {
#define BML_GUI_THUNK(Name, Method, Params, Args)                                                  \
    int BML_CDECL Name(BML_ModContextHandle h, BML_ObjectRef ref, Params) {                        \
        return WithModContext(h, [&](ModContext &ctx, IMod &owner) {                               \
            return ctx.GetGuiApi().Method(ctx, owner, ref, Args);                                  \
        });                                                                                        \
    }

int BML_CDECL Create(BML_ModContextHandle h, uint32_t kind, const char *name, BML_ObjectRef *out) {
    if (!name || !out || kind > BML_GUI_ELEMENT)
        return BML_ERROR_INVALID_PARAMETER;
    *out = {};
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetGuiApi().Create(ctx, owner, kind, name, out);
    });
}

int BML_CDECL Destroy(BML_ModContextHandle h, BML_ObjectRef ref) {
    return WithModContext(
        h, [&](ModContext &ctx, IMod &owner) { return ctx.GetGuiApi().Destroy(ctx, owner, ref); });
}

int BML_CDECL Read(BML_ModContextHandle h, BML_ObjectRef ref, BML_GuiElementInfo *out) {
    if (!out || out->StructSize < BML_GUI_ELEMENT_INFO_1_0_SIZE)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetGuiApi().Read(ctx, owner, ref, out);
    });
}
BML_GUI_THUNK(SetPosition, SetPosition, BML_Vec2 v, v)
BML_GUI_THUNK(SetSize, SetSize, BML_Vec2 v, v)
BML_GUI_THUNK(SetZOrder, SetZOrder, int32_t v, v)
BML_GUI_THUNK(SetVisible, SetVisible, uint32_t v, v)
BML_GUI_THUNK(SetActive, SetActive, uint32_t v, v)
BML_GUI_THUNK(SetColor, SetColor, BML_Color v, v)

int BML_CDECL SetText(BML_ModContextHandle h, BML_ObjectRef ref, const char *v) {
    if (!v)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetGuiApi().SetText(ctx, owner, ref, v);
    });
}

int BML_CDECL GetText(BML_ModContextHandle h, BML_ObjectRef ref, char *b, size_t n,
                      size_t *required) {
    if (!required || (!b && n))
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetGuiApi().GetText(ctx, owner, ref, b, n, required);
    });
}

int BML_CDECL SetFont(BML_ModContextHandle h, BML_ObjectRef ref, const char *face, int32_t size,
                      int32_t weight, uint32_t flags) {
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetGuiApi().SetFont(ctx, owner, ref, face, size, weight, flags);
    });
}
BML_GUI_THUNK(SetAlignment, SetAlignment, uint32_t v, v)
BML_GUI_THUNK(SetButtonType, SetButtonType, uint32_t v, v)
BML_GUI_THUNK(SetHovered, SetHovered, uint32_t v, v)
BML_GUI_THUNK(SetGameFont, SetGameFont, uint32_t v, v)
BML_GUI_THUNK(SetTextFlags, SetTextFlags, uint32_t v, v)
BML_GUI_THUNK(SetOffset, SetOffset, BML_Vec2 v, v)
BML_GUI_THUNK(SetFocused, SetFocused, uint32_t v, v)
BML_GUI_THUNK(SetKey, SetKey, uint32_t v, v)

int BML_CDECL CharTyped(BML_ModContextHandle h, BML_ObjectRef ref, uint32_t key,
                        BML_GuiEvent *out) {
    if (!out || out->StructSize < BML_GUI_EVENT_1_0_SIZE)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetGuiApi().CharTyped(ctx, owner, ref, key, out);
    });
}

int BML_CDECL Process(BML_ModContextHandle h, BML_ObjectRef ref) {
    return WithModContext(
        h, [&](ModContext &ctx, IMod &owner) { return ctx.GetGuiApi().Process(ctx, owner, ref); });
}

const BML_GuiInterface table{BML_IFACE_HEADER(BML_GuiInterface, BML_GUI_INTERFACE_ID, 1, 0),
                             Create,
                             Destroy,
                             Read,
                             SetPosition,
                             SetSize,
                             SetZOrder,
                             SetVisible,
                             SetActive,
                             SetColor,
                             SetText,
                             GetText,
                             SetFont,
                             SetAlignment,
                             SetButtonType,
                             SetHovered,
                             SetGameFont,
                             SetTextFlags,
                             SetOffset,
                             SetFocused,
                             SetKey,
                             CharTyped,
                             Process};
#undef BML_GUI_THUNK
} // namespace

const BML_GuiInterface &GuiInterface() {
    return table;
}
} // namespace BML::Api
