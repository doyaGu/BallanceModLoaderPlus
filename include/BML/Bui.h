#ifndef BML_BUI_H
#define BML_BUI_H

#include <cassert>
#include <string>
#include <stack>
#include <unordered_map>
#include <memory>
#include <type_traits>

#include "imgui.h"

#include "CKContext.h"

#include "BML/Defines.h"

namespace Bui {
    // Forward declarations
    class Window;
    class Page;
    class Menu;

    // Button types
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

    // Initialization functions
    BML_EXPORT bool InitTextures(CKContext *context);
    BML_EXPORT bool InitMaterials(CKContext *context);
    BML_EXPORT bool InitSounds(CKContext *context);

    // Context management
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

    // Texture and material functions
    BML_EXPORT CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot = 0);

    // Menu layout functions
    BML_EXPORT ImVec2 GetMenuPos();
    BML_EXPORT ImVec2 GetMenuSize();
    BML_EXPORT ImVec4 GetMenuColor();

    // Button layout functions
    BML_EXPORT ImVec2 GetButtonSize(ButtonType type);
    BML_EXPORT float GetButtonIndent(ButtonType type);

    BML_EXPORT ImVec2 GetButtonSizeInCoord(ButtonType type);
    BML_EXPORT float GetButtonIndentInCoord(ButtonType type);

    // Key conversion functions
    BML_EXPORT ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key);
    BML_EXPORT CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key);
    BML_EXPORT bool KeyChordToString(ImGuiKeyChord key_chord, char *buf, size_t size);
    BML_EXPORT bool SetKeyChordFromIO(ImGuiKeyChord *key_chord);

    // Sound functions
    BML_EXPORT void PlayMenuClickSound();

    // Image drawing functions
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text, const ImVec2 &text_align);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text, const ImVec2 &text_align);

    // =============================================================================
    // BUTTON FUNCTIONS
    // =============================================================================

    // Basic buttons
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

    // Special buttons
    BML_EXPORT bool KeyButton(const char *label, bool *toggled, ImGuiKeyChord *key_chord);
    BML_EXPORT bool YesNoButton(const char *label, bool *v);
    BML_EXPORT bool RadioButton(const char *label, int *current_item, const char *const items[], int items_count);

    // Input buttons
    BML_EXPORT bool InputTextButton(const char *label, char *buf, size_t buf_size,
                                    ImGuiInputTextFlags flags = 0,
                                    ImGuiInputTextCallback callback = nullptr,
                                    void *user_data = nullptr);
    BML_EXPORT bool InputFloatButton(const char *label, float *v, float step = 0.0f,
                                     float step_fast = 0.0f, const char *format = "%.3f",
                                     ImGuiInputTextFlags flags = 0);
    BML_EXPORT bool InputIntButton(const char *label, int *v, int step = 1,
                                   int step_fast = 100, ImGuiInputTextFlags flags = 0);

    // Text rendering helper
    BML_EXPORT void WrappedText(const char *text, float width, float baseX = 0.0f, float scale = 1.0f);

    // =============================================================================
    // HIGH-LEVEL UI HELPERS
    // =============================================================================

    // Entry list renderer
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

    // Navigation helpers
    BML_EXPORT bool NavLeft(float x = 0.36f, float y = 0.124f);
    BML_EXPORT bool NavRight(float x = 0.6038f, float y = 0.124f);
    BML_EXPORT bool NavBack(float x = 0.4031f, float y = 0.85f);

    // Pagination helpers
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

    // Menu lifecycle helpers
    BML_EXPORT void BlockKeyboardInput();
    BML_EXPORT void ActivateScript(const char *scriptName);
    BML_EXPORT void UnblockKeyboardAfterRelease();
    BML_EXPORT void TransitionToScriptAndUnblock(const char *scriptName);

    // UI component helpers
    BML_EXPORT void Title(const char *text, float y = 0.13f, float scale = 1.5f, ImU32 color = IM_COL32_WHITE);
    BML_EXPORT bool SearchBar(char *buffer, size_t bufferSize, float x = 0.4f, float y = 0.18f, float width = 0.2f);

    // =============================================================================
    // UI CLASSES
    // =============================================================================

    // Base window class
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

    // Page class for menu pages
    class Page : public Window {
    public:
        explicit Page(std::string name) : Window(std::move(name)), m_Title(m_Name), m_Menu(nullptr) { Hide(); }
        Page(std::string name, std::string title) : Window(std::move(name)), m_Title(std::move(title)), m_Menu(nullptr) { Hide(); }

        // Properties
        const std::string &GetTitle() const { return m_Title; }
        void SetTitle(const std::string &title) { m_Title = title; }

        // Menu integration
        Menu *GetMenu() const { return m_Menu; }
        void SetMenu(Menu *menu) { m_Menu = menu; }

        // Page navigation
        int GetPage() const { return m_PageIndex; }
        int GetPageCount() const { return m_PageCount; }

        void SetPage(int page) {
            page = std::max(0, std::min(page, m_PageCount - 1));
            if (m_PageIndex != page) {
                int oldPage = m_PageIndex;
                m_PageIndex = page;
                OnPageChanged(m_PageIndex, oldPage);
            }
        }

        void NextPage() { SetPage(m_PageIndex + 1); }
        void PrevPage() { SetPage(m_PageIndex - 1); }

        void SetPageCount(int count) {
            m_PageCount = std::max(0, count);
            if (m_PageCount == 0) {
                m_PageIndex = 0;
            } else if (m_PageIndex >= m_PageCount) {
                SetPage(m_PageCount - 1);
            }
        }

        // Page operations
        void Open() { if (OnOpen()) Show(); }
        void Close() { Hide(); OnClose(); }

        // Window overrides
        ImGuiWindowFlags GetFlags() override {
            return ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoSavedSettings;
        }

        void OnPreBegin() override {
            const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(vpSize.x, vpSize.y), ImGuiCond_Appearing);
        }

        void OnPostBegin() override {
            Title(m_Title.c_str());

            // Navigation
            if (m_PageIndex > 0 && NavLeft()) PrevPage();
            if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 && NavRight()) NextPage();
        }

        void OnPreEnd() override;

        virtual bool OnOpen() { return true; }
        virtual void OnClose() {}
        virtual void OnPageChanged(int newPage, int oldPage) {}

    protected:
        std::string m_Title;
        int m_PageIndex = 0;
        int m_PageCount = 0;
        Menu *m_Menu;
    };

    // Menu class for managing multiple pages
    class Menu {
    public:
        Menu() = default;

        virtual ~Menu() = default;

        // Page management
        bool AddPage(std::unique_ptr<Page> page) {
            if (!page) return false;

            const std::string &name = page->GetName();
            if (m_Pages.find(name) != m_Pages.end()) return false;

            m_Pages[name] = std::move(page);
            return true;
        }

        // Template factory method for creating pages
        template <typename PageType, typename... Args>
        PageType *CreatePage(Args &&... args) {
            static_assert(std::is_base_of_v<Page, PageType>, "PageType must inherit from Page");

            auto page = std::make_unique<PageType>(std::forward<Args>(args)...);
            PageType *pagePtr = page.get();

            // Set menu association after construction
            pagePtr->SetMenu(this);

            if (AddPage(std::move(page))) {
                return pagePtr;
            }

            return nullptr;
        }

        bool RemovePage(const std::string &name) {
            const auto it = m_Pages.find(name);
            if (it == m_Pages.end()) return false;

            Page *page = it->second.get();

            if (m_CurrentPage == page) {
                CloseCurrentPage();
                while (!m_PageStack.empty()) m_PageStack.pop();
            }

            // Clean up the stack, remove all occurrences of this page
            std::stack<Page *> tempStack;
            while (!m_PageStack.empty()) {
                Page *stackPage = m_PageStack.top();
                m_PageStack.pop();
                if (stackPage != page) {
                    tempStack.push(stackPage);
                }
            }
            // Rebuild stack without the removed page
            while (!tempStack.empty()) {
                m_PageStack.push(tempStack.top());
                tempStack.pop();
            }

            m_Pages.erase(it);
            return true;
        }

        Page *GetPage(const std::string &name) {
            const auto it = m_Pages.find(name);
            return (it != m_Pages.end()) ? it->second.get() : nullptr;
        }

        // Navigation
        bool OpenPage(const std::string &name) {
            Page *page = GetPage(name);
            if (!page) return false;

            PushPage(m_CurrentPage);
            m_CurrentPage = page;
            m_CurrentPage->Open();
            return true;
        }

        bool OpenPrevPage() {
            assert(m_CurrentPage != nullptr);
            CloseCurrentPage();
            Page *page = PopPage();
            m_CurrentPage = page;
            if (m_CurrentPage) {
                m_CurrentPage->Open();
                return true;
            } else {
                OnClose();
                return false;
            }
        }

        void CloseCurrentPage() {
            if (m_CurrentPage) {
                m_CurrentPage->Close();
                m_CurrentPage = nullptr;
            }
        }

        // Menu operations
        void Open(const std::string &name) {
            // Clear navigation history when opening a new menu session
            while (!m_PageStack.empty()) m_PageStack.pop();
            CloseCurrentPage();

            Page *page = GetPage(name);
            if (page) {
                m_CurrentPage = page;
                m_CurrentPage->Open();
                OnOpen();
            }
            // Note: If page is not found, menu is left in inactive state
        }

        void Close() {
            CloseCurrentPage();
            while (!m_PageStack.empty()) {
                m_PageStack.pop();
            }
            m_CurrentPage = nullptr;
            OnClose();
        }

        void Render() {
            if (m_CurrentPage) m_CurrentPage->Render();
        }

        // Virtual interface
        virtual void OnOpen() = 0;
        virtual void OnClose() = 0;

    protected:
        static constexpr size_t MAX_NAVIGATION_DEPTH = 32;

        void PushPage(Page *page) {
            if (page && m_PageStack.size() < MAX_NAVIGATION_DEPTH) {
                m_PageStack.push(page);
            }
        }

        Page *PopPage() {
            if (m_PageStack.empty()) return nullptr;
            Page *page = m_PageStack.top();
            m_PageStack.pop();
            return page;
        }

        Page *m_CurrentPage = nullptr;
        std::stack<Page *> m_PageStack;
        std::unordered_map<std::string, std::unique_ptr<Page>> m_Pages;
    };

    inline void Page::OnPreEnd() {
        if (NavBack()) {
            if (m_Menu) {
                m_Menu->OpenPrevPage();
            } else {
                Close();
            }
        }
    }
}

#endif // BML_BUI_H
