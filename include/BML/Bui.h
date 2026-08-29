// Ballance-looking widgets on top of ImGui: the buttons here draw the game's own
// button images and font and play the game's click sound, so a Mod's menu page can
// look like the ones the game ships. Everything else ImGui offers is available
// alongside it, since this is the same ImGui the loader draws its own windows with,
// linked out of the loader rather than built into the Mod.
//
// Call all of it from IMod::OnProcess, the one callback that runs inside the loader's
// ImGui frame. From anywhere else, OnRender included, there is no frame to add to and
// these functions walk ImGui state that is not set up. Leave the ImGui stacks as they
// were found: a PushStyleColor or a Begin left open here breaks the loader's own
// windows, not just the Mod's.
//
// Positions are fractions of the main viewport, 0 to 1, rather than pixels, so a
// layout keeps its proportions at any resolution. At() moves the ImGui cursor there,
// runs the drawing passed to it, and puts the cursor back, which is how a widget goes
// to an absolute spot without a window layout getting in the way. CoordToPixel
// converts, and AtPixel is the same thing in pixels.
//
// The default coordinates on Entries, NavLeft, NavRight, NavBack, Title, and
// SearchBar are the ones the game's own menus use, so a page built out of them lines
// up with the original without measuring anything.
//
// The buttons return true on the frame they are clicked, as ImGui's do, and take the
// label as the ImGui id, so two buttons in one window need different labels or a
// ##suffix.
#ifndef BML_BUI_H
#define BML_BUI_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "imgui.h"

#include "CKContext.h"

#include "BML/Defines.h"

namespace Bui {
    // Forward declarations
    class Window;
    class Page;
    class Menu;

    // Which of the game's button images to draw, and with it the size and the
    // indent: MAIN is the wide one of the main menu, BACK the one at the bottom of a
    // page, OPTION a setting row, LEVEL a level entry, KEY a key picker, SMALL a
    // narrow one, LEFT and RIGHT the page arrows, and PLUS and MINUS the small square
    // ones. The button functions below pick the type themselves; these values are for
    // GetButtonSize and AddButtonImage, which is where a Mod drawing its own widget
    // needs them.
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

    // =============================================================================
    // CORE POSITIONING SYSTEM
    // =============================================================================

    // Primary positioning function - void return type
    template <typename Func>
    std::enable_if_t<std::is_void_v<std::invoke_result_t<Func>>>
    At(float x, float y, Func &&func) {
        const ImVec2 savedPos = ImGui::GetCursorScreenPos();
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * x, vpSize.y * y));
        func();
        ImGui::SetCursorScreenPos(savedPos);
        ImGui::Dummy(ImVec2(0.0f, 0.0f)); // ImGui 1.89+ compliance
    }

    // Primary positioning function - return value type
    template <typename Func>
    std::enable_if_t<!std::is_void_v<std::invoke_result_t<Func>>, std::invoke_result_t<Func>>
    At(float x, float y, Func &&func) {
        const ImVec2 savedPos = ImGui::GetCursorScreenPos();
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * x, vpSize.y * y));
        auto result = func();
        ImGui::SetCursorScreenPos(savedPos);
        ImGui::Dummy(ImVec2(0.0f, 0.0f)); // ImGui 1.89+ compliance
        return result;
    }

    // ImVec2 overloads
    template <typename Func>
    auto At(const ImVec2 &pos, Func &&func) -> decltype(At(pos.x, pos.y, std::forward<Func>(func))) {
        return At(pos.x, pos.y, std::forward<Func>(func));
    }

    // Pixel coordinate version (for internal use)
    template <typename Func>
    auto AtPixel(const ImVec2 &pixelPos, Func &&func) -> decltype(func()) {
        const ImVec2 savedPos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(pixelPos);

        if constexpr (std::is_void_v<decltype(func())>) {
            func();
            ImGui::SetCursorScreenPos(savedPos);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        } else {
            auto result = func();
            ImGui::SetCursorScreenPos(savedPos);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
            return result;
        }
    }

    // =============================================================================
    // CORE FUNCTIONS
    // =============================================================================

    // Utility functions
    inline ImVec2 CoordToPixel(const ImVec2 &coord) {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        return {vpSize.x * coord.x, vpSize.y * coord.y};
    }

    // The loader calls these itself, on the shared textures, materials, and sounds
    // every Bui widget draws with, and destroys them at shutdown. They are exported
    // because the loader's own menus are built the same way as a Mod's, not because a
    // Mod has anything to call them for: calling one again replaces what every other
    // Mod is already drawing with.
    BML_EXPORT bool InitTextures(CKContext *context);
    BML_EXPORT bool InitMaterials(CKContext *context);
    BML_EXPORT bool InitSounds(CKContext *context);
    BML_EXPORT void CleanupResources(CKContext *context);

    // The ImGui context the loader draws in, or null before it has one. A Mod
    // drawing from OnProcess is already inside it and needs neither this nor the
    // scope below; they are for code that reaches ImGui from somewhere else, such as
    // a callback the Mod arranged itself, and even then only the loader's own frame
    // can be drawn into.
    BML_EXPORT ImGuiContext *GetImGuiContext();

    // RAII helper for ImGui context switching
    class ImGuiContextScope {
    public:
        explicit ImGuiContextScope(ImGuiContext *newContext = nullptr)
            : m_PreviousContext(ImGui::GetCurrentContext()) {
            ImGui::SetCurrentContext(newContext ? newContext : GetImGuiContext());
        }

        ~ImGuiContextScope() {
            ImGui::SetCurrentContext(m_PreviousContext);
        }

        ImGuiContextScope(const ImGuiContextScope &) = delete;
        ImGuiContextScope &operator=(const ImGuiContextScope &) = delete;

    private:
        ImGuiContext *m_PreviousContext;
    };

    // Creates a CKTexture named id from filename, which is resolved through the
    // game's bitmap search paths, so a plain name finds a file the game or the Mod
    // dropped in Textures. Null when the file was not found or the image failed to
    // load. The texture belongs to the CKContext from then on and survives until
    // something destroys it, so create it once rather than per frame.
    BML_EXPORT CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot = 0);

    // Where the game puts its menu panel and what it tints it with: the middle 40
    // percent of the viewport, full height, over a black 60 percent overlay. Use them
    // to place a window that should sit where the game's menus sit.
    BML_EXPORT ImVec2 GetMenuPos();
    BML_EXPORT ImVec2 GetMenuSize();
    BML_EXPORT ImVec4 GetMenuColor();

    // How much room a button of that type takes, for a Mod laying out around one.
    // GetButtonSize and GetButtonIndent answer in pixels for the current viewport,
    // the InCoord pair in viewport fractions. An out-of-range type gives zero rather
    // than failing.
    BML_EXPORT ImVec2 GetButtonSize(ButtonType type);
    BML_EXPORT float GetButtonIndent(ButtonType type);

    BML_EXPORT ImVec2 GetButtonSizeInCoord(ButtonType type);
    BML_EXPORT float GetButtonIndentInCoord(ButtonType type);

    // Between the game's key numbering and ImGui's, which is what a Mod storing a key
    // in its config needs: an IProperty of type KEY holds a CKKEYBOARD while ImGui
    // reports an ImGuiKey. Only the keys both sides name are mapped; the rest come
    // back as ImGuiKey_None, or as the CKKEYBOARD 0, which is no key at all. Check the
    // result rather than passing it on.
    //
    // KeyChordToString writes the printable form of a chord, Ctrl+Shift+A and the
    // like, into buf, truncating to size rather than failing, and returns false for an
    // empty chord or a key ImGui cannot name. SetKeyChordFromIO writes whatever the
    // player is pressing this frame into key_chord and returns true only on the frame
    // a key other than the modifiers arrives, which is how KeyButton records a
    // binding; it leaves key_chord alone and returns false otherwise.
    BML_EXPORT ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key);
    BML_EXPORT CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key);
    BML_EXPORT bool KeyChordToString(ImGuiKeyChord key_chord, char *buf, size_t size);
    BML_EXPORT bool SetKeyChordFromIO(ImGuiKeyChord *key_chord);

    // The game's menu click. The buttons here play it themselves; call it for a widget
    // of the Mod's own that should sound like the rest of the menu.
    BML_EXPORT void PlayMenuClickSound();

    // The button images on their own, for a Mod building a widget the functions below
    // do not cover. These only draw: they add nothing to the ImGui layout and report
    // no clicks, so pair them with ImGui::ItemAdd and ImGui::ButtonBehavior. There are
    // three images and state picks between them: 1 is the lit one the game shows under
    // the mouse, 2 is the third one the game uses for a row that is not the current
    // choice, and anything else, 0 included, is the plain one. The bool overloads pass
    // 1 for true and 0 for false, so they never reach the third image. pos is the top
    // left corner in pixels and the size comes from the type. The overloads taking
    // text draw it inside the image, shortened with an ellipsis when it does not fit,
    // centred by ImGuiStyleVar_ButtonTextAlign unless text_align says otherwise.
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text, const ImVec2 &text_align);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text, const ImVec2 &text_align);

    // =============================================================================
    // BUTTON FUNCTIONS
    // =============================================================================

    // One row each, the size of the matching ButtonType, taking the whole width the
    // type asks for and advancing the ImGui cursor by it. All of them return true on
    // the frame they are clicked and play the menu click then.
    //
    // MainButton and OptionButton draw label as their text, so the label is what the
    // player reads and a ##suffix is the only way to give two of them different ids.
    // OkButton and BackButton always read OK and Back, and LeftButton, RightButton,
    // PlusButton, and MinusButton have no text at all, so for those the label is the id
    // and nothing else. Text wider than the button is cut short with an ellipsis,
    // except on LEVEL, where it scrolls while the mouse is on the row.
    //
    // LevelButton and SmallButton take a flag saying whether this row is the one
    // currently chosen, which is how the game's level list and its Yes/No pairs show
    // which entry is live: a true flag draws the plain image, false the third one, and
    // the mouse on the row draws the lit one and sets the flag to true. The widget
    // never clears a flag, so the caller clears the other rows' flags itself. With
    // nullptr the row draws as the third image until the mouse reaches it.
    //
    // flags goes to ImGui::ButtonBehavior, so ImGuiButtonFlags_PressedOnClick,
    // _Repeat, and the rest of them work as they do on ImGui::Button.
    BML_EXPORT bool MainButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool OkButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool BackButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool OptionButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool LevelButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool SmallButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool LeftButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool RightButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool PlusButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool MinusButton(const char *label, ImGuiButtonFlags flags = 0);

    // Rows holding a value the player changes, all three the width of an OPTION row
    // with the label drawn on the left. The Mod owns the value and passes a pointer to
    // it; these write through it and the change is visible on the next frame.
    //
    // KeyButton records a key binding. toggled is the Mod's own bool saying the row is
    // listening: clicking the row sets it, the next key the player presses is written
    // into key_chord and clears it, and a click anywhere else cancels and clears it as
    // well. It returns true in both of those cases, so read key_chord rather than
    // treating a true return as a new binding.
    //
    // YesNoButton draws a Yes and a No next to the label and returns true when the
    // player picks the one that was not set. Clicking the row itself outside those two
    // also flips v, but reports nothing, so read v rather than counting the returns.
    //
    // RadioButton cycles current_item through items with a minus and a plus at the end
    // of the row, wrapping past either end, and returns true only on a frame the index
    // moved. items has to hold items_count entries that outlive the call; a null entry
    // draws as empty, and a current_item outside the range is shown as 0 but only
    // written back once the player moves it.
    BML_EXPORT bool KeyButton(const char *label, bool *toggled, ImGuiKeyChord *key_chord);
    BML_EXPORT bool YesNoButton(const char *label, bool *v);
    BML_EXPORT bool RadioButton(const char *label, int *current_item, const char *const items[], int items_count);

    // An OPTION row with the label on the left and an ImGui input field in it, which is
    // ImGui::InputText, InputFloat, and InputInt with the game's look around them. They
    // return what those return, meaning true on every frame the player changed the
    // value rather than once when done, so a Mod saving to its config on each true
    // writes on every keystroke; pass ImGuiInputTextFlags_EnterReturnsTrue for the
    // other behaviour. buf has to hold buf_size bytes and is both what is shown and
    // where the text ends up. The ImGui item ends inside a group these close
    // themselves, so ImGui::IsItemDeactivatedAfterEdit and the rest of the item queries
    // asked after the call answer about the row, not about the field. The step pair
    // reaches ImGui unchanged, which is why InputIntButton shows the small plus and
    // minus of ImGui and InputFloatButton, defaulting to a step of 0, does not.
    BML_EXPORT bool InputTextButton(const char *label, char *buf, size_t buf_size,
                                    ImGuiInputTextFlags flags = 0,
                                    ImGuiInputTextCallback callback = nullptr,
                                    void *user_data = nullptr);
    BML_EXPORT bool InputFloatButton(const char *label, float *v, float step = 0.0f,
                                     float step_fast = 0.0f, const char *format = "%.3f",
                                     ImGuiInputTextFlags flags = 0);
    BML_EXPORT bool InputIntButton(const char *label, int *v, int step = 1,
                                   int step_fast = 100, ImGuiInputTextFlags flags = 0);

    // Plain text, centred and broken across lines, for the paragraph on a page rather
    // than for a widget. Unlike the rest of this header the measurements here are
    // pixels, since that is what ImGui wraps by: width is how wide the block may get,
    // and 0 means the width left in the window. baseX is where the block starts,
    // measured inside the window as ImGui::SetCursorPosX takes it, and 0 means wherever
    // the cursor already is, so an exact 0 cannot be asked for. scale multiplies the
    // font size for this call alone. The text is added to the window, so the cursor
    // moves down by as many lines as it took.
    BML_EXPORT void WrappedText(const char *text, float width, float baseX = 0.0f, float scale = 1.0f);

    // =============================================================================
    // HIGH-LEVEL UI HELPERS
    // =============================================================================

    // The column of rows a menu page is made of. entryFunc is called with the row
    // number, 0 first, at each position down the column, and returns whether to go on:
    // false stops the run there, which is how a page shows fewer rows than maxCount
    // when it has run out of entries. It is called at most maxCount times, and the row
    // number is the position on the page, so a paged list adds pageIndex * pageSize to
    // it itself. Nothing is drawn for a row the function skipped.
    //
    // The defaults are where the game's own lists sit, one row every 6 percent of the
    // viewport height starting at 24 percent, ten of them. Each call is wrapped in At,
    // so the row draws at that spot whatever the window layout is doing.
    template <typename Func>
    void Entries(Func &&entryFunc, float startX = 0.4031f, float startY = 0.24f, float spacing = 0.06f, int maxCount = 10) {
        for (int i = 0; i < maxCount; ++i) {
            bool shouldContinue = true;
            At(startX, startY + i * spacing, [&]() {
                shouldContinue = entryFunc(i);
            });
            if (!shouldContinue) break;
        }
    }

    // The three arrows the game's pages carry, drawn where the game draws them: the
    // two at the top for the previous and next page and the Back one at the bottom.
    // Each returns true when its button is clicked and also when the key that goes with
    // it is pressed, PageUp, PageDown, and Escape, and that key is read whether or not
    // the mouse is anywhere near the button. So a page calling NavBack closes on Escape
    // without arranging anything, and a page that wants Escape for something else does
    // not call it.
    //
    // Call one only when the move it stands for is possible, since it draws the arrow
    // as well as reading it. Pagination below keeps the corresponding list state.
    BML_EXPORT bool NavLeft(float x = 0.36f, float y = 0.124f);
    BML_EXPORT bool NavRight(float x = 0.6038f, float y = 0.124f);
    BML_EXPORT bool NavBack(float x = 0.4031f, float y = 0.85f);

    // Arithmetic on a paged list, drawing nothing: whether either arrow belongs on the
    // page, and how many pages a list of that length needs. pageIndex counts from 0. A
    // pageSize of 0 or less, or an empty list, is no pages at all rather than one.
    inline bool CanPrevPage(int pageIndex) {
        return pageIndex > 0;
    }

    inline bool CanNextPage(int pageIndex, int totalCount, int pageSize) {
        if (pageSize <= 0)
            return false;
        // There is a next page only if items remain beyond current page
        return totalCount > (pageIndex + 1) * pageSize;
    }

    inline int CalcPageCount(int totalCount, int pageSize) {
        if (pageSize <= 0)
            return 0;
        if (totalCount <= 0)
            return 0;
        // ceil(totalCount / pageSize)
        return (totalCount + pageSize - 1) / pageSize;
    }

    // Opening a menu of one's own over the game means keeping the game from reading the
    // same keys, and leaving it means handing them back without the keypress that
    // closed the menu reaching the game underneath. These do that.
    //
    // BlockKeyboardInput takes the keyboard away from the game through the same block
    // InputHook::AcquireBlock gives, so the game's own scripts stop seeing keys while
    // ImGui still does. UnblockKeyboardAfterRelease gives it back, but not at once: it
    // waits, one game frame at a time, until Escape and Enter are both up, so the press
    // that left the menu is over before the game can act on it.
    //
    // The underlying block is shared by the whole loader and reference-counted. Every
    // successful public BlockKeyboardInput call has one anonymous share and must have
    // one matching UnblockKeyboardAfterRelease call. Loader-owned menus use separate
    // owner shares, so an unmatched public close cannot release a menu's block.
    //
    // ActivateScript starts one of the game's own scripts by name, the way the game's
    // menu buttons move from one screen to the next, and does nothing when there is no
    // script by that name or no scene running.
    // TransitionToScriptAndUnblock is ActivateScript followed by
    // UnblockKeyboardAfterRelease, which is the whole of closing a Mod's page and
    // handing the player back to a game screen.
    BML_EXPORT void BlockKeyboardInput();
    BML_EXPORT void ActivateScript(const char *scriptName);
    BML_EXPORT void UnblockKeyboardAfterRelease();
    BML_EXPORT void TransitionToScriptAndUnblock(const char *scriptName);

    // Title writes the heading of a page, centred on the viewport with y the fraction
    // of its height to put it at and scale a multiplier on the font size. It draws into
    // the foreground list, so it lands over every window including the loader's own and
    // needs no window of its own, and it adds nothing to any layout.
    //
    // SearchBar is an ImGui text field with the menu tint behind it, x, y, and width in
    // viewport fractions, returning true on every frame the text changed. It gives the
    // field a fixed id, so two of them in one window are the same field; put the second
    // one in a window of its own or use ImGui::PushID around it.
    BML_EXPORT void Title(const char *text, float y = 0.13f, float scale = 1.5f, ImU32 color = IM_COL32_WHITE);
    BML_EXPORT bool SearchBar(char *buffer, size_t bufferSize, float x = 0.4f, float y = 0.18f, float width = 0.2f);

    // =============================================================================
    // UI CLASSES
    // =============================================================================
    //
    // Three optional building blocks sit above the drawing functions: Window is a
    // standalone ImGui window with a shown flag, Page is the content and lifecycle of
    // one menu route, and Menu owns, renders, and navigates a set of Pages. Page does
    // not inherit Window and cannot open or close itself. These classes are header-only,
    // so a Mod using them compiles their code into itself rather than calling into the
    // loader, and none of them is needed to use the drawing functions above.
    //
    // All of them are only touched from IMod::OnProcess, like the rest of the header:
    // Window::Render or Menu::Render is what a Mod calls there; they run OnDraw or the
    // current Page's OnFrame respectively.

    // An ImGui window with a shown flag, drawn by calling Render once a frame. Render
    // does nothing while hidden, so a Mod calls it unconditionally and switches with
    // Show, Hide, and Toggle; those run OnShow and OnHide only when the flag actually
    // changes. ImGui::Begin is asked for a close box, so a window that kept its title
    // bar has one, and clicking it hides the window once the frame is finished, which is
    // also when OnHide arrives.
    //
    // OnDraw is the one function a derived class has to write, and it is called inside
    // the window, only when the window is not collapsed. The rest are there to hook:
    // GetFlags for the ImGuiWindowFlags, OnPreBegin for the calls that have to come
    // before ImGui::Begin such as SetNextWindowPos, and OnPostBegin and OnPreEnd for
    // drawing that has to come before or after OnDraw inside the window. OnPreEnd runs
    // even on a collapsed window, since ImGui::End always follows ImGui::Begin.
    //
    // name is passed to ImGui::Begin, so it is both the title and the id ImGui keeps the
    // window's position under. Those ids are shared by every Mod drawing in the loader's
    // context, so put something of the Mod's own in the name.
    class Window {
    public:
        explicit Window(std::string name) : m_Name(std::move(name)), m_Visible(true), m_ShouldHide(false) {}

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
            OnPreBegin();
            bool keepVisible = true;
            const bool notCollapsed = ImGui::Begin(m_Name.c_str(), &keepVisible, GetFlags());
            if (!keepVisible) {
                m_ShouldHide = true;
            }
            if (notCollapsed)
                OnPostBegin();
            return notCollapsed;
        }

        void End() {
            OnPreEnd();
            ImGui::End();
            OnPostEnd();
        }

        // Rendering
        void Render() {
            if (!IsVisible()) return;

            if (Begin())
                OnDraw();
            End();

            if (m_ShouldHide) {
                Hide();
                m_ShouldHide = false;
            }
        }

        // Virtual interface
        virtual ImGuiWindowFlags GetFlags() { return 0; }

        virtual void OnPreBegin() {}
        virtual void OnPostBegin() {}
        virtual void OnDraw() = 0;
        virtual void OnPreEnd() {}
        virtual void OnPostEnd() {}
        virtual void OnShow() {}
        virtual void OnHide() {}

    protected:
        std::string m_Name;
        bool m_Visible;
        bool m_ShouldHide;
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
                m_PageIndex = std::min(m_PageIndex, m_PageCount - 1);
            }
        }

        int GetPage() const { return m_PageIndex; }
        int GetPageCount() const { return m_PageCount; }
        int GetFirstItem() const { return m_PageIndex * m_PageSize; }
        bool CanPrevious() const { return m_PageIndex > 0; }
        bool CanNext() const { return m_PageIndex + 1 < m_PageCount; }

        bool SetPage(int page) {
            const int next = m_PageCount == 0
                ? 0
                : std::max(0, std::min(page, m_PageCount - 1));
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
        static PageAction Replace(std::string target) { return PageAction(Type::Replace, std::move(target)); }
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
        PageAction(Type type, std::string target)
            : m_Type(type), m_Target(std::move(target)) {}

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
    // leaving the current Page.
    //
    // Menu is final so ownership is composed rather than inherited: declare the state
    // Pages refer to before the Menu member, and C++ then destroys Menu and its Pages
    // before that state. Destroying an open Menu sends OnLeave(Close) and then invokes
    // its close callback, preserving the same lifecycle pairing as an explicit Close.
    class Menu final {
    public:
        using SessionCallback = std::function<void()>;

        explicit Menu(SessionCallback onOpen = {}, SessionCallback onClose = {})
            : m_WindowId("##BuiMenu/" +
                         std::to_string(reinterpret_cast<std::uintptr_t>(this))),
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

        template <typename PageType, typename... Args>
        bool CreatePage(std::string id, Args &&... args) {
            static_assert(std::is_base_of_v<Page, PageType>, "PageType must inherit from Page");
            return AddPage(std::move(id),
                           std::make_unique<PageType>(std::forward<Args>(args)...));
        }

        bool HasPage(const std::string &id) const {
            return m_Pages.find(id) != m_Pages.end();
        }

        bool IsCurrentPage(const std::string &id) const {
            return m_CurrentPage == id;
        }

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
                return Activate(id, PageLeaveReason::Open,
                                PageEnterReason::Open, true, false);
            });
        }

        bool Push(const std::string &id) {
            return Mutate([&]() {
                return Activate(id, PageLeaveReason::Push,
                                PageEnterReason::Push, false, true);
            });
        }

        bool Replace(const std::string &id) {
            return Mutate([&]() {
                return Activate(id, PageLeaveReason::Replace,
                                PageEnterReason::Replace, false, false);
            });
        }

        bool Back() { return Mutate([&]() { return NavigateBack(); }); }
        bool Close() { return Mutate([&]() { return CloseMenu(); }); }

        // Draw the current route once during IMod::OnProcess. A closed Menu is a
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
            const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(vpSize.x, vpSize.y));

            constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoSavedSettings;

            const bool drawContents = ImGui::Begin(m_WindowId.c_str(), nullptr, flags);
            PageAction action;
            try {
                if (drawContents) {
                    ImGuiIdGuard idGuard(page);
                    DispatchGuard guard(m_Dispatching);
                    action = page->OnFrame();
                }
            } catch (...) {
                ImGui::End();
                throw;
            }
            ImGui::End();

            return action.IsNone() || ApplyFrameAction(pageId, std::move(action));
        }

    protected:
        static constexpr size_t MAX_NAVIGATION_DEPTH = 32;

    private:
        struct ImGuiIdGuard {
            explicit ImGuiIdGuard(const void *id) { ImGui::PushID(id); }
            ~ImGuiIdGuard() { ImGui::PopID(); }

            ImGuiIdGuard(const ImGuiIdGuard &) = delete;
            ImGuiIdGuard &operator=(const ImGuiIdGuard &) = delete;
        };

        struct DispatchGuard {
            explicit DispatchGuard(bool &dispatching) : Dispatching(dispatching) {
                Dispatching = true;
            }

            ~DispatchGuard() { Dispatching = false; }

            DispatchGuard(const DispatchGuard &) = delete;
            DispatchGuard &operator=(const DispatchGuard &) = delete;

            bool &Dispatching;
        };

        template <typename Operation>
        bool Mutate(Operation &&operation) {
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

        bool Activate(const std::string &target,
                      PageLeaveReason leaveReason,
                      PageEnterReason enterReason,
                      bool clearHistory,
                      bool pushCurrent) {
            if (target.empty() || !HasPage(target))
                return false;
            if (pushCurrent && !m_CurrentPage.empty() &&
                m_PageStack.size() >= MAX_NAVIGATION_DEPTH)
                return false;

            const std::string previous = m_CurrentPage;
            LeaveCurrent(leaveReason);

            m_CurrentPage = target;
            try {
                EnterCurrent(enterReason);
            } catch (...) {
                m_CurrentPage.clear();
                m_PageStack.clear();
                throw;
            }

            if (clearHistory)
                m_PageStack.clear();
            if (pushCurrent && !previous.empty() && HasPage(previous))
                m_PageStack.push_back(previous);
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
                try {
                    EnterCurrent(PageEnterReason::Back);
                } catch (...) {
                    m_CurrentPage.clear();
                    m_PageStack.clear();
                    throw;
                }
                return true;
            }
            return true;
        }

        bool CloseMenu() {
            if (m_CurrentPage.empty())
                return false;
            LeaveCurrent(PageLeaveReason::Close);
            m_PageStack.clear();
            return true;
        }

        bool Remove(const std::string &target) {
            const auto it = m_Pages.find(target);
            if (it == m_Pages.end())
                return false;

            if (m_CurrentPage != target) {
                m_PageStack.erase(
                    std::remove(m_PageStack.begin(), m_PageStack.end(), target),
                    m_PageStack.end());
                m_Pages.erase(it);
                return true;
            }

            it->second->OnLeave(PageLeaveReason::Remove);
            m_CurrentPage.clear();
            m_PageStack.clear();
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

        std::string m_CurrentPage;
        std::string m_WindowId;
        SessionCallback m_OnOpen;
        SessionCallback m_OnClose;
        bool m_SessionOpen = false;
        bool m_Dispatching = false;
        std::vector<std::string> m_PageStack;
        std::unordered_map<std::string, std::unique_ptr<Page>> m_Pages;
    };
}

#endif // BML_BUI_H
