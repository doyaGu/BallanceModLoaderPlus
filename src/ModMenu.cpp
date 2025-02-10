#include "ModMenu.h"

#include "BML/InputHook.h"

#include "ModContext.h"
#include "StringUtils.h"

void ModMenu::Init() {
    m_Pages.push_back(std::make_unique<ModListPage>(this));
    m_Pages.push_back(std::make_unique<ModPage>(this));
    m_Pages.push_back(std::make_unique<ModOptionPage>(this));
}

void ModMenu::Shutdown() {
    m_Pages.clear();
}

void ModMenu::OnOpen() {
    BML_GetModContext()->GetInputManager()->Block(CK_INPUT_DEVICE_KEYBOARD);
}

void ModMenu::OnClose() {
    for (auto &page: m_Pages) {
        page->SetPage(0);
    }

    auto *context = BML_GetCKContext();
    auto *modContext = BML_GetModContext();
    auto *inputHook = modContext->GetInputManager();

    CKBehavior *beh = modContext->GetScriptByName("Menu_Options");
    context->GetCurrentScene()->Activate(beh, true);

    modContext->AddTimerLoop(1ul, [this, inputHook] {
        if (inputHook->oIsKeyDown(CKKEY_ESCAPE) || inputHook->oIsKeyDown(CKKEY_RETURN))
            return true;
        inputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });
}

Config *ModMenu::GetConfig(IMod *mod) {
    return BML_GetModContext()->GetConfig(mod);
}

ModMenuPage::ModMenuPage(ModMenu *menu, std::string name) : Page(std::move(name)), m_Menu(menu) {
    m_Menu->AddPage(this);
}

ModMenuPage::~ModMenuPage() {
    m_Menu->RemovePage(this);
}

void ModMenuPage::OnClose() {
    m_Menu->ShowPrevPage();
}

void ModListPage::OnAfterBegin() {
    if (!IsVisible())
        return;

    DrawCenteredText(m_Title.c_str());

    int count = BML_GetModContext()->GetModCount();
    SetMaxPage(count % 4 == 0 ? count / 4 : count / 4 + 1);

    if (m_PageIndex > 0 &&
        LeftButton("PrevPage")) {
        PrevPage();
    }

    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
        RightButton("NextPage")) {
        NextPage();
    }
}

void ModListPage::OnDraw() {
    const int n = GetPage() * 4;

    DrawEntries([&](std::size_t index) {
        IMod *mod = BML_GetModContext()->GetMod((int)(n + index));
        if (!mod)
            return false;
        const char *id = mod->GetID();
        if (Bui::MainButton(id)) {
            m_Menu->SetCurrentMod(mod);
            m_Menu->ShowPage("Mod Page");
        }
        return true;
    });
}

void ModPage::OnAfterBegin() {
    if (!IsVisible())
        return;

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.1f)));

    auto *mod = m_Menu->GetCurrentMod();

    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(mod->GetName(), menuSize.x, 1.2f);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "By %s", mod->GetAuthor());
    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(m_TextBuf, menuSize.x);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "v%s", mod->GetVersion());
    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(m_TextBuf, menuSize.x);

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::NewLine();

    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(mod->GetDescription(), menuSize.x);

    m_Config = ModMenu::GetConfig(mod);
    if (!m_Config)
        return;

    int count = (int) m_Config->GetCategoryCount();
    SetMaxPage(((count % 4) == 0) ? count / 4 : count / 4 + 1);
}

void ModPage::OnDraw() {
    if (!m_Config)
        return;

    bool v = true;
    const int n = GetPage() * 4;

    DrawEntries([&](std::size_t index) {
        Category *category = m_Config->GetCategory(static_cast<int>(n + index));
        if (!category)
            return false;

        const char *name = category->GetName();
        if (!name)
            return true;

        if (Bui::LevelButton(name, &v)) {
            m_Menu->SetCurrentCategory(category);
            m_Menu->ShowPage("Mod Options");
        }

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(category);
        }
        return true;
    }, ImVec2(0.4031f, 0.5f), 0.06f, 4);

    if (m_PageIndex > 0 &&
        LeftButton("PrevPage", ImVec2(0.35f, 0.59f))) {
        PrevPage();
    }

    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
        RightButton("NextPage", ImVec2(0.6138f, 0.59f))) {
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
    WrappedText(name, commentBoxSize.x);
    WrappedText(category->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ModOptionPage::OnAfterBegin() {
    m_Category = m_Menu->GetCurrentCategory();
    if (!m_Category)
        return;

    int count = static_cast<int>(m_Category->GetPropertyCount());
    SetMaxPage(count % 4 == 0 ? count / 4 : count / 4 + 1);

    Page::OnAfterBegin();
}

void ModOptionPage::OnDraw() {
    if (!m_Category)
        return;

    const int n = GetPage() * 4;

    DrawEntries([&](std::size_t index) {
        Property *property = m_Category->GetProperty(static_cast<int>(n + index));
        if (!property)
            return false;

        const char *name = property->GetName();
        if (!name || name[0] == '\0')
            return true;

        switch (property->GetType()) {
            case IProperty::STRING: {
                if (m_BufferHashes[index] != property->GetHash()) {
                    m_BufferHashes[index] = property->GetHash();
                    strncpy(m_Buffers[index], property->GetString(), property->GetStringSize() + 1);
                }
                Bui::InputTextButton(property->GetName(), m_Buffers[index], sizeof(m_Buffers[index]));
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    m_BufferHashes[index] = utils::HashString(m_Buffers[index]);
                    if (m_BufferHashes[index] != property->GetHash())
                        property->SetString(m_Buffers[index]);
                }
            }
            break;
            case IProperty::BOOLEAN:
                if (Bui::YesNoButton(property->GetName(), property->GetBooleanPtr())) {
                    property->SetModified();
                }
                break;
            case IProperty::INTEGER:
                if (m_IntFlags[index] == 0) {
                    m_IntValues[index] = property->GetInteger();
                    m_IntFlags[index] = 1;
                }
                if (Bui::InputIntButton(property->GetName(), &m_IntValues[index])) {
                    if (m_IntValues[index] != property->GetInteger()) {
                        m_IntFlags[index] = 2;
                    }
                }
                if (ImGui::IsItemDeactivatedAfterEdit() && m_IntFlags[index] == 2) {
                    property->SetInteger(m_IntValues[index]);
                    m_IntFlags[index] = 1;
                }
                break;
            case IProperty::KEY:
                m_KeyChord[index] = Bui::CKKeyToImGuiKey(property->GetKey());
                if (Bui::KeyButton(property->GetName(), &m_KeyToggled[index], &m_KeyChord[index])) {
                    m_KeyChord[index] &= ~ImGuiMod_Mask_;
                    property->SetKey(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(m_KeyChord[index])));
                }
                break;
            case IProperty::FLOAT:
                if (m_FloatFlags[index] == 0) {
                    m_FloatValues[index] = property->GetFloat();
                    m_FloatFlags[index] = 1;
                }
                if (Bui::InputFloatButton(property->GetName(), &m_FloatValues[index])) {
                    if (fabs(m_FloatValues[index] - property->GetFloat()) > EPSILON) {
                        m_FloatFlags[index] = 2;
                    }
                }
                if (ImGui::IsItemDeactivatedAfterEdit() && m_FloatFlags[index] == 2) {
                    property->SetFloat(m_FloatValues[index]);
                    m_FloatFlags[index] = 1;
                }
                break;
            default:
                ImGui::Dummy(Bui::GetButtonSize(Bui::BUTTON_OPTION));
                break;
        }

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(property);
        }

        return true;
    });
}

void ModOptionPage::OnClose() {
    memset(m_IntFlags, 0, sizeof(m_IntFlags));
    memset(m_FloatFlags, 0, sizeof(m_FloatFlags));
    ModMenuPage::OnClose();
}

void ModOptionPage::OnPageChanged(int newPage, int oldPage) {
    FlushBuffers();
}

void ModOptionPage::FlushBuffers() {
    memset(m_Buffers, 0, sizeof(m_Buffers));
    memset(m_BufferHashes, 0, sizeof(m_BufferHashes));
    memset(m_KeyToggled, 0, sizeof(m_KeyToggled));
    memset(m_KeyChord, 0, sizeof(m_KeyChord));
    memset(m_IntFlags, 0, sizeof(m_IntFlags));
    memset(m_FloatFlags, 0, sizeof(m_FloatFlags));
    memset(m_IntValues, 0, sizeof(m_IntValues));
    memset(m_FloatValues, 0, sizeof(m_FloatValues));
}

void ModOptionPage::ShowCommentBox(const Property *property) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.35f);
    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.3f);
    ImGui::SetCursorScreenPos(commentBoxPos);
    ImGui::BeginChild("ModOptionComment", commentBoxSize);

    WrappedText(property->GetName(), commentBoxSize.x);
    WrappedText(property->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}