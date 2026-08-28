#ifndef BML_COMMANDBAR_H
#define BML_COMMANDBAR_H

#include <string>
#include <vector>

#include "BML/Bui.h"

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

    void NextCandidate();
    void PrevCandidate();
    void NextPageOfCandidates();
    void PrevPageOfCandidates();
    void InvalidateCandidates();
    void GenerateCandidatePages();

    size_t OnCompletion(const char *lineStart, const char *lineEnd);
    int OnTextEdit(ImGuiInputTextCallbackData *data);

    static int TextEditCallback(ImGuiInputTextCallbackData *data);
    static void StripLine(const char *&lineStart, const char *&lineEnd);
    static int FirstToken(const char *tokenStart, const char *&tokenEnd);
    static int LastToken(const char *&tokenStart, const char *tokenEnd);

private:
    class CandidateState {
    public:
        bool Empty() const { return m_Items.empty(); }
        size_t Size() const { return m_Items.size(); }
        const std::string &operator[](size_t index) const { return m_Items[index]; }

        bool Add(const std::string &candidate);
        void Clear();
        void BuildPages(float maxWidth);
        void Next();
        void Previous();
        void NextPage();
        void PreviousPage();
        void ShowHints();
        void SelectCurrent();

        bool AreHintsVisible() const { return m_HintsVisible; }
        int CurrentIndex() const { return m_Index; }
        int CurrentPage() const { return m_Page; }
        int PageCount() const { return static_cast<int>(m_PageStarts.size()); }
        int PageBegin() const;
        int PageEnd() const;
        const std::string *Selected() const;

    private:
        void SyncPageFromIndex();

        std::vector<std::string> m_Items;
        std::vector<int> m_PageStarts;
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

    ImVec2 m_WindowPos;
    ImVec2 m_WindowSize;
    bool m_VisiblePrev = false;
    std::string m_Buffer;
    int m_CursorPos = 0;
    int m_HistoryIndex = -1;
    std::vector<std::string> m_History;
    CandidateState m_Candidates;
};

#endif // BML_COMMANDBAR_H
