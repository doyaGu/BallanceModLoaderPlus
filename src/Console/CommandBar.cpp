#include "Console/CommandBar.h"

#include "UI/BuiInternal.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>

#include <utf8.h>
#include <misc/cpp/imgui_stdlib.h>

#include "BML/ICommand.h"
#include "Console/CommandContext.h"
#include "Loader/ModContext.h"
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
}

bool CommandBar::CandidateState::Add(const std::string &candidate) {
    if (!utils::AppendUnique(m_Items, candidate))
        return false;

    m_PageStarts.clear();
    m_Selected = -1;
    m_HintsVisible = false;
    return true;
}

void CommandBar::CandidateState::Clear() {
    m_Items.clear();
    m_PageStarts.clear();
    m_Index = 0;
    m_Page = 0;
    m_Selected = -1;
    m_HintsVisible = false;
}

void CommandBar::CandidateState::BuildPages(float maxWidth) {
    m_PageStarts.clear();
    m_Page = 0;
    if (m_Items.empty()) {
        m_Index = 0;
        return;
    }

    const float separatorWidth = ImGui::CalcTextSize(" | ").x;
    const float pagerWidth = ImGui::CalcTextSize("< ").x;
    float width = -separatorWidth;
    m_PageStarts.push_back(0);

    for (int i = 0; i < static_cast<int>(m_Items.size()); ++i) {
        const float itemWidth = ImGui::CalcTextSize(m_Items[i].c_str()).x;
        width += itemWidth + separatorWidth;
        if (width > maxWidth && i > m_PageStarts.back()) {
            m_PageStarts.push_back(i);
            width = itemWidth + pagerWidth * 2.0f;
        }
    }

    m_Index = std::clamp(m_Index, 0, static_cast<int>(m_Items.size()) - 1);
    SyncPageFromIndex();
}

void CommandBar::CandidateState::SyncPageFromIndex() {
    for (int i = static_cast<int>(m_PageStarts.size()); i-- > 0;) {
        if (m_Index >= m_PageStarts[i]) {
            m_Page = i;
            return;
        }
    }
    m_Page = 0;
}

void CommandBar::CandidateState::Next() {
    if (m_Items.empty())
        return;

    m_Index = (m_Index + 1) % static_cast<int>(m_Items.size());
    SyncPageFromIndex();
}

void CommandBar::CandidateState::Previous() {
    if (m_Items.empty())
        return;

    if (m_Index == 0)
        m_Index = static_cast<int>(m_Items.size());
    --m_Index;
    SyncPageFromIndex();
}

void CommandBar::CandidateState::NextPage() {
    if (m_Items.empty() || m_PageStarts.empty())
        return;
    if (m_PageStarts.size() == 1) {
        m_Index = static_cast<int>(m_Items.size()) - 1;
        return;
    }

    const int pageCount = static_cast<int>(m_PageStarts.size());
    const int nextPage = (m_Page + 1) % pageCount;
    const int nextIndex = nextPage > 0 ? m_PageStarts[nextPage] - 1 : static_cast<int>(m_Items.size()) - 1;
    if (m_Index == nextIndex) {
        m_Index = m_PageStarts[nextPage];
        m_Page = nextPage;
    } else {
        m_Index = nextIndex;
    }
}

void CommandBar::CandidateState::PreviousPage() {
    if (m_Items.empty() || m_PageStarts.empty())
        return;
    if (m_PageStarts.size() == 1) {
        m_Index = 0;
        return;
    }

    const int pageCount = static_cast<int>(m_PageStarts.size());
    const int previousPage = m_Page > 0 ? m_Page - 1 : pageCount - 1;
    const int previousIndex = m_PageStarts[m_Page];
    if (m_Index == previousIndex) {
        m_Index = m_Page > 0 ? m_PageStarts[previousPage + 1] - 1 : static_cast<int>(m_Items.size()) - 1;
        m_Page = previousPage;
    } else {
        m_Index = previousIndex;
    }
}

void CommandBar::CandidateState::ShowHints() {
    m_HintsVisible = !m_Items.empty() && !m_PageStarts.empty();
}

void CommandBar::CandidateState::SelectCurrent() {
    if (m_Items.empty())
        return;
    m_Selected = m_Index;
    m_HintsVisible = false;
}

int CommandBar::CandidateState::PageBegin() const {
    return m_PageStarts.empty() ? 0 : m_PageStarts[m_Page];
}

int CommandBar::CandidateState::PageEnd() const {
    if (m_PageStarts.empty())
        return 0;
    return m_Page + 1 < static_cast<int>(m_PageStarts.size())
        ? m_PageStarts[m_Page + 1]
        : static_cast<int>(m_Items.size());
}

const std::string *CommandBar::CandidateState::Selected() const {
    return m_Selected >= 0 && m_Selected < static_cast<int>(m_Items.size())
        ? &m_Items[m_Selected]
        : nullptr;
}

CommandBar::CommandBar() : Window("CommandBar") {
    m_Buffer.reserve(65535);
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
                                                   ImGuiInputTextFlags_CallbackEdit;
    if (ImGui::InputText("##CmdBar", &m_Buffer, InputTextFlags, &TextEditCallback, this)) {
        if (!m_Buffer.empty()) {
            const std::string commandLine = m_Buffer;
            RecordHistoryEntry(commandLine);
            BML_GetModContext()->ExecuteCommand(commandLine.c_str());
        }
        ToggleCommandBar(false);
    }

    if (m_Candidates.AreHintsVisible()) {
        if (ImGui::BeginChild("##CmdHints")) {
            constexpr ImVec4 SelectedColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

            if (m_Candidates.CurrentPage() != 0) {
                ImGui::TextUnformatted("< ");
                ImGui::SameLine(0, 0);
            }

            const int begin = m_Candidates.PageBegin();
            const int end = m_Candidates.PageEnd();
            for (int i = begin; i < end; ++i) {
                if (i != begin) {
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted(" | ");
                    ImGui::SameLine(0, 0);
                }

                if (i != m_Candidates.CurrentIndex()) {
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

            if (end != static_cast<int>(m_Candidates.Size())) {
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
                m_Candidates.SelectCurrent();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                InvalidateCandidates();
            }
        }
        ImGui::EndChild();
    } else {
        if (!m_Candidates.Empty()) {
            m_Candidates.ShowHints();
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
    const auto commands = BML_GetModContext()->GetCommandSnapshot();
    for (const auto &command : commands) {
        const std::string name = NormalizeCandidateEncoding(command.Name);
        if (!name.empty() && utf8ncasecmp(name.c_str(), cmdStart, cmdLength) == 0)
            m_Candidates.Add(name);

        const std::string alias = NormalizeCandidateEncoding(command.Alias);
        if (!alias.empty() && utf8ncasecmp(alias.c_str(), cmdStart, cmdLength) == 0)
            m_Candidates.Add(alias);
    }
}

void CommandBar::CollectArgumentCandidates(const char *wordStart, int wordLength, const char *cmdStart, const char *lineEnd) {
    const std::string commandLine(cmdStart, lineEnd);
    auto args = BML::CommandContext::ParseCommandLine(commandLine.c_str());
    if (!commandLine.empty() && std::isspace(static_cast<unsigned char>(commandLine.back())))
        args.emplace_back();
    if (args.empty())
        return;

    for (const std::string &rawCandidate :
         BML_GetModContext()->CompleteCommand(args[0].c_str(), args)) {
        const std::string candidate = NormalizeCandidateEncoding(rawCandidate);
        if (!candidate.empty() && utf8ncasecmp(candidate.c_str(), wordStart, wordLength) == 0)
            m_Candidates.Add(candidate);
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

void CommandBar::ToggleCommandBar(bool on) {
    if (on) {
        if (IsVisible())
            return;
        Show();
        m_Buffer.clear();
        Bui::BlockKeyboardInput(this);
        m_HistoryIndex = static_cast<int>(m_History.size());
    } else {
        if (!IsVisible())
            return;
        Hide();
        ImGui::SetWindowFocus(nullptr);
        m_Buffer.clear();
        Bui::UnblockKeyboardAfterRelease(this);
    }
}

void CommandBar::NextCandidate() {
    m_Candidates.Next();
}

void CommandBar::PrevCandidate() {
    m_Candidates.Previous();
}

void CommandBar::NextPageOfCandidates() {
    m_Candidates.NextPage();
}

void CommandBar::PrevPageOfCandidates() {
    m_Candidates.PreviousPage();
}

void CommandBar::InvalidateCandidates() {
    m_Candidates.Clear();
}

void CommandBar::GenerateCandidatePages() {
    m_Candidates.BuildPages(m_WindowSize.x);
}

size_t CommandBar::OnCompletion(const char *lineStart, const char *lineEnd) {
    const char *wordStart = lineStart;
    const int wordLength = LastToken(wordStart, lineEnd);

    // Preserve raw end so arg parsing can keep empty-last-arg cases
    const char *rawLineEnd = lineEnd;
    StripLine(lineStart, lineEnd);

    if (m_Candidates.Empty()) {
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

    return m_Candidates.Size();
}

int CommandBar::OnTextEdit(ImGuiInputTextCallbackData *data) {
    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackCompletion: {
            OnCompletion(data->Buf, data->Buf + data->CursorPos);

            if (m_Candidates.Size() == 1) {
                ReplaceCurrentToken(data, m_Candidates[0].c_str());
            } else if (m_Candidates.Size() > 1) {
                int matchLen = 0;
                for (;;) {
                    int c = 0;
                    bool allCandidatesMatches = true;
                    for (size_t i = 0; i < m_Candidates.Size() && allCandidatesMatches; i++) {
                        const std::string &candidate = m_Candidates[i];
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
            if (!m_Candidates.Empty()) {
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
            if (const std::string *selected = m_Candidates.Selected()) {
                ReplaceCurrentToken(data, selected->c_str());
                InvalidateCandidates();
            }

            if (m_CursorPos != data->CursorPos) {
                InvalidateCandidates();
            }

            m_CursorPos = data->CursorPos;
        }
        break;
        case ImGuiInputTextFlags_CallbackEdit: {
            if (!m_Candidates.Empty()) {
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
