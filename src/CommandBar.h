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

    int OnTextEdit(ImGuiInputTextCallbackData *data);

    static int TextEditCallback(ImGuiInputTextCallbackData *data) {
        auto *mod = static_cast<CommandBar *>(data->UserData);
        return mod->OnTextEdit(data);
    }

    static std::vector<std::string> MakeArgs(const char *line);

private:
    ImVec2 m_WindowPos;
    ImVec2 m_WindowSize;
    bool m_VisiblePrev = false;
    std::string m_Buffer;
    int m_HistoryIndex = -1;
    std::vector<std::string> m_History;
};

#endif // BML_COMMANDBAR_H
