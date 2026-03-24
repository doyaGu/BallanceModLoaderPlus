#ifndef BML_MODMENU_REGISTRY_H
#define BML_MODMENU_REGISTRY_H

#include <memory>
#include <string>
#include <vector>

#include "CKContext.h"

#include "bml_services.hpp"
#include "bml_types.h"

class Config;

class LoadedMod {
public:
    explicit LoadedMod(BML_Mod handle);

    BML_Mod GetHandle() const { return m_Handle; }
    const char *GetID() const { return m_Id.c_str(); }
    const char *GetVersion() const { return m_Version.c_str(); }
    const char *GetName() const { return m_Name.c_str(); }
    const char *GetAuthor() const { return m_Author.c_str(); }
    const char *GetDescription() const { return m_Description.c_str(); }

private:
    BML_Mod m_Handle = nullptr;
    std::string m_Id;
    std::string m_Version;
    std::string m_Name;
    std::string m_Author;
    std::string m_Description;
};

class MenuRegistry {
public:
    static MenuRegistry &Instance();

    void Initialize(CKContext *context,
                    const BML_CoreModuleInterface *moduleInterface,
                    const BML_CoreConfigInterface *configInterface);
    void Reset();

    CKContext *GetCKContext() const { return m_Context; }
    CKBehavior *GetScriptByName(const char *name) const;

    int GetModCount();
    LoadedMod *GetMod(int index);
    Config *GetConfig(LoadedMod *mod);

private:
    MenuRegistry() = default;
    MenuRegistry(const MenuRegistry &) = delete;
    MenuRegistry &operator=(const MenuRegistry &) = delete;

    struct Record {
        std::unique_ptr<LoadedMod> mod;
        std::unique_ptr<Config> config;
    };

    void Refresh();

    CKContext *m_Context = nullptr;
    const BML_CoreModuleInterface *m_ModuleInterface = nullptr;
    const BML_CoreConfigInterface *m_ConfigInterface = nullptr;
    std::vector<Record> m_Records;
};

#endif // BML_MODMENU_REGISTRY_H
