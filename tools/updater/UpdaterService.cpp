#include "UpdaterService.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "CryptoUtils.h"
#include "JsonUtils.h"
#include "StringUtils.h"
#include "UpdaterManifest.h"
#include "UpdaterNetwork.h"
#include "UpdaterPaths.h"
#include "UpdaterRemote.h"
#include "UpdaterZip.h"

namespace bmlupdater {
    namespace {
        std::string HashFileHex(const std::wstring &path) {
            std::array<uint8_t, 32> digest{};
            if (!utils::Sha256File(path, digest.data())) {
                return {};
            }
            return BytesToHex(digest.data(), digest.size());
        }

        bool IsPreserved(const UpdaterManifest &manifest, std::string_view path) {
            const std::string lower = ToLowerAscii(std::string(path));
            for (const std::string &preserve : manifest.preserve) {
                const std::string preserveLower = ToLowerAscii(preserve);
                if (lower == preserveLower || PathMatchesPrefix(lower, preserveLower + "/")) {
                    return true;
                }
            }
            return false;
        }

        const ManagedFile *FindManaged(const UpdaterManifest &manifest, std::string_view path) {
            const std::string lower = ToLowerAscii(std::string(path));
            for (const ManagedFile &file : manifest.managedFiles) {
                if (ToLowerAscii(file.path) == lower) {
                    return &file;
                }
            }
            return nullptr;
        }

        std::wstring LatestTransactionWithRollback(const std::wstring &stateRoot) {
            const std::wstring txRoot = JoinPath(stateRoot, L"transactions");
            if (!DirectoryExists(txRoot)) {
                return {};
            }

            std::wstring latest;
            WIN32_FIND_DATAW data{};
            const std::wstring pattern = JoinPath(txRoot, L"*");
            HANDLE find = FindFirstFileW(pattern.c_str(), &data);
            if (find == INVALID_HANDLE_VALUE) {
                return {};
            }
            do {
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
                    wcscmp(data.cFileName, L".") == 0 ||
                    wcscmp(data.cFileName, L"..") == 0) {
                    continue;
                }
                const std::wstring candidate = JoinPath(txRoot, data.cFileName);
                if (RegularFileExists(JoinPath(candidate, L"rollback.tsv"))) {
                    if (latest.empty() || FileName(candidate) > FileName(latest)) {
                        latest = candidate;
                    }
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
            return latest;
        }

        bool CopyFileWithParents(const std::wstring &from,
                                 const std::wstring &to,
                                 std::string &error) {
            if (!CreateDirectories(ParentPath(to))) {
                error = "Unable to create directory: " + PathUtf8(ParentPath(to));
                return false;
            }
            if (CopyFileW(from.c_str(), to.c_str(), FALSE) == FALSE) {
                error = "Unable to copy " + PathUtf8(from) + " to " + PathUtf8(to);
                return false;
            }
            return true;
        }

        Result PermissionFailure(const std::wstring &path, std::string action) {
            return Result::Failure(
                "Permission denied while checking " + action + ": " + PathUtf8(path) +
                ". Close Ballance if it is running. If the game directory is protected, restart Updater.exe as administrator and retry only the apply/rollback step.");
        }

        bool CanCreateFileInDirectory(const std::wstring &directory) {
            if (!CreateDirectories(directory)) {
                return false;
            }

            DWORD pid = GetCurrentProcessId();
            DWORD tick = GetTickCount();
            std::wstring probe = JoinPath(directory, L".bml-updater-write-test-" + std::to_wstring(pid) + L"-" + std::to_wstring(tick) + L".tmp");
            HANDLE handle = CreateFileW(
                probe.c_str(),
                GENERIC_WRITE | DELETE,
                0,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                nullptr);
            if (handle == INVALID_HANDLE_VALUE) {
                return false;
            }
            CloseHandle(handle);
            std::string ignored;
            (void)RemoveFileIfPresent(probe, ignored);
            return true;
        }

        bool CanOpenExistingFileForWrite(const std::wstring &path, DWORD access) {
            if (!PathExists(path)) {
                return true;
            }
            HANDLE handle = CreateFileW(
                path.c_str(),
                access,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (handle == INVALID_HANDLE_VALUE) {
                return false;
            }
            CloseHandle(handle);
            return true;
        }

        Result CheckTargetWritable(const ApplyOperation &op) {
            if (!CanCreateFileInDirectory(ParentPath(op.targetPath))) {
                return PermissionFailure(ParentPath(op.targetPath), "target directory write access");
            }

            if (op.kind == OperationKind::InstallOrReplace) {
                if (!CanOpenExistingFileForWrite(op.targetPath, GENERIC_WRITE)) {
                    return PermissionFailure(op.targetPath, "target file write access");
                }
            } else {
                if (!CanOpenExistingFileForWrite(op.targetPath, DELETE)) {
                    return PermissionFailure(op.targetPath, "target file delete access");
                }
            }
            return Result::Success();
        }

        bool IsProcessElevated() {
            HANDLE token = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
                return false;
            }

            TOKEN_ELEVATION elevation{};
            DWORD size = 0;
            const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
            CloseHandle(token);
            return ok && elevation.TokenIsElevated != 0;
        }

        std::string StateJson(std::string_view version) {
            utils::MutableJsonDocument doc;
            yyjson_mut_val *root = doc.CreateObject();
            doc.SetRoot(root);
            (void)doc.AddString(root, "installedVersion", version);
            (void)doc.AddString(root, "lastError", "");
            std::string error;
            return doc.Write(true, error);
        }

        Result NormalizeSourceBaseUrl(std::string &baseUrl) {
            while (!baseUrl.empty() && baseUrl.back() == '/') {
                baseUrl.pop_back();
            }
            if (baseUrl.empty()) {
                return Result::Failure("source baseUrl must not be empty");
            }
            if (!baseUrl.starts_with("https://")) {
                return Result::Failure("source baseUrl must use HTTPS");
            }
            for (const char c : baseUrl) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    return Result::Failure("source baseUrl must not contain whitespace");
                }
            }
            return Result::Success();
        }

        std::string RemoteSessionJson(const RemoteUpdateInfo &info,
                                      std::string_view manifestSha256,
                                      std::string_view packageSha256) {
            utils::MutableJsonDocument doc;
            yyjson_mut_val *root = doc.CreateObject();
            doc.SetRoot(root);
            (void)doc.AddInt(root, "schemaVersion", 1);
            (void)doc.AddString(root, "channel", info.channel);
            (void)doc.AddString(root, "version", info.latestVersion);
            (void)doc.AddString(root, "channelUrl", info.channelUrl);
            (void)doc.AddString(root, "packageUrl", info.packageUrl);
            (void)doc.AddString(root, "manifestUrl", info.manifestUrl);
            (void)doc.AddString(root, "manifestSha256", manifestSha256);
            (void)doc.AddString(root, "packageSha256", packageSha256);
            std::string error;
            return doc.Write(true, error);
        }

        std::string SourcesJson(const UpdaterSourceConfig &config) {
            utils::MutableJsonDocument doc;
            yyjson_mut_val *root = doc.CreateObject();
            doc.SetRoot(root);
            (void)doc.AddInt(root, "schemaVersion", 1);
            (void)doc.AddString(root, "baseUrl", config.baseUrl);
            (void)doc.AddString(root, "defaultChannel", config.defaultChannel);
            std::string error;
            return doc.Write(true, error);
        }

        std::string PendingJson(const std::wstring &transactionRoot) {
            utils::MutableJsonDocument doc;
            yyjson_mut_val *root = doc.CreateObject();
            doc.SetRoot(root);
            (void)doc.AddString(root, "transactionRoot", PathUtf8(transactionRoot));
            std::string error;
            return doc.Write(true, error);
        }

        bool AppendRollbackForPath(std::string &rollback,
                                   const std::wstring &source,
                                   const std::wstring &target,
                                   const std::wstring &backupRoot,
                                   std::string_view backupName,
                                   std::string &error) {
            const std::wstring backupPath = JoinPath(backupRoot, utils::Utf8ToUtf16(std::string(backupName)));
            if (PathExists(source)) {
                if (!CopyFileWithParents(source, backupPath, error)) {
                    return false;
                }
                rollback += "restore\t" + PathUtf8(backupPath) + "\t" + PathUtf8(target) + "\n";
            } else {
                rollback += "delete\t\t" + PathUtf8(target) + "\n";
            }
            return true;
        }

        std::string QueryVersionResourceString(const std::wstring &path, const wchar_t *name) {
            DWORD handle = 0;
            const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
            if (size == 0) {
                return {};
            }

            std::vector<uint8_t> data(size);
            if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
                return {};
            }

            struct Translation {
                WORD language;
                WORD codePage;
            };

            Translation *translations = nullptr;
            UINT translationBytes = 0;
            if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                                reinterpret_cast<void **>(&translations), &translationBytes) ||
                translationBytes < sizeof(Translation)) {
                return {};
            }

            const Translation translation = translations[0];
            wchar_t subBlock[128] = {};
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\%s",
                       translation.language, translation.codePage, name);

            wchar_t *value = nullptr;
            UINT valueChars = 0;
            if (!VerQueryValueW(data.data(), subBlock, reinterpret_cast<void **>(&value), &valueChars) ||
                value == nullptr || valueChars == 0) {
                return {};
            }

            return utils::Utf16ToUtf8(value);
        }

        std::string DetectInstalledVersionFromRuntime(const std::wstring &gameRoot) {
            const std::wstring bmlDll = JoinPath(JoinPath(gameRoot, L"BuildingBlocks"), L"BMLPlus.dll");
            std::string version = QueryVersionResourceString(bmlDll, L"ProductVersion");
            if (version.empty()) {
                version = QueryVersionResourceString(bmlDll, L"FileVersion");
            }
            return version;
        }

        std::vector<std::array<std::string, 3>> ParseRollbackRows(std::string_view text) {
            std::vector<std::array<std::string, 3>> rows;
            std::stringstream stream{std::string(text)};
            std::string line;
            while (std::getline(stream, line)) {
                std::array<std::string, 3> row{};
                size_t start = 0;
                for (size_t i = 0; i < 3; ++i) {
                    const size_t tab = line.find('\t', start);
                    row[i] = line.substr(start, tab == std::string::npos ? std::string::npos : tab - start);
                    if (tab == std::string::npos) break;
                    start = tab + 1;
                }
                rows.push_back(std::move(row));
            }
            return rows;
        }
    } // namespace

    UpdaterService::UpdaterService(UpdaterContext context) : m_Context(std::move(context)) {
        if (m_Context.updaterStateRoot.empty()) {
            m_Context.updaterStateRoot = DefaultStateRoot(m_Context.gameRoot);
        }
    }

    const UpdaterContext &UpdaterService::Context() const noexcept {
        return m_Context;
    }

    StatusInfo UpdaterService::GetStatus() const {
        StatusInfo info;
        info.installedVersion = "unknown";
        info.pendingTransaction = PathExists(PendingFile());
        std::string error;
        utils::JsonDocument doc = utils::JsonDocument::ParseFile(StateFile(), error);
        if (doc.IsValid() && yyjson_is_obj(doc.Root())) {
            info.installedVersion = utils::JsonGetString(doc.Root(), "installedVersion", "unknown");
            info.lastError = utils::JsonGetString(doc.Root(), "lastError", "");
        }
        if (info.installedVersion == "unknown") {
            utils::JsonDocument manifestDoc = utils::JsonDocument::ParseFile(InstalledManifestFile(), error);
            if (manifestDoc.IsValid() && yyjson_is_obj(manifestDoc.Root())) {
                info.installedVersion = utils::JsonGetString(manifestDoc.Root(), "version", "unknown");
            }
        }
        if (info.installedVersion == "unknown") {
            const std::string detectedVersion = DetectInstalledVersionFromRuntime(m_Context.gameRoot);
            if (!detectedVersion.empty()) {
                info.installedVersion = detectedVersion;
            }
        }
        return info;
    }

    Result UpdaterService::RunDoctor(std::vector<std::string> &diagnostics) const {
        diagnostics.clear();
        diagnostics.push_back("gameRoot=" + PathUtf8(m_Context.gameRoot));
        diagnostics.push_back("stateRoot=" + PathUtf8(m_Context.updaterStateRoot));
        diagnostics.push_back(std::string("elevated=") + (IsProcessElevated() ? "true" : "false"));
        if (!PathExists(m_Context.gameRoot)) {
            return Result::Failure("Game root does not exist");
        }
        if (!CreateDirectories(m_Context.updaterStateRoot)) {
            return Result::Failure("Unable to create updater state root");
        }
        diagnostics.push_back(PathExists(InstalledManifestFile()) ? "installed manifest found" : "installed manifest missing");
        diagnostics.push_back(PathExists(PendingFile()) ? "pending transaction hint found" : "no pending transaction hint");
        diagnostics.push_back(PathExists(SourcesFile()) ? "remote source configured" : "remote source not configured");
        return Result::Success("doctor passed");
    }

    Result UpdaterService::GetSourceConfig(UpdaterSourceConfig &config) const {
        config = {};
        if (!PathExists(SourcesFile())) {
            return Result::Success("no remote source configured");
        }

        std::string error;
        utils::JsonDocument sourcesDoc = utils::JsonDocument::ParseFile(SourcesFile(), error);
        if (!sourcesDoc.IsValid() || !yyjson_is_obj(sourcesDoc.Root())) {
            return Result::Failure("Unable to parse sources.json: " + error);
        }

        const int64_t schemaVersion = utils::JsonGetInt(sourcesDoc.Root(), "schemaVersion", 1);
        if (schemaVersion != 1) {
            return Result::Failure("Unsupported sources.json schemaVersion");
        }

        config.baseUrl = utils::JsonGetString(sourcesDoc.Root(), "baseUrl", "");
        config.defaultChannel = utils::JsonGetString(sourcesDoc.Root(), "defaultChannel", "stable");
        Result normalized = NormalizeSourceBaseUrl(config.baseUrl);
        if (!normalized.ok) {
            return normalized;
        }
        Result channel = ValidateChannelName(config.defaultChannel);
        if (!channel.ok) {
            return channel;
        }
        return Result::Success("remote source configured");
    }

    Result UpdaterService::SetSourceConfig(UpdaterSourceConfig config) const {
        Result normalized = NormalizeSourceBaseUrl(config.baseUrl);
        if (!normalized.ok) {
            return normalized;
        }
        Result channel = ValidateChannelName(config.defaultChannel);
        if (!channel.ok) {
            return channel;
        }
        if (!CreateDirectories(m_Context.updaterStateRoot)) {
            return Result::Failure("Unable to create updater state root");
        }
        std::string error;
        const std::string json = SourcesJson(config);
        if (json.empty() || !WriteTextFile(SourcesFile(), json, error)) {
            return Result::Failure(error.empty() ? "Unable to write sources.json" : error);
        }
        return Result::Success("remote source configured");
    }

    Result UpdaterService::GetSourceBaseUrl(std::string &baseUrl) const {
        UpdaterSourceConfig config;
        Result result = GetSourceConfig(config);
        baseUrl = config.baseUrl;
        return result;
    }

    Result UpdaterService::SetSourceBaseUrl(std::string baseUrl, std::string defaultChannel) const {
        return SetSourceConfig({std::move(baseUrl), std::move(defaultChannel)});
    }

    Result UpdaterService::ClearSource() const {
        std::string error;
        if (!RemoveFileIfPresent(SourcesFile(), error)) {
            return Result::Failure(error);
        }
        return Result::Success("remote source cleared");
    }

    Result UpdaterService::CheckRemote(const std::string &channel,
                                       bool force,
                                       RemoteUpdateInfo &info,
                                       std::vector<std::string> &diagnostics) const {
        info = {};
        diagnostics.clear();
        UpdaterSourceConfig config;
        Result source = GetSourceConfig(config);
        if (!source.ok) {
            return source;
        }
        const std::string selectedChannel = channel.empty() ? config.defaultChannel : channel;
        Result channelResult = ValidateChannelName(selectedChannel);
        if (!channelResult.ok) {
            return channelResult;
        }
        diagnostics.push_back("channel=" + selectedChannel);
        info.channel = selectedChannel;

        if (config.baseUrl.empty()) {
            diagnostics.push_back("sources.json is missing; local package verification/apply is available");
            return Result::Success("no remote source configured");
        }

        const std::string channelUrl = config.baseUrl + "/" + selectedChannel + ".json";
        const std::string signatureUrl = channelUrl + ".sig";
        info.channelUrl = channelUrl;
        info.channelSignatureUrl = signatureUrl;
        diagnostics.push_back("fetching " + channelUrl);

        std::string channelJson;
        Result fetched = HttpGetUtf8(utils::Utf8ToUtf16(channelUrl), channelJson);
        if (!fetched.ok) {
            return fetched;
        }
        std::string signatureText;
        fetched = HttpGetUtf8(utils::Utf8ToUtf16(signatureUrl), signatureText);
        if (!fetched.ok) {
            return fetched;
        }
        info.channelJson = channelJson;
        info.channelSignature = signatureText;
        Result verified = VerifyDetachedSignature(channelJson, signatureText);
        if (!verified.ok) {
            return verified;
        }

        const std::optional<UpdaterManifest> installed = LoadInstalledManifest();
        const std::string trustedInstalledVersion = installed ? installed->version : std::string();
        Result parsed = ParseRemoteChannelJson(
            channelJson,
            selectedChannel,
            channelUrl,
            signatureUrl,
            force,
            trustedInstalledVersion,
            info);
        if (!parsed.ok) {
            return parsed;
        }
        info.channelJson = channelJson;
        info.channelSignature = signatureText;

        const StatusInfo status = GetStatus();
        diagnostics.push_back("installedVersion=" + status.installedVersion);
        diagnostics.push_back("latestVersion=" + info.latestVersion);
        diagnostics.push_back(std::string("updateAvailable=") + (info.updateAvailable ? "true" : "false"));
        if (info.sameTrustedVersion && !force) {
            diagnostics.push_back("skipReason=same trusted installed version; use --force to reinstall");
        }
        diagnostics.push_back("packageUrl=" + info.packageUrl);
        diagnostics.push_back("manifestUrl=" + info.manifestUrl);
        diagnostics.push_back("channel signature verified");
        return Result::Success("remote channel checked");
    }

    Result UpdaterService::CheckForUpdates(const std::string &channel, std::vector<std::string> &diagnostics) const {
        RemoteUpdateInfo info;
        return CheckRemote(channel, false, info, diagnostics);
    }

    Result UpdaterService::DownloadRemote(const RemoteUpdateInfo &info,
                                          LocalPackageVerification &verification,
                                          ProgressCallback progress) const {
        verification = {};
        if (info.channel.empty() || info.latestVersion.empty() || info.packageUrl.empty() || info.manifestUrl.empty()) {
            return Result::Failure("No remote update selected");
        }
        if (!info.updateAvailable) {
            return Result::Success("remote update skipped");
        }

        const std::wstring downloadRoot = JoinPath(
            JoinPath(JoinPath(m_Context.updaterStateRoot, L"downloads"), utils::Utf8ToUtf16(info.channel)),
            utils::Utf8ToUtf16(info.latestVersion));
        std::string error;
        if (!RemoveDirectoryTree(downloadRoot, error) || !CreateDirectories(downloadRoot)) {
            return Result::Failure(error.empty() ? "Unable to create remote download root: " + PathUtf8(downloadRoot) : error);
        }

        if (!WriteTextFile(JoinPath(downloadRoot, L"channel.json"), info.channelJson, error) ||
            !WriteTextFile(JoinPath(downloadRoot, L"channel.json.sig"), info.channelSignature, error)) {
            return Result::Failure(error.empty() ? "Unable to write remote channel metadata" : error);
        }

        if (progress) progress("downloading manifest");
        std::string manifestJson;
        Result fetched = HttpGetUtf8(utils::Utf8ToUtf16(info.manifestUrl), manifestJson);
        if (!fetched.ok) {
            return fetched;
        }
        if (progress) progress("downloading manifest signature");
        std::string manifestSignature;
        fetched = HttpGetUtf8(utils::Utf8ToUtf16(info.manifestSignatureUrl), manifestSignature);
        if (!fetched.ok) {
            return fetched;
        }
        if (progress) progress("verifying manifest signature");
        Result manifestSignatureResult = VerifyDetachedSignature(manifestJson, manifestSignature);
        if (!manifestSignatureResult.ok) {
            return manifestSignatureResult;
        }

        UpdaterManifest manifest;
        Result parsed = ParseManifestJson(manifestJson, manifest);
        if (!parsed.ok) {
            return parsed;
        }
        if (manifest.version != info.latestVersion) {
            return Result::Failure("Remote manifest version does not match channel version");
        }
        if (FileNameFromUrl(info.packageUrl) != manifest.packageFileName) {
            return Result::Failure("Channel packageUrl file name does not match manifest package fileName");
        }

        const std::wstring packagePath = JoinPath(downloadRoot, utils::Utf8ToUtf16(manifest.packageFileName));
        const std::wstring manifestPath = DeriveManifestPath(packagePath);
        const std::wstring signaturePath = DeriveSignaturePath(manifestPath);
        if (!WriteTextFile(manifestPath, manifestJson, error) ||
            !WriteTextFile(signaturePath, manifestSignature, error)) {
            return Result::Failure(error.empty() ? "Unable to write remote manifest metadata" : error);
        }

        const std::string manifestHash = HashFileHex(manifestPath);
        if (manifestHash.empty()) {
            return Result::Failure("Unable to hash remote manifest");
        }
        if (ContainsAsciiCaseInsensitive(info.revokedManifestHashes, manifestHash)) {
            return Result::Failure("Remote manifest hash is revoked: " + manifestHash);
        }

        if (progress) progress("downloading package");
        Result downloaded = HttpDownloadFile(utils::Utf8ToUtf16(info.packageUrl), packagePath, progress);
        if (!downloaded.ok) {
            return downloaded;
        }
        if (progress) progress("verifying package");
        const std::string packageHash = HashFileHex(packagePath);
        if (packageHash.empty()) {
            return Result::Failure("Unable to hash remote package");
        }
        if (packageHash != ToLowerAscii(manifest.packageSha256)) {
            return Result::Failure("Remote package hash does not match manifest");
        }

        const std::string sessionJson = RemoteSessionJson(info, manifestHash, packageHash);
        if (!sessionJson.empty()) {
            (void)WriteTextFile(JoinPath(downloadRoot, L"remote-session.json"), sessionJson, error);
        }

        return VerifyLocalPackage(packagePath, verification, progress);
    }

    Result UpdaterService::VerifyLocalPackage(const std::wstring &packagePath,
                                              LocalPackageVerification &verification,
                                              ProgressCallback progress) const {
        verification = {};
        if (!PathExists(packagePath)) {
            return Result::Failure("Package does not exist: " + PathUtf8(packagePath));
        }
        verification.manifestPath = DeriveManifestPath(packagePath);
        verification.signaturePath = DeriveSignaturePath(verification.manifestPath);
        if (!PathExists(verification.manifestPath)) {
            return Result::Failure("Manifest is missing: " + PathUtf8(verification.manifestPath));
        }
        if (!PathExists(verification.signaturePath)) {
            return Result::Failure("Manifest signature is missing: " + PathUtf8(verification.signaturePath));
        }

        if (progress) progress("verifying manifest signature");
        Result signature = VerifyManifestSignature(verification.manifestPath, verification.signaturePath);
        if (!signature.ok) {
            return signature;
        }

        Result loaded = LoadManifestFile(verification.manifestPath, verification.manifest);
        if (!loaded.ok) {
            return loaded;
        }
        if (verification.manifest.packageFileName != utils::Utf16ToUtf8(FileName(packagePath))) {
            return Result::Failure("Manifest package fileName does not match package path");
        }

        if (progress) progress("hashing package");
        const std::string packageHash = HashFileHex(packagePath);
        if (packageHash.empty()) {
            return Result::Failure("Unable to hash package");
        }
        if (packageHash != ToLowerAscii(verification.manifest.packageSha256)) {
            return Result::Failure("Package hash does not match manifest");
        }

        verification.stagingRoot = JoinPath(JoinPath(m_Context.updaterStateRoot, L"staging"), utils::Utf8ToUtf16(verification.manifest.version));
        if (progress) progress("extracting package to staging");
        Result extracted = ExtractUpdaterZipToStaging(packagePath, verification.manifest, verification.stagingRoot, verification.stagedFiles, progress);
        if (!extracted.ok) {
            return extracted;
        }
        return Result::Success("local package verified");
    }

    Result UpdaterService::BuildApplyPlan(const LocalPackageVerification &verification, ApplyPlan &plan) const {
        plan = {};
        plan.version = verification.manifest.version;

        std::unordered_map<std::string, StagedFile> staged;
        for (const StagedFile &file : verification.stagedFiles) {
            staged[ToLowerAscii(file.relativePath)] = file;
        }

        for (const ManagedFile &file : verification.manifest.managedFiles) {
            std::string error;
            auto normalized = NormalizeArchivePath(file.path, error);
            if (!normalized) {
                return Result::Failure(error);
            }
            std::wstring target;
            if (!ResolveUnderRoot(m_Context.gameRoot, normalized->wideRelative, target, error)) {
                return Result::Failure(error);
            }
            auto stagedIt = staged.find(ToLowerAscii(file.path));
            if (stagedIt == staged.end()) {
                return Result::Failure("Staged file missing for managed path: " + file.path);
            }
            plan.operations.push_back({
                OperationKind::InstallOrReplace,
                file.path,
                stagedIt->second.stagedPath,
                target,
                {},
                {},
                file.sha256});
        }

        const std::optional<UpdaterManifest> previous = LoadInstalledManifest();
        for (const RemoveFile &remove : verification.manifest.removeFiles) {
            if (!previous) {
                plan.diagnostics.push_back("Skipping removeFiles without a previous trusted manifest: " + remove.path);
                continue;
            }
            if (IsPreserved(verification.manifest, remove.path)) {
                plan.diagnostics.push_back("Skipping preserved removeFiles entry: " + remove.path);
                continue;
            }
            const ManagedFile *previousFile = FindManaged(*previous, remove.path);
            if (!previousFile) {
                plan.diagnostics.push_back("Skipping removeFiles entry not in previous trusted manifest: " + remove.path);
                continue;
            }
            std::string expectedHash = remove.sha256.empty() ? previousFile->sha256 : remove.sha256;
            if (expectedHash.empty()) {
                plan.diagnostics.push_back("Skipping removeFiles entry without trusted hash: " + remove.path);
                continue;
            }
            std::string error;
            auto normalized = NormalizeArchivePath(remove.path, error);
            if (!normalized) {
                return Result::Failure(error);
            }
            std::wstring target;
            if (!ResolveUnderRoot(m_Context.gameRoot, normalized->wideRelative, target, error)) {
                return Result::Failure(error);
            }
            if (!PathExists(target)) {
                plan.diagnostics.push_back("Skipping removeFiles entry because target is absent: " + remove.path);
                continue;
            }
            if (!RegularFileExists(target)) {
                plan.diagnostics.push_back("Skipping removeFiles non-regular file: " + remove.path);
                continue;
            }
            const std::string actualHash = HashFileHex(target);
            if (actualHash != ToLowerAscii(expectedHash)) {
                plan.diagnostics.push_back("Skipping removeFiles entry because hash changed: " + remove.path);
                continue;
            }
            plan.operations.push_back({
                OperationKind::Remove,
                remove.path,
                {},
                target,
                {},
                expectedHash,
                {}});
        }

        return Result::Success("apply plan built");
    }

    Result UpdaterService::CheckApplyAccess(const ApplyPlan &plan) const {
        if (!CanCreateFileInDirectory(m_Context.updaterStateRoot)) {
            return PermissionFailure(m_Context.updaterStateRoot, "updater state write access");
        }
        if (!CanCreateFileInDirectory(JoinPath(m_Context.updaterStateRoot, L"transactions"))) {
            return PermissionFailure(JoinPath(m_Context.updaterStateRoot, L"transactions"), "transaction write access");
        }

        for (const ApplyOperation &op : plan.operations) {
            Result writable = CheckTargetWritable(op);
            if (!writable.ok) {
                return writable;
            }
        }
        return Result::Success("apply access check passed");
    }

    Result UpdaterService::Apply(const LocalPackageVerification &verification,
                                 const ApplyPlan &plan,
                                 ProgressCallback progress) const {
        Result access = CheckApplyAccess(plan);
        if (!access.ok) {
            return access;
        }

        if (!CreateDirectories(m_Context.updaterStateRoot)) {
            return Result::Failure("Unable to create updater state root");
        }
        const std::wstring transaction = MakeTransactionRoot(m_Context);
        const std::wstring backupRoot = JoinPath(transaction, L"backup");
        if (!CreateDirectories(backupRoot)) {
            return Result::Failure("Unable to create transaction backup root");
        }

        std::string error;
        if (!WriteTextFile(PendingFile(), PendingJson(transaction), error)) {
            return Result::Failure(error);
        }

        std::string rollback;

        auto rollbackNow = [&]() {
            (void)Rollback(progress);
            (void)RemoveFileIfPresent(PendingFile(), error);
        };

        for (const ApplyOperation &op : plan.operations) {
            if (progress) progress("backing up " + op.relativePath);
            std::string pathError;
            auto normalized = NormalizeArchivePath(op.relativePath, pathError);
            if (!normalized) {
                rollbackNow();
                return Result::Failure(pathError);
            }
            const std::wstring backupPath = JoinPath(backupRoot, normalized->wideRelative);
            if (PathExists(op.targetPath)) {
                if (!CopyFileWithParents(op.targetPath, backupPath, error)) {
                    rollbackNow();
                    return Result::Failure(error);
                }
                rollback += "restore\t" + PathUtf8(backupPath) + "\t" + PathUtf8(op.targetPath) + "\n";
            } else {
                rollback += "delete\t\t" + PathUtf8(op.targetPath) + "\n";
            }
        }
        if (progress) progress("backing up updater state");
        if (!AppendRollbackForPath(rollback, StateFile(), StateFile(), backupRoot, "state.json", error) ||
            !AppendRollbackForPath(rollback, InstalledManifestFile(), InstalledManifestFile(), backupRoot, "installed.manifest.json", error)) {
            rollbackNow();
            return Result::Failure(error);
        }
        if (!WriteTextFile(JoinPath(transaction, L"rollback.tsv"), rollback, error)) {
            rollbackNow();
            return Result::Failure(error);
        }

        for (const ApplyOperation &op : plan.operations) {
            if (op.kind == OperationKind::InstallOrReplace) {
                if (progress) progress("installing " + op.relativePath);
                if (!CopyFileWithParents(op.sourcePath, op.targetPath, error)) {
                    rollbackNow();
                    return Result::Failure(error);
                }
            } else {
                if (progress) progress("removing " + op.relativePath);
                if (!RemoveFileIfPresent(op.targetPath, error)) {
                    rollbackNow();
                    return Result::Failure(error);
                }
            }
        }

        std::string manifestError;
        const std::string manifestJson = WriteManifestJson(verification.manifest, manifestError);
        if (manifestJson.empty() || !WriteTextFile(InstalledManifestFile(), manifestJson, error)) {
            rollbackNow();
            return Result::Failure(manifestError.empty() ? error : manifestError);
        }
        if (!WriteTextFile(StateFile(), StateJson(verification.manifest.version), error)) {
            rollbackNow();
            return Result::Failure(error);
        }
        (void)RemoveFileIfPresent(PendingFile(), error);
        if (progress) progress("apply complete");
        return Result::Success("apply complete");
    }

    Result UpdaterService::ApplyRemote(const std::string &channel,
                                       bool force,
                                       ProgressCallback progress) const {
        std::vector<std::string> diagnostics;
        RemoteUpdateInfo info;
        Result checked = CheckRemote(channel, force, info, diagnostics);
        for (const std::string &diagnostic : diagnostics) {
            if (progress) progress(diagnostic);
        }
        if (!checked.ok) {
            return checked;
        }
        if (info.latestVersion.empty()) {
            return Result::Success("no remote source configured");
        }
        if (!info.updateAvailable) {
            return Result::Success("remote update skipped");
        }

        LocalPackageVerification verification;
        Result downloaded = DownloadRemote(info, verification, progress);
        if (!downloaded.ok) {
            return downloaded;
        }
        if (!verification.manifest.managedFiles.empty()) {
            if (progress) progress("building apply plan");
        }
        ApplyPlan plan;
        Result planned = BuildApplyPlan(verification, plan);
        if (!planned.ok) {
            return planned;
        }
        for (const std::string &diagnostic : plan.diagnostics) {
            if (progress) progress(diagnostic);
        }
        if (progress) progress("checking write access");
        Result access = CheckApplyAccess(plan);
        if (!access.ok) {
            return access;
        }
        return Apply(verification, plan, progress);
    }

    Result UpdaterService::Update(const std::string &channel,
                                  bool force,
                                  ProgressCallback progress) const {
        return ApplyRemote(channel, force, std::move(progress));
    }

    Result UpdaterService::CheckRollbackAccess() const {
        if (!CanCreateFileInDirectory(m_Context.updaterStateRoot)) {
            return PermissionFailure(m_Context.updaterStateRoot, "updater state write access");
        }

        const std::wstring transaction = LatestTransactionWithRollback(m_Context.updaterStateRoot);
        if (transaction.empty()) {
            return Result::Failure("No rollback transaction found");
        }
        const std::string rollback = ReadTextFile(JoinPath(transaction, L"rollback.tsv"));
        if (rollback.empty()) {
            return Result::Failure("Unable to read rollback metadata");
        }

        for (const auto &row : ParseRollbackRows(rollback)) {
            const std::wstring target = utils::Utf8ToUtf16(row[2]);
            if (row[0] == "restore") {
                if (!CanCreateFileInDirectory(ParentPath(target))) {
                    return PermissionFailure(ParentPath(target), "rollback target directory write access");
                }
                if (!CanOpenExistingFileForWrite(target, GENERIC_WRITE)) {
                    return PermissionFailure(target, "rollback target file write access");
                }
            } else if (row[0] == "delete") {
                if (!CanOpenExistingFileForWrite(target, DELETE)) {
                    return PermissionFailure(target, "rollback target file delete access");
                }
            }
        }
        return Result::Success("rollback access check passed");
    }

    Result UpdaterService::Rollback(ProgressCallback progress) const {
        Result access = CheckRollbackAccess();
        if (!access.ok) {
            return access;
        }

        const std::wstring transaction = LatestTransactionWithRollback(m_Context.updaterStateRoot);
        if (transaction.empty()) {
            return Result::Failure("No rollback transaction found");
        }
        const std::string rollback = ReadTextFile(JoinPath(transaction, L"rollback.tsv"));
        if (rollback.empty()) {
            return Result::Failure("Unable to read rollback metadata");
        }
        std::vector<std::array<std::string, 3>> rows = ParseRollbackRows(rollback);
        std::reverse(rows.begin(), rows.end());

        std::string error;
        for (const auto &row : rows) {
            const std::wstring target = utils::Utf8ToUtf16(row[2]);
            if (row[0] == "restore") {
                if (progress) progress("restoring " + PathUtf8(target));
                if (!CopyFileWithParents(utils::Utf8ToUtf16(row[1]), target, error)) {
                    return Result::Failure(error);
                }
            } else if (row[0] == "delete") {
                if (progress) progress("deleting " + PathUtf8(target));
                if (!RemoveFileIfPresent(target, error)) {
                    return Result::Failure(error);
                }
            }
        }
        (void)RemoveFileIfPresent(PendingFile(), error);
        if (progress) progress("rollback complete");
        return Result::Success("rollback complete");
    }

    std::wstring UpdaterService::StateFile() const {
        return JoinPath(m_Context.updaterStateRoot, L"state.json");
    }

    std::wstring UpdaterService::InstalledManifestFile() const {
        return JoinPath(m_Context.updaterStateRoot, L"installed.manifest.json");
    }

    std::wstring UpdaterService::PendingFile() const {
        return JoinPath(m_Context.updaterStateRoot, L"pending.json");
    }

    std::wstring UpdaterService::SourcesFile() const {
        return JoinPath(m_Context.updaterStateRoot, L"sources.json");
    }

    std::optional<UpdaterManifest> UpdaterService::LoadInstalledManifest() const {
        if (!PathExists(InstalledManifestFile())) {
            return std::nullopt;
        }
        UpdaterManifest manifest;
        Result result = LoadManifestFile(InstalledManifestFile(), manifest);
        if (!result.ok) {
            return std::nullopt;
        }
        return manifest;
    }

    UpdaterContext CreateDefaultContext() {
        std::wstring exe(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        while (length == exe.size()) {
            exe.resize(exe.size() * 2);
            length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        }
        exe.resize(length);
        const std::wstring bin = ParentPath(exe);
        wchar_t cwd[MAX_PATH]{};
        GetCurrentDirectoryW(MAX_PATH, cwd);
        const std::wstring gameRoot = FileName(bin) == L"Bin" ? ParentPath(bin) : std::wstring(cwd);
        return {gameRoot, DefaultStateRoot(gameRoot)};
    }
} // namespace bmlupdater
