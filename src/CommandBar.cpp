#include "CommandBar.h"

#include <utf8.h>

#include "BML/ICommand.h"
#include "BML/InputHook.h"

#include "ModManager.h"
#include "StringUtils.h"

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
           ImGuiWindowFlags_NoNav;
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
                                                   ImGuiInputTextFlags_CallbackResize;
    if (ImGui::InputText("##CmdBar", &m_Buffer[0], m_Buffer.capacity() + 1, InputTextFlags,
                         &TextEditCallback, this)) {
        if (m_Buffer[0] != '\0') {
            m_History.emplace_back(m_Buffer);
            BML_GetModManager()->ExecuteCommand(m_Buffer.c_str());
        }
        ToggleCommandBar(false);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        ToggleCommandBar(false);

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

void CommandBar::ToggleCommandBar(bool on) {
    auto *inputHook = BML_GetModManager()->GetInputManager();
    if (on) {
        Show();
        m_Buffer[0] = '\0';
        inputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
        m_HistoryIndex = static_cast<int>(m_History.size());
    } else {
        Hide();
        m_Buffer[0] = '\0';
        BML_GetModManager()->AddTimerLoop(1ul, [this, inputHook] {
            if (inputHook->oIsKeyDown(CKKEY_ESCAPE) || inputHook->oIsKeyDown(CKKEY_RETURN))
                return true;
            inputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
            return false;
        });
    }
}

int CommandBar::OnTextEdit(ImGuiInputTextCallbackData *data) {
    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackCompletion: {
            // Locate beginning of current word
            const char *wordEnd = data->Buf + data->CursorPos;
            const char *wordStart = wordEnd;
            while (wordStart > data->Buf) {
                const char c = wordStart[-1];
                if (std::isspace(c))
                    break;
                --wordStart;
            }
            const int wordCount = wordEnd - wordStart;

            bool completeCmd = true;
            const char *cmdEnd;
            const char *cmdStart;
            if (wordStart == data->Buf) {
                cmdEnd = wordEnd;
                cmdStart = wordStart;
            } else {
                completeCmd = false;
                cmdStart = data->Buf;
                cmdEnd = cmdStart;
                for (char c = *cmdEnd; !isspace(c); ++cmdEnd, c = *cmdEnd) {}
            }
            const int cmdCount = cmdEnd - cmdStart;

            std::vector<std::string> candidates;

            if (completeCmd) {
                const int count = BML_GetModManager()->GetCommandCount();
                for (int i = 0; i < count; ++i) {
                    ICommand *cmd = BML_GetModManager()->GetCommand(i);
                    if (cmd) {
                        if (utf8ncasecmp(cmd->GetName().c_str(), cmdStart, cmdCount) == 0)
                            candidates.push_back(cmd->GetName());
                    }
                }
            } else {
                const auto args = MakeArgs(cmdStart);
                ICommand *cmd = BML_GetModManager()->FindCommand(args[0].c_str());
                if (cmd) {
                    for (const auto &str: cmd->GetTabCompletion(BML_GetModManager(), args)) {
                        if (utf8ncasecmp(str.c_str(), wordStart, wordCount) == 0)
                            candidates.push_back(str);
                    }
                }
            }

            if (candidates.size() == 1) {
                data->DeleteChars(wordStart - data->Buf, wordCount);
                data->InsertChars(data->CursorPos, candidates[0].c_str());
            } else if (candidates.size() > 1) {
                // Multiple matches. Complete as much as we can..
                // So inputting "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                int matchLen = wordCount;
                for (;;) {
                    int c = 0;
                    bool allCandidatesMatches = true;
                    for (std::size_t i = 0; i < candidates.size() && allCandidatesMatches; i++) {
                        if (i == 0)
                            c = toupper(candidates[i][matchLen]);
                        else if (c == 0 || c != toupper(candidates[i][matchLen]))
                            allCandidatesMatches = false;
                    }
                    if (!allCandidatesMatches)
                        break;
                    ++matchLen;
                }

                if (matchLen > 0) {
                    data->DeleteChars(wordStart - data->Buf, wordCount);
                    data->InsertChars(data->CursorPos, candidates[0].c_str(), candidates[0].c_str() + matchLen);
                }

                BML_GetModManager()->SendIngameMessage(utils::JoinString(candidates, ", ").c_str());
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
        case ImGuiInputTextFlags_CallbackResize: {
            // Resize string callback
            IM_ASSERT(data->Buf == m_Buffer.c_str());
            m_Buffer.resize(data->BufTextLen);
            data->Buf = &m_Buffer[0];
        }
        break;
        default:
            break;
    }

    return 0;
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