// The console's input line. One ImGui text field with a shell-aware editor on
// top: syntax colouring drawn over the field, a fish-style ghost suggestion
// from history, Tab completion that knows quotes and operators, Up/Down history
// that keeps the draft, Ctrl+R reverse search, readline movement and kill keys,
// and continuation rows for lines that are not finished yet.
#ifndef BML_COMMANDBAR_H
#define BML_COMMANDBAR_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "BML/Bui.h"
#include "Console/ConsoleLayout.h"
#include "Console/Shell/ShellCompletion.h"
#include "Console/Shell/ShellHighlighter.h"
#include "Console/Shell/ShellHistory.h"

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

    void ToggleCommandBar(bool on = true);
    static float MeasureRowHeight();

    // Rows the bar occupies right now: the input row plus continuation rows.
    int RowCount() const;

    void SetFrameLayout(const ConsoleLayout::Stack &layout) { m_FrameLayout = layout; }
    void SetCompositionActive(bool active) { m_TextCompositionActive = active; }
    bool HasActiveTextInput() const { return IsVisible() && m_InputActive; }

    // The history the bar records into and navigates; may be null.
    void SetHistory(BML::Shell::History *history);
    // Whether the bar stays open after a line ran.
    void SetKeepOpen(bool keepOpen) { m_KeepOpen = keepOpen; }

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
        const std::string &DisplayItem(std::size_t index) const { return m_DisplayItems[index]; }
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
        std::vector<std::string> m_DisplayItems;
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

    // Completion
    void BeginCompletion(ImGuiInputTextCallbackData *data, bool selectPrevious);
    bool HandleCompletionShortcuts(ImGuiInputTextCallbackData *data);
    void ApplyCandidate(ImGuiInputTextCallbackData *data, std::string_view candidate, bool final);
    void InvalidateCandidates();
    void DrawCompletionSurface();
    void NextCandidate();
    void PrevCandidate();
    void NextPageOfCandidates();
    void PrevPageOfCandidates();

    // Editing keys, ghost suggestion, history walk
    bool HandleEditingShortcuts(ImGuiInputTextCallbackData *data);
    bool HandleSuggestionShortcuts(ImGuiInputTextCallbackData *data);
    void HandleHistoryNavigation(ImGuiInputTextCallbackData *data);
    void UpdateSuggestion(const ImGuiInputTextCallbackData *data);
    void SetBufferText(ImGuiInputTextCallbackData *data, std::string_view text);
    void SetLogicalText(ImGuiInputTextCallbackData *data, std::string_view text, std::size_t logicalCursor);
    void EnforceInputLimit(ImGuiInputTextCallbackData *data);

    // Reverse search
    void BeginSearch(ImGuiInputTextCallbackData *data);
    void AdvanceSearch();
    void UpdateSearch(std::string_view query);
    void EndSearch(bool accept);
    void DrawSearchSurface();

    // Submitting
    void SubmitLine();
    void ResetEditorState();
    void ClearPendingLines();
    std::string JoinPending(std::string_view lastRow) const;
    std::size_t CurrentRowOffset() const;

    // Drawing
    void DrawContinuationRows(ImDrawList *drawList, const ImVec2 &areaMin, float rowHeight);
    void DrawInputOverlay(ImDrawList *drawList, const ImVec2 &itemMin, const ImVec2 &itemMax, bool showSuggestion);
    void RefreshHighlight();
    void RefreshTextMetrics();

    int OnTextEdit(ImGuiInputTextCallbackData *data);
    static int TextEditCallback(ImGuiInputTextCallbackData *data);

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
    bool m_KeepOpen = false;

    std::string m_Buffer;
    int m_CursorPos = 0;
    std::vector<std::string> m_PendingLines;
    std::string m_KillBuffer;

    BML::Shell::History *m_History = nullptr;
    BML::Shell::HistoryNavigator m_Navigator;
    std::string m_NavigatedText;
    std::string m_Suggestion; // remainder of the suggested history entry

    bool m_SearchActive = false;
    std::string m_SearchQuery;
    std::string m_SearchDraft;
    std::string m_SearchMatch;
    int m_SearchIndex = -1;
    bool m_SearchFailed = false;
    bool m_SearchRequested = false;
    bool m_SearchAcceptRequested = false;
    bool m_SearchCancelRequested = false;

    std::string m_HighlightSource;
    std::vector<BML::Shell::HighlightSpan> m_HighlightSpans;

    CandidateState m_Candidates;
    BML::Shell::CompletionPlan m_Plan;
};

#endif // BML_COMMANDBAR_H
