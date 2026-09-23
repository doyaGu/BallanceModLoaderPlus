#ifndef BML_SCRIPT_MENU_PAGE_SERVICE_H
#define BML_SCRIPT_MENU_PAGE_SERVICE_H

#include <cstdint>
#include <memory>
#include <string>

#include "BML/ModMenu.h"

class ModContext;
class asIScriptFunction;

namespace BML {

class ScriptMod;
class ScriptMenuPageServiceState;

struct ScriptMenuPageDefinition {
    std::string Id;
    std::string Label;
    std::string Description;
    bool ShowInDetails = true;
};

class ScriptMenuPageFrame {
public:
    bool Push(const std::string &id);
    bool Replace(const std::string &id);
    void Back();
    void Close();

    BML_ModMenuPageAction GetAction() const { return m_Action; }
    const std::string &GetTargetPageId() const { return m_TargetPageId; }

private:
    BML_ModMenuPageAction m_Action = BML_MOD_MENU_PAGE_NONE;
    std::string m_TargetPageId;
};

class ScriptMenuPageRef {
public:
    ScriptMenuPageRef(std::weak_ptr<ScriptMenuPageServiceState> state,
                      std::string id, std::uint64_t generation);

    void AddRef();
    void Release();
    bool IsValid() const;
    std::string GetId() const;
    bool Unregister();

private:
    int m_RefCount = 1;
    std::weak_ptr<ScriptMenuPageServiceState> m_State;
    std::string m_Id;
    std::uint64_t m_Generation = 0;
};

class ScriptMenuPageService {
public:
    ScriptMenuPageService();
    ~ScriptMenuPageService();

    bool Bind(ModContext *context, ScriptMod *owner);
    ScriptMenuPageRef *Register(const ScriptMenuPageDefinition &definition,
                                asIScriptFunction *draw,
                                asIScriptFunction *enter,
                                asIScriptFunction *leave);
    bool Unregister(const std::string &id);
    void Release();

private:
    std::shared_ptr<ScriptMenuPageServiceState> m_State;
};

} // namespace BML

#endif // BML_SCRIPT_MENU_PAGE_SERVICE_H
