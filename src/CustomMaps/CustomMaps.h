#ifndef BML_CUSTOMMAPS_H
#define BML_CUSTOMMAPS_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "BML/Behavior.hpp"
#include "CustomMaps/MapMenu.h"

class CK2dEntity;
class CKBehavior;
class CKContext;
class CKDataArray;
class IBML;
class IConfig;
class ILogger;
class IProperty;
struct BML_DataShare;

namespace CustomMap {
class LevelLoader;
class MapCommand;
}

class CustomMaps {
public:
    CustomMaps();
    ~CustomMaps();

    CustomMaps(const CustomMaps &) = delete;
    CustomMaps &operator=(const CustomMaps &) = delete;

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
    bool LoadFromCommand(std::string_view relativePath, std::string &error);
    std::vector<std::string> ListMaps(std::string_view fragment, std::string &error) const;

private:
    enum class LoadOrigin {
        Menu,
        Command,
    };

    struct LoadAttempt;
    bool LoadMap(const std::wstring &path);
    bool BeginLoad(const std::wstring &path, LoadOrigin origin, std::string &error);
    bool CreateTempMapFile(const std::wstring &path, std::uint64_t attempt,
                           std::wstring &widePath, std::string &ansiPath) const;
    bool IsRuntimeReady() const;
    bool PublishLoadMetadata(const std::wstring &path, std::uint64_t attempt);
    void PollLoadResult();
    void TryCompleteLoad();
    void CompleteLoadSuccess();
    void CompleteLoadFailure(const char *reason);
    BML::Behavior::Result<void> RollbackLoad();
    void ReactivateStartMenu();
    void InstallLevelLoader();
    void RefreshLevelLoader();
    void ResetLevelLoader();
    void ClearLoadMetadata();
    void ReleaseDataShare();
    void ResetScriptBindings();

    IBML *m_BML = nullptr;
    CKContext *m_CKContext = nullptr;
    ILogger *m_Logger = nullptr;
    BML_DataShare *m_DataShare = nullptr;
    std::wstring m_TempDirectory;
    std::wstring m_MapsDirectory;
    std::unique_ptr<LoadAttempt> m_LoadAttempt;
    std::uint64_t m_NextLoadAttempt = 1;

    MapMenu m_Menu;
    BML::Behavior::Session m_Behavior;
    std::unique_ptr<CustomMap::MapCommand> m_Command;
    std::unique_ptr<CustomMap::LevelLoader> m_LevelLoader;

    IProperty *m_LevelNumber = nullptr;
    IProperty *m_ShowTooltip = nullptr;
    IProperty *m_MaxDepth = nullptr;

    CK2dEntity *m_LevelButton = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKDataArray *m_CurrentLevel = nullptr;
};

#endif // BML_CUSTOMMAPS_H
