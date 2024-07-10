#ifndef BML_BUI_H
#define BML_BUI_H

#include <string>
#include <stack>
#include <vector>
#include <unordered_map>
#include <functional>
#include <utility>

#include "imgui.h"

#include "BML/Export.h"
#include "CKContext.h"
#include "CKRenderContext.h"

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

    inline ImVec2 CoordToPixel(const ImVec2 &coord) {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        return {vpSize.x * coord.x, vpSize.y * coord.y};
    }

    BML_EXPORT bool InitTextures(CKContext *context);
    BML_EXPORT bool InitMaterials(CKContext *context);
    BML_EXPORT bool InitSounds(CKContext *context);

    BML_EXPORT ImGuiContext *GetImGuiContext();

    class ImGuiContextScope {
    public:
        explicit ImGuiContextScope(ImGuiContext *context = nullptr) {
            m_ImGuiContext = (context != nullptr) ? context : ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(GetImGuiContext());
        }

        ImGuiContextScope(const ImGuiContextScope &rhs) = delete;
        ImGuiContextScope(ImGuiContextScope &&rhs) noexcept = delete;

        ~ImGuiContextScope() {
            ImGui::SetCurrentContext(m_ImGuiContext);
        }

        ImGuiContextScope &operator=(const ImGuiContextScope &rhs) = delete;
        ImGuiContextScope &operator=(ImGuiContextScope &&rhs) noexcept = delete;

    private:
        ImGuiContext *m_ImGuiContext;
    };

    BML_EXPORT CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot = 0);

    BML_EXPORT ImVec2 GetMenuPos();
    BML_EXPORT ImVec2 GetMenuSize();
    BML_EXPORT ImVec4 GetMenuColor();

    BML_EXPORT ImVec2 GetButtonSize(ButtonType type);
    BML_EXPORT float GetButtonIndent(ButtonType type);

    BML_EXPORT ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key);
    BML_EXPORT CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key);
    BML_EXPORT bool KeyChordToString(ImGuiKeyChord key_chord, char *buf, size_t size);

    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected);

    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text);

    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text, const ImVec2 &text_align);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text, const ImVec2 &text_align);

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

    BML_EXPORT bool YesNoButton(const char *label, bool *v);
    BML_EXPORT bool RadioButton(const char *label, int *current_item, const char *const items[], int items_count);
    BML_EXPORT bool KeyButton(const char *label, bool *toggled, ImGuiKeyChord *key_chord);

    BML_EXPORT bool InputTextButton(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
    BML_EXPORT bool InputFloatButton(const char *label, float *v, float step = 0.0f, float step_fast = 0.0f, const char *format = "%.3f", ImGuiInputTextFlags flags = 0);
    BML_EXPORT bool InputIntButton(const char *label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);

    class Window {
    public:
        explicit Window(std::string name) : m_Name(std::move(name)) {}

        virtual ~Window() = default;

        const std::string &GetName() const { return m_Name; }

        bool IsVisible() const { return m_Visible; }
        bool IsHovered() const { return m_Hovered; }

        void SetVisibility(bool visible) {
            if (m_Visible != visible) {
                m_Visible = visible;
                if (m_Visible) {
                    OnShow();
                } else {
                    OnHide();
                }
            }
        }
        void ToggleVisibility() { SetVisibility(!m_Visible); }
        void Show() { SetVisibility(true); }
        void Hide() { SetVisibility(false); }

        bool Begin() {
            OnBegin();
            bool keepVisible = true;
            const bool notCollapsed = ImGui::Begin(m_Name.c_str(), &keepVisible, GetFlags());
            if (!keepVisible)
                Hide();
            if (notCollapsed)
                OnAfterBegin();
            return notCollapsed;
        }

        void End() {
            OnEnd();
            m_Hovered = ImGui::IsWindowHovered();
            ImGui::End();
        }

        void Render() {
            if (!IsVisible())
                return;

            if (Begin()) {
                OnDraw();
            }

            End();
        }

        virtual ImGuiWindowFlags GetFlags() { return 0; }
        virtual void OnBegin() {}
        virtual void OnAfterBegin() {}
        virtual void OnDraw() = 0;
        virtual void OnEnd() {}
        virtual void OnShow() {}
        virtual void OnHide() {}

    protected:
        std::string m_Name;
        bool m_Visible = true;
        bool m_Hovered = false;
    };

    class Page : public Window {
    public:
        explicit Page(std::string name) : Window(std::move(name)), m_Title(m_Name) { SetVisibility(false); }
        Page(std::string name, std::string title) : Window(std::move(name)), m_Title(std::move(title)) { SetVisibility(false); }

        const std::string &GetTitle() const { return m_Title; }
        void SetTitle(const std::string &title) { m_Title = title; }

        int GetPage() const { return m_PageIndex; }
        void SetPage(int page) {
            if (page >= 0 && page < m_PageCount)
                m_PageIndex = page;
        }
        int NextPage() { return m_PageIndex < m_PageCount ? ++m_PageIndex : m_PageCount - 1; }
        int PrevPage() { return m_PageIndex > 0 ? --m_PageIndex : 0; }

        int GetMaxPage() const { return m_PageCount; }
        void SetMaxPage(int num) {
            if (num > 0) {
                if (m_PageIndex >= num)
                    m_PageIndex = num - 1;
                m_PageCount = num;
            }
        }

        void Open() { if (OnOpen()) Show(); }
        void Close() { Hide(); OnClose(); }

        ImGuiWindowFlags GetFlags() override {
            return ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoSavedSettings;
        }

        void OnBegin() override {
            const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(vpSize.x, vpSize.y), ImGuiCond_Appearing);
        }

        void OnAfterBegin() override {
            if (!IsVisible())
                return;

            DrawCenteredText(m_Title.c_str());

            if (m_PageIndex > 0 &&
                LeftButton("PrevPage")) {
                PrevPage();
            }

            if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
                RightButton("NextPage")) {
                NextPage();
            }
        }

        void OnEnd() override {
            if (!IsVisible())
                return;

            if (BackButton("Back")) {
                Close();
            }
        }

        virtual bool OnOpen() { return true; }
        virtual void OnClose() {}
        
        static std::size_t DrawEntries(const std::function<bool(std::size_t index)> &onEntry,
                                       const ImVec2 &pos = ImVec2(0.35f, 0.24f), float spacing = 0.14f, std::size_t capability = 4) {
            auto oldPos = ImGui::GetCursorScreenPos();

            for (std::size_t i = 0; i < capability; ++i) {
                ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(pos.x, pos.y + (float) i * spacing)));
                if (!onEntry(i)) {
                    return i;
                }
            }

            ImGui::SetCursorScreenPos(oldPos);
            return capability;
        }

        static bool BackButton(const std::string &label, const ImVec2 &pos = ImVec2(0.4031f, 0.85f)) {
            ImGui::SetCursorScreenPos(Bui::CoordToPixel(pos));
            return (Bui::BackButton(label.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape));
        }

        static bool LeftButton(const std::string &label, const ImVec2 &pos = ImVec2(0.36f, 0.124f)) {
            ImGui::SetCursorScreenPos(Bui::CoordToPixel(pos));
            return (Bui::LeftButton(label.c_str()) || ImGui::IsKeyPressed(ImGuiKey_PageUp));
        }

        static bool RightButton(const std::string &label, const ImVec2 &pos = ImVec2(0.6038f, 0.124f)) {
            ImGui::SetCursorScreenPos(Bui::CoordToPixel(pos));
            return (Bui::RightButton(label.c_str()) || ImGui::IsKeyPressed(ImGuiKey_PageDown));
        }

        static void WrappedText(const char *text, float length, float scale = 1.0f) {
            if (!text || text[0] == '\0')
                return;

            const float width = ImGui::CalcTextSize(text).x;
            const float indent = (length - width) * 0.5f;
            if (indent > 0) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
            }

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + length);

            const float oldScale = ImGui::GetFont()->Scale;
            if (scale != 1.0f) {
                ImGui::GetFont()->Scale *= scale;
                ImGui::PushFont(ImGui::GetFont());
            }
            ImGui::TextUnformatted(text);
            if (scale != 1.0f) {
                ImGui::GetFont()->Scale = oldScale;
                ImGui::PopFont();
            }

            ImGui::PopTextWrapPos();
        }

        static void DrawCenteredText(const char *text, float y = 0.13f, float scale = 1.5f, ImU32 color = IM_COL32_WHITE) {
            if (!text || text[0] == '\0')
                return;

            float oldScale = ImGui::GetFont()->Scale;
            ImGui::GetFont()->Scale *= scale;
            ImGui::PushFont(ImGui::GetFont());

            const auto titleSize = ImGui::CalcTextSize(text);
            const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
            ImGui::GetWindowDrawList()->AddText(ImVec2((vpSize.x - titleSize.x) / 2.0f, vpSize.y * y), color, text);

            ImGui::GetFont()->Scale = oldScale;
            ImGui::PopFont();
        }

    protected:
        std::string m_Title;
        int m_PageIndex = 0;
        int m_PageCount = 1;
    };

    class Menu {
    public:
        Menu() = default;

        bool AddPage(Page *page) {
            if (!page)
                return false;

            const auto &name = page->GetName();
            auto it = m_PageMap.find(name);
            if (it != m_PageMap.end())
                return false;

            m_PageMap[name] = page;
            return true;
        }

        bool RemovePage(Page *page) {
            if (!page)
                return false;

            const auto &name = page->GetName();
            auto it = m_PageMap.find(name);
            if (it == m_PageMap.end())
                return false;

            HidePage();

            while (!m_PageStack.empty())
                m_PageStack.pop();

            m_PageMap.erase(it);
            return true;
        }

        Page *GetPage(const std::string &name) {
            auto it = m_PageMap.find(name);
            if (it == m_PageMap.end())
                return nullptr;
            return m_PageMap[name];
        }

        bool ShowPage(const std::string &name) {
            auto *page = GetPage(name);
            if (!page)
                return false;

            PushPage(m_CurrentPage);
            m_CurrentPage = page;
            m_CurrentPage->Show();
            return true;
        }
        void ShowPrevPage() {
            HidePage();
            auto *page = PopPage();
            m_CurrentPage = page;
            if (m_CurrentPage)
                m_CurrentPage->Show(); 
            else
                OnClose();
        }

        void HidePage() {
            if (m_CurrentPage) {
                m_CurrentPage->Hide();
                m_CurrentPage = nullptr;
            }
        }

        void Render() {
            if (m_CurrentPage)
                m_CurrentPage->Render();
        }

        void Open(const std::string &name) {
            if (ShowPage(name))
                OnOpen();
        }

        void Close() {
            ShowPrevPage();
            HidePage();
            OnClose();
        }

        virtual void OnOpen() = 0;
        virtual void OnClose() = 0;

    protected:
        void PushPage(Page *page) {
            if (page)
                m_PageStack.push(page);
        }
        Page *PopPage() {
            if (m_PageStack.empty())
                return nullptr;
            auto *page = m_PageStack.top();
            m_PageStack.pop();
            return page;
        }

        Page *m_CurrentPage = nullptr;
        std::stack<Page *> m_PageStack;
        std::unordered_map<std::string, Page *> m_PageMap;
    };
}

#endif // BML_BUI_H
