#ifndef BML_COMMANDBAR_H
#define BML_COMMANDBAR_H

#include "BML/Bui.h"

class CommandBar : public Bui::Window {
public:
    CommandBar();
    ~CommandBar() override;

    ImGuiWindowFlags GetFlags() override;

    void OnBegin() override;
    void OnDraw() override;
    void OnAfterEnd() override;
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

    size_t OnCompletion(const char *lineStart, const char *lineEnd);
    int OnTextEdit(ImGuiInputTextCallbackData *data);

    static int TextEditCallback(ImGuiInputTextCallbackData *data);
    static void StripLine(const char *&lineStart, const char *&lineEnd);
    static int FirstToken(const char *tokenStart, const char *&tokenEnd);
    static int LastToken(const char *&tokenStart, const char *tokenEnd);
    static std::vector<std::string> MakeArgs(const char *line);

private:
    ImVec2 m_WindowPos;
    ImVec2 m_WindowSize;
    bool m_VisiblePrev = false;
    bool m_ShowHints = false;
    std::string m_Buffer;
    int m_CursorPos = 0;
    int m_HistoryIndex = -1;
    std::vector<std::string> m_History;
    int m_CandidateSelected = -1;
    int m_CandidateIndex = 0;
    int m_CandidatePage = 0;
    std::vector<int> m_CandidatePages;
    std::vector<std::string> m_Candidates;
};

#endif // BML_COMMANDBAR_H
