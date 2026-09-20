#include "Console/CommandBar.h"

#include "UI/BuiInternal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <utf8.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "BML/ICommand.h"
#include "Console/CommandCompletion.h"
#include "Console/CommandContext.h"
#include "Console/CommandInput.h"
#include "Console/Shell/ShellEnvironment.h"
#include "Console/Shell/ShellParser.h"
#include "Console/Shell/ShellTypes.h"
#include "Loader/ModContext.h"
#include "StringUtils.h"

namespace {
    constexpr float PaddingX = 8.0f;
    constexpr float PaddingY = 5.0f;
    constexpr float ItemGap = 6.0f;
    constexpr float ChipPaddingX = 6.0f;
    constexpr int MaxContinuationRows = 7;
    constexpr ImVec4 PanelBackgroundColor = {0.0f, 0.0f, 0.0f, 155.0f / 255.0f};
    constexpr ImVec4 TextColor = {1.0f, 1.0f, 1.0f, 1.0f};
    constexpr ImVec4 MutedTextColor = {1.0f, 1.0f, 1.0f, 0.65f};
    constexpr ImVec4 GhostTextColor = {1.0f, 1.0f, 1.0f, 0.40f};
    constexpr ImVec4 InvertedTextColor = {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr ImVec4 SelectedBackgroundColor = {1.0f, 1.0f, 1.0f, 1.0f};
    constexpr ImVec4 HoveredBackgroundColor = {1.0f, 1.0f, 1.0f, 0.8f};

    // Syntax colours of the input line.
    constexpr ImVec4 CommandValidColor = {0.60f, 0.90f, 0.60f, 1.0f};
    constexpr ImVec4 CommandInvalidColor = {1.00f, 0.50f, 0.50f, 1.0f};
    constexpr ImVec4 StringColor = {0.98f, 0.84f, 0.55f, 1.0f};
    constexpr ImVec4 VariableColor = {0.60f, 0.85f, 1.00f, 1.0f};
    constexpr ImVec4 OperatorColor = {0.85f, 0.75f, 1.00f, 1.0f};
    constexpr ImVec4 CommentColor = {1.0f, 1.0f, 1.0f, 0.5f};
    constexpr ImVec4 ErrorColor = {1.00f, 0.35f, 0.35f, 1.0f};

    ImU32 ColorForSpan(BML::Shell::HighlightSpan::Kind kind) {
        using Kind = BML::Shell::HighlightSpan::Kind;
        switch (kind) {
            case Kind::CommandValid: return ImGui::GetColorU32(CommandValidColor);
            case Kind::CommandInvalid: return ImGui::GetColorU32(CommandInvalidColor);
            case Kind::String: return ImGui::GetColorU32(StringColor);
            case Kind::Variable: return ImGui::GetColorU32(VariableColor);
            case Kind::Operator: return ImGui::GetColorU32(OperatorColor);
            case Kind::Comment: return ImGui::GetColorU32(CommentColor);
            case Kind::Error: return ImGui::GetColorU32(ErrorColor);
            case Kind::Plain: break;
        }
        return ImGui::GetColorU32(TextColor);
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
        ImU32 mutedText;
        ImU32 invertedText;
        ImU32 selectedBackground;
        ImU32 hoveredBackground;
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
            drawList->AddRectFilled(minimum, maximum,
                                    active ? colors.selectedBackground : colors.hoveredBackground);
        }
        drawList->AddText(CenterText(minimum, maximum, labelSize),
                          hovered ? colors.invertedText : colors.mutedText, label);
        return pressed;
    }

    // Word boundaries for Ctrl+W and Alt+B/F: runs of non-whitespace bytes,
    // which is UTF-8 safe because every delimiter tested is ASCII.
    int WordStartBefore(const char *text, int pos) {
        int i = pos;
        while (i > 0 && std::isspace(static_cast<unsigned char>(text[i - 1])) != 0)
            --i;
        while (i > 0 && std::isspace(static_cast<unsigned char>(text[i - 1])) == 0)
            --i;
        return i;
    }

    int WordEndAfter(const char *text, int pos, int length) {
        int i = pos;
        while (i < length && std::isspace(static_cast<unsigned char>(text[i])) != 0)
            ++i;
        while (i < length && std::isspace(static_cast<unsigned char>(text[i])) == 0)
            ++i;
        return i;
    }

    bool NamesSomethingRunnable(std::string_view name) {
        if (name.empty())
            return false;
        ModContext *context = BML_GetModContext();
        if (!context)
            return false;
        const std::string text(name);
        if (context->FindCommand(text.c_str()))
            return true;
        return context->GetShellEnvironment().HasAlias(name);
    }

    BML::Shell::CompletionProviders MakeProviders() {
        BML::Shell::CompletionProviders providers;
        providers.commandNames = [] {
            std::vector<std::string> names;
            for (const auto &command : BML_GetModContext()->GetCommandSnapshot()) {
                const std::string name = NormalizeCandidateEncoding(command.Name);
                if (!name.empty())
                    names.push_back(name);
                const std::string alias = NormalizeCandidateEncoding(command.Alias);
                if (!alias.empty())
                    names.push_back(alias);
            }
            return names;
        };
        providers.aliasNames = [] { return BML_GetModContext()->GetShellEnvironment().AliasNames(); };
        providers.variableNames = [] { return BML_GetModContext()->GetShellEnvironment().VariableNames(); };
        providers.argumentCandidates = [](const std::vector<std::string> &args) {
            std::vector<std::string> candidates;
            if (args.empty() || args[0].empty())
                return candidates;
            for (const std::string &raw : BML_GetModContext()->CompleteCommand(args[0].c_str(), args)) {
                const std::string candidate = NormalizeCandidateEncoding(raw);
                if (!candidate.empty())
                    candidates.push_back(candidate);
            }
            return candidates;
        };
        return providers;
    }
}

float CommandBar::MeasureRowHeight() {
    return ImGui::GetTextLineHeight() + PaddingY * 2.0f;
}

// CandidateState

bool CommandBar::CandidateState::Add(const std::string &candidate) {
    if (!utils::AppendUnique(m_Items, candidate))
        return false;

    m_DisplayItems.push_back(CommandInput::SingleLinePreview(candidate));
    InvalidateLayout();
    m_Selected = -1;
    m_HintsVisible = false;
    return true;
}

void CommandBar::CandidateState::Clear() {
    m_Items.clear();
    m_DisplayItems.clear();
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
    for (const std::string &item : m_DisplayItems)
        m_ItemSizes.push_back(ImGui::CalcTextSize(item.c_str()));

    const std::string maximumPageStatus = std::to_string(m_Items.size()) + "/" + std::to_string(m_Items.size());
    const float pagerReserve = ImGui::CalcTextSize(maximumPageStatus.c_str()).x +
        (ImGui::CalcTextSize(">").x + ChipPaddingX * 2.0f) * 2.0f +
        ItemGap * 2.0f;
    const float availableWidth = std::max(1.0f, maxWidth - PaddingX * 2.0f - pagerReserve);
    float width = 0.0f;
    m_PageStarts.push_back(0);

    for (int i = 0; i < static_cast<int>(m_Items.size()); ++i) {
        const float itemWidth = m_ItemSizes[i].x + ChipPaddingX * 2.0f;
        const float nextWidth = width + (width > 0.0f ? ItemGap : 0.0f) + itemWidth;
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
    return !m_PageStarts.empty() && m_Context == context && m_Font == font && m_FontSize == fontSize &&
           m_BakedId == bakedId && std::abs(m_MaxWidth - maxWidth) < 0.5f;
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
    const int nextEnd = nextPage + 1 < pageCount ? m_PageStarts[nextPage + 1] : static_cast<int>(m_Items.size());
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
    return m_Selected >= 0 && m_Selected < static_cast<int>(m_Items.size()) ? &m_Items[m_Selected] : nullptr;
}

std::size_t CommandBar::CandidateState::CommonPrefixLength() const {
    return CommandCompletion::CommonPrefixLength(m_Items);
}

// CommandBar

CommandBar::CommandBar() : Window("CommandBar") {
    m_Buffer.reserve(65535);
    Hide();
}

CommandBar::~CommandBar() = default;

void CommandBar::SetHistory(BML::Shell::History *history) {
    m_History = history;
    m_Navigator.Reset();
    m_Suggestion.clear();
}

int CommandBar::RowCount() const {
    return 1 + std::min(static_cast<int>(m_PendingLines.size()), MaxContinuationRows);
}

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
    const bool showTransient = !m_TextCompositionActive && (m_Candidates.AreHintsVisible() || m_SearchActive);
    const float hostHeight = showTransient
        ? stack.transientSurface.y + stack.transientSurface.height - stack.commandBar.y
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
    // The game also receives clicks outside ImGui windows. Do not leave the
    // command session holding its keyboard block after the player returns to
    // a native menu, while preserving clicks on the transient child window.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        ToggleCommandBar(false);
        return;
    }

    if (m_TextCompositionActive && !m_Candidates.Empty())
        InvalidateCandidates();

    const bool completionVisible = !m_TextCompositionActive && m_Candidates.AreHintsVisible();
    const bool searchVisible = !m_TextCompositionActive && m_SearchActive;
    const float rowHeight = m_TextLineHeight + PaddingY * 2.0f;
    const ImVec2 areaMin = m_WindowPos;
    const ImVec2 areaMax(m_WindowPos.x + m_WindowSize.x, m_WindowPos.y + m_CommandHeight);
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(areaMin, areaMax, ImGui::GetColorU32(PanelBackgroundColor));

    DrawContinuationRows(drawList, areaMin, rowHeight);

    const float inputTop = std::max(areaMin.y, areaMax.y - rowHeight);
    const char *prompt = m_PendingLines.empty() ? ">" : "...";
    const ImVec2 promptSize = m_PendingLines.empty() ? m_PromptSize : ImGui::CalcTextSize(prompt);
    const ImVec2 promptPos(areaMin.x + PaddingX, inputTop + std::max(0.0f, rowHeight - promptSize.y) * 0.5f);
    drawList->AddText(promptPos, ImGui::GetColorU32(m_PendingLines.empty() ? TextColor : MutedTextColor), prompt);

    const float inputX = promptPos.x + promptSize.x + ItemGap;
    const float inputWidth = std::max(1.0f, areaMax.x - PaddingX - inputX);
    ImGui::SetCursorScreenPos(ImVec2(inputX, inputTop));
    ImGui::SetNextItemWidth(inputWidth);
    if (!m_VisiblePrev || m_FocusInputNextFrame)
        ImGui::SetKeyboardFocusHere();
    m_FocusInputNextFrame = false;

    // InputText's CallbackAlways can run after it has already consumed the
    // Ctrl+R key event. Latch the chord while the open command bar owns
    // keyboard input, then transition search state from inside the callback
    // where its buffer can be replaced safely.
    const bool reverseSearchPressed = ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false);
    if (!m_TextCompositionActive && reverseSearchPressed) {
        m_SearchRequested = true;
    }

    // While the completion rail or the search prompt is up, Enter and Escape
    // belong to them rather than to the text field.
    bool acceptTransient = false;
    bool dismissTransient = false;
    const bool transientOwnsKeys = completionVisible || searchVisible;
    if (transientOwnsKeys) {
        const ImGuiID owner = ImGui::GetCurrentWindow()->GetID("##transient-controls");
        constexpr ImGuiInputFlags OwnershipFlags = ImGuiInputFlags_LockThisFrame;
        ImGui::SetKeyOwner(ImGuiKey_Enter, owner, OwnershipFlags);
        ImGui::SetKeyOwner(ImGuiKey_KeypadEnter, owner, OwnershipFlags);
        ImGui::SetKeyOwner(ImGuiKey_Escape, owner, OwnershipFlags);
        acceptTransient = ImGui::IsKeyPressed(ImGuiKey_Enter, ImGuiInputFlags_None, owner) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, ImGuiInputFlags_None, owner);
        dismissTransient = ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, owner);
    }

    const bool overlayText = !m_TextCompositionActive;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, PaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    const ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, transparent);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, transparent);
    ImGui::PushStyleColor(ImGuiCol_Border, transparent);
    // The field's own text is hidden and redrawn coloured by DrawInputOverlay.
    ImGui::PushStyleColor(ImGuiCol_Text, overlayText ? transparent : TextColor);
    ImGui::PushStyleColor(ImGuiCol_InputTextCursor, TextColor);

    // Keep EnterReturnsTrue stable while transient rails are visible. Without
    // it, InputText returns true for an ordinary text edit and the first query
    // character would be mistaken for a submitted command. Key ownership above
    // prevents Enter from reaching the field while a rail is active.
    ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_CallbackAlways |
                                         ImGuiInputTextFlags_CallbackEdit |
                                         ImGuiInputTextFlags_EnterReturnsTrue;
    if (!transientOwnsKeys)
        inputTextFlags |= ImGuiInputTextFlags_EscapeClearsAll;
    if (!m_TextCompositionActive && !transientOwnsKeys)
        inputTextFlags |= ImGuiInputTextFlags_CallbackHistory;
    const char *hint = m_SearchActive ? "Search history" : (m_PendingLines.empty() ? "Enter a command" : "");
    const bool submitted = ImGui::InputTextWithHint("##CmdBar", hint, &m_Buffer, inputTextFlags,
                                                    &TextEditCallback, this);
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(2);
    m_InputActive = ImGui::IsItemActive();

    if (overlayText) {
        if (!CommandInput::Equals(m_PendingLines, m_Buffer, m_HighlightSource))
            RefreshHighlight();
        DrawInputOverlay(drawList, itemMin, itemMax, !completionVisible && !m_SearchActive);
    }

    // Enter confirms the native IME composition first. Even if a backend also
    // exposes that key to ImGui, it must not execute a partially composed line.
    if (submitted && !m_TextCompositionActive) {
        SubmitLine();
        m_VisiblePrev = true;
        return;
    }

    if (completionVisible) {
        if (acceptTransient) {
            m_Candidates.SelectCurrent();
            m_FocusInputNextFrame = true;
        } else if (dismissTransient) {
            InvalidateCandidates();
            m_FocusInputNextFrame = true;
        }
        DrawCompletionSurface();
    } else if (searchVisible) {
        if (acceptTransient)
            m_SearchAcceptRequested = true;
        else if (dismissTransient)
            m_SearchCancelRequested = true;
        if (m_SearchAcceptRequested || m_SearchCancelRequested)
            m_FocusInputNextFrame = true;
        DrawSearchSurface();
    } else if (!m_TextCompositionActive) {
        if (!m_Candidates.Empty())
            m_Candidates.ShowHints();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            if (!m_PendingLines.empty()) {
                ClearPendingLines();
                m_Buffer.clear();
                m_CursorPos = 0;
                m_Navigator.Reset();
                m_NavigatedText.clear();
                m_Suggestion.clear();
                InvalidateCandidates();
                m_HighlightSource.clear();
                m_FocusInputNextFrame = true;
            } else {
                ToggleCommandBar(false);
            }
        }
    }

    m_VisiblePrev = true;
}

void CommandBar::DrawContinuationRows(ImDrawList *drawList, const ImVec2 &areaMin, float rowHeight) {
    if (m_PendingLines.empty())
        return;
    const int pendingCount = static_cast<int>(m_PendingLines.size());
    const bool elided = pendingCount > MaxContinuationRows;
    const int actualRows = elided ? MaxContinuationRows - 1 : pendingCount;
    const int first = pendingCount - actualRows;
    const ImU32 promptColor = ImGui::GetColorU32(MutedTextColor);
    const ImU32 textColor = ImGui::GetColorU32(TextColor);
    const float right = areaMin.x + m_WindowSize.x - PaddingX;
    int displayRow = 0;
    if (elided) {
        const float top = areaMin.y;
        const float textY = top + std::max(0.0f, rowHeight - m_TextLineHeight) * 0.5f;
        drawList->AddText(ImVec2(areaMin.x + PaddingX, textY), promptColor, "...");
        const float textX = areaMin.x + PaddingX + ImGui::CalcTextSize("...").x + ItemGap;
        const std::string label = "[" + std::to_string(first) + " earlier rows]";
        drawList->PushClipRect(ImVec2(textX, top), ImVec2(right, top + rowHeight), true);
        drawList->AddText(ImVec2(textX, textY), promptColor, label.c_str());
        drawList->PopClipRect();
        ++displayRow;
    }
    for (int i = 0; i < actualRows; ++i, ++displayRow) {
        const int rowIndex = first + i;
        const float top = areaMin.y + static_cast<float>(displayRow) * rowHeight;
        const float textY = top + std::max(0.0f, rowHeight - m_TextLineHeight) * 0.5f;
        const char *prompt = rowIndex == 0 ? ">" : "...";
        const ImVec2 promptSize = ImGui::CalcTextSize(prompt);
        drawList->AddText(ImVec2(areaMin.x + PaddingX, textY), promptColor, prompt);
        const float textX = areaMin.x + PaddingX + promptSize.x + ItemGap;
        drawList->PushClipRect(ImVec2(textX, top), ImVec2(right, top + rowHeight), true);
        drawList->AddText(ImVec2(textX, textY), textColor,
                          m_PendingLines[static_cast<std::size_t>(rowIndex)].c_str());
        drawList->PopClipRect();
    }
}

void CommandBar::RefreshHighlight() {
    m_HighlightSource = JoinPending(m_Buffer);
    m_HighlightSpans.clear();
    if (m_SearchActive) {
        return;
    }

    const std::size_t currentOffset = CurrentRowOffset();
    for (const BML::Shell::HighlightSpan &span :
         BML::Shell::Highlight(m_HighlightSource, NamesSomethingRunnable)) {
        if (span.end <= currentOffset)
            continue;
        BML::Shell::HighlightSpan visible = span;
        visible.begin = std::max(visible.begin, currentOffset) - currentOffset;
        visible.end = std::min(visible.end, m_HighlightSource.size()) - currentOffset;
        if (visible.end > visible.begin)
            m_HighlightSpans.push_back(visible);
    }
}

void CommandBar::DrawInputOverlay(ImDrawList *drawList, const ImVec2 &itemMin, const ImVec2 &itemMax,
                                  bool showSuggestion) {
    if (m_Buffer.empty() && (!showSuggestion || m_Suggestion.empty()))
        return;

    float scrollX = 0.0f;
    if (const ImGuiInputTextState *state = ImGui::GetInputTextState(ImGui::GetItemID()))
        scrollX = state->Scroll.x;

    ImFont *font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    ImVec2 pos(itemMin.x - scrollX, itemMin.y + PaddingY);
    drawList->PushClipRect(itemMin, itemMax, true);

    const char *base = m_Buffer.c_str();
    if (m_HighlightSpans.empty()) {
        drawList->AddText(font, fontSize, pos, ImGui::GetColorU32(TextColor), base, base + m_Buffer.size());
        pos.x += ImGui::CalcTextSize(base, base + m_Buffer.size(), false).x;
    } else {
        for (const BML::Shell::HighlightSpan &span : m_HighlightSpans) {
            const char *begin = base + span.begin;
            const char *end = base + span.end;
            if (end <= begin)
                continue;
            drawList->AddText(font, fontSize, pos, ColorForSpan(span.kind), begin, end);
            pos.x += ImGui::CalcTextSize(begin, end, false).x;
        }
    }

    if (showSuggestion && !m_Suggestion.empty() && m_CursorPos == static_cast<int>(m_Buffer.size())) {
        const std::string preview = CommandInput::SingleLinePreview(m_Suggestion);
        drawList->AddText(font, fontSize, pos, ImGui::GetColorU32(GhostTextColor), preview.c_str());
    }

    drawList->PopClipRect();
}

void CommandBar::DrawSearchSurface() {
    ImGui::SetCursorScreenPos(m_TransientPos);
    constexpr ImGuiWindowFlags ChildFlags = ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoScrollWithMouse |
                                            ImGuiWindowFlags_NoNav;
    if (ImGui::BeginChild("##CmdSearch", m_TransientSize, ImGuiChildFlags_None, ChildFlags)) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImVec2 railMin = m_TransientPos;
        const ImVec2 railMax(m_TransientPos.x + m_TransientSize.x, m_TransientPos.y + m_TransientSize.y);
        drawList->AddRectFilled(railMin, railMax, ImGui::GetColorU32(PanelBackgroundColor));

        const float contentY = railMin.y + std::max(0.0f, m_TransientSize.y - m_TextLineHeight) * 0.5f;
        std::string label = m_SearchFailed ? "(failing reverse-i-search)`" : "(reverse-i-search)`";
        label += m_SearchQuery;
        label += "': ";
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        const std::string matchPreview = CommandInput::SingleLinePreview(m_SearchMatch);
        drawList->PushClipRect(railMin, railMax, true);
        drawList->AddText(ImVec2(railMin.x + PaddingX, contentY), ImGui::GetColorU32(MutedTextColor), label.c_str());
        drawList->AddText(ImVec2(railMin.x + PaddingX + labelSize.x, contentY), ImGui::GetColorU32(TextColor),
                          matchPreview.c_str());
        drawList->PopClipRect();
    }
    ImGui::EndChild();
}

void CommandBar::DrawCompletionSurface() {
    ImGui::SetCursorScreenPos(m_TransientPos);
    constexpr ImGuiWindowFlags ChildFlags = ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoScrollWithMouse |
                                            ImGuiWindowFlags_NoNav;
    if (ImGui::BeginChild("##CmdHints", m_TransientSize, ImGuiChildFlags_None, ChildFlags)) {
        ImGui::PushStyleColor(ImGuiCol_Text, TextColor);
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const RailColors colors{
            ImGui::GetColorU32(MutedTextColor),
            ImGui::GetColorU32(InvertedTextColor),
            ImGui::GetColorU32(SelectedBackgroundColor),
            ImGui::GetColorU32(HoveredBackgroundColor),
        };
        const ImVec2 railMin = m_TransientPos;
        const ImVec2 railMax(m_TransientPos.x + m_TransientSize.x, m_TransientPos.y + m_TransientSize.y);
        drawList->AddRectFilled(railMin, railMax, ImGui::GetColorU32(PanelBackgroundColor));

        const float lineHeight = m_TextLineHeight;
        const float contentY = railMin.y + std::max(0.0f, m_TransientSize.y - lineHeight) * 0.5f;
        float contentX = railMin.x + PaddingX;
        float contentMaxX = railMax.x - PaddingX;

        const int pageCount = std::max(1, m_Candidates.PageCount());
        const std::string &status = m_Candidates.Status();
        const float statusWidth = m_Candidates.StatusWidth();
        const float buttonWidth = lineHeight + ChipPaddingX;
        const float controlsWidth = statusWidth + (pageCount > 1 ? buttonWidth * 2.0f + ItemGap * 2.0f : 0.0f);
        const float controlsX = std::max(contentX, contentMaxX - controlsWidth);
        contentMaxX = std::max(contentX, controlsX - ItemGap);

        const int begin = m_Candidates.PageBegin();
        const int end = m_Candidates.PageEnd();
        for (int i = begin; i < end && contentX < contentMaxX; ++i) {
            const char *label = m_Candidates.DisplayItem(static_cast<std::size_t>(i)).c_str();
            const ImVec2 &labelSize = m_Candidates.ItemSize(i);
            const float desiredWidth = labelSize.x + ChipPaddingX * 2.0f;
            const float width = std::min(desiredWidth, contentMaxX - contentX);
            if (width <= 1.0f)
                break;

            const ImVec2 chipMin(contentX, railMin.y + 2.0f);
            const ImVec2 chipMax(contentX + width, railMax.y - 2.0f);
            const bool selected = i == m_Candidates.CurrentIndex();
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

            if (selected || hovered) {
                const ImU32 color = selected || active ? colors.selectedBackground : colors.hoveredBackground;
                drawList->AddRectFilled(chipMin, chipMax, color);
            }
            const ImVec2 textMin(chipMin.x + ChipPaddingX, contentY);
            const ImVec2 textMax(std::max(textMin.x, chipMax.x - ChipPaddingX), contentY + lineHeight);
            drawList->PushClipRect(chipMin, chipMax, true);
            if (selected || hovered)
                ImGui::PushStyleColor(ImGuiCol_Text, InvertedTextColor);
            ImGui::RenderTextEllipsis(drawList, textMin, textMax, textMax.x, label, nullptr, &labelSize);
            if (selected || hovered)
                ImGui::PopStyleColor();
            drawList->PopClipRect();

            if (pressed) {
                m_Candidates.Select(i);
                m_FocusInputNextFrame = true;
            }
            contentX += width + ItemGap;
        }

        float controlX = controlsX;
        if (pageCount > 1) {
            const ImVec2 previousMin(controlX, railMin.y + 2.0f);
            const ImVec2 previousMax(controlX + buttonWidth, railMax.y - 2.0f);
            if (DrawRailButton("##previous-page", "<", m_PreviousPageLabelSize, previousMin, previousMax, colors)) {
                PrevPageOfCandidates();
                m_FocusInputNextFrame = true;
            }
            controlX = previousMax.x + ItemGap;
        }

        drawList->AddText(ImVec2(controlX, contentY), colors.mutedText, status.c_str());
        controlX += statusWidth;

        if (pageCount > 1) {
            controlX += ItemGap;
            const ImVec2 nextMin(controlX, railMin.y + 2.0f);
            const ImVec2 nextMax(controlX + buttonWidth, railMax.y - 2.0f);
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
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

void CommandBar::OnPostEnd() {
    ImGui::PopStyleVar(3);
}

void CommandBar::OnShow() {
    ResetEditorState();
    m_VisiblePrev = false;
}

void CommandBar::OnHide() {
    ResetEditorState();
    m_InputActive = false;
    m_VisiblePrev = true;
}

void CommandBar::ToggleCommandBar(bool on) {
    if (on) {
        if (IsVisible())
            return;
        Show();
        Bui::BlockKeyboardInput(this);
    } else {
        if (!IsVisible())
            return;
        Hide();
        if (ImGuiContext *context = Bui::GetImGuiContext()) {
            Bui::ImGuiContextScope scope(context);
            ImGui::SetWindowFocus(nullptr);
        }
        Bui::UnblockKeyboardAfterRelease(this);
    }
}

void CommandBar::ResetEditorState() {
    m_Buffer.clear();
    m_CursorPos = 0;
    ClearPendingLines();
    m_Navigator.Reset();
    m_NavigatedText.clear();
    m_Suggestion.clear();

    m_SearchActive = false;
    m_SearchQuery.clear();
    m_SearchDraft.clear();
    m_SearchMatch.clear();
    m_SearchIndex = -1;
    m_SearchFailed = false;
    m_SearchRequested = false;
    m_SearchAcceptRequested = false;
    m_SearchCancelRequested = false;

    m_HighlightSource.clear();
    m_HighlightSpans.clear();
    InvalidateCandidates();
    m_FocusInputNextFrame = false;
}

void CommandBar::ClearPendingLines() {
    m_PendingLines.clear();
}

std::string CommandBar::JoinPending(std::string_view lastRow) const {
    return CommandInput::Join(m_PendingLines, lastRow);
}

std::size_t CommandBar::CurrentRowOffset() const {
    return CommandInput::PendingBytes(m_PendingLines);
}

void CommandBar::SubmitLine() {
    const std::string row = m_Buffer;
    std::string line = JoinPending(row);
    ModContext *context = BML_GetModContext();

    // Enforce the executor's hard limit before parsing or retaining another
    // continuation row. Otherwise an unterminated quote could grow pending
    // input without bound and repeatedly reparse an ever-larger string.
    if (line.size() > BML::Shell::Limits::MaxLineBytes) {
        ClearPendingLines();
        if (context)
            context->ExecuteCommandLine(line.c_str());
        m_Navigator.Reset();
        m_Suggestion.clear();
        if (m_KeepOpen && IsVisible()) {
            m_Buffer.clear();
            m_CursorPos = 0;
            m_FocusInputNextFrame = true;
        } else {
            ToggleCommandBar(false);
        }
        return;
    }

    // A line that is not finished yet asks for another row instead of running.
    const BML::Shell::ParseResult parsed = BML::Shell::Parse(line, nullptr);
    if (parsed.incomplete) {
        m_PendingLines.push_back(row);
        m_Buffer.clear();
        m_CursorPos = 0;
        m_Navigator.Reset();
        m_Suggestion.clear();
        m_FocusInputNextFrame = true;
        return;
    }
    ClearPendingLines();

    const bool blank = std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    if (!blank && context) {
        if (m_History) {
            std::string expanded;
            std::string error;
            bool changed = false;
            if (!m_History->Expand(line, expanded, changed, error)) {
                const std::string message = "\x1b[31m" + error + "\x1b[0m";
                context->SendIngameMessage(message.c_str());
                m_Buffer.clear();
                m_CursorPos = 0;
                m_FocusInputNextFrame = true;
                return;
            }
            if (changed) {
                line = expanded;
                const std::string echo = "\x1b[2m" + CommandInput::SingleLinePreview(line) + "\x1b[0m";
                context->SendIngameMessage(echo.c_str());
            }
            m_History->Add(line);
        }
        m_Navigator.Reset();
        m_Suggestion.clear();
        context->ExecuteCommandLine(line.c_str());
    }

    if (m_KeepOpen && IsVisible()) {
        m_Buffer.clear();
        m_CursorPos = 0;
        m_FocusInputNextFrame = true;
    } else {
        ToggleCommandBar(false);
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
    m_Plan = BML::Shell::CompletionPlan{};
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

void CommandBar::ApplyCandidate(ImGuiInputTextCallbackData *data, std::string_view candidate, bool final) {
    if (!data)
        return;

    const std::string_view current(data->Buf, static_cast<std::size_t>(data->BufTextLen));
    std::string logical = JoinPending(current);
    const std::size_t end = std::min(CurrentRowOffset() + static_cast<std::size_t>(data->CursorPos), logical.size());
    const std::size_t begin = std::min(m_Plan.replaceBegin, end);
    const std::string replacement = BML::Shell::RenderReplacement(m_Plan, candidate, final);
    logical.replace(begin, end - begin, replacement);
    if (logical.size() > BML::Shell::Limits::MaxLineBytes) {
        if (ModContext *context = BML_GetModContext())
            context->SendIngameMessage("\x1b[31mcommand line is too long\x1b[0m");
        return;
    }
    SetLogicalText(data, logical, begin + replacement.size());
}

void CommandBar::BeginCompletion(ImGuiInputTextCallbackData *data, bool selectPrevious) {
    InvalidateCandidates();
    const std::string_view current(data->Buf, static_cast<std::size_t>(data->BufTextLen));
    const std::string logical = JoinPending(current);
    const std::size_t cursor = CurrentRowOffset() + static_cast<std::size_t>(data->CursorPos);
    m_Plan = BML::Shell::BuildCompletion(logical, cursor, MakeProviders(),
                                         &BML_GetModContext()->GetShellEnvironment());
    for (const std::string &candidate : m_Plan.candidates)
        m_Candidates.Add(candidate);
    m_Candidates.BuildPages(m_WindowSize.x);

    if (m_Candidates.Size() == 1) {
        ApplyCandidate(data, m_Candidates[0], true);
        InvalidateCandidates();
    } else if (m_Candidates.Size() > 1) {
        const std::size_t commonLength = m_Candidates.CommonPrefixLength();
        if (commonLength > m_Plan.prefix.size())
            ApplyCandidate(data, std::string_view(m_Candidates[0]).substr(0, commonLength), false);
        if (selectPrevious)
            PrevCandidate();
    }

    m_CursorPos = data->CursorPos;
}

bool CommandBar::HandleCompletionShortcuts(ImGuiInputTextCallbackData *data) {
    if (!data)
        return false;

    constexpr ImGuiInputFlags ShortcutFlags = ImGuiInputFlags_RouteAlways;
    constexpr ImGuiInputFlags RepeatShortcutFlags = ShortcutFlags | ImGuiInputFlags_Repeat;
    if (!m_Candidates.AreHintsVisible()) {
        if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_Tab, ShortcutFlags, data->ID)) {
            BeginCompletion(data, true);
            return true;
        }
        if (ImGui::Shortcut(ImGuiKey_Tab, ShortcutFlags, data->ID)) {
            BeginCompletion(data, false);
            return true;
        }
        return false;
    }

    if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_Tab, ShortcutFlags, data->ID) ||
        ImGui::Shortcut(ImGuiKey_UpArrow, RepeatShortcutFlags, data->ID)) {
        PrevCandidate();
        return true;
    }
    if (ImGui::Shortcut(ImGuiKey_Tab, ShortcutFlags, data->ID) ||
        ImGui::Shortcut(ImGuiKey_DownArrow, RepeatShortcutFlags, data->ID)) {
        NextCandidate();
        return true;
    }
    if (ImGui::Shortcut(ImGuiKey_PageUp, RepeatShortcutFlags, data->ID)) {
        PrevPageOfCandidates();
        return true;
    }
    if (ImGui::Shortcut(ImGuiKey_PageDown, RepeatShortcutFlags, data->ID)) {
        NextPageOfCandidates();
        return true;
    }
    return false;
}

void CommandBar::SetBufferText(ImGuiInputTextCallbackData *data, std::string_view text) {
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, text.data(), text.data() + text.size());
    data->CursorPos = data->BufTextLen;
    data->SelectionStart = data->SelectionEnd = data->CursorPos;
}

void CommandBar::SetLogicalText(ImGuiInputTextCallbackData *data, std::string_view text,
                                std::size_t logicalCursor) {
    CommandInput::Rows rows = CommandInput::Split(text);
    m_PendingLines = std::move(rows.pending);
    SetBufferText(data, rows.current);

    const std::size_t relativeCursor = logicalCursor > rows.currentOffset
        ? logicalCursor - rows.currentOffset
        : 0;
    data->CursorPos = static_cast<int>(std::min(relativeCursor, rows.current.size()));
    data->SelectionStart = data->SelectionEnd = data->CursorPos;
    m_Suggestion.clear();
    m_HighlightSource.clear();
}

void CommandBar::EnforceInputLimit(ImGuiInputTextCallbackData *data) {
    const std::size_t prefixBytes = CurrentRowOffset();
    const std::size_t available = prefixBytes < BML::Shell::Limits::MaxLineBytes
        ? BML::Shell::Limits::MaxLineBytes - prefixBytes
        : 0;
    const std::size_t length = static_cast<std::size_t>(data->BufTextLen);
    if (length <= available)
        return;

    // Do not leave half of a UTF-8 codepoint at the end after clipping a paste.
    std::size_t keep = available;
    while (keep > 0 && keep < length &&
           (static_cast<unsigned char>(data->Buf[keep]) & 0xC0u) == 0x80u) {
        --keep;
    }
    data->DeleteChars(static_cast<int>(keep), data->BufTextLen - static_cast<int>(keep));
    data->CursorPos = std::min(data->CursorPos, data->BufTextLen);
    data->SelectionStart = std::min(data->SelectionStart, data->BufTextLen);
    data->SelectionEnd = std::min(data->SelectionEnd, data->BufTextLen);
    m_Suggestion.clear();
    InvalidateCandidates();
}

bool CommandBar::HandleEditingShortcuts(ImGuiInputTextCallbackData *data) {
    constexpr ImGuiInputFlags Flags = ImGuiInputFlags_RouteAlways;
    const int cursor = data->CursorPos;
    const int length = data->BufTextLen;

    auto placeCursor = [&](int pos) {
        data->CursorPos = std::clamp(pos, 0, data->BufTextLen);
        data->SelectionStart = data->SelectionEnd = data->CursorPos;
    };
    auto kill = [&](int begin, int end) {
        begin = std::clamp(begin, 0, length);
        end = std::clamp(end, begin, length);
        if (end == begin)
            return;
        m_KillBuffer.assign(data->Buf + begin, static_cast<std::size_t>(end - begin));
        data->DeleteChars(begin, end - begin);
        placeCursor(begin);
    };

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, Flags, data->ID)) {
        placeCursor(0);
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_E, Flags, data->ID)) {
        placeCursor(length);
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_U, Flags, data->ID)) {
        kill(0, cursor);
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_K, Flags, data->ID)) {
        kill(cursor, length);
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W, Flags, data->ID)) {
        kill(WordStartBefore(data->Buf, cursor), cursor);
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, Flags, data->ID)) {
        if (!m_KillBuffer.empty()) {
            data->InsertChars(cursor, m_KillBuffer.c_str(), m_KillBuffer.c_str() + m_KillBuffer.size());
            placeCursor(cursor + static_cast<int>(m_KillBuffer.size()));
        }
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_B, Flags | ImGuiInputFlags_Repeat, data->ID)) {
        placeCursor(WordStartBefore(data->Buf, cursor));
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_F, Flags | ImGuiInputFlags_Repeat, data->ID)) {
        placeCursor(WordEndAfter(data->Buf, cursor, length));
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_L, Flags, data->ID)) {
        if (ModContext *context = BML_GetModContext())
            context->ClearIngameMessages();
        return true;
    }
    return false;
}

void CommandBar::UpdateSuggestion(const ImGuiInputTextCallbackData *data) {
    m_Suggestion.clear();
    if (!m_History || m_SearchActive || data->CursorPos != data->BufTextLen)
        return;
    const std::string_view current(data->Buf, static_cast<std::size_t>(data->BufTextLen));
    const std::string logical = JoinPending(current);
    if (logical.empty())
        return;
    if (const std::string *entry = m_History->Suggest(logical))
        m_Suggestion = entry->substr(logical.size());
}

bool CommandBar::HandleSuggestionShortcuts(ImGuiInputTextCallbackData *data) {
    if (m_Suggestion.empty() || m_SearchActive)
        return false;
    // m_CursorPos still holds last frame's caret, so a Right press that only
    // moved the caret to the end does not also accept the suggestion.
    if (m_CursorPos != data->BufTextLen || data->CursorPos != data->BufTextLen)
        return false;

    constexpr ImGuiInputFlags Flags = ImGuiInputFlags_RouteAlways;
    if (ImGui::Shortcut(ImGuiKey_RightArrow, Flags, data->ID) ||
        ImGui::Shortcut(ImGuiKey_End, Flags, data->ID) ||
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, Flags, data->ID)) {
        const std::string_view current(data->Buf, static_cast<std::size_t>(data->BufTextLen));
        std::string logical = JoinPending(current);
        logical += m_Suggestion;
        if (logical.size() > BML::Shell::Limits::MaxLineBytes)
            return true;
        SetLogicalText(data, logical, logical.size());
        m_Suggestion.clear();
        return true;
    }
    if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_RightArrow, Flags, data->ID)) {
        const int wordEnd = WordEndAfter(m_Suggestion.c_str(), 0, static_cast<int>(m_Suggestion.size()));
        const std::string_view current(data->Buf, static_cast<std::size_t>(data->BufTextLen));
        std::string logical = JoinPending(current);
        logical.append(m_Suggestion.data(), static_cast<std::size_t>(wordEnd));
        if (logical.size() > BML::Shell::Limits::MaxLineBytes)
            return true;
        SetLogicalText(data, logical, logical.size());
        return true;
    }
    return false;
}

void CommandBar::HandleHistoryNavigation(ImGuiInputTextCallbackData *data) {
    if (!m_History)
        return;
    const std::string_view currentRow(data->Buf, static_cast<std::size_t>(data->BufTextLen));
    const std::string current = JoinPending(currentRow);
    std::string text;
    bool changed = false;
    if (data->EventKey == ImGuiKey_UpArrow)
        changed = m_Navigator.Up(*m_History, current, text);
    else if (data->EventKey == ImGuiKey_DownArrow)
        changed = m_Navigator.Down(*m_History, current, text);
    if (!changed)
        return;
    SetLogicalText(data, text, text.size());
    m_NavigatedText = text;
}

void CommandBar::BeginSearch(ImGuiInputTextCallbackData *data) {
    m_SearchActive = true;
    const std::string_view current(data->Buf, static_cast<std::size_t>(data->BufTextLen));
    m_SearchDraft = JoinPending(current);
    m_SearchQuery.clear();
    m_SearchMatch.clear();
    m_SearchFailed = false;
    m_SearchIndex = m_History ? static_cast<int>(m_History->Size()) : 0;
    m_SearchAcceptRequested = false;
    m_SearchCancelRequested = false;
    m_Suggestion.clear();
    InvalidateCandidates();
    SetBufferText(data, "");
    m_HighlightSource.clear();
}

void CommandBar::UpdateSearch(std::string_view query) {
    m_SearchQuery.assign(query.data(), query.size());
    if (!m_History) {
        m_SearchFailed = true;
        return;
    }
    const int found = m_History->SearchBackward(m_SearchQuery, static_cast<int>(m_History->Size()));
    m_SearchFailed = found < 0;
    if (found >= 0) {
        m_SearchIndex = found;
        m_SearchMatch = m_History->Entries()[static_cast<std::size_t>(found)];
    }
}

void CommandBar::AdvanceSearch() {
    if (!m_History)
        return;
    const int found = m_History->SearchBackward(m_SearchQuery, m_SearchIndex);
    m_SearchFailed = found < 0;
    if (found >= 0) {
        m_SearchIndex = found;
        m_SearchMatch = m_History->Entries()[static_cast<std::size_t>(found)];
    }
}

void CommandBar::EndSearch(bool accept) {
    m_SearchActive = false;
    m_SearchAcceptRequested = false;
    m_SearchCancelRequested = false;
    m_SearchRequested = false;
    m_SearchQuery.clear();
    m_SearchFailed = false;
    (void) accept;
}

int CommandBar::OnTextEdit(ImGuiInputTextCallbackData *data) {
    if (m_TextCompositionActive) {
        if (!m_Candidates.Empty())
            InvalidateCandidates();
        m_CursorPos = data->CursorPos;
        return 0;
    }

    if (m_SearchRequested) {
        m_SearchRequested = false;
        if (m_SearchActive)
            AdvanceSearch();
        else
            BeginSearch(data);
    }

    if (m_SearchActive) {
        if (m_SearchAcceptRequested || m_SearchCancelRequested) {
            const std::string text = m_SearchAcceptRequested && !m_SearchMatch.empty() ? m_SearchMatch : m_SearchDraft;
            EndSearch(m_SearchAcceptRequested);
            SetLogicalText(data, text, text.size());
            m_CursorPos = data->CursorPos;
            return 0;
        }
        const std::string_view query(data->Buf, static_cast<std::size_t>(data->BufTextLen));
        if (query != m_SearchQuery)
            UpdateSearch(query);
        m_CursorPos = data->CursorPos;
        return 0;
    }

    EnforceInputLimit(data);

    if (HandleCompletionShortcuts(data))
        return 0;

    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackHistory:
            if (!m_Candidates.Empty())
                InvalidateCandidates();
            HandleHistoryNavigation(data);
            break;
        case ImGuiInputTextFlags_CallbackAlways: {
            if (const std::string *selected = m_Candidates.Selected()) {
                ApplyCandidate(data, *selected, true);
                InvalidateCandidates();
            }

            if (m_CursorPos != data->CursorPos)
                InvalidateCandidates();

            // Editing the text after walking the history starts a fresh walk.
            const std::string_view currentRow(data->Buf, static_cast<std::size_t>(data->BufTextLen));
            if (m_Navigator.Browsing() && JoinPending(currentRow) != m_NavigatedText) {
                m_Navigator.Reset();
            }

            if (HandleSuggestionShortcuts(data) || HandleEditingShortcuts(data)) {
                InvalidateCandidates();
            }
            if (!m_SearchActive)
                UpdateSuggestion(data);

            m_CursorPos = data->CursorPos;
        }
        break;
        case ImGuiInputTextFlags_CallbackEdit:
            if (!m_Candidates.Empty())
                InvalidateCandidates();
            break;
        default:
            break;
    }

    return 0;
}

int CommandBar::TextEditCallback(ImGuiInputTextCallbackData *data) {
    auto *bar = static_cast<CommandBar *>(data->UserData);
    return bar->OnTextEdit(data);
}
