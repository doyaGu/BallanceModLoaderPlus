#include "CommandBar.h"

#include <sstream>
#include <utf8.h>

#include "BML/ICommand.h"
#include "BML/InputHook.h"

#include "ModContext.h"
#include "PathUtils.h"

#define COMMAND_HISTORY_FILE L"\\CommandBar.history"

CommandBar::CommandBar() : Window("CommandBar"), m_Buffer(65535, '\0') {
    Hide();
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 1.0f));
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
    constexpr ImU32 ButtonColor = IM_COL32(99, 99, 99, 255); // Dark Grey
    ImGui::PushStyleColor(ImGuiCol_Button, ButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ButtonColor);
    ImGui::Button(">");
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    const ImVec2 buttonSize = ImGui::GetItemRectSize();
    ImGui::SetNextItemWidth(m_WindowSize.x * ((m_WindowSize.x - buttonSize.x) / m_WindowSize.x));

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
            BML_GetModContext()->ExecuteCommand(m_Buffer.c_str());
            if (m_HistorySet.find(m_Buffer) == m_HistorySet.end()) {
                m_HistorySet.insert(m_Buffer);
                m_History.emplace_back(m_Buffer);
                m_HistoryIndex = -1;
            }
        }
        ToggleCommandBar(false);
    }

    if (m_ShowHints) {
        if (ImGui::BeginChild("##CmdHints")) {
            constexpr ImVec4 SelectedColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

            if (m_CandidatePage != 0) {
                ImGui::TextUnformatted("< ");
                ImGui::SameLine(0, 0);
            }

            const int n = m_CandidatePage != m_CandidatePages.size() - 1 ?
                m_CandidatePages[m_CandidatePage + 1] : (int) m_Candidates.size();
            for (int i = m_CandidatePages[m_CandidatePage]; i < n; ++i) {
                if (i != m_CandidatePages[m_CandidatePage]) {
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted(" | ");
                    ImGui::SameLine(0, 0);
                }

                if (i != m_CandidateIndex) {
                    ImGui::Text("%s", m_Candidates[i].c_str());
                } else {
                    const auto str = m_Candidates[i].c_str();

                    // Draw selected candidate background
                    ImDrawList *dl = ImGui::GetWindowDrawList();
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    const ImVec2 size = ImGui::CalcTextSize(str);
                    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32_WHITE);

                    ImGui::TextColored(SelectedColor, "%s", str);
                }
            }

            if (n != (int) m_Candidates.size()) {
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(" >");
            }

            if ((ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyPressed(ImGuiKey_Tab)) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                PrevCandidate();
            } else if (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                NextCandidate();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                PrevPageOfCandidates();
            } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                NextPageOfCandidates();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                m_CandidateSelected = m_CandidateIndex;
                m_ShowHints = false;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                InvalidateCandidates();
            }
        }
        ImGui::EndChild();
    } else {
        if (!m_Candidates.empty()) {
            m_ShowHints = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ToggleCommandBar(false);
    }

    ImGui::SetItemDefaultFocus();
    if (!m_VisiblePrev)
        ImGui::SetKeyboardFocusHere(-1);
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
        BML_GetModContext()->SendIngameMessage(str.c_str());
    }
}

void CommandBar::ExecuteHistory(int index) {
    if (index < 1 || index > static_cast<int>(m_History.size()))
        return;

    const std::string line = m_History[(m_History.size() - index)];
    BML_GetModContext()->ExecuteCommand(line.c_str());
}

void CommandBar::ClearHistory() {
    m_History.clear();
    m_HistorySet.clear();
    m_HistoryIndex = -1;
}

void CommandBar::LoadHistory() {
    std::wstring path = BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
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
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '\0')
            continue;
        if (m_HistorySet.find(line) != m_HistorySet.end())
            continue;

        m_History.emplace_back(line);
    }
}

void CommandBar::SaveHistory() {
    if (m_History.empty())
        return;

    std::wstring path = BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
    path.append(COMMAND_HISTORY_FILE);

    FILE *fp = _wfopen(path.c_str(), L"wb");
    if (!fp)
        return;

    for (const auto &str: m_History)
        fprintf(fp, "%s\n", str.c_str());

    fclose(fp);
}

void CommandBar::ToggleCommandBar(bool on) {
    auto *inputHook = BML_GetModContext()->GetInputManager();
    if (on) {
        Show();
        m_Buffer[0] = '\0';
        inputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
        m_HistoryIndex = static_cast<int>(m_History.size());
    } else {
        Hide();
        ImGui::SetWindowFocus(nullptr);
        m_Buffer[0] = '\0';
        BML_GetModContext()->AddTimerLoop(1ul, [this, inputHook] {
            if (inputHook->oIsKeyDown(CKKEY_ESCAPE) || inputHook->oIsKeyDown(CKKEY_RETURN))
                return true;
            inputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
            return false;
        });
    }
}

void CommandBar::NextCandidate() {
    m_CandidateIndex = (m_CandidateIndex + 1) % (int) m_Candidates.size();

    for (int i = (int) (m_CandidatePages.size() - 1); i >= 0; --i) {
        if (m_CandidateIndex >= m_CandidatePages[i]) {
            m_CandidatePage = i;
            break;
        }
    }
}

void CommandBar::PrevCandidate() {
    if (m_CandidateIndex == 0)
        m_CandidateIndex = (int) m_Candidates.size();
    m_CandidateIndex = (m_CandidateIndex - 1) % (int) m_Candidates.size();

    for (int i = (int) (m_CandidatePages.size() - 1); i >= 0; --i) {
        if (m_CandidateIndex >= m_CandidatePages[i]) {
            m_CandidatePage = i;
            break;
        }
    }
}

void CommandBar::NextPageOfCandidates() {
    if (m_CandidatePages.size() == 1) {
        m_CandidateIndex = (int) m_Candidates.size() - 1;
    } else {
        const int nextPage = (m_CandidatePage + 1) % (int) m_CandidatePages.size();
        const int nextIndex = nextPage > 0 ? m_CandidatePages[nextPage] - 1 : (int) m_Candidates.size() - 1;
        if (m_CandidateIndex == nextIndex) {
            m_CandidateIndex = m_CandidatePages[nextPage];
            m_CandidatePage = nextPage;
        } else {
            m_CandidateIndex = nextIndex;
        }
    }
}

void CommandBar::PrevPageOfCandidates() {
    if (m_CandidatePages.size() == 1) {
        m_CandidateIndex = 0;
    } else {
        const int prevPage = m_CandidatePage > 0 ? (m_CandidatePage - 1) % (int) m_CandidatePages.size() : (int) m_CandidatePages.size() - 1;
        const int prevIndex = m_CandidatePages[m_CandidatePage];
        if (m_CandidateIndex == prevIndex) {
            m_CandidateIndex = m_CandidatePage > 0 ? m_CandidatePages[prevPage + 1] - 1 : (int) m_Candidates.size() - 1;
            m_CandidatePage = prevPage;
        } else {
            m_CandidateIndex = prevIndex;
        }
    }
}

void CommandBar::InvalidateCandidates() {
    m_CandidateSelected = -1;
    m_CandidateIndex = 0;
    m_CandidatePage = 0;
    m_CandidatePages.clear();
    m_Candidates.clear();

    m_ShowHints = false;
}

void CommandBar::GenerateCandidatePages() {
    if (m_Candidates.empty())
        return;

    const float sep = ImGui::CalcTextSize(" | ").x;
    const float pager = ImGui::CalcTextSize("< ").x;
    const float max = m_WindowSize.x;
    float width = -sep;

    m_CandidatePages.clear();
    m_CandidatePages.push_back(0); // Start the first page

    for (int i = 0; i < (int) m_Candidates.size(); ++i) {
        const ImVec2 size = ImGui::CalcTextSize(m_Candidates[i].c_str());
        width += size.x + sep;
        if (width > max) {
            m_CandidatePages.push_back(i); // Start a new page
            width = size.x + pager * 2;
        }
    }
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
            const int count = BML_GetModContext()->GetCommandCount();
            for (int i = 0; i < count; ++i) {
                ICommand *cmd = BML_GetModContext()->GetCommand(i);
                if (cmd) {
                    auto name = cmd->GetName();
                    if (utf8ncasecmp(name.c_str(), cmdStart, cmdLength) == 0)
                        m_Candidates.emplace_back(std::move(name));
                    auto alias = cmd->GetAlias();
                    if (!alias.empty() && utf8ncasecmp(alias.c_str(), cmdStart, cmdLength) == 0)
                        m_Candidates.emplace_back(std::move(alias));
                }
            }
        } else {
            // Add candidate arguments
            const auto args = MakeArgs(cmdStart);
            if (!args.empty()) {
                ICommand *cmd = BML_GetModContext()->FindCommand(args[0].c_str());
                if (cmd) {
                    for (const auto &str : cmd->GetTabCompletion(BML_GetModContext(), args)) {
                        if (utf8ncasecmp(str.c_str(), wordStart, wordCount) == 0)
                            m_Candidates.push_back(str);
                    }
                }
            }
        }

        GenerateCandidatePages();
    } else {
        NextCandidate();
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
                data->InsertChars(data->CursorPos, " ");
            } else if (m_Candidates.size() > 1) {
                // Multiple matches. Complete as much as we can..
                // So inputting "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                int matchLen = 0;
                for (;;) {
                    int c = 0;
                    bool allCandidatesMatches = true;
                    for (size_t i = 0; i < m_Candidates.size() && allCandidatesMatches; i++) {
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
            if (!m_Candidates.empty()) {
                InvalidateCandidates();
            }

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
            if (!m_Candidates.empty()) {
                if (m_CandidateSelected != -1) {
                    const char *wordStart = data->Buf;
                    const char *wordEnd = data->Buf + data->CursorPos;
                    const int wordCount = LastToken(wordStart, wordEnd);
                    data->DeleteChars(wordStart - data->Buf, wordCount);
                    data->InsertChars(data->CursorPos, m_Candidates[m_CandidateSelected].c_str());
                    data->InsertChars(data->CursorPos, " ");

                    InvalidateCandidates();
                }
            }

            if (m_CursorPos != data->CursorPos) {
                InvalidateCandidates();
            }

            m_CursorPos = data->CursorPos;
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
            const size_t len = rp - lp;
            if (len != 0) {
                const char bk = *rp;
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
            if (std::isspace(*rp) && *lp == '\0') {
                args.emplace_back("");
                break;
            }
        }

        rp = utf8codepoint(rp, &cp);
    }

    delete[] buf;

    return std::move(args);
}
