#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>

#include <BML/BMLAll.h>

#include "physics_RT.h"

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

struct KeyState {
    unsigned key_up: 1;
    unsigned key_down: 1;
    unsigned key_left: 1;
    unsigned key_right: 1;
    unsigned key_shift: 1;
    unsigned key_space: 1;
    unsigned key_q: 1;
    unsigned key_esc: 1;
    unsigned key_enter: 1;
};

struct FrameData {
    FrameData() = default;

    explicit FrameData(float deltaTime) : deltaTime(deltaTime) {
        keyStates = 0;
    }

    float deltaTime;
    union {
        KeyState keyState;
        int keyStates;
    };
};

struct DumpData {
    DumpData() = default;

    explicit DumpData(float deltaTime) : deltaTime(deltaTime) {}

    float deltaTime = 0.0f;
    VxVector position;
    VxVector angles;
    VxVector velocity;
    VxVector angularVelocity;
};

struct TASInfo {
    std::string name;
    std::string path;

    bool operator<(const TASInfo &o) const {
        return name < o.name;
    }
};

class TASSupport : public IMod {
public:
    explicit TASSupport(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "TASSupport"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "TAS Support"; }
    const char *GetAuthor() override { return "Gamepiaynmo & Kakuty"; }
    const char *GetDescription() override { return "Make TAS possible in Ballance (WIP)."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnUnload() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

    void OnProcess() override;

    void OnPostStartMenu() override;
    void OnExitGame() override;

    void OnPreLoadLevel() override { OnStart(); }
    void OnPreResetLevel() override { OnStop(); }
    void OnPreExitLevel() override { OnStop(); }
    void OnLevelFinish() override { OnFinish(); }

    void OnBallOff() override;

    void OnPreProcessInput();
    void OnPreProcessTime();

    void OnStart();
    void OnStop();
    void OnFinish();

    void OnDrawMenu();
    void OnDrawKeys();
    void OnDrawInfo();

    void OpenTASMenu();
    void ExitTASMenu();

    void RefreshRecords();
    void LoadTAS(const std::string &filename);

    CK3dEntity *GetActiveBall() const;

    CKDWORD m_PhysicsRTVersion = 0;
    CKIpionManager *m_IpionManager = nullptr;
    CKTimeManager *m_TimeManager = nullptr;
    InputHook *m_InputHook = nullptr;

    CKDataArray *m_CurLevel = nullptr;
    CKDataArray *m_Keyboard = nullptr;
    CKKEYBOARD m_KeyUp = CKKEY_UP;
    CKKEYBOARD m_KeyDown = CKKEY_DOWN;
    CKKEYBOARD m_KeyLeft = CKKEY_LEFT;
    CKKEYBOARD m_KeyRight = CKKEY_RIGHT;
    CKKEYBOARD m_KeyShift = CKKEY_LSHIFT;
    CKKEYBOARD m_KeySpace = CKKEY_SPACE;

    IProperty *m_Enabled = nullptr;
    IProperty *m_Record = nullptr;
    IProperty *m_StopKey = nullptr;

    bool m_ReadyToPlay = false;
    bool m_Recording = false;
    bool m_Playing = false;
    bool m_ShowMenu = false;
    int m_CurPage = 0;
    std::vector<TASInfo> m_Records;

    std::size_t m_CurFrame = 0;
    std::vector<FrameData> m_RecordData;
    std::vector<DumpData> m_DumpData;
    std::string m_MapName;

    CK2dEntity *m_Level01 = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKBehavior *m_ExitMain = nullptr;
    CKParameter *m_ActiveBall = nullptr;

    IProperty *m_ShowKeys = nullptr;
    IProperty *m_ShowInfo = nullptr;
    char m_FrameCountText[100] = {};

    IProperty *m_SkipRender = nullptr;
    IProperty *m_ExitOnDead = nullptr;
    IProperty *m_ExitOnFinish = nullptr;
    IProperty *m_ExitKey = nullptr;
    IProperty *m_LoadTAS = nullptr;
    IProperty *m_LoadLevel = nullptr;
    IProperty *m_EnableDump = nullptr;

    std::unique_ptr<std::thread> m_LoadThread;
};