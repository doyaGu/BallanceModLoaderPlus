#include "ScriptMod.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include "BML/IConfig.h"
#include "ExportRegistry.h"
#include "ModContext.h"
#include "ScriptDevToolsService.h"
#include "ScriptModDefinitionBuilder.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

static constexpr const char *kReloadDependencyBoundary =
    "Hot reload never reorders the dependency graph and never cascades reload into dependent mods; restart or reload dependents explicitly.";

static constexpr const char *kReloadRollbackBoundary =
    "Rollback restores only BML-managed script resources such as callbacks, exports, timers, commands, DataShare requests, and script runtime handles; it cannot undo game-world changes made by script code.";

class ScriptModReloadPhaseScope {
public:
    ScriptModReloadPhaseScope(ScriptMod &mod, ScriptModReloadPhase phase)
        : m_Mod(mod), m_Previous(mod.GetReloadPhase()) {
        m_Mod.SetReloadPhase(phase);
    }

    ~ScriptModReloadPhaseScope() {
        m_Mod.SetReloadPhase(m_Previous);
    }

    ScriptModReloadPhaseScope(const ScriptModReloadPhaseScope &) = delete;
    ScriptModReloadPhaseScope &operator=(const ScriptModReloadPhaseScope &) = delete;

private:
    ScriptMod &m_Mod;
    ScriptModReloadPhase m_Previous;
};

static void AddReloadField(std::vector<ScriptModReloadDiagnosticField> *fields,
                           const std::string &key,
                           const std::string &value) {
    if (fields)
        fields->push_back({key, value});
}

static void AddReloadBoundary(std::vector<ScriptModReloadDiagnosticField> *fields,
                              const char *boundary,
                              const char *action) {
    AddReloadField(fields, "boundary", boundary ? boundary : "");
    AddReloadField(fields, "action", action ? action : "");
}

static bool IsConfigKeyValid(const std::string &key) {
    if (key.empty())
        return false;
    for (char ch : key) {
        if (ch <= ' ' || ch == '{' || ch == '}')
            return false;
    }
    return true;
}

static void AppendHostRegistrationPart(std::ostringstream &stream, const std::string &value) {
    stream << value.size() << ':' << value << ';';
}

static void AppendHostRegistrationPart(std::ostringstream &stream, const char *value) {
    AppendHostRegistrationPart(stream, std::string(value ? value : ""));
}

static void AppendHostRegistrationPart(std::ostringstream &stream, bool value) {
    stream << (value ? "1;" : "0;");
}

static void AppendHostRegistrationPart(std::ostringstream &stream, float value) {
    stream << std::setprecision(9) << value << ';';
}

template<typename... Args>
static std::string MakeHostRegistrationKey(const Args &... args) {
    std::ostringstream stream;
    (AppendHostRegistrationPart(stream, args), ...);
    return stream.str();
}

static std::vector<std::string> CanonicalHostRegistrations(const std::vector<ScriptModHostRegistration> &items) {
    std::vector<std::string> canonical;
    canonical.reserve(items.size());
    for (const auto &item : items)
        canonical.push_back(item.Kind + "\n" + item.Key);
    std::sort(canonical.begin(), canonical.end());
    return canonical;
}

static std::string MakeReloadModuleName(const ScriptModRuntime &runtime) {
    static std::atomic<unsigned long> s_ReloadSerial{1};
    std::string base = runtime.GetModuleName();
    for (;;) {
        const size_t marker = base.rfind(".reload.");
        if (marker == std::string::npos)
            break;

        const size_t suffixStart = marker + 8;
        if (suffixStart == base.size())
            break;

        bool numericSuffix = true;
        for (size_t i = suffixStart; i < base.size(); ++i) {
            if (base[i] < '0' || base[i] > '9') {
                numericSuffix = false;
                break;
            }
        }
        if (!numericSuffix)
            break;
        base.resize(marker);
    }
    return base + ".reload." + std::to_string(s_ReloadSerial.fetch_add(1, std::memory_order_relaxed));
}

static ScriptModLoadCandidate MakeReloadCandidate(const ScriptModEntry &entry);
static bool PrepareFilesystemReloadCandidate(const ScriptModEntry &entry,
                                             ScriptModLoadCandidate &candidate,
                                             std::wstring &stagingRoot,
                                             ScriptDiagnostic &diagnostic);
static bool PrepareZipReloadCandidate(const ScriptModEntry &entry,
                                      ScriptModLoadCandidate &candidate,
                                      std::wstring &stagingRoot,
                                      ScriptDiagnostic &diagnostic);

struct ScriptModReloadSourceSnapshot {
    ScriptModLoadCandidate CompileCandidate;
    ScriptModEntry CompileEntry;
    ScriptModEntry CommitEntry;
    std::string CompileEntryPathUtf8;
    std::string CommitEntryPathUtf8;
    std::wstring StagedRoot;
    std::string DiagnosticStagedRootUtf8;
    std::string DiagnosticDisplayRootUtf8;

    ScriptModReloadSourceSnapshot() = default;
    ScriptModReloadSourceSnapshot(const ScriptModReloadSourceSnapshot &) = delete;
    ScriptModReloadSourceSnapshot &operator=(const ScriptModReloadSourceSnapshot &) = delete;

    ~ScriptModReloadSourceSnapshot() {
        Cleanup();
    }

    void Reset() {
        Cleanup();
        CompileCandidate = ScriptModLoadCandidate();
        CompileEntry = ScriptModEntry();
        CommitEntry = ScriptModEntry();
        CompileEntryPathUtf8.clear();
        CommitEntryPathUtf8.clear();
        DiagnosticStagedRootUtf8.clear();
        DiagnosticDisplayRootUtf8.clear();
    }

    void Cleanup() {
        if (!StagedRoot.empty()) {
            utils::DeleteDirectoryW(StagedRoot);
            StagedRoot.clear();
        }
    }

    void KeepStagedRoot() {
        StagedRoot.clear();
    }
};

static void ReplaceAllText(std::string &value, const std::string &from, const std::string &to) {
    if (from.empty())
        return;

    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

static void ReplacePathPrefixText(std::string &value, const std::string &from, const std::string &to) {
    if (value.empty() || from.empty() || from == to)
        return;

    ReplaceAllText(value, from, to);
    std::string slashFrom = from;
    std::replace(slashFrom.begin(), slashFrom.end(), '\\', '/');
    if (slashFrom != from)
        ReplaceAllText(value, slashFrom, to);
}

static void RewriteSnapshotDiagnosticPaths(const ScriptModReloadSourceSnapshot &snapshot,
                                           ScriptDiagnostic &diagnostic) {
    if (snapshot.DiagnosticStagedRootUtf8.empty() ||
        snapshot.DiagnosticDisplayRootUtf8.empty()) {
        return;
    }

    ReplacePathPrefixText(diagnostic.EntryPath,
                          snapshot.DiagnosticStagedRootUtf8,
                          snapshot.DiagnosticDisplayRootUtf8);
    ReplacePathPrefixText(diagnostic.RawMessage,
                          snapshot.DiagnosticStagedRootUtf8,
                          snapshot.DiagnosticDisplayRootUtf8);
    ReplacePathPrefixText(diagnostic.StackTrace,
                          snapshot.DiagnosticStagedRootUtf8,
                          snapshot.DiagnosticDisplayRootUtf8);
    for (ScriptCompilerMessage &message : diagnostic.CompilerMessages) {
        ReplacePathPrefixText(message.Section,
                              snapshot.DiagnosticStagedRootUtf8,
                              snapshot.DiagnosticDisplayRootUtf8);
    }
}

static ScriptModEntry MakeFilesystemCommitEntry(const ScriptModEntry &currentEntry,
                                                const ScriptModLoadCandidate &compileCandidate,
                                                const ScriptModEntry &compileEntry) {
    ScriptModEntry commitEntry = currentEntry;
    if (currentEntry.SourceKind != ScriptModEntrySourceKind::Directory ||
        currentEntry.RootDirectory.empty() ||
        compileCandidate.RootDirectory.empty() ||
        compileEntry.EntryPath.empty() ||
        !utils::IsPathInsideRootW(compileEntry.EntryPath, compileCandidate.RootDirectory)) {
        return commitEntry;
    }

    const std::wstring relativeEntry = utils::MakeRelativePathW(compileEntry.EntryPath,
                                                               compileCandidate.RootDirectory);
    if (relativeEntry.empty() || relativeEntry == L".")
        return commitEntry;

    commitEntry.EntryPath = utils::CombinePathW(currentEntry.RootDirectory, relativeEntry);
    commitEntry.EntryFilename = utils::Utf16ToUtf8(utils::GetFileNameW(commitEntry.EntryPath));
    return commitEntry;
}

static bool CaptureReloadSourceSnapshot(const ScriptModEntry &entry,
                                        ScriptModReloadSourceSnapshot &snapshot,
                                        ScriptDiagnostic &diagnostic) {
    snapshot.Reset();

    ScriptModLoadCandidate candidate;
    std::wstring stagedRoot;
    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage) {
        if (!PrepareZipReloadCandidate(entry, candidate, stagedRoot, diagnostic))
            return false;
    } else {
        if (!PrepareFilesystemReloadCandidate(entry, candidate, stagedRoot, diagnostic))
            return false;
    }

    snapshot.CompileCandidate = std::move(candidate);
    snapshot.StagedRoot = std::move(stagedRoot);
    snapshot.DiagnosticStagedRootUtf8 = utils::Utf16ToUtf8(snapshot.StagedRoot);
    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage) {
        snapshot.DiagnosticDisplayRootUtf8 = utils::Utf16ToUtf8(entry.SourcePath.empty()
                                                                    ? entry.RootDirectory
                                                                    : entry.SourcePath);
    } else {
        snapshot.DiagnosticDisplayRootUtf8 = utils::Utf16ToUtf8(entry.SourceKind == ScriptModEntrySourceKind::SingleFile
                                                                    ? utils::GetDirectoryW(entry.EntryPath)
                                                                    : entry.RootDirectory);
    }

    if (snapshot.CompileCandidate.EntryPaths.size() != 1) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Reload requires exactly one *.mod.as entry file.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(snapshot.CompileCandidate.RootDirectory.empty()
                                                      ? snapshot.CompileCandidate.SourcePath
                                                      : snapshot.CompileCandidate.RootDirectory);
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        return false;
    }

    snapshot.CompileEntry = MakeScriptModEntry(snapshot.CompileCandidate,
                                               snapshot.CompileCandidate.EntryPaths.front());
    snapshot.CompileEntryPathUtf8 = utils::Utf16ToUtf8(snapshot.CompileEntry.EntryPath);

    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage) {
        snapshot.CommitEntry = snapshot.CompileEntry;
    } else {
        snapshot.CommitEntry = MakeFilesystemCommitEntry(entry,
                                                         snapshot.CompileCandidate,
                                                         snapshot.CompileEntry);
    }
    snapshot.CommitEntryPathUtf8 = utils::Utf16ToUtf8(snapshot.CommitEntry.EntryPath);

    return true;
}

class ScriptModCallScope {
public:
    explicit ScriptModCallScope(const ScriptMod *mod) : m_Mod(mod) {
        if (m_Mod)
            m_Entered = m_Mod->EnterScriptCall();
    }

    ~ScriptModCallScope() {
        if (m_Mod && m_Entered)
            m_Mod->LeaveScriptCall();
    }

    bool Entered() const { return m_Entered; }

    ScriptModCallScope(const ScriptModCallScope &) = delete;
    ScriptModCallScope &operator=(const ScriptModCallScope &) = delete;

private:
    const ScriptMod *m_Mod = nullptr;
    bool m_Entered = false;
};

static bool CopyDirectorySnapshotContentsW(const std::wstring &source,
                                           const std::wstring &dest) {
    if (source.empty() || dest.empty())
        return false;

    std::error_code ec;
    const std::filesystem::path sourcePath(source);
    const std::filesystem::path destPath(dest);

    std::filesystem::create_directories(destPath, ec);
    if (ec)
        return false;

    for (std::filesystem::recursive_directory_iterator it(sourcePath, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        const std::filesystem::path relative = it->path().lexically_relative(sourcePath);
        const std::filesystem::path target = destPath / relative;
        if (it->is_directory(ec)) {
            if (ec)
                return false;
            std::filesystem::create_directories(target, ec);
            if (ec)
                return false;
            continue;
        }
        if (!it->is_regular_file(ec)) {
            if (ec)
                return false;
            continue;
        }
        std::filesystem::create_directories(target.parent_path(), ec);
        if (ec)
            return false;
        std::filesystem::copy_file(it->path(),
                                   target,
                                   std::filesystem::copy_options::overwrite_existing,
                                   ec);
        if (ec)
            return false;
    }

    return !ec;
}

static std::wstring MakeReloadSnapshotRoot(const ScriptModEntry &entry) {
    static std::atomic<unsigned long> s_ReloadSnapshotSerial{1};
    const std::wstring tempRoot = utils::CombinePathW(utils::GetTempPathW(), L"BMLScriptReload");

    std::wstring baseName;
    if (!entry.EntryPath.empty()) {
        baseName = utils::RemoveExtensionW(utils::RemoveExtensionW(utils::GetFileNameW(entry.EntryPath)));
    }
    if (baseName.empty() && !entry.RootDirectory.empty())
        baseName = utils::GetFileNameW(entry.RootDirectory);
    if (baseName.empty())
        baseName = L"script-mod";

    for (int attempt = 0; attempt < 128; ++attempt) {
        const unsigned long serial = s_ReloadSnapshotSerial.fetch_add(1, std::memory_order_relaxed);
        std::wstring candidate = utils::CombinePathW(tempRoot, baseName + L".snapshot." + std::to_wstring(serial));
        if (!utils::PathExistsW(candidate))
            return candidate;
    }

    return {};
}

static std::wstring MakeZipReloadStagingRoot(const ScriptModEntry &entry) {
    static std::atomic<unsigned long> s_ZipReloadSerial{1};
    const std::wstring parent = utils::GetDirectoryW(entry.RootDirectory.empty() ? entry.SourcePath : entry.RootDirectory);
    std::wstring baseName = utils::RemoveExtensionW(utils::GetFileNameW(entry.SourcePath));
    if (baseName.empty())
        baseName = L"script-package";

    for (int attempt = 0; attempt < 128; ++attempt) {
        const unsigned long serial = s_ZipReloadSerial.fetch_add(1, std::memory_order_relaxed);
        std::wstring candidate = utils::CombinePathW(parent, baseName + L".reload." + std::to_wstring(serial));
        if (!utils::PathExistsW(candidate))
            return candidate;
    }

    return {};
}

static bool PrepareFilesystemReloadCandidate(const ScriptModEntry &entry,
                                             ScriptModLoadCandidate &candidate,
                                             std::wstring &stagingRoot,
                                             ScriptDiagnostic &diagnostic) {
    candidate = ScriptModLoadCandidate();
    stagingRoot.clear();

    if (entry.EntryPath.empty() || !utils::FileExistsW(entry.EntryPath)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Script entry file no longer exists.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.EntryPath);
        return false;
    }

    stagingRoot = MakeReloadSnapshotRoot(entry);
    if (stagingRoot.empty()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to allocate script reload snapshot directory.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.EntryPath);
        return false;
    }
    if (utils::PathExistsW(stagingRoot) && !utils::DeleteDirectoryW(stagingRoot)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to clear script reload snapshot directory.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(stagingRoot);
        stagingRoot.clear();
        return false;
    }
    if (!utils::CreateFileTreeW(stagingRoot)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to create script reload snapshot directory.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(stagingRoot);
        stagingRoot.clear();
        return false;
    }

    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile) {
        const std::wstring stagedEntry = utils::CombinePathW(stagingRoot, utils::GetFileNameW(entry.EntryPath));
        if (!utils::CopyFileW(entry.EntryPath, stagedEntry)) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to copy script entry into reload snapshot.");
            diagnostic.EntryPath = utils::Utf16ToUtf8(entry.EntryPath);
            utils::DeleteDirectoryW(stagingRoot);
            stagingRoot.clear();
            return false;
        }

        const std::wstring resourceRootName = utils::GetFileNameW(entry.ResourceRootDirectory);
        const std::wstring stagedResourceRoot = resourceRootName.empty()
                                                    ? stagingRoot
                                                    : utils::CombinePathW(stagingRoot, resourceRootName);
        if (!entry.ResourceRootDirectory.empty() &&
            utils::DirectoryExistsW(entry.ResourceRootDirectory) &&
            !CopyDirectorySnapshotContentsW(entry.ResourceRootDirectory, stagedResourceRoot)) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to copy script resource directory into reload snapshot.");
            diagnostic.EntryPath = utils::Utf16ToUtf8(entry.ResourceRootDirectory);
            utils::DeleteDirectoryW(stagingRoot);
            stagingRoot.clear();
            return false;
        }

        candidate.SourceKind = ScriptModEntrySourceKind::SingleFile;
        candidate.SourcePath = entry.SourcePath;
        candidate.RootDirectory = stagedResourceRoot;
        candidate.ResourceRootDirectory = stagedResourceRoot;
        candidate.EntryPaths.push_back(stagedEntry);
        candidate.SyntheticId = entry.SyntheticId;
        return true;
    }

    if (entry.RootDirectory.empty() || !utils::DirectoryExistsW(entry.RootDirectory)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Script mod directory no longer exists.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.RootDirectory);
        utils::DeleteDirectoryW(stagingRoot);
        stagingRoot.clear();
        return false;
    }

    if (!CopyDirectorySnapshotContentsW(entry.RootDirectory, stagingRoot)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to copy script mod directory into reload snapshot.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.RootDirectory);
        utils::DeleteDirectoryW(stagingRoot);
        stagingRoot.clear();
        return false;
    }

    candidate = MakeDirectoryScriptModCandidate(stagingRoot, entry.SourceKind, entry.SourcePath);
    candidate.SyntheticId = entry.SyntheticId;
    return true;
}

static ScriptModLoadCandidate MakeZipReloadCandidate(const ScriptModEntry &entry,
                                                     const std::wstring &stagingRoot) {
    std::vector<ScriptModLoadCandidate> packageCandidates;

    std::vector<std::wstring> rootEntries = FindScriptModEntryPaths(stagingRoot);
    if (!rootEntries.empty()) {
        ScriptModLoadCandidate rootCandidate =
            MakeDirectoryScriptModCandidate(stagingRoot, ScriptModEntrySourceKind::ZipPackage, entry.SourcePath);
        rootCandidate.EntryPaths = std::move(rootEntries);
        rootCandidate.SyntheticId = entry.SyntheticId;
        packageCandidates.push_back(std::move(rootCandidate));
    }

    std::vector<ScriptModLoadCandidate> nestedCandidates;
    FindScriptModCandidates(stagingRoot, nestedCandidates);
    for (auto &nestedCandidate : nestedCandidates) {
        if (nestedCandidate.SourceKind != ScriptModEntrySourceKind::Directory)
            continue;
        nestedCandidate.SourceKind = ScriptModEntrySourceKind::ZipPackage;
        nestedCandidate.SourcePath = entry.SourcePath;
        nestedCandidate.SyntheticId = entry.SyntheticId;
        packageCandidates.push_back(std::move(nestedCandidate));
    }

    if (packageCandidates.size() == 1 && packageCandidates.front().EntryPaths.size() == 1)
        return std::move(packageCandidates.front());

    ScriptModLoadCandidate aggregate =
        MakeDirectoryScriptModCandidate(stagingRoot, ScriptModEntrySourceKind::ZipPackage, entry.SourcePath);
    aggregate.EntryPaths.clear();
    aggregate.SyntheticId = entry.SyntheticId;
    for (const auto &packageCandidate : packageCandidates) {
        aggregate.EntryPaths.insert(aggregate.EntryPaths.end(),
                                    packageCandidate.EntryPaths.begin(),
                                    packageCandidate.EntryPaths.end());
    }
    return aggregate;
}

static bool PrepareZipReloadCandidate(const ScriptModEntry &entry,
                                      ScriptModLoadCandidate &candidate,
                                      std::wstring &stagingRoot,
                                      ScriptDiagnostic &diagnostic) {
    candidate = ScriptModLoadCandidate();
    stagingRoot.clear();
    if (entry.SourceKind != ScriptModEntrySourceKind::ZipPackage) {
        candidate = MakeReloadCandidate(entry);
        return true;
    }

    if (entry.SourcePath.empty() || entry.RootDirectory.empty()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Zip script mod reload is missing source or root path.");
        return false;
    }
    if (!utils::FileExistsW(entry.SourcePath)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Zip script package no longer exists.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.SourcePath);
        return false;
    }

    stagingRoot = MakeZipReloadStagingRoot(entry);
    if (stagingRoot.empty()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to allocate script package reload staging directory.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.SourcePath);
        return false;
    }
    if (utils::PathExistsW(stagingRoot) && !utils::DeleteDirectoryW(stagingRoot)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to clear script package reload staging directory.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(stagingRoot);
        stagingRoot.clear();
        return false;
    }
    if (!utils::CreateFileTreeW(stagingRoot) || !utils::ExtractZipW(entry.SourcePath, stagingRoot)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Failed to extract script package for reload.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.SourcePath);
        if (!stagingRoot.empty())
            utils::DeleteDirectoryW(stagingRoot);
        stagingRoot.clear();
        return false;
    }

    candidate = MakeZipReloadCandidate(entry, stagingRoot);
    return true;
}

static ScriptModLoadCandidate MakeReloadCandidate(const ScriptModEntry &entry) {
    ScriptModLoadCandidate candidate;
    candidate.SourceKind = entry.SourceKind;
    candidate.SourcePath = entry.SourcePath;
    candidate.RootDirectory = entry.RootDirectory;
    candidate.ResourceRootDirectory = entry.ResourceRootDirectory;
    candidate.SyntheticId = entry.SyntheticId;
    if (entry.SourceKind == ScriptModEntrySourceKind::Directory) {
        candidate.EntryPaths = FindScriptModEntryPaths(entry.RootDirectory);
    }
    if (candidate.EntryPaths.empty() && !entry.EntryPath.empty())
        candidate.EntryPaths.push_back(entry.EntryPath);
    return candidate;
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
    LoadCurrentRuntime(false);
}

bool ScriptMod::LoadCurrentRuntime(bool validateHostRegistrations, bool failedLoadRecovery) {
    if (m_State.IsFailed())
        return false;

    if (!m_Definition.Enabled) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod is disabled."));
        return false;
    }

    auto cleanupFailedPrepare = [&]() {
        const ScriptDiagnostic failure = m_State.GetLastDiagnostic();
        ReleaseRuntime();
        if (!failure.Message.empty() ||
            !failure.RawMessage.empty() ||
            !failure.StackTrace.empty() ||
            !failure.CompilerMessages.empty() ||
            failure.Status != CKAS_OK) {
            m_State.Fail(failure);
            TouchModGeneration();
        }
    };

    if (!CompileAndCreate()) {
        cleanupFailedPrepare();
        return false;
    }
    m_EventRouter.Bind(m_Context ? m_Context->GetCKContext() : nullptr, &m_Runtime, &m_ContextView);

    ScriptDiagnostic diagnostic;
    if (!m_EventRouter.Cache(diagnostic)) {
        Fail(diagnostic);
        cleanupFailedPrepare();
        return false;
    }
    if (!m_Exports.Cache(m_Context ? m_Context->GetCKContext() : nullptr, m_Runtime, m_Definition.Exports, diagnostic)) {
        Fail(diagnostic);
        cleanupFailedPrepare();
        return false;
    }

    RebindServices();
    m_State.MarkLoaded(true);
    const HostRegistrationMode previousMode = m_HostRegistrationMode;
    m_HostRegistrationMode = validateHostRegistrations ? HostRegistrationMode::Validate : HostRegistrationMode::Capture;
    m_PendingHostRegistrations.clear();
    m_InLoadCallback = true;
    const bool onLoadOk = m_EventRouter.CallOnLoad(diagnostic);
    m_InLoadCallback = false;
    m_HostRegistrationMode = previousMode;
    if (!onLoadOk || m_State.IsFailed()) {
        if (!onLoadOk)
            Fail(diagnostic);
        CleanupFailedLoad();
        return false;
    }
    if (validateHostRegistrations) {
        std::string registrationDiagnostic;
        if (!ValidateHostRegistrationSet(registrationDiagnostic, failedLoadRecovery)) {
            Fail(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, registrationDiagnostic));
            CleanupFailedLoad();
            return false;
        }
    }
    m_Runtime.SetLoaded(true);
    m_PendingFailureCleanup.store(false, std::memory_order_release);
    TouchRuntimeGeneration();
    TouchModGeneration();
    return true;
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
    TouchModGeneration();
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
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
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
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallLoadScript(filename, script, diagnostic),
                          diagnostic);
}

void ScriptMod::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;

    const std::string eventCategory = category ? category : "";
    const std::string eventKey = key ? key : "";
    for (const auto &activeEvent : m_ActiveConfigEvents) {
        if (activeEvent.first == eventCategory && activeEvent.second == eventKey)
            return;
    }

    struct ActiveConfigEventGuard {
        explicit ActiveConfigEventGuard(std::vector<std::pair<std::string, std::string>> &events,
                                        std::string category,
                                        std::string key)
            : Events(events) {
            Events.emplace_back(std::move(category), std::move(key));
        }

        ~ActiveConfigEventGuard() {
            Events.pop_back();
        }

        std::vector<std::pair<std::string, std::string>> &Events;
    };

    ActiveConfigEventGuard activeEvent(m_ActiveConfigEvents, eventCategory, eventKey);
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
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;
    if (!m_EventRouter.HasOnProcess())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallOnProcess(diagnostic), diagnostic);
}

void ScriptMod::OnRender(CK_RENDER_FLAGS flags) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;
    if (!m_EventRouter.HasOnRender())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallRender(flags, diagnostic), diagnostic);
}

void ScriptMod::OnCheatEnabled(bool enable) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
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
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
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
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallUnphysicalize(target, diagnostic), diagnostic);
}

void ScriptMod::OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallCommandEvent(true, command, args, diagnostic), diagnostic);
}

void ScriptMod::OnPostCommandExecute(ICommand *command, const std::vector<std::string> &args) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;

    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallCommandEvent(false, command, args, diagnostic), diagnostic);
}

void ScriptMod::SetLoadFailure(const std::string &diagnostic) {
    SetLoadFailure(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, diagnostic));
}

void ScriptMod::SetLoadFailure(const ScriptDiagnostic &diagnostic) {
    FailIfEventCallFailed(false, diagnostic);
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

bool ScriptMod::CanHotReloadNow() const {
    std::lock_guard<std::mutex> lock(m_ReloadMutex);
    return !m_Reloading.load(std::memory_order_acquire) &&
           m_ActiveScriptCalls == 0 &&
           GetQueuedScriptServiceCallbackCount() == 0;
}

bool ScriptMod::EnterScriptCall() const {
    std::lock_guard<std::mutex> lock(m_ReloadMutex);
    if (m_Reloading.load(std::memory_order_acquire) &&
        m_ReloadThreadId != std::this_thread::get_id()) {
        return false;
    }
    ++m_ActiveScriptCalls;
    return true;
}

void ScriptMod::LeaveScriptCall() const {
    std::lock_guard<std::mutex> lock(m_ReloadMutex);
    if (m_ActiveScriptCalls > 0)
        --m_ActiveScriptCalls;
}

int ScriptMod::GetActiveScriptCallCount() const {
    std::lock_guard<std::mutex> lock(m_ReloadMutex);
    return m_ActiveScriptCalls;
}

size_t ScriptMod::GetQueuedScriptServiceCallbackCount() const {
    return m_DataShareRequests.GetQueuedCallbackCount();
}

void ScriptMod::RebindServices() {
    m_Timers.Bind(m_Context, this, &m_Runtime, &m_ContextView);
    m_Commands.Bind(m_Context, this, &m_ContextView);
    m_DataShareRequests.Bind(m_Context, this, &m_Runtime, &m_ContextView);
    m_HookBlocks.Bind(m_Context, this, &m_ContextView);
}

bool ScriptMod::CanDispatchScriptCallback() {
    if (!m_State.IsLoaded() || m_State.IsFailed() || m_Reloading.load(std::memory_order_acquire))
        return false;

    if (!m_CallbackFenceActive.load(std::memory_order_acquire))
        return true;

    CKTimeManager *time = m_Context ? m_Context->GetTimeManager() : nullptr;
    if (!time)
        return false;

    const unsigned int currentFrame = static_cast<unsigned int>(time->GetMainTickCount());
    const unsigned int fencedFrame = m_CallbackFenceFrame.load(std::memory_order_acquire);
    if (currentFrame == fencedFrame)
        return false;

    m_CallbackFenceActive.store(false, std::memory_order_release);
    return true;
}

bool ScriptMod::CanDispatchScriptServiceCallback() {
    return CanDispatchScriptCallback();
}

void ScriptMod::FenceCallbacksForCurrentFrame() {
    CKTimeManager *time = m_Context ? m_Context->GetTimeManager() : nullptr;
    if (!time)
        return;
    m_CallbackFenceFrame.store(static_cast<unsigned int>(time->GetMainTickCount()), std::memory_order_release);
    m_CallbackFenceActive.store(true, std::memory_order_release);
}

void ScriptMod::SetReloadDiagnostic(const std::string &diagnostic) {
    m_LastReloadDiagnostic = diagnostic;
    m_LastReloadDiagnosticInfo = diagnostic.empty()
                                     ? ScriptDiagnostic()
                                     : MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, diagnostic);
    TouchModGeneration();
    if (!diagnostic.empty() && m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Warn("Script mod %s reload: %s", GetID(), diagnostic.c_str());
}

void ScriptMod::SetReloadDiagnostic(const ScriptDiagnostic &diagnostic) {
    m_LastReloadDiagnosticInfo = RewriteRuntimeDiagnosticPaths(diagnostic);
    m_LastReloadDiagnostic = FormatScriptDiagnostic(m_LastReloadDiagnosticInfo);
    TouchModGeneration();
    if (!m_LastReloadDiagnostic.empty() && m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Warn("Script mod %s reload: %s", GetID(), m_LastReloadDiagnostic.c_str());
}

ScriptDiagnostic ScriptMod::RewriteRuntimeDiagnosticPaths(ScriptDiagnostic diagnostic) const {
    if (m_RuntimeDiagnosticPathFrom.empty() || m_RuntimeDiagnosticPathTo.empty())
        return diagnostic;

    ReplacePathPrefixText(diagnostic.EntryPath, m_RuntimeDiagnosticPathFrom, m_RuntimeDiagnosticPathTo);
    ReplacePathPrefixText(diagnostic.RawMessage, m_RuntimeDiagnosticPathFrom, m_RuntimeDiagnosticPathTo);
    ReplacePathPrefixText(diagnostic.StackTrace, m_RuntimeDiagnosticPathFrom, m_RuntimeDiagnosticPathTo);
    for (ScriptCompilerMessage &message : diagnostic.CompilerMessages)
        ReplacePathPrefixText(message.Section, m_RuntimeDiagnosticPathFrom, m_RuntimeDiagnosticPathTo);
    return diagnostic;
}

void ScriptMod::SetRuntimeDiagnosticPathRewrite(std::string from, std::string to) {
    m_RuntimeDiagnosticPathFrom = std::move(from);
    m_RuntimeDiagnosticPathTo = std::move(to);
}

void ScriptMod::TouchModGeneration() {
    m_ModGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void ScriptMod::TouchRuntimeGeneration() {
    m_RuntimeGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void ScriptMod::TouchReloadAttempt() {
    m_ReloadAttemptId.fetch_add(1, std::memory_order_acq_rel);
    TouchModGeneration();
}

void ScriptMod::SetReloadPhase(ScriptModReloadPhase phase) {
    const int oldPhase = m_ReloadPhase.exchange(static_cast<int>(phase), std::memory_order_acq_rel);
    if (oldPhase != static_cast<int>(phase))
        TouchModGeneration();
}

bool ScriptMod::ValidateHostRegistrationSet(std::string &diagnostic, bool failedLoadRecovery) {
    const auto expected = CanonicalHostRegistrations(m_HostRegistrations);
    const auto actual = CanonicalHostRegistrations(m_PendingHostRegistrations);
    if (expected == actual)
        return true;

    if (failedLoadRecovery && expected.empty() && !actual.empty()) {
        diagnostic = "Script mod failed-load recovery introduced ball/floor/module registrations. "
                     "Hot reload cannot create irreversible host registrations for a script mod that never loaded; "
                     "restart is required.";
        return false;
    }

    diagnostic = "Script mod changed ball/floor/module registrations; restart is required.";
    return false;
}

bool ScriptMod::NoteHostRegistration(const char *kind, const std::string &key) {
    ScriptModHostRegistration registration{kind ? kind : "", key};
    if (m_HostRegistrationMode == HostRegistrationMode::Validate) {
        m_PendingHostRegistrations.push_back(std::move(registration));
        return true;
    }
    m_HostRegistrations.push_back(std::move(registration));
    return true;
}

bool ScriptMod::ValidateReloadDefinition(const ScriptModDefinition &candidate,
                                         const ScriptExportTable &candidateExports,
                                         const ScriptModReloadOptions &options,
                                         std::string &diagnostic,
                                         std::vector<ScriptModReloadDiagnosticField> *fields) const {
    const bool recoveringPlaceholder = IsFailedPlaceholder();
    if (candidate.Id != m_Definition.Id) {
        if (!recoveringPlaceholder) {
            diagnostic = "Script mod id changed from '" + m_Definition.Id + "' to '" + candidate.Id + "'.";
            AddReloadBoundary(fields, "mod_identity", "restart_required");
            AddReloadField(fields, "oldId", m_Definition.Id);
            AddReloadField(fields, "newId", candidate.Id);
            return false;
        }
    }
    if (!candidate.Enabled) {
        diagnostic = "Script mod reload candidate is disabled.";
        AddReloadBoundary(fields, "mod_enabled", "enable_candidate_or_restart");
        return false;
    }

    auto dependencyKey = [](const ScriptModDependency &dependency) {
        return dependency.Id + "\n" +
               std::to_string(dependency.MinVersion.major) + "." +
               std::to_string(dependency.MinVersion.minor) + "." +
               std::to_string(dependency.MinVersion.patch) + "\n" +
               (dependency.Optional ? "optional" : "required");
    };
    std::vector<std::string> oldDependencies;
    std::vector<std::string> newDependencies;
    oldDependencies.reserve(m_Definition.Dependencies.size());
    newDependencies.reserve(candidate.Dependencies.size());
    for (const auto &dependency : m_Definition.Dependencies)
        oldDependencies.push_back(dependencyKey(dependency));
    for (const auto &dependency : candidate.Dependencies)
        newDependencies.push_back(dependencyKey(dependency));
    std::sort(oldDependencies.begin(), oldDependencies.end());
    std::sort(newDependencies.begin(), newDependencies.end());
    if (!recoveringPlaceholder && oldDependencies != newDependencies) {
        diagnostic = "Script mod dependency declarations changed; restart is required. "
                     + std::string(kReloadDependencyBoundary);
        AddReloadBoundary(fields, "dependency_graph", "restart_required");
        AddReloadField(fields, "cascade", "false");
        return false;
    }

    BMLVersion currentBml;
    if (currentBml < candidate.MinBmlVersion) {
        diagnostic = "Script mod reload candidate requires newer BML.";
        AddReloadBoundary(fields, "bml_version", "upgrade_bml_or_restart");
        return false;
    }

    if (!m_Context || !m_Context->ValidateScriptModReloadDependencies(this, candidate, diagnostic, fields))
        return false;

    if (!options.ForceExports) {
        for (const ScriptModExportDefinition &exportInfo : m_Definition.Exports) {
            const std::string &name = exportInfo.Name;
            const std::string &signature = exportInfo.Signature;
            if (!candidateExports.HasExport(name, signature)) {
                diagnostic = "Script mod reload removed or changed export '" + name +
                             "' signature '" + signature + "'. "
                             "Hot reload keeps existing dependent mods running and does not cascade reload them, "
                             "so non-forced reload requires every previous export name/signature to remain available. "
                             "Keep the export, use manual --force-exports only when callers can tolerate it, or restart.";
                AddReloadBoundary(fields, "export_compatibility", "keep_export_force_exports_or_restart");
                AddReloadField(fields, "cascade", "false");
                AddReloadField(fields, "export", name);
                AddReloadField(fields, "signature", signature);
                return false;
            }
        }
    }

    return true;
}
bool ScriptMod::HasExport(const std::string &name, const std::string &signature) const {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return false;
    return m_Exports.HasExport(name, signature);
}

ScriptTimerRef *ScriptMod::AddScriptTimer(asIScriptObject *timer) {
    return m_Timers.Add(timer);
}

ScriptTimerRef *ScriptMod::AddScriptTimeoutTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name) {
    return m_Timers.AddTimeoutTicks(delayTicks, callback, name);
}

ScriptTimerRef *ScriptMod::AddScriptTimeoutMs(float delayMs, asIScriptFunction *callback, const std::string &name) {
    return m_Timers.AddTimeoutMs(delayMs, callback, name);
}

ScriptTimerRef *ScriptMod::AddScriptIntervalTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name) {
    return m_Timers.AddIntervalTicks(delayTicks, callback, name);
}

ScriptTimerRef *ScriptMod::AddScriptIntervalMs(float delayMs, asIScriptFunction *callback, const std::string &name) {
    return m_Timers.AddIntervalMs(delayMs, callback, name);
}

ScriptCommandRef *ScriptMod::RegisterScriptCommand(asIScriptObject *command) {
    return m_Commands.Register(command);
}

ScriptCommandRef *ScriptMod::RegisterScriptCommand(const ScriptCommandDefinition &definition,
                                                   asIScriptFunction *execute,
                                                   asIScriptFunction *complete) {
    return m_Commands.Register(definition, execute, complete);
}

bool ScriptMod::UnregisterScriptCommand(const std::string &name) {
    return m_Commands.Unregister(name);
}

ScriptDataShareRequestRef *ScriptMod::RequestScriptDataShare(asIScriptObject *request) {
    return m_DataShareRequests.Request(request);
}

ScriptDataShareRequestRef *ScriptMod::RequestScriptDataShare(const std::string &key,
                                                             int type,
                                                             asIScriptFunction *callback,
                                                             const std::string &name) {
    return m_DataShareRequests.Request(key, type, callback, name);
}

ScriptHookBlockRef *ScriptMod::CreateScriptHookBlock(CKBehavior *ownerScript,
                                                     asIScriptFunction *callback,
                                                     const std::string &name,
                                                     int inputCount,
                                                     int outputCount) {
    return m_HookBlocks.Create(ownerScript, callback, name, inputCount, outputCount);
}

ScriptHookBlockRef *ScriptMod::InsertScriptHookBlockAfter(CKBehavior *ownerScript,
                                                          CKBehavior *source,
                                                          asIScriptFunction *callback,
                                                          const std::string &name,
                                                          int sourceOutput,
                                                          int targetInput) {
    return m_HookBlocks.InsertAfter(ownerScript, source, callback, name, sourceOutput, targetInput);
}

ScriptHookBlockRef *ScriptMod::InsertScriptHookBlockBefore(CKBehavior *ownerScript,
                                                           CKBehavior *target,
                                                           asIScriptFunction *callback,
                                                           const std::string &name,
                                                           int sourceOutput,
                                                           int targetInput) {
    return m_HookBlocks.InsertBefore(ownerScript, target, callback, name, sourceOutput, targetInput);
}

ScriptHookBlockRef *ScriptMod::InsertScriptHookBlockBetween(CKBehavior *ownerScript,
                                                            CKBehavior *source,
                                                            CKBehavior *target,
                                                            asIScriptFunction *callback,
                                                            const std::string &name,
                                                            int sourceOutput,
                                                            int targetInput) {
    return m_HookBlocks.InsertBetween(ownerScript, source, target, callback, name, sourceOutput, targetInput);
}

void ScriptMod::ProcessQueuedScriptServiceCallbacks() {
    m_DataShareRequests.ProcessQueuedCallbacks();
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
    if (!NoteHostRegistration("ball", MakeHostRegistrationKey(ballFile, ballId, ballName, objName, friction,
                                                              elasticity, mass, collGroup, linearDamp, rotDamp,
                                                              force, radius)))
        return false;
    if (m_HostRegistrationMode == HostRegistrationMode::Validate)
        return true;
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
    if (!NoteHostRegistration("floor", MakeHostRegistrationKey(floorName, friction, elasticity, mass, collGroup,
                                                               enableColl)))
        return false;
    if (m_HostRegistrationMode == HostRegistrationMode::Validate)
        return true;
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
    if (!NoteHostRegistration("module-ball", MakeHostRegistrationKey(modulName, fixed, friction, elasticity, mass,
                                                                     collGroup, frozen, enableColl, calcMassCenter,
                                                                     linearDamp, rotDamp, radius)))
        return false;
    if (m_HostRegistrationMode == HostRegistrationMode::Validate)
        return true;
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
    if (!NoteHostRegistration("module-convex", MakeHostRegistrationKey(modulName, fixed, friction, elasticity, mass,
                                                                       collGroup, frozen, enableColl, calcMassCenter,
                                                                       linearDamp, rotDamp)))
        return false;
    if (m_HostRegistrationMode == HostRegistrationMode::Validate)
        return true;
    m_Context->RegisterModulConvex(modulName.c_str(), fixed, friction, elasticity, mass, collGroup.c_str(), frozen,
                                   enableColl, calcMassCenter, linearDamp, rotDamp);
    return true;
}

bool ScriptMod::RegisterScriptTrafo(const std::string &modulName) {
    if (!m_InLoadCallback || !m_Context || modulName.empty())
        return false;
    if (!NoteHostRegistration("trafo", MakeHostRegistrationKey(modulName)))
        return false;
    if (m_HostRegistrationMode == HostRegistrationMode::Validate)
        return true;
    m_Context->RegisterTrafo(modulName.c_str());
    return true;
}

bool ScriptMod::RegisterScriptModul(const std::string &modulName) {
    if (!m_InLoadCallback || !m_Context || modulName.empty())
        return false;
    if (!NoteHostRegistration("module", MakeHostRegistrationKey(modulName)))
        return false;
    if (m_HostRegistrationMode == HostRegistrationMode::Validate)
        return true;
    m_Context->RegisterModul(modulName.c_str());
    return true;
}

const ScriptExportBinding *ScriptMod::ResolveExport(const std::string &name, const std::string &signature) const {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return nullptr;
    return m_Exports.Resolve(name, signature);
}

std::string ScriptMod::GetExportSignature(const std::string &name) const {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return {};
    return m_Exports.GetSignature(name);
}

void ScriptMod::GetExportSignatures(const std::string &name, std::vector<std::string> &out) const {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    m_Exports.GetSignatures(name, out);
}

int ScriptMod::GetExportCount() const {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return 0;
    return m_Exports.GetCount();
}

bool ScriptMod::GetExportInfo(int index, std::string &name, std::string &signature) const {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return false;
    return m_Exports.GetInfo(index, name, signature);
}

int ScriptMod::CallVoidExport(const std::string &name, const std::string &signature) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return BML_ERROR_INTEROP_TARGET_FAILED;
    const ScriptExportBinding *binding = m_Exports.Resolve(name, signature);
    if (!binding) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                    BuildMissingExportDiagnostic(m_Definition, m_Exports, name, signature)));
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    }
    if (!CanDispatchScriptCallback())
        return BML_ERROR_INTEROP_TARGET_FAILED;

    ScriptDiagnostic diagnostic;
    const int status = ScriptExportDispatcher::CallVoid(m_Context ? m_Context->GetCKContext() : nullptr,
                                                        m_Runtime,
                                                        *binding,
                                                        diagnostic);
    if (status == BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED) {
        const bool wasLoaded = m_State.IsLoaded();
        Fail(diagnostic);
        if (wasLoaded)
            ScheduleFailureCleanup();
    } else if (status != BML_OK) {
        Record(diagnostic);
    }
    return status;
}

int ScriptMod::CallStringExport(const std::string &name,
                                const std::string &signature,
                                const std::string &argument,
                                bool hasArgument,
    std::string &out) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return BML_ERROR_INTEROP_TARGET_FAILED;
    const ScriptExportBinding *binding = m_Exports.Resolve(name, signature);
    if (!binding) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                    BuildMissingExportDiagnostic(m_Definition, m_Exports, name, signature)));
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    }
    if (!CanDispatchScriptCallback())
        return BML_ERROR_INTEROP_TARGET_FAILED;

    ScriptDiagnostic diagnostic;
    const int status = ScriptExportDispatcher::CallString(m_Context ? m_Context->GetCKContext() : nullptr,
                                                          m_Runtime,
                                                          *binding,
                                                          argument,
                                                          hasArgument,
                                                          out,
                                                          diagnostic);
    if (status == BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED) {
        const bool wasLoaded = m_State.IsLoaded();
        Fail(diagnostic);
        if (wasLoaded)
            ScheduleFailureCleanup();
    } else if (status != BML_OK) {
        Record(diagnostic);
    }
    return status;
}

int ScriptMod::CallExport(const std::string &name, const std::string &signature, BML_CallFrame *frame) {
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;

    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return BML_ERROR_INTEROP_TARGET_FAILED;
    const ScriptExportBinding *binding = m_Exports.Resolve(name, signature);
    if (!binding) {
        Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::ExportLookup,
                                    BuildMissingExportDiagnostic(m_Definition, m_Exports, name, signature)));
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    }
    if (!CanDispatchScriptCallback())
        return BML_ERROR_INTEROP_TARGET_FAILED;

    ScriptDiagnostic diagnostic;
    const int status = ScriptExportDispatcher::CallFrame(m_Context ? m_Context->GetCKContext() : nullptr,
                                                         m_Runtime,
                                                         *binding,
                                                         frame,
                                                         diagnostic);
    if (status == BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED) {
        const bool wasLoaded = m_State.IsLoaded();
        Fail(diagnostic);
        if (wasLoaded)
            ScheduleFailureCleanup();
    } else if (status != BML_OK) {
        Record(diagnostic);
    }
    return status;
}

void ScriptMod::CallGameEvent(size_t eventIndex) {
    ScriptModCallScope callScope(this);
    if (!callScope.Entered())
        return;
    if (!CanDispatchScriptCallback())
        return;
    if (!m_EventRouter.HasGameEvent())
        return;
    ScriptDiagnostic diagnostic;
    FailIfEventCallFailed(m_EventRouter.CallGameEvent(eventIndex, diagnostic), diagnostic);
}

void ScriptMod::CleanupFailedLoad() {
    const ScriptDiagnostic failureDiagnostic = m_State.GetLastDiagnostic();
    ScriptDiagnostic unloadDiagnostic;
    if (GetReloadPhase() == ScriptModReloadPhase::None) {
        m_EventRouter.CallOnUnload(unloadDiagnostic);
    } else {
        ScriptModReloadPhaseScope cleanupPhase(*this, ScriptModReloadPhase::Cleanup);
        m_EventRouter.CallOnUnload(unloadDiagnostic);
    }
    ReleaseRuntime();
    m_State.MarkLoaded(false);
    m_State.Fail(failureDiagnostic);
    m_PendingFailureCleanup.store(false, std::memory_order_release);
}

void ScriptMod::FailIfEventCallFailed(bool ok, const ScriptDiagnostic &diagnostic) {
    if (!ok && diagnostic.Status == CKAS_INUSE && m_Reloading.load(std::memory_order_acquire))
        return;
    if (!ok) {
        const bool wasLoaded = m_State.IsLoaded();
        Fail(diagnostic);
        if (wasLoaded && !m_InLoadCallback && !m_Reloading.load(std::memory_order_acquire))
            ScheduleFailureCleanup();
    }
}

void ScriptMod::Record(const ScriptDiagnostic &diagnostic) {
    ScriptDiagnostic rewritten = RewriteRuntimeDiagnosticPaths(diagnostic);
    m_State.Record(rewritten);
    TouchModGeneration();
    if (m_Context && !rewritten.Message.empty() && rewritten.Message != "Export call succeeded.") {
        m_Context->PublishScriptDevDiagnostic(ScriptDevEventSeverity::Info,
                                              "ScriptDiagnostic",
                                              GetID() ? GetID() : "",
                                              rewritten);
    }
}

void ScriptMod::Fail(const std::string &diagnostic) {
    Fail(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, diagnostic));
}

void ScriptMod::Fail(const ScriptDiagnostic &diagnostic) {
    ScriptDiagnostic rewritten = RewriteRuntimeDiagnosticPaths(diagnostic);
    m_State.MarkLoaded(false);
    m_State.Fail(rewritten);
    TouchModGeneration();
    if (m_Context) {
        m_Context->PublishScriptDevDiagnostic(ScriptDevEventSeverity::Error,
                                              rewritten.Phase == ScriptDiagnosticPhase::Callback
                                                  ? "ScriptCallbackFailed"
                                                  : "ScriptModFailed",
                                              GetID() ? GetID() : "",
                                              rewritten);
    }
    if (m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Error("Script mod %s failed: %s", GetID(), m_State.GetLastDiagnosticText().c_str());
}

void ScriptMod::ScheduleFailureCleanup() {
    m_PendingFailureCleanup.store(true, std::memory_order_release);
}

void ScriptMod::ProcessFailureCleanup() {
    if (!m_PendingFailureCleanup.load(std::memory_order_acquire))
        return;

    {
        std::lock_guard<std::mutex> lock(m_ReloadMutex);
        if (m_Reloading.load(std::memory_order_acquire) || m_ActiveScriptCalls != 0)
            return;
        m_ReloadThreadId = std::this_thread::get_id();
        m_Reloading.store(true, std::memory_order_release);
    }

    auto clearGate = [&]() {
        std::lock_guard<std::mutex> lock(m_ReloadMutex);
        m_ReloadThreadId = std::thread::id();
        m_Reloading.store(false, std::memory_order_release);
    };

    if (!m_State.IsFailed()) {
        m_PendingFailureCleanup.store(false, std::memory_order_release);
        clearGate();
        return;
    }

    const ScriptDiagnostic failure = m_State.GetLastDiagnostic();
    m_PendingFailureCleanup.store(false, std::memory_order_release);
    ExportRegistry::NotifyScriptExportsChanged();
    ReleaseRuntime();
    m_State.Fail(failure);
    TouchModGeneration();

    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                     "ScriptRuntimeResourcesReleased",
                                                     GetID() ? GetID() : "",
                                                     "runtime",
                                                     failure.EntryPath,
                                                     "Released BML-managed script resources after runtime failure.",
                                                     {{"boundary", "bml_managed_resources"},
                                                      {"notRestored", "game-world side effects made by script code"},
                                                      {"action", "fix_script_and_reload_or_restart"}},
                                                     GetReloadAttemptId());
    }

    ExportRegistry::NotifyScriptExportsChanged();
    clearGate();
}

bool ScriptMod::ReleaseRuntime() {
    CKContext *ckContext = m_Context ? m_Context->GetCKContext() : nullptr;
    bool ok = ReleaseScriptServices();
    ok = ReleaseScriptMethodHandles() && ok;

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

bool ScriptMod::ReleaseScriptServices() {
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
    m_HookBlocks.Release(&releaseDiagnostic);
    if (!releaseDiagnostic.Message.empty()) {
        Record(releaseDiagnostic);
        ok = false;
    }
    releaseDiagnostic = ScriptDiagnostic();
    return ok;
}

bool ScriptMod::ReleaseScriptMethodHandles() {
    CKContext *ckContext = m_Context ? m_Context->GetCKContext() : nullptr;
    ScriptDiagnostic releaseDiagnostic;
    bool ok = true;
    if (!m_Exports.Release(ckContext, m_Runtime, &releaseDiagnostic)) {
        Record(releaseDiagnostic);
        ok = false;
    }
    releaseDiagnostic = ScriptDiagnostic();
    if (!m_EventRouter.Release(&releaseDiagnostic)) {
        Record(releaseDiagnostic);
        ok = false;
    }
    return ok;
}

bool ScriptMod::ReleaseRuntimeOnly(ScriptModRuntime &runtime) {
    CKContext *ckContext = m_Context ? m_Context->GetCKContext() : nullptr;
    ScriptDiagnostic diagnostic;
    const bool ok = runtime.Release(ckContext, &diagnostic);
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

ScriptModReloadResult ScriptMod::TryHotReloadDryRun(const ScriptModReloadOptions &options) {
    ScriptModReloadResult result;
    {
        std::lock_guard<std::mutex> lock(m_ReloadMutex);
        if (m_Reloading.load(std::memory_order_acquire)) {
            result.RetryLater = true;
            result.Diagnostic = "Reload already in progress.";
            return result;
        }
        if (m_ActiveScriptCalls != 0) {
            result.RetryLater = true;
            result.Diagnostic = "Script callback/export is active; reload dry-run deferred.";
            return result;
        }
        if (GetQueuedScriptServiceCallbackCount() != 0) {
            result.RetryLater = true;
            result.Diagnostic = "Script service callback is queued; reload dry-run deferred.";
            return result;
        }
        m_ReloadThreadId = std::this_thread::get_id();
        m_Reloading.store(true, std::memory_order_release);
    }
    TouchReloadAttempt();
    result.ReloadAttemptId = GetReloadAttemptId();

    auto finish = [&](bool success,
                      const std::string &diagnostic,
                      const ScriptDiagnostic *structuredDiagnostic = nullptr,
                      const std::vector<ScriptModReloadDiagnosticField> *fields = nullptr) {
        result.Success = success;
        result.ReloadAttemptId = GetReloadAttemptId();
        result.Diagnostic = diagnostic;
        if (fields)
            result.Fields = *fields;
        if (!success) {
            if (structuredDiagnostic)
                SetReloadDiagnostic(*structuredDiagnostic);
            else
                SetReloadDiagnostic(diagnostic);
        } else {
            SetReloadDiagnostic(std::string());
        }
        {
            std::lock_guard<std::mutex> lock(m_ReloadMutex);
            m_ReloadThreadId = std::thread::id();
            m_Reloading.store(false, std::memory_order_release);
        }
        return result;
    };
    auto finishWithDiagnostic = [&](const ScriptDiagnostic &diagnostic) {
        if (result.SourcePath.empty())
            result.SourcePath = diagnostic.EntryPath;
        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishDiagnostic(ScriptDevEventSeverity::Error,
                                                              "ScriptReloadDryRunDiagnostic",
                                                              GetID() ? GetID() : "",
                                                              diagnostic,
                                                              GetReloadAttemptId());
        }
        return finish(false, FormatScriptDiagnostic(diagnostic), &diagnostic);
    };

    ScriptDiagnostic diagnostic;
    ScriptModReloadSourceSnapshot snapshot;
    if (!CaptureReloadSourceSnapshot(m_Entry, snapshot, diagnostic))
        return finishWithDiagnostic(diagnostic);
    result.SourcePath = snapshot.CommitEntryPathUtf8;

    ScriptModDefinition candidateDefinition;
    ScriptModRuntime candidateRuntime(MakeReloadModuleName(m_Runtime));
    if (!candidateRuntime.LoadModule(m_Context ? m_Context->GetCKContext() : nullptr,
                                     snapshot.CompileEntryPathUtf8,
                                     diagnostic)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    ScriptModDefinitionBuilder builder;
    if (!builder.Build(m_Context ? m_Context->GetCKContext() : nullptr,
                       snapshot.CommitEntry,
                       candidateRuntime,
                       candidateDefinition,
                       diagnostic)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    candidateRuntime.SetOwner(this);
    if (!candidateRuntime.CreateObject(m_Context ? m_Context->GetCKContext() : nullptr,
                                       candidateDefinition.ClassNamespace,
                                       candidateDefinition.ClassName,
                                       diagnostic)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    ScriptModEventRouter candidateEvents;
    candidateEvents.Bind(m_Context ? m_Context->GetCKContext() : nullptr, &candidateRuntime, &m_ContextView);
    if (!candidateEvents.Cache(diagnostic)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        candidateEvents.Release(nullptr);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    ScriptExportTable candidateExports;
    if (!candidateExports.Cache(m_Context ? m_Context->GetCKContext() : nullptr,
                                candidateRuntime,
                                candidateDefinition.Exports,
                                diagnostic,
                                false)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        candidateEvents.Release(nullptr);
        candidateExports.Release(m_Context ? m_Context->GetCKContext() : nullptr, candidateRuntime, nullptr, false);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    std::string validationDiagnostic;
    std::vector<ScriptModReloadDiagnosticField> validationFields;
    if (!ValidateReloadDefinition(candidateDefinition, candidateExports, options, validationDiagnostic, &validationFields)) {
        candidateEvents.Release(nullptr);
        candidateExports.Release(m_Context ? m_Context->GetCKContext() : nullptr, candidateRuntime, nullptr, false);
        ReleaseRuntimeOnly(candidateRuntime);
        const ScriptDiagnostic failure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata, validationDiagnostic);
        return finish(false, validationDiagnostic, &failure, &validationFields);
    }
    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     "ScriptReloadPrepared",
                                                     GetID() ? GetID() : "",
                                                     "reload",
                                                     snapshot.CommitEntryPathUtf8,
                                                     "Script reload candidate prepared.",
                                                     {},
                                                     GetReloadAttemptId());
    }

    candidateEvents.Release(nullptr);
    candidateExports.Release(m_Context ? m_Context->GetCKContext() : nullptr, candidateRuntime, nullptr, false);
    ReleaseRuntimeOnly(candidateRuntime);
    return finish(true, "Reload dry-run passed.");
}

ScriptModReloadResult ScriptMod::TryHotReload(const ScriptModReloadOptions &options) {
    ScriptModReloadResult result;
    {
        std::lock_guard<std::mutex> lock(m_ReloadMutex);
        if (m_Reloading.load(std::memory_order_acquire)) {
            result.RetryLater = true;
            result.Diagnostic = "Reload already in progress.";
            return result;
        }
        if (m_ActiveScriptCalls != 0) {
            result.RetryLater = true;
            result.Diagnostic = "Script callback/export is active; reload deferred.";
            return result;
        }
        if (GetQueuedScriptServiceCallbackCount() != 0) {
            result.RetryLater = true;
            result.Diagnostic = "Script service callback is queued; reload deferred.";
            return result;
        }
        m_ReloadThreadId = std::this_thread::get_id();
        m_Reloading.store(true, std::memory_order_release);
    }
    TouchReloadAttempt();
    result.ReloadAttemptId = GetReloadAttemptId();

    auto finish = [&](bool success,
                      const std::string &diagnostic,
                      const ScriptDiagnostic *structuredDiagnostic = nullptr,
                      const std::vector<ScriptModReloadDiagnosticField> *fields = nullptr) {
        result.Success = success;
        result.ReloadAttemptId = GetReloadAttemptId();
        result.Diagnostic = diagnostic;
        if (fields)
            result.Fields = *fields;
        if (!success) {
            if (structuredDiagnostic)
                SetReloadDiagnostic(*structuredDiagnostic);
            else
                SetReloadDiagnostic(diagnostic);
        } else {
            SetReloadDiagnostic(std::string());
        }
        {
            std::lock_guard<std::mutex> lock(m_ReloadMutex);
            m_ReloadThreadId = std::thread::id();
            m_Reloading.store(false, std::memory_order_release);
        }
        return result;
    };
    auto finishWithDiagnostic = [&](const ScriptDiagnostic &diagnostic) {
        if (result.SourcePath.empty())
            result.SourcePath = diagnostic.EntryPath;
        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishDiagnostic(ScriptDevEventSeverity::Error,
                                                              "ScriptReloadDiagnostic",
                                                              GetID() ? GetID() : "",
                                                              diagnostic,
                                                              GetReloadAttemptId());
        }
        return finish(false, FormatScriptDiagnostic(diagnostic), &diagnostic);
    };

    ScriptDiagnostic diagnostic;
    ScriptModReloadSourceSnapshot snapshot;
    if (!CaptureReloadSourceSnapshot(m_Entry, snapshot, diagnostic))
        return finishWithDiagnostic(diagnostic);
    result.SourcePath = snapshot.CommitEntryPathUtf8;

    ScriptModDefinition candidateDefinition;
    ScriptModRuntime candidateRuntime(MakeReloadModuleName(m_Runtime));
    if (!candidateRuntime.LoadModule(m_Context ? m_Context->GetCKContext() : nullptr,
                                     snapshot.CompileEntryPathUtf8,
                                     diagnostic)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    ScriptModDefinitionBuilder builder;
    if (!builder.Build(m_Context ? m_Context->GetCKContext() : nullptr,
                       snapshot.CommitEntry,
                       candidateRuntime,
                       candidateDefinition,
                       diagnostic)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    candidateRuntime.SetOwner(this);
    if (!candidateRuntime.CreateObject(m_Context ? m_Context->GetCKContext() : nullptr,
                                       candidateDefinition.ClassNamespace,
                                       candidateDefinition.ClassName,
                                       diagnostic)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    ScriptExportTable candidateExports;
    if (!candidateExports.Cache(m_Context ? m_Context->GetCKContext() : nullptr,
                                candidateRuntime,
                                candidateDefinition.Exports,
                                diagnostic,
                                false)) {
        RewriteSnapshotDiagnosticPaths(snapshot, diagnostic);
        candidateExports.Release(m_Context ? m_Context->GetCKContext() : nullptr, candidateRuntime, nullptr, false);
        ReleaseRuntimeOnly(candidateRuntime);
        return finishWithDiagnostic(diagnostic);
    }

    std::string validationDiagnostic;
    std::vector<ScriptModReloadDiagnosticField> validationFields;
    if (!ValidateReloadDefinition(candidateDefinition, candidateExports, options, validationDiagnostic, &validationFields)) {
        candidateExports.Release(m_Context ? m_Context->GetCKContext() : nullptr, candidateRuntime, nullptr, false);
        ReleaseRuntimeOnly(candidateRuntime);
        const ScriptDiagnostic failure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata, validationDiagnostic);
        return finish(false, validationDiagnostic, &failure, &validationFields);
    }
    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     "ScriptReloadPrepared",
                                                     GetID() ? GetID() : "",
                                                     "reload",
                                                     snapshot.CommitEntryPathUtf8,
                                                     "Script reload candidate prepared.",
                                                     {},
                                                     GetReloadAttemptId());
    }
    candidateExports.Release(m_Context ? m_Context->GetCKContext() : nullptr, candidateRuntime, nullptr, false);
    if (!ReleaseRuntimeOnly(candidateRuntime)) {
        const ScriptDiagnostic failure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                                              "Reload candidate cleanup failed; keeping previous runtime.");
        return finish(false, failure.Message, &failure);
    }

    const bool recoveringPlaceholder = IsFailedPlaceholder();
    const std::string recoveryOldId = m_Definition.Id;
    bool recoveryPromoted = false;
    if (recoveringPlaceholder) {
        if (!m_Context) {
            const ScriptDiagnostic failure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata,
                                                                  "Script mod failed-load recovery requires a ModContext.");
            return finish(false, failure.Message, &failure);
        }
        std::string recoveryDiagnostic;
        if (!m_Context->PromoteFailedScriptModPlaceholder(this,
                                                          recoveryOldId,
                                                          candidateDefinition,
                                                          recoveryDiagnostic)) {
            const ScriptDiagnostic failure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata,
                                                                  recoveryDiagnostic);
            return finish(false, recoveryDiagnostic, &failure);
        }
        recoveryPromoted = true;
    }

    ExportRegistry::NotifyScriptExportsChanged();
    ScriptDiagnostic unloadDiagnostic;
    if (m_State.IsLoaded()) {
        ScriptModReloadPhaseScope unloadPhase(*this, ScriptModReloadPhase::Unload);
        if (!m_EventRouter.CallOnUnload(unloadDiagnostic))
            Record(unloadDiagnostic);
    }

    ReleaseScriptServices();
    ReleaseScriptMethodHandles();
    m_State.MarkLoaded(false);

    ScriptModDefinition oldDefinition = std::move(m_Definition);
    ScriptModEntry oldEntry = std::move(m_Entry);
    ScriptModRuntime oldRuntime = std::move(m_Runtime);
    const std::string oldDiagnosticPathFrom = m_RuntimeDiagnosticPathFrom;
    const std::string oldDiagnosticPathTo = m_RuntimeDiagnosticPathTo;
    const std::string liveModuleName = MakeReloadModuleName(oldRuntime);

    m_Definition = std::move(candidateDefinition);
    m_Entry = snapshot.CommitEntry;
    m_Runtime = ScriptModRuntime(liveModuleName);
    m_Runtime.SetOwner(this);
    SetRuntimeDiagnosticPathRewrite(snapshot.DiagnosticStagedRootUtf8,
                                    snapshot.DiagnosticDisplayRootUtf8);
    m_State.ClearFailure();

    ScriptDiagnostic liveDiagnostic;
    const bool liveModuleLoaded = m_Runtime.LoadModule(m_Context ? m_Context->GetCKContext() : nullptr,
                                                       snapshot.CompileEntryPathUtf8,
                                                       liveDiagnostic);
    if (!liveModuleLoaded) {
        RewriteSnapshotDiagnosticPaths(snapshot, liveDiagnostic);
        Fail(liveDiagnostic);
    }

    bool liveRuntimeLoaded = false;
    if (liveModuleLoaded) {
        const ScriptModReloadPhase loadPhase = recoveringPlaceholder
                                                   ? ScriptModReloadPhase::Recovery
                                                   : ScriptModReloadPhase::Load;
        ScriptModReloadPhaseScope phase(*this, loadPhase);
        liveRuntimeLoaded = LoadCurrentRuntime(true, recoveringPlaceholder);
    }

    if (liveModuleLoaded && liveRuntimeLoaded) {
        ReleaseRuntimeOnly(oldRuntime);
        if (oldEntry.SourceKind == ScriptModEntrySourceKind::ZipPackage &&
            !oldEntry.RootDirectory.empty() &&
            oldEntry.RootDirectory != m_Entry.RootDirectory) {
            utils::DeleteDirectoryW(oldEntry.RootDirectory);
        }
        if (snapshot.CommitEntry.RootDirectory == snapshot.CompileEntry.RootDirectory)
            snapshot.KeepStagedRoot();
        if (m_Context)
            m_Context->GetCommandContext().SortCommands();
        ExportRegistry::NotifyScriptExportsChanged();
        FenceCallbacksForCurrentFrame();
        return finish(true, std::string());
    }

    const ScriptDiagnostic reloadDiagnostic = m_State.GetLastDiagnostic();
    const std::string reloadFailure = m_State.GetLastDiagnosticText().empty()
                                          ? "Reload candidate OnLoad failed."
                                          : m_State.GetLastDiagnosticText();
    ReleaseRuntime();

    if (recoveringPlaceholder) {
        const std::string candidateId = m_Definition.Id;
        m_Definition = std::move(oldDefinition);
        m_Entry = std::move(oldEntry);
        m_Runtime = std::move(oldRuntime);
        m_Runtime.SetOwner(this);
        SetRuntimeDiagnosticPathRewrite(oldDiagnosticPathFrom, oldDiagnosticPathTo);
        if (recoveryPromoted && m_Context)
            m_Context->RestoreFailedScriptModPlaceholder(this, candidateId, m_Definition);

        ScriptDiagnostic failure = reloadDiagnostic;
        failure.Phase = ScriptDiagnosticPhase::Runtime;
        if (failure.Message.empty())
            failure.Message = "Failed-load recovery did not complete; fixed script was not loaded.";
        if (failure.RawMessage.empty() && failure.CompilerMessages.empty() && failure.StackTrace.empty())
            failure.RawMessage = reloadFailure;
        Fail(failure);
        ExportRegistry::NotifyScriptExportsChanged();
        FenceCallbacksForCurrentFrame();
        std::vector<ScriptModReloadDiagnosticField> fields = {
            {"boundary", "failed_load_recovery"},
            {"candidateId", candidateId},
            {"action", "fix_script_and_reload_again_or_restart"},
        };
        return finish(false, FormatScriptDiagnostic(failure), &failure, &fields);
    }

    m_Definition = std::move(oldDefinition);
    m_Entry = std::move(oldEntry);
    m_Runtime = std::move(oldRuntime);
    m_Runtime.SetOwner(this);
    SetRuntimeDiagnosticPathRewrite(oldDiagnosticPathFrom, oldDiagnosticPathTo);
    m_State.ClearFailure();

    bool rollbackLoaded = false;
    {
        ScriptModReloadPhaseScope phase(*this, ScriptModReloadPhase::Rollback);
        rollbackLoaded = LoadCurrentRuntime(true);
    }

    if (rollbackLoaded) {
        if (m_Context)
            m_Context->GetCommandContext().SortCommands();
        ExportRegistry::NotifyScriptExportsChanged();
        FenceCallbacksForCurrentFrame();
        ScriptDiagnostic failure = reloadDiagnostic;
        failure.Phase = ScriptDiagnosticPhase::Runtime;
        failure.Message = "Reload failed; rolled back to previous runtime. "
                          + std::string(kReloadRollbackBoundary);
        if (failure.RawMessage.empty() && failure.CompilerMessages.empty() && failure.StackTrace.empty())
            failure.RawMessage = reloadFailure;
        std::vector<ScriptModReloadDiagnosticField> fields = {
            {"boundary", "rollback_scope"},
            {"rollback", "success"},
            {"restored", "BML-managed script resources and runtime handles"},
            {"notRestored", "game-world side effects made by script code"},
            {"action", "restart_if_game_world_state_changed"},
        };
        return finish(false, FormatScriptDiagnostic(failure), &failure, &fields);
    }

    const std::string rollbackFailure = m_State.GetLastDiagnosticText().empty()
                                            ? "Rollback failed."
                                            : m_State.GetLastDiagnosticText();
    ScriptDiagnostic rollbackDiagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                                               "Reload failed and rollback failed. "
                                                               + std::string(kReloadRollbackBoundary));
    rollbackDiagnostic.RawMessage = reloadFailure + "\n" + rollbackFailure;
    Fail(rollbackDiagnostic);
    ExportRegistry::NotifyScriptExportsChanged();
    std::vector<ScriptModReloadDiagnosticField> fields = {
        {"boundary", "rollback_scope"},
        {"rollback", "failed"},
        {"state", "failed"},
        {"action", "restart_required"},
    };
    return finish(false, m_State.GetLastDiagnosticText(), &m_State.GetLastDiagnostic(), &fields);
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

bool ScriptMod::IsFailedPlaceholder() const {
    return m_State.IsFailed() &&
           !m_Entry.SyntheticId.empty() &&
           m_Definition.Id == m_Entry.SyntheticId &&
           !m_State.IsLoaded();
}

} // namespace BML
