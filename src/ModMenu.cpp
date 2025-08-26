#include "ModMenu.h"

#include "BML/InputHook.h"

#include "ModContext.h"
#include "StringUtils.h"

void ModMenu::Init() {
    CreatePage<ModListPage>();
    CreatePage<ModPage>();
    CreatePage<ModOptionPage>();
}

void ModMenu::OnOpen() {
    BML_GetModContext()->GetInputManager()->Block(CK_INPUT_DEVICE_KEYBOARD);
}

void ModMenu::OnClose() {
    auto *context = BML_GetCKContext();
    auto *modContext = BML_GetModContext();
    auto *inputHook = modContext->GetInputManager();

    CKBehavior *beh = modContext->GetScriptByName("Menu_Options");
    context->GetCurrentScene()->Activate(beh, true);

    modContext->AddTimerLoop(1ul, [inputHook] {
        if (inputHook->oIsKeyDown(CKKEY_ESCAPE) || inputHook->oIsKeyDown(CKKEY_RETURN))
            return true;
        inputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });
}

Config *ModMenu::GetConfig(IMod *mod) {
    return BML_GetModContext()->GetConfig(mod);
}

void ModListPage::OnPostBegin() {
    Bui::Title(m_Title.c_str());

    const int count = BML_GetModContext()->GetModCount();
    SetPageCount(count % 4 == 0 ? count / 4 : count / 4 + 1);

    if (m_PageIndex > 0 && Bui::NavLeft()) PrevPage();
    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 && Bui::NavRight()) NextPage();
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
    SetPageCount(((count % 4) == 0) ? count / 4 : count / 4 + 1);
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

    if (m_PageIndex > 0 &&
        Bui::NavLeft(0.35f, 0.59f)) {
        PrevPage();
    }

    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
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
    if (m_PageIndex > 0 && Bui::NavLeft()) PrevPage();
    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 && Bui::NavRight()) NextPage();

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
    SetPageCount(count % 4 == 0 ? count / 4 : count / 4 + 1);

    FlushBuffers();
    LoadOriginalValues();
    return true;
}

void ModOptionPage::OnClose() {
    FlushBuffers();
}

void ModOptionPage::OnPageChanged(int newPage, int oldPage) {
    FlushBuffers();
    LoadOriginalValues();
}

void ModOptionPage::FlushBuffers() {
    memset(m_Buffers, 0, sizeof(m_Buffers));
    memset(m_OriginalBuffers, 0, sizeof(m_OriginalBuffers));
    memset(m_BufferHashes, 0, sizeof(m_BufferHashes));
    memset(m_KeyToggled, 0, sizeof(m_KeyToggled));
    memset(m_KeyChord, 0, sizeof(m_KeyChord));
    memset(m_OriginalKeyChord, 0, sizeof(m_OriginalKeyChord));
    memset(m_IntFlags, 0, sizeof(m_IntFlags));
    memset(m_FloatFlags, 0, sizeof(m_FloatFlags));
    memset(m_IntValues, 0, sizeof(m_IntValues));
    memset(m_OriginalIntValues, 0, sizeof(m_OriginalIntValues));
    memset(m_FloatValues, 0, sizeof(m_FloatValues));
    memset(m_OriginalFloatValues, 0, sizeof(m_OriginalFloatValues));
    memset(m_BoolValues, 0, sizeof(m_BoolValues));
    memset(m_OriginalBoolValues, 0, sizeof(m_OriginalBoolValues));
    m_HasPendingChanges = false;
}

void ModOptionPage::LoadOriginalValues() {
    if (!m_Category)
        return;

    const int n = GetPage() * 4;

    for (int i = 0; i < 4; ++i) {
        Property *property = m_Category->GetProperty(n + i);
        if (!property)
            continue;

        switch (property->GetType()) {
            case IProperty::STRING: {
                size_t copySize = std::min(property->GetStringSize() + 1, sizeof(m_Buffers[i]) - 1);
                strncpy(m_Buffers[i], property->GetString(), copySize);
                strncpy(m_OriginalBuffers[i], property->GetString(), copySize);
                m_Buffers[i][copySize] = '\0';
                m_OriginalBuffers[i][copySize] = '\0';
                m_BufferHashes[i] = property->GetHash();
                break;
            }
            case IProperty::BOOLEAN:
                m_BoolValues[i] = property->GetBoolean();
                m_OriginalBoolValues[i] = property->GetBoolean();
                break;
            case IProperty::INTEGER:
                m_IntValues[i] = property->GetInteger();
                m_OriginalIntValues[i] = property->GetInteger();
                m_IntFlags[i] = 1;
                break;
            case IProperty::KEY:
                m_KeyChord[i] = Bui::CKKeyToImGuiKey(property->GetKey());
                m_OriginalKeyChord[i] = m_KeyChord[i];
                break;
            case IProperty::FLOAT:
                m_FloatValues[i] = property->GetFloat();
                m_OriginalFloatValues[i] = property->GetFloat();
                m_FloatFlags[i] = 1;
                break;
            default:
                break;
        }
    }

    m_HasPendingChanges = false;
}


void ModOptionPage::SaveChanges() {
    if (!m_Category)
        return;

    const int n = GetPage() * 4;

    for (int i = 0; i < 4; ++i) {
        Property *property = m_Category->GetProperty(n + i);
        if (!property)
            continue;

        switch (property->GetType()) {
            case IProperty::STRING:
                if (strcmp(m_Buffers[i], m_OriginalBuffers[i]) != 0) {
                    property->SetString(m_Buffers[i]);
                    strcpy(m_OriginalBuffers[i], m_Buffers[i]);
                }
                break;
            case IProperty::BOOLEAN:
                if (m_BoolValues[i] != m_OriginalBoolValues[i]) {
                    property->SetBoolean(m_BoolValues[i]);
                    m_OriginalBoolValues[i] = m_BoolValues[i];
                }
                break;
            case IProperty::INTEGER:
                if (m_IntValues[i] != m_OriginalIntValues[i]) {
                    property->SetInteger(m_IntValues[i]);
                    m_OriginalIntValues[i] = m_IntValues[i];
                }
                break;
            case IProperty::KEY:
                if (m_KeyChord[i] != m_OriginalKeyChord[i]) {
                    property->SetKey(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(m_KeyChord[i])));
                    m_OriginalKeyChord[i] = m_KeyChord[i];
                }
                break;
            case IProperty::FLOAT:
                if (fabs(m_FloatValues[i] - m_OriginalFloatValues[i]) > EPSILON) {
                    property->SetFloat(m_FloatValues[i]);
                    m_OriginalFloatValues[i] = m_FloatValues[i];
                }
                break;
            default:
                break;
        }
    }

    m_HasPendingChanges = false;
}

void ModOptionPage::RevertChanges() {
    if (!m_Category)
        return;

    const int n = GetPage() * 4;

    for (int i = 0; i < 4; ++i) {
        Property *property = m_Category->GetProperty(n + i);
        if (!property)
            continue;

        switch (property->GetType()) {
            case IProperty::STRING:
                strcpy(m_Buffers[i], m_OriginalBuffers[i]);
                m_BufferHashes[i] = utils::HashString(m_Buffers[i]);
                break;
            case IProperty::BOOLEAN:
                m_BoolValues[i] = m_OriginalBoolValues[i];
                break;
            case IProperty::INTEGER:
                m_IntValues[i] = m_OriginalIntValues[i];
                break;
            case IProperty::KEY:
                m_KeyChord[i] = m_OriginalKeyChord[i];
                m_KeyToggled[i] = false;
                break;
            case IProperty::FLOAT:
                m_FloatValues[i] = m_OriginalFloatValues[i];
                break;
            default:
                break;
        }
    }

    m_HasPendingChanges = false;
}

bool ModOptionPage::HasPendingChanges() const {
    if (!m_Category)
        return false;

    const int n = GetPage() * 4;

    for (int i = 0; i < 4; ++i) {
        Property *property = m_Category->GetProperty(n + i);
        if (!property)
            continue;

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
                if (fabs(m_FloatValues[i] - m_OriginalFloatValues[i]) > EPSILON)
                    return true;
                break;
            default:
                break;
        }
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