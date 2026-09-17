#ifndef BML_COMMANDBAR_H
#define BML_COMMANDBAR_H

#include <cstddef>
#include <string>
#include <vector>

#include "BML/Bui.h"
#include "Console/ConsoleLayout.h"

class CommandBar : public Bui::Window {
public:
    CommandBar();
    ~CommandBar() override;

    ImGuiWindowFlags GetFlags() override;

    void OnPreBegin() override;
    void OnDraw() override;
    void OnPostEnd() override;
    void OnShow() override;
    void OnHide() override;

    void PrintHistory();
    void ExecuteHistory(int index);
    void ClearHistory();
    void LoadHistory();
    void SaveHistory();

    void ToggleCommandBar(bool on = true);
    static float MeasureRowHeight();
    void SetFrameLayout(const ConsoleLayout::Stack &layout) { m_FrameLayout = layout; }
    void SetCompositionActive(bool active) { m_TextCompositionActive = active; }
    bool HasActiveTextInput() const { return IsVisible() && m_InputActive; }

private:
    class CandidateState {
    public:
        bool Empty() const { return m_Items.empty(); }
        std::size_t Size() const { return m_Items.size(); }
        const std::string &operator[](std::size_t index) const { return m_Items[index]; }

        bool Add(const std::string &candidate);
        void Clear();
        void BuildPages(float maxWidth);
        void Next();
        void Previous();
        void NextPage();
        void PreviousPage();
        void ShowHints();
        void SelectCurrent();
        void Select(int index);

        bool AreHintsVisible() const { return m_HintsVisible; }
        int CurrentIndex() const { return m_Index; }
        int CurrentPage() const { return m_Page; }
        int PageCount() const { return static_cast<int>(m_PageStarts.size()); }
        int PageBegin() const;
        int PageEnd() const;
        const ImVec2 &ItemSize(std::size_t index) const { return m_ItemSizes[index]; }
        const std::string &Status() const { return m_Status; }
        float StatusWidth() const { return m_StatusWidth; }
        bool LayoutMatches(float maxWidth, ImGuiContext *context, ImFont *font, float fontSize, ImGuiID bakedId) const;
        const std::string *Selected() const;
        std::size_t CommonPrefixLength() const;

    private:
        void SyncPageFromIndex();
        void UpdateStatus();
        void InvalidateLayout();

        std::vector<std::string> m_Items;
        std::vector<ImVec2> m_ItemSizes;
        std::vector<int> m_PageStarts;
        std::string m_Status;
        ImGuiContext *m_Context = nullptr;
        ImFont *m_Font = nullptr;
        ImGuiID m_BakedId = 0;
        float m_FontSize = 0.0f;
        float m_MaxWidth = 0.0f;
        float m_StatusWidth = 0.0f;
        int m_Index = 0;
        int m_Page = 0;
        int m_Selected = -1;
        bool m_HintsVisible = false;
    };

    std::wstring GetHistoryPath() const;
    void RecordHistoryEntry(const std::string &entry);
    void CollectCommandCandidates(const char *cmdStart, int cmdLength);
    void CollectArgumentCandidates(const char *wordStart, int wordLength, const char *cmdStart, const char *lineEnd);
    void ReplaceCurrentToken(ImGuiInputTextCallbackData *data, const char *replacement, int replacementLength = -1);
    void DrawCompletionSurface();
    void NextCandidate();
    void PrevCandidate();
    void NextPageOfCandidates();
    void PrevPageOfCandidates();
    void InvalidateCandidates();
    void GenerateCandidatePages();
    void RefreshTextMetrics();
    void BuildCompletionCandidates(const char *lineStart, const char *lineEnd);
    void BeginCompletion(ImGuiInputTextCallbackData *data, bool selectPrevious);
    bool HandleCompletionShortcuts(ImGuiInputTextCallbackData *data);
    int OnTextEdit(ImGuiInputTextCallbackData *data);

    static int TextEditCallback(ImGuiInputTextCallbackData *data);
    static void StripLine(const char *&lineStart, const char *&lineEnd);
    static int FirstToken(const char *tokenStart, const char *&tokenEnd);
    static int LastToken(const char *&tokenStart, const char *tokenEnd);

    ImVec2 m_WindowPos;
    ImVec2 m_WindowSize;
    ImVec2 m_TransientPos;
    ImVec2 m_TransientSize;
    ConsoleLayout::Stack m_FrameLayout;
    float m_CommandHeight = 0.0f;
    ImGuiContext *m_Context = nullptr;
    ImFont *m_Font = nullptr;
    ImGuiID m_BakedId = 0;
    float m_FontSize = 0.0f;
    float m_TextLineHeight = 0.0f;
    ImVec2 m_PromptSize;
    ImVec2 m_PreviousPageLabelSize;
    ImVec2 m_NextPageLabelSize;
    bool m_TextCompositionActive = false;
    bool m_InputActive = false;
    bool m_VisiblePrev = false;
    bool m_FocusInputNextFrame = false;
    std::string m_Buffer;
    int m_CursorPos = 0;
    int m_HistoryIndex = -1;
    std::vector<std::string> m_History;
    CandidateState m_Candidates;
};

#endif // BML_COMMANDBAR_H
