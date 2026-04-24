#include "CommandBar.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>

#include <utf8.h>

#include "BML/ICommand.h"
#include "BML/InputHook.h"

#include "ModContext.h"
#include "PathUtils.h"
#include "StringUtils.h"

namespace {
    constexpr wchar_t kCommandHistoryFile[] = L"CommandBar.history";

    std::wstring BuildHistoryPath(const std::wstring &loaderDirectory) {
        if (loaderDirectory.empty())
            return {};

        return utils::CombinePathW(loaderDirectory, kCommandHistoryFile);
    }

    void MergeHistoryEntry(std::vector<std::string> &history, const std::string &entry) {
        if (entry.empty())
            return;

        auto it = std::find(history.begin(), history.end(), entry);
        if (it != history.end())
            history.erase(it);

        history.emplace_back(entry);
    }

    std::vector<std::string> ReadHistoryEntries(const std::wstring &path) {
        if (path.empty())
            return {};

        const std::vector<std::uint8_t> bytes = utils::ReadBinaryFileW(path);
        if (bytes.empty())
            return {};

        std::vector<std::string> history;
        std::istringstream input(std::string(bytes.begin(), bytes.end()));
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '\0')
                continue;

            MergeHistoryEntry(history, line);
        }

        return history;
    }

    bool WriteHistoryEntries(const std::wstring &path, const std::vector<std::string> &history) {
        if (path.empty())
            return false;

        const std::wstring tempPath = path + L".tmp";
        if (history.empty()) {
            utils::DeleteFileW(tempPath);
            utils::DeleteFileW(path);
            return true;
        }

        std::string content;
        for (const std::string &entry : history) {
            content.append(entry);
            content.push_back('\n');
        }

        const std::vector<std::uint8_t> bytes(content.begin(), content.end());
        if (!utils::WriteBinaryFileW(tempPath, bytes)) {
            utils::DeleteFileW(tempPath);
            return false;
        }

        if (!utils::MoveFileW(tempPath, path)) {
            utils::DeleteFileW(tempPath);
            return false;
        }

        return true;
    }

    std::string NormalizeCandidateEncoding(const std::string &candidate) {
        if (candidate.empty())
            return candidate;

        const auto *utf8Candidate = reinterpret_cast<const utf8_int8_t *>(candidate.c_str());
        if (utf8valid(utf8Candidate) == nullptr)
            return candidate;

        return utils::Utf16ToUtf8(utils::AnsiToUtf16(candidate));
    }

    bool InsertUniqueCandidate(std::vector<std::string> &candidates, const std::string &candidate) {
        if (candidate.empty())
            return false;

        if (std::find(candidates.begin(), candidates.end(), candidate) != candidates.end())
            return false;

        candidates.emplace_back(candidate);
        return true;
    }
}

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

void CommandBar::OnPreBegin() {
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
    if (ImGui::InputText("##CmdBar", &m_Buffer[0], m_Buffer.capacity() + 1, InputTextFlags, &TextEditCallback, this)) {
        if (m_Buffer[0] != '\0') {
            const std::string commandLine(m_Buffer.c_str());
            RecordHistoryEntry(commandLine);
            BML_GetModContext()->ExecuteCommand(commandLine.c_str());
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

            if (ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                PrevCandidate();
            } else if (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                NextCandidate();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                PrevPageOfCandidates();
            } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                NextPageOfCandidates();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
                m_CandidateSelected = m_CandidateIndex;
                m_ShowHints = false;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                InvalidateCandidates();
            }
        }
        ImGui::EndChild();
    } else {
        if (!m_Candidates.empty()) {
            m_ShowHints = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ToggleCommandBar(false);
    }

    ImGui::SetItemDefaultFocus();
    if (!m_VisiblePrev)
        ImGui::SetKeyboardFocusHere(-1);
}

void CommandBar::OnPostEnd() {
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
    m_HistoryIndex = -1;
    SaveHistory();
}

std::wstring CommandBar::GetHistoryPath() const {
    return BuildHistoryPath(BML_GetModContext()->GetDirectory(BML_DIR_LOADER));
}

void CommandBar::LoadHistory() {
    m_History = ReadHistoryEntries(GetHistoryPath());
    m_HistoryIndex = -1;
}

void CommandBar::SaveHistory() {
    WriteHistoryEntries(GetHistoryPath(), m_History);
}

void CommandBar::RecordHistoryEntry(const std::string &entry) {
    if (entry.empty())
        return;

    MergeHistoryEntry(m_History, entry);
    m_HistoryIndex = -1;
    SaveHistory();
}

void CommandBar::CollectCommandCandidates(const char *cmdStart, int cmdLength) {
    const int count = BML_GetModContext()->GetCommandCount();
    for (int i = 0; i < count; ++i) {
        ICommand *cmd = BML_GetModContext()->GetCommand(i);
        if (!cmd)
            continue;

        const std::string name = NormalizeCandidateEncoding(cmd->GetName());
        if (utf8ncasecmp(name.c_str(), cmdStart, cmdLength) == 0)
            InsertUniqueCandidate(m_Candidates, name);

        const std::string alias = NormalizeCandidateEncoding(cmd->GetAlias());
        if (!alias.empty() && utf8ncasecmp(alias.c_str(), cmdStart, cmdLength) == 0)
            InsertUniqueCandidate(m_Candidates, alias);
    }
}

void CommandBar::CollectArgumentCandidates(const char *wordStart, int wordLength, const char *cmdStart, const char *lineEnd) {
    const auto args = MakeArgsRange(cmdStart, lineEnd);
    if (args.empty())
        return;

    ICommand *cmd = BML_GetModContext()->FindCommand(args[0].c_str());
    if (!cmd)
        return;

    for (const std::string &rawCandidate : cmd->GetTabCompletion(BML_GetModContext(), args)) {
        const std::string candidate = NormalizeCandidateEncoding(rawCandidate);
        if (utf8ncasecmp(candidate.c_str(), wordStart, wordLength) == 0)
            InsertUniqueCandidate(m_Candidates, candidate);
    }
}

void CommandBar::ReplaceCurrentToken(ImGuiInputTextCallbackData *data, const char *replacement, int replacementLength) {
    if (!data || !replacement)
        return;

    const char *tokenStart = data->Buf;
    const char *cursor = data->Buf + data->CursorPos;
    const int leftCount = LastToken(tokenStart, cursor);
    const char *textEnd = data->Buf + data->BufTextLen;
    const char *tokenEnd = cursor;
    while (tokenEnd < textEnd && !std::isspace(static_cast<unsigned char>(*tokenEnd)))
        ++tokenEnd;

    const int deletePos = static_cast<int>(tokenStart - data->Buf);
    const int deleteCount = leftCount + static_cast<int>(tokenEnd - cursor);
    data->DeleteChars(deletePos, deleteCount);

    const int insertLength = replacementLength >= 0 ? replacementLength : static_cast<int>(std::strlen(replacement));
    data->InsertChars(deletePos, replacement, replacement + insertLength);

    if (replacementLength >= 0)
        return;

    const bool hasSpaceAfter = (tokenEnd < textEnd) && std::isspace(static_cast<unsigned char>(*tokenEnd));
    if (!hasSpaceAfter)
        data->InsertChars(deletePos + insertLength, " ");
}

void CommandBar::SyncCandidatePageFromIndex() {
    for (int i = static_cast<int>(m_CandidatePages.size() - 1); i >= 0; --i) {
        if (m_CandidateIndex >= m_CandidatePages[i]) {
            m_CandidatePage = i;
            return;
        }
    }

    m_CandidatePage = 0;
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
    SyncCandidatePageFromIndex();
}

void CommandBar::PrevCandidate() {
    if (m_CandidateIndex == 0)
        m_CandidateIndex = (int) m_Candidates.size();
    m_CandidateIndex = (m_CandidateIndex - 1) % (int) m_Candidates.size();
    SyncCandidatePageFromIndex();
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
    const int wordLength = LastToken(wordStart, lineEnd);

    // Preserve raw end so arg parsing can keep empty-last-arg cases
    const char *rawLineEnd = lineEnd;
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
            CollectCommandCandidates(cmdStart, cmdLength);
        } else {
            CollectArgumentCandidates(wordStart, wordLength, cmdStart, rawLineEnd);
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

            if (m_Candidates.size() == 1) {
                ReplaceCurrentToken(data, m_Candidates[0].c_str());
            } else if (m_Candidates.size() > 1) {
                int matchLen = 0;
                for (;;) {
                    int c = 0;
                    bool allCandidatesMatches = true;
                    for (size_t i = 0; i < m_Candidates.size() && allCandidatesMatches; i++) {
                        auto &candidate = m_Candidates[i];
                        if (i == 0)
                            c = toupper(static_cast<unsigned char>(candidate[matchLen]));
                        else if (c == 0 || c != toupper(static_cast<unsigned char>(candidate[matchLen])))
                            allCandidatesMatches = false;
                    }
                    if (!allCandidatesMatches)
                        break;
                    ++matchLen;
                }

                if (matchLen > 0) {
                    ReplaceCurrentToken(data, m_Candidates[0].c_str(), matchLen);
                }
            }
        }
        break;
        case ImGuiInputTextFlags_CallbackHistory: {
            if (!m_Candidates.empty()) {
                InvalidateCandidates();
            }

            const int prevHistoryPos = m_HistoryIndex;
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
                    ReplaceCurrentToken(data, m_Candidates[m_CandidateSelected].c_str());

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
        if (!std::isspace(static_cast<unsigned char>(c))) {
            break;
        }
        ++lineStart;
    }
    if (lineStart == lineEnd)
        return;

    // Skip white spaces at the end of the line
    while (lineEnd > lineStart) {
        const char c = lineEnd[-1];
        if (!std::isspace(static_cast<unsigned char>(c)))
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
        if (std::isspace(static_cast<unsigned char>(c)))
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
        if (std::isspace(static_cast<unsigned char>(c)))
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
        if (std::isspace(static_cast<unsigned char>(*rp)) || *rp == '\0') {
            const size_t len = rp - lp;
            if (len != 0) {
                const char bk = *rp;
                *rp = '\0';
                args.emplace_back(lp);
                *rp = bk;
            }

            if (*rp != '\0') {
                while (std::isspace(static_cast<unsigned char>(*rp)))
                    ++rp;
                --rp;
            }

            lp = utf8codepoint(rp, &temp);
            if (std::isspace(static_cast<unsigned char>(*rp)) && *lp == '\0') {
                args.emplace_back("");
                break;
            }
        }

        rp = utf8codepoint(rp, &cp);
    }

    delete[] buf;

    return args;
}

std::vector<std::string> CommandBar::MakeArgsRange(const char *begin, const char *end) {
    if (!begin || !end || begin >= end)
        return {};

    const size_t size = static_cast<size_t>(end - begin);
    char *buf = new char[size + 1];
    // Copy bytes as-is and NUL-terminate; downstream parser will handle tokenization
    memcpy(buf, begin, size);
    buf[size] = '\0';

    auto args = MakeArgs(buf);
    delete[] buf;
    return args;
}
