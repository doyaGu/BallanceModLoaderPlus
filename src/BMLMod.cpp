#include "BMLMod.h"

#include <map>
#include <algorithm>

#include "BML/Gui.h"
#include "BML/InputHook.h"
#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"
#include "ModManager.h"
#include "Commands.h"
#include "Config.h"

namespace ExecuteBB {
    void Init();
    void InitFont(FontType type, int fontIndex);
}

using namespace ScriptHelper;

GuiList::GuiList() : m_CurPage(0), m_MaxPage(0), m_Size(0), m_MaxSize(0) {
    m_Left = AddLeftButton("M_List_Left", 0.12f, 0.36f, [this]() { PreviousPage(); });
    m_Right = AddRightButton("M_List_Right", 0.12f, 0.6038f, [this]() { NextPage(); });
    AddBackButton("M_Opt_Mods_Back")->SetCallback([this]() { Exit(); });
}

void GuiList::Init(int size, int maxsize) {
    m_Size = size;
    m_MaxPage = (size + maxsize - 1) / maxsize;
    m_MaxSize = maxsize;
    m_CurPage = 0;

    for (int i = 0; i < m_Size; i++)
        m_GuiList.push_back(CreateSubGui(i));
    for (int i = 0; i < m_MaxSize; i++) {
        BGui::Button *button = CreateButton(i);
        if (m_Size > 0 && m_GuiList[0] != nullptr) {
            button->SetCallback([this, i]() {
                BGui::Gui *gui = m_GuiList[m_MaxSize * m_CurPage + i];
                BML_GetModManager()->GetBMLMod()->ShowGui(gui);
            });
        }
        m_Buttons.push_back(button);
    }
}

void GuiList::SetPage(int page) {
    int size = (std::min)(m_MaxSize, m_Size - page * m_MaxSize);
    for (int i = 0; i < m_MaxSize; i++)
        m_Buttons[i]->SetVisible(i < size);
    for (int i = 0; i < size; i++)
        m_Buttons[i]->SetText(GetButtonText(page * m_MaxSize + i).c_str());

    m_CurPage = page;
    m_Left->SetVisible(page > 0);
    m_Right->SetVisible(page < m_MaxPage - 1);
}

void GuiList::Exit() {
    BML_GetModManager()->GetBMLMod()->ShowGui(GetParentGui());
}

void GuiList::SetVisible(bool visible) {
    Gui::SetVisible(visible);
    if (visible)
        SetPage(m_CurPage);
}

GuiModOption::GuiModOption() {
    Init(BML_GetModManager()->GetModCount(), 4);

    AddTextLabel("M_Opt_Mods_Title", "Mod List", ExecuteBB::GAMEFONT_02, 0.35f, 0.1f, 0.3f, 0.1f);
}

BGui::Button *GuiModOption::CreateButton(int index) {
    return AddSettingButton(("M_Opt_Mods_" + std::to_string(index)).c_str(), "", 0.25f + 0.13f * index);
}

std::string GuiModOption::GetButtonText(int index) {
    return BML_GetModManager()->GetMod(index)->GetID();
}

BGui::Gui *GuiModOption::CreateSubGui(int index) {
    return new GuiModMenu(BML_GetModManager()->GetMod(index));
}

BGui::Gui *GuiModOption::GetParentGui() {
    return nullptr;
}

void GuiModOption::Exit() {
    GuiList::Exit();
    BML_GetModManager()->GetCKContext()->GetCurrentScene()->Activate(
        BML_GetModManager()->GetScriptByName("Menu_Options"),
        true);
}

GuiModMenu::GuiModMenu(IMod *mod) : GuiList() {
    m_Left->SetPosition(Vx2DVector(0.36f, 0.3f));
    m_Right->SetPosition(Vx2DVector(0.6038f, 0.3f));
    AddTextLabel("M_Opt_ModMenu_Name", mod->GetName(), ExecuteBB::GAMEFONT_01, 0.35f, 0.1f, 0.3f, 0.05f);
    AddTextLabel("M_Opt_ModMenu_Author", (std::string("by ") + mod->GetAuthor()).c_str(), ExecuteBB::GAMEFONT_03, 0.35f, 0.13f, 0.3f, 0.04f);
    AddTextLabel("M_Opt_ModMenu_Version", (std::string("v") + mod->GetVersion()).c_str(), ExecuteBB::GAMEFONT_03, 0.35f, 0.15f, 0.3f, 0.04f);
    BGui::Label *desc = AddTextLabel("M_Opt_ModMenu_Description", mod->GetDescription(), ExecuteBB::GAMEFONT_03, 0.35f, 0.20f, 0.3f, 0.1f);
    desc->SetTextFlags(TEXT_SCREEN | TEXT_WORDWRAP);
    desc->SetAlignment(ALIGN_TOP);
    m_CommentBackground = AddPanel("M_Opt_ModMenu_Comment_Bg", VxColor(0, 0, 0, 110), 0.725f, 0.4f, 0.25f, 0.2f);
    m_Comment = AddTextLabel("M_Opt_ModMenu_Comment", "", ExecuteBB::GAMEFONT_03, 0.725f, 0.4f, 0.25f, 0.2f);
    m_Comment->SetTextFlags(TEXT_SCREEN | TEXT_WORDWRAP);
    m_Comment->SetAlignment(ALIGN_TOP);

    m_Config = BML_GetModManager()->GetConfig(mod);
    if (m_Config) {
        AddTextLabel("M_Opt_ModMenu_Title", "Mod Options", ExecuteBB::GAMEFONT_01, 0.35f, 0.4f, 0.3f, 0.05f);
        const size_t count = m_Config->GetCategoryCount();
        for (size_t i = 0; i < count; ++i)
            m_Categories.emplace_back(m_Config->GetCategory(i)->GetName());
    }

    Init((int)m_Categories.size(), 6);
    GuiModMenu::SetVisible(false);
}

void GuiModMenu::Process() {
    bool show_cmt = false;
    if (m_CurPage >= 0 && m_CurPage < m_MaxPage) {
        CKRenderContext *rc = BML_GetModManager()->GetRenderContext();
        InputHook *input = BML_GetModManager()->GetInputManager();
        Vx2DVector mousePos;
        input->GetMousePosition(mousePos, false);
        int size = (std::min)(m_MaxSize, m_Size - m_CurPage * m_MaxSize);
        for (int i = 0; i < size; i++) {
            if (Intersect(mousePos.x / (float)rc->GetWidth(), mousePos.y / (float)rc->GetHeight(), m_Buttons[i])) {
                if (m_CurComment != i) {
                    m_CommentBackground->SetVisible(true);
                    m_Comment->SetVisible(true);
                    m_Comment->SetText(m_Config->GetCategory(i + m_CurPage * m_MaxSize)->GetComment());
                    m_CurComment = i;
                }

                show_cmt = true;
                break;
            }
        }
    }

    if (!show_cmt) {
        if (m_CurComment >= 0) {
            m_CommentBackground->SetVisible(false);
            m_Comment->SetVisible(false);
            m_CurComment = -1;
        }
    }

    GuiList::Process();
}

void GuiModMenu::SetVisible(bool visible) {
    GuiList::SetVisible(visible);
    if (visible) {
        m_CommentBackground->SetVisible(false);
        m_Comment->SetVisible(false);
        m_CurComment = -1;
    }
}

BGui::Button *GuiModMenu::CreateButton(int index) {
    BGui::Button *button = AddLevelButton(("M_Opt_ModMenu_" + std::to_string(index)).c_str(), "", 0.45f + 0.06f * index);
    button->SetFont(ExecuteBB::GAMEFONT_03);
    return button;
}

std::string GuiModMenu::GetButtonText(int index) {
    return m_Categories[index];
}

BGui::Gui *GuiModMenu::CreateSubGui(int index) {
    return new GuiModCategory(this, m_Config, m_Categories[index]);
}

BGui::Gui *GuiModMenu::GetParentGui() {
    return BML_GetModManager()->GetBMLMod()->m_ModOption;
}

GuiCustomMap::GuiCustomMap(BMLMod *mod) : GuiList(), m_BMLMod(mod) {
    m_Left->SetPosition(Vx2DVector(0.34f, 0.4f));
    m_Right->SetPosition(Vx2DVector(0.6238f, 0.4f));

    AddTextLabel("M_Opt_Mods_Title", "Custom Maps", ExecuteBB::GAMEFONT_02, 0.35f, 0.07f, 0.3f, 0.1f);

    CKDirectoryParser mapParser("..\\ModLoader\\Maps", "*.nmo", TRUE);
    char *mapPath = mapParser.GetNextFile();
    while (mapPath) {
        MapInfo info;

        info.DisplayName = CKPathSplitter(mapPath).GetName();
        info.SearchName = info.DisplayName;
        info.FilePath = "..\\ModLoader\\Maps\\" + info.DisplayName + ".nmo";

        std::transform(info.SearchName.begin(), info.SearchName.end(), info.SearchName.begin(), tolower);
        m_Maps.push_back(info);
        mapPath = mapParser.GetNextFile();
    }

    m_Exit = AddLeftButton("M_Exit_Custom_Maps", 0.4f, 0.34f, [this]() {
        GuiList::Exit();
        BML_GetCKContext()->GetCurrentScene()->Activate(BML_GetScriptByName("Menu_Start"), true);
    });

    AddPanel("M_Map_Search_Bg", VxColor(0, 0, 0, 110), 0.4f, 0.18f, 0.2f, 0.03f);
    m_SearchBar = AddTextInput("M_Search_Map", ExecuteBB::GAMEFONT_03, 0.4f, 0.18f, 0.2f, 0.03f, [this](CKDWORD) {
        m_SearchRes.clear();
        std::string text = m_SearchBar->GetText();
        std::transform(text.begin(), text.end(), text.begin(), tolower);

        for (auto &p: m_Maps) {
            if (text.length() == 0 || p.SearchName.find(text) != std::string::npos) {
                m_SearchRes.push_back(&p);
            }
        }
        m_Size = m_SearchRes.size();
        m_MaxPage = (m_Size + m_MaxSize - 1) / m_MaxSize;
        SetPage(0);
    });

    std::sort(m_Maps.begin(), m_Maps.end());
    for (auto &p: m_Maps)
        m_SearchRes.push_back(&p);
    Init(m_SearchRes.size(), 10);
    SetVisible(false);
}

BGui::Button *GuiCustomMap::CreateButton(int index) {
    m_Texts.push_back(
        AddText(("M_Opt_ModMenu_" + std::to_string(index)).c_str(), "", 0.44f, 0.23f + 0.06f * index, 0.3f, 0.05f));
    return AddLevelButton(("M_Opt_ModMenu_" + std::to_string(index)).c_str(), "", 0.23f + 0.06f * index, 0.4031f,
                          [this, index]() {
                              GuiList::Exit();
                              SetParamString(m_BMLMod->m_MapFile, m_SearchRes[m_CurPage * m_MaxSize + index]->FilePath.c_str());
                              SetParamValue(m_BMLMod->m_LoadCustom, TRUE);
                              int level = m_BMLMod->GetConfig()->GetProperty("Misc", "CustomMapNumber")->GetInteger();
                              level = (level >= 1 && level <= 13) ? level : rand() % 10 + 2;
                              m_BMLMod->m_CurLevel->SetElementValue(0, 0, &level);
                              level--;
                              SetParamValue(m_BMLMod->m_LevelRow, level);

                              CKMessageManager *mm = m_BMLMod->m_CKContext->GetMessageManager();
                              CKMessageType loadLevel = mm->AddMessageType("Load Level");
                              CKMessageType loadMenu = mm->AddMessageType("Menu_Load");

                              mm->SendMessageSingle(loadLevel, m_BMLMod->m_CKContext->GetCurrentLevel());
                              mm->SendMessageSingle(loadMenu, BML_GetGroupByName("All_Sound"));
                              BML_Get2dEntityByName("M_BlackScreen")->Show(CKHIDE);
                              m_BMLMod->m_ExitStart->ActivateInput(0);
                              m_BMLMod->m_ExitStart->Activate();
                          });
}

std::string GuiCustomMap::GetButtonText(int index) {
    return "";
}

BGui::Gui *GuiCustomMap::CreateSubGui(int index) {
    return nullptr;
}

BGui::Gui *GuiCustomMap::GetParentGui() {
    return nullptr;
}

void GuiCustomMap::SetPage(int page) {
    GuiList::SetPage(page);
    int size = (std::min)(m_MaxSize, m_Size - page * m_MaxSize);
    for (int i = 0; i < m_MaxSize; i++)
        m_Texts[i]->SetVisible(i < size);
    for (int i = 0; i < size; i++)
        m_Texts[i]->SetText(m_SearchRes[page * m_MaxSize + i]->DisplayName.c_str());
    m_BMLMod->m_BML->AddTimer(1ul, [this, page]() { m_Exit->SetVisible(page == 0); });
}

void GuiCustomMap::Exit() {
    GuiList::Exit();
    BML_GetCKContext()->GetCurrentScene()->Activate(
        BML_GetScriptByName("Menu_Main"),
        true);
}

GuiModCategory::GuiModCategory(GuiModMenu *parent, Config *config, const std::string &category) {
    auto *cate = config->GetCategory(category.c_str());
    const size_t count = cate->GetPropertyCount();
    for (size_t i = 0; i < count; ++i) {
        auto *prop = cate->GetProperty(i);
        auto *newprop = new Property(nullptr, category, prop->GetName());
        newprop->CopyValue(prop);
        newprop->SetComment(prop->GetComment());
        m_Data.push_back(newprop);
    }

    m_Size = (int)m_Data.size();
    m_CurPage = 0;

    m_Parent = parent;
    m_Config = config;
    m_Category = category;

    AddTextLabel("M_Opt_Category_Title", category.c_str(), ExecuteBB::GAMEFONT_02, 0.35f, 0.1f, 0.3f, 0.1f);
    m_Left = AddLeftButton("M_List_Left", 0.12f, 0.35f, [this]() { PreviousPage(); });
    m_Right = AddRightButton("M_List_Right", 0.12f, 0.6138f, [this]() { NextPage(); });
    AddBackButton("M_Opt_Category_Back")->SetCallback([this]() { SaveAndExit(); });
    m_Exit = AddBackButton("M_Opt_Category_Back");
    m_Exit->SetCallback([this]() { Exit(); });
    m_CommentBackground = AddPanel("M_Opt_Comment_Bg", VxColor(0, 0, 0, 110), 0.725f, 0.4f, 0.25f, 0.2f);
    m_Comment = AddTextLabel("M_Opt_Comment", "", ExecuteBB::GAMEFONT_03, 0.725f, 0.4f, 0.25f, 0.2f);
    m_Comment->SetTextFlags(TEXT_SCREEN | TEXT_WORDWRAP);
    m_Comment->SetAlignment(ALIGN_TOP);

    Vx2DVector offset(0.0f, BML_GetRenderContext()->GetHeight() * 0.015f);

    float cnt = 0.25f, page = 0;
    std::vector<BGui::Element *> elements;
    std::vector<std::pair<Property *, BGui::Element *>> comments;
    for (Property *prop: m_Data) {
        const char *name = prop->GetName();
        switch (prop->GetType()) {
            case IProperty::STRING: {
                BGui::Button *bg = AddSettingButton(name, name, cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                BGui::Input *input = AddTextInput(name, ExecuteBB::GAMEFONT_03, 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                input->SetText(prop->GetString());
                input->SetCallback([prop, input](CKDWORD) { prop->SetString(input->GetText()); });
                elements.push_back(input);
                BGui::Panel *panel = AddPanel(name, VxColor(0, 0, 0, 110), 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                elements.push_back(panel);
                m_Inputs.emplace_back(input, nullptr);
                comments.emplace_back(prop, bg);
                cnt += 0.12f;
                break;
            }
            case IProperty::INTEGER: {
                BGui::Button *bg = AddSettingButton(name, name, cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                BGui::Input *input = AddTextInput(name, ExecuteBB::GAMEFONT_03, 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                input->SetText(std::to_string(prop->GetInteger()).c_str());
                input->SetCallback([prop, input](CKDWORD) { prop->SetInteger(atoi(input->GetText())); });
                elements.push_back(input);
                BGui::Panel *panel = AddPanel(name, VxColor(0, 0, 0, 110), 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                elements.push_back(panel);
                m_Inputs.emplace_back(input, nullptr);
                comments.emplace_back(prop, bg);
                cnt += 0.12f;
                break;
            }
            case IProperty::FLOAT: {
                BGui::Button *bg = AddSettingButton(name, name, cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                BGui::Input *input = AddTextInput(name, ExecuteBB::GAMEFONT_03, 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                std::ostringstream stream;
                stream << prop->GetFloat();
                input->SetText(stream.str().c_str());
                input->SetCallback([prop, input](CKDWORD) { prop->SetFloat((float) atof(input->GetText())); });
                elements.push_back(input);
                BGui::Panel *panel = AddPanel(name, VxColor(0, 0, 0, 110), 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                elements.push_back(panel);
                m_Inputs.emplace_back(input, nullptr);
                comments.emplace_back(prop, bg);
                cnt += 0.12f;
                break;
            }
            case IProperty::KEY: {
                std::pair<BGui::Button *, BGui::KeyInput *> key = AddKeyButton(name, name, cnt);
                key.second->SetKey(prop->GetKey());
                key.second->SetCallback([prop](CKDWORD key) { prop->SetKey(CKKEYBOARD(key)); });
                elements.push_back(key.first);
                elements.push_back(key.second);
                m_Inputs.emplace_back(key.second, nullptr);
                comments.emplace_back(prop, key.first);
                cnt += 0.06f;
                break;
            }
            case IProperty::BOOLEAN: {
                BGui::Button *bg = AddSettingButton(name, name, cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                auto yesno = AddYesNoButton(name, cnt + 0.043f, 0.4350f, 0.5200f, [prop](bool value) { prop->SetBoolean(value); });
                yesno.first->SetActive(prop->GetBoolean());
                yesno.second->SetActive(!prop->GetBoolean());
                elements.push_back(yesno.first);
                elements.push_back(yesno.second);
                m_Inputs.emplace_back(yesno);
                comments.emplace_back(prop, bg);
                cnt += 0.12f;
                break;
            }
            case IProperty::NONE:
                break;
        }

        if (cnt > 0.7f) {
            cnt = 0.25f;
            page++;
            m_Elements.push_back(elements);
            elements.clear();
            m_Comments.push_back(comments);
            comments.clear();
        }
    }

    if (cnt > 0.25f) {
        m_Elements.push_back(elements);
        m_Comments.push_back(comments);
    }

    m_MaxPage = (int)m_Elements.size();

    GuiModCategory::SetVisible(false);
}

void GuiModCategory::Process() {
    bool show_cmt = false;
    if (m_CurPage >= 0 && m_CurPage < (int) m_Comments.size()) {
        CKRenderContext *rc = BML_GetRenderContext();
        InputHook *input = BML_GetInputHook();
        Vx2DVector mousePos;
        input->GetMousePosition(mousePos, false);
        for (auto &pair: m_Comments[m_CurPage]) {
            if (Intersect(mousePos.x / rc->GetWidth(), mousePos.y / rc->GetHeight(), pair.second)) {
                if (m_CurComment != pair.first) {
                    m_CommentBackground->SetVisible(true);
                    m_Comment->SetVisible(true);
                    m_Comment->SetText(pair.first->GetComment());
                    m_CurComment = pair.first;
                }

                show_cmt = true;
                break;
            }
        }
    }

    if (!show_cmt) {
        if (m_CurComment != nullptr) {
            m_CommentBackground->SetVisible(false);
            m_Comment->SetVisible(false);
            m_CurComment = nullptr;
        }
    }

    Gui::Process();
}

void GuiModCategory::SetVisible(bool visible) {
    Gui::SetVisible(visible);
    if (visible) {
        auto *cate = m_Config->GetCategory(m_Category.c_str());
        const size_t count = cate->GetPropertyCount();
        for (size_t i = 0; i < count; i++) {
            Property *prop = cate->GetProperty(i);
            m_Data[i]->CopyValue(prop);
            std::pair<BGui::Element *, BGui::Element *> input = m_Inputs[i];
            switch (prop->GetType()) {
                case IProperty::STRING:
                    reinterpret_cast<BGui::Input *>(input.first)->SetText(prop->GetString());
                    break;
                case IProperty::INTEGER:
                    reinterpret_cast<BGui::Input *>(input.first)->SetText(std::to_string(prop->GetInteger()).c_str());
                    break;
                case IProperty::FLOAT: {
                    std::ostringstream stream;
                    stream << prop->GetFloat();
                    reinterpret_cast<BGui::Input *>(input.first)->SetText(stream.str().c_str());
                    break;
                }
                case IProperty::KEY:
                    reinterpret_cast<BGui::KeyInput *>(input.first)->SetKey(prop->GetKey());
                    break;
                case IProperty::BOOLEAN: {
                    reinterpret_cast<BGui::Button *>(input.first)->SetActive(prop->GetBoolean());
                    reinterpret_cast<BGui::Button *>(input.second)->SetActive(!prop->GetBoolean());
                    break;
                }
                case IProperty::NONE:
                    break;
            }
        }

        m_CommentBackground->SetVisible(false);
        m_Comment->SetVisible(false);
        m_CurComment = nullptr;

        SetPage(m_CurPage);
    }
}

void GuiModCategory::SetPage(int page) {
    for (auto &p: m_Elements)
        for (auto &e: p)
            e->SetVisible(false);

    for (auto &e: m_Elements[page])
        e->SetVisible(true);

    m_CurPage = page;
    m_Left->SetVisible(page > 0);
    m_Right->SetVisible(page < m_MaxPage - 1);
    m_Exit->SetVisible(false);
}

void GuiModCategory::SaveAndExit() {
    auto *cate = m_Config->GetCategory(m_Category.c_str());
    for (Property *p: m_Data)
        cate->GetProperty(p->GetName())->CopyValue(p);
    BML_GetModManager()->SaveConfig(m_Config);
    Exit();
}

void GuiModCategory::Exit() {
    BML_GetModManager()->GetBMLMod()->ShowGui(m_Parent);
}

void BMLMod::OnLoad() {
    m_CKContext = m_BML->GetCKContext();
    m_RenderContext = m_BML->GetRenderContext();
    m_TimeManager = m_BML->GetTimeManager();
    m_InputHook = m_BML->GetInputManager();

    m_RenderContext->Get2dRoot(TRUE)->GetRect(m_WindowRect);

    ExecuteBB::Init();

    InitConfigs();
    RegisterCommands();
}

void BMLMod::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                          CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                          XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Menu.nmo")) {
        BGui::Gui::InitMaterials();

        GetLogger()->Info("Create Command Gui");
        m_CmdBar = new BGui::Gui();
        m_CmdBar->AddPanel("M_Cmd_Bg", VxColor(0, 0, 0, 110), 0.02f, 0.94f, 0.96f, 0.025f)->SetZOrder(100);
        m_CmdInput = m_CmdBar->AddTextInput("M_Cmd_Text", ExecuteBB::GAMEFONT_03, 0.02f, 0.94f, 0.96f, 0.025f, [this](CKDWORD key) { OnCmdEdit(key); });
        m_CmdInput->SetAlignment(ALIGN_LEFT);
        m_CmdInput->SetTextFlags(TEXT_SCREEN | TEXT_SHOWCARET);
        m_CmdInput->SetZOrder(110);
        m_CmdBar->SetCanBeBlocked(false);
        m_CmdBar->SetVisible(false);

        GetLogger()->Info("Create Console Gui");
        m_MsgLog = new BGui::Gui();
        for (int i = 0; i < MSG_MAXSIZE; i++) {
            m_Msgs[i].m_Background = m_MsgLog->AddPanel((std::string("M_Cmd_Log_Bg_") + std::to_string(i + 1)).c_str(),
                                                        VxColor(0, 0, 0, 110), 0.02f, 0.9f - i * 0.025f, 0.96f, 0.025f);
            m_Msgs[i].m_Background->SetVisible(false);
            m_Msgs[i].m_Background->SetZOrder(100);
            m_Msgs[i].m_Text = m_MsgLog->AddTextLabel((std::string("M_Cmd_Log_Text_") + std::to_string(i + 1)).c_str(),
                                                      "", ExecuteBB::GAMEFONT_03, 0.02f, 0.9f - i * 0.025f, 0.96f, 0.025f);
            m_Msgs[i].m_Text->SetVisible(false);
            m_Msgs[i].m_Text->SetAlignment(ALIGN_LEFT);
            m_Msgs[i].m_Text->SetZOrder(110);
            m_Msgs[i].m_Timer = 0;
        }

        GetLogger()->Info("Create BML Gui");
        m_IngameBanner = new BGui::Gui();
        m_Title = m_IngameBanner->AddTextLabel("M_Use_BML", "BML Plus " BML_VERSION, ExecuteBB::GAMEFONT_01, 0, 0, 1, 0.03f);
        m_Title->SetVisible(m_ShowTitle->GetBoolean());
        m_Cheat = m_IngameBanner->AddTextLabel("M_Use_Cheat", "Cheat Mode Enabled", ExecuteBB::GAMEFONT_01, 0, 0.85f, 1, 0.05f);
        m_FPS = m_IngameBanner->AddTextLabel("M_Show_Fps", "", ExecuteBB::GAMEFONT_01, 0, 0, 0.2f, 0.03f);
        m_SRTitle = m_IngameBanner->AddTextLabel("M_Time_Counter_Title", "SR Timer", ExecuteBB::GAMEFONT_01, 0.03f, 0.8f, 0.2f, 0.03f);
        m_SRScore = m_IngameBanner->AddTextLabel("M_Time_Counter", "", ExecuteBB::GAMEFONT_01, 0.05f, 0.83f, 0.2f, 0.03f);
        m_FPS->SetAlignment(ALIGN_LEFT);
        m_FPS->SetVisible(m_ShowFPS->GetBoolean());
        m_SRTitle->SetAlignment(ALIGN_LEFT);
        m_SRTitle->SetVisible(false);
        m_SRScore->SetAlignment(ALIGN_LEFT);
        m_SRScore->SetVisible(false);
        m_Cheat->SetVisible(false);
        m_CustomMaps = m_IngameBanner->AddRightButton("M_Enter_Custom_Maps", 0.4f, 0.6238f, [this]() {
            m_ExitStart->ActivateInput(0);
            m_ExitStart->Activate();
            ShowGui(m_MapsGui);
        });

        GetLogger()->Info("Create Mod Options Gui");
        m_ModOption = new GuiModOption();
        m_ModOption->SetVisible(false);

        m_MapsGui = new GuiCustomMap(this);
        m_Level01 = m_BML->Get2dEntityByName("M_Start_But_01");
        CKBehavior *menuMain = m_BML->GetScriptByName("Menu_Start");
        m_ExitStart = FindFirstBB(menuMain, "Exit");
    }

    if (!strcmp(filename, "3D Entities\\MenuLevel.nmo")) {
        if (m_AdaptiveCamera->GetBoolean()) {
            GetLogger()->Info("Adjust MenuLevel Camera");
            CKCamera *cam = m_BML->GetTargetCameraByName("Cam_MenuLevel");
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            m_BML->SetIC(cam);
        }
    }

    if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
        if (m_AdaptiveCamera->GetBoolean()) {
            GetLogger()->Info("Adjust Ingame Camera");
            CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            m_BML->SetIC(cam);
        }
    }
}

void BMLMod::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Event_handler"))
        OnEditScript_Base_EventHandler(script);

    if (!strcmp(script->GetName(), "Menu_Init"))
        OnEditScript_Menu_MenuInit(script);

    if (!strcmp(script->GetName(), "Menu_Options"))
        OnEditScript_Menu_OptionsMenu(script);

    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);

    if (!strcmp(script->GetName(), "Gameplay_Energy"))
        OnEditScript_Gameplay_Energy(script);

    if (!strcmp(script->GetName(), "Gameplay_Events"))
        OnEditScript_Gameplay_Events(script);

    if (!strcmp(script->GetName(), "Levelinit_build"))
        OnEditScript_Levelinit_build(script);

    if (m_FixLifeBall) {
        if (!strcmp(script->GetName(), "P_Extra_Life_Particle_Blob Script") ||
            !strcmp(script->GetName(), "P_Extra_Life_Particle_Fizz Script"))
            OnEditScript_ExtraLife_Fix(script);
    }
}

void BMLMod::OnProcess() {
    m_OldWindowRect = m_WindowRect;
    m_RenderContext->Get2dRoot(TRUE)->GetRect(m_WindowRect);
    if (m_WindowRect != m_OldWindowRect) {
        OnResize();
    }

    if (m_IngameBanner) {
        OnProcess_FpsDisplay();
    }

    if (m_CmdBar) {
        OnProcess_CommandBar();
    }

    if (m_SRActivated) {
        OnProcess_SRTimer();
    }

    if (m_Level01 && m_MapsGui) {
        bool inStart = m_Level01->IsVisible();
        m_CustomMaps->SetVisible(inStart);
    }
}

void BMLMod::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (prop == m_UnlockFPS) {
        if (prop->GetBoolean()) {
            AdjustFrameRate(false, 0);
        } else {
            int val = m_FPSLimit->GetInteger();
            if (val > 0)
                AdjustFrameRate(false, static_cast<float>(val));
            else
                AdjustFrameRate(true);
        }
    } else if (prop == m_FPSLimit && !m_UnlockFPS->GetBoolean()) {
        int val = prop->GetInteger();
        if (val > 0)
            AdjustFrameRate(false, static_cast<float>(val));
        else
            AdjustFrameRate(true);
    } else if (prop == m_ShowTitle) {
        m_Title->SetVisible(prop->GetBoolean());
    } else if (prop == m_ShowFPS) {
        m_FPS->SetVisible(prop->GetBoolean());
    } else if (prop == m_ShowSR && m_BML->IsIngame()) {
        m_SRScore->SetVisible(m_ShowSR->GetBoolean());
        m_SRTitle->SetVisible(m_ShowSR->GetBoolean());
    } else if (prop == m_MsgDuration) {
        m_MsgMaxTimer = m_MsgDuration->GetFloat() * 1000;
        if (m_MsgMaxTimer < 2000) {
            m_MsgDuration->SetFloat(2.0f);
        }
    }
}

void BMLMod::OnPreStartMenu() {
    if (m_UnlockFPS->GetBoolean()) {
        AdjustFrameRate(false, 0);
    } else {
        int val = m_FPSLimit->GetInteger();
        if (val > 0)
            AdjustFrameRate(false, static_cast<float>(val));
        else
            AdjustFrameRate(true);
    }
}

void BMLMod::OnExitGame() {
    m_Level01 = nullptr;
}

void BMLMod::OnStartLevel() {
    if (m_UnlockFPS->GetBoolean()) {
        AdjustFrameRate(false, 0);
    } else {
        int val = m_FPSLimit->GetInteger();
        if (val > 0)
            AdjustFrameRate(false, static_cast<float>(val));
        else
            AdjustFrameRate(true);
    }
    m_SRTimer = 0.0f;
    m_SRScore->SetText("00:00:00.000");
    if (m_ShowSR->GetBoolean()) {
        m_SRScore->SetVisible(true);
        m_SRTitle->SetVisible(true);
    }
    SetParamValue(m_LoadCustom, FALSE);
}

void BMLMod::OnPostExitLevel() {
    m_SRScore->SetVisible(false);
    m_SRTitle->SetVisible(false);
}

void BMLMod::OnPauseLevel() {
    m_SRActivated = false;
}

void BMLMod::OnUnpauseLevel() {
    m_SRActivated = true;
}

void BMLMod::OnCounterActive() {
    m_SRActivated = true;
}

void BMLMod::OnCounterInactive() {
    m_SRActivated = false;
}

void BMLMod::AddIngameMessage(const char *msg) {
    for (int i = std::min(MSG_MAXSIZE - 1, m_MsgCount) - 1; i >= 0; i--) {
        const char *text = m_Msgs[i].m_Text->GetText();
        m_Msgs[i + 1].m_Text->SetText(text);
        m_Msgs[i + 1].m_Timer = m_Msgs[i].m_Timer;
    }

    m_Msgs[0].m_Text->SetText(msg);
    m_Msgs[0].m_Timer = m_MsgMaxTimer;
    m_MsgCount++;

    GetLogger()->Info(msg);
}

void BMLMod::ClearIngameMessages() {
    m_MsgCount = 0;
    for (int i = 0; i < MSG_MAXSIZE; i++) {
        m_Msgs[i].m_Background->SetVisible(false);
        m_Msgs[i].m_Text->SetVisible(false);
        m_Msgs[i].m_Text->SetText("");
        m_Msgs[i].m_Timer = 0;
    }
}

void BMLMod::ShowCheatBanner(bool show) {
    m_Cheat->SetVisible(show);
}

void BMLMod::ShowModOptions() {
    ShowGui(m_ModOption);
    m_ModOption->SetPage(0);
}

void BMLMod::ShowGui(BGui::Gui *gui) {
    if (m_CurGui != nullptr)
        CloseCurrentGui();
    m_BML->AddTimer(1ul, [this, gui]() {
        m_CurGui = gui;
        if (gui) gui->SetVisible(true);
    });
}

void BMLMod::CloseCurrentGui() {
    m_CurGui->SetVisible(false);
    m_CurGui = nullptr;
}

int BMLMod::GetHSScore() {
    int points, lifes;
    CKDataArray *energy = m_BML->GetArrayByName("Energy");
    energy->GetElementValue(0, 0, &points);
    energy->GetElementValue(0, 1, &lifes);
    return points + lifes * 200;
}

void BMLMod::AdjustFrameRate(bool sync, float limit) {
    if (sync) {
        m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_SYNC);
    } else if (limit > 0) {
        m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_LIMIT);
        m_TimeManager->SetFrameRateLimit(limit);
    } else {
        m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_FREE);
    }
}

int BMLMod::GetHUD() {
    int code = 0;
    if (m_ShowTitle->GetBoolean()) {
        code |= HUD_TITLE;
    }
    if (m_ShowFPS->GetBoolean()) {
        code |= HUD_FPS;
    }
    if (m_ShowSR->GetBoolean()) {
        code |= HUD_SR;
    }
    return code;
}

void BMLMod::SetHUD(int mode) {
    m_ShowTitle->SetBoolean((mode & HUD_TITLE) != 0);
    m_ShowFPS->SetBoolean((mode & HUD_FPS) != 0);
    m_ShowSR->SetBoolean((mode & HUD_SR) != 0);
}

void BMLMod::InitConfigs() {
    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");
    m_UnlockFPS = GetConfig()->GetProperty("Misc", "UnlockFrameRate");
    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(true);

    m_FPSLimit = GetConfig()->GetProperty("Misc", "SetMaxFrameRate");
    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync.");
    m_FPSLimit->SetDefaultInteger(0);

    m_AdaptiveCamera = GetConfig()->GetProperty("Misc", "AdaptiveCamera");
    m_AdaptiveCamera->SetComment("Adjust cameras on screen mode changed");
    m_AdaptiveCamera->SetDefaultBoolean(true);

    m_ShowTitle = GetConfig()->GetProperty("Misc", "ShowTitle");
    m_ShowTitle->SetComment("Show BML Title at top");
    m_ShowTitle->SetDefaultBoolean(true);

    m_ShowFPS = GetConfig()->GetProperty("Misc", "ShowFPS");
    m_ShowFPS->SetComment("Show FPS at top-left corner");
    m_ShowFPS->SetDefaultBoolean(true);

    m_ShowSR = GetConfig()->GetProperty("Misc", "ShowSRTimer");
    m_ShowSR->SetComment("Show SR Timer above Time Score");
    m_ShowSR->SetDefaultBoolean(true);

    m_FixLifeBall = GetConfig()->GetProperty("Misc", "FixLifeBallFreeze");
    m_FixLifeBall->SetComment("Game won't freeze when picking up life balls");
    m_FixLifeBall->SetDefaultBoolean(true);

    m_MsgDuration = GetConfig()->GetProperty("Misc", "MessageDuration");
    m_MsgDuration->SetComment("Maximum visible time of each notification message, measured in seconds (default: 6)");
    m_MsgDuration->SetDefaultFloat(m_MsgMaxTimer / 1000);
    m_MsgMaxTimer = m_MsgDuration->GetFloat() * 1000;

    m_CustomMapNumber = GetConfig()->GetProperty("Misc", "CustomMapNumber");
    m_CustomMapNumber->SetComment("Level number to use for custom maps (affects level bonus and sky textures)."
                                  " Must be in the range of 1~13; 0 to randomly select one between 2 and 11");
    m_CustomMapNumber->SetDefaultInteger(0);
}

void BMLMod::RegisterCommands() {
    m_BML->RegisterCommand(new CommandBML());
    m_BML->RegisterCommand(new CommandHelp());
    m_BML->RegisterCommand(new CommandExit());
    m_BML->RegisterCommand(new CommandEcho());
    m_BML->RegisterCommand(new CommandCheat());
    m_BML->RegisterCommand(new CommandClear(this));
    m_BML->RegisterCommand(new CommandHUD(this));
}

void BMLMod::OnEditScript_Base_EventHandler(CKBehavior *script) {
    CKBehavior *som = FindFirstBB(script, "Switch On Message", false, 2, 11, 11, 0);

    GetLogger()->Info("Insert message Start Menu Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnPreStartMenu();
                 return CKBR_OK;
             }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 0, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPostStartMenu();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Exit Game Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnExitGame();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Load Level Hook");
    CKBehaviorLink *link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 2, 0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreLoadLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 2, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPostLoadLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Start Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 3, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnStartLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Reset Level Hook");
    CKBehavior *rl = FindFirstBB(script, "reset Level");
    link = FindNextLink(rl, FindNextBB(rl, FindNextBB(rl, rl->GetInput(0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreResetLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 4, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPostResetLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Pause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 5, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPauseLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Unpause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 6, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnUnpauseLevel();
                   return CKBR_OK;
               }));

    CKBehavior *bs = FindNextBB(script, FindFirstBB(script, "DeleteCollisionSurfaces"));

    GetLogger()->Info("Insert message Exit Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 7, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreExitLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnPostExitLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Next Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 8, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreNextLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnPostNextLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Dead Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 9, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnDead();
                   return CKBR_OK;
               }));

    CKBehavior *hs = FindFirstBB(script, "Highscore");
    hs->AddOutput("Out");
    FindBB(hs, [hs](CKBehavior *beh) {
        CreateLink(hs, beh, hs->GetOutput(0));
        return true;
    }, "Activate Script");

    GetLogger()->Info("Insert message End Level Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 10, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnPreEndLevel();
                 return CKBR_OK;
             }));
    CreateLink(script, hs, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostEndLevel();
        return CKBR_OK;
    }));
}

void BMLMod::OnEditScript_Menu_MenuInit(CKBehavior *script) {
    m_BML->AddTimer(1ul, [this]() {
        GetLogger()->Info("Acquire Game Fonts");
        CKBehavior *script = m_BML->GetScriptByName("Menu_Init");
        CKBehavior *fonts = FindFirstBB(script, "Fonts");
        CKBehavior *bbs[7] = {nullptr};
        int cnt = 0;
        FindBB(fonts, [&bbs, &cnt](CKBehavior *beh) {
            bbs[cnt++] = beh;
            return true;
        }, "TT CreateFontEx");

        std::map<std::string, ExecuteBB::FontType> fontid;
        fontid["GameFont_01"] = ExecuteBB::GAMEFONT_01;
        fontid["GameFont_02"] = ExecuteBB::GAMEFONT_02;
        fontid["GameFont_03"] = ExecuteBB::GAMEFONT_03;
        fontid["GameFont_03a"] = ExecuteBB::GAMEFONT_03A;
        fontid["GameFont_04"] = ExecuteBB::GAMEFONT_04;
        fontid["GameFont_Credits_Small"] = ExecuteBB::GAMEFONT_CREDITS_SMALL;
        fontid["GameFont_Credits_Big"] = ExecuteBB::GAMEFONT_CREDITS_BIG;

        for (int i = 0; i < 7; i++) {
            int font = 0;
            bbs[i]->GetOutputParameterValue(0, &font);
            ExecuteBB::InitFont(fontid[static_cast<const char *>(bbs[i]->GetInputParameterReadDataPtr(0))], font);
        }
    });
}

void BMLMod::OnEditScript_Menu_OptionsMenu(CKBehavior *script) {
    GetLogger()->Info("Start to insert Mods Button into Options Menu");

    char but_name[] = "M_Options_But_X";
    CK2dEntity *buttons[6] = {nullptr};
    buttons[0] = m_BML->Get2dEntityByName("M_Options_Title");
    for (int i = 1; i < 4; i++) {
        but_name[14] = '0' + i;
        buttons[i] = m_BML->Get2dEntityByName(but_name);
    }

    buttons[5] = m_BML->Get2dEntityByName("M_Options_But_Back");
    buttons[4] = (CK2dEntity *) m_CKContext->CopyObject(buttons[1]);
    buttons[4]->SetName("M_Options_But_4");
    for (int i = 0; i < 5; i++) {
        Vx2DVector pos;
        buttons[i]->GetPosition(pos, true);
        pos.y = 0.1f + 0.14f * i;
        buttons[i]->SetPosition(pos, true);
    }

    CKDataArray *array = m_BML->GetArrayByName("Menu_Options_ShowHide");
    array->InsertRow(3);
    array->SetElementObject(3, 0, buttons[4]);
    CKBOOL show = 1;
    array->SetElementValue(3, 1, &show, sizeof(show));
    m_BML->SetIC(array);

    CKBehavior *graph = FindFirstBB(script, "Options Menu");
    CKBehavior *up_sop = nullptr, *down_sop = nullptr, *up_ps = nullptr, *down_ps = nullptr;
    FindBB(graph, [graph, &up_sop, &down_sop](CKBehavior *beh) {
        CKBehavior *previous = FindPreviousBB(graph, beh);
        const char *name = previous->GetName();
        if (!strcmp(name, "Set 2D Material"))
            up_sop = beh;
        if (!strcmp(name, "Send Message"))
            down_sop = beh;
        return !(up_sop && down_sop);
    }, "Switch On Parameter");
    FindBB(graph, [graph, &up_ps, &down_ps](CKBehavior *beh) {
        CKBehavior *previous = FindNextBB(graph, beh);
        const char *name = previous->GetName();
        if (!strcmp(name, "Keyboard"))
            up_ps = beh;
        if (!strcmp(name, "Send Message"))
            down_ps = beh;
        return !(up_ps && down_ps);
    }, "Parameter Selector");

    CKParameterLocal *pin = CreateParamValue(graph, "Pin 5", CKPGUID_INT, 4);
    up_sop->CreateInputParameter("Pin 5", CKPGUID_INT)->SetDirectSource(pin);
    up_sop->AddOutput("Out 5");
    down_sop->CreateInputParameter("Pin 5", CKPGUID_INT)->SetDirectSource(pin);
    down_sop->AddOutput("Out 5");
    up_ps->CreateInputParameter("pIn 4", CKPGUID_INT)->SetDirectSource(pin);
    up_ps->AddInput("In 4");
    down_ps->CreateInputParameter("pIn 4", CKPGUID_INT)->SetDirectSource(pin);
    down_ps->AddInput("In 4");

    CKBehavior *text2d = CreateBB(graph, VT_INTERFACE_2DTEXT, true);
    CKBehavior *pushbutton = CreateBB(graph, TT_TOOLBOX_RT_TTPUSHBUTTON2, true);
    CKBehavior *text2dref = FindFirstBB(graph, "2D Text");
    CKBehavior *nop = FindFirstBB(graph, "Nop");
    CKParameterLocal *entity2d = CreateParamObject(graph, "Button", CKPGUID_2DENTITY, buttons[4]);
    CKParameterLocal *buttonname = CreateParamString(graph, "Text", "Mods");
    int textflags;
    text2dref->GetLocalParameterValue(0, &textflags);
    text2d->SetLocalParameterValue(0, &textflags, sizeof(textflags));

    text2d->GetTargetParameter()->SetDirectSource(entity2d);
    pushbutton->GetTargetParameter()->SetDirectSource(entity2d);
    text2d->GetInputParameter(0)->ShareSourceWith(text2dref->GetInputParameter(0));
    text2d->GetInputParameter(1)->SetDirectSource(buttonname);
    for (int i = 2; i < 6; i++)
        text2d->GetInputParameter(i)->ShareSourceWith(text2dref->GetInputParameter(i));

    FindNextLink(graph, up_sop, nullptr, 4, 0)->SetInBehaviorIO(up_sop->GetOutput(5));
    CreateLink(graph, up_sop, text2d, 4, 0);
    CreateLink(graph, text2d, nop, 0, 0);
    CreateLink(graph, text2d, pushbutton, 0, 0);
    FindPreviousLink(graph, up_ps, nullptr, 1, 3)->SetOutBehaviorIO(up_ps->GetInput(4));
    FindPreviousLink(graph, down_ps, nullptr, 2, 3)->SetOutBehaviorIO(down_ps->GetInput(4));
    CreateLink(graph, pushbutton, up_ps, 1, 3);
    CreateLink(graph, pushbutton, down_ps, 2, 3);
    graph->AddOutput("Button 5 Pressed");
    CreateLink(graph, down_sop, graph->GetOutput(4), 5);
    FindNextLink(script, graph, nullptr, 3, 0)->SetInBehaviorIO(graph->GetOutput(4));

    CKBehavior *modsmenu = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OpenModsMenu();
        return CKBR_OK;
    });
    CKBehavior *exit = FindFirstBB(script, "Exit", false, 1, 0);
    CreateLink(script, graph, modsmenu, 3, 0);
    CreateLink(script, modsmenu, exit, 0, 0);
    CKBehavior *keyboard = FindFirstBB(graph, "Keyboard");
    FindBB(keyboard, [keyboard](CKBehavior *beh) {
        CKParameter *source = beh->GetInputParameter(0)->GetRealSource();
        if (GetParamValue<CKKEYBOARD>(source) == CKKEY_ESCAPE) {
            CKBehavior *id = FindNextBB(keyboard, beh);
            SetParamValue(id->GetInputParameter(0)->GetRealSource(), 4);
            return false;
        }
        return true;
    }, "Secure Key");

    GetLogger()->Info("Mods Button inserted");
}

void BMLMod::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    GetLogger()->Info("Insert Ball/Camera Active/Inactive Hook");
    CKBehavior *camonoff = FindFirstBB(script, "CamNav On/Off");
    CKBehavior *ballonoff = FindFirstBB(script, "BallNav On/Off");
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType camon = mm->AddMessageType("CamNav activate");
    CKMessageType camoff = mm->AddMessageType("CamNav deactivate");
    CKMessageType ballon = mm->AddMessageType("BallNav activate");
    CKMessageType balloff = mm->AddMessageType("BallNav deactivate");
    CKBehavior *con, *coff, *bon, *boff;
    FindBB(camonoff, [camon, camoff, &con, &coff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == camon) con = beh;
        if (msg == camoff) coff = beh;
        return true;
    }, "Wait Message");
    CreateLink(camonoff, con, ExecuteBB::CreateHookBlock(camonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnCamNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(camonoff, coff, ExecuteBB::CreateHookBlock(camonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnCamNavInactive();
        return CKBR_OK;
    }), 0, 0);
    FindBB(ballonoff, [ballon, balloff, &bon, &boff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == ballon) bon = beh;
        if (msg == balloff) boff = beh;
        return true;
    }, "Wait Message");
    CreateLink(ballonoff, bon, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnBallNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(ballonoff, boff, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnBallNavInactive();
        return CKBR_OK;
    }), 0, 0);

    m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
}

void BMLMod::OnEditScript_Gameplay_Energy(CKBehavior *script) {
    GetLogger()->Info("Insert Counter Active/Inactive Hook");
    CKBehavior *som = FindFirstBB(script, "Switch On Message");
    InsertBB(script, FindNextLink(script, som, nullptr, 3), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnCounterActive();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, som, nullptr, 1), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnCounterInactive();
        return CKBR_OK;
    }));

    GetLogger()->Info("Insert Life/Point Hooks");
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType lifeup = mm->AddMessageType("Life_Up"), balloff = mm->AddMessageType("Ball Off"),
        sublife = mm->AddMessageType("Sub Life"), extrapoint = mm->AddMessageType("Extrapoint");
    CKBehavior *lu, *bo, *sl, *ep;
    FindBB(script, [lifeup, balloff, sublife, extrapoint, &lu, &bo, &sl, &ep](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == lifeup) lu = beh;
        if (msg == balloff) bo = beh;
        if (msg == sublife) sl = beh;
        if (msg == extrapoint) ep = beh;
        return true;
    }, "Wait Message");
    CKBehavior *luhook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreLifeUp();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, lu, "add Life"), luhook);
    CreateLink(script, FindEndOfChain(script, luhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostLifeUp();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, bo, "Delayer"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnBallOff();
        return CKBR_OK;
    }));
    CKBehavior *slhook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreSubLife();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, sl, "sub Life"), slhook);
    CreateLink(script, FindEndOfChain(script, slhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostSubLife();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, ep, "Show"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnExtraPoint();
        return CKBR_OK;
    }));
}

void BMLMod::OnEditScript_Gameplay_Events(CKBehavior *script) {
    GetLogger()->Info("Insert Checkpoint & GameOver Hooks");
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType checkpoint = mm->AddMessageType("Checkpoint reached"),
        gameover = mm->AddMessageType("Game Over"),
        levelfinish = mm->AddMessageType("Level_Finish");
    CKBehavior *cp, *go, *lf;
    FindBB(script, [checkpoint, gameover, levelfinish, &cp, &go, &lf](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == checkpoint) cp = beh;
        if (msg == gameover) go = beh;
        if (msg == levelfinish) lf = beh;
        return true;
    }, "Wait Message");
    CKBehavior *hook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreCheckpointReached();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, cp, "set Resetpoint"), hook);
    CreateLink(script, FindEndOfChain(script, hook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostCheckpointReached();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, go, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnGameOver();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, lf, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnLevelFinish();
        return CKBR_OK;
    }));
}

void BMLMod::OnEditScript_Levelinit_build(CKBehavior *script) {
    CKBehavior *loadLevel = FindFirstBB(script, "Load LevelXX");
    CKBehaviorLink *inLink = FindNextLink(loadLevel, loadLevel->GetInput(0));
    CKBehavior *op = FindNextBB(loadLevel, inLink->GetOutBehaviorIO()->GetOwner());
    m_LevelRow = op->GetOutputParameter(0)->GetDestination(0);
    CKBehavior *objLoad = FindFirstBB(loadLevel, "Object Load");
    CKBehavior *bin = CreateBB(loadLevel, VT_LOGICS_BINARYSWITCH);
    CreateLink(loadLevel, loadLevel->GetInput(0), bin, 0);
    m_LoadCustom = CreateLocalParameter(loadLevel, "Custom Level", CKPGUID_BOOL);
    bin->GetInputParameter(0)->SetDirectSource(m_LoadCustom);
    inLink->SetInBehaviorIO(bin->GetOutput(1));
    CreateLink(loadLevel, bin, objLoad);
    m_MapFile = objLoad->GetInputParameter(0)->GetDirectSource();
}

void BMLMod::OnEditScript_ExtraLife_Fix(CKBehavior *script) {
    CKBehavior *emitter = FindFirstBB(script, "SphericalParticleSystem");
    emitter->CreateInputParameter("Real-Time Mode", CKPGUID_BOOL)
        ->SetDirectSource(CreateParamValue<CKBOOL>(script, "Real-Time Mode", CKPGUID_BOOL, 1));
    emitter->CreateInputParameter("DeltaTime", CKPGUID_FLOAT)
        ->SetDirectSource(CreateParamValue<float>(script, "DeltaTime", CKPGUID_FLOAT, 20.0f));
}

void BMLMod::OnProcess_FpsDisplay() {
    CKStats stats;
    m_CKContext->GetProfileStats(&stats);
    m_FPSCount += int(1000 / stats.TotalFrameTime);
    if (++m_FPSTimer == 60) {
        m_FPS->SetText(("FPS: " + std::to_string(m_FPSCount / 60)).c_str());
        m_FPSTimer = 0;
        m_FPSCount = 0;
    }
}

void BMLMod::OnProcess_CommandBar() {
    if (!m_CmdTyping && m_InputHook->oIsKeyPressed(CKKEY_SLASH)) {
        GetLogger()->Info("Toggle Command Bar");
        m_CmdTyping = true;
        m_InputHook->SetBlock(true);
        m_CmdBar->SetVisible(true);
        m_HistoryPos = m_CmdHistory.size();
    }

    m_MsgLog->Process();
    m_IngameBanner->Process();

    if (m_CurGui)
        m_CurGui->Process();

    if (m_CmdTyping) {
        m_CmdBar->Process();

        for (int i = 0; i < std::min(MSG_MAXSIZE, m_MsgCount); i++) {
            m_Msgs[i].m_Background->SetVisible(true);
            m_Msgs[i].m_Background->SetColor(VxColor(0, 0, 0, 110));
            m_Msgs[i].m_Text->SetVisible(true);
        }
    } else {
        for (int i = 0; i < std::min(MSG_MAXSIZE, m_MsgCount); i++) {
            float &timer = m_Msgs[i].m_Timer;
            m_Msgs[i].m_Background->SetVisible(timer > 0);
            m_Msgs[i].m_Background->SetColor(VxColor(0, 0, 0, std::min(110, (int)(timer / 20))));
            m_Msgs[i].m_Text->SetVisible(timer > 1000);
        }
    }

    CKStats stats;
    m_CKContext->GetProfileStats(&stats);
    for (int i = 0; i < std::min(MSG_MAXSIZE, m_MsgCount); i++) {
        m_Msgs[i].m_Timer -= stats.TotalFrameTime;
    }
}

void BMLMod::OnProcess_SRTimer() {
    m_SRTimer += m_TimeManager->GetLastDeltaTime();
    int counter = int(m_SRTimer);
    int ms = counter % 1000;
    counter /= 1000;
    int s = counter % 60;
    counter /= 60;
    int m = counter % 60;
    counter /= 60;
    int h = counter % 100;
    static char time[16];
    sprintf(time, "%02d:%02d:%02d.%03d", h, m, s, ms);
    m_SRScore->SetText(time);
}

void BMLMod::OnResize() {
    CKCamera *cams[] = {
        m_BML->GetTargetCameraByName("Cam_MenuLevel"),
        m_BML->GetTargetCameraByName("InGameCam")
    };

    for (CKCamera *cam : cams) {
        if (cam) {
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            CKStateChunk *chunk = CKSaveObjectState(cam);

            m_BML->RestoreIC(cam);
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            m_BML->SetIC(cam);

            CKReadObjectState(cam, chunk);
        }
    }
}

void BMLMod::OnCmdEdit(CKDWORD key) {
    switch (key) {
        case CKKEY_RETURN:
            m_CmdHistory.emplace_back(m_CmdInput->GetText());
            if (m_CmdInput->GetText()[0] == '/') {
                BML_GetModManager()->ExecuteCommand(m_CmdInput->GetText() + 1);
            } else {
                AddIngameMessage(m_CmdInput->GetText());
            }
        case CKKEY_ESCAPE:
            m_CmdTyping = false;
            BML_GetModManager()->AddTimerLoop(1ul, [this, key] {
                if (m_CmdTyping)
                    return false;
                InputHook *input = BML_GetInputHook();
                if (input->oIsKeyDown(key))
                    return true;
                input->SetBlock(false);
                return false;
            });
            m_CmdBar->SetVisible(false);
            m_CmdInput->SetText("");
            break;
        case CKKEY_TAB:
            if (m_CmdInput->GetText()[0] == '/') {
                m_CmdInput->SetText(('/' + BML_GetModManager()->TabCompleteCommand(m_CmdInput->GetText() + 1)).c_str());
            }
            break;
        case CKKEY_UP:
            if (m_HistoryPos > 0)
                m_CmdInput->SetText(m_CmdHistory[--m_HistoryPos].c_str());
            break;
        case CKKEY_DOWN:
            if (m_HistoryPos < m_CmdHistory.size())
                m_CmdInput->SetText(++m_HistoryPos == m_CmdHistory.size() ? "/" : m_CmdHistory[m_HistoryPos].c_str());
            break;
        default:
            break;
    }
}