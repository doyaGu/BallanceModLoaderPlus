#include "RegisterBB.h"

#include <cctype>
#include <vector>
#include <memory>
#include <unordered_map>

#include "CKAll.h"

#include "xbyak/xbyak.h"

struct CKGUIDHash {
    size_t operator()(const CKGUID &guid) const {
        auto hasher = std::hash<CKDWORD>();
        return hasher(guid.d1) ^ hasher(guid.d2);
    }
};

class CreateFunctionGenerator : public Xbyak::CodeGenerator {
public:
    explicit CreateFunctionGenerator(BuildingBlock *bb) {
        sub(esp, 8);
        mov(ecx, dword[esp + 12]);
        mov(dword[esp + 4], ecx);
        mov(dword[esp], reinterpret_cast<uint32_t>(bb));
        call(BuildingBlock::CreatePrototype);
        mov(ecx, dword[esp + 4]);
        mov(dword[ecx], eax);
        xor_(eax, eax);
        add(esp, 8);
        ret();
    }

    CKDLL_CREATEPROTOFUNCTION Get() const {
        return getCode<CKDLL_CREATEPROTOFUNCTION>();
    }

    static CreateFunctionGenerator &NewGenerator(BuildingBlock *bb) {
        s_CreateProtoFunctionGenerators.emplace_back(std::make_unique<CreateFunctionGenerator>(bb));
        return *s_CreateProtoFunctionGenerators.back();
    }

    static std::vector<std::unique_ptr<CreateFunctionGenerator>> s_CreateProtoFunctionGenerators;
};

std::vector<std::unique_ptr<CreateFunctionGenerator>> CreateFunctionGenerator::s_CreateProtoFunctionGenerators;

class BehaviorFunctionGenerator : public Xbyak::CodeGenerator {
public:
    typedef std::unordered_map<CKGUID, std::unique_ptr<BehaviorFunctionGenerator>, CKGUIDHash> BehaviorFunctionGeneratorMap;

    explicit BehaviorFunctionGenerator(CKBEHAVIORFCT fct, BehaviorFunction prologue, BehaviorFunction episode) : 
        m_Prologue(std::move(prologue)), m_Episode(std::move(episode)) {
        sub(esp, 12);

        if (m_Prologue != nullptr) {
            mov(eax, dword[esp + 16]);
            mov(dword[esp], eax);
            mov(dword[esp + 4], reinterpret_cast<uint32_t>(&m_Prologue));
            call(PrologueWrapper);

            if (m_Episode != nullptr) {
                cmp(eax, CK_OK);
                jne(".episode");
            }
        }

        if (fct != nullptr) {
            mov(eax, dword[esp + 16]);
            mov(dword[esp], eax);
            call(fct);
            mov(dword[esp + 8], eax);
        }

        if (m_Episode != nullptr) {
            L(".episode");
            mov(eax, dword[esp + 16]);
            mov(dword[esp], eax);
            mov(dword[esp + 4], reinterpret_cast<uint32_t>(&m_Episode));
            call(EpilogueWrapper);
        }

        if (fct != nullptr) {
            mov(eax, dword[esp + 8]);
        }

        add(esp, 12);
        ret();
    }

    CKBEHAVIORFCT Get() const {
        return getCode<CKBEHAVIORFCT>();
    }

    static BehaviorFunctionGenerator &NewGenerator(CKGUID guid, CKBEHAVIORFCT fct, const BehaviorFunction &prologue, const BehaviorFunction &episode) {
        if (s_BehaviorFunctionGeneratorMap.find(guid) == s_BehaviorFunctionGeneratorMap.end())
            s_BehaviorFunctionGeneratorMap.insert({guid, std::make_unique<BehaviorFunctionGenerator>(fct, prologue, episode)});
        return *s_BehaviorFunctionGeneratorMap[guid];
    }

    static void DeleteGenerator(CKGUID guid) {
        s_BehaviorFunctionGeneratorMap.erase(guid);
    }

private:
    static int PrologueWrapper(const CKBehaviorContext &behcontext, BehaviorFunction *prologue) {
        return (*prologue)(behcontext);
    }

    static int EpilogueWrapper(const CKBehaviorContext &behcontext, BehaviorFunction *epilogue) {
        return (*epilogue)(behcontext);
    }

    BehaviorFunction m_Prologue;
    BehaviorFunction m_Episode;

    static BehaviorFunctionGeneratorMap s_BehaviorFunctionGeneratorMap;
};

BehaviorFunctionGenerator::BehaviorFunctionGeneratorMap BehaviorFunctionGenerator::s_BehaviorFunctionGeneratorMap;

typedef std::unordered_map<CKGUID, CKBEHAVIORFCT, CKGUIDHash> BehaviorFunctionMap;
static BehaviorFunctionMap g_BehaviorFunctionMap;

CKObjectDeclaration *BuildingBlock::FillDeclaration(BuildingBlock &bb) {
    CKObjectDeclaration *od = CreateCKObjectDeclaration(TOCKSTRING(bb.m_Name.c_str()));
    od->SetDescription(TOCKSTRING(bb.m_Description.c_str()));
    od->SetCategory(TOCKSTRING(bb.m_Category.c_str()));
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(bb.m_Guid);
    od->SetAuthorGuid(bb.m_AuthorGuid);
    od->SetAuthorName(TOCKSTRING(bb.m_Author.c_str()));
    od->SetVersion(bb.m_Version);

    auto &generator = CreateFunctionGenerator::NewGenerator(&bb);
    od->SetCreationFunction(generator.Get());

    od->SetCompatibleClassId(bb.m_CompatibleClassId);

    return od;
}

CKBehaviorPrototype *BuildingBlock::CreatePrototype(BuildingBlock &bb) {
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype(TOCKSTRING(bb.m_Name.c_str()));

    for (auto &name: bb.m_Inputs)
        proto->DeclareInput(TOCKSTRING(name.c_str()));
    for (auto &name: bb.m_Outputs)
        proto->DeclareOutput(TOCKSTRING(name.c_str()));

    for (auto &param: bb.m_InputParams)
        proto->DeclareInParameter(TOCKSTRING(param.name.c_str()), param.type, TOCKSTRING(param.value.c_str()));
    for (auto &param: bb.m_OutputParams)
        proto->DeclareOutParameter(TOCKSTRING(param.name.c_str()), param.type, TOCKSTRING(param.value.c_str()));

    for (auto &param: bb.m_LocalParams)
        proto->DeclareOutParameter(TOCKSTRING(param.name.c_str()), param.type, TOCKSTRING(param.value.c_str()));
    for (auto &setting: bb.m_Settings)
        proto->DeclareOutParameter(TOCKSTRING(setting.name.c_str()), setting.type, TOCKSTRING(setting.value.c_str()));

    proto->SetFlags(bb.m_Flags);

    if (bb.m_Function)
        proto->SetFunction(bb.m_Function);
    else
        proto->SetFunction([](const CKBehaviorContext &context) { return CK_OK; });

    if (bb.m_BehaviorFlags != 0)
        proto->SetBehaviorFlags(static_cast<CK_BEHAVIOR_FLAGS>(bb.m_BehaviorFlags));

    if (bb.m_Callback)
        proto->SetBehaviorCallbackFct(bb.m_Callback, bb.m_CallbackMask, bb.m_CallbackParam);

    return proto;
}

bool BuildingBlock::Load() {
    CKObjectDeclaration *od = CKGetObjectDeclarationFromGuid(m_Guid);
    if (!od) return false;

    m_Name = od->GetName();
    m_Description = od->GetDescription();
    m_Category = od->GetCategory();
    m_AuthorGuid = od->GetAuthorGuid();
    m_Author = od->GetAuthorName();
    m_Version = od->GetVersion();
    m_CompatibleClassId = od->GetCompatibleClassId();

    for (int i = 0; i < od->GetManagerNeededCount(); ++i)
        AddManagerNeeded(od->GetManagerNeeded(i));

    CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(m_Guid);
    if (!proto) return false;

    m_PrototypeName = proto->GetName();

    auto *inIos = proto->GetInIOList();
    for (int i = 0; i < proto->GetInputCount(); ++i)
        AddInput(inIos[i]->Name);

    auto *outIos = proto->GetOutIOList();
    for (int i = 0; i < proto->GetOutputCount(); ++i)
        AddOutput(outIos[i]->Name);

    auto *inParams = proto->GetInParameterList();
    for (int i = 0; i < proto->GetInParameterCount(); ++i)
        AddInputParam(inParams[i]->Name, inParams[i]->Guid, inParams[i]->DefaultValueString);

    auto *outParams = proto->GetOutParameterList();
    for (int i = 0; i < proto->GetOutParameterCount(); ++i)
        AddOutputParam(outParams[i]->Name, outParams[i]->Guid, outParams[i]->DefaultValueString);

    auto *localParams = proto->GetLocalParameterList();
    for (int i = 0; i < proto->GetLocalParameterCount(); ++i) {
        if (localParams[i]->Type == CKPARAMETER_LOCAL)
            AddLocalParam(localParams[i]->Name, localParams[i]->Guid, localParams[i]->DefaultValueString);
        else
        if (localParams[i]->Type == CKPARAMETER_SETTING)
            AddSetting(localParams[i]->Name, localParams[i]->Guid, localParams[i]->DefaultValueString);
    }

    m_Function = proto->GetFunction();
    m_Callback = proto->GetBehaviorCallbackFct();
    m_Flags = proto->GetFlags();
    m_BehaviorFlags = proto->GetBehaviorFlags();

    return true;
}

void BuildingBlock::Register(XObjectDeclarationArray *reg) {
    auto *od = FillDeclaration(*this);
    CKStoreDeclaration(reg, od);
}

std::unique_ptr<BuildingBlock> LoadBuildingBlock(CKGUID guid) {
    if (guid == CKGUID())
        return nullptr;

    CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(guid);
    if (!proto) return nullptr;

    auto buildingBlock = std::make_unique<BuildingBlock>(guid);
    buildingBlock->Load();

    return buildingBlock;
}

std::unique_ptr<BuildingBlock>
CreateBuildingBlock(const char *name, const char *desc, const char *category, CKGUID guid, CKGUID authorGuid, const char *author,
                    CKDWORD version, const std::vector<std::string> &inputs, const std::vector<std::string> &outputs,
                    const std::vector<BuildingBlockParameter> &inParams, const std::vector<BuildingBlockParameter> &outParams,
                    const std::vector<BuildingBlockParameter> &localParams, const std::vector<BuildingBlockParameter> &settings,
                    CKBEHAVIORFCT fct, CKBEHAVIORCALLBACKFCT callback, CKDWORD callbackMask, void *callbackParam, CK_BEHAVIORPROTOTYPE_FLAGS flags,
                    CK_BEHAVIOR_FLAGS behaviorFlags) {
    auto buildingBlock = std::make_unique<BuildingBlock>(guid);
    auto &bb = *buildingBlock;
    bb.SetName(name)
        .SetDescription(desc)
        .SetCategory(category)
        .SetAuthorGuid(authorGuid)
        .SetAuthor(author)
        .SetVersion(version)
        .AddInputs(inputs)
        .AddOutputs(outputs)
        .AddInputParams(inParams)
        .AddOutputParams(outParams)
        .AddLocalParams(localParams)
        .AddSettings(settings)
        .SetFunction(fct)
        .SetCallback(callback)
        .SetCallbackMask(callbackMask)
        .SetCallbackParam(callbackParam)
        .SetFlags(flags)
        .SetBehaviorFlags(behaviorFlags);

    return buildingBlock;
}

std::unique_ptr<BuildingBlockHook> CreateBuildingBlockHook(const char *name, const char *desc, CKGUID guid, const BehaviorFunction &callback) {
    auto buildingBlock = std::make_unique<BuildingBlockHook>(guid);
    auto &bb = *buildingBlock;
    bb.SetName(name)
        .SetDescription(desc)
        .SetCategory("BML/Hook")
        .SetAuthorGuid(BML_GUID)
        .SetAuthor("BML")
        .SetVersion(BML_MAJOR_VER << 16 | BML_MINOR_VER);

    auto &generator = BehaviorFunctionGenerator::NewGenerator(guid, nullptr, [&bb, callback](const CKBehaviorContext &behcontext) {
        CKBehavior *beh = behcontext.Behavior;

        const auto inParamCount = bb.GetOutputPos().size();
        for (size_t pos = 0; pos < inParamCount; pos++)
            beh->GetOutputParameter(static_cast<int>(pos))->CopyValue(beh->GetInputParameter(bb.GetOutputPos()[pos])->GetRealSource());

        bool cancelled = (callback(behcontext) != CK_OK) && beh->GetOutputCount() > 1;

        beh->ActivateInput(0, FALSE);
        beh->ActivateOutput(cancelled ? 1 : 0);
        return CKBR_OK;
    }, nullptr);
    bb.SetFunction(generator.Get());

    return buildingBlock;
}

bool HookBehaviorFunction(CKGUID guid, CKBEHAVIORFCT fct) {
    CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(guid);
    if (!proto) return false;

    g_BehaviorFunctionMap.insert({guid, proto->GetFunction()});

    proto->SetFunction(fct);

    return true;
}

bool HookBehaviorFunction(CKGUID guid, const BehaviorFunction& prologue, const BehaviorFunction& episode) {
    if (IsBehaviorFunctionHooked(guid))
        return false;

    CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(guid);
    if (!proto) return false;

    g_BehaviorFunctionMap.insert({guid, proto->GetFunction()});

    auto &generator = BehaviorFunctionGenerator::NewGenerator(guid, proto->GetFunction(), prologue, episode);
    proto->SetFunction(generator.Get());

    return true;
}

bool UnhookBehaviorFunction(CKGUID guid) {
    if (IsBehaviorFunctionHooked(guid)) {
        CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(guid);
        if (!proto) return false;
        proto->SetFunction(GetBehaviorFunction(guid));
        BehaviorFunctionGenerator::DeleteGenerator(guid);
        g_BehaviorFunctionMap.erase(guid);
    }
    return true;
}

bool IsBehaviorFunctionHooked(CKGUID guid) {
    return GetBehaviorFunction(guid) != nullptr;
}

CKBEHAVIORFCT GetBehaviorFunction(CKGUID guid) {
    auto it = g_BehaviorFunctionMap.find(guid);
    if (it == g_BehaviorFunctionMap.end())
        return nullptr;
    return it->second;
}