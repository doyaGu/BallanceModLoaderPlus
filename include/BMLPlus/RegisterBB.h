#ifndef BML_REGISTERBB_H
#define BML_REGISTERBB_H

#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <functional>

#include "Defines.h"

typedef std::function<int(const CKBehaviorContext &)> BehaviorFunction;

struct BuildingBlockParameter {
    std::string name;
    CKGUID type;
    std::string value;

    BuildingBlockParameter(const char *name, CKGUID type) : name(name), type(type) {}

    BuildingBlockParameter(std::string name, CKGUID type) : name(std::move(name)), type(type) {}

    BuildingBlockParameter(const char *name, CKGUID type, const char *value) :
        name(name), type(type), value(value) {}

    BuildingBlockParameter(std::string name, CKGUID type, std::string value) :
        name(std::move(name)), type(type), value(std::move(value)) {}

    BuildingBlockParameter(const BuildingBlockParameter &rhs) = default;

    BuildingBlockParameter &operator=(const BuildingBlockParameter &rhs) = default;

    BuildingBlockParameter(BuildingBlockParameter &&rhs) = default;

    BuildingBlockParameter &operator=(BuildingBlockParameter &&rhs) = default;

    bool operator==(const BuildingBlockParameter &rhs) const {
        return name == rhs.name &&
               type == rhs.type &&
               value == rhs.value;
    }

    bool operator!=(const BuildingBlockParameter &rhs) const {
        return !(rhs == *this);
    }

    bool operator<(const BuildingBlockParameter &rhs) const {
        if (name < rhs.name)
            return true;
        if (rhs.name < name)
            return false;
        if (type < rhs.type)
            return true;
        if (rhs.type < type)
            return false;
        return value < rhs.value;
    }

    bool operator>(const BuildingBlockParameter &rhs) const {
        return rhs < *this;
    }

    bool operator<=(const BuildingBlockParameter &rhs) const {
        return !(rhs < *this);
    }

    bool operator>=(const BuildingBlockParameter &rhs) const {
        return !(*this < rhs);
    }
};

class BuildingBlock {
public:
    BuildingBlock() = default;

    explicit BuildingBlock(CKGUID guid) : m_Guid(guid) {}

    BuildingBlock(const BuildingBlock &rhs) = delete;
    BuildingBlock &operator=(const BuildingBlock &rhs) = delete;

    BuildingBlock(BuildingBlock &&rhs) = default;
    BuildingBlock &operator=(BuildingBlock &&rhs) = default;

    bool operator==(const BuildingBlock &rhs) const {
        return m_Guid == rhs.m_Guid;
    }
    bool operator!=(const BuildingBlock &rhs) const {
        return !(rhs == *this);
    }
    bool operator<(const BuildingBlock &rhs) const {
        return m_Guid < rhs.m_Guid;
    }
    bool operator>(const BuildingBlock &rhs) const {
        return rhs < *this;
    }
    bool operator<=(const BuildingBlock &rhs) const {
        return !(rhs < *this);
    }
    bool operator>=(const BuildingBlock &rhs) const {
        return !(*this < rhs);
    }

    const std::string &GetName() const {
        return m_Name;
    }

    const std::string &GetDescription() const {
        return m_Description;
    }

    const std::string &GetCategory() const {
        return m_Category;
    }

    CKGUID GetGuid() const {
        return m_Guid;
    }

    CKGUID GetAuthorGuid() const {
        return m_AuthorGuid;
    }

    const std::string &GetAuthor() const {
        return m_Author;
    }

    CKDWORD GetVersion() const {
        return m_Version;
    }

    CK_CLASSID GetCompatibleClassId() const {
        return m_CompatibleClassId;
    }

    const std::vector<CKGUID> &GetManagerNeeded() const {
        return m_ManagerNeeded;
    }

    const std::vector<std::string> &GetInputs() const {
        return m_Inputs;
    }

    const std::vector<std::string> &GetOutputs() const {
        return m_Outputs;
    }

    const std::vector<BuildingBlockParameter> &GetInputParams() const {
        return m_InputParams;
    }

    const std::vector<BuildingBlockParameter> &GetOutputParams() const {
        return m_OutputParams;
    }

    const std::vector<BuildingBlockParameter> &GetLocalParams() const {
        return m_LocalParams;
    }

    const std::vector<BuildingBlockParameter> &GetSettings() const {
        return m_Settings;
    }

    CKBEHAVIORFCT GetFunction() const {
        return m_Function;
    }

    CKBEHAVIORCALLBACKFCT GetCallback() const {
        return m_Callback;
    }

    CKDWORD GetCallbackMask() const {
        return m_CallbackMask;
    }

    void *GetCallbackParam() const {
        return m_CallbackParam;
    }

    CK_BEHAVIORPROTOTYPE_FLAGS GetFlags() const {
        return m_Flags;
    }

    CK_BEHAVIOR_FLAGS GetBehaviorFlags() const {
        return m_BehaviorFlags;
    }

    BuildingBlock &SetName(const char *name) {
        m_Name = name;
        return *this;
    }

    BuildingBlock &SetDescription(const char *description) {
        m_Description = description;
        return *this;
    }

    BuildingBlock &SetCategory(const char *category) {
        m_Category = category;
        return *this;
    }

    BuildingBlock &SetGuid(CKGUID guid) {
        m_Guid = guid;
        return *this;
    }

    BuildingBlock &SetAuthorGuid(CKGUID guid) {
        m_AuthorGuid = guid;
        return *this;
    }

    BuildingBlock &SetAuthor(const char *author) {
        m_Author = author;
        return *this;
    }

    BuildingBlock &SetVersion(CKDWORD version) {
        m_Version = version;
        return *this;
    }

    BuildingBlock &SetCompatibleClassId(CK_CLASSID cid) {
        m_CompatibleClassId = cid;
        return *this;
    }

    BuildingBlock &AddManagerNeeded(CKGUID guid) {
        m_ManagerNeeded.push_back(guid);
        return *this;
    }

    BuildingBlock &AddInput(const char *input) {
        m_Inputs.emplace_back(input);
        return *this;
    }

    BuildingBlock &AddInputs(const std::vector<std::string> &inputs) {
        for (auto &input: inputs)
            m_Inputs.push_back(input);
        return *this;
    }

    BuildingBlock &AddOutput(const char *output) {
        m_Outputs.emplace_back(output);
        return *this;
    }

    BuildingBlock &AddOutputs(const std::vector<std::string> &outputs) {
        for (auto &output: outputs)
            m_Outputs.push_back(output);
        return *this;
    }

    BuildingBlock &AddInputParam(const char *name, CKGUID type, const char *defaultValue = "") {
        if (name == nullptr)
            name = "";
        if (defaultValue == nullptr)
            defaultValue = "";
        m_InputParams.emplace_back(name, type, defaultValue);
        return *this;
    }

    BuildingBlock &AddInputParams(const std::vector<BuildingBlockParameter> &inputParams) {
        for (auto &param: inputParams)
            m_InputParams.push_back(param);
        return *this;
    }

    BuildingBlock &AddOutputParam(const char *name, CKGUID type, const char *defaultValue = "") {
        if (name == nullptr)
            name = "";
        if (defaultValue == nullptr)
            defaultValue = "";
        m_OutputParams.emplace_back(name, type, defaultValue);
        return *this;
    }

    BuildingBlock &AddOutputParams(const std::vector<BuildingBlockParameter> &outputParams) {
        for (auto &param: outputParams)
            m_OutputParams.push_back(param);
        return *this;
    }

    BuildingBlock &AddLocalParam(const char *name, CKGUID type, const char *defaultValue = "") {
        if (name == nullptr)
            name = "";
        if (defaultValue == nullptr)
            defaultValue = "";
        m_LocalParams.emplace_back(name, type, defaultValue);
        return *this;
    }

    BuildingBlock &AddLocalParams(const std::vector<BuildingBlockParameter> &localParams) {
        for (auto &param: localParams)
            m_LocalParams.push_back(param);
        return *this;
    }

    BuildingBlock &AddSetting(const char *name, CKGUID type, const char *defaultValue = "") {
        if (name == nullptr)
            name = "";
        if (defaultValue == nullptr)
            defaultValue = "";
        m_Settings.emplace_back(name, type, defaultValue);
        return *this;
    }

    BuildingBlock &AddSettings(const std::vector<BuildingBlockParameter> &settings) {
        for (auto &setting: settings)
            m_Settings.push_back(setting);
        return *this;
    }

    BuildingBlock &SetFunction(CKBEHAVIORFCT fct) {
        m_Function = fct;
        return *this;
    }

    BuildingBlock &SetCallback(CKBEHAVIORCALLBACKFCT callback) {
        m_Callback = callback;
        return *this;
    }

    BuildingBlock &SetCallbackMask(CKDWORD mask) {
        m_CallbackMask = mask;
        return *this;
    }

    BuildingBlock &SetCallbackParam(void *param) {
        m_CallbackParam = param;
        return *this;
    }

    BuildingBlock &SetFlags(CK_BEHAVIORPROTOTYPE_FLAGS flags) {
        m_Flags = flags;
        return *this;
    }

    BuildingBlock &SetBehaviorFlags(CK_BEHAVIOR_FLAGS behaviorFlags) {
        m_BehaviorFlags = behaviorFlags;
        return *this;
    }


    BML_EXPORT bool Load(CKGUID guid = CKGUID());

    BML_EXPORT void Register(XObjectDeclarationArray *reg);

    BML_EXPORT static CKObjectDeclaration *FillDeclaration(BuildingBlock &bb);

    BML_EXPORT static CKBehaviorPrototype *CreatePrototype(BuildingBlock &bb);

protected:
    std::string m_Name;
    std::string m_Description;
    std::string m_Category;
    CKGUID m_Guid;
    CKGUID m_AuthorGuid;
    std::string m_Author;
    CKDWORD m_Version = 0x00010000;
    CK_CLASSID m_CompatibleClassId = CKCID_BEOBJECT;
    std::vector<CKGUID> m_ManagerNeeded;
    std::string m_PrototypeName;
    std::vector<std::string> m_Inputs;
    std::vector<std::string> m_Outputs;
    std::vector<BuildingBlockParameter> m_InputParams;
    std::vector<BuildingBlockParameter> m_OutputParams;
    std::vector<BuildingBlockParameter> m_LocalParams;
    std::vector<BuildingBlockParameter> m_Settings;
    CKBEHAVIORFCT m_Function = nullptr;
    CKBEHAVIORCALLBACKFCT m_Callback = nullptr;
    CKDWORD m_CallbackMask = CKCB_BEHAVIORALL;
    void *m_CallbackParam = nullptr;
    CK_BEHAVIORPROTOTYPE_FLAGS m_Flags = CK_BEHAVIORPROTOTYPE_NORMAL;
    CK_BEHAVIOR_FLAGS m_BehaviorFlags = CKBEHAVIOR_NONE;
};

class BuildingBlockHook : public BuildingBlock {
public:
    explicit BuildingBlockHook(const CKGUID &guid) : BuildingBlock(guid) {
        AddInput("In");
        AddOutput("Out");
        SetCategory("BML/Hook");
    }

    BuildingBlockHook(const BuildingBlockHook &rhs) = delete;

    BuildingBlockHook &operator=(const BuildingBlockHook &rhs) = delete;

    BuildingBlockHook(BuildingBlockHook &&rhs) = default;

    BuildingBlockHook &operator=(BuildingBlockHook &&rhs) = default;


    void SetCancellable() {
        AddOutput("Cancelled");
    }

    void AddModifiableParam(const char *name, CKGUID type) {
        m_OutputPos.push_back(static_cast<int>(m_InputParams.size()));
        AddInputParam(name, type);
        AddOutputParam(name, type);
    }

    const std::vector<int> &GetOutputPos() const {
        return m_OutputPos;
    }

protected:
    std::vector<int> m_OutputPos;
};

BML_EXPORT std::unique_ptr<BuildingBlockHook> CreateBuildingBlockHook(const char *name, const char *desc, CKGUID guid, const BehaviorFunction &callback);

BML_EXPORT bool HookBehaviorFunction(CKGUID guid, CKBEHAVIORFCT fct);

BML_EXPORT bool HookBehaviorFunction(CKGUID guid, const BehaviorFunction& prologue, const BehaviorFunction& episode);

BML_EXPORT bool UnhookBehaviorFunction(CKGUID guid);

BML_EXPORT bool IsBehaviorFunctionHooked(CKGUID guid);

BML_EXPORT CKBEHAVIORFCT GetBehaviorFunction(CKGUID guid);

#endif // BML_REGISTERBB_H