#ifndef BML_BUI_HPP
#define BML_BUI_HPP

#include "BML/Bui.h"
#include "BML/Mod.hpp"
#include <algorithm>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Bui {
enum ButtonType {
    BUTTON_MAIN,
    BUTTON_BACK,
    BUTTON_OPTION,
    BUTTON_LEVEL,
    BUTTON_KEY,
    BUTTON_SMALL,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_PLUS,
    BUTTON_MINUS,
    BUTTON_COUNT
};

namespace Detail {
inline const BML_BuiInterface *Api() noexcept {
    return BML::Detail::Bind<BML_BuiInterface>(
        BML_BUI_INTERFACE_ID, 1, BML_BUI_INTERFACE_1_0_SIZE, +[](const BML_BuiInterface *table) {
            return BML_IFACE_HAS(table, BML_BuiInterface, ReadMenuStyle) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, ReadButtonStyle) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, BeginWindow) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, EndWindow) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, At) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, Button) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, Navigate) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, KeyButton) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, YesNoButton) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, RadioButton) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, InputTextButton) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, InputFloatButton) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, InputIntButton) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, ColorButton) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, WrappedText) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, Title) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, SearchBar) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, PlayMenuClickSound) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, ActivateScript) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, BlockKeyboardInput) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, UnblockKeyboardAfterRelease) &&
                   BML_IFACE_HAS(table, BML_BuiInterface, TransitionToScriptAndUnblock);
        });
}

inline int Report(const BML::Mod::Context &ctx, int code) noexcept {
    if (code != BML_OK) {
        char message[96];
        std::snprintf(message, sizeof message, "Bui operation failed (%d)", code);
        (void)ctx.LogError(message);
    }
    return code;
}

template <class F> int Call(const BML::Mod::Context &ctx, F &&f) {
    auto *api = Api();
    return Report(ctx, api ? f(*api) : BML_ERROR_UNAVAILABLE);
}

template <class F> bool Changed(const BML::Mod::Context &ctx, F &&f) {
    uint32_t result = 0;
    return Call(ctx, [&](const auto &api) { return f(api, &result); }) == BML_OK && result != 0;
}

// Stable nesting slots retain their capacity; a label passed to ImGui is never
// invalidated by a reentrant widget in another slot.
struct Label {
    inline static thread_local std::deque<std::string> Slots;
    inline static thread_local size_t Depth = 0;
    size_t Index;

    Label(const char *text, const char *id) : Index(Depth) {
        if (Depth == Slots.size())
            Slots.emplace_back();
        auto &s = Slots[Index];
        s = text;
        s += "###";
        s += id ? id : "";
        ++Depth;
    }

    ~Label() { --Depth; }

    const char *Get() const { return Slots[Index].c_str(); }
};

inline bool Button(const BML::Mod::Context &ctx, ButtonType kind, const char *label, bool *selected,
                   uint32_t flags) {
    uint32_t choice = selected && *selected;
    bool result = Changed(ctx, [&](const auto &api, uint32_t *clicked) {
        return api.Button(ctx.Handle(), kind, label, flags, selected ? &choice : nullptr, clicked);
    });
    if (selected)
        *selected = choice != 0;
    return result;
}
template <class R, class F, bool Ref = std::is_reference_v<R>> struct DrawResult;

template <class R, class F> struct DrawResult<R, F, false> {
    F &Function;
    std::exception_ptr Failure;
    std::optional<R> Value;

    static int BML_CDECL Run(void *data) noexcept {
        auto &s = *static_cast<DrawResult *>(data);
        try {
            s.Value.emplace(std::invoke(s.Function));
            return BML_OK;
        } catch (...) {
            s.Failure = std::current_exception();
            return BML_ERROR_FAIL;
        }
    }

    R Take() { return std::move(*Value); }
};

template <class R, class F> struct DrawResult<R, F, true> {
    F &Function;
    std::exception_ptr Failure;
    std::remove_reference_t<R> *Value = nullptr;

    static int BML_CDECL Run(void *data) noexcept {
        auto &s = *static_cast<DrawResult *>(data);
        try {
            auto &&value = std::invoke(s.Function);
            s.Value = std::addressof(value);
            return BML_OK;
        } catch (...) {
            s.Failure = std::current_exception();
            return BML_ERROR_FAIL;
        }
    }

    R Take() { return static_cast<R>(*Value); }
};

template <class F> struct DrawResult<void, F, false> {
    F &Function;
    std::exception_ptr Failure;

    static int BML_CDECL Run(void *data) noexcept {
        auto &s = *static_cast<DrawResult *>(data);
        try {
            std::invoke(s.Function);
            return BML_OK;
        } catch (...) {
            s.Failure = std::current_exception();
            return BML_ERROR_FAIL;
        }
    }

    void Take() {}
};

template <class F>
auto At(const BML::Mod::Context &ctx, BML_Vec2 p, bool pixels, F &&f) -> std::invoke_result_t<F> {
    using State = DrawResult<std::invoke_result_t<F>, std::remove_reference_t<F>>;
    State state{f};
    int code = Call(
        ctx, [&](const auto &api) { return api.At(ctx.Handle(), p, pixels, State::Run, &state); });
    if (state.Failure)
        std::rethrow_exception(state.Failure);
    if (code != BML_OK)
        throw std::runtime_error("Bui::At failed (" + std::to_string(code) + ")");
    return state.Take();
}
} // namespace Detail

inline BML_Vec2 GetMenuPos(const BML::Mod::Context &ctx) {
    BML_Vec2 p{}, s{};
    BML_Color c{};
    Detail::Call(ctx, [&](const auto &a) { return a.ReadMenuStyle(ctx.Handle(), &p, &s, &c); });
    return p;
}

inline BML_Vec2 GetMenuSize(const BML::Mod::Context &ctx) {
    BML_Vec2 p{}, s{};
    BML_Color c{};
    Detail::Call(ctx, [&](const auto &a) { return a.ReadMenuStyle(ctx.Handle(), &p, &s, &c); });
    return s;
}

inline BML_Color GetMenuColor(const BML::Mod::Context &ctx) {
    BML_Vec2 p{}, s{};
    BML_Color c{};
    Detail::Call(ctx, [&](const auto &a) { return a.ReadMenuStyle(ctx.Handle(), &p, &s, &c); });
    return c;
}

inline BML_Vec2 GetButtonSize(const BML::Mod::Context &ctx, ButtonType t) {
    BML_Vec2 s{};
    float indent = 0;
    Detail::Call(ctx,
                 [&](const auto &a) { return a.ReadButtonStyle(ctx.Handle(), t, &s, &indent); });
    return s;
}

inline float GetButtonIndent(const BML::Mod::Context &ctx, ButtonType t) {
    BML_Vec2 s{};
    float indent = 0;
    Detail::Call(ctx,
                 [&](const auto &a) { return a.ReadButtonStyle(ctx.Handle(), t, &s, &indent); });
    return indent;
}

inline BML_Vec2 CoordToPixel(const BML::Mod::Context &ctx, BML_Vec2 p) {
    auto s = GetMenuSize(ctx);
    return {s.x * 2.5f * p.x, s.y * p.y};
}

inline BML_Vec2 GetButtonSizeInCoord(const BML::Mod::Context &ctx, ButtonType t) {
    auto s = GetButtonSize(ctx, t), vp = CoordToPixel(ctx, {1, 1});
    return {vp.x ? s.x / vp.x : 0, vp.y ? s.y / vp.y : 0};
}

inline float GetButtonIndentInCoord(const BML::Mod::Context &ctx, ButtonType t) {
    auto vp = CoordToPixel(ctx, {1, 1});
    return vp.x ? GetButtonIndent(ctx, t) / vp.x : 0;
}

inline bool MainButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    return Detail::Button(c, BUTTON_MAIN, l, nullptr, f);
}

inline bool OkButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    Detail::Label label("OK", l);
    return Detail::Button(c, BUTTON_BACK, label.Get(), nullptr, f);
}

inline bool BackButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    Detail::Label label("Back", l);
    return Detail::Button(c, BUTTON_BACK, label.Get(), nullptr, f);
}

inline bool OptionButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    return Detail::Button(c, BUTTON_OPTION, l, nullptr, f);
}

inline bool LevelButton(const BML::Mod::Context &c, const char *l, bool *v = nullptr,
                        uint32_t f = 0) {
    return Detail::Button(c, BUTTON_LEVEL, l, v, f);
}

inline bool SmallButton(const BML::Mod::Context &c, const char *l, bool *v = nullptr,
                        uint32_t f = 0) {
    return Detail::Button(c, BUTTON_SMALL, l, v, f);
}

inline bool LeftButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    return Detail::Button(c, BUTTON_LEFT, l, nullptr, f);
}

inline bool RightButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    return Detail::Button(c, BUTTON_RIGHT, l, nullptr, f);
}

inline bool PlusButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    return Detail::Button(c, BUTTON_PLUS, l, nullptr, f);
}

inline bool MinusButton(const BML::Mod::Context &c, const char *l, uint32_t f = 0) {
    return Detail::Button(c, BUTTON_MINUS, l, nullptr, f);
}

inline bool KeyButton(const BML::Mod::Context &c, const char *l, bool *v, BML_KeyChord *key) {
    if (!v)
        return false;
    uint32_t active = *v;
    bool changed = Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.KeyButton(c.Handle(), l, &active, key, out);
    });
    *v = active != 0;
    return changed;
}

inline bool YesNoButton(const BML::Mod::Context &c, const char *l, bool *v) {
    if (!v)
        return false;
    uint32_t value = *v;
    bool changed = Detail::Changed(
        c, [&](const auto &a, uint32_t *out) { return a.YesNoButton(c.Handle(), l, &value, out); });
    *v = value != 0;
    return changed;
}

inline bool RadioButton(const BML::Mod::Context &c, const char *l, int32_t *v,
                        const char *const *items, size_t n) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.RadioButton(c.Handle(), l, v, items, n, out);
    });
}

inline bool InputTextButton(const BML::Mod::Context &c, const char *l, char *b, size_t n,
                            uint32_t f = 0) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.InputTextButton(c.Handle(), l, b, n, f, out);
    });
}

inline bool InputFloatButton(const BML::Mod::Context &c, const char *l, float *v, float step = 0,
                             float fast = 0, const char *format = "%.3f", uint32_t f = 0) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.InputFloatButton(c.Handle(), l, v, step, fast, format, f, out);
    });
}

inline bool InputIntButton(const BML::Mod::Context &c, const char *l, int32_t *v, int32_t step = 1,
                           int32_t fast = 100, uint32_t f = 0) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.InputIntButton(c.Handle(), l, v, step, fast, f, out);
    });
}

inline bool ColorButton(const BML::Mod::Context &c, const char *l, BML_Color *v, uint32_t f = 0) {
    return Detail::Changed(
        c, [&](const auto &a, uint32_t *out) { return a.ColorButton(c.Handle(), l, v, f, out); });
}

inline void WrappedText(const BML::Mod::Context &c, const char *t, float w, float x = 0,
                        float s = 1) {
    Detail::Call(c, [&](const auto &a) { return a.WrappedText(c.Handle(), t, w, x, s); });
}

inline bool NavLeft(const BML::Mod::Context &c, float x = .36f, float y = .124f) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.Navigate(c.Handle(), BUTTON_LEFT, {x, y}, out);
    });
}

inline bool NavRight(const BML::Mod::Context &c, float x = .6038f, float y = .124f) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.Navigate(c.Handle(), BUTTON_RIGHT, {x, y}, out);
    });
}

inline bool NavBack(const BML::Mod::Context &c, float x = .4031f, float y = .85f) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.Navigate(c.Handle(), BUTTON_BACK, {x, y}, out);
    });
}

inline void Title(const BML::Mod::Context &c, const char *t, float y = .13f, float s = 1.5f,
                  uint32_t color = 0xffffffffu) {
    Detail::Call(c, [&](const auto &a) { return a.Title(c.Handle(), t, y, s, color); });
}

inline bool SearchBar(const BML::Mod::Context &c, char *b, size_t n, float x = .4f, float y = .18f,
                      float w = .2f) {
    return Detail::Changed(c, [&](const auto &a, uint32_t *out) {
        return a.SearchBar(c.Handle(), b, n, {x, y}, w, out);
    });
}

inline void PlayMenuClickSound(const BML::Mod::Context &c) {
    Detail::Call(c, [&](const auto &a) { return a.PlayMenuClickSound(c.Handle()); });
}

inline void ActivateScript(const BML::Mod::Context &c, const char *n) {
    Detail::Call(c, [&](const auto &a) { return a.ActivateScript(c.Handle(), n); });
}

inline void BlockKeyboardInput(const BML::Mod::Context &c) {
    Detail::Call(c, [&](const auto &a) { return a.BlockKeyboardInput(c.Handle()); });
}

inline void UnblockKeyboardAfterRelease(const BML::Mod::Context &c) {
    Detail::Call(c, [&](const auto &a) { return a.UnblockKeyboardAfterRelease(c.Handle()); });
}

inline void TransitionToScriptAndUnblock(const BML::Mod::Context &c, const char *n) {
    Detail::Call(c, [&](const auto &a) { return a.TransitionToScriptAndUnblock(c.Handle(), n); });
}

template <class F>
auto AtPixel(const BML::Mod::Context &c, BML_Vec2 p, F &&f) -> std::invoke_result_t<F> {
    return Detail::At(c, p, true, std::forward<F>(f));
}

template <class F>
auto At(const BML::Mod::Context &c, BML_Vec2 p, F &&f) -> std::invoke_result_t<F> {
    return Detail::At(c, p, false, std::forward<F>(f));
}

template <class F>
auto At(const BML::Mod::Context &c, float x, float y, F &&f) -> std::invoke_result_t<F> {
    return At(c, BML_Vec2{x, y}, std::forward<F>(f));
}

template <class F>
void Entries(const BML::Mod::Context &c, F &&f, float x = .4031f, float y = .24f,
             float spacing = .06f, int maxCount = 10) {
    for (int i = 0; i < maxCount; ++i)
        if (!At(c, x, y + spacing * i, [&] { return f(i); }))
            break;
}

inline bool CanPrevPage(int page) {
    return page > 0;
}

inline bool CanNextPage(int page, int count, int size) {
    return page >= 0 && size > 0 && int64_t(count) > (int64_t(page) + 1) * size;
}

inline int CalcPageCount(int count, int size) {
    return size > 0 && count > 0 ? 1 + (count - 1) / size : 0;
}

class Window {
public:
    explicit Window(const BML::Mod::Context &context, std::string name)
        : m_Context(context), m_Name(std::move(name)) {}

    virtual ~Window() = default;

    // Properties
    const std::string &GetName() const { return m_Name; }

    bool IsVisible() const { return m_Visible; }

    // Visibility control
    void Show() {
        if (!m_Visible) {
            m_Visible = true;
            OnShow();
        }
    }

    void Hide() {
        if (m_Visible) {
            m_Visible = false;
            OnHide();
        }
    }

    void Toggle() { m_Visible ? Hide() : Show(); }

    bool Begin() {
        if (m_Begun) {
            Detail::Report(m_Context, BML_ERROR_BUSY);
            return false;
        }
        OnPreBegin();
        bool keepVisible = true;
        uint32_t open = 1;
        uint32_t draw = 0;
        const int code = Detail::Call(m_Context, [&](const auto &api) {
            return api.BeginWindow(m_Context.Handle(), m_Name.c_str(), m_Name.c_str(), &open,
                                   GetFlags(), &draw);
        });
        m_Begun = code == BML_OK;
        const bool notCollapsed = m_Begun && draw;
        keepVisible = open != 0;
        if (!keepVisible)
            m_ShouldHide = true;
        if (notCollapsed) {
            try {
                OnPostBegin();
            } catch (...) {
                EndNative();
                throw;
            }
        }
        return notCollapsed;
    }

    void End() {
        if (!m_Begun)
            return;
        try {
            OnPreEnd();
        } catch (...) {
            EndNative();
            throw;
        }
        EndNative();
        OnPostEnd();
    }

    // Rendering
    void Render() {
        if (!IsVisible() || m_Begun)
            return;

        const bool drawContents = Begin();
        try {
            if (drawContents)
                OnDraw();
        } catch (...) {
            const std::exception_ptr drawFailure = std::current_exception();
            try {
                End();
            } catch (...) {
                // Preserve the drawing failure after making a best effort to
                // complete the window lifecycle and restore ImGui state.
            }
            std::rethrow_exception(drawFailure);
        }
        End();

        if (m_ShouldHide) {
            Hide();
            m_ShouldHide = false;
        }
    }

    // Virtual interface
    virtual uint32_t GetFlags() { return 0; }

    virtual void OnPreBegin() {}

    virtual void OnPostBegin() {}

    virtual void OnDraw() = 0;

    virtual void OnPreEnd() {}

    virtual void OnPostEnd() {}

    virtual void OnShow() {}

    virtual void OnHide() {}

private:
    void EndNative() {
        if (std::exchange(m_Begun, false))
            Detail::Call(m_Context,
                         [&](const auto &api) { return api.EndWindow(m_Context.Handle()); });
    }

    bool m_Begun = false;

protected:
    BML::Mod::Context m_Context;
    std::string m_Name;
    bool m_Visible = true;
    bool m_ShouldHide = false;
};

// Pagination is local list state, independent of a Menu route. Update recalculates
// the number of pages and clamps the current index when a list changes. It invokes
// no callback, so changing pages cannot mutate the Menu lifecycle reentrantly.
class Pagination {
public:
    void Update(int itemCount, int pageSize) {
        m_PageSize = pageSize > 0 ? pageSize : 1;
        m_PageCount = pageSize > 0 ? CalcPageCount(itemCount, pageSize) : 0;
        if (m_PageCount == 0) {
            m_PageIndex = 0;
        } else {
            m_PageIndex = (std::min)(m_PageIndex, m_PageCount - 1);
        }
    }

    int GetPage() const { return m_PageIndex; }

    int GetPageCount() const { return m_PageCount; }

    int GetFirstItem() const { return m_PageIndex * m_PageSize; }

    bool CanPrevious() const { return m_PageIndex > 0; }

    bool CanNext() const { return m_PageIndex + 1 < m_PageCount; }

    bool SetPage(int page) {
        const int next = m_PageCount == 0 ? 0 : (std::max)(0, (std::min)(page, m_PageCount - 1));
        if (next == m_PageIndex)
            return false;
        m_PageIndex = next;
        return true;
    }

    bool Previous() { return SetPage(m_PageIndex - 1); }

    bool Next() { return SetPage(m_PageIndex + 1); }

    bool Reset() { return SetPage(0); }

private:
    int m_PageIndex = 0;
    int m_PageCount = 0;
    int m_PageSize = 1;
};

enum class PageEnterReason {
    Open,
    Push,
    Replace,
    Back,
};

enum class PageLeaveReason {
    Open,
    Push,
    Replace,
    Back,
    Close,
    Remove,
};

// OnFrame returns one action instead of calling back into the Menu. The Menu
// interprets it only after OnFrame and ImGui::End have completed, so navigation
// never destroys or re-enters a Page while that Page is drawing.
class PageAction {
public:
    PageAction() = default;

    static PageAction None() { return {}; }

    static PageAction Open(std::string target) { return PageAction(Type::Open, std::move(target)); }

    static PageAction Push(std::string target) { return PageAction(Type::Push, std::move(target)); }

    static PageAction Replace(std::string target) {
        return PageAction(Type::Replace, std::move(target));
    }

    static PageAction Back() { return PageAction(Type::Back); }

    static PageAction Close() { return PageAction(Type::Close); }

    static PageAction RemoveSelf() { return PageAction(Type::RemoveSelf); }

    bool IsNone() const { return m_Type == Type::None; }

private:
    enum class Type {
        None,
        Open,
        Push,
        Replace,
        Back,
        Close,
        RemoveSelf,
    };

    explicit PageAction(Type type) : m_Type(type) {}

    PageAction(Type type, std::string target) : m_Type(type), m_Target(std::move(target)) {}

    Type m_Type = Type::None;
    std::string m_Target;

    friend class Menu;
};

// A Page is content, not a Window and not a Menu controller. OnEnter and OnLeave
// are lifecycle notifications; only OnFrame can request navigation. There is no
// independent visibility state: a Page is visible exactly when its Menu route is
// current. A Page that needs application state receives that state through its
// constructor rather than retrieving an untyped Menu owner from this base class.
class Page {
public:
    Page() = default;
    virtual ~Page() = default;

    Page(const Page &) = delete;
    Page &operator=(const Page &) = delete;

protected:
    virtual void OnEnter(PageEnterReason) {}

    virtual PageAction OnFrame() = 0;

    virtual void OnLeave(PageLeaveReason) {}

private:
    friend class Menu;
};

// Menu is the sole lifecycle controller for its Pages. The registry uniquely owns
// each Page; current and history contain only route ids. Direct registry or route
// mutation while OnEnter, OnFrame, OnLeave, or a session callback is running
// returns false. OnFrame's returned action is applied after drawing. History is
// capped at 32 routes, and an operation that would exceed it returns false without
// leaving the current Page. Pages follow ImGui's immediate-mode model and are
// submitted on every frame. Entering a route requests keyboard focus for its first
// focusable item; input handoff from a non-ImGui surface belongs to that surface's
// adapter rather than the route controller.
//
// Menu is final so ownership is composed rather than inherited: declare the state
// Pages refer to before the Menu member, and C++ then destroys Menu and its Pages
// before that state. Destroying an open Menu sends OnLeave(Close) and then invokes
// its close callback, preserving the same lifecycle pairing as an explicit Close.
class Menu final {
public:
    using SessionCallback = std::function<void()>;

    explicit Menu(const BML::Mod::Context &context, SessionCallback onOpen = {},
                  SessionCallback onClose = {})
        : m_Context(context),
          m_WindowId("##BuiMenu/" + std::to_string(reinterpret_cast<std::uintptr_t>(this))),
          m_OnOpen(std::move(onOpen)), m_OnClose(std::move(onClose)) {}

    ~Menu() { Shutdown(); }

    Menu(const Menu &) = delete;
    Menu &operator=(const Menu &) = delete;

    // The registry owns each Page under a non-empty route id. Registration fails
    // for null Pages, duplicate ids, or attempts made from a lifecycle callback.
    bool AddPage(std::string id, std::unique_ptr<Page> page) {
        if (m_Dispatching || id.empty() || !page || m_Pages.find(id) != m_Pages.end())
            return false;
        m_Pages.emplace(std::move(id), std::move(page));
        return true;
    }

    template <typename PageType, typename... Args> bool CreatePage(std::string id, Args &&...args) {
        static_assert(std::is_base_of_v<Page, PageType>, "PageType must inherit from Page");
        return AddPage(std::move(id), std::make_unique<PageType>(std::forward<Args>(args)...));
    }

    bool HasPage(const std::string &id) const { return m_Pages.find(id) != m_Pages.end(); }

    bool IsCurrentPage(const std::string &id) const { return m_CurrentPage == id; }

    bool IsOpen() const { return !m_CurrentPage.empty(); }

    // Open starts a root route and clears history. Push remembers the current
    // route, Replace changes it without touching history, and Back restores the
    // most recently pushed route or closes when none remains. All operations
    // return false without changing the route if their request is invalid.
    bool RemovePage(const std::string &id) {
        return Mutate([&]() { return Remove(id); });
    }

    bool Open(const std::string &id) {
        return Mutate([&]() {
            return Activate(id, PageLeaveReason::Open, PageEnterReason::Open, true, false);
        });
    }

    bool Push(const std::string &id) {
        return Mutate([&]() {
            return Activate(id, PageLeaveReason::Push, PageEnterReason::Push, false, true);
        });
    }

    bool Replace(const std::string &id) {
        return Mutate([&]() {
            return Activate(id, PageLeaveReason::Replace, PageEnterReason::Replace, false, false);
        });
    }

    bool Back() {
        return Mutate([&]() { return NavigateBack(); });
    }

    bool Close() {
        return Mutate([&]() { return CloseMenu(); });
    }

    // Draw the current route once during Mod::Instance::OnProcess. A closed Menu is a
    // successful no-op. False means rendering was re-entered, the registry was
    // inconsistent, or OnFrame returned an action that could not be applied.
    bool Render() {
        if (m_Dispatching)
            return false;
        if (m_CurrentPage.empty())
            return true;

        const auto it = m_Pages.find(m_CurrentPage);
        if (it == m_Pages.end())
            return false;

        const std::string pageId = m_CurrentPage;
        Page *page = it->second.get();
        constexpr uint32_t flags = BML_BUI_WINDOW_NO_TITLE | BML_BUI_WINDOW_NO_RESIZE |
                                   BML_BUI_WINDOW_NO_COLLAPSE | BML_BUI_WINDOW_NO_SCROLLBAR |
                                   BML_BUI_WINDOW_NO_BACKGROUND | BML_BUI_WINDOW_NO_MOVE |
                                   BML_BUI_WINDOW_NO_SAVED_SETTINGS | BML_BUI_WINDOW_MENU_BOUNDS;
        m_DrawWindowId = m_WindowId;
        m_DrawWindowId += "/";
        m_DrawWindowId += pageId;
        uint32_t drawContents = 0;
        if (Detail::Call(m_Context, [&](const auto &api) {
                return api.BeginWindow(m_Context.Handle(), m_DrawWindowId.c_str(), "", nullptr,
                                       flags, &drawContents);
            }) != BML_OK)
            return false;
        auto end = [&] {
            Detail::Call(m_Context,
                         [&](const auto &api) { return api.EndWindow(m_Context.Handle()); });
        };
        PageAction action;
        try {
            if (drawContents) {
                DispatchGuard guard(m_Dispatching);
                action = page->OnFrame();
            }
        } catch (...) {
            end();
            throw;
        }
        end();

        return action.IsNone() || ApplyFrameAction(pageId, std::move(action));
    }

protected:
    static constexpr std::size_t MAX_NAVIGATION_DEPTH = 32;

private:
    struct DispatchGuard {
        explicit DispatchGuard(bool &dispatching) : m_Dispatching(dispatching) {
            m_Dispatching = true;
        }

        ~DispatchGuard() { m_Dispatching = false; }

        DispatchGuard(const DispatchGuard &) = delete;
        DispatchGuard &operator=(const DispatchGuard &) = delete;

        bool &m_Dispatching;
    };

    template <typename Operation> bool Mutate(Operation &&operation) {
        if (m_Dispatching)
            return false;

        DispatchGuard guard(m_Dispatching);
        bool success = false;
        try {
            success = operation();
        } catch (...) {
            try {
                SynchronizeSession();
            } catch (...) {
            }
            throw;
        }
        SynchronizeSession();
        return success;
    }

    bool ApplyFrameAction(const std::string &pageId, PageAction action) {
        switch (action.m_Type) {
        case PageAction::Type::None:
            return true;
        case PageAction::Type::Open:
            return Open(action.m_Target);
        case PageAction::Type::Push:
            return Push(action.m_Target);
        case PageAction::Type::Replace:
            return Replace(action.m_Target);
        case PageAction::Type::Back:
            return Back();
        case PageAction::Type::Close:
            return Close();
        case PageAction::Type::RemoveSelf:
            return RemovePage(pageId);
        }
        return false;
    }

    bool Activate(const std::string &target, PageLeaveReason leaveReason,
                  PageEnterReason enterReason, bool clearHistory, bool pushCurrent) {
        if (target.empty() || !HasPage(target))
            return false;
        if (pushCurrent && !m_CurrentPage.empty() && m_PageStack.size() >= MAX_NAVIGATION_DEPTH)
            return false;

        std::string next = target;
        std::string previous = m_CurrentPage;
        if (pushCurrent && !previous.empty())
            m_PageStack.reserve(m_PageStack.size() + 1);
        LeaveCurrent(leaveReason);

        m_CurrentPage = std::move(next);
        m_RequestInitialFocus = true;
        try {
            EnterCurrent(enterReason);
        } catch (...) {
            m_CurrentPage.clear();
            m_PageStack.clear();
            m_RequestInitialFocus = false;
            throw;
        }

        if (clearHistory)
            m_PageStack.clear();
        if (pushCurrent && !previous.empty() && HasPage(previous))
            m_PageStack.push_back(std::move(previous));
        return true;
    }

    bool NavigateBack() {
        if (m_CurrentPage.empty())
            return false;

        LeaveCurrent(PageLeaveReason::Back);

        while (!m_PageStack.empty()) {
            std::string target = std::move(m_PageStack.back());
            m_PageStack.pop_back();
            if (!HasPage(target))
                continue;
            m_CurrentPage = std::move(target);
            m_RequestInitialFocus = true;
            try {
                EnterCurrent(PageEnterReason::Back);
            } catch (...) {
                m_CurrentPage.clear();
                m_PageStack.clear();
                m_RequestInitialFocus = false;
                throw;
            }
            return true;
        }
        m_RequestInitialFocus = false;
        return true;
    }

    bool CloseMenu() {
        if (m_CurrentPage.empty())
            return false;
        LeaveCurrent(PageLeaveReason::Close);
        m_PageStack.clear();
        m_RequestInitialFocus = false;
        return true;
    }

    bool Remove(const std::string &target) {
        const auto it = m_Pages.find(target);
        if (it == m_Pages.end())
            return false;

        if (m_CurrentPage != target) {
            m_PageStack.erase(std::remove(m_PageStack.begin(), m_PageStack.end(), target),
                              m_PageStack.end());
            m_Pages.erase(it);
            return true;
        }

        it->second->OnLeave(PageLeaveReason::Remove);
        m_CurrentPage.clear();
        m_PageStack.clear();
        m_RequestInitialFocus = false;
        m_Pages.erase(it);
        return true;
    }

    void LeaveCurrent(PageLeaveReason reason) {
        if (m_CurrentPage.empty())
            return;
        const auto it = m_Pages.find(m_CurrentPage);
        if (it != m_Pages.end())
            it->second->OnLeave(reason);
        m_CurrentPage.clear();
    }

    void EnterCurrent(PageEnterReason reason) {
        const auto it = m_Pages.find(m_CurrentPage);
        if (it == m_Pages.end()) {
            m_CurrentPage.clear();
            return;
        }
        it->second->OnEnter(reason);
    }

    void SynchronizeSession() {
        const bool active = !m_CurrentPage.empty();
        if (active && !m_SessionOpen) {
            m_SessionOpen = true;
            if (m_OnOpen)
                m_OnOpen();
        } else if (!active && m_SessionOpen) {
            m_SessionOpen = false;
            if (m_OnClose)
                m_OnClose();
        }
    }

    void Shutdown() noexcept {
        m_Dispatching = true;

        if (!m_CurrentPage.empty()) {
            const auto it = m_Pages.find(m_CurrentPage);
            if (it != m_Pages.end()) {
                try {
                    it->second->OnLeave(PageLeaveReason::Close);
                } catch (...) {
                }
            }
        }

        m_CurrentPage.clear();
        m_PageStack.clear();
        m_RequestInitialFocus = false;

        if (m_SessionOpen) {
            m_SessionOpen = false;
            if (m_OnClose) {
                try {
                    m_OnClose();
                } catch (...) {
                }
            }
        }

        m_Pages.clear();
    }

    BML::Mod::Context m_Context;
    std::string m_DrawWindowId;
    std::string m_CurrentPage;
    std::string m_WindowId;
    SessionCallback m_OnOpen;
    SessionCallback m_OnClose;
    bool m_SessionOpen = false;
    bool m_Dispatching = false;
    bool m_RequestInitialFocus = false;
    std::vector<std::string> m_PageStack;
    std::unordered_map<std::string, std::unique_ptr<Page>> m_Pages;
};
} // namespace Bui

// Keep the existing global namespace available for source compatibility.
namespace BML {
namespace Bui = ::Bui;
}

#endif // BML_BUI_HPP
