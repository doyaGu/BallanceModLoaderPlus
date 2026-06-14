#include "ScriptMod.h"

#include <utility>
#include <vector>

#include "BML/IConfig.h"
#include "ModContext.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

static bool IsConfigKeyValid(const std::string &key) {
    if (key.empty())
        return false;
    for (char ch : key) {
        if (ch <= ' ' || ch == '{' || ch == '}')
            return false;
    }
    return true;
}

static std::string BuildMissingExportDiagnostic(const ScriptModDefinition &definition,
                                                const ScriptExportTable &exports,
                                                const std::string &name,
                                                const std::string &signature) {
    std::string message = "Missing script export owner='";
    message += definition.Id;
    message += "' name='";
    message += name;
    message += "' signature='";
    message += signature.empty() ? "<any>" : signature;
    message += "'";

    const std::string availableSignature = exports.GetSignature(name);
    if (!availableSignature.empty()) {
        message += "; available signature='";
        message += availableSignature;
        message += "'";
    }

    return message;
}

ScriptMod::ScriptMod(ModContext *context,
                     ScriptModDefinition definition,
                     ScriptModEntry entry,
                     ScriptModRuntime runtime)
    : IMod(context),
      m_Context(context),
      m_Definition(std::move(definition)),
      m_Entry(std::move(entry)),
      m_ContextView(context, this),
      m_Runtime(std::move(runtime)) {
    m_Runtime.SetOwner(this);
}

ScriptMod::~ScriptMod() {
    ReleaseRuntime();
}

const char *ScriptMod::GetID() {
    return m_Definition.Id.c_str();
}

const char *ScriptMod::GetVersion() {
    return m_Definition.Version.c_str();
}

const char *ScriptMod::GetName() {
    return m_Definition.Name.c_str();
}

const char *ScriptMod::GetAuthor() {
    return m_Definition.Author.c_str();
}

const char *ScriptMod::GetDescription() {
    return m_Definition.Description.c_str();
}

BMLVersion ScriptMod::GetBMLVersion() {
    return m_Definition.MinBmlVersion;
}

void ScriptMod::OnLoad() {
    if (m_State.IsFailed())
        return;

    if (!m_Definition.Enabled) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod is disabled."));
        return;
    }

    if (!CompileAndCreate())
        return;
    m_EventRouter.Bind(m_Context ? m_Context->GetCKContext() : nullptr, &m_Runtime, &m_ContextView);

    ScriptDiagnostic diagnostic;
    if (!m_EventRouter.Cache(diagnostic)) {
        Fail(diagnostic);
        return;
    }
    if (!m_Exports.Cache(m_Context ? m_Context->GetCKContext() : nullptr, m_Runtime, m_Definition.Exports, diagnostic)) {
        Fail(diagnostic);
        return;
    }

    m_Timers.Bind(m_Context, this, &m_Runtime, &m_ContextView);
    m_Commands.Bind(m_Context, this, &m_ContextView);
    m_DataShareRequests.Bind(m_Context, this, &m_Runtime, &m_ContextView);
    m_State.MarkLoaded(true);
    m_InLoadCallback = true;
    const bool onLoadOk = m_EventRouter.CallOnLoad(diagnostic);
    m_InLoadCallback = false;
    if (!onLoadOk) {
        Fail(diagnostic);
        CleanupFailedLoad();
        return;
    }
    m_Runtime.SetLoaded(true);
}

void ScriptMod::OnUnload() {
    if (m_State.IsLoaded()) {
        ScriptDiagnostic diagnostic;
        if (!m_EventRouter.CallOnUnload(diagnostic)) {
            Record(diagnostic);
            if (m_Context && m_Context->GetLogger())
                m_Context->GetLogger()->Warn("Script mod %s cleanup failed: %s",
                                             GetID(),
                                             m_State.GetLastDiagnosticText().c_str());
        }
    }
    ReleaseRuntime();
    m_State.MarkLoaded(false);
}

void ScriptMod::OnLoadObject(const char *filename,
                             CKBOOL isMap,
                             const char *masterName,
                             CK_CLASSID filterClass,
                             CKBOOL addToScene,
                             CKBOOL reuseMeshes,
                             CKBOOL reuseMaterials,
                             CKBOOL dynamic,
                             XObjectArray *objArray,
                             CKObject *masterObj) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallLoadObject(filename,
                                                       isMap,
                                                       masterName,
                                                       filterClass,
                                                       addToScene,
                                                       reuseMeshes,
                                                       reuseMaterials,
                                                       dynamic,
                                                       objArray,
                                                       masterObj,
                                                       diagnostic),
                          diagnostic);
}

void ScriptMod::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallLoadScript(filename, script, diagnostic),
                          diagnostic);
}

void ScriptMod::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallModifyConfig(GetID(), category, key, prop, diagnostic), diagnostic);
}

void ScriptMod::OnPreStartMenu() {
    CallGameEvent(ScriptGameEventPreStartMenu);
}

void ScriptMod::OnPostStartMenu() {
    CallGameEvent(ScriptGameEventPostStartMenu);
}

void ScriptMod::OnExitGame() {
    CallGameEvent(ScriptGameEventExitGame);
}

void ScriptMod::OnPreLoadLevel() {
    CallGameEvent(ScriptGameEventPreLoadLevel);
}

void ScriptMod::OnPostLoadLevel() {
    CallGameEvent(ScriptGameEventPostLoadLevel);
}

void ScriptMod::OnStartLevel() {
    CallGameEvent(ScriptGameEventStartLevel);
}

void ScriptMod::OnPreResetLevel() {
    CallGameEvent(ScriptGameEventPreResetLevel);
}

void ScriptMod::OnPostResetLevel() {
    CallGameEvent(ScriptGameEventPostResetLevel);
}

void ScriptMod::OnPauseLevel() {
    CallGameEvent(ScriptGameEventPauseLevel);
}

void ScriptMod::OnUnpauseLevel() {
    CallGameEvent(ScriptGameEventUnpauseLevel);
}

void ScriptMod::OnPreExitLevel() {
    CallGameEvent(ScriptGameEventPreExitLevel);
}

void ScriptMod::OnPostExitLevel() {
    CallGameEvent(ScriptGameEventPostExitLevel);
}

void ScriptMod::OnPreNextLevel() {
    CallGameEvent(ScriptGameEventPreNextLevel);
}

void ScriptMod::OnPostNextLevel() {
    CallGameEvent(ScriptGameEventPostNextLevel);
}

void ScriptMod::OnDead() {
    CallGameEvent(ScriptGameEventDead);
}

void ScriptMod::OnPreEndLevel() {
    CallGameEvent(ScriptGameEventPreEndLevel);
}

void ScriptMod::OnPostEndLevel() {
    CallGameEvent(ScriptGameEventPostEndLevel);
}

void ScriptMod::OnCounterActive() {
    CallGameEvent(ScriptGameEventCounterActive);
}

void ScriptMod::OnCounterInactive() {
    CallGameEvent(ScriptGameEventCounterInactive);
}

void ScriptMod::OnBallNavActive() {
    CallGameEvent(ScriptGameEventBallNavActive);
}

void ScriptMod::OnBallNavInactive() {
    CallGameEvent(ScriptGameEventBallNavInactive);
}

void ScriptMod::OnCamNavActive() {
    CallGameEvent(ScriptGameEventCamNavActive);
}

void ScriptMod::OnCamNavInactive() {
    CallGameEvent(ScriptGameEventCamNavInactive);
}

void ScriptMod::OnBallOff() {
    CallGameEvent(ScriptGameEventBallOff);
}

void ScriptMod::OnPreCheckpointReached() {
    CallGameEvent(ScriptGameEventPreCheckpointReached);
}

void ScriptMod::OnPostCheckpointReached() {
    CallGameEvent(ScriptGameEventPostCheckpointReached);
}

void ScriptMod::OnLevelFinish() {
    CallGameEvent(ScriptGameEventLevelFinish);
}

void ScriptMod::OnGameOver() {
    CallGameEvent(ScriptGameEventGameOver);
}

void ScriptMod::OnExtraPoint() {
    CallGameEvent(ScriptGameEventExtraPoint);
}

void ScriptMod::OnPreSubLife() {
    CallGameEvent(ScriptGameEventPreSubLife);
}

void ScriptMod::OnPostSubLife() {
    CallGameEvent(ScriptGameEventPostSubLife);
}

void ScriptMod::OnPreLifeUp() {
    CallGameEvent(ScriptGameEventPreLifeUp);
}

void ScriptMod::OnPostLifeUp() {
    CallGameEvent(ScriptGameEventPostLifeUp);
}

void ScriptMod::OnProcess() {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;
    if (!m_EventRouter.HasOnProcess())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallOnProcess(diagnostic), diagnostic);
}

void ScriptMod::OnRender(CK_RENDER_FLAGS flags) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;
    if (!m_EventRouter.HasOnRender())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallRender(flags, diagnostic), diagnostic);
}

void ScriptMod::OnCheatEnabled(bool enable) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallCheatEnabled(enable, diagnostic), diagnostic);
}

void ScriptMod::OnPhysicalize(CK3dEntity *target,
                              CKBOOL fixed,
                              float friction,
                              float elasticity,
                              float mass,
                              const char *collGroup,
                              CKBOOL startFrozen,
                              CKBOOL enableColl,
                              CKBOOL calcMassCenter,
                              float linearDamp,
                              float rotDamp,
                              const char *collSurface,
                              VxVector massCenter,
                              int convexCnt,
                              CKMesh **convexMesh,
                              int ballCnt,
                              VxVector *ballCenter,
                              float *ballRadius,
                              int concaveCnt,
                              CKMesh **concaveMesh) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallPhysicalize(target,
                                                        fixed,
                                                        friction,
                                                        elasticity,
                                                        mass,
                                                        collGroup,
                                                        startFrozen,
                                                        enableColl,
                                                        calcMassCenter,
                                                        linearDamp,
                                                        rotDamp,
                                                        collSurface,
                                                        massCenter,
                                                        convexCnt,
                                                        convexMesh,
                                                        ballCnt,
                                                        ballCenter,
                                                        ballRadius,
                                                        concaveCnt,
                                                        concaveMesh,
                                                        diagnostic),
                          diagnostic);
}

void ScriptMod::OnUnphysicalize(CK3dEntity *target) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallUnphysicalize(target, diagnostic), diagnostic);
}

void ScriptMod::OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallCommandEvent(true, command, args, diagnostic), diagnostic);
}

void ScriptMod::OnPostCommandExecute(ICommand *command, const std::vector<std::string> &args) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallCommandEvent(false, command, args, diagnostic), diagnostic);
}

void ScriptMod::SetLoadFailure(const std::string &diagnostic) {
    Fail(diagnostic);
}

void ScriptMod::SetLoadFailure(const ScriptDiagnostic &diagnostic) {
    Fail(diagnostic);
}

void ScriptMod::RecordScriptDiagnostic(const ScriptDiagnostic &diagnostic) {
    Record(diagnostic);
}

std::string ScriptMod::GetRootDirectoryUtf8() const {
    return utils::Utf16ToUtf8(m_Entry.RootDirectory);
}

std::string ScriptMod::ResolveResourcePathUtf8(const std::string &relativePath) const {
    if (!utils::IsRelativePathUtf8(relativePath))
        return {};

    std::wstring relative = utils::Utf8ToUtf16(relativePath);
    for (wchar_t &ch : relative) {
        if (ch == L'/')
            ch = L'\\';
    }

    const std::wstring resourceRoot = m_Entry.ResourceRootDirectory.empty()
                                          ? m_Entry.RootDirectory
                                          : m_Entry.ResourceRootDirectory;
    const std::wstring path = utils::CombinePathW(resourceRoot, relative);
    if (!utils::IsPathInsideRootW(path, resourceRoot))
        return {};

    return utils::Utf16ToUtf8(utils::ResolvePathW(path));
}

bool ScriptMod::ModFileExistsUtf8(const std::string &relativePath) const {
    const std::string path = ResolveResourcePathUtf8(relativePath);
    return !path.empty() && BML_FileExistsUtf8(path.c_str()) != 0;
}

bool ScriptMod::ModDirectoryExistsUtf8(const std::string &relativePath) const {
    const std::string path = ResolveResourcePathUtf8(relativePath);
    return !path.empty() && BML_DirectoryExistsUtf8(path.c_str()) != 0;
}

std::string ScriptMod::ReadModTextFileUtf8(const std::string &relativePath,
                                           const std::string &defaultValue) const {
    const std::string path = ResolveResourcePathUtf8(relativePath);
    if (path.empty())
        return defaultValue;

    std::string text;
    if (!utils::ReadFileBytesUtf8(path, text))
        return defaultValue;
    return text;
}

void ScriptMod::LogInfo(const std::string &message) {
    if (ILogger *logger = GetLogger())
        logger->Info("%s", message.c_str());
}

void ScriptMod::LogWarn(const std::string &message) {
    if (ILogger *logger = GetLogger())
        logger->Warn("%s", message.c_str());
}

void ScriptMod::LogError(const std::string &message) {
    if (ILogger *logger = GetLogger())
        logger->Error("%s", message.c_str());
}

bool ScriptMod::HasConfigCategory(const std::string &category) {
    if (!IsConfigKeyValid(category))
        return false;
    IConfig *config = GetConfig();
    return config && config->HasCategory(category.c_str());
}

bool ScriptMod::HasConfigKey(const std::string &category, const std::string &key) {
    if (!IsConfigKeyValid(category) || !IsConfigKeyValid(key))
        return false;
    IConfig *config = GetConfig();
    return config && config->HasKey(category.c_str(), key.c_str());
}

IProperty *ScriptMod::GetConfigProperty(const std::string &category, const std::string &key) {
    if (!IsConfigKeyValid(category) || !IsConfigKeyValid(key))
        return nullptr;
    IConfig *config = GetConfig();
    return config ? config->GetProperty(category.c_str(), key.c_str()) : nullptr;
}

void ScriptMod::SetConfigCategoryComment(const std::string &category, const std::string &comment) {
    if (!IsConfigKeyValid(category))
        return;
    IConfig *config = GetConfig();
    if (config)
        config->SetCategoryComment(category.c_str(), comment.c_str());
}

bool ScriptMod::CompileAndCreate() {
    if (m_Runtime.HasObject())
        return true;

    ScriptDiagnostic diagnostic;
    if (!m_Runtime.CreateObject(m_Context ? m_Context->GetCKContext() : nullptr,
                                m_Definition.ClassNamespace,
                                m_Definition.ClassName,
                                diagnostic)) {
        Fail(diagnostic);
        return false;
    }

    return true;
}
bool ScriptMod::HasExport(const std::string &name, const std::string &signature) const {
    return m_Exports.HasExport(name, signature);
}

ScriptTimerRef *ScriptMod::AddScriptTimer(asIScriptObject *timer) {
    return m_Timers.Add(timer);
}

ScriptCommandRef *ScriptMod::RegisterScriptCommand(asIScriptObject *command) {
    return m_Commands.Register(command);
}

bool ScriptMod::UnregisterScriptCommand(const std::string &name) {
    return m_Commands.Unregister(name);
}

ScriptDataShareRequestRef *ScriptMod::RequestScriptDataShare(asIScriptObject *request) {
    return m_DataShareRequests.Request(request);
}

bool ScriptMod::RegisterScriptBallType(const std::string &ballFile,
                                       const std::string &ballId,
                                       const std::string &ballName,
                                       const std::string &objName,
                                       float friction,
                                       float elasticity,
                                       float mass,
                                       const std::string &collGroup,
                                       float linearDamp,
                                       float rotDamp,
                                       float force,
                                       float radius) {
    if (!m_InLoadCallback || !m_Context || ballFile.empty() || ballId.empty() || ballName.empty() || objName.empty())
        return false;
    m_Context->RegisterBallType(ballFile.c_str(), ballId.c_str(), ballName.c_str(), objName.c_str(), friction,
                                elasticity, mass, collGroup.c_str(), linearDamp, rotDamp, force, radius);
    return true;
}

bool ScriptMod::RegisterScriptFloorType(const std::string &floorName,
                                        float friction,
                                        float elasticity,
                                        float mass,
                                        const std::string &collGroup,
                                        bool enableColl) {
    if (!m_InLoadCallback || !m_Context || floorName.empty())
        return false;
    m_Context->RegisterFloorType(floorName.c_str(), friction, elasticity, mass, collGroup.c_str(), enableColl);
    return true;
}

bool ScriptMod::RegisterScriptModulBall(const std::string &modulName,
                                        bool fixed,
                                        float friction,
                                        float elasticity,
                                        float mass,
                                        const std::string &collGroup,
                                        bool frozen,
                                        bool enableColl,
                                        bool calcMassCenter,
                                        float linearDamp,
                                        float rotDamp,
                                        float radius) {
    if (!m_InLoadCallback || !m_Context || modulName.empty())
        return false;
    m_Context->RegisterModulBall(modulName.c_str(), fixed, friction, elasticity, mass, collGroup.c_str(), frozen,
                                 enableColl, calcMassCenter, linearDamp, rotDamp, radius);
    return true;
}

bool ScriptMod::RegisterScriptModulConvex(const std::string &modulName,
                                          bool fixed,
                                          float friction,
                                          float elasticity,
                                          float mass,
                                          const std::string &collGroup,
                                          bool frozen,
                                          bool enableColl,
                                          bool calcMassCenter,
                                          float linearDamp,
                                          float rotDamp) {
    if (!m_InLoadCallback || !m_Context || modulName.empty())
        return false;
    m_Context->RegisterModulConvex(modulName.c_str(), fixed, friction, elasticity, mass, collGroup.c_str(), frozen,
                                   enableColl, calcMassCenter, linearDamp, rotDamp);
    return true;
}

bool ScriptMod::RegisterScriptTrafo(const std::string &modulName) {
    if (!m_InLoadCallback || !m_Context || modulName.empty())
        return false;
    m_Context->RegisterTrafo(modulName.c_str());
    return true;
}

bool ScriptMod::RegisterScriptModul(const std::string &modulName) {
    if (!m_InLoadCallback || !m_Context || modulName.empty())
        return false;
    m_Context->RegisterModul(modulName.c_str());
    return true;
}

const ScriptExportBinding *ScriptMod::ResolveExport(const std::string &name, const std::string &signature) const {
    return m_Exports.Resolve(name, signature);
}

std::string ScriptMod::GetExportSignature(const std::string &name) const {
    return m_Exports.GetSignature(name);
}

int ScriptMod::GetExportCount() const {
    return m_Exports.GetCount();
}

bool ScriptMod::GetExportInfo(int index, std::string &name, std::string &signature) const {
    return m_Exports.GetInfo(index, name, signature);
}

int ScriptMod::CallVoidExport(const std::string &name, const std::string &signature) {
    const ScriptExportBinding *binding = m_Exports.Resolve(name, signature);
    if (!binding) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                    BuildMissingExportDiagnostic(m_Definition, m_Exports, name, signature)));
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    }
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return BML_ERROR_INTEROP_TARGET_FAILED;

    ScriptDiagnostic diagnostic;
    const int status = ScriptExportDispatcher::CallVoid(m_Context ? m_Context->GetCKContext() : nullptr,
                                                        m_Runtime,
                                                        *binding,
                                                        diagnostic);
    if (status == BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED)
        Fail(diagnostic);
    else if (status != BML_OK)
        Record(diagnostic);
    else
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportCall, "Export call succeeded."));
    return status;
}

int ScriptMod::CallStringExport(const std::string &name,
                                const std::string &signature,
                                const std::string &argument,
                                bool hasArgument,
    std::string &out) {
    const ScriptExportBinding *binding = m_Exports.Resolve(name, signature);
    if (!binding) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                    BuildMissingExportDiagnostic(m_Definition, m_Exports, name, signature)));
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    }
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return BML_ERROR_INTEROP_TARGET_FAILED;

    ScriptDiagnostic diagnostic;
    const int status = ScriptExportDispatcher::CallString(m_Context ? m_Context->GetCKContext() : nullptr,
                                                          m_Runtime,
                                                          *binding,
                                                          argument,
                                                          hasArgument,
                                                          out,
                                                          diagnostic);
    if (status == BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED)
        Fail(diagnostic);
    else if (status != BML_OK)
        Record(diagnostic);
    return status;
}

int ScriptMod::CallExport(const std::string &name, const std::string &signature, BML_CallFrame *frame) {
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;

    const ScriptExportBinding *binding = m_Exports.Resolve(name, signature);
    if (!binding) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                    BuildMissingExportDiagnostic(m_Definition, m_Exports, name, signature)));
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    }
    return CallResolvedExport(binding, frame);
}

int ScriptMod::CallResolvedExport(const ScriptExportBinding *binding, BML_CallFrame *frame) {
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    if (!binding)
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return BML_ERROR_INTEROP_TARGET_FAILED;

    ScriptDiagnostic diagnostic;
    const int status = ScriptExportDispatcher::CallFrame(m_Context ? m_Context->GetCKContext() : nullptr,
                                                         m_Runtime,
                                                         *binding,
                                                         frame,
                                                         diagnostic);
    if (status == BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED)
        Fail(diagnostic);
    else if (status != BML_OK)
        Record(diagnostic);
    return status;
}

void ScriptMod::CallGameEvent(size_t eventIndex) {
    if (!m_State.IsLoaded() || m_State.IsFailed())
        return;
    if (!m_EventRouter.HasGameEvent())
        return;
    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallGameEvent(eventIndex, diagnostic), diagnostic);
}

void ScriptMod::CleanupFailedLoad() {
    const ScriptDiagnostic failureDiagnostic = m_State.GetLastDiagnostic();
    ScriptDiagnostic unloadDiagnostic;
    m_EventRouter.CallOnUnload(unloadDiagnostic);
    ReleaseRuntime();
    m_State.MarkLoaded(false);
    m_State.Fail(failureDiagnostic);
}

void ScriptMod::FailIfEventCallFailed(bool ok, const ScriptDiagnostic &diagnostic) {
    if (!ok)
        Fail(diagnostic);
}

void ScriptMod::Record(const ScriptDiagnostic &diagnostic) {
    m_State.Record(diagnostic);
}

void ScriptMod::Fail(const std::string &diagnostic) {
    Fail(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, diagnostic));
}

void ScriptMod::Fail(const ScriptDiagnostic &diagnostic) {
    m_State.MarkLoaded(false);
    m_State.Fail(diagnostic);
    if (m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Error("Script mod %s failed: %s", GetID(), m_State.GetLastDiagnosticText().c_str());
}

bool ScriptMod::ReleaseRuntime() {
    CKContext *ckContext = m_Context ? m_Context->GetCKContext() : nullptr;
    ScriptDiagnostic releaseDiagnostic;
    bool ok = true;
    m_DataShareRequests.Release(&releaseDiagnostic);
    if (!releaseDiagnostic.Message.empty()) {
        Record(releaseDiagnostic);
        ok = false;
    }
    releaseDiagnostic = ScriptDiagnostic();
    m_Timers.Release(&releaseDiagnostic);
    if (!releaseDiagnostic.Message.empty()) {
        Record(releaseDiagnostic);
        ok = false;
    }
    releaseDiagnostic = ScriptDiagnostic();
    m_Commands.Release(&releaseDiagnostic);
    if (!releaseDiagnostic.Message.empty()) {
        Record(releaseDiagnostic);
        ok = false;
    }
    releaseDiagnostic = ScriptDiagnostic();
    if (!m_Exports.Release(ckContext, m_Runtime, &releaseDiagnostic)) {
        Record(releaseDiagnostic);
        ok = false;
    }
    if (!m_EventRouter.Release(&releaseDiagnostic)) {
        Record(releaseDiagnostic);
        ok = false;
    }

    ScriptDiagnostic diagnostic;
    ok = m_Runtime.Release(ckContext, &diagnostic) && ok;
    m_State.MarkLoaded(false);
    if (!ok) {
        if (!diagnostic.Message.empty())
            Record(diagnostic);
        if (m_Context && m_Context->GetLogger()) {
            m_Context->GetLogger()->Warn("Script mod %s runtime release failed: %s",
                                         GetID(),
                                         m_State.GetLastDiagnosticText().c_str());
        }
    }
    return ok;
}

std::wstring ScriptMod::GetEntryPath() const {
    if (m_Entry.EntryPath.empty())
        return {};
    return utils::ResolvePathW(m_Entry.EntryPath);
}

bool IsFailedScriptMod(const IMod *mod) {
    auto *scriptMod = dynamic_cast<const ScriptMod *>(mod);
    return scriptMod && scriptMod->IsFailed();
}

} // namespace BML
