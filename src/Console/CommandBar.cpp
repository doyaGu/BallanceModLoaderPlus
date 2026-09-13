#include "Console/CommandBar.h"

#include "UI/BuiInternal.h"
#include "UI/InputSurfaceStyle.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>

#include <utf8.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "BML/ICommand.h"
#include "Console/CommandContext.h"
#include "Loader/ModContext.h"
#include "PathUtils.h"
#include "StringUtils.h"

namespace {
    constexpr wchar_t CommandHistoryFile[] = L"CommandBar.history";

    std::wstring BuildHistoryPath(const std::wstring &loaderDirectory) {
        if (loaderDirectory.empty())
            return {};

        return utils::CombinePathW(loaderDirectory, CommandHistoryFile);
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

    ImVec2 CenterText(const ImVec2 &minimum, const ImVec2 &maximum, const ImVec2 &size) {
        return {
            minimum.x + std::max(0.0f, maximum.x - minimum.x - size.x) * 0.5f,
            minimum.y + std::max(0.0f, maximum.y - minimum.y - size.y) * 0.5f,
        };
    }

    struct RailColors {
        ImU32 text;
        ImU32 disabled;
        ImU32 selection;
        ImU32 hover;
        ImU32 active;
    };

    bool DrawRailButton(const char *id, const char *label, const ImVec2 &labelSize,
                        const ImVec2 &minimum, const ImVec2 &maximum, const RailColors &colors) {
        ImGui::SetCursorScreenPos(minimum);
        const ImVec2 size(std::max(1.0f, maximum.x - minimum.x), std::max(1.0f, maximum.y - minimum.y));
        const bool pressed = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_MouseButtonLeft);
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            drawList->AddRectFilled(minimum, maximum, active ? colors.active : colors.hover,
                                    InputSurfaceStyle::Rounding);
        }
        drawList->AddText(CenterText(minimum, maximum, labelSize), hovered ? colors.text : colors.disabled, label);
        return pressed;
    }
}

bool CommandBar::CandidateState::Add(const std::string &candidate) {
    if (!utils::AppendUnique(m_Items, candidate))
        return false;

    InvalidateLayout();
    m_Selected = -1;
    m_HintsVisible = false;
    return true;
}

void CommandBar::CandidateState::Clear() {
    m_Items.clear();
    InvalidateLayout();
    m_Index = 0;
    m_Page = 0;
    m_Selected = -1;
    m_HintsVisible = false;
}

void CommandBar::CandidateState::BuildPages(float maxWidth) {
    InvalidateLayout();
    m_Context = ImGui::GetCurrentContext();
    m_Font = ImGui::GetFont();
    m_FontSize = ImGui::GetFontSize();
    ImFontBaked *baked = m_Font ? ImGui::GetFontBaked() : nullptr;
    m_BakedId = baked ? baked->BakedId : 0;
    m_MaxWidth = maxWidth;
    m_Page = 0;
    if (m_Items.empty()) {
        m_Index = 0;
        return;
    }

    m_ItemSizes.reserve(m_Items.size());
    for (const std::string &item : m_Items)
        m_ItemSizes.push_back(ImGui::CalcTextSize(item.c_str()));

    const std::string maximumPageStatus = std::to_string(m_Items.size()) + "/" + std::to_string(m_Items.size());
    const float pagerReserve = ImGui::CalcTextSize(maximumPageStatus.c_str()).x +
        (ImGui::CalcTextSize(">").x + InputSurfaceStyle::ChipPaddingX * 2.0f) * 2.0f +
        InputSurfaceStyle::ItemGap * 2.0f;
    const float availableWidth = std::max(
        1.0f, maxWidth - InputSurfaceStyle::PaddingX * 2.0f -
                  pagerReserve);
    float width = 0.0f;
    m_PageStarts.push_back(0);

    for (int i = 0; i < static_cast<int>(m_Items.size()); ++i) {
        const float itemWidth = m_ItemSizes[i].x + InputSurfaceStyle::ChipPaddingX * 2.0f;
        const float nextWidth = width + (width > 0.0f
            ? InputSurfaceStyle::ItemGap
            : 0.0f) + itemWidth;
        if (nextWidth > availableWidth && i > m_PageStarts.back()) {
            m_PageStarts.push_back(i);
            width = itemWidth;
        } else {
            width = nextWidth;
        }
    }

    m_Index = std::clamp(m_Index, 0, static_cast<int>(m_Items.size()) - 1);
    SyncPageFromIndex();
}

void CommandBar::CandidateState::SyncPageFromIndex() {
    for (int i = static_cast<int>(m_PageStarts.size()); i-- > 0;) {
        if (m_Index >= m_PageStarts[i]) {
            if (m_Page != i || m_Status.empty()) {
                m_Page = i;
                UpdateStatus();
            }
            return;
        }
    }
    if (m_Page != 0 || m_Status.empty()) {
        m_Page = 0;
        UpdateStatus();
    }
}

void CommandBar::CandidateState::UpdateStatus() {
    if (m_PageStarts.empty()) {
        m_Status.clear();
        m_StatusWidth = 0.0f;
        return;
    }

    m_Status = std::to_string(m_Page + 1) + "/" + std::to_string(m_PageStarts.size());
    m_StatusWidth = ImGui::CalcTextSize(m_Status.c_str()).x;
}

void CommandBar::CandidateState::InvalidateLayout() {
    m_ItemSizes.clear();
    m_PageStarts.clear();
    m_Status.clear();
    m_Context = nullptr;
    m_Font = nullptr;
    m_BakedId = 0;
    m_FontSize = 0.0f;
    m_MaxWidth = 0.0f;
    m_StatusWidth = 0.0f;
}

bool CommandBar::CandidateState::LayoutMatches(float maxWidth, ImGuiContext *context, ImFont *font,
                                                float fontSize, ImGuiID bakedId) const {
    return !m_PageStarts.empty() && m_Context == context && m_Font == font && m_FontSize == fontSize && m_BakedId == bakedId &&
           std::abs(m_MaxWidth - maxWidth) < 0.5f;
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
    const int pageCount = static_cast<int>(m_PageStarts.size());
    const int offset = m_Index - PageBegin();
    const int nextPage = (m_Page + 1) % pageCount;
    const int nextBegin = m_PageStarts[nextPage];
    const int nextEnd = nextPage + 1 < pageCount
        ? m_PageStarts[nextPage + 1]
        : static_cast<int>(m_Items.size());
    m_Page = nextPage;
    m_Index = std::min(nextBegin + offset, nextEnd - 1);
    UpdateStatus();
}

void CommandBar::CandidateState::PreviousPage() {
    if (m_Items.empty() || m_PageStarts.empty())
        return;
    const int pageCount = static_cast<int>(m_PageStarts.size());
    const int offset = m_Index - PageBegin();
    const int previousPage = m_Page > 0 ? m_Page - 1 : pageCount - 1;
    const int previousBegin = m_PageStarts[previousPage];
    const int previousEnd = previousPage + 1 < pageCount
        ? m_PageStarts[previousPage + 1]
        : static_cast<int>(m_Items.size());
    m_Page = previousPage;
    m_Index = std::min(previousBegin + offset, previousEnd - 1);
    UpdateStatus();
}

void CommandBar::CandidateState::ShowHints() {
    m_HintsVisible = !m_Items.empty() && !m_PageStarts.empty();
}

void CommandBar::CandidateState::SelectCurrent() {
    Select(m_Index);
}

void CommandBar::CandidateState::Select(int index) {
    if (index < 0 || index >= static_cast<int>(m_Items.size()))
        return;
    m_Index = index;
    SyncPageFromIndex();
    m_Selected = index;
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

std::size_t CommandBar::CandidateState::CommonPrefixLength() const {
    if (m_Items.empty())
        return 0;

    std::size_t prefixLength = m_Items.front().size();
    for (std::size_t itemIndex = 1; itemIndex < m_Items.size(); ++itemIndex) {
        const std::string &item = m_Items[itemIndex];
        prefixLength = std::min(prefixLength, item.size());
        for (std::size_t character = 0; character < prefixLength; ++character) {
            const int first = std::toupper(
                static_cast<unsigned char>(m_Items.front()[character]));
            const int current = std::toupper(
                static_cast<unsigned char>(item[character]));
            if (first != current) {
                prefixLength = character;
                break;
            }
        }
    }
    return prefixLength;
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

    RefreshTextMetrics();
    const ConsoleLayout::Stack &stack = m_FrameLayout;
    m_WindowPos = ImVec2(stack.commandBar.x, stack.commandBar.y);
    m_CommandHeight = stack.commandBar.height;
    if (!m_TextCompositionActive && m_Candidates.AreHintsVisible() &&
        !m_Candidates.LayoutMatches(stack.commandBar.width, m_Context, m_Font, m_FontSize, m_BakedId))
        m_Candidates.BuildPages(stack.commandBar.width);
    const bool showCompletionSurface = m_Candidates.AreHintsVisible() && !m_TextCompositionActive;
    const float hostHeight = showCompletionSurface
        ? stack.transientSurface.y + stack.transientSurface.height -
              stack.commandBar.y
        : stack.commandBar.height;
    m_WindowSize = ImVec2(stack.commandBar.width, hostHeight);
    m_TransientPos = ImVec2(stack.transientSurface.x, stack.transientSurface.y);
    m_TransientSize = ImVec2(stack.transientSurface.width, stack.transientSurface.height);
    ImGui::SetNextWindowPos(m_WindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_WindowSize, ImGuiCond_Always);

    if (!m_VisiblePrev)
        ImGui::SetNextWindowFocus();
}

void CommandBar::OnDraw() {
    if (m_TextCompositionActive && !m_Candidates.Empty())
        InvalidateCandidates();

    const bool completionVisible = !m_TextCompositionActive && m_Candidates.AreHintsVisible();
    const ImVec2 rowMin = m_WindowPos;
    const ImVec2 rowMax(m_WindowPos.x + m_WindowSize.x,
                        m_WindowPos.y + m_CommandHeight);
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    InputSurfaceStyle::DrawPanel(
        drawList, rowMin, rowMax, InputSurfaceStyle::PanelBackground());

    constexpr const char *Prompt = ">";
    const ImVec2 promptPos(
        rowMin.x + InputSurfaceStyle::PaddingX,
        rowMin.y + std::max(0.0f, m_CommandHeight - m_PromptSize.y) * 0.5f);
    drawList->AddText(promptPos, ImGui::GetColorU32(InputSurfaceStyle::HoverColor()), Prompt);

    const float inputX = promptPos.x + m_PromptSize.x +
                         InputSurfaceStyle::ItemGap;
    const float inputWidth = std::max(
        1.0f, rowMax.x - InputSurfaceStyle::PaddingX - inputX);
    ImGui::SetCursorScreenPos(ImVec2(inputX, rowMin.y));
    ImGui::SetNextItemWidth(inputWidth);
    if (!m_VisiblePrev || m_FocusInputNextFrame)
        ImGui::SetKeyboardFocusHere();
    m_FocusInputNextFrame = false;

    bool acceptCompletion = false;
    bool dismissCompletion = false;
    if (completionVisible) {
        const ImGuiID completionOwner = ImGui::GetCurrentWindow()->GetID("##completion-controls");
        constexpr ImGuiInputFlags OwnershipFlags = ImGuiInputFlags_LockThisFrame;
        ImGui::SetKeyOwner(ImGuiKey_Enter, completionOwner, OwnershipFlags);
        ImGui::SetKeyOwner(ImGuiKey_KeypadEnter, completionOwner, OwnershipFlags);
        ImGui::SetKeyOwner(ImGuiKey_Escape, completionOwner, OwnershipFlags);
        acceptCompletion = ImGui::IsKeyPressed(ImGuiKey_Enter, ImGuiInputFlags_None, completionOwner) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, ImGuiInputFlags_None, completionOwner);
        dismissCompletion = ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, completionOwner);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, InputSurfaceStyle::PaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    const ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, transparent);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, transparent);
    ImGui::PushStyleColor(ImGuiCol_Border, transparent);

    ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EscapeClearsAll |
                                        ImGuiInputTextFlags_CallbackAlways |
                                        ImGuiInputTextFlags_CallbackEdit;
    if (!m_TextCompositionActive) {
        inputTextFlags |= ImGuiInputTextFlags_CallbackCompletion;
        if (!completionVisible) {
            inputTextFlags |= ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
        }
    }
    const bool submitted = ImGui::InputTextWithHint("##CmdBar", "Enter a command", &m_Buffer, inputTextFlags,
                                                    &TextEditCallback, this);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    m_InputActive = ImGui::IsItemActive();

    if (submitted) {
        if (!m_Buffer.empty()) {
            const std::string commandLine = m_Buffer;
            RecordHistoryEntry(commandLine);
            BML_GetModContext()->ExecuteCommand(commandLine.c_str());
        }
        ToggleCommandBar(false);
    }

    if (!submitted && !m_TextCompositionActive && m_Candidates.AreHintsVisible()) {
        DrawCompletionSurface(acceptCompletion, dismissCompletion);
    } else if (!m_TextCompositionActive) {
        if (!m_Candidates.Empty()) {
            m_Candidates.ShowHints();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ToggleCommandBar(false);
    }

    m_VisiblePrev = true;
}

void CommandBar::DrawCompletionSurface(bool acceptSelection, bool dismissCompletion) {
    ImGui::SetCursorScreenPos(m_TransientPos);
    constexpr ImGuiWindowFlags ChildFlags = ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoScrollWithMouse |
                                            ImGuiWindowFlags_NoNav;
    if (ImGui::BeginChild("##CmdHints", m_TransientSize, ImGuiChildFlags_None, ChildFlags)) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const RailColors colors{
            ImGui::GetColorU32(ImGuiCol_Text),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            ImGui::GetColorU32(InputSurfaceStyle::SelectionColor()),
            ImGui::GetColorU32(InputSurfaceStyle::HoverColor()),
            ImGui::GetColorU32(InputSurfaceStyle::ActiveColor()),
        };
        const ImVec2 railMin = m_TransientPos;
        const ImVec2 railMax(m_TransientPos.x + m_TransientSize.x,
                             m_TransientPos.y + m_TransientSize.y);
        InputSurfaceStyle::DrawPanel(drawList, railMin, railMax, InputSurfaceStyle::PanelBackground());

        const float lineHeight = m_TextLineHeight;
        const float contentY = railMin.y +
            std::max(0.0f, m_TransientSize.y - lineHeight) * 0.5f;
        float contentX = railMin.x + InputSurfaceStyle::PaddingX;
        float contentMaxX = railMax.x - InputSurfaceStyle::PaddingX;

        const int pageCount = std::max(1, m_Candidates.PageCount());
        const std::string &status = m_Candidates.Status();
        const float statusWidth = m_Candidates.StatusWidth();
        const float buttonWidth = lineHeight + InputSurfaceStyle::ChipPaddingX;
        const float controlsWidth = statusWidth +
            (pageCount > 1
                 ? buttonWidth * 2.0f + InputSurfaceStyle::ItemGap * 2.0f
                 : 0.0f);
        const float controlsX = std::max(contentX, contentMaxX - controlsWidth);
        contentMaxX = std::max(contentX, controlsX - InputSurfaceStyle::ItemGap);

        const int begin = m_Candidates.PageBegin();
        const int end = m_Candidates.PageEnd();
        for (int i = begin; i < end && contentX < contentMaxX; ++i) {
            const char *label = m_Candidates[i].c_str();
            const ImVec2 &labelSize = m_Candidates.ItemSize(i);
            const float desiredWidth = labelSize.x + InputSurfaceStyle::ChipPaddingX * 2.0f;
            const float width = std::min(desiredWidth, contentMaxX - contentX);
            if (width <= 1.0f)
                break;

            const ImVec2 chipMin(contentX, railMin.y + 2.0f);
            const ImVec2 chipMax(contentX + width, railMax.y - 2.0f);
            ImGui::PushID(i);
            ImGui::SetCursorScreenPos(chipMin);
            const bool pressed = ImGui::InvisibleButton(
                "##candidate",
                ImVec2(chipMax.x - chipMin.x, chipMax.y - chipMin.y),
                ImGuiButtonFlags_MouseButtonLeft);
            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();
            if (hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::PopID();

            const bool selected = i == m_Candidates.CurrentIndex();
            if (selected || hovered) {
                const ImU32 color = hovered ? (active ? colors.active : colors.hover) : colors.selection;
                drawList->AddRectFilled(chipMin, chipMax, color, InputSurfaceStyle::Rounding);
            }
            const ImVec2 textMin(chipMin.x + InputSurfaceStyle::ChipPaddingX, contentY);
            const ImVec2 textMax(std::max(textMin.x, chipMax.x - InputSurfaceStyle::ChipPaddingX),
                                 contentY + lineHeight);
            drawList->PushClipRect(chipMin, chipMax, true);
            ImGui::RenderTextEllipsis(drawList, textMin, textMax, textMax.x,
                                      label, nullptr, &labelSize);
            drawList->PopClipRect();

            if (pressed) {
                m_Candidates.Select(i);
                m_FocusInputNextFrame = true;
            }
            contentX += width + InputSurfaceStyle::ItemGap;
        }

        float controlX = controlsX;
        if (pageCount > 1) {
            const ImVec2 previousMin(controlX, railMin.y + 2.0f);
            const ImVec2 previousMax(controlX + buttonWidth,
                                     railMax.y - 2.0f);
            if (DrawRailButton("##previous-page", "<", m_PreviousPageLabelSize, previousMin,
                               previousMax, colors)) {
                PrevPageOfCandidates();
                m_FocusInputNextFrame = true;
            }
            controlX = previousMax.x + InputSurfaceStyle::ItemGap;
        }

        drawList->AddText(
            ImVec2(controlX, contentY), colors.disabled, status.c_str());
        controlX += statusWidth;

        if (pageCount > 1) {
            controlX += InputSurfaceStyle::ItemGap;
            const ImVec2 nextMin(controlX, railMin.y + 2.0f);
            const ImVec2 nextMax(controlX + buttonWidth,
                                 railMax.y - 2.0f);
            if (DrawRailButton("##next-page", ">", m_NextPageLabelSize, nextMin, nextMax, colors)) {
                NextPageOfCandidates();
                m_FocusInputNextFrame = true;
            }
        }

        const ImGuiIO &io = ImGui::GetIO();
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
            if (io.MouseWheel > 0.0f)
                PrevPageOfCandidates();
            else
                NextPageOfCandidates();
            m_FocusInputNextFrame = true;
        }
    }
    ImGui::EndChild();

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
        ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
        PrevPageOfCandidates();
    } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
               ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        NextPageOfCandidates();
    }

    if (acceptSelection) {
        m_Candidates.SelectCurrent();
        m_FocusInputNextFrame = true;
    }

    if (dismissCompletion) {
        InvalidateCandidates();
        m_FocusInputNextFrame = true;
    }
}

void CommandBar::OnPostEnd() {
    ImGui::PopStyleVar(3);
}

void CommandBar::OnShow() {
    m_VisiblePrev = false;
}

void CommandBar::OnHide() {
    m_InputActive = false;
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

void CommandBar::CollectArgumentCandidates(const char *wordStart, int wordLength,
                                           const char *cmdStart, const char *lineEnd) {
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

    const bool hasSpaceAfter = tokenEnd < textEnd && std::isspace(static_cast<unsigned char>(*tokenEnd));
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

void CommandBar::RefreshTextMetrics() {
    ImGuiContext *context = ImGui::GetCurrentContext();
    ImFont *font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    ImFontBaked *baked = font ? ImGui::GetFontBaked() : nullptr;
    const ImGuiID bakedId = baked ? baked->BakedId : 0;
    if (m_Context == context && m_Font == font && m_FontSize == fontSize && m_BakedId == bakedId)
        return;

    m_Context = context;
    m_Font = font;
    m_BakedId = bakedId;
    m_FontSize = fontSize;
    m_TextLineHeight = ImGui::GetTextLineHeight();
    m_PromptSize = ImGui::CalcTextSize(">");
    m_PreviousPageLabelSize = ImGui::CalcTextSize("<");
    m_NextPageLabelSize = ImGui::CalcTextSize(">");
}

std::size_t CommandBar::OnCompletion(const char *lineStart, const char *lineEnd) {
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
        if (ImGui::GetIO().KeyShift)
            PrevCandidate();
        else
            NextCandidate();
    }

    return m_Candidates.Size();
}

int CommandBar::OnTextEdit(ImGuiInputTextCallbackData *data) {
    if (m_TextCompositionActive) {
        if (!m_Candidates.Empty())
            InvalidateCandidates();
        m_CursorPos = data->CursorPos;
        return 0;
    }

    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackCompletion: {
            OnCompletion(data->Buf, data->Buf + data->CursorPos);

            if (m_Candidates.Size() == 1) {
                ReplaceCurrentToken(data, m_Candidates[0].c_str());
            } else if (m_Candidates.Size() > 1) {
                const std::size_t commonLength = m_Candidates.CommonPrefixLength();
                if (commonLength > 0)
                    ReplaceCurrentToken(data, m_Candidates[0].c_str(), static_cast<int>(commonLength));
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
                const std::string &historyStr = m_HistoryIndex >= 0 ? m_History[m_HistoryIndex] : "";
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
