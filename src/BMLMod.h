#ifndef BML_BMLMOD_H
#define BML_BMLMOD_H

#include "BML/BML.h"
#include "BML/IMod.h"
#include "BML/IBML.h"

#include "HUD.h"
#include "ModMenu.h"
#include "MapMenu.h"
#include "CommandBar.h"
#include "MessageBoard.h"

enum HudTypes {
    HUD_TITLE = 1,
    HUD_FPS = 2,
    HUD_SR = 4,
};

class BMLMod : public IMod {
public:
    explicit BMLMod(IBML *bml) : IMod(bml), m_MapMenu(this) {}

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
    void OnUnload() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                      CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnProcess() override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;

    void OnPreStartMenu() override;
    void OnExitGame() override;
    void OnStartLevel() override;
    void OnPostExitLevel() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnCounterActive() override;
    void OnCounterInactive() override;

    void AddIngameMessage(const char *msg);
    void ClearIngameMessages();

    void OpenModsMenu();
    void CloseModsMenu();

    void OpenMapMenu();
    void CloseMapMenu();

    void LoadMap(const std::wstring &path);

    float GetSRScore() const;

    int GetHSScore();

    void AdjustFrameRate(bool sync = false, float limit = 60.0f);

    int GetHUD();
    void SetHUD(int mode);

private:
    void InitConfigs();
    void InitGUI();
    void RegisterCommands();

    void OnEditScript_Base_EventHandler(CKBehavior *script);
    void OnEditScript_Menu_MenuInit(CKBehavior *script);
    void OnEditScript_Menu_OptionsMenu(CKBehavior *script);
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);
    void OnEditScript_Gameplay_Energy(CKBehavior *script);
    void OnEditScript_Gameplay_Events(CKBehavior *script);
    void OnEditScript_Levelinit_build(CKBehavior *script);
    void OnEditScript_ExtraLife_Fix(CKBehavior *script);

    void OnProcess_HUD();
    void OnProcess_CommandBar();
    void OnProcess_Menu();

    void OnResize();

    BML::IDataShare *m_DataShare = nullptr;

    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    CKTimeManager *m_TimeManager = nullptr;

    VxRect m_OldWindowRect;
    VxRect m_WindowRect;

    HUD m_HUD;
    ModMenu m_ModMenu;
    MapMenu m_MapMenu;
    CommandBar m_CommandBar;
    MessageBoard m_MessageBoard;

    std::string m_ImGuiIniFilename;
    std::string m_ImGuiLogFilename;

#ifndef NDEBUG
    bool m_ShowImGuiDemo = false;
#endif

    IProperty *m_FontFilename = nullptr;
    IProperty *m_FontSize = nullptr;
    IProperty *m_FontRanges = nullptr;
    IProperty *m_EnableSecondaryFont = nullptr;
    IProperty *m_SecondaryFontFilename = nullptr;
    IProperty *m_SecondaryFontSize = nullptr;
    IProperty *m_SecondaryFontRanges = nullptr;

    IProperty *m_ShowTitle = nullptr;
    IProperty *m_ShowFPS = nullptr;
    IProperty *m_ShowSR = nullptr;

    IProperty *m_UnlockFPS = nullptr;
    IProperty *m_FPSLimit = nullptr;
    IProperty *m_WidescreenFix = nullptr;

    IProperty *m_LanternAlphaTest = nullptr;
    IProperty *m_FixLifeBall = nullptr;
    IProperty *m_Overclock = nullptr;

    IProperty *m_MsgCapability = nullptr;
    IProperty *m_MsgDuration = nullptr;
    IProperty *m_CustomMapNumber = nullptr;
    IProperty *m_CustomMapTooltip = nullptr;
    IProperty *m_CustomMapMaxDepth = nullptr;

    CK2dEntity *m_Level01 = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKParameter *m_LoadCustom = nullptr;
    CKParameter *m_MapFile = nullptr;
    CKParameter *m_LevelRow = nullptr;
    CKDataArray *m_CurLevel = nullptr;

    CKBehaviorLink *m_OverclockLinks[3] = {};
    CKBehaviorIO *m_OverclockLinkIO[3][2] = {};
};

#endif // BML_BMLMOD_H