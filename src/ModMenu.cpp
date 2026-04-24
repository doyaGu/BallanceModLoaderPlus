#include "ModMenu.h"

#include <cmath>
#include <cstring>
#include <algorithm>

#include "BML/InputHook.h"

#include "ModContext.h"
#include "StringUtils.h"

namespace {
    void CopyStringToBuffer(const std::string &value, char *buffer, size_t bufferSize) {
        if (!buffer || bufferSize == 0)
            return;

        const size_t copySize = std::min(value.size(), bufferSize - 1);
        memcpy(buffer, value.data(), copySize);
        buffer[copySize] = '\0';
    }
}

void ModMenu::Init() {
    CreatePage<ModListPage>();
    CreatePage<ModPage>();
    CreatePage<ModOptionPage>();
}

void ModMenu::OnOpen() {
    Bui::BlockKeyboardInput();
}

void ModMenu::OnClose() {
    Bui::TransitionToScriptAndUnblock("Menu_Options");
}

Config *ModMenu::GetConfig(IMod *mod) {
    return BML_GetModContext()->GetConfig(mod);
}

void ModListPage::OnPostBegin() {
    Bui::Title(m_Title.c_str());

    const int count = BML_GetModContext()->GetModCount();
    SetPageCount(Bui::CalcPageCount(count, 4));

    if (Bui::CanPrevPage(m_PageIndex) && Bui::NavLeft()) PrevPage();
    if (Bui::CanNextPage(m_PageIndex, count, 4) && Bui::NavRight()) NextPage();
}

void ModListPage::OnDraw() {
    const int n = GetPage() * 4;

    Bui::Entries([&](size_t index) {
        IMod *mod = BML_GetModContext()->GetMod(static_cast<int>(n + index));
        if (!mod)
            return false;
        const char *id = mod->GetID();
        if (Bui::MainButton(id)) {
            dynamic_cast<ModMenu *>(m_Menu)->SetCurrentMod(mod);
            m_Menu->OpenPage("Mod Page");
        }
        return true;
    }, 0.35f, 0.24f, 0.14f, 4);
}

void ModPage::OnPostBegin() {
    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    const float titleX = menuPos.x * 1.03f;
    const float titleWidth = menuSize.x * 0.95f;

    ImGui::Dummy(Bui::CoordToPixel(ImVec2(1.0f, 0.1f)));

    auto *mod = dynamic_cast<ModMenu *>(m_Menu)->GetCurrentMod();

    Bui::WrappedText(mod->GetName(), titleWidth, titleX, 1.2f);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "By %s", mod->GetAuthor());
    Bui::WrappedText(m_TextBuf, titleWidth, titleX);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "v%s", mod->GetVersion());
    Bui::WrappedText(m_TextBuf, titleWidth, titleX);

    ImGui::NewLine();

    Bui::WrappedText(mod->GetDescription(), titleWidth, titleX);

    m_Config = ModMenu::GetConfig(mod);
    if (!m_Config)
        return;

    int count = (int) m_Config->GetCategoryCount();
    SetPageCount(Bui::CalcPageCount(count, 4));
}

void ModPage::OnDraw() {
    if (!m_Config)
        return;

    bool v = true;
    const int n = GetPage() * 4;

    Bui::Entries([&](size_t index) {
        Category *category = m_Config->GetCategory(static_cast<int>(n + index));
        if (!category)
            return false;

        const char *name = category->GetName();
        if (!name)
            return true;

        if (Bui::LevelButton(name, &v)) {
            dynamic_cast<ModMenu *>(m_Menu)->SetCurrentCategory(category);
            m_Menu->OpenPage("Mod Options");
        }

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(category);
        }
        return true;
    }, 0.4031f, 0.5f, 0.06f, 4);

    const int totalCategories = m_Config ? (int)m_Config->GetCategoryCount() : 0;

    if (Bui::CanPrevPage(m_PageIndex) &&
        Bui::NavLeft(0.35f, 0.59f)) {
        PrevPage();
    }

    if (Bui::CanNextPage(m_PageIndex, totalCategories, 4) &&
        Bui::NavRight(0.6138f, 0.59f)) {
        NextPage();
    }
}

void ModPage::ShowCommentBox(Category *category) {
    if (!category)
        return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.4f);
    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.2f);
    ImGui::SetCursorScreenPos(commentBoxPos);
    ImGui::BeginChild("ModComment", commentBoxSize);

    const char *name = category->GetName();
    if (name[0] == '@')
        ++name;
    Bui::WrappedText(name, commentBoxSize.x);
    Bui::WrappedText(category->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ModOptionPage::OnPostBegin() {
    Bui::Title(m_Name.c_str(), 0.13f, 1.5f,  m_HasPendingChanges ? IM_COL32(255, 255, 128, 255) : IM_COL32_WHITE);

    // Navigation
    const int totalProps = m_Category ? (int)m_Category->GetPropertyCount() : 0;
    if (Bui::CanPrevPage(m_PageIndex) && Bui::NavLeft()) PrevPage();
    if (Bui::CanNextPage(m_PageIndex, totalProps, 4) && Bui::NavRight()) NextPage();

    // Update pending changes status
    m_HasPendingChanges = HasPendingChanges();
}

void ModOptionPage::OnDraw() {
    if (!m_Category)
        return;

    const int n = GetPage() * 4;

    Bui::Entries([&](size_t index) {
        Property *property = m_Category->GetProperty(static_cast<int>(n + index));
        if (!property)
            return false;

        const char *name = property->GetName();
        if (!name || name[0] == '\0')
            return true;

        switch (property->GetType()) {
            case IProperty::STRING: {
                Bui::InputTextButton(property->GetName(), m_Buffers[index], sizeof(m_Buffers[index]));
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    m_BufferHashes[index] = utils::HashString(m_Buffers[index]);
                }
                break;
            }
            case IProperty::BOOLEAN: {
                Bui::YesNoButton(property->GetName(), &m_BoolValues[index]);
                break;
            }
            case IProperty::INTEGER: {
                Bui::InputIntButton(property->GetName(), &m_IntValues[index]);
                break;
            }
            case IProperty::KEY: {
                if (Bui::KeyButton(property->GetName(), &m_KeyToggled[index], &m_KeyChord[index])) {
                    m_KeyChord[index] &= ~ImGuiMod_Mask_;
                }
                break;
            }
            case IProperty::FLOAT: {
                Bui::InputFloatButton(property->GetName(), &m_FloatValues[index]);
                break;
            }
            default:
                ImGui::Dummy(Bui::GetButtonSize(Bui::BUTTON_OPTION));
                break;
        }

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(property);
        }

        return true;
    }, 0.35f, 0.24f, 0.14f, 4);
}

void ModOptionPage::OnPreEnd() {
    // Show save/revert buttons if there are pending changes
    if (m_HasPendingChanges) {
        const float x = Bui::GetButtonSizeInCoord(Bui::BUTTON_SMALL).x;
        Bui::At(0.5f - (x + 0.04f), 0.85f, [&]() {
            if (Bui::SmallButton("Save")) {
                SaveChanges();
            }
        });

        Bui::At(0.54f, 0.85f, [&]() {
            if (Bui::SmallButton("Revert")) {
                RevertChanges();
            }
        });
    } else {
        if (Bui::NavBack()) {
            if (m_Menu) {
                m_Menu->OpenPrevPage();
            } else {
                Close();
            }
        }
    }
}

bool ModOptionPage::OnOpen() {
    m_Category = dynamic_cast<ModMenu *>(m_Menu)->GetCurrentCategory();
    if (!m_Category)
        return false;

    int count = static_cast<int>(m_Category->GetPropertyCount());
    SetPageCount(Bui::CalcPageCount(count, PROPERTY_SLOTS));

    FlushBuffers();
    LoadOriginalValues();
    return true;
}

void ModOptionPage::OnClose() {
    FlushBuffers();
    m_PendingValues.clear();
    m_HasPendingChanges = false;
}

void ModOptionPage::OnPageChanged(int /*newPage*/, int /*oldPage*/) {
    SyncVisiblePageToPending();
    FlushBuffers();
    LoadOriginalValues();
}

void ModOptionPage::FlushBuffers() {
    std::memset(m_Buffers, 0, sizeof(m_Buffers));
    std::memset(m_OriginalBuffers, 0, sizeof(m_OriginalBuffers));
    std::fill_n(m_BufferHashes, PROPERTY_SLOTS, 0U);
    std::fill_n(m_KeyToggled, PROPERTY_SLOTS, false);
    std::fill_n(m_KeyChord, PROPERTY_SLOTS, static_cast<ImGuiKeyChord>(0));
    std::fill_n(m_OriginalKeyChord, PROPERTY_SLOTS, static_cast<ImGuiKeyChord>(0));
    std::fill_n(m_IntFlags, PROPERTY_SLOTS, static_cast<std::uint8_t>(0));
    std::fill_n(m_FloatFlags, PROPERTY_SLOTS, static_cast<std::uint8_t>(0));
    std::fill_n(m_IntValues, PROPERTY_SLOTS, 0);
    std::fill_n(m_OriginalIntValues, PROPERTY_SLOTS, 0);
    std::fill_n(m_FloatValues, PROPERTY_SLOTS, 0.0f);
    std::fill_n(m_OriginalFloatValues, PROPERTY_SLOTS, 0.0f);
    std::fill_n(m_BoolValues, PROPERTY_SLOTS, false);
    std::fill_n(m_OriginalBoolValues, PROPERTY_SLOTS, false);
    m_HasPendingChanges = false;
}

int ModOptionPage::GetPropertyStartIndex() const {
    return GetPage() * PROPERTY_SLOTS;
}

Property *ModOptionPage::GetVisibleProperty(int index) const {
    if (!m_Category || index < 0 || index >= PROPERTY_SLOTS)
        return nullptr;

    return m_Category->GetProperty(GetPropertyStartIndex() + index);
}

const ModOptionPage::PendingPropertyState *ModOptionPage::FindPendingState(Property *property) const {
    if (!property)
        return nullptr;

    auto it = m_PendingValues.find(property);
    return it != m_PendingValues.end() ? &it->second : nullptr;
}

ModOptionPage::PendingPropertyState &ModOptionPage::GetOrCreatePendingState(Property *property, IProperty::PropertyType type) {
    PendingPropertyState &state = m_PendingValues[property];
    state.type = type;
    return state;
}

void ModOptionPage::SyncVisiblePageToPending() {
    if (!m_Category)
        return;

    for (int i = 0; i < PROPERTY_SLOTS; ++i) {
        Property *property = GetVisibleProperty(i);
        if (!property)
            continue;

        switch (property->GetType()) {
            case IProperty::STRING: {
                const std::string currentValue = m_Buffers[i];
                const std::string originalValue = m_OriginalBuffers[i];
                if (currentValue == originalValue) {
                    m_PendingValues.erase(property);
                    break;
                }

                PendingPropertyState &state = GetOrCreatePendingState(property, IProperty::STRING);
                state.originalString = originalValue;
                state.currentString = currentValue;
                break;
            }
            case IProperty::BOOLEAN: {
                if (m_BoolValues[i] == m_OriginalBoolValues[i]) {
                    m_PendingValues.erase(property);
                    break;
                }

                PendingPropertyState &state = GetOrCreatePendingState(property, IProperty::BOOLEAN);
                state.originalBool = m_OriginalBoolValues[i];
                state.currentBool = m_BoolValues[i];
                break;
            }
            case IProperty::INTEGER: {
                if (m_IntValues[i] == m_OriginalIntValues[i]) {
                    m_PendingValues.erase(property);
                    break;
                }

                PendingPropertyState &state = GetOrCreatePendingState(property, IProperty::INTEGER);
                state.originalInt = m_OriginalIntValues[i];
                state.currentInt = m_IntValues[i];
                break;
            }
            case IProperty::KEY: {
                if (m_KeyChord[i] == m_OriginalKeyChord[i]) {
                    m_PendingValues.erase(property);
                    break;
                }

                PendingPropertyState &state = GetOrCreatePendingState(property, IProperty::KEY);
                state.originalKeyChord = m_OriginalKeyChord[i];
                state.currentKeyChord = m_KeyChord[i];
                break;
            }
            case IProperty::FLOAT: {
                if (std::fabs(m_FloatValues[i] - m_OriginalFloatValues[i]) <= EPSILON) {
                    m_PendingValues.erase(property);
                    break;
                }

                PendingPropertyState &state = GetOrCreatePendingState(property, IProperty::FLOAT);
                state.originalFloat = m_OriginalFloatValues[i];
                state.currentFloat = m_FloatValues[i];
                break;
            }
            default:
                m_PendingValues.erase(property);
                break;
        }
    }
}

void ModOptionPage::LoadOriginalValues() {
    if (!m_Category)
        return;

    for (int i = 0; i < PROPERTY_SLOTS; ++i) {
        Property *property = GetVisibleProperty(i);
        if (!property)
            continue;

        const PendingPropertyState *pendingState = FindPendingState(property);

        switch (property->GetType()) {
            case IProperty::STRING: {
                const std::string originalValue = pendingState
                    ? pendingState->originalString
                    : std::string(property->GetString() ? property->GetString() : "");
                const std::string currentValue = pendingState
                    ? pendingState->currentString
                    : originalValue;

                CopyStringToBuffer(currentValue, m_Buffers[i], BUFFER_SIZE);
                CopyStringToBuffer(originalValue, m_OriginalBuffers[i], BUFFER_SIZE);
                m_BufferHashes[i] = utils::HashString(m_Buffers[i]);
                break;
            }
            case IProperty::BOOLEAN:
                m_OriginalBoolValues[i] = pendingState
                    ? pendingState->originalBool
                    : property->GetBoolean();
                m_BoolValues[i] = pendingState
                    ? pendingState->currentBool
                    : m_OriginalBoolValues[i];
                break;
            case IProperty::INTEGER:
                m_OriginalIntValues[i] = pendingState
                    ? pendingState->originalInt
                    : property->GetInteger();
                m_IntValues[i] = pendingState
                    ? pendingState->currentInt
                    : m_OriginalIntValues[i];
                m_IntFlags[i] = 1;
                break;
            case IProperty::KEY:
                m_OriginalKeyChord[i] = pendingState
                    ? pendingState->originalKeyChord
                    : Bui::CKKeyToImGuiKey(property->GetKey());
                m_KeyChord[i] = pendingState
                    ? pendingState->currentKeyChord
                    : m_OriginalKeyChord[i];
                break;
            case IProperty::FLOAT:
                m_OriginalFloatValues[i] = pendingState
                    ? pendingState->originalFloat
                    : property->GetFloat();
                m_FloatValues[i] = pendingState
                    ? pendingState->currentFloat
                    : m_OriginalFloatValues[i];
                m_FloatFlags[i] = 1;
                break;
            default:
                break;
        }
    }

    m_HasPendingChanges = HasPendingChanges();
}


void ModOptionPage::SaveChanges() {
    if (!m_Category)
        return;

    SyncVisiblePageToPending();

    for (const auto &entry : m_PendingValues) {
        Property *property = entry.first;
        const PendingPropertyState &state = entry.second;
        if (!property)
            continue;

        switch (state.type) {
            case IProperty::STRING:
                property->SetString(state.currentString.c_str());
                break;
            case IProperty::BOOLEAN:
                property->SetBoolean(state.currentBool);
                break;
            case IProperty::INTEGER:
                property->SetInteger(state.currentInt);
                break;
            case IProperty::KEY:
                property->SetKey(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(state.currentKeyChord)));
                break;
            case IProperty::FLOAT:
                property->SetFloat(state.currentFloat);
                break;
            default:
                break;
        }
    }

    m_PendingValues.clear();
    FlushBuffers();
    LoadOriginalValues();
    m_HasPendingChanges = false;
}

void ModOptionPage::RevertChanges() {
    if (!m_Category)
        return;

    m_PendingValues.clear();
    FlushBuffers();
    LoadOriginalValues();
    m_HasPendingChanges = false;
}

bool ModOptionPage::HasPendingChanges() const {
    if (!m_Category)
        return false;

    std::array<Property *, PROPERTY_SLOTS> visibleProperties{};

    for (int i = 0; i < PROPERTY_SLOTS; ++i) {
        Property *property = GetVisibleProperty(i);
        if (!property)
            continue;

        visibleProperties[i] = property;

        switch (property->GetType()) {
            case IProperty::STRING:
                if (strcmp(m_Buffers[i], m_OriginalBuffers[i]) != 0)
                    return true;
                break;
            case IProperty::BOOLEAN:
                if (m_BoolValues[i] != m_OriginalBoolValues[i])
                    return true;
                break;
            case IProperty::INTEGER:
                if (m_IntValues[i] != m_OriginalIntValues[i])
                    return true;
                break;
            case IProperty::KEY:
                if (m_KeyChord[i] != m_OriginalKeyChord[i])
                    return true;
                break;
            case IProperty::FLOAT:
                if (std::fabs(m_FloatValues[i] - m_OriginalFloatValues[i]) > EPSILON)
                    return true;
                break;
            default:
                break;
        }
    }

    for (const auto &entry : m_PendingValues) {
        if (std::find(visibleProperties.begin(), visibleProperties.end(), entry.first) == visibleProperties.end())
            return true;
    }

    return false;
}

void ModOptionPage::ShowCommentBox(const Property *property) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.35f);
    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.3f);
    ImGui::SetCursorScreenPos(commentBoxPos);
    ImGui::BeginChild("ModOptionComment", commentBoxSize);

    Bui::WrappedText(property->GetName(), commentBoxSize.x);
    Bui::WrappedText(property->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}
