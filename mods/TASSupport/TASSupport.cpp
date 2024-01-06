#include "TASSupport.h"

#include <cstdio>
#include <ctime>
#include <sys/stat.h>

#include <MinHook.h>

TASSupport *g_Mod = nullptr;

IMod *BMLEntry(IBML *bml) {
    g_Mod = new TASSupport(bml);
    return g_Mod;
}

void BMLExit(IMod *mod) {
    delete mod;
}

char *UncompressDataFromFile(int &size, const char *filename) {
    if (!filename) return nullptr;

    FILE *fp = fopen(filename, "rb");
    if (!fp) return nullptr;

    fseek(fp, 0, SEEK_END);
    int nsize = ftell(fp) - 4;
    fseek(fp, 0, SEEK_SET);

    char *buffer = new char[nsize];
    fread(&size, sizeof(int), 1, fp);
    fread(buffer, nsize, 1, fp);
    fclose(fp);

    char *res = CKUnPackData(size, buffer, nsize);
    delete[] buffer;

    return res;
}

bool CompressDataToFile(char *data, int size, const char *filename) {
    if (!data || size == 0 || !filename)
        return false;

    int nsize;
    char *res = CKPackData(data, size, nsize, 9);

    FILE *fp = fopen(filename, "wb");
    if (!fp) return false;

    fwrite(&size, sizeof(int), 1, fp);
    fwrite(res, nsize, 1, fp);
    fclose(fp);

    CKDeletePointer(res);

    return true;
}

typedef CKERROR(CKBaseManager::*PreProcessFunc)();

class TimeManagerHook : public CKTimeManager {
public:
    CKERROR PreProcessHook() {
        CKERROR ret = (this->*s_PreProcessFuncOrig)();
        g_Mod->OnPreProcessTime();
        return ret;
    }

    static PreProcessFunc s_PreProcessFunc;
    static PreProcessFunc s_PreProcessFuncOrig;
    static PreProcessFunc s_PreProcessFuncTarget;
};

PreProcessFunc TimeManagerHook::s_PreProcessFunc = reinterpret_cast<PreProcessFunc>(&TimeManagerHook::PreProcessHook);
PreProcessFunc TimeManagerHook::s_PreProcessFuncOrig = nullptr;
PreProcessFunc TimeManagerHook::s_PreProcessFuncTarget = nullptr;

class InputManagerHook : public CKInputManager {
public:
    CKERROR PreProcessHook() {
        CKERROR ret = (this->*s_PreProcessFuncOrig)();
        g_Mod->OnPreProcessInput();
        return ret;
    }

    static PreProcessFunc s_PreProcessFunc;
    static PreProcessFunc s_PreProcessFuncOrig;
    static PreProcessFunc s_PreProcessFuncTarget;
};

PreProcessFunc InputManagerHook::s_PreProcessFunc = reinterpret_cast<PreProcessFunc>(&InputManagerHook::PreProcessHook);
PreProcessFunc InputManagerHook::s_PreProcessFuncOrig = nullptr;
PreProcessFunc InputManagerHook::s_PreProcessFuncTarget = nullptr;

void TASSupport::OnLoad() {
    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");

    m_Enabled = GetConfig()->GetProperty("Misc", "Enable");
    m_Enabled->SetComment("Enable TAS Features");
    m_Enabled->SetDefaultBoolean(false);

    m_Record = GetConfig()->GetProperty("Misc", "Record");
    m_Record->SetComment("Record Actions");
    m_Record->SetDefaultBoolean(false);

    m_StopKey = GetConfig()->GetProperty("Misc", "StopKey");
    m_StopKey->SetComment("Key for stop playing");
    m_StopKey->SetDefaultKey(CKKEY_F3);

    m_ShowKeys = GetConfig()->GetProperty("Misc", "ShowKeysGui");
    m_ShowKeys->SetComment("Show realtime keyboard behavior for TAS records");
    m_ShowKeys->SetDefaultBoolean(true);

    m_SkipRender = GetConfig()->GetProperty("Misc", "SkipRenderUntil");
    m_SkipRender->SetComment("Skip render until the given frame to speed up TAS playing");
    m_SkipRender->SetDefaultInteger(0);

    m_ExitOnDead = GetConfig()->GetProperty("Misc", "ExitOnDead");
    m_ExitOnDead->SetComment("Automatically exit game when ball fell");
    m_ExitOnDead->SetDefaultBoolean(false);

    m_ExitOnFinish = GetConfig()->GetProperty("Misc", "ExitOnFinish");
    m_ExitOnFinish->SetComment("Automatically exit game when TAS playing finished");
    m_ExitOnFinish->SetDefaultBoolean(false);

    m_ExitKey = GetConfig()->GetProperty("Misc", "ExitKey");
    m_ExitKey->SetComment("Press to exit game quickly");
    m_ExitKey->SetDefaultKey(CKKEY_DELETE);

    m_LoadTAS = GetConfig()->GetProperty("Misc", "AutoLoadTAS");
    m_LoadTAS->SetComment("Automatically load TAS record on game startup");
    m_LoadTAS->SetDefaultString("");

    m_LoadLevel = GetConfig()->GetProperty("Misc", "AutoLoadLevel");
    m_LoadLevel->SetComment("Automatically load given level on game startup");
    m_LoadLevel->SetDefaultInteger(0);

    VxMakeDirectory("..\\ModLoader\\TASRecords\\");

    m_TimeManager = m_BML->GetTimeManager();
    m_InputManager = (CKInputManager *)m_BML->GetCKContext()->GetManagerByGuid(INPUT_MANAGER_GUID);

    if (m_Enabled->GetBoolean()) {
        if (MH_Initialize() != MH_OK)
            return;

        void **vtableTimeManager = reinterpret_cast<void**>(*reinterpret_cast<void**>(m_TimeManager));
        TimeManagerHook::s_PreProcessFuncTarget = *reinterpret_cast<PreProcessFunc *>(&vtableTimeManager[5]);
        if (MH_CreateHook(*reinterpret_cast<LPVOID*>(&TimeManagerHook::s_PreProcessFuncTarget),
                          *reinterpret_cast<LPVOID*>(&TimeManagerHook::s_PreProcessFunc),
                          reinterpret_cast<LPVOID*>(&TimeManagerHook::s_PreProcessFuncOrig)) != MH_OK ||
            MH_EnableHook(*reinterpret_cast<LPVOID*>(&TimeManagerHook::s_PreProcessFuncTarget)) != MH_OK) {
            GetLogger()->Error("Create Time Manager Hook Failed");
            return;
        }

        void **vtableInputManager = reinterpret_cast<void**>(*reinterpret_cast<void**>(m_InputManager));
        InputManagerHook::s_PreProcessFuncTarget = *reinterpret_cast<PreProcessFunc *>(&vtableInputManager[5]);
        if (MH_CreateHook(*reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFuncTarget),
                          *reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFunc),
                          reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFuncOrig)) != MH_OK ||
            MH_EnableHook(*reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFuncTarget)) != MH_OK) {
            GetLogger()->Error("Create Input Manager Hook Failed");
            return;
        }

        m_TASEntryGui = new BGui::Gui();
        m_TASEntry = m_TASEntryGui->AddSmallButton("M_Enter_TAS_Records", "TAS", 0.88f, 0.61f, [this]() {
            m_ExitStart->ActivateInput(0);
            m_ExitStart->Activate();
            m_TASListGui->SetVisible(true);
        });
        m_TASEntry->SetActive(false);

        m_TASListGui = new GuiTASList();
        m_TASListGui->SetVisible(false);
        m_TASListGui->Init(0, 12);

        m_KeysGui = new BGui::Gui();
        m_KeysGui->AddPanel("M_TAS_Keys_Bg", VxColor(0, 0, 0, 180), 0.28f, 0.74f, 0.45f, 0.12f);
        m_ButtonUp = m_KeysGui->AddSmallButton("M_TAS_Keys_Up", "^", 0.76f, 0.56f);
        m_ButtonDown = m_KeysGui->AddSmallButton("M_TAS_Keys_Down", "v", 0.8f, 0.56f);
        m_ButtonLeft = m_KeysGui->AddSmallButton("M_TAS_Keys_Left", "<", 0.8f, 0.48f);
        m_ButtonRight = m_KeysGui->AddSmallButton("M_TAS_Keys_Right", ">", 0.8f, 0.64f);
        m_ButtonShift = m_KeysGui->AddSmallButton("M_TAS_Keys_Shift", "Shift", 0.8f, 0.30f);
        m_ButtonSpace = m_KeysGui->AddSmallButton("M_TAS_Keys_Space", "Space", 0.8f, 0.38f);
        m_ButtonQ = m_KeysGui->AddSmallButton("M_TAS_Keys_Q", "Q", 0.76f, 0.38f);
        m_ButtonEsc = m_KeysGui->AddSmallButton("M_TAS_Keys_Esc", "Esc", 0.76f, 0.30f);
        m_FrameCountLabel = m_KeysGui->AddTextLabel("M_TAS_FrameCount", "", ExecuteBB::GAMEFONT_01, 0.0f, 0.7f, 1.0f,
                                                    0.0353f);
        m_FrameCountLabel->SetAlignment(ALIGN_CENTER);
        m_ButtonUp->SetActive(false);
        m_ButtonDown->SetActive(false);
        m_ButtonLeft->SetActive(false);
        m_ButtonRight->SetActive(false);
        m_ButtonShift->SetActive(false);
        m_ButtonSpace->SetActive(false);
        m_ButtonQ->SetActive(false);
        m_ButtonEsc->SetActive(false);
        m_KeysGui->SetVisible(false);
    }
}

void TASSupport::OnUnload() {
    if (m_Enabled->GetBoolean()) {
        delete m_KeysGui;
        delete m_TASListGui;
        delete m_TASEntryGui;

        MH_DisableHook(*reinterpret_cast<void **>(&TimeManagerHook::s_PreProcessFuncTarget));
        MH_RemoveHook(*reinterpret_cast<void **>(&TimeManagerHook::s_PreProcessFuncTarget));

        MH_DisableHook(*reinterpret_cast<void **>(&InputManagerHook::s_PreProcessFuncTarget));
        MH_RemoveHook(*reinterpret_cast<void **>(&InputManagerHook::s_PreProcessFuncTarget));

        MH_Uninitialize();
    }
}

void TASSupport::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
        m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
    }

    if (!strcmp(filename, "3D Entities\\Menu.nmo")) {
        m_Level01 = m_BML->Get2dEntityByName("M_Start_But_01");
        CKBehavior *menuStart = m_BML->GetScriptByName("Menu_Start");
        m_ExitStart = ScriptHelper::FindFirstBB(menuStart, "Exit");
        m_Keyboard = m_BML->GetArrayByName("Keyboard");
        CKBehavior *menuMain = m_BML->GetScriptByName("Menu_Main");
        m_ExitMain = ScriptHelper::FindFirstBB(menuMain, "Exit", false, 1, 0);
    }

    if (isMap) {
        m_MapName = filename;
        m_MapName = m_MapName.substr(m_MapName.find_last_of('\\') + 1);
        m_MapName = m_MapName.substr(0, m_MapName.find_last_of('.'));
    }
}

void TASSupport::OnLoadScript(const char *filename, CKBehavior *script) {
    if (m_Enabled->GetBoolean()) {
        if (!strcmp(script->GetName(), "Ball_Explosion_Wood")
            || !strcmp(script->GetName(), "Ball_Explosion_Paper")
            || !strcmp(script->GetName(), "Ball_Explosion_Stone")) {
            CKBehavior *beh = ScriptHelper::FindFirstBB(script, "Set Position");
            ScriptHelper::DeleteBB(script, beh);
        }

        if (!strcmp(script->GetName(), "Balls_Init")) {
            m_InitPieces = ScriptHelper::FindFirstBB(script, "Init Ballpieces");
        }
    }
}

void TASSupport::OnProcess() {
    if (m_Enabled->GetBoolean()) {
#ifndef _DEBUG
        if (m_BML->IsCheatEnabled() && m_Recording)
            OnStop();
#endif

        if (m_TASEntryGui && m_Level01) {
            m_TASEntryGui->Process();
            bool visible = m_Level01->IsVisible();
            m_TASEntry->SetVisible(visible);

            if (m_TASListGui->IsVisible())
                m_TASListGui->Process();
        }

        if (m_Playing) {
            if (m_ShowKeys->GetBoolean() && m_KeysGui && m_CurFrame < m_RecordData.size()) {
                KeyState state = m_RecordData[m_CurFrame].keyState;
                state.key_up ? m_ButtonUp->OnMouseEnter() : m_ButtonUp->OnMouseLeave();
                m_ButtonUp->Process();
                state.key_down ? m_ButtonDown->OnMouseEnter() : m_ButtonDown->OnMouseLeave();
                m_ButtonDown->Process();
                state.key_left ? m_ButtonLeft->OnMouseEnter() : m_ButtonLeft->OnMouseLeave();
                m_ButtonLeft->Process();
                state.key_right ? m_ButtonRight->OnMouseEnter() : m_ButtonRight->OnMouseLeave();
                m_ButtonRight->Process();
                state.key_shift ? m_ButtonShift->OnMouseEnter() : m_ButtonShift->OnMouseLeave();
                m_ButtonShift->Process();
                state.key_space ? m_ButtonSpace->OnMouseEnter() : m_ButtonSpace->OnMouseLeave();
                m_ButtonSpace->Process();
                state.key_q ? m_ButtonQ->OnMouseEnter() : m_ButtonQ->OnMouseLeave();
                m_ButtonQ->Process();
                state.key_esc ? m_ButtonEsc->OnMouseEnter() : m_ButtonEsc->OnMouseLeave();
                m_ButtonEsc->Process();
                sprintf(m_FrameCountText, "#%d", m_CurFrame);
                m_FrameCountLabel->SetText(m_FrameCountText);
                m_FrameCountLabel->Process();
            }

            if (m_InputManager->IsKeyToggled(m_StopKey->GetKey()))
                OnStop();

            if (m_CurFrame < (std::size_t) m_SkipRender->GetInteger())
                m_BML->SkipRenderForNextTick();

            if (m_InputManager->IsKeyToggled(m_ExitKey->GetKey()))
                m_BML->ExitGame();
        }
    }
}

void TASSupport::OnBallOff() {
    if (m_Enabled->GetBoolean() && m_Playing && m_ExitOnDead->GetBoolean())
        m_BML->ExitGame();
}

void TASSupport::OnPostStartMenu() {
    static bool firstTime = true;

    if (firstTime) {
        std::string tasFile = m_LoadTAS->GetString();
        if (m_Enabled->GetBoolean() && !tasFile.empty()) {
            std::string tasPath = "..\\ModLoader\\TASRecords\\" + tasFile + ".tas";
            struct stat buf = {0};
            if (stat(tasPath.c_str(), &buf) == 0) {
                m_BML->SendIngameMessage(("Loading TAS Record: " + tasFile + ".tas").c_str());
                LoadTAS(tasPath);

                int level = m_LoadLevel->GetInteger();
                if (level >= 1 && level <= 13) {
                    m_BML->AddTimer(2ul, [this, level]() {
                        m_CurLevel->SetElementValue(0, 0, (void *) &level);

                        CKContext *ctx = m_BML->GetCKContext();
                        CKMessageManager *mm = ctx->GetMessageManager();
                        CKMessageType loadLevel = mm->AddMessageType("Load Level");
                        CKMessageType loadMenu = mm->AddMessageType("Menu_Load");

                        mm->SendMessageSingle(loadLevel, ctx->GetCurrentLevel());
                        mm->SendMessageSingle(loadMenu, m_BML->GetGroupByName("All_Sound"));
                        m_BML->Get2dEntityByName("M_BlackScreen")->Show(CKHIDE);
                        m_ExitMain->ActivateInput(0);
                        m_ExitMain->Activate();
                    });
                }
            } else
                m_BML->SendIngameMessage(("TAS file " + tasFile + ".tas not found.").c_str());
        }

        firstTime = false;
    }
}

void TASSupport::OnPreProcessInput() {
    if (m_Enabled->GetBoolean()) {
        if (m_Playing) {
            if (m_CurFrame < m_RecordData.size()) {
                KeyState state = m_RecordData[m_CurFrame++].keyState;
                CKBYTE *stateBuf = m_InputManager->GetKeyboardState();
                stateBuf[m_KeyUp] = state.key_up;
                stateBuf[m_KeyDown] = state.key_down;
                stateBuf[m_KeyLeft] = state.key_left;
                stateBuf[m_KeyRight] = state.key_right;
                stateBuf[CKKEY_Q] = state.key_q;
                stateBuf[m_KeyShift] = state.key_shift;
                stateBuf[m_KeySpace] = state.key_space;
                stateBuf[CKKEY_ESCAPE] = state.key_esc;
                stateBuf[CKKEY_RETURN] = state.key_enter;
            } else {
                OnStop();
            }
        } else if (m_Recording) {
            KeyState state = {};
            CKBYTE *stateBuf = m_InputManager->GetKeyboardState();
            state.key_up = stateBuf[m_KeyUp];
            state.key_down = stateBuf[m_KeyDown];
            state.key_left = stateBuf[m_KeyLeft];
            state.key_right = stateBuf[m_KeyRight];
            state.key_q = stateBuf[CKKEY_Q];
            state.key_shift = stateBuf[m_KeyShift];
            state.key_space = stateBuf[m_KeySpace];
            state.key_esc = stateBuf[CKKEY_ESCAPE];
            state.key_enter = stateBuf[CKKEY_RETURN];
            m_RecordData.rbegin()->keyState = state;
        }
    }
}

void TASSupport::OnPreProcessTime() {
    if (m_Enabled->GetBoolean()) {
        if (m_Playing) {
            if (m_CurFrame < m_RecordData.size()) {
                m_TimeManager->SetLastDeltaTime(m_RecordData[m_CurFrame].deltaTime);
            } else {
                OnStop();
            }
        } else if (m_Recording) {
            m_RecordData.emplace_back(m_TimeManager->GetLastDeltaTime());
        }
    }
}

void TASSupport::OnStart() {
    if (m_Enabled->GetBoolean()) {
        m_BML->AddTimer(1ul, [this]() {
            CKBaseManager *physicsManager = m_BML->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
            auto *averageTickTime = reinterpret_cast<float *>(reinterpret_cast<CKBYTE *>(physicsManager) + 0xC8);
            *averageTickTime = m_TimeManager->GetLastDeltaTime();
            CKBYTE *timer = *reinterpret_cast<CKBYTE **>(reinterpret_cast<CKBYTE *>(physicsManager) + 0xC0);
            *reinterpret_cast<double *>(timer + 0x120) = 0;
            *reinterpret_cast<double *>(timer + 0x128) = 1.0 / 66;
            *reinterpret_cast<double *>(timer + 0x130) = 0;
            *reinterpret_cast<CKDWORD *>(timer + 0x138) = 0;
            *reinterpret_cast<double *>(*reinterpret_cast<CKBYTE **>(timer + 0x4) + 0x18) = 0;
        });

        if (m_Keyboard) {
            m_Keyboard->GetElementValue(0, 0, &m_KeyUp);
            m_Keyboard->GetElementValue(0, 1, &m_KeyDown);
            m_Keyboard->GetElementValue(0, 2, &m_KeyLeft);
            m_Keyboard->GetElementValue(0, 3, &m_KeyRight);
            m_Keyboard->GetElementValue(0, 4, &m_KeyShift);
            m_Keyboard->GetElementValue(0, 5, &m_KeySpace);
        }

        if (m_ReadyToPlay) {
            m_LoadThread->join();

            m_ReadyToPlay = false;
            m_Playing = true;
            m_CurFrame = 0;
            m_BML->SendIngameMessage("Start playing TAS.");
            if (m_ShowKeys->GetBoolean())
                m_KeysGui->SetVisible(true);
        } else if (m_Record->GetBoolean()) {
            m_Recording = true;
            m_CurFrame = 0;
            m_RecordData.clear();
            m_BML->SendIngameMessage("Start recording TAS.");
        }
    }
}

void TASSupport::OnStop() {
    if (m_Enabled->GetBoolean()) {
        if (m_Playing || m_Recording) {
            if (m_Playing) {
                CKBYTE *stateBuf = m_InputManager->GetKeyboardState();
                stateBuf[m_KeyUp] = KS_IDLE;
                stateBuf[m_KeyDown] = KS_IDLE;
                stateBuf[m_KeyLeft] = KS_IDLE;
                stateBuf[m_KeyRight] = KS_IDLE;
                stateBuf[CKKEY_Q] = KS_IDLE;
                stateBuf[m_KeyShift] = KS_IDLE;
                stateBuf[m_KeySpace] = KS_IDLE;
                stateBuf[CKKEY_ESCAPE] = KS_IDLE;
                m_BML->SendIngameMessage("TAS playing stopped.");
                if (m_ExitOnFinish->GetBoolean())
                    m_BML->ExitGame();
            } else m_BML->SendIngameMessage("TAS recording stopped.");
            if (m_ShowKeys->GetBoolean())
                m_KeysGui->SetVisible(false);
            m_Playing = m_Recording = false;
            m_RecordData.clear();
            m_RecordData.shrink_to_fit();
            m_CurFrame = 0;
        }
    }
}

void TASSupport::OnFinish() {
    if (m_Enabled->GetBoolean()) {
        if (m_Recording) {
            m_BML->AddTimer(4ul, [this]() {
                static char filename[MAX_PATH];
                time_t stamp = time(nullptr);
                tm *curTime = localtime(&stamp);
                sprintf(filename, "%04d%02d%02d_%02d%02d%02d_%s.tas", curTime->tm_year + 1900, curTime->tm_mon + 1,
                        curTime->tm_mday, curTime->tm_hour, curTime->tm_min, curTime->tm_sec, m_MapName.c_str());
                m_BML->SendIngameMessage(("TAS record saved to " + std::string(filename)).c_str());
                const int count = (int)m_RecordData.size();
                std::thread([this, count]() {
                    char filepath[MAX_PATH];
                    sprintf(filepath, "..\\ModLoader\\TASRecords\\%s", filename);
                    CompressDataToFile((char *)&m_RecordData[0], count * (int)sizeof(FrameData), filepath);
                    OnStop();
                }).detach();
            });
        }
    }
}

void TASSupport::LoadTAS(const std::string &filename) {
    m_LoadThread = std::make_unique<std::thread>([=]() {
        int size = 0;
        char *data = UncompressDataFromFile(size, filename.c_str());
        if (data) {
            const int count = size / (int)sizeof(FrameData);
            m_RecordData.resize(count);
            for (int i = 0; i < count; i++) {
                memcpy(&m_RecordData[i], &data[i * sizeof(FrameData)], sizeof(FrameData));
            }
            CKDeletePointer(data);
        }
    });
    m_ReadyToPlay = true;
}

GuiTASList::GuiTASList() {
    m_Left = AddLeftButton("M_List_Left", 0.4f, 0.34f, [this]() { PreviousPage(); });
    m_Right = AddRightButton("M_List_Right", 0.4f, 0.6238f, [this]() { NextPage(); });
    AddBackButton("M_Opt_Mods_Back")->SetCallback([this]() { Exit(); });
    AddTextLabel("M_Opt_Mods_Title", "TAS Records", ExecuteBB::GAMEFONT_02, 0.35f, 0.07f, 0.3f, 0.05f);
}

void GuiTASList::Init(int size, int maxsize) {
    m_Size = size;
    m_MaxPage = (size + maxsize - 1) / maxsize;
    m_MaxSize = maxsize;
    m_CurPage = 0;

    for (int i = 0; i < m_MaxSize; i++) {
        BGui::Button *button = CreateButton(i);
        m_Buttons.push_back(button);
        BGui::Text *text = AddText(("M_TAS_Text_" + std::to_string(i)).c_str(), "", 0.44f, 0.15f + 0.057f * i, 0.3f,
                                   0.05f);
        text->SetZOrder(100);
        m_Texts.push_back(text);
    }
}

void GuiTASList::Resize(int size) {
    m_Size = size;
    m_MaxPage = (size + m_MaxSize - 1) / m_MaxSize;
}

void GuiTASList::RefreshRecords() {
    m_Records.clear();

    CKDirectoryParser tasTraverser("..\\ModLoader\\TASRecords", "*.tas", TRUE);
    for (char *tasPath = tasTraverser.GetNextFile(); tasPath != nullptr; tasPath = tasTraverser.GetNextFile()) {
        std::string tasFile = tasPath;
        TASInfo info;
        auto start = tasFile.find_last_of('\\') + 1;
        auto count = tasFile.find_last_of('.') - start;
        info.displayName = tasFile.substr(start, count);
        info.filepath = "..\\ModLoader\\TASRecords\\" + info.displayName + ".tas";
        m_Records.push_back(info);
    }

    std::sort(m_Records.begin(), m_Records.end());
}

void GuiTASList::SetPage(int page) {
    int size = (std::min)(m_MaxSize, m_Size - page * m_MaxSize);
    for (int i = 0; i < m_MaxSize; i++) {
        m_Buttons[i]->SetVisible(i < size);
        m_Texts[i]->SetVisible(i < size);
    }
    for (int i = 0; i < size; i++)
        m_Texts[i]->SetText(GetButtonText(page * m_MaxSize + i).c_str());

    m_CurPage = page;
    m_Left->SetVisible(page > 0);
    m_Right->SetVisible(page < m_MaxPage - 1);
}

void GuiTASList::SetVisible(bool visible) {
    Gui::SetVisible(visible);
    m_Visible = visible;
    if (visible) {
        RefreshRecords();
        Resize(m_Records.size());
        SetPage((std::max)((std::min)(m_CurPage, m_MaxPage - 1), 0));
    }
}

BGui::Button *GuiTASList::CreateButton(int index) {
    return AddLevelButton(("M_TAS_But_" + std::to_string(index)).c_str(), "", 0.15f + 0.057f * index, 0.4031f,
                          [this, index]() {
                              GuiTASList::Exit();
                              TASInfo &info = m_Records[m_CurPage * m_MaxSize + index];
                              g_Mod->GetBML()->SendIngameMessage(("Loading TAS Record: " + info.displayName).c_str());
                              g_Mod->LoadTAS(info.filepath);
                          });
}

std::string GuiTASList::GetButtonText(int index) {
    return m_Records[index].displayName;
}

void GuiTASList::Exit() {
    g_Mod->GetBML()->GetCKContext()->GetCurrentScene()->Activate(g_Mod->GetBML()->GetScriptByName("Menu_Main"), true);
    SetVisible(false);
}