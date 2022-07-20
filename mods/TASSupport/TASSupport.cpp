#include "TASSupport.h"

#include <sys/stat.h>
#include <stdio.h>
#include <ctime>
#include <thread>

#include <Windows.h>

#include "MinHook.h"

TASSupport *g_mod = nullptr;

IMod *BMLEntry(IBML *bml) {
    g_mod = new TASSupport(bml);
    return g_mod;
}

std::pair<char *, int> ReadDataFromFile(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buffer = new char[size];
    fread(buffer, size, 1, fp);
    fclose(fp);
    return {buffer, size};
}

void WriteDataToFile(char *data, int size, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    fwrite(data, size, 1, fp);
    fclose(fp);
}

std::pair<char *, int> UncompressDataFromFile(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    int nsize = ftell(fp) - 4;
    fseek(fp, 0, SEEK_SET);

    int size;
    char *buffer = new char[nsize];
    fread(&size, 4, 1, fp);
    fread(buffer, nsize, 1, fp);
    fclose(fp);

    char *res = CKUnPackData(size, buffer, nsize);
    delete[] buffer;
    return {res, size};
}

void CompressDataToFile(char *data, int size, const char *filename) {
    int nsize;
    char *res = CKPackData(data, size, nsize, 9);

    FILE *fp = fopen(filename, "wb");
    fwrite(&size, 4, 1, fp);
    fwrite(res, nsize, 1, fp);
    fclose(fp);

    CKDeletePointer(res);
}

//BOOL WINAPI HookQueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
//	lpPerformanceCount->QuadPart = 0;
//	return true;
//}
//
//auto hookCount = &HookQueryPerformanceCounter;
//
//clock_t hook_clock(void) {
//	return 0;
//}
//
//auto hookClock = &hook_clock;
//
//time_t hook_time(time_t* const _Time) {
//	if (_Time) *_Time = 0;
//	return 0;
//}
//
//auto hookTime = &hook_time;

#ifdef _DEBUG

void TASSupport::OnPhysicalize(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                               const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                               float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                               int convexCnt, CKMesh **convexMesh, int ballCnt, VxVector *ballCenter, float *ballRadius,
                               int concaveCnt, CKMesh **concaveMesh) {

    //CK3dEntity* curBall = static_cast<CK3dEntity*>(m_CurLevel->GetElementObject(0, 1));
    //if (curBall == target) {
    CKSTRING name = target->GetName();
    VxVector pos;
    reinterpret_cast<CK3dEntity *>(target)->GetPosition(&pos);
    GetLogger()->Info("Physicalize %s %f %f %f", name, pos.x, pos.y, pos.z);
    //}
}

void TASSupport::OnUnphysicalize(CK3dEntity *target) {

    //CK3dEntity* curBall = static_cast<CK3dEntity*>(m_CurLevel->GetElementObject(0, 1));
    //if (curBall == target) {
    CKSTRING name = target->GetName();
    VxVector pos;
    reinterpret_cast<CK3dEntity *>(target)->GetPosition(&pos);
    GetLogger()->Info("Unphysicalize %s %f %f %f", name, pos.x, pos.y, pos.z);
    //}
}

#endif

class HookInputManager : public CKInputManager {
public:
    typedef CKERROR(HookInputManager::* PreProcessFunc)();

    static PreProcessFunc m_preProcess;

    CKERROR HookPreProcess() {
        CKERROR ret = (this->*m_preProcess)();

        if (g_mod->m_Playing) {
            if (g_mod->m_CurFrame < g_mod->m_RecordData.size()) {
                KeyState state = g_mod->m_RecordData[g_mod->m_CurFrame++].keyState;
                CKBYTE *stateBuf = GetKeyboardState();
                stateBuf[g_mod->m_KeyUp] = state.key_up;
                stateBuf[g_mod->m_KeyDown] = state.key_down;
                stateBuf[g_mod->m_KeyLeft] = state.key_left;
                stateBuf[g_mod->m_KeyRight] = state.key_right;
                stateBuf[CKKEY_Q] = state.key_q;
                stateBuf[g_mod->m_KeyShift] = state.key_shift;
                stateBuf[g_mod->m_KeySpace] = state.key_space;
                stateBuf[CKKEY_ESCAPE] = state.key_esc;
            } else {
                g_mod->OnStop();
            }
        }

        if (g_mod->m_Recording) {
            KeyState state;
            CKBYTE *stateBuf = GetKeyboardState();
            state.key_up = stateBuf[g_mod->m_KeyUp];
            state.key_down = stateBuf[g_mod->m_KeyDown];
            state.key_left = stateBuf[g_mod->m_KeyLeft];
            state.key_right = stateBuf[g_mod->m_KeyRight];
            state.key_q = stateBuf[CKKEY_Q];
            state.key_shift = stateBuf[g_mod->m_KeyShift];
            state.key_space = stateBuf[g_mod->m_KeySpace];
            state.key_esc = stateBuf[CKKEY_ESCAPE];
            g_mod->m_RecordData.rbegin()->keyState = state;
        }

        return ret;
    }
};

HookInputManager::PreProcessFunc HookInputManager::m_preProcess = &HookInputManager::HookPreProcess;

class HookTimeManager : public CKTimeManager {
public:
    typedef CKERROR(HookTimeManager::* PreProcessFunc)();

    //static CKDWORD tickCount;
    static PreProcessFunc m_preProcess;

    CKERROR HookPreProcess() {
        CKERROR ret = (this->*m_preProcess)();

        //tickCount++;
        if (g_mod->m_Recording) {
            g_mod->m_RecordData.emplace_back(GetLastDeltaTime());
        }

        if (g_mod->m_Playing) {
            if (g_mod->m_CurFrame < g_mod->m_RecordData.size())
                SetLastDeltaTime(g_mod->m_RecordData[g_mod->m_CurFrame].deltaTime);
            else g_mod->OnStop();
        }

        return ret;
    }
};

//CKDWORD HookTimeManager::tickCount = 0;
HookTimeManager::PreProcessFunc HookTimeManager::m_preProcess = &HookTimeManager::HookPreProcess;

//void WriteMemory(LPVOID dest, LPVOID src, int len) {
//	DWORD oldFlag;
//	VirtualProtect(dest, len, PAGE_EXECUTE_READWRITE, &oldFlag);
//	memcpy(dest, src, len);
//	VirtualProtect(dest, len, oldFlag, &oldFlag);
//}

void TASSupport::OnLoad() {
    //void* countHook = (reinterpret_cast<CKBYTE*>(LoadLibrary("Physics_RT.dll")) + 0x630E8);
    //WriteMemory(countHook, &hookCount, sizeof(&hookCount));

    //void* clockHook = (reinterpret_cast<CKBYTE*>(LoadLibrary("Physics_RT.dll")) + 0x63154);
    //WriteMemory(clockHook, &hookClock, sizeof(&hookClock));

    //void* timeHook = (reinterpret_cast<CKBYTE*>(LoadLibrary("Physics_RT.dll")) + 0x63190);
    //WriteMemory(timeHook, &hookTime, sizeof(&hookTime));

    //CKBYTE randcode[] = { 0x33, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90 };
    //void* randHook = (reinterpret_cast<CKBYTE*>(LoadLibrary("Physics_RT.dll")) + 0x52f95);
    //WriteMemory(randHook, randcode, sizeof(randcode));

    VxMakeDirectory("..\\ModLoader\\TASRecords\\");

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

    if (m_Enabled->GetBoolean()) {
        MH_Initialize();

        if (MH_CreateHook(reinterpret_cast<LPVOID>(0x24017F8C),
                          *reinterpret_cast<LPVOID *>(&HookTimeManager::m_preProcess),
                          reinterpret_cast<LPVOID *>(&HookTimeManager::m_preProcess)) != MH_OK
            || MH_EnableHook(reinterpret_cast<LPVOID>(0x24017F8C)) != MH_OK) {
            GetLogger()->Error("Create Time Manager Hook Failed");
            return;
        }

        if (MH_CreateHook(reinterpret_cast<LPVOID>(0x24AC1C30),
                          *reinterpret_cast<LPVOID *>(&HookInputManager::m_preProcess),
                          reinterpret_cast<LPVOID *>(&HookInputManager::m_preProcess)) != MH_OK
            || MH_EnableHook(reinterpret_cast<LPVOID>(0x24AC1C30)) != MH_OK) {
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
    if (m_BML->IsPlaying()) {
        /*CK3dEntity* curBall = static_cast<CK3dEntity*>(m_CurLevel->GetElementObject(0, 1));
        VxVector pos;
        curBall->GetPosition(&pos);
        GetLogger()->Info("%f %f %f %x %x %x", pos.x, pos.y, pos.z, *(DWORD*)&pos.x, *(DWORD*)&pos.y, *(DWORD*)&pos.z);

        CKBaseManager* physicsManager = m_BML->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
        int objCnt = *reinterpret_cast<short*>(reinterpret_cast<CKBYTE*>(physicsManager) + 0x2A);
        void** objList = *reinterpret_cast<void***>(reinterpret_cast<CKBYTE*>(physicsManager) + 0x2C);
        for (int i = 0; i < objCnt; i++) {
            void* obj = objList[i];
            CKBYTE* posptr = *reinterpret_cast<CKBYTE**>(reinterpret_cast<CKBYTE*>(obj) + 0xA4);
            double* pos = reinterpret_cast<double*>(posptr + 0xB8);
            float* speed = reinterpret_cast<float*>(posptr + 0xD8);
            GetLogger()->Info("Physics Process %lf %lf %lf %f %f %f", pos[0], pos[1], pos[2], speed[0], speed[1], speed[2]);
        }*/

        //GetLogger()->Info("Tick %d", HookTimeManager::tickCount);
        //VxVector pos;
        //CK3dEntity* stoneball = m_BML->Get3dObjectByName("P_Ball_Stone_MF");
        //stoneball->GetPosition(&pos);
        //GetLogger()->Info("%d %f %f %f %x %x %x", 0, pos.x, pos.y, pos.z, *(DWORD*)&pos.x, *(DWORD*)&pos.y, *(DWORD*)&pos.z);

        //char ballname[100];
        //for (int i = 1; i < 16; i++) {
        //	sprintf(ballname, "P_Ball_Stone_MF%03d", i);
        //	CK3dEntity* stoneball = m_BML->Get3dObjectByName(ballname);
        //	stoneball->GetPosition(&pos);
        //	GetLogger()->Info("%d %f %f %f %x %x %x", i, pos.x, pos.y, pos.z, *(DWORD*)&pos.x, *(DWORD*)&pos.y, *(DWORD*)&pos.z);
        //}
    }

    if (m_Playing || m_Recording) {
        //CK3dEntity* curBall = static_cast<CK3dEntity*>(m_CurLevel->GetElementObject(0, 1));
        //if (curBall != nullptr) {
        //	VxVector pos;
        //	curBall->GetPosition(&pos);
        //	GetLogger()->Info("%d %d %f %f %f %f %f", m_CurFrame, m_RecordData.size(), m_BML->GetTimeManager()->GetLastDeltaTime(),
        //		pos.x, pos.y, pos.z, m_BML->GetSRScore());
        //}
        //else {
        //	GetLogger()->Info("%d %d %f", m_CurFrame, m_RecordData.size(), m_BML->GetTimeManager()->GetLastDeltaTime());

        //	//std::function<void(CKBehavior*, int)> active = [this, &active](CKBehavior* script, int depth) {
        //	//	int num = script->GetSubBehaviorCount();
        //	//	for (int j = 0; j < num; j++) {
        //	//		CKBehavior* beh = script->GetSubBehavior(j);
        //	//		if (beh->IsActive()) {
        //	//			GetLogger()->Info("%s%s", std::string(depth, ' ').c_str(), beh->GetName());
        //	//			if (!beh->IsUsingFunction())
        //	//				active(beh, depth + 1);
        //	//		}
        //	//	}
        //	//};

        //	//CKContext* context = m_BML->GetCKContext();
        //	//int cnt = context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
        //	//CK_ID* scripts = context->GetObjectsListByClassID(CKCID_BEHAVIOR);
        //	//bool act = false;
        //	//for (int i = 0; i < cnt; i++) {
        //	//	CKBehavior* script = (CKBehavior*)context->GetObject(scripts[i]);
        //	//	if (script->GetType() == CKBEHAVIORTYPE_SCRIPT) {
        //	//		// delay(script);
        //	//		if (script->IsActive()) {
        //	//			GetLogger()->Info("%s", script->GetName());
        //	//			act = true;
        //	//			active(script, 1);
        //	//		}
        //	//	}
        //	//}
        //}

        //CKBaseManager* physicsManager = m_BML->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
        //CKBYTE* m_Timer = *reinterpret_cast<CKBYTE**>(reinterpret_cast<CKBYTE*>(physicsManager) + 0xC0);

        //GetLogger()->Info("%f %f %llx %llx", m_BML->GetTimeManager()->GetLastDeltaTime(),
        //	m_BML->GetTimeManager()->GetLastDeltaTimeFree(),
        //	*reinterpret_cast<double*>(reinterpret_cast<CKBYTE*>(physicsManager) + 0xC8),
        //	*reinterpret_cast<double*>(m_Timer + 0x120));
    }

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

            if (m_BML->GetInputManager()->IsKeyPressed(m_StopKey->GetKey()))
                OnStop();

            if (m_CurFrame < m_SkipRender->GetInteger())
                m_BML->SkipRenderForNextTick();

            if (m_BML->GetInputManager()->IsKeyPressed(m_ExitKey->GetKey()))
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
                g_mod->m_BML->SendIngameMessage(("Loading TAS Record: " + tasFile + ".tas").c_str());
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
            } else m_BML->SendIngameMessage(("TAS file " + tasFile + ".tas not found.").c_str());
        }

        firstTime = false;
    }
}

void TASSupport::OnBallOff() {
    if (m_Enabled->GetBoolean() && m_Playing && m_ExitOnDead->GetBoolean())
        m_BML->ExitGame();
}

#ifdef _DEBUG

void TASSupport::OnPostExitLevel() {
    CKBaseManager *physicsManager = m_BML->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
    CKBYTE *moduleAddr = reinterpret_cast<CKBYTE *>(GetModuleHandle("physics_RT.dll"));
    CKERROR(CKBaseManager::*
    ClearFunc)();
    *reinterpret_cast<DWORD *>(&ClearFunc) = *(*reinterpret_cast<DWORD **>(physicsManager) + 4);
    (physicsManager->*ClearFunc)();
    CKERROR(CKBaseManager::*
    EndFunc)();
    *reinterpret_cast<DWORD *>(&EndFunc) = *(*reinterpret_cast<DWORD **>(physicsManager) + 12);
    (physicsManager->*EndFunc)();
    CKERROR(CKBaseManager::*
    ConstructorFunc)(CKContext * );
    *reinterpret_cast<CKBYTE **>(&ConstructorFunc) = reinterpret_cast<CKBYTE *>(GetModuleHandle("physics_RT.dll")) + 0x6730;
    (physicsManager->*ConstructorFunc)(m_BML->GetCKContext());
    //CKERROR(CKBaseManager:: * PauseFunc)();
    //*reinterpret_cast<DWORD*>(&PauseFunc) = *(*reinterpret_cast<DWORD**>(physicsManager) + 15);
    //(physicsManager->*PauseFunc)();
    //CKERROR(CKBaseManager:: * ResetFunc)();
    //*reinterpret_cast<DWORD*>(&ResetFunc) = *(*reinterpret_cast<DWORD**>(physicsManager) + 13);
    //(physicsManager->*ResetFunc)();
    CKERROR(CKBaseManager::*
    InitFunc)();
    *reinterpret_cast<DWORD *>(&InitFunc) = *(*reinterpret_cast<DWORD **>(physicsManager) + 11);
    (physicsManager->*InitFunc)();
    CKERROR(CKBaseManager::*
    PlayFunc)();
    *reinterpret_cast<CKBYTE **>(&PlayFunc) = reinterpret_cast<CKBYTE *>(GetModuleHandle("physics_RT.dll")) + 0x6BB0;
    (physicsManager->*PlayFunc)();

    m_BML->RestoreIC(m_BML->Get3dEntityByName("Ball_PaperPieces_Frame"), true);
    m_BML->RestoreIC(m_BML->Get3dEntityByName("Ball_StonePieces_Frame"), true);
    m_BML->RestoreIC(m_BML->Get3dEntityByName("Ball_WoodPieces_Frame"), true);

    m_InitPieces->ActivateInput(0);
    m_InitPieces->Activate();
    CKBehavior *Balls_Init = m_BML->GetScriptByName("Balls_Init");
    CKBehavior *temp = m_InitPieces->GetParent();
    m_InitPieces->GetParent()->Activate();
}

#endif

void TASSupport::OnStart() {
    if (m_Enabled->GetBoolean()) {
        m_BML->AddTimer(1ul, [this]() {
            // 0x43ec
            // OnCKReset
            // OnCKPlay
            // OnCKPause empty
            // OnCKEnd
            // OnCKInit
            // PostClearAll
            // PostProcess
            // PreProcess
            // vtable 100632D0
            CKBaseManager *physicsManager = m_BML->GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
            float *averageTickTime = reinterpret_cast<float *>(reinterpret_cast<CKBYTE *>(physicsManager) + 0xC8);
            *averageTickTime = m_BML->GetTimeManager()->GetLastDeltaTime();
            CKBYTE *timer = *reinterpret_cast<CKBYTE **>(reinterpret_cast<CKBYTE *>(physicsManager) + 0xC0);
            *reinterpret_cast<double *>(timer + 0x120) = 0;
            *reinterpret_cast<double *>(timer + 0x128) = 1.0 / 66;
            *reinterpret_cast<double *>(timer + 0x130) = 0;
            *reinterpret_cast<DWORD *>(timer + 0x138) = 0;
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
                time_t stamp = time(0);
                tm *curTime = localtime(&stamp);
                sprintf(filename, "%04d%02d%02d_%02d%02d%02d_%s.tas", curTime->tm_year + 1900, curTime->tm_mon + 1,
                        curTime->tm_mday, curTime->tm_hour, curTime->tm_min, curTime->tm_sec, m_MapName.c_str());
                m_BML->SendIngameMessage(("TAS record saved to " + std::string(filename)).c_str());
                int length = m_RecordData.size();

                std::thread([this, length]() {
                    char filepath[MAX_PATH];
                    sprintf(filepath, "..\\ModLoader\\TASRecords\\%s", filename);
                    CompressDataToFile((char *) &m_RecordData[0], length * sizeof(FrameData), filepath);
                    OnStop();
                }).detach();
            });
        }
    }
}

void TASSupport::LoadTAS(const std::string &filename) {
    std::thread([this, filename]() {
        auto data = UncompressDataFromFile(filename.c_str());
        int length = data.second / sizeof(FrameData);
        m_RecordData.resize(length);
        memcpy(&m_RecordData[0], data.first, data.second);
        m_ReadyToPlay = true;
        CKDeletePointer(data.first);
    }).detach();
}

GuiTASList::GuiTASList() {
    m_Left = AddLeftButton("M_List_Left", 0.4f, 0.34f, [this]() { PreviousPage(); });
    m_Right = AddRightButton("M_List_Right", 0.4f, 0.6238f, [this]() { NextPage(); });
    AddBackButton("M_Opt_Mods_Back")->SetCallback([this]() { Exit(); });
    AddTextLabel("M_Opt_Mods_Title", "TAS Records", ExecuteBB::GAMEFONT_02, 0.35f, 0.07f, 0.3f, 0.05f);

    Init(0, 12);
    SetVisible(false);
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
        info.displayName = tasFile.substr(tasFile.find_last_of('\\') + 1);;
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
                              g_mod->GetBML()->SendIngameMessage(("Loading TAS Record: " + info.displayName).c_str());
                              g_mod->LoadTAS(info.filepath);
                          });
}

std::string GuiTASList::GetButtonText(int index) {
    return m_Records[index].displayName;
}

void GuiTASList::Exit() {
    g_mod->GetBML()->GetCKContext()->GetCurrentScene()->Activate(
        g_mod->GetBML()->GetScriptByName("Menu_Main"), true);
    SetVisible(false);
}