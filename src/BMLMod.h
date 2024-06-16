#ifndef BML_BMLMOD_H
#define BML_BMLMOD_H

#include <string>
#include <vector>
#include <stack>

#include "BML/ICommand.h"
#include "BML/IMod.h"

#include "imgui.h"

#define MSG_MAXSIZE 35

class Config;
class Category;
class Property;
class InputHook;

enum MenuId {
    MENU_NULL = 0,
    MENU_MOD_LIST,
    MENU_MOD_PAGE,
    MENU_MOD_OPTIONS,
    MENU_MAP_LIST,
};

enum HudTypes {
    HUD_TITLE = 1,
    HUD_FPS = 2,
    HUD_SR = 4,
};

struct MapInfo {
    std::string name;
    std::wstring path;
};

class BMLMod : public IMod {
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
    void ExitModsMenu();

    void OpenMapMenu();
    void ExitMapMenu(bool backToMenu = true);

    float GetSRScore() const { return m_SRTimer; }
    int GetHSScore();

    void AdjustFrameRate(bool sync = false, float limit = 60.0f);

    int GetHUD();
    void SetHUD(int mode);

    void ToggleCommandBar(bool on = true);

    int OnTextEdit(ImGuiInputTextCallbackData *data);

private:
    void InitConfigs();
    void RegisterCommands();
    void InitGUI();
    void LoadFont();

    void RefreshMaps();
    size_t ExploreMaps(const std::wstring &path, std::vector<MapInfo> &maps);

    void LoadMap(const std::wstring &path);
    std::string CreateTempMapFile(const std::wstring &path);

    void OnEditScript_Base_EventHandler(CKBehavior *script);
    void OnEditScript_Menu_MenuInit(CKBehavior *script);
    void OnEditScript_Menu_OptionsMenu(CKBehavior *script);
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);
    void OnEditScript_Gameplay_Energy(CKBehavior *script);
    void OnEditScript_Gameplay_Events(CKBehavior *script);
    void OnEditScript_Levelinit_build(CKBehavior *script);
    void OnEditScript_ExtraLife_Fix(CKBehavior *script);

    void OnProcess_Fps();
    void OnProcess_SRTimer();
    void OnProcess_HUD();
    void OnProcess_CommandBar();
    void OnProcess_Menu();

    void ShowMenu(MenuId id);
    void ShowPreviousMenu();
    void HideMenu();

    void PushMenu(MenuId id);
    MenuId PopWindow();

    void OnDrawMenu();
    void OnDrawModList();
    void OnDrawModPage();
    void OnDrawModOptions();
    void OnDrawMapList();

    void OnSearchMaps();
    void OnResize();

    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    CKTimeManager *m_TimeManager = nullptr;
    InputHook *m_InputHook = nullptr;

    VxRect m_OldWindowRect;
    VxRect m_WindowRect;

    ImFont *m_Font = nullptr;

    MenuId m_CurrentMenu = MENU_NULL;
    std::stack<MenuId> m_MenuStack;

    int m_ModListPage = 0;
    int m_ModPage = 0;
    int m_ModOptionPage = 0;
    int m_MapPage = 0;

    IMod *m_CurrentMod = nullptr;
    Category *m_CurrentCategory = nullptr;

#ifndef NDEBUG
    bool m_ShowImGuiDemo = false;
#endif

    bool m_CmdTyping = false;
    char m_CmdBuf[65536] = {};
    std::vector<std::string> m_CmdHistory;
    int m_HistoryPos = 0;

    int m_MsgCount = 0;
    struct {
        char Text[256] = {};
        float Timer = 0.0f;
    } m_Msgs[MSG_MAXSIZE] = {};
    float m_MsgMaxTimer = 6000; // ms

    int m_FPSCount = 0;
    int m_FPSTimer = 0;
    char m_FPSText[16] = {};
    float m_SRTimer = 0.0f;
    char m_SRScore[16] = {};
    bool m_SRActivated = false;
    bool m_SRShouldDraw = false;

    IProperty *m_ShowTitle = nullptr;
    IProperty *m_ShowFPS = nullptr;
    IProperty *m_ShowSR = nullptr;
    IProperty *m_UnlockFPS = nullptr;
    IProperty *m_FPSLimit = nullptr;
    IProperty *m_AlphaTestEnabled = nullptr;
    IProperty *m_FixWidescreen = nullptr;
    IProperty *m_FixLifeBall = nullptr;
    IProperty* m_MsgDuration = nullptr;
    IProperty* m_CustomMapNumber = nullptr;
    IProperty *m_Overclock = nullptr;

    IProperty *m_FontFilename = nullptr;
    IProperty *m_FontSize = nullptr;
    IProperty *m_FontGlyphRanges = nullptr;

    CK2dEntity *m_Level01 = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKParameter *m_LoadCustom = nullptr;
    CKParameter *m_MapFile = nullptr;
    CKParameter *m_LevelRow = nullptr;
    CKDataArray *m_CurLevel = nullptr;

    CKBehaviorLink *m_OverclockLinks[3] = {};
    CKBehaviorIO *m_OverclockLinkIO[3][2] = {};

    char m_MapSearchBuf[65536] = {};
    std::vector<size_t> m_MapSearchResult;
    std::vector<MapInfo> m_Maps;
};

#endif // BML_BMLMOD_H