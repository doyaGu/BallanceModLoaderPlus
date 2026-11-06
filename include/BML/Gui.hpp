#ifndef BML_GUI_HPP
#define BML_GUI_HPP

#include "BML/Gui.h"
#include "BML/Bui.hpp"
#include "BML/Input.hpp"
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace BGui {
class Gui;

namespace Detail {
inline const BML_GuiInterface *Api() noexcept {
    return BML::Detail::Bind<BML_GuiInterface>(
        BML_GUI_INTERFACE_ID, 1, BML_GUI_INTERFACE_1_0_SIZE, +[](const BML_GuiInterface *table) {
            return BML_IFACE_HAS(table, BML_GuiInterface, CreateElement) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, DestroyElement) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, ReadElement) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetPosition) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetSize) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetZOrder) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetVisible) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetActive) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetColor) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetText) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, GetText) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetFont) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetAlignment) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetButtonType) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetHovered) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetGameFont) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetTextFlags) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetOffset) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetFocused) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, SetKey) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, OnCharTyped) &&
                   BML_IFACE_HAS(table, BML_GuiInterface, ProcessElement);
        });
}

inline int Report(const BML::Mod::Context &ctx, int code) noexcept {
    if (code != BML_OK) {
        char message[96];
        std::snprintf(message, sizeof message, "Gui operation failed (%d)", code);
        (void)ctx.LogError(message);
    }
    return code;
}

inline BML_Color Unpack(uint32_t c) {
    return {float((c >> 16) & 255) / 255.f, float((c >> 8) & 255) / 255.f, float(c & 255) / 255.f,
            float((c >> 24) & 255) / 255.f};
}

inline uint32_t Pack(BML_Color c) {
    auto b = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.f, 1.f) * 255.f); };
    return (b(c.a) << 24) | (b(c.r) << 16) | (b(c.g) << 8) | b(c.b);
}
} // namespace Detail

class Element {
    friend class Gui;

public:
    explicit Element(const BML::Mod::Context &c, const char *name)
        : Element(c, name, BML_GUI_ELEMENT) {}

    virtual ~Element() {
        if (m_2dEntity.Domain && m_Api) {
            const int code = m_Api->DestroyElement(m_Context.Handle(), m_2dEntity);
            if (code != BML_OK && code != BML_ERROR_OBJECT_INVALID &&
                code != BML_ERROR_INVALID_HANDLE)
                Detail::Report(m_Context, code);
        }
    }

    Element(const Element &) = delete;
    Element &operator=(const Element &) = delete;
    Element(Element &&) = delete;
    Element &operator=(Element &&) = delete;

    virtual BML_Vec2 GetPosition() { return Read().Position; }

    virtual void SetPosition(BML_Vec2 v) {
        Call([&](const auto &a) { return a.SetPosition(m_Context.Handle(), m_2dEntity, v); });
    }

    virtual BML_Vec2 GetSize() { return Read().Size; }

    virtual void SetSize(BML_Vec2 v) {
        Call([&](const auto &a) { return a.SetSize(m_Context.Handle(), m_2dEntity, v); });
    }

    virtual int32_t GetZOrder() { return Read().ZOrder; }

    virtual void SetZOrder(int32_t v) {
        Call([&](const auto &a) { return a.SetZOrder(m_Context.Handle(), m_2dEntity, v); });
    }

    virtual bool IsVisible() { return Read().Visible != 0; }

    virtual void SetVisible(bool v) {
        Call([&](const auto &a) { return a.SetVisible(m_Context.Handle(), m_2dEntity, v); });
    }

    virtual void Process() {}

protected:
    Element(const BML::Mod::Context &c, const char *name, uint32_t kind)
        : m_Context(c), m_Api(Detail::Api()) {
        const int code = m_Api ? m_Api->CreateElement(c.Handle(), kind, name, &m_2dEntity)
                               : BML_ERROR_UNAVAILABLE;
        if (code != BML_OK)
            throw std::runtime_error("GUI element creation failed (" + std::to_string(code) + ")");
    }

    template <class F> int Call(F &&f) {
        const int code = Detail::Report(m_Context, m_Api ? f(*m_Api) : BML_ERROR_UNAVAILABLE);
        if (code != BML_OK)
            m_LastFailure = code;
        return code;
    }

    BML_GuiElementInfo Read() {
        BML_GuiElementInfo v = BML_GUI_ELEMENT_INFO_INIT;
        if (Call([&](const auto &a) {
                return a.ReadElement(m_Context.Handle(), m_2dEntity, &v);
            }) != BML_OK)
            v = {};
        return v;
    }

    bool Live() const noexcept {
        BML_GuiElementInfo v = BML_GUI_ELEMENT_INFO_INIT;
        return m_Api && m_Api->ReadElement(m_Context.Handle(), m_2dEntity, &v) == BML_OK;
    }

    void ValidateConstruction() {
        if (m_LastFailure != BML_OK)
            throw std::runtime_error("GUI element setup failed (" + std::to_string(m_LastFailure) +
                                     ")");
    }

    int m_LastFailure = BML_OK;

    const char *ReadText() {
        auto r = BML::Detail::ReadBuffer<std::string>([&](char *p, size_t n, size_t *required) {
            return m_Api ? m_Api->GetText(m_Context.Handle(), m_2dEntity, p, n, required)
                         : BML_ERROR_UNAVAILABLE;
        });
        if (r)
            m_TextCache = std::move(r.Value());
        else {
            Detail::Report(m_Context, r.Code());
            m_TextCache.clear();
        }
        return m_TextCache.c_str();
    }

    void WriteText(const char *v) {
        Call([&](const auto &a) { return a.SetText(m_Context.Handle(), m_2dEntity, v); });
        m_TextCache.clear();
    }

    BML_ObjectRef m_2dEntity{};
    BML::Mod::Context m_Context;
    const BML_GuiInterface *m_Api = nullptr;
    std::string m_TextCache;
};

class Panel : public Element {
public:
    explicit Panel(const BML::Mod::Context &c, const char *n) : Element(c, n, BML_GUI_PANEL) {}

    BML_Color GetColor() { return Read().Color; }

    void SetColor(BML_Color v) {
        Call([&](const auto &a) { return a.SetColor(m_Context.Handle(), m_2dEntity, v); });
    }
};

class Text : private Element {
    friend class Gui;

public:
    explicit Text(const BML::Mod::Context &c, const char *n) : Element(c, n, BML_GUI_TEXT) {}

    BML_Vec2 GetPosition() override { return Element::GetPosition(); }

    void SetPosition(BML_Vec2 v) override { Element::SetPosition(v); }

    BML_Vec2 GetSize() override { return Element::GetSize(); }

    void SetSize(BML_Vec2 v) override { Element::SetSize(v); }

    int32_t GetZOrder() override { return Element::GetZOrder(); }

    void SetZOrder(int32_t v) override { Element::SetZOrder(v); }

    bool IsVisible() override { return Element::IsVisible(); }

    void SetVisible(bool v) override { Element::SetVisible(v); }

    const char *GetText() { return ReadText(); }

    void SetText(const char *v) { WriteText(v); }

    void SetFont(const char *face, int32_t size, int32_t weight, bool italic, bool underline) {
        Call([&](const auto &a) {
            return a.SetFont(m_Context.Handle(), m_2dEntity, face, size, weight,
                             (italic ? 1u : 0u) | (underline ? 2u : 0u));
        });
    }

    void SetAlignment(uint32_t v) {
        Call([&](const auto &a) { return a.SetAlignment(m_Context.Handle(), m_2dEntity, v); });
    }

    uint32_t GetTextColor() { return Detail::Pack(Read().Color); }

    void SetTextColor(uint32_t c) {
        Call([&](const auto &a) {
            return a.SetColor(m_Context.Handle(), m_2dEntity, Detail::Unpack(c));
        });
    }

    void UpdateFont() { Process(); }

    void Process() override {
        Call([&](const auto &a) { return a.ProcessElement(m_Context.Handle(), m_2dEntity); });
    }

protected:
    BML_ObjectRef m_Sprite{};
};

class Label : public Element {
public:
    explicit Label(const BML::Mod::Context &c, const char *n) : Label(c, n, BML_GUI_LABEL) {}

    virtual const char *GetText() { return ReadText(); }

    virtual void SetText(const char *v) { WriteText(v); }

    uint32_t GetFont() { return Read().GameFont; }

    void SetFont(uint32_t v) {
        Call([&](const auto &a) { return a.SetGameFont(m_Context.Handle(), m_2dEntity, v); });
    }

    void SetAlignment(int32_t v) {
        Call([&](const auto &a) {
            return a.SetAlignment(m_Context.Handle(), m_2dEntity, static_cast<uint32_t>(v));
        });
    }

    int32_t GetTextFlags() { return static_cast<int32_t>(Read().TextFlags); }

    void SetTextFlags(int32_t v) {
        Call([&](const auto &a) {
            return a.SetTextFlags(m_Context.Handle(), m_2dEntity, static_cast<uint32_t>(v));
        });
    }

    void SetOffset(BML_Vec2 v) {
        Call([&](const auto &a) { return a.SetOffset(m_Context.Handle(), m_2dEntity, v); });
    }

    void Process() override {
        Call([&](const auto &a) { return a.ProcessElement(m_Context.Handle(), m_2dEntity); });
    }

protected:
    Label(const BML::Mod::Context &c, const char *n, uint32_t kind) : Element(c, n, kind) {}

    BML_ObjectRef m_Text2d{};
};

enum ButtonType {
    BUTTON_NORMAL,
    BUTTON_BACK,
    BUTTON_SETTING,
    BUTTON_LEVEL,
    BUTTON_KEY,
    BUTTON_KEYSEL,
    BUTTON_SMALL,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_PLUS,
    BUTTON_MINUS
};

class Button : public Label {
public:
    explicit Button(const BML::Mod::Context &c, const char *n) : Label(c, n, BML_GUI_BUTTON) {}

    ButtonType GetType() { return static_cast<ButtonType>(Read().ButtonType); }

    void SetType(ButtonType v) {
        Call([&](const auto &a) { return a.SetButtonType(m_Context.Handle(), m_2dEntity, v); });
    }

    bool IsActive() { return Read().Active != 0; }

    void SetActive(bool v) {
        Call([&](const auto &a) { return a.SetActive(m_Context.Handle(), m_2dEntity, v); });
    }

    void InvokeCallback() {
        auto callback = m_Callback;
        if (callback)
            callback();
    }

    void SetCallback(std::function<void()> f) { m_Callback = std::move(f); }

    void OnMouseEnter() {
        Call([&](const auto &a) { return a.SetHovered(m_Context.Handle(), m_2dEntity, 1); });
    }

    void OnMouseLeave() {
        Call([&](const auto &a) { return a.SetHovered(m_Context.Handle(), m_2dEntity, 0); });
    }

protected:
    std::function<void()> m_Callback;
};

class Input : public Label {
public:
    explicit Input(const BML::Mod::Context &c, const char *n) : Input(c, n, BML_GUI_INPUT) {}

    void InvokeCallback(uint32_t key) {
        auto f = m_Callback;
        if (f)
            f(key);
    }

    void SetCallback(std::function<void(uint32_t)> f) { m_Callback = std::move(f); }

    virtual void OnCharTyped(uint32_t key) {
        BML_GuiEvent e = BML_GUI_EVENT_INIT;
        int code = Call(
            [&](const auto &a) { return a.OnCharTyped(m_Context.Handle(), m_2dEntity, key, &e); });
        if (code == BML_OK && e.NotifyCallback)
            InvokeCallback(e.Key);
    }

    const char *GetText() override { return ReadText(); }

    void SetText(const char *v) override { WriteText(v); }

    virtual void GetFocus() {
        Call([&](const auto &a) { return a.SetFocused(m_Context.Handle(), m_2dEntity, 1); });
    }

    virtual void LoseFocus() {
        Call([&](const auto &a) { return a.SetFocused(m_Context.Handle(), m_2dEntity, 0); });
    }

protected:
    Input(const BML::Mod::Context &c, const char *n, uint32_t kind) : Label(c, n, kind) {}

    std::function<void(uint32_t)> m_Callback;
};

class KeyInput : public Input {
public:
    explicit KeyInput(const BML::Mod::Context &c, const char *n) : Input(c, n, BML_GUI_KEY_INPUT) {}

    void OnCharTyped(uint32_t key) override { Input::OnCharTyped(key); }

    uint32_t GetKey() { return Read().Key; }

    void SetKey(uint32_t key) {
        Call([&](const auto &a) { return a.SetKey(m_Context.Handle(), m_2dEntity, key); });
    }

    void GetFocus() override { Input::GetFocus(); }

    void LoseFocus() override { Input::LoseFocus(); }
};

class Gui {
    struct State {
        explicit State(const BML::Mod::Context &c) : Context(c) {}

        BML::Mod::Context Context;
        bool Alive = true;
        bool Visible = true;
        bool Block = true;
        std::vector<std::unique_ptr<Element>> Elements;
        std::vector<Button *> Buttons;
        std::vector<Input *> Inputs;
        std::vector<Text *> Texts;

        ~State() {
            for (auto &element : Elements)
                element.reset();
        }

        Input *Focus = nullptr;
        Button *Back = nullptr;
        BML_Vec2 OldMouse{};
        int32_t Width = 0;
        int32_t Height = 0;
    };

public:
    explicit Gui(const BML::Mod::Context &c) : m_State(std::make_shared<State>(c)) {}

    virtual ~Gui() { m_State->Alive = false; }

    Gui(const Gui &) = delete;
    Gui &operator=(const Gui &) = delete;
    Gui(Gui &&) = delete;
    Gui &operator=(Gui &&) = delete;

    Button *AddNormalButton(
        const char *n, const char *t, float y, float x = .35f, std::function<void()> f = []() {}) {
        return AddButton(n, t, x, y, BUTTON_NORMAL, std::move(f));
    }

    Button *AddBackButton(
        const char *n, const char *t = "Back", float y = .85f, float x = .4031f,
        std::function<void()> f = []() {}) {
        auto *p = AddButton(n, t, x, y, BUTTON_BACK, std::move(f));
        m_State->Back = p;
        return p;
    }

    Button *AddSettingButton(
        const char *n, const char *t, float y, float x = .35f, std::function<void()> f = []() {}) {
        return AddButton(n, t, x, y, BUTTON_SETTING, std::move(f));
    }

    Button *AddLevelButton(
        const char *n, const char *t, float y, float x = .4031f,
        std::function<void()> f = []() {}) {
        return AddButton(n, t, x, y, BUTTON_LEVEL, std::move(f));
    }

    Button *AddSmallButton(
        const char *n, const char *t, float y, float x, std::function<void()> f = []() {}) {
        return AddButton(n, t, x, y, BUTTON_SMALL, std::move(f));
    }

    Button *AddLeftButton(const char *n, float y, float x, std::function<void()> f = []() {}) {
        return AddButton(n, "", x, y, BUTTON_LEFT, std::move(f));
    }

    Button *AddRightButton(const char *n, float y, float x, std::function<void()> f = []() {}) {
        return AddButton(n, "", x, y, BUTTON_RIGHT, std::move(f));
    }

    Button *AddPlusButton(const char *n, float y, float x, std::function<void()> f = []() {}) {
        return AddButton(n, "", x, y, BUTTON_PLUS, std::move(f));
    }

    Button *AddMinusButton(const char *n, float y, float x, std::function<void()> f = []() {}) {
        return AddButton(n, "", x, y, BUTTON_MINUS, std::move(f));
    }

    Button *AddKeyBgButton(const char *n, float y, float x) {
        return AddButton(n, "", x, y, BUTTON_KEY, [] {});
    }

    Panel *AddPanel(const char *n, BML_Color color, float x, float y, float w, float h) {
        auto p = std::make_unique<Panel>(m_State->Context, n);
        p->SetColor(color);
        Layout(*p, x, y, w, h);
        return Commit(std::move(p));
    }

    Label *AddTextLabel(const char *n, const char *text, uint32_t font, float x, float y, float w,
                        float h) {
        auto p = std::make_unique<Label>(m_State->Context, n);
        p->SetText(text);
        p->SetFont(font);
        Layout(*p, x, y, w, h);
        return Commit(std::move(p));
    }

    Text *AddText(const char *n, const char *text, float x, float y, float w, float h) {
        auto p = std::make_unique<Text>(m_State->Context, n);
        p->SetText(text);
        Layout(*p, x, y, w, h);
        auto *raw = p.get();
        auto &s = *m_State;
        s.Texts.reserve(s.Texts.size() + 1);
        Commit(std::move(p));
        s.Texts.push_back(raw);
        return raw;
    }

    Input *AddTextInput(
        const char *n, uint32_t font, float x, float y, float w, float h,
        std::function<void(uint32_t)> f = [](uint32_t) {}) {
        auto p = std::make_unique<Input>(m_State->Context, n);
        p->SetFont(font);
        p->SetCallback(std::move(f));
        Layout(*p, x, y, w, h);
        auto *raw = p.get();
        auto &s = *m_State;
        s.Inputs.reserve(s.Inputs.size() + 1);
        Commit(std::move(p));
        s.Inputs.push_back(raw);
        if (!s.Focus)
            SetFocus(raw);
        return raw;
    }

    std::pair<Button *, KeyInput *> AddKeyButton(
        const char *n, const char *text, float y, float x = .35f,
        std::function<void(uint32_t)> f = [](uint32_t) {}) {
        auto s = m_State;
        auto bg = MakeButton(n, text, x, y, BUTTON_KEY, [] {});
        auto p = std::make_unique<KeyInput>(s->Context, n);
        bg->SetAlignment(BML_GUI_ALIGN_LEFT);
        bg->SetZOrder(15);
        bg->SetOffset({Bui::CoordToPixel(s->Context, {.03f, 0}).x, 0});
        p->SetFont(BML_GUI_GAMEFONT_03);
        Layout(*p, x + .155f, y, .145f, .0396f);
        p->SetZOrder(25);
        p->SetCallback(std::move(f));
        bg->ValidateConstruction();
        p->ValidateConstruction();
        auto *a = bg.get();
        auto *b = p.get();
        s->Elements.reserve(s->Elements.size() + 2);
        s->Buttons.reserve(s->Buttons.size() + 1);
        s->Inputs.reserve(s->Inputs.size() + 1);
        Commit(std::move(bg));
        Commit(std::move(p));
        s->Buttons.push_back(a);
        s->Inputs.push_back(b);
        return {a, b};
    }

    std::pair<Button *, Button *> AddYesNoButton(
        const char *n, float y, float x1, float x2, std::function<void(bool)> f = [](bool) {}) {
        auto s = m_State;
        auto yes = MakeButton(n, "Yes", x1, y, BUTTON_SMALL, [] {});
        auto no = MakeButton(n, "No", x2, y, BUTTON_SMALL, [] {});
        auto *a = yes.get();
        auto *b = no.get();
        std::weak_ptr<State> weak = s;
        yes->SetCallback([f, weak, a, b] {
            f(true);
            if (auto s = weak.lock(); s && s->Alive && s->Visible && a->Live() && b->Live()) {
                a->SetActive(true);
                b->SetActive(false);
            }
        });
        no->SetCallback([f, weak, a, b] {
            f(false);
            if (auto s = weak.lock(); s && s->Alive && s->Visible && a->Live() && b->Live()) {
                a->SetActive(false);
                b->SetActive(true);
            }
        });
        yes->ValidateConstruction();
        no->ValidateConstruction();
        s->Elements.reserve(s->Elements.size() + 2);
        s->Buttons.reserve(s->Buttons.size() + 2);
        Commit(std::move(yes));
        Commit(std::move(no));
        s->Buttons.push_back(a);
        s->Buttons.push_back(b);
        return {a, b};
    }

    virtual void OnCharTyped(uint32_t key) {
        auto s = m_State;
        if (key == BML_KEY_ESCAPE && s->Back && s->Back->Live() && s->Back->IsVisible())
            s->Back->InvokeCallback();
        if (Ready(s) && s->Focus && s->Focus->Live() && s->Focus->IsVisible())
            s->Focus->OnCharTyped(key);
    }

    virtual void OnMouseDown(float x, float y, uint32_t key) {
        auto s = m_State;
        if (key != BML_MOUSE_LEFT || !Ready(s))
            return;
        bool hit = false;
        const auto n = s->Buttons.size();
        for (size_t i = 0; i < n && Ready(s); ++i) {
            auto *p = s->Buttons[i];
            if (Inside(x, y, p)) {
                p->InvokeCallback();
                hit = true;
            }
        }
        if (!Ready(s))
            return;
        if (s->Buttons.empty() && s->Inputs.size() == 1)
            SetFocus(s->Inputs[0]);
        else {
            SetFocus(nullptr);
            for (auto *p : s->Inputs)
                if (Inside(x, y, p)) {
                    SetFocus(p);
                    hit = true;
                }
        }
        if (Ready(s) && hit)
            Bui::PlayMenuClickSound(s->Context);
    }

    virtual void OnMouseWheel(float) {}

    virtual void OnMouseMove(float, float, float, float) {}

    virtual void OnScreenModeChanged() {
        auto s = m_State;
        for (auto *t : s->Texts)
            if (t->Live())
                t->UpdateFont();
    }

    virtual void Process() {
        auto s = m_State;
        if (!Ready(s))
            return;
        auto size = Bui::CoordToPixel(s->Context, {1, 1});
        if (size.x <= 0 || size.y <= 0)
            return;
        if (s->Width != int32_t(size.x) || s->Height != int32_t(size.y)) {
            s->Width = int32_t(size.x);
            s->Height = int32_t(size.y);
            OnScreenModeChanged();
            if (!Ready(s))
                return;
        }
        for (const auto &p : s->Elements)
            if (p->Live())
                p->Process();
        BML::Input::View input(s->Context);
        const uint32_t flags = s->Block ? 0u : BML_INPUT_RAW;
        const auto keyboard = input.ReadKeyboard(flags);
        const auto mouse = input.ReadMouse(flags);
        if (keyboard)
            for (const auto &e : keyboard.Value().Events)
                if (e.State == BML_INPUT_KEY_PRESSED) {
                    OnCharTyped(e.Key);
                    if (!Ready(s))
                        return;
                }
        if (!mouse || !Ready(s))
            return;
        const auto &m = mouse.Value();
        const float x = m.WindowPosition.x / size.x;
        const float y = m.WindowPosition.y / size.y;
        for (uint32_t i = 0; i < 4; ++i)
            if ((m.ClickedMask & (1u << i))) {
                OnMouseDown(x, y, i);
                if (!Ready(s))
                    return;
            }
        for (auto *p : s->Buttons)
            if (p->Live())
                p->OnMouseLeave();
        for (auto *p : s->Buttons)
            if (Inside(x, y, p))
                p->OnMouseEnter();
        if (m.RelativePosition.x != 0 || m.RelativePosition.y != 0) {
            OnMouseMove(x, y, s->OldMouse.x, s->OldMouse.y);
            if (!Ready(s))
                return;
        }
        s->OldMouse = {x, y};
        if (m.RelativePosition.z != 0)
            OnMouseWheel(m.RelativePosition.z);
    }

    virtual void SetVisible(bool v) {
        auto s = m_State;
        s->Visible = v;
        for (const auto &p : s->Elements)
            if (p->Live())
                p->SetVisible(v);
    }

    bool CanBeBlocked() { return m_State->Block; }

    void SetCanBeBlocked(bool v) { m_State->Block = v; }

    void SetFocus(Input *p) {
        auto s = m_State;
        if (p && std::find(s->Inputs.begin(), s->Inputs.end(), p) == s->Inputs.end())
            return;
        if (s->Focus && s->Focus->Live())
            s->Focus->LoseFocus();
        s->Focus = p;
        if (p && p->Live())
            p->GetFocus();
    }

    bool Intersect(float x, float y, Element *p) { return Inside(x, y, p); }

private:
    static bool Ready(const std::shared_ptr<State> &s) { return s->Alive && s->Visible; }

    static bool Inside(float x, float y, Element *p) {
        if (!p || !p->Live() || !p->IsVisible())
            return false;
        auto pos = p->GetPosition(), size = p->GetSize();
        return x >= pos.x && x <= pos.x + size.x && y >= pos.y && y <= pos.y + size.y;
    }

    template <class T> static void Layout(T &p, float x, float y, float w, float h) {
        p.SetPosition({x, y});
        p.SetSize({w, h});
    }

    template <class T> T *Commit(std::unique_ptr<T> p) {
        p->ValidateConstruction();
        auto *raw = p.get();
        std::unique_ptr<Element> owned(static_cast<Element *>(p.release()));
        m_State->Elements.push_back(std::move(owned));
        return raw;
    }

    std::unique_ptr<Button> MakeButton(const char *n, const char *text, float x, float y,
                                       ButtonType type, std::function<void()> f) {
        auto p = std::make_unique<Button>(m_State->Context, n);
        p->SetType(type);
        p->SetText(text);
        p->SetPosition({x, y});
        p->SetCallback(std::move(f));
        return p;
    }

    Button *AddButton(const char *n, const char *text, float x, float y, ButtonType type,
                      std::function<void()> f) {
        auto p = MakeButton(n, text, x, y, type, std::move(f));
        auto *raw = p.get();
        m_State->Buttons.reserve(m_State->Buttons.size() + 1);
        Commit(std::move(p));
        m_State->Buttons.push_back(raw);
        return raw;
    }

    std::shared_ptr<State> m_State;
};
} // namespace BGui

// Keep the existing global namespace available for source compatibility.
namespace BML {
namespace Gui = ::BGui;
}

#endif
