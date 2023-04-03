#pragma once

#include <memory>
#include <thread>

#include <BMLPlus/BMLAll.h>

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

class GuiTASList : public BGui::Gui {
public:
    GuiTASList();

    void Init(int size, int maxsize);

    void Resize(int size);

    void RefreshRecords();

    virtual BGui::Button *CreateButton(int index);

    virtual std::string GetButtonText(int index);

    virtual void SetPage(int page);

    virtual void Exit();

    void SetVisible(bool visible) override;
    bool IsVisible() const { return m_Visible; }

    void PreviousPage() { if (m_CurPage > 0) SetPage(m_CurPage - 1); }
    void NextPage() { if (m_CurPage < m_MaxPage - 1) SetPage(m_CurPage + 1); }

protected:
    std::vector<BGui::Button *> m_Buttons;
    int m_CurPage = 0;
    int m_MaxPage = 0;
    int m_Size = 0;
    int m_MaxSize = 0;
    BGui::Button *m_Left;
    BGui::Button *m_Right;

    struct TASInfo {
        std::string displayName, filepath;

        bool operator<(const TASInfo &o) const {
            return displayName < o.displayName;
        }
    };

    std::vector<TASInfo> m_Records;
    std::vector<BGui::Text *> m_Texts;
    bool m_Visible = false;
};

class TASSupport : public IMod {
public:
    explicit TASSupport(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "TASSupport"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "TAS Support"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Make TAS possible in Ballance (WIP)."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnUnload() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

    void OnPreProcess();
    void OnProcess() override;

    void OnBallOff() override;

    void OnPreLoadLevel() override { OnStart(); }
    void OnPreResetLevel() override { OnStop(); }
    void OnPreExitLevel() override { OnStop(); }
    void OnLevelFinish() override { OnFinish(); }

    void OnPostStartMenu() override;

    IBML* GetBML() { return m_BML; }

    void OnStart();
    void OnStop();
    void OnFinish();

    void LoadTAS(const std::string &filename);

    CKTimeManager *m_TimeManager = nullptr;
    CKInputManager *m_InputManager = nullptr;

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
    std::size_t m_CurFrame = 0;
    std::vector<FrameData> m_RecordData;
    std::string m_MapName;

    CK2dEntity *m_Level01 = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKBehavior *m_InitPieces = nullptr;
    BGui::Gui *m_TASEntryGui = nullptr;
    BGui::Button *m_TASEntry = nullptr;
    GuiTASList *m_TASListGui = nullptr;
    CKBehavior *m_ExitMain = nullptr;

    IProperty *m_ShowKeys = nullptr;
    BGui::Gui *m_KeysGui = nullptr;
    BGui::Button *m_ButtonUp = nullptr;
    BGui::Button *m_ButtonDown = nullptr;
    BGui::Button *m_ButtonLeft = nullptr;
    BGui::Button *m_ButtonRight = nullptr;
    BGui::Button *m_ButtonShift = nullptr;
    BGui::Button *m_ButtonSpace = nullptr;
    BGui::Button *m_ButtonQ = nullptr;
    BGui::Button *m_ButtonEsc = nullptr;
    char m_FrameCountText[100] = {};
    BGui::Label *m_FrameCountLabel = nullptr;

    IProperty *m_SkipRender = nullptr;
    IProperty *m_ExitOnDead = nullptr;
    IProperty *m_ExitOnFinish = nullptr;
    IProperty *m_ExitKey = nullptr;
    IProperty *m_LoadTAS = nullptr;
    IProperty *m_LoadLevel = nullptr;

    std::unique_ptr<std::thread> m_LoadThread;
};