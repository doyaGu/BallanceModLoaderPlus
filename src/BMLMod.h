#ifndef BML_BMLMOD_H
#define BML_BMLMOD_H

#include <string>
#include <vector>
#include <queue>
#include <map>

#include "Defines.h"
#include "ICommand.h"
#include "IMod.h"
#include "Gui.h"
#include "ModLoader.h"

#define MSG_MAXSIZE 35

class Config;
class Property;

class GuiList : public BGui::Gui {
public:
    GuiList();

    void Init(int size, int maxsize);

    virtual BGui::Button *CreateButton(int index) = 0;
    virtual std::string GetButtonText(int index) = 0;

    virtual BGui::Gui *CreateSubGui(int index) = 0;
    virtual BGui::Gui *GetParentGui() = 0;

    virtual void SetPage(int page);
    void PreviousPage() {
        if (m_CurPage > 0)
            SetPage(m_CurPage - 1);
    }
    void NextPage() {
        if (m_CurPage < m_MaxPage - 1)
            SetPage(m_CurPage + 1);
    }

    virtual void Exit();

    void SetVisible(bool visible) override;

protected:
    std::vector<BGui::Button *> m_Buttons;
    int m_CurPage;
    int m_MaxPage;
    int m_Size;
    int m_MaxSize;
    std::vector<BGui::Gui *> m_GuiList;
    BGui::Button *m_Left, *m_Right;
};

class GuiModOption : public GuiList {
public:
    GuiModOption();

    BGui::Button *CreateButton(int index) override;
    std::string GetButtonText(int index) override;

    BGui::Gui *CreateSubGui(int index) override;
    BGui::Gui *GetParentGui() override;

    void Exit() override;
};

class GuiModMenu : public GuiList {
public:
    explicit GuiModMenu(IMod *mod);

    void Process() override;

    void SetVisible(bool visible) override;

    BGui::Button *CreateButton(int index) override;
    std::string GetButtonText(int index) override;

    BGui::Gui *CreateSubGui(int index) override;
    BGui::Gui *GetParentGui() override;

private:
    Config *m_Config;
    std::vector<std::string> m_Categories;

    BGui::Panel *m_CommentBackground;
    BGui::Label *m_Comment;
    int m_CurComment = -1;
};

class GuiCustomMap : public GuiList {
public:
    explicit GuiCustomMap(class BMLMod *mod);

    BGui::Button *CreateButton(int index) override;
    std::string GetButtonText(int index) override;

    BGui::Gui *CreateSubGui(int index) override;
    BGui::Gui *GetParentGui() override;

    void SetPage(int page) override;

    void Exit() override;

private:
    struct MapInfo {
        std::string DisplayName;
        std::string SearchName;
        std::string FilePath;

        bool operator<(const MapInfo &o) const {
            return DisplayName < o.DisplayName;
        }
    };

    std::vector<MapInfo> m_Maps;
    std::vector<MapInfo *> m_SearchRes;
    std::vector<BGui::Text *> m_Texts;
    BGui::Input *m_SearchBar = nullptr;
    BGui::Button *m_Exit = nullptr;

    BMLMod *m_BMLMod;
};

class GuiModCategory : public BGui::Gui {
public:
    GuiModCategory(GuiModMenu *parent, Config *config, const std::string& category);

    void Process() override;

    void SetVisible(bool visible) override;

    void SetPage(int page);
    void PreviousPage() {
        if (m_CurPage > 0)
            SetPage(m_CurPage - 1);
    }
    void NextPage() {
        if (m_CurPage < m_MaxPage - 1)
            SetPage(m_CurPage + 1);
    }

    void SaveAndExit();
    void Exit();

private:
    std::vector<Property *> m_Data;
    std::vector<std::vector<BGui::Element *>> m_Elements;
    std::vector<std::pair<BGui::Element *, BGui::Element *>> m_Inputs;
    int m_CurPage;
    int m_MaxPage;
    int m_Size;
    BGui::Button *m_Left;
    BGui::Button *m_Right;

    GuiModMenu *m_Parent;
    Config *m_Config;
    std::string m_Category;
    BGui::Button *m_Exit;

    std::vector<std::vector<std::pair<Property *, BGui::Element *>>> m_Comments;
    BGui::Panel *m_CommentBackground;
    BGui::Label *m_Comment;
    Property *m_CurComment = nullptr;
};

class BMLMod : public IMod {
    friend class CommandClear;
    friend class CommandSector;
    friend class GuiModMenu;
    friend class GuiCustomMap;

public:
    explicit BMLMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "BML"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Ballance Mod Loader"; }
    const char *GetAuthor() override { return "Gamepiaynmo & YingChe & Kakuty"; }
    const char *GetDescription() override {
        return "Implementation of functions provided by Ballance Mod Loader."
               "\n\n https://github.com/doyaGu/BallanceModLoaderPlus";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                      CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnProcess() override;
    void OnCheatEnabled(bool enable) override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;
    void OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) override;

    void OnPreStartMenu() override;
    void OnPostResetLevel() override;
    void OnStartLevel() override;
    void OnPostExitLevel() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnCounterActive() override;
    void OnCounterInactive() override;

    void AddIngameMessage(const char *msg);

    void ShowCheatBanner(bool show);
    void ShowModOptions();
    void ShowGui(BGui::Gui *gui);

    void CloseCurrentGui();

    float GetSRScore() const { return m_SRTimer; }
    int GetHSScore();

    void EnterTravelCam();
    void ExitTravelCam();
    bool IsInTravelCam();

    void AdjustFrameRate(bool sync = false, float limit = 60.0f);

    void ChangeBallSpeed(float times);

private:
    void OnEditScript_Base_EventHandler(CKBehavior *script);
    void OnEditScript_Menu_MenuInit(CKBehavior *script);
    void OnEditScript_Menu_OptionsMenu(CKBehavior *script);
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);
    void OnEditScript_Gameplay_Energy(CKBehavior *script);
    void OnEditScript_Gameplay_Events(CKBehavior *script);
    void OnEditScript_Levelinit_build(CKBehavior *script);
    void OnEditScript_ExtraLife_Fix(CKBehavior *script);

    void OnProcess_FpsDisplay();
    void OnProcess_CommandBar();
    void OnProcess_Suicide();
    void OnProcess_ChangeSpeed();
    void OnProcess_ChangeBall();
    void OnProcess_ResetBall();
    void OnProcess_Travel();
    void OnProcess_AddLife();
    void OnProcess_Summon();
    void OnProcess_SRTimer();
    void OnProcess_SkipRender();

    void OnResize();
    void OnCmdEdit(CKDWORD key);

    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    CKTimeManager *m_TimeManager = nullptr;
    InputHook *m_InputHook = nullptr;

    VxRect m_OldWindowRect;
    VxRect m_WindowRect;

    float m_DeltaTime = 0.0f;
    bool m_CheatEnabled = false;
    bool m_SkipRender = false;
    IProperty* m_SkipRenderKey = nullptr;

    BGui::Gui *m_CmdBar = nullptr;
    bool m_CmdTyping = false;
    BGui::Input *m_CmdInput = nullptr;
    std::vector<std::string> m_CmdHistory;
    unsigned int m_HistoryPos = 0;

    BGui::Gui *m_MsgLog = nullptr;
    int m_MsgCount = 0;
    struct {
        BGui::Panel *m_Background;
        BGui::Label *m_Text;
        float m_Timer;
    } m_Msgs[MSG_MAXSIZE] = {};
    float m_MsgMaxTimer = 6000; // ms

    BGui::Gui *m_IngameBanner = nullptr;
    BGui::Label* m_Title = nullptr;
    BGui::Label *m_Cheat = nullptr;
    BGui::Label *m_FPS = nullptr;
    BGui::Label *m_SRScore = nullptr;
    BGui::Label *m_SRTitle = nullptr;
    int m_FPSCount = 0;
    int m_FPSTimer = 0;
    float m_SRTimer = 0.0f;
    bool m_SRActivated = false;

    BGui::Gui *m_CurGui = nullptr;
    GuiModOption *m_ModOption = nullptr;

    IProperty *m_UnlockFPS = nullptr;
    IProperty *m_FPSLimit = nullptr;
    IProperty *m_AdaptiveCamera = nullptr;
    IProperty* m_ShowTitle = nullptr;
    IProperty *m_ShowFPS = nullptr;
    IProperty *m_ShowSR = nullptr;
    IProperty *m_FixLifeBall = nullptr;
    IProperty* m_MsgDuration = nullptr;

    IProperty *m_BallCheat[2] = {};
    IProperty *m_EnableSuicide = nullptr;
    IProperty* m_Suicide = nullptr;
    CKParameterLocal *m_BallForce[2] = {};
    bool m_SuicideCd = false;

    IProperty *m_CamRot[2] = {};
    IProperty *m_CamY[2] = {};
    IProperty *m_CamZ[2] = {};
    IProperty *m_Cam45 = nullptr;
    IProperty *m_CamReset = nullptr;
    IProperty *m_CamOn = nullptr;

    CK3dEntity *m_CamPos = nullptr;
    CK3dEntity *m_CamOrient = nullptr;
    CK3dEntity *m_CamOrientRef = nullptr;
    CK3dEntity *m_CamTarget = nullptr;

    IProperty *m_Overclock = nullptr;
    CKBehaviorLink *m_OverclockLinks[3] = {};
    CKBehaviorIO *m_OverclockLinkIO[3][2] = {};

    IProperty *m_ChangeBall[3] = {};
    CKBehavior *m_SetNewBall = nullptr;
    CKParameter *m_CurTrafo = nullptr;
    CKDataArray *m_CurLevel = nullptr;
    CKDataArray *m_IngameParam = nullptr;
    int m_ChangeBallCd = 0;
    IProperty *m_SpeedupBall = nullptr;
    IProperty *m_SpeedNotification = nullptr;
    bool m_Speedup = false;

    CKDataArray *m_PhysicsBall = nullptr;
    CKParameter *m_Force = nullptr;
    std::map<std::string, float> m_Forces;

    IProperty *m_ResetBall = nullptr;
    CKParameter *m_CurSector = nullptr;
    CKBehavior *m_PhysicsNewBall = nullptr;
    CKBehavior *m_DynamicPos = nullptr;

    IProperty *m_AddLife = nullptr;
    bool m_AddLifeCd = false;

    IProperty *m_AddBall[4] = {};
    int m_CurSel = -1;
    CK3dEntity *m_CurObj = nullptr;
    CK3dEntity *m_Balls[4] = {};
    std::vector<std::pair<int, CK3dEntity *>> m_TempBalls;
    IProperty *m_MoveKeys[6] = {};

    float m_TravelSpeed = 0.2f;
    CKCamera *m_TravelCam = nullptr;

    GuiCustomMap *m_MapsGui = nullptr;
    CK2dEntity *m_Level01 = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    BGui::Button *m_CustomMaps = nullptr;
    CKParameter *m_LoadCustom = nullptr;
    CKParameter *m_MapFile = nullptr;
    CKParameter *m_LevelRow = nullptr;
};

#endif // BML_BMLMOD_H