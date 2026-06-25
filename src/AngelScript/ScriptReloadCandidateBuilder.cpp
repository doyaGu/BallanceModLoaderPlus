#include "ScriptReloadCandidateBuilder.h"

#include <algorithm>
#include <atomic>
#include <cwchar>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

#include "ModContext.h"
#include "ScriptLibraryServices.h"
#include "ScriptModDefinitionBuilder.h"
#include "ScriptSourceSnapshotBuilder.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

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

static bool EndsWithInsensitiveW(const std::wstring &value, const wchar_t *suffix) {
    if (!suffix)
        return false;
    const size_t suffixLength = std::wcslen(suffix);
    if (value.size() < suffixLength)
        return false;
    return _wcsicmp(value.c_str() + value.size() - suffixLength, suffix) == 0;
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

static bool CopyScriptSourceSnapshotContentsW(const std::wstring &source,
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
        if (!EndsWithInsensitiveW(it->path().wstring(), L".as"))
            continue;
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
            !CopyScriptSourceSnapshotContentsW(entry.ResourceRootDirectory, stagedResourceRoot)) {
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

    if (!CopyScriptSourceSnapshotContentsW(entry.RootDirectory, stagingRoot)) {
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

static bool CaptureReloadSourceSnapshot(ModContext *context,
                                        const ScriptModEntry &entry,
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
        RewriteReloadSnapshotDiagnosticPaths(snapshot, diagnostic);
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

    ScriptLibraryRegistry registry = MakeInstalledScriptLibraryRegistry(context, diagnostic);
    if (!diagnostic.Message.empty())
        return false;

    ScriptSourceSnapshot sourceSnapshot;
    ScriptSourceSnapshotBuilder snapshotBuilder(std::move(registry));
    if (!snapshotBuilder.Build(snapshot.CompileEntry, sourceSnapshot, diagnostic)) {
        RewriteReloadSnapshotDiagnosticPaths(snapshot, diagnostic);
        return false;
    }
    snapshot.EntrySectionNameUtf8 = sourceSnapshot.EntrySectionName;
    snapshot.SourceSections = std::move(sourceSnapshot.Sections);
    snapshot.SourceLibraries = std::move(sourceSnapshot.Libraries);
    snapshot.SourceDependencies = std::move(sourceSnapshot.Dependencies);

    return true;
}

ScriptReloadCandidateBuilder::ScriptReloadCandidateBuilder(ScriptMod &mod,
                                                           ScriptModReloadCandidate::State &state,
                                                           const ScriptModReloadOptions &options)
    : m_Mod(mod), m_State(state), m_Options(options) {
}

ScriptReloadCandidateBuilder::~ScriptReloadCandidateBuilder() {
    if (m_RuntimeOwned)
        DiscardPreparedCandidate();
}

bool ScriptReloadCandidateBuilder::FailWithDiagnostic(ScriptDiagnostic diagnostic,
                                                      Failure &failure,
                                                      bool rewriteSnapshotPaths) {
    if (rewriteSnapshotPaths)
        RewriteReloadSnapshotDiagnosticPaths(m_State.Snapshot, diagnostic);
    failure = Failure();
    failure.Diagnostic = diagnostic;
    failure.PublishDiagnostic = true;
    return false;
}

bool ScriptReloadCandidateBuilder::FailWithMessage(const std::string &message,
                                                   const ScriptDiagnostic &diagnostic,
                                                   const std::vector<ScriptModReloadDiagnosticField> &fields,
                                                   Failure &failure) {
    failure = Failure();
    failure.Diagnostic = diagnostic;
    failure.Fields = fields;
    failure.Message = message;
    failure.PublishDiagnostic = false;
    return false;
}

void ScriptReloadCandidateBuilder::ReleasePreparedHandles() {
    if (m_EventsBound) {
        m_CandidateEvents.Release(nullptr);
        m_EventsBound = false;
    }
    if (m_ExportsTouched) {
        m_CandidateExports.Release(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                   m_State.CandidateRuntime,
                                   nullptr,
                                   false);
        m_ExportsTouched = false;
    }
}

void ScriptReloadCandidateBuilder::DiscardPreparedCandidate() {
    ReleasePreparedHandles();
    if (m_RuntimeOwned) {
        m_Mod.ReleaseRuntimeOnly(m_State.CandidateRuntime);
        m_RuntimeOwned = false;
    }
}

void ScriptReloadCandidateBuilder::KeepPreparedRuntime() {
    ReleasePreparedHandles();
    m_RuntimeOwned = false;
}

bool ScriptReloadCandidateBuilder::Build(ScriptModReloadResult &result, Failure &failure) {
    failure = Failure();

    ScriptDiagnostic diagnostic;
    if (!CaptureReloadSourceSnapshot(m_Mod.m_Context, m_Mod.m_Entry, m_State.Snapshot, diagnostic))
        return FailWithDiagnostic(diagnostic, failure, false);
    result.SourcePath = m_State.Snapshot.CommitEntryPathUtf8;

    m_State.CandidateRuntime = ScriptModRuntime(MakeReloadModuleName(m_Mod.m_Runtime));
    m_RuntimeOwned = true;
    if (!m_State.CandidateRuntime.LoadModuleFromSections(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                                         m_State.Snapshot.SourceSections,
                                                         m_State.Snapshot.EntrySectionNameUtf8,
                                                         diagnostic)) {
        return FailWithDiagnostic(diagnostic, failure);
    }

    ScriptModDefinitionBuilder builder;
    if (!builder.Build(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                       m_State.Snapshot.CommitEntry,
                       m_State.CandidateRuntime,
                       m_State.CandidateDefinition,
                       diagnostic)) {
        return FailWithDiagnostic(diagnostic, failure);
    }
    m_State.CandidateDefinition.SourceLibraries = m_State.Snapshot.SourceLibraries;
    m_State.CandidateDefinition.SourceDependencies = m_State.Snapshot.SourceDependencies;

    m_State.CandidateRuntime.SetOwner(&m_Mod);
    if (!m_State.CandidateRuntime.CreateObject(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                               m_State.CandidateDefinition.ClassNamespace,
                                               m_State.CandidateDefinition.ClassName,
                                               diagnostic)) {
        return FailWithDiagnostic(diagnostic, failure);
    }

    m_CandidateEvents.Bind(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                           &m_State.CandidateRuntime,
                           &m_Mod.m_ContextView);
    m_EventsBound = true;
    if (!m_CandidateEvents.Cache(diagnostic))
        return FailWithDiagnostic(diagnostic, failure);

    m_ExportsTouched = true;
    if (!m_CandidateExports.Cache(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                  m_State.CandidateRuntime,
                                  m_State.CandidateDefinition.Exports,
                                  diagnostic,
                                  false)) {
        return FailWithDiagnostic(diagnostic, failure);
    }

    std::string validationDiagnostic;
    std::vector<ScriptModReloadDiagnosticField> validationFields;
    if (!m_Mod.ValidateReloadDefinition(m_State.CandidateDefinition,
                                        m_CandidateExports,
                                        m_Options,
                                        validationDiagnostic,
                                        &validationFields)) {
        const ScriptDiagnostic validationFailure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata,
                                                                       validationDiagnostic);
        return FailWithMessage(validationDiagnostic, validationFailure, validationFields, failure);
    }

    return true;
}

} // namespace BML
