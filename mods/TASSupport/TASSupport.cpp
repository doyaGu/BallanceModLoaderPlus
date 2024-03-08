#include "TASSupport.h"

#include <cstdio>
#include <ctime>
#include <sys/stat.h>

#include <MinHook.h>

#include <BML/Bui.h>

TASSupport *g_Mod = nullptr;

IMod *BMLEntry(IBML *bml) {
    g_Mod = new TASSupport(bml);
    return g_Mod;
}

void BMLExit(IMod *mod) {
    delete mod;
}

static CKDWORD GetPhysicsRTVersion() {
    CKPluginManager *pm = CKGetPluginManager();

    CKPluginEntry *entry = pm->FindComponent(CKGUID(0x6BED328B, 0x141F5148));
    if (!entry)
        return 0;

    return entry->m_PluginInfo.m_Version;
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

    m_ShowInfo = GetConfig()->GetProperty("Misc", "ShowInfoGui");
    m_ShowInfo->SetComment("Show realtime physics info");
    m_ShowInfo->SetDefaultBoolean(true);

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

    m_EnableDump = GetConfig()->GetProperty("Misc", "Dump Data");
    m_EnableDump->SetComment("Dump position data while playing");
    m_EnableDump->SetDefaultBoolean(false);

    VxMakeDirectory((CKSTRING) "..\\ModLoader\\TASRecords\\");

    m_PhysicsRTVersion = GetPhysicsRTVersion();
    if (m_PhysicsRTVersion == 0x000001) {
        InitPhysicsMethodPointers();
    }

    m_IpionManager = (CKIpionManager *)m_BML->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
    m_TimeManager = m_BML->GetTimeManager();
    m_InputHook = m_BML->GetInputManager();

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

        auto *inputManager = (CKInputManager *)m_BML->GetCKContext()->GetManagerByGuid(INPUT_MANAGER_GUID);
        void **vtableInputManager = reinterpret_cast<void**>(*reinterpret_cast<void**>(inputManager));
        InputManagerHook::s_PreProcessFuncTarget = *reinterpret_cast<PreProcessFunc *>(&vtableInputManager[5]);
        if (MH_CreateHook(*reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFuncTarget),
                          *reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFunc),
                          reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFuncOrig)) != MH_OK ||
            MH_EnableHook(*reinterpret_cast<LPVOID*>(&InputManagerHook::s_PreProcessFuncTarget)) != MH_OK) {
            GetLogger()->Error("Create Input Manager Hook Failed");
            return;
        }
    }
}

void TASSupport::OnUnload() {
    if (m_Enabled->GetBoolean()) {
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
        if (!strcmp(script->GetName(), "Gameplay_Ingame")) {
            for (int i = 0; i < script->GetLocalParameterCount(); ++i) {
                CKParameter *param = script->GetLocalParameter(i);
                if (!strcmp(param->GetName(), "ActiveBall")) {
                    m_ActiveBall = param;
                    break;
                }
            }
        }
    }
}

void TASSupport::OnProcess() {
    if (m_Enabled->GetBoolean()) {
        Bui::ImGuiContextScope scope;

#ifndef _DEBUG
        if (m_BML->IsCheatEnabled() && m_Recording)
            OnStop();
#endif

        if (m_Level01 && m_Level01->IsVisible()) {
            const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.61f, vpSize.y * 0.88f));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            constexpr ImGuiWindowFlags ButtonFlags = ImGuiWindowFlags_NoDecoration |
                                                     ImGuiWindowFlags_NoBackground |
                                                     ImGuiWindowFlags_NoMove |
                                                     ImGuiWindowFlags_NoNav |
                                                     ImGuiWindowFlags_AlwaysAutoResize |
                                                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                                                     ImGuiWindowFlags_NoFocusOnAppearing;

            if (ImGui::Begin("Button_TAS", nullptr, ButtonFlags)) {
                if (Bui::SmallButton("TAS")) {
                    m_ExitStart->ActivateInput(0);
                    m_ExitStart->Activate();
                    OpenTASMenu();
                }
            }
            ImGui::End();

            ImGui::PopStyleVar(2);
        }

        OnDrawMenu();

        if (m_Playing) {
            OnDrawKeys();
            OnDrawInfo();

            if (m_InputHook->IsKeyToggled(m_StopKey->GetKey()))
                OnStop();

            if (m_CurFrame < (std::size_t) m_SkipRender->GetInteger())
                m_BML->SkipRenderForNextTick();

            if (m_InputHook->IsKeyToggled(m_ExitKey->GetKey()))
                m_BML->ExitGame();
        }
    }
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

void TASSupport::OnExitGame() {
    m_Level01 = nullptr;
}

void TASSupport::OnBallOff() {
    if (m_Enabled->GetBoolean() && m_Playing && m_ExitOnDead->GetBoolean())
        m_BML->ExitGame();
}

void TASSupport::OnPreProcessInput() {
    if (m_Playing) {
        if (m_CurFrame < m_RecordData.size()) {
            KeyState state = m_RecordData[m_CurFrame++].keyState;
            CKBYTE *stateBuf = m_InputHook->GetKeyboardState();
            stateBuf[m_KeyUp] = state.key_up;
            stateBuf[m_KeyDown] = state.key_down;
            stateBuf[m_KeyLeft] = state.key_left;
            stateBuf[m_KeyRight] = state.key_right;
            stateBuf[CKKEY_Q] = state.key_q;
            stateBuf[m_KeyShift] = state.key_shift;
            stateBuf[m_KeySpace] = state.key_space;
            stateBuf[CKKEY_ESCAPE] = state.key_esc;
            stateBuf[CKKEY_RETURN] = state.key_enter;

            if (m_EnableDump->GetBoolean()) {
                auto *ball = GetActiveBall();
                if (ball) {
                    auto &data = m_DumpData.back();

                    if (m_PhysicsRTVersion == 0x000001) {
                        auto *obj = m_IpionManager->GetPhysicsObject0(ball);
                        if (obj) {
                            obj->GetPosition(&data.position, &data.angles);
                            obj->GetVelocity(&data.velocity, &data.angularVelocity);
                        }
                    } else if (m_PhysicsRTVersion == 0x000002) {
                        auto *obj = m_IpionManager->GetPhysicsObject(ball);
                        if (obj) {
                            obj->GetPosition(&data.position, &data.angles);
                            obj->GetVelocity(&data.velocity, &data.angularVelocity);
                        }
                    }
                }
            }
        } else {
            OnStop();
        }
    } else if (m_Recording) {
        KeyState state = {};
        CKBYTE *stateBuf = m_InputHook->GetKeyboardState();
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

void TASSupport::OnPreProcessTime() {
    if (m_Playing) {
        if (m_CurFrame < m_RecordData.size()) {
            m_TimeManager->SetLastDeltaTime(m_RecordData[m_CurFrame].deltaTime);

            if (m_EnableDump->GetBoolean()) {
                m_DumpData.emplace_back(m_TimeManager->GetLastDeltaTime());
            }
        } else {
            OnStop();
        }
    } else if (m_Recording) {
        m_RecordData.emplace_back(m_TimeManager->GetLastDeltaTime());
    }
}

void TASSupport::OnStart() {
    if (m_Enabled->GetBoolean()) {
        m_BML->AddTimer(1ul, [this]() {
            if (m_PhysicsRTVersion == 0x000001) {
                auto *env = *reinterpret_cast<CKBYTE **>(reinterpret_cast<CKBYTE *>(m_IpionManager) + 0xC0);
                *reinterpret_cast<double *>(env + 0x120) = 0;
                *reinterpret_cast<double *>(env + 0x128) = 1.0 / 66;
                *reinterpret_cast<double *>(env + 0x130) = 0;
                *reinterpret_cast<CKDWORD *>(env + 0x138) = 0;
                *reinterpret_cast<double *>(*reinterpret_cast<CKBYTE **>(env + 0x4) + 0x18) = 0;

                auto *time = reinterpret_cast<float *>(reinterpret_cast<CKBYTE *>(m_IpionManager) + 0xC8);
                *time = m_TimeManager->GetLastDeltaTime();
            } else if (m_PhysicsRTVersion == 0x000002) {
                m_IpionManager->ResetSimulationClock();
                m_IpionManager->SetDeltaTime(m_TimeManager->GetLastDeltaTime());
            }
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
                CKBYTE *stateBuf = m_InputHook->GetKeyboardState();
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
            m_Playing = m_Recording = false;
            m_RecordData.clear();
            m_RecordData.shrink_to_fit();
            m_CurFrame = 0;

            if (m_EnableDump->GetBoolean()) {
                m_DumpData.shrink_to_fit();
                m_BML->AddTimer(4ul, [this]() {
                    static char filename[MAX_PATH];
                    time_t stamp = time(nullptr);
                    tm *curTime = localtime(&stamp);
                    sprintf(filename, "%04d%02d%02d_%02d%02d%02d_%s.dump", curTime->tm_year + 1900, curTime->tm_mon + 1,
                            curTime->tm_mday, curTime->tm_hour, curTime->tm_min, curTime->tm_sec, m_MapName.c_str());
                    m_BML->SendIngameMessage(("TAS dump saved to " + std::string(filename)).c_str());
                    const int count = (int)m_DumpData.size();
                    std::thread([this, count]() {
                        char filepath[MAX_PATH];
                        sprintf(filepath, "..\\ModLoader\\TASRecords\\%s", filename);
                        CompressDataToFile((char *)&m_DumpData[0], count * (int)sizeof(DumpData), filepath);
                    }).detach();
                });
            }
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

void TASSupport::OnDrawMenu() {
    if (!m_ShowMenu)
        return;

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.3f, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(vpSize.x * 0.7f, vpSize.y), ImGuiCond_Appearing);

    constexpr auto TitleText = "TAS Records";

    constexpr ImGuiWindowFlags MenuWinFlags = ImGuiWindowFlags_NoDecoration |
                                              ImGuiWindowFlags_NoBackground |
                                              ImGuiWindowFlags_NoMove |
                                              ImGuiWindowFlags_NoScrollWithMouse |
                                              ImGuiWindowFlags_NoBringToFrontOnFocus |
                                              ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin(TitleText, nullptr, MenuWinFlags);

    {
        float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= 1.5f;
        ImGui::PushFont(ImGui::GetFont());

        const auto titleSize = ImGui::CalcTextSize(TitleText);
        ImGui::GetWindowDrawList()->AddText(ImVec2((vpSize.x - titleSize.x) / 2.0f, vpSize.y * 0.07f), IM_COL32_WHITE,
                                            TitleText);

        ImGui::GetFont()->Scale = oldScale;
        ImGui::PopFont();
    }

    int recordCount = (int)m_Records.size();
    int maxPage = ((recordCount % 13) == 0) ? recordCount / 13 : recordCount / 13 + 1;

    if (m_CurPage > 0) {
        ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.34f, 0.4f)));
        if (Bui::LeftButton("TASPrevPage")) {
            --m_CurPage;
        }
    }

    if (maxPage > 1 && m_CurPage < maxPage - 1) {
        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.6238f, vpSize.y * 0.4f));
        if (Bui::RightButton("TASNextPage")) {
            ++m_CurPage;
        }
    }

    bool v = true;
    const int n = m_CurPage * 13;
    for (int i = 0; i < 13 && n + i < recordCount; ++i) {
        auto &info = m_Records[n + i];

        ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.15f + (float) i * 0.06f)));
        if (Bui::LevelButton(info.name.c_str(), &v)) {
            ExitTASMenu();

            m_BML->SendIngameMessage(("Loading TAS Record: " + info.name).c_str());
            LoadTAS(info.path);
        }
    }

    ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.85f)));
    if (Bui::BackButton("TASBack") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_CurPage = 0;
        ExitTASMenu();
    }

    ImGui::End();
}

void TASSupport::OnDrawKeys() {
    if (m_ShowKeys->GetBoolean() && m_CurFrame < m_RecordData.size()) {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

        ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.28f, vpSize.y * 0.7f));
        ImGui::SetNextWindowSize(ImVec2(vpSize.x * 0.45f, vpSize.y * 0.15f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.57f));

        constexpr ImGuiWindowFlags WinFlags = ImGuiWindowFlags_NoDecoration |
                                              ImGuiWindowFlags_NoResize |
                                              ImGuiWindowFlags_NoMove |
                                              ImGuiWindowFlags_NoInputs |
                                              ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("TAS Keys", nullptr, WinFlags)) {
            ImDrawList *drawList = ImGui::GetWindowDrawList();

            KeyState state = m_RecordData[m_CurFrame].keyState;

            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.56f, 0.76f)), Bui::BUTTON_SMALL, state.key_up ? 1 : 2, "^");
            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.56f, 0.8f)), Bui::BUTTON_SMALL, state.key_down ? 1 : 2, "v");
            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.48f, 0.8f)), Bui::BUTTON_SMALL, state.key_left ? 1 : 2, "<");
            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.64f, 0.8f)), Bui::BUTTON_SMALL, state.key_right ? 1 : 2, ">");
            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.30f, 0.8f)), Bui::BUTTON_SMALL, state.key_shift ? 1 : 2, "Shift");
            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.38f, 0.8f)), Bui::BUTTON_SMALL, state.key_space ? 1 : 2, "Space");
            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.38f, 0.76f)), Bui::BUTTON_SMALL, state.key_q ? 1 : 2, "Q");
            Bui::AddButtonImage(drawList, Bui::CoordToScreenPos(ImVec2(0.30f, 0.76f)), Bui::BUTTON_SMALL, state.key_esc ? 1 : 2, "ESC");

            sprintf(m_FrameCountText, "#%d", m_CurFrame);
            const auto textSize = ImGui::CalcTextSize(m_FrameCountText);
            drawList->AddText(ImVec2((vpSize.x - textSize.x) / 2.0f, vpSize.y * 0.7f), IM_COL32_WHITE, m_FrameCountText);
        }
        ImGui::End();

        ImGui::PopStyleColor();
    }
}

void TASSupport::OnDrawInfo() {
    if (!m_ShowInfo->GetBoolean())
        return;

    constexpr ImGuiWindowFlags WinFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoNav |
                                          ImGuiWindowFlags_NoFocusOnAppearing |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("Info", nullptr, WinFlags)) {
        auto *ball = GetActiveBall();
        if (ball) {
            ImGui::Text("Active Ball: %s", ball->GetName());

            VxVector position, angles;
            VxVector velocity, angularVelocity;

            if (m_PhysicsRTVersion == 0x000001) {
                auto *obj = m_IpionManager->GetPhysicsObject0(ball);
                if (obj) {
                    obj->GetPosition(&position, &angles);
                    obj->GetVelocity(&velocity, &angularVelocity);
                }
            } else if (m_PhysicsRTVersion == 0x000002) {
                auto *obj = m_IpionManager->GetPhysicsObject(ball);
                if (obj) {
                    obj->GetPosition(&position, &angles);
                    obj->GetVelocity(&velocity, &angularVelocity);
                }
            }

            ImGui::Text("Position:");
            ImGui::Text("(%.3f, %.3f, %.3f)", position.x, position.y, position.z);
            ImGui::Text("Angles:");
            ImGui::Text("(%.3f, %.3f, %.3f)", angles.x, angles.y, angles.z);

            ImGui::Text("Velocity:");
            ImGui::Text("(%.3f, %.3f, %.3f)", velocity.x, velocity.y, velocity.z);
            ImGui::Text("Angular Velocity:");
            ImGui::Text("(%.3f, %.3f, %.3f)", angularVelocity.x, angularVelocity.y, angularVelocity.z);
        }
    }
    ImGui::End();
}

void TASSupport::OpenTASMenu() {
    m_ShowMenu = true;
    m_InputHook->Block(CK_INPUT_DEVICE_KEYBOARD);

    RefreshRecords();
}

void TASSupport::ExitTASMenu() {
    m_ShowMenu = false;

    CKBehavior *beh = m_BML->GetScriptByName("Menu_Start");
    m_BML->GetCKContext()->GetCurrentScene()->Activate(beh, true);

    m_BML->AddTimerLoop(1ul, [this] {
        if (m_InputHook->oIsKeyDown(CKKEY_ESCAPE) || m_InputHook->oIsKeyDown(CKKEY_RETURN))
            return true;
        m_InputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });
}

void TASSupport::RefreshRecords() {
    m_Records.clear();

    CKDirectoryParser tasTraverser((CKSTRING)"..\\ModLoader\\TASRecords", "*.tas", TRUE);
    for (char *tasPath = tasTraverser.GetNextFile(); tasPath != nullptr; tasPath = tasTraverser.GetNextFile()) {
        std::string tasFile = tasPath;
        TASInfo info;
        auto start = tasFile.find_last_of('\\') + 1;
        auto count = tasFile.find_last_of('.') - start;
        info.name = tasFile.substr(start, count);
        info.path = "..\\ModLoader\\TASRecords\\" + info.name + ".tas";
        m_Records.push_back(info);
    }

    std::sort(m_Records.begin(), m_Records.end());
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

CK3dEntity *TASSupport::GetActiveBall() const {
    if (!m_ActiveBall)
        return nullptr;
    return (CK3dEntity *)m_ActiveBall->GetValueObject();
}