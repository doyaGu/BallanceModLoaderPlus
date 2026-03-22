#ifndef BML_MENU_HPP
#define BML_MENU_HPP

#include <algorithm>
#include <cassert>
#include <memory>
#include <stack>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "bml_imgui.hpp"
#include "bml_core.h"
#include "bml_interface.hpp"
#include "bml_menu.h"

#include "CKContext.h"

#include "bml_types.h"

namespace Menu {
    class Window;
    class Page;
    class Menu;

    enum ButtonType : int {
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

    namespace detail {
        inline BML_Mod ResolveHostMenuOwner() {
            return bmlGetHostModule ? bmlGetHostModule() : nullptr;
        }

        inline bml::InterfaceLease<BML_MenuApi> AcquireMenu() {
            return bml::AcquireInterface<BML_MenuApi>(
                ResolveHostMenuOwner(), BML_MENU_INTERFACE_ID, 1, 0, 0);
        }

        inline const BML_ImGuiApi *GetCurrentImGuiApi() {
            const BML_ImGuiApi *api = bml::imgui::GetCurrentApi();
            assert(api != nullptr && "Menu functions require BML_IMGUI_SCOPE_FROM_CONTEXT(ctx)");
            return api;
        }
    } // namespace detail

    template <typename Func>
    std::enable_if_t<std::is_void_v<std::invoke_result_t<Func>>> At(float x, float y, Func &&func) {
        const ImVec2 savedPos = ImGui::GetCursorScreenPos();
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * x, vpSize.y * y));
        func();
        ImGui::SetCursorScreenPos(savedPos);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }

    template <typename Func>
    std::enable_if_t<!std::is_void_v<std::invoke_result_t<Func>>, std::invoke_result_t<Func>>
    At(float x, float y, Func &&func) {
        const ImVec2 savedPos = ImGui::GetCursorScreenPos();
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * x, vpSize.y * y));
        auto result = func();
        ImGui::SetCursorScreenPos(savedPos);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        return result;
    }

    template <typename Func>
    auto At(const ImVec2 &pos, Func &&func) -> decltype(At(pos.x, pos.y, std::forward<Func>(func))) {
        return At(pos.x, pos.y, std::forward<Func>(func));
    }

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

    inline ImVec2 CoordToPixel(const ImVec2 &coord) {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        return {vpSize.x * coord.x, vpSize.y * coord.y};
    }

#ifdef BML_MENU_PROVIDER_IMPLEMENTATION

    bool InitTextures(CKContext *context);
    bool InitMaterials(CKContext *context);
    bool InitSounds(CKContext *context);

    CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot = 0);

    ImVec2 GetMenuPos();
    ImVec2 GetMenuSize();
    ImVec4 GetMenuColor();

    ImVec2 GetButtonSize(ButtonType type);
    float GetButtonIndent(ButtonType type);
    ImVec2 GetButtonSizeInCoord(ButtonType type);
    float GetButtonIndentInCoord(ButtonType type);

    ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key);
    CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key);
    bool KeyChordToString(ImGuiKeyChord keyChord, char *buf, size_t size);
    bool SetKeyChordFromIO(ImGuiKeyChord *keyChord);

    void PlayMenuClickSound();

    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state);
    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected);
    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text);
    void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text);
    void AddButtonImage(
        ImDrawList *drawList,
        const ImVec2 &pos,
        ButtonType type,
        int state,
        const char *text,
        const ImVec2 &textAlign);
    void AddButtonImage(
        ImDrawList *drawList,
        const ImVec2 &pos,
        ButtonType type,
        bool selected,
        const char *text,
        const ImVec2 &textAlign);

    bool MainButton(const char *label, ImGuiButtonFlags flags = 0);
    bool OkButton(const char *label, ImGuiButtonFlags flags = 0);
    bool BackButton(const char *label, ImGuiButtonFlags flags = 0);
    bool OptionButton(const char *label, ImGuiButtonFlags flags = 0);
    bool LevelButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0);
    bool SmallButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0);
    bool LeftButton(const char *label, ImGuiButtonFlags flags = 0);
    bool RightButton(const char *label, ImGuiButtonFlags flags = 0);
    bool PlusButton(const char *label, ImGuiButtonFlags flags = 0);
    bool MinusButton(const char *label, ImGuiButtonFlags flags = 0);

    bool KeyButton(const char *label, bool *toggled, ImGuiKeyChord *keyChord);
    bool YesNoButton(const char *label, bool *v);
    bool RadioButton(const char *label, int *currentItem, const char *const items[], int itemsCount);

    bool InputTextButton(const char *label,
        char *buf,
        size_t bufSize,
        ImGuiInputTextFlags flags = 0,
        ImGuiInputTextCallback callback = nullptr,
        void *userData = nullptr);
    bool InputFloatButton(const char *label,
        float *v,
        float step = 0.0f,
        float stepFast = 0.0f,
        const char *format = "%.3f",
        ImGuiInputTextFlags flags = 0);
    bool InputIntButton(
        const char *label,
        int *v,
        int step = 1,
        int stepFast = 100,
        ImGuiInputTextFlags flags = 0);

    void WrappedText(const char *text, float width, float baseX = 0.0f, float scale = 1.0f);

    bool NavLeft(float x = 0.36f, float y = 0.124f);
    bool NavRight(float x = 0.6038f, float y = 0.124f);
    bool NavBack(float x = 0.4031f, float y = 0.85f);

    void Title(const char *text, float y = 0.13f, float scale = 1.5f, ImU32 color = IM_COL32_WHITE);
    bool SearchBar(char *buffer, size_t bufferSize, float x = 0.4f, float y = 0.18f, float width = 0.2f);

#else

    inline bool InitTextures(CKContext *context) {
        auto api = detail::AcquireMenu();
        return api && api->InitTextures ? api->InitTextures(context) : false;
    }

    inline bool InitMaterials(CKContext *context) {
        auto api = detail::AcquireMenu();
        return api && api->InitMaterials ? api->InitMaterials(context) : false;
    }

    inline bool InitSounds(CKContext *context) {
        auto api = detail::AcquireMenu();
        return api && api->InitSounds ? api->InitSounds(context) : false;
    }

    inline CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot = 0) {
        auto api = detail::AcquireMenu();
        return api && api->LoadTexture ? api->LoadTexture(context, id, filename, slot) : nullptr;
    }

    inline ImVec2 GetMenuPos() {
        auto api = detail::AcquireMenu();
        return api && api->GetMenuPos ? api->GetMenuPos(detail::GetCurrentImGuiApi()) : ImVec2();
    }

    inline ImVec2 GetMenuSize() {
        auto api = detail::AcquireMenu();
        return api && api->GetMenuSize ? api->GetMenuSize(detail::GetCurrentImGuiApi()) : ImVec2();
    }

    inline ImVec4 GetMenuColor() {
        auto api = detail::AcquireMenu();
        return api && api->GetMenuColor ? api->GetMenuColor() : ImVec4();
    }

    inline ImVec2 GetButtonSize(ButtonType type) {
        auto api = detail::AcquireMenu();
        return api && api->GetButtonSize ? api->GetButtonSize(type, detail::GetCurrentImGuiApi()) : ImVec2();
    }

    inline float GetButtonIndent(ButtonType type) {
        auto api = detail::AcquireMenu();
        return api && api->GetButtonIndent ? api->GetButtonIndent(type, detail::GetCurrentImGuiApi()) : 0.0f;
    }

    inline ImVec2 GetButtonSizeInCoord(ButtonType type) {
        auto api = detail::AcquireMenu();
        return api && api->GetButtonSizeInCoord ? api->GetButtonSizeInCoord(type) : ImVec2();
    }

    inline float GetButtonIndentInCoord(ButtonType type) {
        auto api = detail::AcquireMenu();
        return api && api->GetButtonIndentInCoord ? api->GetButtonIndentInCoord(type) : 0.0f;
    }

    inline ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key) {
        auto api = detail::AcquireMenu();
        return api && api->CKKeyToImGuiKey ? api->CKKeyToImGuiKey(key) : ImGuiKey_None;
    }

    inline CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key) {
        auto api = detail::AcquireMenu();
        return api && api->ImGuiKeyToCKKey ? api->ImGuiKeyToCKKey(key) : static_cast<CKKEYBOARD>(0);
    }

    inline bool KeyChordToString(ImGuiKeyChord keyChord, char *buf, size_t size) {
        auto api = detail::AcquireMenu();
        return api && api->KeyChordToString ? api->KeyChordToString(keyChord, buf, size, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool SetKeyChordFromIO(ImGuiKeyChord *keyChord) {
        auto api = detail::AcquireMenu();
        return api && api->SetKeyChordFromIO ? api->SetKeyChordFromIO(keyChord, detail::GetCurrentImGuiApi()) : false;
    }

    inline void PlayMenuClickSound() {
        auto api = detail::AcquireMenu();
        if (api && api->PlayMenuClickSound) {
            api->PlayMenuClickSound();
        }
    }

    inline void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state) {
        auto api = detail::AcquireMenu();
        if (api && api->AddButtonImageState) {
            api->AddButtonImageState(drawList, pos, type, state, detail::GetCurrentImGuiApi());
        }
    }

    inline void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected) {
        auto api = detail::AcquireMenu();
        if (api && api->AddButtonImageSelected) {
            api->AddButtonImageSelected(drawList, pos, type, selected, detail::GetCurrentImGuiApi());
        }
    }

    inline void AddButtonImage(
        ImDrawList *drawList,
        const ImVec2 &pos,
        ButtonType type,
        int state,
        const char *text) {
        auto api = detail::AcquireMenu();
        if (api && api->AddButtonImageStateText) {
            api->AddButtonImageStateText(drawList, pos, type, state, text, detail::GetCurrentImGuiApi());
        }
    }

    inline void AddButtonImage(
        ImDrawList *drawList,
        const ImVec2 &pos,
        ButtonType type,
        bool selected,
        const char *text) {
        auto api = detail::AcquireMenu();
        if (api && api->AddButtonImageSelectedText) {
            api->AddButtonImageSelectedText(drawList, pos, type, selected, text, detail::GetCurrentImGuiApi());
        }
    }

    inline void AddButtonImage(
        ImDrawList *drawList,
        const ImVec2 &pos,
        ButtonType type,
        int state,
        const char *text,
        const ImVec2 &textAlign) {
        auto api = detail::AcquireMenu();
        if (api && api->AddButtonImageStateTextAlign) {
            api->AddButtonImageStateTextAlign(drawList, pos, type, state, text, textAlign, detail::GetCurrentImGuiApi());
        }
    }

    inline void AddButtonImage(
        ImDrawList *drawList,
        const ImVec2 &pos,
        ButtonType type,
        bool selected,
        const char *text,
        const ImVec2 &textAlign) {
        auto api = detail::AcquireMenu();
        if (api && api->AddButtonImageSelectedTextAlign) {
            api->AddButtonImageSelectedTextAlign(drawList, pos, type, selected, text, textAlign, detail::GetCurrentImGuiApi());
        }
    }

    inline bool MainButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->MainButton ? api->MainButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool OkButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->OkButton ? api->OkButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool BackButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->BackButton ? api->BackButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool OptionButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->OptionButton ? api->OptionButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool LevelButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->LevelButton ? api->LevelButton(label, v, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool SmallButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->SmallButton ? api->SmallButton(label, v, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool LeftButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->LeftButton ? api->LeftButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool RightButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->RightButton ? api->RightButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool PlusButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->PlusButton ? api->PlusButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool MinusButton(const char *label, ImGuiButtonFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->MinusButton ? api->MinusButton(label, flags, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool KeyButton(const char *label, bool *toggled, ImGuiKeyChord *keyChord) {
        auto api = detail::AcquireMenu();
        return api && api->KeyButton ? api->KeyButton(label, toggled, keyChord, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool YesNoButton(const char *label, bool *v) {
        auto api = detail::AcquireMenu();
        return api && api->YesNoButton ? api->YesNoButton(label, v, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool RadioButton(const char *label, int *currentItem, const char *const items[], int itemsCount) {
        auto api = detail::AcquireMenu();
        return api && api->RadioButton
            ? api->RadioButton(label, currentItem, items, itemsCount, detail::GetCurrentImGuiApi())
            : false;
    }

    inline bool InputTextButton(const char *label,
        char *buf,
        size_t bufSize,
        ImGuiInputTextFlags flags = 0,
        ImGuiInputTextCallback callback = nullptr,
        void *userData = nullptr) {
        auto api = detail::AcquireMenu();
        return api && api->InputTextButton
            ? api->InputTextButton(label, buf, bufSize, flags, callback, userData, detail::GetCurrentImGuiApi())
            : false;
    }

    inline bool InputFloatButton(const char *label,
        float *v,
        float step = 0.0f,
        float stepFast = 0.0f,
        const char *format = "%.3f",
        ImGuiInputTextFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->InputFloatButton
            ? api->InputFloatButton(label, v, step, stepFast, format, flags, detail::GetCurrentImGuiApi())
            : false;
    }

    inline bool InputIntButton(
        const char *label,
        int *v,
        int step = 1,
        int stepFast = 100,
        ImGuiInputTextFlags flags = 0) {
        auto api = detail::AcquireMenu();
        return api && api->InputIntButton
            ? api->InputIntButton(label, v, step, stepFast, flags, detail::GetCurrentImGuiApi())
            : false;
    }

    inline void WrappedText(const char *text, float width, float baseX = 0.0f, float scale = 1.0f) {
        auto api = detail::AcquireMenu();
        if (api && api->WrappedText) {
            api->WrappedText(text, width, baseX, scale, detail::GetCurrentImGuiApi());
        }
    }

    inline bool NavLeft(float x = 0.36f, float y = 0.124f) {
        auto api = detail::AcquireMenu();
        return api && api->NavLeft ? api->NavLeft(x, y, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool NavRight(float x = 0.6038f, float y = 0.124f) {
        auto api = detail::AcquireMenu();
        return api && api->NavRight ? api->NavRight(x, y, detail::GetCurrentImGuiApi()) : false;
    }

    inline bool NavBack(float x = 0.4031f, float y = 0.85f) {
        auto api = detail::AcquireMenu();
        return api && api->NavBack ? api->NavBack(x, y, detail::GetCurrentImGuiApi()) : false;
    }

    inline void Title(const char *text, float y = 0.13f, float scale = 1.5f, ImU32 color = IM_COL32_WHITE) {
        auto api = detail::AcquireMenu();
        if (api && api->Title) {
            api->Title(text, y, scale, color, detail::GetCurrentImGuiApi());
        }
    }

    inline bool SearchBar(char *buffer, size_t bufferSize, float x = 0.4f, float y = 0.18f, float width = 0.2f) {
        auto api = detail::AcquireMenu();
        return api && api->SearchBar
            ? api->SearchBar(buffer, bufferSize, x, y, width, detail::GetCurrentImGuiApi())
            : false;
    }

#endif

    template <typename Func>
    void Entries(
        Func &&entryFunc,
        float startX = 0.4031f,
        float startY = 0.24f,
        float spacing = 0.06f,
        int maxCount = 10) {
        for (int i = 0; i < maxCount; ++i) {
            bool shouldContinue = true;
            At(startX, startY + i * spacing, [&]() {
                shouldContinue = entryFunc(i);
            });
            if (!shouldContinue) {
                break;
            }
        }
    }

    inline bool CanPrevPage(int pageIndex) {
        return pageIndex > 0;
    }

    inline bool CanNextPage(int pageIndex, int totalCount, int pageSize) {
        if (pageSize <= 0) {
            return false;
        }
        return totalCount > (pageIndex + 1) * pageSize;
    }

    inline int CalcPageCount(int totalCount, int pageSize) {
        if (pageSize <= 0 || totalCount <= 0) {
            return 0;
        }
        return (totalCount + pageSize - 1) / pageSize;
    }

    class Window {
    public:
        explicit Window(std::string name)
            : m_Name(std::move(name)),
              m_Visible(true),
              m_ShouldHide(false) {
        }

        virtual ~Window() = default;

        const std::string &GetName() const { return m_Name; }
        bool IsVisible() const { return m_Visible; }

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

        void Toggle() {
            m_Visible ? Hide() : Show();
        }

        bool Begin() {
            OnPreBegin();
            bool keepVisible = true;
            const bool notCollapsed = ImGui::Begin(m_Name.c_str(), &keepVisible, GetFlags());
            if (!keepVisible) {
                m_ShouldHide = true;
            }
            if (notCollapsed) {
                OnPostBegin();
            }
            return notCollapsed;
        }

        void End() {
            OnPreEnd();
            ImGui::End();
            OnPostEnd();
        }

        void Render() {
            if (!IsVisible()) {
                return;
            }

            if (Begin()) {
                OnDraw();
            }
            End();

            if (m_ShouldHide) {
                Hide();
                m_ShouldHide = false;
            }
        }

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

    class Page : public Window {
    public:
        explicit Page(std::string name)
            : Window(std::move(name)),
              m_Title(m_Name),
              m_Menu(nullptr) {
            Hide();
        }

        Page(std::string name, std::string title)
            : Window(std::move(name)),
              m_Title(std::move(title)),
              m_Menu(nullptr) {
            Hide();
        }

        const std::string &GetTitle() const { return m_Title; }
        void SetTitle(const std::string &title) { m_Title = title; }

        Menu *GetMenu() const { return m_Menu; }
        void SetMenu(Menu *menu) { m_Menu = menu; }

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

        void Open() {
            if (OnOpen()) {
                Show();
            }
        }

        void Close() {
            Hide();
            OnClose();
        }

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

            if (m_PageIndex > 0 && NavLeft()) {
                PrevPage();
            }
            if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 && NavRight()) {
                NextPage();
            }
        }

        void OnPreEnd() override;

        virtual bool OnOpen() { return true; }
        virtual void OnClose() {}
        virtual void OnPageChanged(int newPage, int oldPage) {
            BML_UNUSED(newPage);
            BML_UNUSED(oldPage);
        }

    protected:
        std::string m_Title;
        int m_PageIndex = 0;
        int m_PageCount = 0;
        Menu *m_Menu;
    };

    class Menu {
    public:
        Menu() = default;
        virtual ~Menu() = default;

        bool AddPage(std::unique_ptr<Page> page) {
            if (!page) {
                return false;
            }

            const std::string &name = page->GetName();
            if (m_Pages.find(name) != m_Pages.end()) {
                return false;
            }

            m_Pages[name] = std::move(page);
            return true;
        }

        template <typename PageType, typename... Args>
        PageType *CreatePage(Args &&... args) {
            static_assert(std::is_base_of_v<Page, PageType>, "PageType must inherit from Page");

            auto page = std::make_unique<PageType>(std::forward<Args>(args)...);
            PageType *pagePtr = page.get();
            pagePtr->SetMenu(this);

            if (AddPage(std::move(page))) {
                return pagePtr;
            }

            return nullptr;
        }

        bool RemovePage(const std::string &name) {
            const auto it = m_Pages.find(name);
            if (it == m_Pages.end()) {
                return false;
            }

            Page *page = it->second.get();

            if (m_CurrentPage == page) {
                CloseCurrentPage();
                while (!m_PageStack.empty()) {
                    m_PageStack.pop();
                }
            }

            std::stack<Page *> tempStack;
            while (!m_PageStack.empty()) {
                Page *stackPage = m_PageStack.top();
                m_PageStack.pop();
                if (stackPage != page) {
                    tempStack.push(stackPage);
                }
            }
            while (!tempStack.empty()) {
                m_PageStack.push(tempStack.top());
                tempStack.pop();
            }

            m_Pages.erase(it);
            return true;
        }

        Page *GetPage(const std::string &name) {
            const auto it = m_Pages.find(name);
            return it != m_Pages.end() ? it->second.get() : nullptr;
        }

        bool OpenPage(const std::string &name) {
            Page *page = GetPage(name);
            if (!page) {
                return false;
            }

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
            }

            OnClose();
            return false;
        }

        void CloseCurrentPage() {
            if (m_CurrentPage) {
                m_CurrentPage->Close();
                m_CurrentPage = nullptr;
            }
        }

        void Open(const std::string &name) {
            while (!m_PageStack.empty()) {
                m_PageStack.pop();
            }
            CloseCurrentPage();

            Page *page = GetPage(name);
            if (page) {
                m_CurrentPage = page;
                m_CurrentPage->Open();
                OnOpen();
            }
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
            if (m_CurrentPage) {
                m_CurrentPage->Render();
            }
        }

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
            if (m_PageStack.empty()) {
                return nullptr;
            }
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
} // namespace Menu

#endif // BML_MENU_HPP
