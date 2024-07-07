#ifndef BML_BMLMOD_H
#define BML_BMLMOD_H

#include <string>
#include <vector>

#include "BML/IMod.h"
#include "BML/IBML.h"
#include "BML/Bui.h"

#include "Config.h"

#define MSG_MAXSIZE 35

class BMLMod;

enum HudTypes {
    HUD_TITLE = 1,
    HUD_FPS = 2,
    HUD_SR = 4,
};

class ModMenu;

class ModMenuPage : public Bui::Page {
public:
    ModMenuPage(ModMenu *menu, std::string name);
    ~ModMenuPage() override;

    void OnClose() override;

protected:
    ModMenu *m_Menu;
};

class ModMenu : public Bui::Menu {
public:
    explicit ModMenu(BMLMod *mod) : m_Mod(mod) {}

    void Init();
    void Shutdown();

    IMod *GetCurrentMod() const { return m_CurrentMod; };
    void SetCurrentMod(IMod *mod) { m_CurrentMod = mod; };

    Category *GetCurrentCategory() const { return m_CurrentCategory; };
    void SetCurrentCategory(Category *category) { m_CurrentCategory = category; };

    void OnOpen() override;
    void OnClose() override;

    static IBML *GetBML();
    static Config *GetConfig(IMod *mod);

private:
    BMLMod *m_Mod;
    IMod *m_CurrentMod = nullptr;
    Category *m_CurrentCategory = nullptr;
    std::vector<std::unique_ptr<ModMenuPage>> m_Pages;
};

class ModListPage : public ModMenuPage {
public:
    explicit ModListPage(ModMenu *menu) : ModMenuPage(menu, "Mod List") {}

    void OnBegin() override;
    void OnDraw() override;
};

class ModPage : public ModMenuPage {
public:
    explicit ModPage(ModMenu *menu) : ModMenuPage(menu, "Mod Page") {}

    void OnAfterBegin() override;
    void OnDraw() override;

protected:
    static void ShowCommentBox(Category *category);

    Config *m_Config = nullptr;
    char m_TextBuf[1024] = {};
};

class ModOptionPage : public ModMenuPage {
public:
    explicit ModOptionPage(ModMenu *menu) : ModMenuPage(menu, "Mod Options") {}

    void OnAfterBegin() override;
    void OnDraw() override;

protected:
    static void ShowCommentBox(Property *property);

    Category *m_Category = nullptr;
    char m_Buffers[4][4096] = {};
    size_t m_BufferHashes[4] = {};
    bool m_KeyToggled[4] = {};
    ImGuiKeyChord m_KeyChord[4] = {};
};

class MapListPage : public Bui::Page {
public:
    explicit MapListPage(BMLMod *mod) : Page("Custom Maps"), m_Mod(mod) {}
    ~MapListPage() override = default;

    void OnAfterBegin() override;
    void OnDraw() override;

    bool OnOpen() override;
    void OnClose() override;

    void RefreshMaps();

private:
    struct MapInfo {
        std::string name;
        std::wstring path;
    };

    size_t ExploreMaps(const std::wstring &path, std::vector<MapInfo> &maps);
    void OnSearchMaps();

    BMLMod *m_Mod;
    char m_MapSearchBuf[65536] = {};
    std::vector<size_t> m_MapSearchResult;
    std::vector<MapInfo> m_Maps;
};

class BMLMod : public IMod {
    friend class ModMenu;
    friend class MapListPage;
public:
    explicit BMLMod(IBML *bml) : IMod(bml), m_ModMenu(this) {}

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

    void OnDrawMenu();
    void OnResize();

    void OnOpenModsMenu();
    void OnCloseModsMenu();

    void OnOpenMapMenu();
    void OnCloseMapMenu(bool backToMenu = true);

    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    CKTimeManager *m_TimeManager = nullptr;
    InputHook *m_InputHook = nullptr;

    VxRect m_OldWindowRect;
    VxRect m_WindowRect;

    ImFont *m_Font = nullptr;

    ModMenu m_ModMenu;
    std::unique_ptr<MapListPage> m_MapListPage;

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
    IProperty *m_LanternAlphaTest = nullptr;
    IProperty *m_WidescreenFix = nullptr;
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
};

#endif // BML_BMLMOD_H