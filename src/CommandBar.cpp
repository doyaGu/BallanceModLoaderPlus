#include "CommandBar.h"

#include <sstream>
#include <utf8.h>

#include "BML/ICommand.h"
#include "BML/InputHook.h"

#include "ModManager.h"
#include "PathUtils.h"

#define COMMAND_HISTORY_FILE L"\\CommandBar.history"

CommandBar::CommandBar() : Window("CommandBar"), m_Buffer(65535, '\0') {
    SetVisibility(false);
}

CommandBar::~CommandBar() = default;

ImGuiWindowFlags CommandBar::GetFlags() {
    return ImGuiWindowFlags_NoDecoration |
           ImGuiWindowFlags_NoBackground |
           ImGuiWindowFlags_NoResize |
           ImGuiWindowFlags_NoCollapse |
           ImGuiWindowFlags_NoMove |
           ImGuiWindowFlags_NoNav |
           ImGuiWindowFlags_NoSavedSettings;
}

void CommandBar::OnBegin() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Bui::GetMenuColor());
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Bui::GetMenuColor());

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    m_WindowPos = ImVec2(vpSize.x * 0.02f, vpSize.y * 0.93f);
    m_WindowSize = ImVec2(vpSize.x * 0.96f, 0.0f);
    ImGui::SetNextWindowPos(m_WindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_WindowSize, ImGuiCond_Always);

    if (!m_VisiblePrev)
        ImGui::SetNextWindowFocus();
}

void CommandBar::OnDraw() {
    ImGui::SetNextItemWidth(m_WindowSize.x);

    constexpr ImGuiInputTextFlags InputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                   ImGuiInputTextFlags_EscapeClearsAll |
                                                   ImGuiInputTextFlags_CallbackCompletion |
                                                   ImGuiInputTextFlags_CallbackHistory |
                                                   ImGuiInputTextFlags_CallbackAlways |
                                                   ImGuiInputTextFlags_CallbackResize |
                                                   ImGuiInputTextFlags_CallbackEdit;
    if (ImGui::InputText("##CmdBar", &m_Buffer[0], m_Buffer.capacity() + 1, InputTextFlags,
                         &TextEditCallback, this)) {
        if (m_Buffer[0] != '\0') {
            BML_GetModManager()->ExecuteCommand(m_Buffer.c_str());
            m_History.emplace_back(m_Buffer);
            m_HistoryIndex = -1;
        }
        ToggleCommandBar(false);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        ToggleCommandBar(false);

    ImGui::SetItemDefaultFocus();
    if (!m_VisiblePrev)
        ImGui::SetKeyboardFocusHere(-1);

    if (m_Completion) {
        constexpr ImVec4 SelectedColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

        const int n = m_CandidatePage != m_CandidatePages.size() - 1 ?
            m_CandidatePages[m_CandidatePage + 1] : (int) m_Candidates.size();
        for (int i = m_CandidatePages[m_CandidatePage]; i < n; ++i) {
            if (i != m_CandidateIndex) {
                if (i < n - 1) {
                    ImGui::Text("%s | ", m_Candidates[i].c_str());
                    ImGui::SameLine();
                } else {
                    ImGui::Text("%s", m_Candidates[i].c_str());
                }
            } else {
                const auto str = m_Candidates[i].c_str();

                // Draw selected candidate background
                ImDrawList *dl = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                const ImVec2 size = ImGui::CalcTextSize(str);
                dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32_WHITE);

                ImGui::TextColored(SelectedColor, "%s", str);

                if (i < n - 1) {
                    ImGui::SameLine();
                    ImGui::Text(" | ");
                    ImGui::SameLine();
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            m_CandidateSelected = m_CandidateIndex;
        }
    }
}

void CommandBar::OnAfterEnd() {
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void CommandBar::OnShow() {
    m_VisiblePrev = false;
}

void CommandBar::OnHide() {
    m_VisiblePrev = true;
}

void CommandBar::PrintHistory() {
    const int count = static_cast<int>(m_History.size());
    for (int i = 0; i < count; ++i) {
        const std::string str = "[" + std::to_string(i + 1) + "] " + m_History[(count - 1) - i];
        BML_GetModManager()->SendIngameMessage(str.c_str());
    }
}

void CommandBar::ExecuteHistory(int index) {
    if (index < 1 || index > static_cast<int>(m_History.size()))
        return;

    const std::string line = m_History[(m_History.size() - index)];
    BML_GetModManager()->ExecuteCommand(line.c_str());
}

void CommandBar::ClearHistory() {
    m_History.clear();
    m_HistoryIndex = -1;
}

void CommandBar::LoadHistory() {
    std::wstring path = BML_GetModManager()->GetDirectory(BML_DIR_LOADER);
    path.append(COMMAND_HISTORY_FILE);

    if (!utils::FileExistsW(path))
        return;

    FILE *fp = _wfopen(path.c_str(), L"rb");
    if (!fp)
        return;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    if (len == 0) {
        fclose(fp);
        return;
    }

    char *buf = new char[len + 1];
    if (fread(buf, sizeof(char), len, fp) != len) {
        delete[] buf;
        fclose(fp);
        return;
    }

    buf[len] = '\0';
    fclose(fp);

    std::istringstream iss(buf);
    delete[] buf;

    std::string line;
    while (std::getline(iss, line))
        m_History.emplace_back(line);
}

void CommandBar::SaveHistory() {
    if (m_History.empty())
        return;

    std::wstring path = BML_GetModManager()->GetDirectory(BML_DIR_LOADER);
    path.append(COMMAND_HISTORY_FILE);

    FILE *fp = _wfopen(path.c_str(), L"wb");
    if (!fp)
        return;

    for (const auto &str: m_History)
        fprintf(fp, "%s\n", str.c_str());

    fclose(fp);
}

void CommandBar::ToggleCommandBar(bool on) {
    auto *inputHook = BML_GetModManager()->GetInputManager();
    if (on) {
        Show();
        m_Buffer[0] = '\0';
        inputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
        m_HistoryIndex = static_cast<int>(m_History.size());
    } else {
        Hide();
        ImGui::SetWindowFocus(nullptr);
        m_Buffer[0] = '\0';
        BML_GetModManager()->AddTimerLoop(1ul, [this, inputHook] {
            if (inputHook->oIsKeyDown(CKKEY_ESCAPE) || inputHook->oIsKeyDown(CKKEY_RETURN))
                return true;
            inputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
            return false;
        });
    }
}

void CommandBar::InvalidateCandidates() {
    m_CandidateSelected = -1;
    m_Completion = false;
    m_CandidateIndex = 0;
    m_CandidatePage = 0;
    m_CandidatePages.clear();
    m_Candidates.clear();
}

size_t CommandBar::OnCompletion(const char *lineStart, const char *lineEnd) {
    const char *wordStart = lineStart;
    int wordCount = LastToken(wordStart, lineEnd);

    StripLine(lineStart, lineEnd);

    if (m_Candidates.empty()) {
        bool completeCmd = true;
        const char *cmdEnd = lineEnd;
        const char *cmdStart;

        if (wordStart == lineStart) {
            // If the cursor is at the beginning of the line, complete the command
            cmdStart = wordStart;
        } else {
            // Otherwise, complete the argument
            cmdStart = lineStart;
            completeCmd = false;
        }
        const int cmdLength = FirstToken(cmdStart, cmdEnd);

        if (completeCmd) {
            // Add candidate commands
            const int count = BML_GetModManager()->GetCommandCount();
            for (int i = 0; i < count; ++i) {
                ICommand *cmd = BML_GetModManager()->GetCommand(i);
                if (cmd) {
                    if (utf8ncasecmp(cmd->GetName().c_str(), cmdStart, cmdLength) == 0)
                        m_Candidates.push_back(cmd->GetName());
                }
            }
        } else {
            // Add candidate arguments
            const auto args = MakeArgs(cmdStart);
            if (!args.empty()) {
                ICommand *cmd = BML_GetModManager()->FindCommand(args[0].c_str());
                if (cmd) {
                    for (const auto &str : cmd->GetTabCompletion(BML_GetModManager(), args)) {
                        if (utf8ncasecmp(str.c_str(), wordStart, wordCount) == 0)
                            m_Candidates.push_back(str);
                    }
                }
            }
        }

        const float max = m_WindowSize.x * 0.8f;
        float width = 0.0f;
        m_CandidatePages.push_back(0);
        for (int i = 0; i < (int) m_Candidates.size(); ++i) {
            const ImVec2 size = ImGui::CalcTextSize(m_Candidates[i].c_str());
            width += size.x;
            if (width > max) {
                m_CandidatePages.push_back(i);
                width = 0.0f;
            }
        }
    } else {
        m_CandidateIndex = (m_CandidateIndex + 1) % m_Candidates.size();
        for (int i = (int) (m_CandidatePages.size() - 1); i >= 0; --i) {
            if ((int) m_CandidateIndex >= m_CandidatePages[i]) {
                m_CandidatePage = i;
                break;
            }
        }
    }

    return m_Candidates.size();
}

int CommandBar::OnTextEdit(ImGuiInputTextCallbackData *data) {
    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackCompletion: {
            OnCompletion(data->Buf, data->Buf + data->CursorPos);

            const char *wordStart = data->Buf;
            int wordCount = LastToken(wordStart, data->Buf + data->CursorPos);

            if (m_Candidates.size() == 1) {
                // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
                data->DeleteChars(wordStart - data->Buf, wordCount);
                data->InsertChars(data->CursorPos, m_Candidates[0].c_str());
            } else if (m_Candidates.size() > 1) {
                m_Completion = true;

                // Multiple matches. Complete as much as we can..
                // So inputting "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                int matchLen = 0;
                for (;;) {
                    int c = 0;
                    bool allCandidatesMatches = true;
                    for (std::size_t i = 0; i < m_Candidates.size() && allCandidatesMatches; i++) {
                        auto &candidate = m_Candidates[i];
                        if (i == 0)
                            c = toupper(candidate[matchLen]);
                        else if (c == 0 || c != toupper(candidate[matchLen]))
                            allCandidatesMatches = false;
                    }
                    if (!allCandidatesMatches)
                        break;
                    ++matchLen;
                }

                if (matchLen > 0) {
                    data->DeleteChars(wordStart - data->Buf, wordCount);
                    const auto &best = m_Candidates[0];
                    data->InsertChars(data->CursorPos, best.c_str(), best.c_str() + matchLen);
                }
            }
        }
        break;
        case ImGuiInputTextFlags_CallbackHistory: {
            const unsigned int prevHistoryPos = m_HistoryIndex;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (m_HistoryIndex == -1)
                    m_HistoryIndex = static_cast<int>(m_History.size() - 1);
                else if (m_HistoryIndex > 0)
                    m_HistoryIndex--;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (m_HistoryIndex != -1)
                    if (++m_HistoryIndex >= static_cast<int>(m_History.size()))
                        m_HistoryIndex = -1;
            }

            if (prevHistoryPos != m_HistoryIndex) {
                const std::string &historyStr = (m_HistoryIndex >= 0) ? m_History[m_HistoryIndex] : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, historyStr.c_str());
            }
        }
        break;
        case ImGuiInputTextFlags_CallbackAlways: {
            if (m_Completion && m_CandidateSelected != -1) {
                const char *wordStart = data->Buf;
                const char *wordEnd = data->Buf + data->CursorPos;
                int wordCount = LastToken(wordStart, wordEnd);
                data->DeleteChars(wordStart - data->Buf, wordCount);
                data->InsertChars(data->CursorPos, m_Candidates[m_CandidateSelected].c_str());

                InvalidateCandidates();
            }
        }
        break;
        case ImGuiInputTextFlags_CallbackResize: {
            // Resize string callback
            IM_ASSERT(data->Buf == m_Buffer.c_str());
            m_Buffer.resize(data->BufTextLen);
            data->Buf = &m_Buffer[0];
        }
        break;
        case ImGuiInputTextFlags_CallbackEdit: {
            if (!m_Candidates.empty()) {
                InvalidateCandidates();
            }
        }
        break;
        default:
            break;
    }

    return 0;
}

int CommandBar::TextEditCallback(ImGuiInputTextCallbackData *data) {
    auto *mod = static_cast<CommandBar *>(data->UserData);
    return mod->OnTextEdit(data);
}

void CommandBar::StripLine(const char *&lineStart, const char *&lineEnd) {
    if (lineStart == lineEnd)
        return;

    // Skip white spaces at the beginning of the line
    while (lineStart < lineEnd) {
        const char c = *lineStart;
        if (!std::isspace(c)) {
            break;
        }
        ++lineStart;
    }
    if (lineStart == lineEnd)
        return;

    // Skip white spaces at the end of the line
    while (lineEnd > lineStart) {
        const char c = lineEnd[-1];
        if (!std::isspace(c))
            break;
        --lineEnd;
    }
}

int CommandBar::FirstToken(const char *tokenStart, const char *&tokenEnd) {
    if (tokenStart == tokenEnd)
        return 0;

    const char *lineEnd = tokenEnd;
    tokenEnd = tokenStart;
    while (tokenEnd < lineEnd) {
        const char c = *tokenEnd;
        if (std::isspace(c))
            break;
        ++tokenEnd;
    }

    return tokenEnd - tokenStart;
}

int CommandBar::LastToken(const char *&tokenStart, const char *tokenEnd) {
    if (tokenStart == tokenEnd)
        return 0;

    const char *lineStart = tokenStart;
    tokenStart = tokenEnd;
    while (tokenStart > lineStart) {
        const char c = tokenStart[-1];
        if (std::isspace(c))
            break;
        --tokenStart;
    }

    return tokenEnd - tokenStart;
}

std::vector<std::string> CommandBar::MakeArgs(const char *line) {
    if (!line || line[0] == '\0')
        return {};

    size_t size = utf8size(line);
    char *buf = new char[size + 1];
    utf8ncpy(buf, line, size);
    buf[size] = '\0';

    std::vector<std::string> args;

    char *lp = buf;
    char *rp = lp;
    char *end = lp + size;
    utf8_int32_t cp, temp;
    utf8codepoint(rp, &cp);
    while (rp != end) {
        if (std::isspace(*rp) || *rp == '\0') {
            size_t len = rp - lp;
            if (len != 0) {
                char bk = *rp;
                *rp = '\0';
                args.emplace_back(lp);
                *rp = bk;
            }

            if (*rp != '\0') {
                while (std::isspace(*rp))
                    ++rp;
                --rp;
            }

            lp = utf8codepoint(rp, &temp);
        }

        rp = utf8codepoint(rp, &cp);
    }

    delete[] buf;

    return std::move(args);
}
