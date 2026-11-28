#include "UpdaterService.h"

#include "UpdaterSdl.h"
#include "PathUtils.h"
#include "UpdaterTarget.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "CryptoUtils.h"
#include "JsonUtils.h"
#include "StringUtils.h"
#include "UpdaterDoctor.h"
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
        m_Context.gameRoot = utils::ResolvePathW(m_Context.gameRoot);
        if (!m_Context.updaterStateRoot.empty()) m_Context.updaterStateRoot = utils::ResolvePathW(m_Context.updaterStateRoot);
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
        std::vector<std::string> checks;
        Result result = RunDoctorChecks(m_Context, checks);
        diagnostics.insert(diagnostics.end(), checks.begin(), checks.end());
        return result;
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
        auto paths = CheckUpdaterPaths(m_Context);
        if (!paths.ok) return paths;
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
        auto paths = CheckUpdaterPaths(m_Context);
        if (!paths.ok) return paths;
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

        auto paths = CheckUpdaterPaths(m_Context);
        if (!paths.ok) return paths;
        UpdateTarget target;
        auto targetResult = ReadRuntimeTarget(m_Context, target);
        if (!targetResult.ok) return targetResult;
        const std::string channelUrl = config.baseUrl + "/" + target.runtime + "/" + target.platform + "/" +
            target.architecture + "/" + target.hostLinkage + "/" + selectedChannel + ".json";
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
        if (info.target != target) return Result::Failure("Remote channel target does not match installed runtime");
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
        auto paths = CheckUpdaterPaths(m_Context);
        if (!paths.ok) return paths;
        UpdateTarget target;
        auto targetResult = ReadRuntimeTarget(m_Context, target);
        if (!targetResult.ok) return targetResult;
        if (target != info.target || !IsValidUpdateVersion(info.latestVersion))
            return Result::Failure("Invalid remote update target/version");
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
        std::wstring checkedDownloadRoot;
        if (!ValidateChannelName(info.channel).ok || !ResolveUnderRoot(m_Context.updaterStateRoot,
            L"downloads/" + utils::Utf8ToUtf16(info.channel) + L"/" + utils::Utf8ToUtf16(info.latestVersion), checkedDownloadRoot, error))
            return Result::Failure("Invalid remote download path: " + error);
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
        auto packageTargetResult = CheckPackageTarget(m_Context, manifest);
        if (!packageTargetResult.ok) return packageTargetResult;
        if (manifest.target != info.target) return Result::Failure("Channel and manifest target mismatch");
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
        auto paths = CheckUpdaterPaths(m_Context);
        if (!paths.ok) return paths;
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
        auto targetResult = CheckPackageTarget(m_Context, verification.manifest);
        if (!targetResult.ok) return targetResult;
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
        std::string pathError;
        if (!ResolveUnderRoot(m_Context.updaterStateRoot, L"staging/" + utils::Utf8ToUtf16(verification.manifest.version),
                              verification.stagingRoot, pathError)) return Result::Failure(pathError);
        if (progress) progress("extracting package to staging");
        Result extracted = ExtractUpdaterZipToStaging(packagePath, verification.manifest, verification.stagingRoot, verification.stagedFiles, progress);
        if (!extracted.ok) {
            return extracted;
        }
        ApplyPlan checked;
        auto payload = BuildApplyPlan(verification, checked);
        if (!payload.ok) return payload;
        return Result::Success("local package verified");
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
        if (!VerifyManifestSignature(InstalledManifestFile(), InstalledManifestFile() + L".sig").ok)
            return std::nullopt;
        UpdaterManifest manifest;
        Result result = LoadManifestFile(InstalledManifestFile(), manifest);
        if (!result.ok) {
            return std::nullopt;
        }
        if (!CheckPackageTarget(m_Context, manifest).ok) return std::nullopt;
        return manifest;
    }


} // namespace bmlupdater
