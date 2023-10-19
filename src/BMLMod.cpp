#include "BMLMod.h"

#include <ctime>
#include <map>
#include <algorithm>

#include "Gui.h"
#include "InputHook.h"
#include "ExecuteBB.h"
#include "ScriptHelper.h"
#include "ModLoader.h"
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
                ModLoader::GetInstance().GetBMLMod()->ShowGui(gui);
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
    ModLoader::GetInstance().GetBMLMod()->ShowGui(GetParentGui());
}

void GuiList::SetVisible(bool visible) {
    Gui::SetVisible(visible);
    if (visible)
        SetPage(m_CurPage);
}

GuiModOption::GuiModOption() {
    Init(ModLoader::GetInstance().GetModCount(), 4);

    AddTextLabel("M_Opt_Mods_Title", "Mod List", ExecuteBB::GAMEFONT_02, 0.35f, 0.1f, 0.3f, 0.1f);
}

BGui::Button *GuiModOption::CreateButton(int index) {
    return AddSettingButton(("M_Opt_Mods_" + std::to_string(index)).c_str(), "", 0.25f + 0.13f * index);
}

std::string GuiModOption::GetButtonText(int index) {
    return ModLoader::GetInstance().GetMod(index)->GetID();
}

BGui::Gui *GuiModOption::CreateSubGui(int index) {
    return new GuiModMenu(ModLoader::GetInstance().GetMod(index));
}

BGui::Gui *GuiModOption::GetParentGui() {
    return nullptr;
}

void GuiModOption::Exit() {
    GuiList::Exit();
    ModLoader::GetInstance().GetCKContext()->GetCurrentScene()->Activate(
        ModLoader::GetInstance().GetScriptByName("Menu_Options"),
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

    m_Config = ModLoader::GetInstance().GetConfig(mod);
    if (m_Config) {
        AddTextLabel("M_Opt_ModMenu_Title", "Mod Options", ExecuteBB::GAMEFONT_01, 0.35f, 0.4f, 0.3f, 0.05f);
        for (auto &cate: m_Config->m_Data)
            m_Categories.push_back(cate.name);
    }

    Init(m_Categories.size(), 6);
    SetVisible(false);
}

void GuiModMenu::Process() {
    bool show_cmt = false;
    if (m_CurPage >= 0 && m_CurPage < m_MaxPage) {
        CKRenderContext *rc = ModLoader::GetInstance().GetRenderContext();
        InputHook *input = ModLoader::GetInstance().GetInputManager();
        Vx2DVector mousePos;
        input->GetMousePosition(mousePos, false);
        int size = (std::min)(m_MaxSize, m_Size - m_CurPage * m_MaxSize);
        for (int i = 0; i < size; i++) {
            if (Intersect(mousePos.x / rc->GetWidth(), mousePos.y / rc->GetHeight(), m_Buttons[i])) {
                if (m_CurComment != i) {
                    m_CommentBackground->SetVisible(true);
                    m_Comment->SetVisible(true);
                    m_Comment->SetText(m_Config->m_Data[i + m_CurPage * m_MaxSize].comment.c_str());
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
    return ModLoader::GetInstance().GetBMLMod()->m_ModOption;
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
        ModLoader::GetInstance().GetCKContext()->GetCurrentScene()->Activate(
            ModLoader::GetInstance().GetScriptByName("Menu_Start"),
            true);
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
                              mm->SendMessageSingle(loadMenu, ModLoader::GetInstance().GetGroupByName("All_Sound"));
                              ModLoader::GetInstance().Get2dEntityByName("M_BlackScreen")->Show(CKHIDE);
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
    ModLoader::GetInstance().GetCKContext()->GetCurrentScene()->Activate(
        ModLoader::GetInstance().GetScriptByName("Menu_Main"),
        true);
}

GuiModCategory::GuiModCategory(GuiModMenu *parent, Config *config, const std::string &category) {
    for (Property *prop: config->GetCategory(category.c_str()).props) {
        auto *newprop = new Property(nullptr, category, prop->m_key);
        newprop->CopyValue(prop);
        newprop->SetComment(prop->GetComment());
        m_Data.push_back(newprop);
    }

    m_Size = m_Data.size();
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

    Vx2DVector offset(0.0f, ModLoader::GetInstance().GetRenderContext()->GetHeight() * 0.015f);

    float cnt = 0.25f, page = 0;
    std::vector<BGui::Element *> elements;
    std::vector<std::pair<Property *, BGui::Element *>> comments;
    for (Property *prop: m_Data) {
        std::string name = prop->m_key;
        switch (prop->GetType()) {
            case IProperty::STRING: {
                BGui::Button *bg = AddSettingButton(name.c_str(), name.c_str(), cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                BGui::Input *input = AddTextInput(name.c_str(), ExecuteBB::GAMEFONT_03, 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                input->SetText(prop->GetString());
                input->SetCallback([prop, input](CKDWORD) { prop->SetString(input->GetText()); });
                elements.push_back(input);
                BGui::Panel *panel = AddPanel(name.c_str(), VxColor(0, 0, 0, 110), 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                elements.push_back(panel);
                m_Inputs.emplace_back(input, nullptr);
                comments.emplace_back(prop, bg);
                cnt += 0.12f;
                break;
            }
            case IProperty::INTEGER: {
                BGui::Button *bg = AddSettingButton(name.c_str(), name.c_str(), cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                BGui::Input *input = AddTextInput(name.c_str(), ExecuteBB::GAMEFONT_03, 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                input->SetText(std::to_string(prop->GetInteger()).c_str());
                input->SetCallback([prop, input](CKDWORD) { prop->SetInteger(atoi(input->GetText())); });
                elements.push_back(input);
                BGui::Panel *panel = AddPanel(name.c_str(), VxColor(0, 0, 0, 110), 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                elements.push_back(panel);
                m_Inputs.emplace_back(input, nullptr);
                comments.emplace_back(prop, bg);
                cnt += 0.12f;
                break;
            }
            case IProperty::FLOAT: {
                BGui::Button *bg = AddSettingButton(name.c_str(), name.c_str(), cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                BGui::Input *input = AddTextInput(name.c_str(), ExecuteBB::GAMEFONT_03, 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                std::ostringstream stream;
                stream << prop->GetFloat();
                input->SetText(stream.str().c_str());
                input->SetCallback([prop, input](CKDWORD) { prop->SetFloat((float) atof(input->GetText())); });
                elements.push_back(input);
                BGui::Panel *panel = AddPanel(name.c_str(), VxColor(0, 0, 0, 110), 0.43f, cnt + 0.05f, 0.18f, 0.025f);
                elements.push_back(panel);
                m_Inputs.emplace_back(input, nullptr);
                comments.emplace_back(prop, bg);
                cnt += 0.12f;
                break;
            }
            case IProperty::KEY: {
                std::pair<BGui::Button *, BGui::KeyInput *> key = AddKeyButton(name.c_str(), name.c_str(), cnt);
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
                BGui::Button *bg = AddSettingButton(name.c_str(), name.c_str(), cnt);
                bg->SetAlignment(ALIGN_TOP);
                bg->SetFont(ExecuteBB::GAMEFONT_03);
                bg->SetZOrder(15);
                bg->SetOffset(offset);
                elements.push_back(bg);
                auto yesno = AddYesNoButton(name.c_str(), cnt + 0.043f, 0.4350f, 0.5200f, [prop](bool value) { prop->SetBoolean(value); });
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

    m_MaxPage = m_Elements.size();

    SetVisible(false);
}

void GuiModCategory::Process() {
    bool show_cmt = false;
    if (m_CurPage >= 0 && m_CurPage < (int) m_Comments.size()) {
        CKRenderContext *rc = ModLoader::GetInstance().GetRenderContext();
        InputHook *input = ModLoader::GetInstance().GetInputManager();
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
        std::vector<Property *> &props = m_Config->GetCategory(m_Category.c_str()).props;
        for (size_t i = 0; i < props.size(); i++) {
            Property *prop = props[i];
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
    Config::Category &cate = m_Config->GetCategory(m_Category.c_str());
    for (Property *p: m_Data)
        cate.GetProperty(p->m_key.c_str())->CopyValue(p);
    m_Config->Save();
    Exit();
}

void GuiModCategory::Exit() {
    ModLoader::GetInstance().GetBMLMod()->ShowGui(m_Parent);
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

    m_Balls[0] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Paper.nmo", true, "P_Ball_Paper_MF").second;
    m_Balls[1] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Wood.nmo", true, "P_Ball_Wood_MF").second;
    m_Balls[2] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Stone.nmo", true, "P_Ball_Stone_MF").second;
    m_Balls[3] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Box.nmo", true, "P_Box_MF").second;

    m_TravelCam = (CKCamera *) m_CKContext->CreateObject(CKCID_CAMERA, "TravelCam");
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

        m_CamPos = m_BML->Get3dEntityByName("Cam_Pos");
        m_CamOrient = m_BML->Get3dEntityByName("Cam_Orient");
        m_CamOrientRef = m_BML->Get3dEntityByName("Cam_OrientRef");
        m_CamTarget = m_BML->Get3dEntityByName("Cam_Target");
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
    m_DeltaTime = m_TimeManager->GetLastDeltaTime() / 10;
    m_CheatEnabled = m_BML->IsCheatEnabled();

    m_OldWindowRect = m_WindowRect;
    m_RenderContext->Get2dRoot(TRUE)->GetRect(m_WindowRect);
    if (m_WindowRect != m_OldWindowRect) {
        OnResize();
    }

    OnProcess_SkipRender();

    if (m_IngameBanner) {
        OnProcess_FpsDisplay();
    }

    if (m_CmdBar) {
        OnProcess_CommandBar();
    }

    if (m_BML->IsPlaying()) {
        OnProcess_Suicide();
        OnProcess_Travel();

        if (m_CheatEnabled) {
            OnProcess_ChangeSpeed();
            OnProcess_ChangeBall();
            OnProcess_ResetBall();
            OnProcess_AddLife();
            OnProcess_Summon();
        }
    }

    if (m_SRActivated) {
        OnProcess_SRTimer();
    }

    if (m_MapsGui) {
        bool inStart = m_Level01->IsVisible();
        m_CustomMaps->SetVisible(inStart);
    }
}

void BMLMod::OnCheatEnabled(bool enable) {
    if (enable) {
        SetParamValue(m_BallForce[0], m_BallCheat[0]->GetKey());
        SetParamValue(m_BallForce[1], m_BallCheat[1]->GetKey());
    } else {
        SetParamValue(m_BallForce[0], CKKEYBOARD(0));
        SetParamValue(m_BallForce[1], CKKEYBOARD(0));
    }
}

void BMLMod::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (m_BML->IsCheatEnabled()) {
        if (prop == m_BallCheat[0])
            SetParamValue(m_BallForce[0], m_BallCheat[0]->GetKey());
        else if (prop == m_BallCheat[1])
            SetParamValue(m_BallForce[1], m_BallCheat[1]->GetKey());
    } else {
        if (prop == m_UnlockFPS) {
            if (prop->GetBoolean())
                AdjustFrameRate(false, 0);
            else {
                int val = m_FPSLimit->GetInteger();
                if (val > 0)
                    AdjustFrameRate(false, static_cast<float>(val));
                else
                    AdjustFrameRate(true);
            }
        }
        else if (prop == m_FPSLimit && !m_UnlockFPS->GetBoolean()) {
            int val = prop->GetInteger();
            if (val > 0)
                AdjustFrameRate(false, static_cast<float>(val));
            else
                AdjustFrameRate(true);
        }
        else if (prop == m_Overclock) {
            for (int i = 0; i < 3; i++) {
                m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][m_Overclock->GetBoolean()]);
            }
        }
        else if (prop == m_ShowTitle) {
            m_Title->SetVisible(prop->GetBoolean());
        }
        else if (prop == m_ShowFPS) {
            m_FPS->SetVisible(prop->GetBoolean());
        }
        else if (prop == m_ShowSR && m_BML->IsIngame()) {
            m_SRScore->SetVisible(m_ShowSR->GetBoolean());
            m_SRTitle->SetVisible(m_ShowSR->GetBoolean());
        }
        else if (prop == m_MsgDuration) {
            m_MsgMaxTimer = m_MsgDuration->GetFloat() * 1000;
            if (m_MsgMaxTimer < 2000) {
                m_MsgDuration->SetFloat(2.0f);
            }
        }
    }
}

void BMLMod::OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) {
    if (args[0] == "cheat") {
        if (m_BML->IsCheatEnabled() && (args.size() == 1 || !ICommand::ParseBoolean(args[1]))) {
            ChangeBallSpeed(1);
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

void BMLMod::OnPostResetLevel() {
    CKDataArray *ph = m_BML->GetArrayByName("PH");
    for (auto iter = m_TempBalls.rbegin(); iter != m_TempBalls.rend(); iter++) {
        ph->RemoveRow(iter->first);
        m_CKContext->DestroyObject(iter->second);
    }
    m_TempBalls.clear();
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

void BMLMod::EnterTravelCam() {
    CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
    m_TravelCam->SetWorldMatrix(cam->GetWorldMatrix());
    int width, height;
    cam->GetAspectRatio(width, height);
    m_TravelCam->SetAspectRatio(width, height);
    m_TravelCam->SetFov(cam->GetFov());
    m_BML->GetRenderContext()->AttachViewpointToCamera(m_TravelCam);
}

void BMLMod::ExitTravelCam() {
    CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
    m_BML->GetRenderContext()->AttachViewpointToCamera(cam);
}

int BMLMod::GetHSScore() {
    int points, lifes;
    CKDataArray *energy = m_BML->GetArrayByName("Energy");
    energy->GetElementValue(0, 0, &points);
    energy->GetElementValue(0, 1, &lifes);
    return points + lifes * 200;
}

bool BMLMod::IsInTravelCam() {
    return m_BML->GetRenderContext()->GetAttachedCamera() == m_TravelCam;
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

void BMLMod::ChangeBallSpeed(float times) {
    if (m_BML->IsIngame()) {
        bool notify = true;

        if (!m_PhysicsBall) {
            m_PhysicsBall = m_BML->GetArrayByName("Physicalize_GameBall");
            CKBehavior *ingame = m_BML->GetScriptByName("Gameplay_Ingame");
            m_Force = ScriptHelper::FindFirstBB(ingame, "Ball Navigation")->GetInputParameter(0)->GetRealSource();

            for (int i = 0; i < m_PhysicsBall->GetRowCount(); i++) {
                std::string ballName(m_PhysicsBall->GetElementStringValue(i, 0, nullptr), '\0');
                m_PhysicsBall->GetElementStringValue(i, 0, &ballName[0]);
                ballName.pop_back();
                float force;
                m_PhysicsBall->GetElementValue(i, 7, &force);
                m_Forces[ballName] = force;
            }
        }

        if (m_PhysicsBall) {
            CKObject *curBall = m_CurLevel->GetElementObject(0, 1);
            if (curBall) {
                auto iter = m_Forces.find(curBall->GetName());
                if (iter != m_Forces.end()) {
                    float force = iter->second;
                    force *= times;
                    if (force == ScriptHelper::GetParamValue<float>(m_Force))
                        notify = false;
                    ScriptHelper::SetParamValue(m_Force, force);
                }
            }

            for (int i = 0; i < m_PhysicsBall->GetRowCount(); i++) {
                std::string ballName(m_PhysicsBall->GetElementStringValue(i, 0, nullptr), '\0');
                m_PhysicsBall->GetElementStringValue(i, 0, &ballName[0]);
                ballName.pop_back();
                auto iter = m_Forces.find(ballName);
                if (iter != m_Forces.end()) {
                    float force = iter->second;
                    force *= times;
                    m_PhysicsBall->SetElementValue(i, 7, &force);
                }
            }

            if (notify && m_SpeedNotification->GetBoolean())
                AddIngameMessage(("Current Ball Speed Changed to " + std::to_string(times) + " times").c_str());
        }
    }
}

void BMLMod::ResetBall() {
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

    mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Gameplay"));
    mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Sound"));

    m_BML->AddTimer(2ul, [this]() {
        auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
        if (curBall) {
            ExecuteBB::Unphysicalize(curBall);

            m_DynamicPos->ActivateInput(1);
            m_DynamicPos->Activate();

            m_BML->AddTimer(1ul, [this, curBall]() {
                VxMatrix matrix;
                m_CurLevel->GetElementValue(0, 3, &matrix);
                curBall->SetWorldMatrix(matrix);

                CK3dEntity *camMF = m_BML->Get3dEntityByName("Cam_MF");
                m_BML->RestoreIC(camMF, true);
                camMF->SetWorldMatrix(matrix);

                m_BML->AddTimer(1ul, [this]() {
                    m_DynamicPos->ActivateInput(0);
                    m_DynamicPos->Activate();
                    m_PhysicsNewBall->ActivateInput(0);
                    m_PhysicsNewBall->Activate();
                    m_PhysicsNewBall->GetParent()->Activate();
                });
            });
        }
    });
}

int BMLMod::GetSectorCount() {
    CKDataArray *checkPoints = m_BML->GetArrayByName("Checkpoints");
    if (!checkPoints)
        return 0;
    return checkPoints->GetRowCount();
}

void BMLMod::SetSector(int sector) {
    if (m_BML->IsPlaying()) {
        CKDataArray *checkPoints = m_BML->GetArrayByName("Checkpoints");
        CKDataArray *resetPoints = m_BML->GetArrayByName("ResetPoints");

        if (sector < 1 || sector > checkPoints->GetRowCount() + 1)
            return;

        int curSector = ScriptHelper::GetParamValue<int>(m_CurSector);
        if (curSector != sector) {
            VxMatrix matrix;
            resetPoints->GetElementValue(sector - 1, 0, &matrix);
            m_CurLevel->SetElementValue(0, 3, &matrix);

            m_IngameParam->SetElementValue(0, 1, &sector);
            m_IngameParam->SetElementValue(0, 2, &curSector);
            ScriptHelper::SetParamValue(m_CurSector, sector);

            AddIngameMessage(("Changed to Sector " + std::to_string(sector)).c_str());

            CKBehavior *sectorMgr = m_BML->GetScriptByName("Gameplay_SectorManager");
            m_CKContext->GetCurrentScene()->Activate(sectorMgr, true);

            m_BML->AddTimerLoop(1ul, [this, sector, checkPoints, sectorMgr]() {
                if (sectorMgr->IsActive())
                    return true;

                m_BML->AddTimer(2ul, [this, checkPoints, sector]() {
                    CKBOOL active = false;
                    m_CurLevel->SetElementValue(0, 4, &active);

                    CK_ID flameId;
                    checkPoints->GetElementValue(sector % 2, 1, &flameId);
                    auto *flame = (CK3dEntity *) m_CKContext->GetObject(flameId);
                    m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);

                    checkPoints->GetElementValue(sector - 1, 1, &flameId);
                    flame = (CK3dEntity *) m_CKContext->GetObject(flameId);
                    m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);

                    if (sector > checkPoints->GetRowCount()) {
                        CKMessageManager *mm = m_BML->GetMessageManager();
                        CKMessageType msg = mm->AddMessageType("last Checkpoint reached");
                        mm->SendMessageSingle(msg, m_BML->GetGroupByName("All_Sound"));

                        ResetBall();
                    } else {
                        m_BML->AddTimer(2ul, [this, sector, checkPoints, flame]() {
                            VxMatrix matrix;
                            checkPoints->GetElementValue(sector - 1, 0, &matrix);
                            flame->SetWorldMatrix(matrix);
                            CKBOOL active = true;
                            m_CurLevel->SetElementValue(0, 4, &active);
                            m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);
                            m_BML->Show(flame, CKSHOW, true);

                            ResetBall();
                        });
                    }
                });
                return false;
            });
        }
    }
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

    m_Overclock = GetConfig()->GetProperty("Misc", "Overclock");
    m_Overclock->SetComment("Remove delay of spawn / respawn");
    m_Overclock->SetDefaultBoolean(false);

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

    GetConfig()->SetCategoryComment("Debug", "Debug Utilities");
    m_EnableSuicide = GetConfig()->GetProperty("Debug", "EnableSuicide");
    m_EnableSuicide->SetComment("Enable the Suicide Hotkey");
    m_EnableSuicide->SetDefaultBoolean(true);
    m_Suicide = GetConfig()->GetProperty("Debug", "Suicide");
    m_Suicide->SetComment("Suicide");
    m_Suicide->SetDefaultKey(CKKEY_R);

    m_BallCheat[0] = GetConfig()->GetProperty("Debug", "BallUp");
    m_BallCheat[0]->SetComment("Apply an upward force to the ball");
    m_BallCheat[0]->SetDefaultKey(CKKEY_F1);

    m_BallCheat[1] = GetConfig()->GetProperty("Debug", "BallDown");
    m_BallCheat[1]->SetComment("Apply a downward force to the ball");
    m_BallCheat[1]->SetDefaultKey(CKKEY_F2);

    m_ChangeBall[0] = GetConfig()->GetProperty("Debug", "TurnPaper");
    m_ChangeBall[0]->SetComment("Turn into paper ball");
    m_ChangeBall[0]->SetDefaultKey(CKKEY_I);

    m_ChangeBall[1] = GetConfig()->GetProperty("Debug", "TurnWood");
    m_ChangeBall[1]->SetComment("Turn into wood ball");
    m_ChangeBall[1]->SetDefaultKey(CKKEY_O);

    m_ChangeBall[2] = GetConfig()->GetProperty("Debug", "TurnStone");
    m_ChangeBall[2]->SetComment("Turn into stone ball");
    m_ChangeBall[2]->SetDefaultKey(CKKEY_P);

    m_ResetBall = GetConfig()->GetProperty("Debug", "ResetBall");
    m_ResetBall->SetComment("Reset ball and all moduls");
    m_ResetBall->SetDefaultKey(CKKEY_BACK);

    m_AddLife = GetConfig()->GetProperty("Debug", "AddLife");
    m_AddLife->SetComment("Add one extra Life");
    m_AddLife->SetDefaultKey(CKKEY_L);

    m_SpeedupBall = GetConfig()->GetProperty("Debug", "BallSpeedUp");
    m_SpeedupBall->SetComment("Change to 3 times ball speed");
    m_SpeedupBall->SetDefaultKey(CKKEY_LCONTROL);

    m_SpeedNotification = GetConfig()->GetProperty("Debug", "SpeedNotification");
    m_SpeedNotification->SetComment("Notify the player when speed of the ball changes.");
    m_SpeedNotification->SetDefaultBoolean(true);

    m_SkipRenderKey = GetConfig()->GetProperty("Debug", "SkipRender");
    m_SkipRenderKey->SetComment("Skip rendering of current frames while holding.");
    m_SkipRenderKey->SetDefaultKey(CKKEY_F);

    GetConfig()->SetCategoryComment("Auxiliaries", "Temporal Auxiliary Moduls");
    m_AddBall[0] = GetConfig()->GetProperty("Auxiliaries", "PaperBall");
    m_AddBall[0]->SetComment("Add a Paper Ball");
    m_AddBall[0]->SetDefaultKey(CKKEY_J);

    m_AddBall[1] = GetConfig()->GetProperty("Auxiliaries", "WoodBall");
    m_AddBall[1]->SetComment("Add a Wood Ball");
    m_AddBall[1]->SetDefaultKey(CKKEY_K);

    m_AddBall[2] = GetConfig()->GetProperty("Auxiliaries", "StoneBall");
    m_AddBall[2]->SetComment("Add a Stone Ball");
    m_AddBall[2]->SetDefaultKey(CKKEY_N);

    m_AddBall[3] = GetConfig()->GetProperty("Auxiliaries", "Box");
    m_AddBall[3]->SetComment("Add a Box");
    m_AddBall[3]->SetDefaultKey(CKKEY_M);

    m_MoveKeys[0] = GetConfig()->GetProperty("Auxiliaries", "MoveFront");
    m_MoveKeys[0]->SetComment("Move Front");
    m_MoveKeys[0]->SetDefaultKey(CKKEY_UP);

    m_MoveKeys[1] = GetConfig()->GetProperty("Auxiliaries", "MoveBack");
    m_MoveKeys[1]->SetComment("Move Back");
    m_MoveKeys[1]->SetDefaultKey(CKKEY_DOWN);

    m_MoveKeys[2] = GetConfig()->GetProperty("Auxiliaries", "MoveLeft");
    m_MoveKeys[2]->SetComment("Move Left");
    m_MoveKeys[2]->SetDefaultKey(CKKEY_LEFT);

    m_MoveKeys[3] = GetConfig()->GetProperty("Auxiliaries", "MoveRight");
    m_MoveKeys[3]->SetComment("Move Right");
    m_MoveKeys[3]->SetDefaultKey(CKKEY_RIGHT);

    m_MoveKeys[4] = GetConfig()->GetProperty("Auxiliaries", "MoveUp");
    m_MoveKeys[4]->SetComment("Move Up");
    m_MoveKeys[4]->SetDefaultKey(CKKEY_RSHIFT);

    m_MoveKeys[5] = GetConfig()->GetProperty("Auxiliaries", "MoveDown");
    m_MoveKeys[5]->SetComment("Move Down");
    m_MoveKeys[5]->SetDefaultKey(CKKEY_RCONTROL);

    GetConfig()->SetCategoryComment("Camera", "Camera Utilities");
    m_CamOn = GetConfig()->GetProperty("Camera", "Enable");
    m_CamOn->SetComment("Enable Camera Utilities");
    m_CamOn->SetDefaultBoolean(false);

    m_CamReset = GetConfig()->GetProperty("Camera", "Reset");
    m_CamReset->SetComment("Reset Camera");
    m_CamReset->SetDefaultKey(CKKEY_D);

    m_Cam45 = GetConfig()->GetProperty("Camera", "Rotate45");
    m_Cam45->SetComment("Set to 45 degrees");
    m_Cam45->SetDefaultKey(CKKEY_W);

    m_CamRot[0] = GetConfig()->GetProperty("Camera", "RotateLeft");
    m_CamRot[0]->SetComment("Rotate the camera");
    m_CamRot[0]->SetDefaultKey(CKKEY_Q);

    m_CamRot[1] = GetConfig()->GetProperty("Camera", "RotateRight");
    m_CamRot[1]->SetComment("Rotate the camera");
    m_CamRot[1]->SetDefaultKey(CKKEY_E);

    m_CamY[0] = GetConfig()->GetProperty("Camera", "MoveUp");
    m_CamY[0]->SetComment("Move the camera");
    m_CamY[0]->SetDefaultKey(CKKEY_A);

    m_CamY[1] = GetConfig()->GetProperty("Camera", "MoveDown");
    m_CamY[1]->SetComment("Move the camera");
    m_CamY[1]->SetDefaultKey(CKKEY_Z);

    m_CamZ[0] = GetConfig()->GetProperty("Camera", "MoveFront");
    m_CamZ[0]->SetComment("Move the camera");
    m_CamZ[0]->SetDefaultKey(CKKEY_S);

    m_CamZ[1] = GetConfig()->GetProperty("Camera", "MoveBack");
    m_CamZ[1]->SetComment("Move the camera");
    m_CamZ[1]->SetDefaultKey(CKKEY_X);
}

void BMLMod::RegisterCommands() {
    m_BML->RegisterCommand(new CommandBML());
    m_BML->RegisterCommand(new CommandHelp());
    m_BML->RegisterCommand(new CommandCheat());
    m_BML->RegisterCommand(new CommandClear(this));
    m_BML->RegisterCommand(new CommandScore());
    m_BML->RegisterCommand(new CommandKill());
    m_BML->RegisterCommand(new CommandSetSpawn());
    m_BML->RegisterCommand(new CommandSector(this));
    m_BML->RegisterCommand(new CommandWin());
    m_BML->RegisterCommand(new CommandSpeed(this));
    m_BML->RegisterCommand(new CommandTravel(this));
}

void BMLMod::OnEditScript_Base_EventHandler(CKBehavior *script) {
    CKBehavior *som = FindFirstBB(script, "Switch On Message", false, 2, 11, 11, 0);

    GetLogger()->Info("Insert message Start Menu Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 ModLoader::GetInstance().OnPreStartMenu();
                 return CKBR_OK;
             }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 0, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   ModLoader::GetInstance().OnPostStartMenu();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Exit Game Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 ModLoader::GetInstance().OnExitGame();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Load Level Hook");
    CKBehaviorLink *link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 2, 0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPreLoadLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 2, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   ModLoader::GetInstance().OnPostLoadLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Start Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 3, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   ModLoader::GetInstance().OnStartLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Reset Level Hook");
    CKBehavior *rl = FindFirstBB(script, "reset Level");
    link = FindNextLink(rl, FindNextBB(rl, FindNextBB(rl, rl->GetInput(0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPreResetLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 4, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   ModLoader::GetInstance().OnPostResetLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Pause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 5, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   ModLoader::GetInstance().OnPauseLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Unpause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 6, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   ModLoader::GetInstance().OnUnpauseLevel();
                   return CKBR_OK;
               }));

    CKBehavior *bs = FindNextBB(script, FindFirstBB(script, "DeleteCollisionSurfaces"));

    GetLogger()->Info("Insert message Exit Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 7, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPreExitLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 ModLoader::GetInstance().OnPostExitLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Next Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 8, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPreNextLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 ModLoader::GetInstance().OnPostNextLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Dead Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 9, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   ModLoader::GetInstance().OnDead();
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
                 ModLoader::GetInstance().OnPreEndLevel();
                 return CKBR_OK;
             }));
    CreateLink(script, hs, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPostEndLevel();
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
        ModLoader::GetInstance().OpenModsMenu();
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
        ModLoader::GetInstance().OnCamNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(camonoff, coff, ExecuteBB::CreateHookBlock(camonoff, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnCamNavInactive();
        return CKBR_OK;
    }), 0, 0);
    FindBB(ballonoff, [ballon, balloff, &bon, &boff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == ballon) bon = beh;
        if (msg == balloff) boff = beh;
        return true;
    }, "Wait Message");
    CreateLink(ballonoff, bon, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnBallNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(ballonoff, boff, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnBallNavInactive();
        return CKBR_OK;
    }), 0, 0);

    GetLogger()->Info("Debug Ball Force");
    CKBehavior *ballNav = FindFirstBB(script, "Ball Navigation");
    CKBehavior *nop[2] = {nullptr, nullptr};
    FindBB(ballNav, [&nop](CKBehavior *beh) {
        if (nop[0]) nop[1] = beh;
        else nop[0] = beh;
        return !nop[1];
    }, "Nop");
    CKBehavior *keyevent[2] = {CreateBB(ballNav, VT_CONTROLLERS_KEYEVENT), CreateBB(ballNav, VT_CONTROLLERS_KEYEVENT)};
    m_BallForce[0] = CreateParamValue(ballNav, "Up", CKPGUID_KEY, CKKEYBOARD(0));
    m_BallForce[1] = CreateParamValue(ballNav, "Down", CKPGUID_KEY, CKKEYBOARD(0));
    CKBehavior *phyforce[2] = {CreateBB(ballNav, PHYSICS_RT_PHYSICSFORCE, true),
                               CreateBB(ballNav, PHYSICS_RT_PHYSICSFORCE, true)};
    CKBehavior *op = FindFirstBB(ballNav, "Op");
    CKParameter *mass = op->GetInputParameter(0)->GetDirectSource();
    CKBehavior *spf = FindFirstBB(ballNav, "SetPhysicsForce");
    CKParameter *dir[2] = {CreateParamValue(ballNav, "Up", CKPGUID_VECTOR, VxVector(0, 1, 0)),
                           CreateParamValue(ballNav, "Down", CKPGUID_VECTOR, VxVector(0, -1, 0))};
    CKBehavior *wake = FindFirstBB(ballNav, "Physics WakeUp");

    for (int i = 0; i < 2; i++) {
        keyevent[i]->GetInputParameter(0)->SetDirectSource(m_BallForce[i]);
        CreateLink(ballNav, nop[0], keyevent[i], 0, 0);
        CreateLink(ballNav, nop[1], keyevent[i], 0, 1);
        phyforce[i]->GetTargetParameter()->ShareSourceWith(spf->GetTargetParameter());
        phyforce[i]->GetInputParameter(0)->ShareSourceWith(spf->GetInputParameter(0));
        phyforce[i]->GetInputParameter(1)->ShareSourceWith(spf->GetInputParameter(1));
        phyforce[i]->GetInputParameter(2)->SetDirectSource(dir[i]);
        phyforce[i]->GetInputParameter(3)->ShareSourceWith(spf->GetInputParameter(3));
        phyforce[i]->GetInputParameter(4)->SetDirectSource(mass);
        CreateLink(ballNav, keyevent[i], phyforce[i], 0, 0);
        CreateLink(ballNav, keyevent[i], phyforce[i], 1, 1);
        CreateLink(ballNav, nop[1], phyforce[i], 0, 1);
        CreateLink(ballNav, phyforce[i], wake, 0, 0);
        CreateLink(ballNav, phyforce[i], wake, 1, 0);
    }

    CKBehavior *ballMgr = FindFirstBB(script, "BallManager");
    m_DynamicPos = FindNextBB(script, ballMgr, "TT Set Dynamic Position");
    CKBehavior *deactBall = FindFirstBB(ballMgr, "Deactivate Ball");
    CKBehavior *pieces = FindFirstBB(deactBall, "reset Ballpieces");
    m_OverclockLinks[0] = FindNextLink(deactBall, pieces);
    CKBehavior *unphy = FindNextBB(deactBall, FindNextBB(deactBall, m_OverclockLinks[0]->GetOutBehaviorIO()->GetOwner()));
    m_OverclockLinkIO[0][1] = unphy->GetInput(1);

    CKBehavior *newBall = FindFirstBB(ballMgr, "New Ball");
    m_PhysicsNewBall = FindFirstBB(newBall, "physicalize new Ball");
    m_OverclockLinks[1] = FindPreviousLink(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, m_PhysicsNewBall))));
    m_OverclockLinkIO[1][1] = m_PhysicsNewBall->GetInput(0);

    CKBehavior *trafoMgr = FindFirstBB(script, "Trafo Manager");
    m_SetNewBall = FindFirstBB(trafoMgr, "set new Ball");
    CKBehavior *sop = FindFirstBB(m_SetNewBall, "Switch On Parameter");
    m_CurTrafo = sop->GetInputParameter(0)->GetDirectSource();
    m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
    m_IngameParam = m_BML->GetArrayByName("IngameParameter");
}

void BMLMod::OnEditScript_Gameplay_Energy(CKBehavior *script) {
    GetLogger()->Info("Insert Counter Active/Inactive Hook");
    CKBehavior *som = FindFirstBB(script, "Switch On Message");
    InsertBB(script, FindNextLink(script, som, nullptr, 3), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnCounterActive();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, som, nullptr, 1), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnCounterInactive();
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
        ModLoader::GetInstance().OnPreLifeUp();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, lu, "add Life"), luhook);
    CreateLink(script, FindEndOfChain(script, luhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPostLifeUp();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, bo, "Delayer"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnBallOff();
        return CKBR_OK;
    }));
    CKBehavior *slhook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPreSubLife();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, sl, "sub Life"), slhook);
    CreateLink(script, FindEndOfChain(script, slhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPostSubLife();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, ep, "Show"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnExtraPoint();
        return CKBR_OK;
    }));

    CKBehavior *delay = FindFirstBB(script, "Delayer");
    m_OverclockLinks[2] = FindPreviousLink(script, delay);
    CKBehaviorLink *link = FindNextLink(script, delay);
    m_OverclockLinkIO[2][1] = link->GetOutBehaviorIO();

    for (int i = 0; i < 3; i++) {
        m_OverclockLinkIO[i][0] = m_OverclockLinks[i]->GetOutBehaviorIO();
        if (m_Overclock->GetBoolean())
            m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][1]);
    }
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
        ModLoader::GetInstance().OnPreCheckpointReached();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, cp, "set Resetpoint"), hook);
    CreateLink(script, FindEndOfChain(script, hook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnPostCheckpointReached();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, go, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnGameOver();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, lf, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        ModLoader::GetInstance().OnLevelFinish();
        return CKBR_OK;
    }));

    CKBehavior *id = FindNextBB(script, script->GetInput(0));
    m_CurSector = id->GetOutputParameter(0)->GetDestination(0);
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
    if (!IsInTravelCam())
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

void BMLMod::OnProcess_Suicide() {
    if (m_EnableSuicide->GetBoolean() && !m_SuicideCd && m_InputHook->IsKeyPressed(m_Suicide->GetKey())) {
        ModLoader::GetInstance().ExecuteCommand("kill");
        m_BML->AddTimer(1000.0f, [this]() { m_SuicideCd = false; });
        m_SuicideCd = true;
    }
}

void BMLMod::OnProcess_ChangeSpeed() {
    bool speedup = m_InputHook->IsKeyDown(m_SpeedupBall->GetKey());
    if (speedup && !m_Speedup)
        ModLoader::GetInstance().ExecuteCommand("speed 3");
    if (!speedup && m_Speedup)
        ModLoader::GetInstance().ExecuteCommand("speed 1");
    m_Speedup = speedup;
}

void BMLMod::OnProcess_ChangeBall() {
    if (m_ChangeBallCd != 0) {
        m_ChangeBallCd--;
        return;
    }

    for (int i = 0; i < 3; i++) {
        if (m_InputHook->IsKeyPressed(m_ChangeBall[i]->GetKey())) {
            CKMessageManager *mm = m_BML->GetMessageManager();
            CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

            mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Gameplay"));
            mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Sound"));
            m_ChangeBallCd = 2;

            m_BML->AddTimer(2ul, [this, i]() {
                auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
                ExecuteBB::Unphysicalize(curBall);

                static char trafoTypes[3][6] = {"paper", "wood", "stone"};
                SetParamString(m_CurTrafo, trafoTypes[i]);
                m_SetNewBall->ActivateInput(0);
                m_SetNewBall->Activate();

                GetLogger()->Info("Set to %s Ball", i == 0 ? "Paper" : i == 1 ? "Wood" : "Stone");
            });
        }
    }
}

void BMLMod::OnProcess_ResetBall() {
    if (m_InputHook->IsKeyPressed(m_ResetBall->GetKey())) {
        CKMessageManager *mm = m_BML->GetMessageManager();
        CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

        mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Gameplay"));
        mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Sound"));

        m_BML->AddTimer(2ul, [this]() {
            auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
            if (curBall) {
                ExecuteBB::Unphysicalize(curBall);

                CKDataArray *ph = m_BML->GetArrayByName("PH");
                for (int i = 0; i < ph->GetRowCount(); i++) {
                    CKBOOL set = true;
                    char name[100];
                    ph->GetElementStringValue(i, 1, name);
                    if (!strcmp(name, "P_Extra_Point"))
                        ph->SetElementValue(i, 4, &set);
                }

                m_IngameParam->SetElementValueFromParameter(0, 1, m_CurSector);
                m_IngameParam->SetElementValueFromParameter(0, 2, m_CurSector);
                CKBehavior *sectorMgr = m_BML->GetScriptByName("Gameplay_SectorManager");
                m_CKContext->GetCurrentScene()->Activate(sectorMgr, true);

                m_BML->AddTimerLoop(1ul, [this, curBall, sectorMgr]() {
                    if (sectorMgr->IsActive())
                        return true;

                    m_DynamicPos->ActivateInput(1);
                    m_DynamicPos->Activate();

                    m_BML->AddTimer(1ul, [this, curBall]() {
                        VxMatrix matrix;
                        m_CurLevel->GetElementValue(0, 3, &matrix);
                        curBall->SetWorldMatrix(matrix);

                        CK3dEntity *camMF = m_BML->Get3dEntityByName("Cam_MF");
                        m_BML->RestoreIC(camMF, true);
                        camMF->SetWorldMatrix(matrix);

                        m_BML->AddTimer(1ul, [this]() {
                            m_DynamicPos->ActivateInput(0);
                            m_DynamicPos->Activate();

                            m_PhysicsNewBall->ActivateInput(0);
                            m_PhysicsNewBall->Activate();
                            m_PhysicsNewBall->GetParent()->Activate();

                            GetLogger()->Info("Sector Reset");
                        });
                    });

                    return false;
                });
            }
        });
    }
}

void BMLMod::OnProcess_Travel() {
    VxVector vect;
    VxQuaternion quat;

    if (IsInTravelCam()) {
        if (m_InputHook->IsKeyDown(CKKEY_1)) {
            m_TravelSpeed = 0.2f;
        } else if (m_InputHook->IsKeyDown(CKKEY_2)) {
            m_TravelSpeed = 0.4f;
        } else if (m_InputHook->IsKeyDown(CKKEY_3)) {
            m_TravelSpeed = 0.8f;
        } else if (m_InputHook->IsKeyDown(CKKEY_4)) {
            m_TravelSpeed = 1.6f;
        } else if (m_InputHook->IsKeyDown(CKKEY_5)) {
            m_TravelSpeed = 2.4f;
        }

        if (m_InputHook->IsKeyDown(CKKEY_W)) {
            vect = VxVector(0, 0, m_TravelSpeed * m_DeltaTime);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_S)) {
            vect = VxVector(0, 0, -m_TravelSpeed * m_DeltaTime);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_A)) {
            vect = VxVector(-m_TravelSpeed * m_DeltaTime, 0, 0);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_D)) {
            vect = VxVector(m_TravelSpeed * m_DeltaTime, 0, 0);
            m_TravelCam->Translate(&vect, m_TravelCam);
        }
        if (m_InputHook->IsKeyDown(CKKEY_SPACE)) {
            vect = VxVector(0, m_TravelSpeed * m_DeltaTime, 0);
            m_TravelCam->Translate(&vect);
        }
        if (m_InputHook->IsKeyDown(CKKEY_LSHIFT)) {
            vect = VxVector(0, -m_TravelSpeed * m_DeltaTime, 0);
            m_TravelCam->Translate(&vect);
        }

        int width = m_BML->GetRenderContext()->GetWidth();
        int height = m_BML->GetRenderContext()->GetHeight();

        VxVector delta;
        m_InputHook->GetMouseRelativePosition(delta);
        delta.x = static_cast<float>(std::fmod(delta.x, width));
        delta.y = static_cast<float>(std::fmod(delta.y, height));

        vect = VxVector(0, 1, 0);
        m_TravelCam->Rotate(&vect, -delta.x * 2 / width);
        vect = VxVector(1, 0, 0);
        m_TravelCam->Rotate(&vect, -delta.y * 2 / height, m_TravelCam);
    } else if (m_CamOn->GetBoolean()) {
        if (m_InputHook->IsKeyPressed(m_Cam45->GetKey())) {
            vect = VxVector(0, 1, 0);
            m_CamOrientRef->Rotate(&vect, PI / 4, m_CamOrientRef);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamRot[0]->GetKey())) {
            vect = VxVector(0, 1, 0);
            m_CamOrientRef->Rotate(&vect, -0.01f * m_DeltaTime, m_CamOrientRef);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamRot[1]->GetKey())) {
            vect = VxVector(0, 1, 0);
            m_CamOrientRef->Rotate(&vect, 0.01f * m_DeltaTime, m_CamOrientRef);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamY[0]->GetKey())) {
            vect = VxVector(0, 0.15f * m_DeltaTime, 0);
            m_CamPos->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamY[1]->GetKey())) {
            vect = VxVector(0, -0.15f * m_DeltaTime, 0);
            m_CamPos->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamZ[0]->GetKey())) {
            VxVector position;
            m_CamPos->GetPosition(&position, m_CamOrientRef);
            position.z = (std::min)(position.z + 0.1f * m_DeltaTime, -0.1f);
            m_CamPos->SetPosition(&position, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamZ[1]->GetKey())) {
            vect = VxVector(0, 0, -0.1f * m_DeltaTime);
            m_CamPos->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->IsKeyDown(m_CamReset->GetKey())) {
            VxQuaternion rotation;
            m_CamOrientRef->GetQuaternion(&rotation, m_CamTarget);
            if (rotation.angle > 0.9f)
                rotation = VxQuaternion();
            else {
                rotation = rotation + VxQuaternion();
                rotation *= 0.5f;
            }
            m_CamOrientRef->SetQuaternion(&rotation, m_CamTarget);
            m_CamOrient->SetQuaternion(&quat, m_CamOrientRef);
            vect = VxVector(0, 35, -22);
            m_CamPos->SetPosition(&vect, m_CamOrient);
        }
    }
}

void BMLMod::OnProcess_AddLife() {
    if (!m_AddLifeCd && m_InputHook->IsKeyPressed(m_AddLife->GetKey())) {
        CKMessageManager *mm = m_BML->GetMessageManager();
        CKMessageType addLife = mm->AddMessageType("Life_Up");

        mm->SendMessageSingle(addLife, m_BML->GetGroupByName("All_Gameplay"));
        mm->SendMessageSingle(addLife, m_BML->GetGroupByName("All_Sound"));
        m_AddLifeCd = true;
        m_BML->AddTimer(1000.0f, [this]() { m_AddLifeCd = false; });
    }
}

void BMLMod::OnProcess_Summon() {
    VxVector vect;

    if (m_CurSel < 0) {
        for (int i = 0; i < 4; i++) {
            if (m_InputHook->IsKeyDown(m_AddBall[i]->GetKey())) {
                m_CurSel = i;
                m_InputHook->SetBlock(true);
            }
        }

        if (m_CurSel >= 0) {
            m_CurObj = (CK3dEntity *) m_CKContext->CopyObject(m_Balls[m_CurSel]);
            vect = VxVector(0, 5, 0);
            m_CurObj->SetPosition(&vect, m_CamTarget);
            m_CurObj->Show();
        }
    } else if (m_InputHook->oIsKeyDown(m_AddBall[m_CurSel]->GetKey())) {
        if (m_InputHook->oIsKeyDown(m_MoveKeys[0]->GetKey())) {
            vect = VxVector(0, 0, 0.1f * m_DeltaTime);
            m_CurObj->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[1]->GetKey())) {
            vect = VxVector(0, 0, -0.1f * m_DeltaTime);
            m_CurObj->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[2]->GetKey())) {
            vect = VxVector(-0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[3]->GetKey())) {
            vect = VxVector(0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[4]->GetKey())) {
            vect = VxVector(0, 0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vect, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[5]->GetKey())) {
            vect = VxVector(0, -0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vect, m_CamOrientRef);
        }
    } else {
        CKMesh *mesh = m_CurObj->GetMesh(0);
        switch (m_CurSel) {
            case 0:
                ExecuteBB::PhysicalizeConvex(m_CurObj, false, 0.5f, 0.4f, 0.2f, "", false, true, false, 1.5f, 0.1f, mesh->GetName(), VxVector(0, 0, 0), mesh);
                break;
            case 1:
                ExecuteBB::PhysicalizeBall(m_CurObj, false, 0.6f, 0.2f, 2.0f, "", false, true, false, 0.6f, 0.1f, mesh->GetName());
                break;
            case 2:
                ExecuteBB::PhysicalizeBall(m_CurObj, false, 0.7f, 0.1f, 10.0f, "", false, true, false, 0.2f, 0.1f, mesh->GetName());
                break;
            default:
                ExecuteBB::PhysicalizeConvex(m_CurObj, false, 0.7f, 0.3f, 1.0f, "", false, true, false, 0.1f, 0.1f, mesh->GetName(), VxVector(0, 0, 0), mesh);
                break;
        }

        CKDataArray *ph = m_BML->GetArrayByName("PH");
        ph->AddRow();
        int index = ph->GetRowCount() - 1;
        ph->SetElementValueFromParameter(index, 0, m_CurSector);
        static char P_BALL_NAMES[4][13] = {"P_Ball_Paper", "P_Ball_Wood", "P_Ball_Stone", "P_Box"};
        ph->SetElementStringValue(index, 1, P_BALL_NAMES[m_CurSel]);
        VxMatrix matrix = m_CurObj->GetWorldMatrix();
        ph->SetElementValue(index, 2, &matrix);
        ph->SetElementObject(index, 3, m_CurObj);
        CKBOOL set = false;
        ph->SetElementValue(index, 4, &set);

        CKGroup *depth = m_BML->GetGroupByName("DepthTest");
        depth->AddObject(m_CurObj);
        m_TempBalls.emplace_back(index, m_CurObj);

        m_CurSel = -1;
        m_CurObj = nullptr;
        m_InputHook->SetBlock(false);

        GetLogger()->Info("Summoned a %s", m_CurSel < 2 ? m_CurSel == 0 ? "Paper Ball" : "Wood Ball" : m_CurSel == 2 ? "Stone Ball" : "Box");
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

void BMLMod::OnProcess_SkipRender() {
    m_SkipRender = (m_BML->IsCheatEnabled() && m_InputHook->IsKeyDown(m_SkipRenderKey->GetKey()));
    if (m_SkipRender)
        m_RenderContext->ChangeCurrentRenderOptions(0, CK_RENDER_DEFAULTSETTINGS);
    else
        m_RenderContext->ChangeCurrentRenderOptions(CK_RENDER_DEFAULTSETTINGS, 0);
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
                ModLoader::GetInstance().ExecuteCommand(m_CmdInput->GetText() + 1);
            } else {
                AddIngameMessage(m_CmdInput->GetText());
            }
        case CKKEY_ESCAPE:
            m_CmdTyping = false;
            ModLoader::GetInstance().AddTimerLoop(1ul, [this, key] {
                if (m_CmdTyping)
                    return false;
                InputHook *input = ModLoader::GetInstance().GetInputManager();
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
                m_CmdInput->SetText(('/' + ModLoader::GetInstance().TabCompleteCommand(m_CmdInput->GetText() + 1)).c_str());
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