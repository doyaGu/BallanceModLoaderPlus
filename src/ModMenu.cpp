#include "ModMenu.h"

#include <cmath>

#include "BML/InputHook.h"

#include "ModContext.h"

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

        PropertyState *state = m_CurrentStates[index];
        if (!state)
            return true;

        const char *name = property->GetName();
        if (!name || name[0] == '\0')
            return true;

        switch (property->GetType()) {
            case IProperty::STRING: {
                Bui::InputTextButton(property->GetName(), m_StringBuffers[index], sizeof(m_StringBuffers[index]));
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    state->stringValue = m_StringBuffers[index];
                    UpdateStateDirty(state);
                }
                break;
            }
            case IProperty::BOOLEAN: {
                Bui::YesNoButton(property->GetName(), &state->boolValue);
                UpdateStateDirty(state);
                break;
            }
            case IProperty::INTEGER: {
                Bui::InputIntButton(property->GetName(), &state->intValue);
                UpdateStateDirty(state);
                break;
            }
            case IProperty::KEY: {
                if (Bui::KeyButton(property->GetName(), &m_KeyToggled[index], &state->keyChord)) {
                    state->keyChord &= ~ImGuiMod_Mask_;
                }
                UpdateStateDirty(state);
                break;
            }
            case IProperty::FLOAT: {
                Bui::InputFloatButton(property->GetName(), &state->floatValue);
                UpdateStateDirty(state);
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
    m_HasPendingChanges = HasPendingChanges();
}

void ModOptionPage::OnPreEnd() {
    m_HasPendingChanges = HasPendingChanges();
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
    SetPageCount(Bui::CalcPageCount(count, 4));

    m_PropertyStates.clear();
    m_CurrentStates.fill(nullptr);
    memset(m_StringBuffers, 0, sizeof(m_StringBuffers));
    memset(m_KeyToggled, 0, sizeof(m_KeyToggled));

    BindPageStates(GetPage());
    m_HasPendingChanges = HasPendingChanges();
    return true;
}

void ModOptionPage::OnClose() {
    m_PropertyStates.clear();
    m_CurrentStates.fill(nullptr);
    m_HasPendingChanges = false;
}

void ModOptionPage::OnPageChanged(int newPage, int oldPage) {
    (void)oldPage;
    BindPageStates(newPage);
    m_HasPendingChanges = HasPendingChanges();
}

void ModOptionPage::BindPageStates(int pageIndex) {
    m_CurrentStates.fill(nullptr);

    if (!m_Category || pageIndex < 0)
        return;

    const int n = pageIndex * 4;

    for (int i = 0; i < 4; ++i) {
        m_KeyToggled[i] = false;
        Property *property = m_Category->GetProperty(n + i);
        if (!property)
            continue;

        auto &state = m_PropertyStates[property];
        if (!state.property) {
            state.property = property;
            state.type = property->GetType();

            switch (state.type) {
                case IProperty::STRING: {
                    const char *value = property->GetString();
                    state.originalString = value ? value : "";
                    state.stringValue = state.originalString;
                    break;
                }
                case IProperty::BOOLEAN:
                    state.originalBool = property->GetBoolean();
                    state.boolValue = state.originalBool;
                    break;
                case IProperty::INTEGER:
                    state.originalInt = property->GetInteger();
                    state.intValue = state.originalInt;
                    break;
                case IProperty::KEY:
                    state.originalKeyChord = Bui::CKKeyToImGuiKey(property->GetKey());
                    state.keyChord = state.originalKeyChord;
                    break;
                case IProperty::FLOAT:
                    state.originalFloat = property->GetFloat();
                    state.floatValue = state.originalFloat;
                    break;
                default:
                    break;
            }
        }

        m_CurrentStates[i] = &state;

        if (state.type == IProperty::STRING) {
            strncpy(m_StringBuffers[i], state.stringValue.c_str(), BUFFER_SIZE - 1);
            m_StringBuffers[i][BUFFER_SIZE - 1] = '\0';
        }
    }
}

void ModOptionPage::SaveChanges() {
    if (!m_Category)
        return;

    for (auto &entry : m_PropertyStates) {
        PropertyState &state = entry.second;
        if (!state.property || !state.dirty)
            continue;

        switch (state.type) {
            case IProperty::STRING:
                state.property->SetString(state.stringValue.c_str());
                state.originalString = state.stringValue;
                break;
            case IProperty::BOOLEAN:
                state.property->SetBoolean(state.boolValue);
                state.originalBool = state.boolValue;
                break;
            case IProperty::INTEGER:
                state.property->SetInteger(state.intValue);
                state.originalInt = state.intValue;
                break;
            case IProperty::KEY:
                state.property->SetKey(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(state.keyChord)));
                state.originalKeyChord = state.keyChord;
                break;
            case IProperty::FLOAT:
                state.property->SetFloat(state.floatValue);
                state.originalFloat = state.floatValue;
                break;
            default:
                break;
        }

        state.dirty = false;
    }

    m_HasPendingChanges = HasPendingChanges();
}

void ModOptionPage::RevertChanges() {
    if (!m_Category)
        return;

    for (int i = 0; i < 4; ++i) {
        PropertyState *state = m_CurrentStates[i];
        if (!state)
            continue;

        switch (state->type) {
            case IProperty::STRING:
                state->stringValue = state->originalString;
                strncpy(m_StringBuffers[i], state->stringValue.c_str(), BUFFER_SIZE - 1);
                m_StringBuffers[i][BUFFER_SIZE - 1] = '\0';
                break;
            case IProperty::BOOLEAN:
                state->boolValue = state->originalBool;
                break;
            case IProperty::INTEGER:
                state->intValue = state->originalInt;
                break;
            case IProperty::KEY:
                state->keyChord = state->originalKeyChord;
                m_KeyToggled[i] = false;
                break;
            case IProperty::FLOAT:
                state->floatValue = state->originalFloat;
                break;
            default:
                break;
        }

        state->dirty = false;
    }

    m_HasPendingChanges = HasPendingChanges();
}

bool ModOptionPage::HasPendingChanges() const {
    for (const auto &entry : m_PropertyStates) {
        if (entry.second.dirty)
            return true;
    }
    return false;
}

void ModOptionPage::UpdateStateDirty(PropertyState *state) {
    if (!state)
        return;

    bool dirty = false;
    switch (state->type) {
        case IProperty::STRING:
            dirty = state->stringValue != state->originalString;
            break;
        case IProperty::BOOLEAN:
            dirty = state->boolValue != state->originalBool;
            break;
        case IProperty::INTEGER:
            dirty = state->intValue != state->originalInt;
            break;
        case IProperty::KEY:
            dirty = state->keyChord != state->originalKeyChord;
            break;
        case IProperty::FLOAT:
            dirty = std::fabs(state->floatValue - state->originalFloat) > EPSILON;
            break;
        default:
            break;
    }

    state->dirty = dirty;
    m_HasPendingChanges = HasPendingChanges();
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
