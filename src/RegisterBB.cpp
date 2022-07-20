#include "RegisterBB.h"

#include <vector>

#include "Defines.h"
#include "ModLoader.h"

#include "asmjit/x86.h"

namespace {
    BuilderFactory<HookBuilder> g_BuilderFactory;
    asmjit::JitRuntime g_JitRuntime;
}

CKObjectDeclaration *BBBuilder::Build() {
    CKObjectDeclaration *od = CreateCKObjectDeclaration(TOCKSTRING(m_Name.c_str()));
    od->SetDescription(TOCKSTRING(m_Desc.c_str()));
    od->SetCategory(TOCKSTRING(m_Category.c_str()));
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(m_Guid);
    od->SetAuthorGuid(m_AuthorGuid);
    od->SetAuthorName(TOCKSTRING(m_Author.c_str()));
    od->SetVersion(m_Version);
    od->SetCreationFunction(BuildProto());
    od->SetCompatibleClassId(m_ClassID);
    return od;
}

CKBehaviorPrototype *CreatePrototype(BBBuilder *builder) {
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype(TOCKSTRING(builder->m_Name.c_str()));

    for (auto &name: builder->m_Inputs)
        proto->DeclareInput(TOCKSTRING(name.c_str()));
    for (auto &name: builder->m_Outputs)
        proto->DeclareOutput(TOCKSTRING(name.c_str()));

    for (auto &pair: builder->m_InputParams)
        proto->DeclareInParameter(TOCKSTRING(pair.first.c_str()), pair.second);
    for (auto &pair: builder->m_OutputParams)
        proto->DeclareOutParameter(TOCKSTRING(pair.first.c_str()), pair.second);

    if (builder->m_BehFlags != 0)
        proto->SetBehaviorFlags(static_cast<CK_BEHAVIOR_FLAGS>(builder->m_BehFlags));
    proto->SetFlags(builder->m_Flags);

    if (builder->m_Callback != nullptr)
        proto->SetBehaviorCallbackFct(builder->m_Callback, builder->m_CallbackMask, builder->m_CallbackParam);

    proto->SetFunction(builder->BuildFunction());

    return proto;
}

CKDLL_CREATEPROTOFUNCTION GenCreateProtoFunction(BBBuilder *builder) {
    using namespace asmjit;
    CodeHolder code;
    code.init(g_JitRuntime.environment());

    using namespace asmjit::x86;
    Compiler cc(&code);

    FuncNode *funcNode = cc.addFunc(FuncSignatureT<CKERROR, CKBehaviorPrototype**>(CallConvId::kHost));
    Gp pproto = cc.newIntPtr("pproto");
    Gp proto = cc.newIntPtr("proto");
    funcNode->setArg(0, pproto);

    InvokeNode *invokeNode;
    cc.invoke(&invokeNode,
              imm((void*)CreatePrototype),
              FuncSignatureT<CKBehaviorPrototype*, BBBuilder*>(CallConvId::kHost));
    invokeNode->setArg(0, imm((void*)builder));
    invokeNode->setRet(0, proto);

    cc.mov(dword_ptr(pproto), proto);
    cc.xor_(eax, eax);
    cc.ret(eax);
    cc.endFunc();

    cc.finalize();

    CKDLL_CREATEPROTOFUNCTION fct;
    Error err = g_JitRuntime.add(&fct, &code);
    if (err) return nullptr;
    return fct;
}

CKDLL_CREATEPROTOFUNCTION BBBuilder::BuildProto() {
    return GenCreateProtoFunction(this);
}

int HookFunction(const CKBehaviorContext &behcontext, HookBuilder *builder) {
    CKBehavior *beh = behcontext.Behavior;

    for (size_t pos = 0; pos < builder->m_OutputPos.size(); pos++)
        beh->GetOutputParameter(pos)->CopyValue(beh->GetInputParameter(builder->m_OutputPos[pos])->GetRealSource());

    HookParams params(beh);
    bool cancelled = builder->m_ProcFunc(&params) && beh->GetOutputCount() > 1;

    beh->ActivateInput(0, FALSE);
    beh->ActivateOutput(cancelled ? 1 : 0);
    return CKBR_OK;
}

CKBEHAVIORFCT GenBehaviorFunction(HookBuilder *builder) {
    using namespace asmjit;
    CodeHolder code;
    code.init(g_JitRuntime.environment());

    using namespace asmjit::x86;
    Compiler cc(&code);

    FuncNode* funcNode = cc.addFunc(FuncSignatureT<int, const CKBehaviorContext&>(CallConvId::kHost));
    Gp behcontext = cc.newIntPtr("behcontext");
    Gp retval = cc.newInt32("retval");
    funcNode->setArg(0, behcontext);

    InvokeNode* invokeNode;
    cc.invoke(&invokeNode,
              imm((void*)HookFunction),
              FuncSignatureT<int, const CKBehaviorContext&, HookBuilder*>(CallConvId::kHost));
    invokeNode->setArg(0, behcontext);
    invokeNode->setArg(1, imm((void*)builder));
    invokeNode->setRet(0, retval);

    cc.mov(eax, retval);
    cc.ret(eax);
    cc.endFunc();

    cc.finalize();

    CKBEHAVIORFCT fct;
    Error err = g_JitRuntime.add(&fct, &code);
    if (err) return nullptr;
    return fct;
}

CKBEHAVIORFCT HookBuilder::BuildFunction() {
    return GenBehaviorFunction(this);
}

void RegisterCallback(XObjectDeclarationArray *reg, const char *name, const char *desc, CKGUID guid,
                      std::function<void()> callback) {
    auto *builder = g_BuilderFactory.NewBuilder();
    CKStoreDeclaration(reg, builder
        ->SetName(name)
        ->SetDescription(desc)
        ->SetGuid(guid)
        ->SetProcessFunction([callback](HookParams *params) {
            callback();
            return false;
        })
        ->Build());
}

void RegisterBBs(XObjectDeclarationArray *reg) {
    RegisterCallback(reg, "BML OnPreStartMenu", "PreStartMenu Hook.", BML_ONPRESTARTMENU_GUID,
                     []() { ModLoader::GetInstance().OnPreStartMenu(); });
    RegisterCallback(reg, "BML OnPostStartMenu", "PostStartMenu Hook.", BML_ONPOSTSTARTMENU_GUID,
                     []() { ModLoader::GetInstance().OnPostStartMenu(); });
    RegisterCallback(reg, "BML OnExitGame", "ExitGame Hook.", BML_ONEXITGAME_GUID,
                     []() { ModLoader::GetInstance().OnExitGame(); });
    RegisterCallback(reg, "BML OnPreLoadLevel", "PreLoadLevel Hook.", BML_ONPRELOADLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPreLoadLevel(); });
    RegisterCallback(reg, "BML OnPostLoadLevel", "PostLoadLevel Hook.", BML_ONPOSTLOADLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPostLoadLevel(); });
    RegisterCallback(reg, "BML OnStartLevel", "StartLevel Hook.", BML_ONSTARTLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnStartLevel(); });
    RegisterCallback(reg, "BML OnPreResetLevel", "PreResetLevel Hook.", BML_ONPRERESETLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPreResetLevel(); });
    RegisterCallback(reg, "BML OnPostResetLevel", "PostResetLevel Hook.", BML_ONPOSTRESETLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPostResetLevel(); });
    RegisterCallback(reg, "BML OnPauseLevel", "PauseLevel Hook.", BML_ONPAUSELEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPauseLevel(); });
    RegisterCallback(reg, "BML OnUnpauseLevel", "UnpauseLevel Hook.", BML_ONUNPAUSELEVEL_GUID,
                     []() { ModLoader::GetInstance().OnUnpauseLevel(); });
    RegisterCallback(reg, "BML OnPreExitLevel", "PreExitLevel Hook.", BML_ONPREEXITLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPreExitLevel(); });
    RegisterCallback(reg, "BML OnPostExitLevel", "PostExitLevel Hook.", BML_ONPOSTEXITLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPostExitLevel(); });
    RegisterCallback(reg, "BML OnPreNextLevel", "PreNextLevel Hook.", BML_ONPRENEXTLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPreNextLevel(); });
    RegisterCallback(reg, "BML OnPostNextLevel", "PostNextLevel Hook.", BML_ONPOSTNEXTLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPostNextLevel(); });
    RegisterCallback(reg, "BML OnDead", "Dead Hook.", BML_ONDEAD_GUID, []() { ModLoader::GetInstance().OnDead(); });
    RegisterCallback(reg, "BML OnPreEndLevel", "PreEndLevel Hook.", BML_ONPREENDLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPreEndLevel(); });
    RegisterCallback(reg, "BML OnPostEndLevel", "PostEndLevel Hook.", BML_ONPOSTENDLEVEL_GUID,
                     []() { ModLoader::GetInstance().OnPostEndLevel(); });

    RegisterCallback(reg, "BML OnCounterActive", "CounterActive Hook.", BML_ONCOUNTERACTIVE_GUID,
                     []() { ModLoader::GetInstance().OnCounterActive(); });
    RegisterCallback(reg, "BML OnCounterInactive", "CounterInactive Hook.", BML_ONCOUNTERINACTIVE_GUID,
                     []() { ModLoader::GetInstance().OnCounterInactive(); });
    RegisterCallback(reg, "BML OnBallNavActive", "BallNavActive Hook.", BML_ONBALLNAVACTIVE_GUID,
                     []() { ModLoader::GetInstance().OnBallNavActive(); });
    RegisterCallback(reg, "BML OnBallNavInactive", "BallNavInactive Hook.", BML_ONBALLNAVINACTIVE_GUID,
                     []() { ModLoader::GetInstance().OnBallNavInactive(); });
    RegisterCallback(reg, "BML OnCamNavActive", "CamNavActive Hook.", BML_ONCAMNAVACTIVE_GUID,
                     []() { ModLoader::GetInstance().OnCamNavActive(); });
    RegisterCallback(reg, "BML OnCamNavInactive", "CamNavInactive Hook.", BML_ONCAMNAVINACTIVE_GUID,
                     []() { ModLoader::GetInstance().OnCamNavInactive(); });
    RegisterCallback(reg, "BML OnBallOff", "BallOff Hook.", BML_ONBALLOFF_GUID,
                     []() { ModLoader::GetInstance().OnBallOff(); });
    RegisterCallback(reg, "BML OnPreCheckpoint", "PreCheckpoint Hook.", BML_ONPRECHECKPOINT_GUID,
                     []() { ModLoader::GetInstance().OnPreCheckpointReached(); });
    RegisterCallback(reg, "BML OnPostCheckpoint", "PostCheckpoint Hook.", BML_ONPOSTCHECKPOINT_GUID,
                     []() { ModLoader::GetInstance().OnPostCheckpointReached(); });
    RegisterCallback(reg, "BML OnLevelFinish", "LevelFinish Hook.", BML_ONLEVELFINISH_GUID,
                     []() { ModLoader::GetInstance().OnLevelFinish(); });
    RegisterCallback(reg, "BML OnGameOver", "GameOver Hook.", BML_ONGAMEOVER_GUID,
                     []() { ModLoader::GetInstance().OnGameOver(); });
    RegisterCallback(reg, "BML OnExtraPoint", "ExtraPoint Hook.", BML_ONEXTRAPOINT_GUID,
                     []() { ModLoader::GetInstance().OnExtraPoint(); });
    RegisterCallback(reg, "BML OnPreSubLife", "PreSubLife Hook.", BML_ONPRESUBLIFE_GUID,
                     []() { ModLoader::GetInstance().OnPreSubLife(); });
    RegisterCallback(reg, "BML ONPostSubLife", "PostSubLife Hook.", BML_ONPOSTSUBLIFE_GUID,
                     []() { ModLoader::GetInstance().OnPostSubLife(); });
    RegisterCallback(reg, "BML OnPreLifeUp", "PreLifeUp Hook.", BML_ONPRELIFEUP_GUID,
                     []() { ModLoader::GetInstance().OnPreLifeUp(); });
    RegisterCallback(reg, "BML OnPostLifeUp", "PostLifeUp Hook.", BML_ONPOSTLIFEUP_GUID,
                     []() { ModLoader::GetInstance().OnPostLifeUp(); });

    RegisterCallback(reg, "BML ModsMenu", "Show BML Mods Menu.", BML_MODSMENU_GUID,
                     []() { ModLoader::GetInstance().OpenModsMenu(); });
}