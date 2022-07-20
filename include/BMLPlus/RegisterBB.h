#ifndef BML_REGISTERBB_H
#define BML_REGISTERBB_H

#include <utility>
#include <vector>
#include <string>
#include <functional>

#include "CKAll.h"

#include "Export.h"
#include "Version.h"
#include "Defines.h"

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif

class BML_EXPORT HookParams {
public:
    explicit HookParams(CKBehavior *beh) : m_Beh(beh) {}

    void SetParamObject(int pos, CKObject *value) { m_Beh->SetOutputParameterObject(pos, value); }
    void SetParamString(int pos, const char *value) {
        m_Beh->GetOutputParameter(pos)->SetStringValue(TOCKSTRING(value));
    }
    template<typename T>
    void SetParamValue(int pos, T value) {
        m_Beh->SetOutputParameterValue(pos, &value, sizeof(value));
    }

    CKObject *GetParamObject(int pos) { return m_Beh->GetInputParameterObject(pos); }
    const char *GetParamString(int pos) { return static_cast<const char *>(m_Beh->GetInputParameterReadDataPtr(pos)); }
    template<typename T>
    T GetParamValue(int pos) {
        T res;
        m_Beh->GetInputParameterValue(pos, &res);
        return res;
    }

    void ActivateInput(int pos) { m_Beh->ActivateInput(pos); }
    void DeactivateInput(int pos) { m_Beh->ActivateInput(pos, false); }

    void ActivateOutput(int pos) { m_Beh->ActivateOutput(pos); }
    void DeactivateOutput(int pos) { m_Beh->ActivateOutput(pos, false); }

    CKBehavior *m_Beh;
};

class BML_EXPORT BBBuilder {
public:
    BBBuilder *SetName(const char *name) {
        m_Name = name;
        return this;
    }

    BBBuilder *SetDescription(const char *desc) {
        m_Desc = desc;
        return this;
    }

    BBBuilder *SetCategory(const char *cate) {
        m_Category = std::string("BML/") + cate;
        return this;
    }

    BBBuilder *SetGuid(CKGUID guid) {
        m_Guid = guid;
        return this;
    }

    BBBuilder *SetAuthor(const char *author) {
        m_Author = author;
        return this;
    }

    BBBuilder *SetAuthorGuid(CKGUID guid) {
        m_AuthorGuid = guid;
        return this;
    }

    BBBuilder *SetVersion(CKDWORD version) {
        m_Version = version;
        return this;
    }

    BBBuilder *SetClassID(CK_CLASSID id) {
        m_ClassID = id;
        return this;
    }

    BBBuilder *SetBehaviorFlags(CKDWORD flags) {
        m_BehFlags = flags;
        return this;
    }

    BBBuilder *SetFlags(CK_BEHAVIORPROTOTYPE_FLAGS flags) {
        m_Flags = flags;
        return this;
    }

    BBBuilder *SetFunction(CKBEHAVIORFCT function) {
        m_Function = function;
        return this;
    }

    BBBuilder *SetCallback(CKBEHAVIORCALLBACKFCT fct, CKDWORD CallbackMask = CKCB_BEHAVIORALL, void *param = nullptr) {
        m_Callback = fct;
        m_CallbackMask = CallbackMask;
        m_CallbackParam = param;
        return this;
    }

    BBBuilder *AddInput(const char *name) {
        m_Inputs.emplace_back(name);
        return this;
    }

    BBBuilder *AddOutput(const char *name) {
        m_Outputs.emplace_back(name);
        return this;
    }

    BBBuilder *AddInputParam(const char *name, CKGUID type) {
        m_InputParams.emplace_back(name, type);
        return this;
    }

    BBBuilder *AddOutputParam(const char *name, CKGUID type) {
        m_OutputParams.emplace_back(name, type);
        return this;
    }

    CKObjectDeclaration *Build();

    CKDLL_CREATEPROTOFUNCTION BuildProto();

    virtual CKBEHAVIORFCT BuildFunction() { return m_Function; }

    virtual ~BBBuilder() = default;

    std::string m_Name;
    std::string m_Desc;
    std::string m_Category;
    std::string m_Author = "Gamepiaynmo";
    CKGUID m_Guid;
    CKGUID m_AuthorGuid = BML_GUID;
    CKDWORD m_Version = BML_MAJOR_VER << 16 | BML_MINOR_VER;
    CK_CLASSID m_ClassID = CKCID_BEOBJECT;
    std::vector<std::string> m_Inputs;
    std::vector<std::string> m_Outputs;
    std::vector<std::pair<std::string, CKGUID>> m_InputParams;
    std::vector<std::pair<std::string, CKGUID>> m_OutputParams;
    CKDWORD m_BehFlags = 0;
    CK_BEHAVIORPROTOTYPE_FLAGS m_Flags = CK_BEHAVIORPROTOTYPE_NORMAL;
    CKBEHAVIORCALLBACKFCT m_Callback = nullptr;
    CKDWORD m_CallbackMask = 0;
    void *m_CallbackParam = nullptr;
    CKBEHAVIORFCT m_Function = nullptr;
};

class BML_EXPORT HookBuilder : public BBBuilder {
public:
    typedef std::function<bool(HookParams *)> ProcessFunction;

    HookBuilder() {
        AddInput("In");
        AddOutput("Out");
        SetCategory("Hook");
    }

    HookBuilder *SetName(const char *name) { m_Name = name; return this; }
    HookBuilder *SetDescription(const char *desc) { m_Desc = desc; return this; }
    HookBuilder *SetCategory(const char *cate) { m_Category = std::string("BML/") + cate; return this; }
    HookBuilder *SetGuid(CKGUID guid) { m_Guid = guid; return this; }

    HookBuilder *SetProcessFunction(ProcessFunction function) {
        m_ProcFunc = std::move(function);
        return this;
    }

    HookBuilder *SetCancellable() {
        AddOutput("Cancelled");
        return this;
    }

    HookBuilder *AddModifiableParam(const char *name, CKGUID type) {
        m_OutputPos.push_back(m_InputParams.size());
        AddInputParam(name, type);
        AddOutputParam(name, type);
        return this;
    }

    CKBEHAVIORFCT BuildFunction() override;

    std::vector<int> m_OutputPos;
    ProcessFunction m_ProcFunc;
};

template <typename T>
class BuilderFactory {
public:
    BuilderFactory() = default;
    ~BuilderFactory() {
        for (auto* builder : m_Builders) {
            if (builder) delete builder;
        }
    }

    T* NewBuilder() {
        auto* builder = new T();
        m_Builders.push_back(builder);
        return builder;
    }

    void DeleteBuilder(T* builder) {
        if (std::find(m_Builders.cbegin(), m_Builders.cend(), builder) != m_Builders.cend()) {
            std::remove(m_Builders.begin(), m_Builders.end(), builder);
            if (builder) delete builder;
        }
    }

private:
    std::vector<T*> m_Builders;
};

#ifdef BML_EXPORTS
void RegisterBBs(XObjectDeclarationArray *reg);
#endif

#endif // BML_REGISTERBB_H