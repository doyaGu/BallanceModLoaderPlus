#ifndef BML_BUILTINCUSTOMMAPS_H
#define BML_BUILTINCUSTOMMAPS_H

#include <string>

#include "MapMenu.h"

class CK2dEntity;
class CKBehavior;
class CKContext;
class CKDataArray;
class CKParameter;
class IBML;
class IConfig;
class ILogger;
class IProperty;
struct BML_DataShare;

class BuiltinCustomMaps {
public:
    BuiltinCustomMaps();
    ~BuiltinCustomMaps();

    BuiltinCustomMaps(const BuiltinCustomMaps &) = delete;
    BuiltinCustomMaps &operator=(const BuiltinCustomMaps &) = delete;

    void InitConfig(IConfig &config);
    void ApplyConfig();
    bool OnModifyConfig(const char *category, const char *key, IProperty *property);

    void OnLoad(IBML &bml, ILogger &logger,
                const std::wstring &loaderDirectory, const std::wstring &tempDirectory);
    void OnUnload();
    void OnLoadObject(const char *filename);
    void OnLoadScript(CKBehavior *script);
    void OnProcess();
    void OnStartLevel();
    void OnExitGame();

    bool Open();
    bool Close();

private:
    bool LoadMap(const std::wstring &path);
    std::string CreateTempMapFile(const std::wstring &path) const;
    void PatchLevelLoader(CKBehavior *script);
    void ClearLoadMetadata();
    void ReleaseDataShare();
    void ResetScriptBindings();

    IBML *m_BML = nullptr;
    CKContext *m_CKContext = nullptr;
    ILogger *m_Logger = nullptr;
    BML_DataShare *m_DataShare = nullptr;
    bool m_MetadataPublished = false;
    std::wstring m_TempDirectory;

    MapMenu m_Menu;

    IProperty *m_LevelNumber = nullptr;
    IProperty *m_ShowTooltip = nullptr;
    IProperty *m_MaxDepth = nullptr;

    CK2dEntity *m_LevelButton = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKParameter *m_LoadCustom = nullptr;
    CKParameter *m_MapFile = nullptr;
    CKParameter *m_LevelRow = nullptr;
    CKDataArray *m_CurrentLevel = nullptr;
};

#endif // BML_BUILTINCUSTOMMAPS_H
