#include "UpdaterService.h"
#include "UpdaterManifest.h"
#include "UpdaterPaths.h"
#include "UpdaterSdl.h"
#include "UpdaterTarget.h"
#include "CryptoUtils.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include <algorithm>
#include <exception>
#include <set>
#include <unordered_map>

namespace bmlupdater {
namespace {
struct Backup {
    std::string root;
    std::string path;
    bool existed{};
    uint32_t mode{};
    std::string hash;
    std::wstring backup;
    std::wstring target;
};

Result CheckContext(const UpdaterContext &context) {
    return CheckUpdaterPaths(context);
}

std::string JournalJson(const std::vector<Backup> &backups) {
    utils::MutableJsonDocument doc;
    auto *root = doc.CreateObject(); doc.SetRoot(root);
    (void)doc.AddInt(root, "schemaVersion", 1);
    auto *files = doc.CreateArray();
    for (const auto &backup : backups) {
        auto *file = doc.CreateObject();
        (void)doc.AddString(file, "root", backup.root);
        (void)doc.AddString(file, "path", backup.path);
        (void)doc.AddBool(file, "existed", backup.existed);
        (void)doc.AddInt(file, "mode", backup.mode);
        (void)doc.AddString(file, "sha256", backup.hash);
        (void)doc.AddValue(files, file);
    }
    (void)doc.AddValue(root, "files", files);
    std::string error;
    return doc.Write(true, error);
}

Result LoadJournal(const UpdaterContext &context, const std::wstring &transaction, std::vector<Backup> &backups) {
    backups.clear();
    std::string error;
    auto doc = utils::JsonDocument::ParseFile(JoinPath(transaction, L"journal.json"), error);
    if (!doc.IsValid() || utils::JsonGetInt(doc.Root(), "schemaVersion", 0) != 1)
        return Result::Failure("Invalid rollback journal: " + error);
    auto *files = yyjson_obj_get(doc.Root(), "files");
    if (!yyjson_is_arr(files) || yyjson_arr_size(files) == 0) return Result::Failure("Empty rollback journal");
    std::set<std::wstring> targets;
    size_t index{}, count{}; yyjson_val *file{};
    yyjson_arr_foreach(files, index, count, file) {
        Backup backup;
        backup.root = utils::JsonGetString(file, "root", "");
        backup.path = utils::JsonGetString(file, "path", "");
        backup.existed = utils::JsonGetBool(file, "existed", false);
        backup.mode = static_cast<uint32_t>(utils::JsonGetInt(file, "mode", -1));
        backup.hash = utils::JsonGetString(file, "sha256", "");
        if (backup.mode > 0777 || (backup.root != "game" && backup.root != "state"))
            return Result::Failure("Invalid rollback entry");
        if (backup.root == "state") {
            if (backup.path != "state.json" && backup.path != "installed.manifest.json" && backup.path != "installed.manifest.json.sig")
                return Result::Failure("Invalid rollback metadata path");
        } else if (!IsAllowedUpdatePath(backup.path, {"sdl", BML_UPDATER_PLATFORM, BML_UPDATER_ARCH, "static"}))
            return Result::Failure("Forbidden rollback path");
        if (!ResolveUnderRoot(backup.root == "game" ? context.gameRoot : context.updaterStateRoot,
                              utils::Utf8ToUtf16(backup.path), backup.target, error) ||
            !targets.insert(backup.target).second)
            return Result::Failure("Invalid rollback target: " + error);
        if (!ResolveUnderRoot(transaction, L"backup/" + std::to_wstring(index), backup.backup, error)) return Result::Failure(error);
        if (backup.existed && (!RegularFileExists(backup.backup) || backup.hash.size() != 64 ||
                               utils::Sha256FileHex(backup.backup) != backup.hash))
            return Result::Failure("Backup missing or corrupted; transaction retained: " + PathUtf8(backup.backup));
        backups.push_back(std::move(backup));
    }
    return Result::Success();
}

Result SelectTransaction(const UpdaterContext &context, std::wstring &transaction) {
    transaction.clear();
    const auto pending = JoinPath(context.updaterStateRoot, L"pending.json");
    const auto transactions = JoinPath(context.updaterStateRoot, L"transactions");
    if (PathExists(pending)) {
        std::string error;
        auto doc = utils::JsonDocument::ParseFile(pending, error);
        const auto name = doc.IsValid() ? utils::JsonGetString(doc.Root(), "transaction", "") : std::string();
        if (!IsValidUpdateVersion(name) || !ResolveUnderRoot(transactions, utils::Utf8ToUtf16(name), transaction, error))
            return Result::Failure("Invalid pending transaction; repair metadata before retrying");
        return Result::Success();
    }
    for (const auto &name : utils::ListDirectoriesW(transactions, L"*")) {
        std::string error; std::wstring candidate;
        if (!ResolveUnderRoot(transactions, name, candidate, error)) continue;
        if (RegularFileExists(JoinPath(candidate, L"committed")) && !PathExists(JoinPath(candidate, L"rolled-back")))
            transaction = candidate;
    }
    return transaction.empty() ? Result::Failure("No rollback transaction found") : Result::Success();
}

Result Restore(const UpdaterContext &context, const std::wstring &transaction, ProgressCallback progress) {
    std::vector<Backup> backups;
    auto result = LoadJournal(context, transaction, backups);
    if (!result.ok) return result;
    std::string error;
    utils::MutableJsonDocument pending;
    auto *pendingRoot = pending.CreateObject(); pending.SetRoot(pendingRoot);
    (void)pending.AddString(pendingRoot, "transaction", PathUtf8(FileName(transaction)));
    if (!WriteTextFile(JoinPath(context.updaterStateRoot, L"pending.json"), pending.Write(true, error), error))
        return Result::Failure(error);
    // Keep the journal and pending hint until every restore succeeds. Repeating
    // this operation after another interruption is safe.
    for (auto it = backups.rbegin(); it != backups.rend(); ++it) {
        if (progress) progress("restoring " + it->path);
        if (it->existed) {
            if (RegularFileExists(it->target) && FileMode(it->target) == it->mode && utils::Sha256FileHex(it->target) == it->hash)
                continue;
            if (!CopyFileAtomic(it->backup, it->target, it->mode, error, it->hash)) return Result::Failure(error);
        } else if (!RemoveFileIfPresent(it->target, error)) return Result::Failure(error);
    }
    if (!WriteTextFile(JoinPath(transaction, L"rolled-back"), "\n", error) ||
        !RemoveFileIfPresent(JoinPath(context.updaterStateRoot, L"pending.json"), error) || !SyncDirectory(context.updaterStateRoot))
        return Result::Failure("Rollback restored files but could not finalize metadata: " + error);
    return Result::Success("rollback complete");
}
}

Result UpdaterService::BuildApplyPlan(const LocalPackageVerification &verification, ApplyPlan &plan) const {
    plan = {};
    auto result = CheckPackageTarget(m_Context, verification.manifest);
    if (!result.ok) return result;
    plan.version = verification.manifest.version;
    std::unordered_map<std::string, const StagedFile *> staged;
    for (const auto &file : verification.stagedFiles) {
        if (!staged.emplace(ArchivePathKey(file.relativePath), &file).second) return Result::Failure("Duplicate staged file");
    }
    std::string error;
    std::wstring stagingRoot;
    if (!ResolveUnderRoot(m_Context.updaterStateRoot, L"staging/" + utils::Utf8ToUtf16(plan.version), stagingRoot, error))
        return Result::Failure(error);
    std::set<std::wstring> targets;
    for (const auto &file : verification.manifest.managedFiles) {
        std::wstring target, source;
        if (!ResolveUnderRoot(m_Context.gameRoot, utils::Utf8ToUtf16(file.path), target, error) ||
            !ResolveUnderRoot(stagingRoot, utils::Utf8ToUtf16(file.path), source, error) || !targets.insert(target).second)
            return Result::Failure("Invalid managed target: " + error);
        auto found = staged.find(ArchivePathKey(file.path));
        if (found == staged.end() || utils::ResolvePathW(found->second->stagedPath) != source ||
            !RegularFileExists(source) || FileSize(source) != file.size || utils::Sha256FileHex(source) != file.sha256)
            return Result::Failure("Staged file missing or changed: " + file.path);
        const bool player = file.path == "Player" || file.path == "Player.exe" || file.path == "Bin/Player" || file.path == "Bin/Player.exe";
        const auto lower = ToLowerAscii(file.path);
        const bool library = lower.ends_with(".dll") || lower.ends_with(".dylib") || lower.ends_with(".so") || lower.find(".so.") != std::string::npos;
        if ((player || library) &&
            !IsCompatibleBinary(source, verification.manifest.target, player))
            return Result::Failure("Payload binary format/architecture does not match update target: " + file.path);
        plan.operations.push_back({OperationKind::InstallOrReplace, file.path, source, target, {}, {}, file.sha256});
    }
    auto previous = LoadInstalledManifest();
    for (const auto &remove : verification.manifest.removeFiles) {
        const ManagedFile *old = nullptr;
        if (previous) for (const auto &file : previous->managedFiles)
            if (ArchivePathKey(file.path) == ArchivePathKey(remove.path)) old = &file;
        if (!old) { plan.diagnostics.push_back("Skipping removeFiles without a previous trusted file: " + remove.path); continue; }
        const auto expected = remove.sha256.empty() ? old->sha256 : remove.sha256;
        std::wstring target;
        if (!ResolveUnderRoot(m_Context.gameRoot, utils::Utf8ToUtf16(remove.path), target, error)) return Result::Failure(error);
        if (!RegularFileExists(target) || utils::Sha256FileHex(target) != expected) {
            plan.diagnostics.push_back("Skipping absent or modified removeFiles entry: " + remove.path); continue;
        }
        if (!targets.insert(target).second) return Result::Failure("Conflicting update/remove target");
        plan.operations.push_back({OperationKind::Remove, remove.path, {}, target, {}, expected, {}});
    }
    // The descriptor is part of the signed payload, and must describe exactly
    // the runtime this package is allowed to replace.
    auto doc = utils::JsonDocument::ParseFile(JoinPath(stagingRoot, L"ModLoader/BML.abi.json"), error);
    UpdateTarget payload;
    if (!doc.IsValid() || utils::JsonGetInt(doc.Root(), "schema", 0) != 1 ||
        !ReadUpdateTarget(doc.Root(), payload).ok || payload != verification.manifest.target)
        return Result::Failure("Package ABI descriptor does not match its signed update target");
    const auto sdk = utils::JsonGetString(doc.Root(), "sdk_abi", "");
    if (sdk.size() != 64 || HexToBytes(sdk).size() != 32) return Result::Failure("Package SDK ABI identifier is invalid");
    return Result::Success("apply plan built");
}

Result UpdaterService::CheckApplyAccess(const ApplyPlan &plan) const {
    auto result = CheckContext(m_Context);
    if (!result.ok) return result;
    result = CheckPlayerStopped();
    if (!result.ok) return result;
    if (PathExists(PendingFile())) return Result::Failure("An interrupted transaction is pending; run rollback first");
    if (!CanCreateFileInDirectory(m_Context.updaterStateRoot)) return Result::Failure("Updater state directory is not writable");
    for (const auto &op : plan.operations) {
        std::string error; std::wstring target;
        if (!ResolveUnderRoot(m_Context.gameRoot, utils::Utf8ToUtf16(op.relativePath), target, error) || target != op.targetPath)
            return Result::Failure("Invalid apply target: " + error);
        if ((PathExists(target) && (!RegularFileExists(target) || !(FileMode(target) & 0200))) ||
            !CanCreateFileInDirectory(ParentPath(target)))
            return Result::Failure("Update target is not replaceable: " + PathUtf8(target));
    }
    for (const auto &path : {StateFile(), InstalledManifestFile(), InstalledManifestFile() + L".sig"})
        if (PathExists(path) && (!RegularFileExists(path) || !(FileMode(path) & 0200)))
            return Result::Failure("Updater metadata is not writable: " + PathUtf8(path));
    return Result::Success("apply access check passed");
}

Result UpdaterService::Apply(const LocalPackageVerification &verification, const ApplyPlan &plan, ProgressCallback progress) const {
    auto result = CheckApplyAccess(plan);
    if (!result.ok) return result;
    UpdaterLock lock(m_Context.updaterStateRoot);
    if (!lock) return Result::Failure("Another updater is active, or the state directory cannot be locked");
    if (PathExists(PendingFile())) return Result::Failure("An interrupted transaction is pending; run rollback first");
    result = VerifyManifestSignature(verification.manifestPath, verification.signaturePath);
    if (!result.ok) return result;
    UpdaterManifest signedManifest;
    result = LoadManifestFile(verification.manifestPath, signedManifest);
    std::string error;
    if (!result.ok || WriteManifestJson(signedManifest, error) != WriteManifestJson(verification.manifest, error))
        return Result::Failure("Verified manifest changed before apply");
    ApplyPlan checked;
    result = BuildApplyPlan(verification, checked);
    if (!result.ok) return result;
    if (checked.operations != plan.operations || checked.version != plan.version) return Result::Failure("Apply plan changed before apply");

    const auto transaction = MakeTransactionRoot(m_Context);
    std::vector<Backup> backups;
    for (const auto &op : plan.operations) backups.push_back({"game", op.relativePath, false, 0, {}, {}, op.targetPath});
    for (const auto *name : {"state.json", "installed.manifest.json", "installed.manifest.json.sig"})
        backups.push_back({"state", name, false, 0, {}, {}, JoinPath(m_Context.updaterStateRoot, utils::Utf8ToUtf16(name))});
    if (!CreateDirectories(JoinPath(transaction, L"backup"))) return Result::Failure("Unable to create backup directory");
    if (!SyncDirectory(transaction) || !SyncDirectory(ParentPath(transaction)) ||
        !SyncDirectory(m_Context.updaterStateRoot) || !SyncDirectory(ParentPath(m_Context.updaterStateRoot)))
        return Result::Failure("Unable to flush transaction directories");
    for (size_t i = 0; i < backups.size(); ++i) {
        auto &backup = backups[i];
        backup.backup = JoinPath(transaction, L"backup/" + std::to_wstring(i));
        backup.existed = PathExists(backup.target);
        if (backup.existed) {
            if (!RegularFileExists(backup.target)) return Result::Failure("Non-regular backup target");
            backup.mode = FileMode(backup.target);
            backup.hash = utils::Sha256FileHex(backup.target);
            if (backup.hash.empty() || !CopyFileAtomic(backup.target, backup.backup, 0600, error, backup.hash)) return Result::Failure(error);
        }
    }
    if (!WriteTextFile(JoinPath(transaction, L"journal.json"), JournalJson(backups), error)) return Result::Failure(error);
    utils::MutableJsonDocument pending;
    auto *root = pending.CreateObject(); pending.SetRoot(root);
    (void)pending.AddString(root, "transaction", PathUtf8(FileName(transaction)));
    if (!WriteTextFile(PendingFile(), pending.Write(true, error), error)) return Result::Failure(error);

    auto fail = [&](std::string message) {
        auto restored = Restore(m_Context, transaction, {});
        if (!restored.ok) message += "; rollback failed; transaction retained: " + restored.message;
        return Result::Failure(std::move(message));
    };
    try {
        for (const auto &op : plan.operations) {
            if (progress) progress((op.kind == OperationKind::Remove ? "removing " : "installing ") + op.relativePath);
            // Recheck path identity after callbacks and before publication.
            std::wstring target;
            if (!ResolveUnderRoot(m_Context.gameRoot, utils::Utf8ToUtf16(op.relativePath), target, error)) return fail(error);
            if (op.kind == OperationKind::InstallOrReplace) {
                const auto found = std::find_if(signedManifest.managedFiles.begin(), signedManifest.managedFiles.end(),
                    [&](const auto &file) { return file.path == op.relativePath; });
                if (found == signedManifest.managedFiles.end() ||
                    !CopyFileAtomic(op.sourcePath, target, found->mode, error, op.newSha256)) return fail(error);
            } else if (utils::Sha256FileHex(target) != op.expectedOldSha256 || !RemoveFileIfPresent(target, error))
                return fail("Removal target changed or could not be removed: " + op.relativePath);
        }
        const auto manifestBytes = ReadTextFile(verification.manifestPath);
        const auto signatureBytes = ReadTextFile(verification.signaturePath);
        utils::MutableJsonDocument state;
        auto *object = state.CreateObject(); state.SetRoot(object);
        (void)state.AddString(object, "installedVersion", plan.version);
        (void)state.AddString(object, "lastError", "");
        if (!WriteTextFile(InstalledManifestFile(), manifestBytes, error) ||
            !WriteTextFile(InstalledManifestFile() + L".sig", signatureBytes, error) ||
            !WriteTextFile(StateFile(), state.Write(true, error), error) ||
            !WriteTextFile(JoinPath(transaction, L"committed"), "\n", error)) return fail(error);
        if (!RemoveFileIfPresent(PendingFile(), error) || !SyncDirectory(m_Context.updaterStateRoot))
            return Result::Failure("Update installed but pending metadata could not be cleared; transaction retained: " + error);
    } catch (const std::exception &exception) { return fail(exception.what()); }
      catch (...) { return fail("Update callback failed"); }
    if (progress) progress("apply complete");
    return Result::Success("apply complete");
}

Result UpdaterService::CheckRollbackAccess() const {
    auto result = CheckContext(m_Context);
    if (!result.ok) return result;
    result = CheckPlayerStopped();
    if (!result.ok) return result;
    std::wstring transaction;
    result = SelectTransaction(m_Context, transaction);
    if (!result.ok) return result;
    std::vector<Backup> backups;
    result = LoadJournal(m_Context, transaction, backups);
    if (!result.ok) return result;
    for (const auto &backup : backups) if (!CanCreateFileInDirectory(ParentPath(backup.target)))
        return Result::Failure("Rollback target directory is not writable: " + PathUtf8(backup.target));
    return Result::Success("rollback access check passed");
}
Result UpdaterService::Rollback(ProgressCallback progress) const {
    auto result = CheckContext(m_Context);
    if (!result.ok) return result;
    UpdaterLock lock(m_Context.updaterStateRoot);
    if (!lock) return Result::Failure("Another updater is active, or the state directory cannot be locked");
    result = CheckRollbackAccess();
    if (!result.ok) return result;
    std::wstring transaction;
    result = SelectTransaction(m_Context, transaction);
    if (!result.ok) return result;
    return Restore(m_Context, transaction, std::move(progress));
}
}
