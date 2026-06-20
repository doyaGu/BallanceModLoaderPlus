#include "UpdaterRemote.h"

#include <cctype>

#include "JsonUtils.h"
#include "UpdaterPaths.h"

namespace bmlupdater {
    namespace {
        bool ReadRequiredStringArray(yyjson_val *root,
                                     const char *name,
                                     std::vector<std::string> &out,
                                     std::string &error) {
            out.clear();
            yyjson_val *array = yyjson_obj_get(root, name);
            if (!yyjson_is_arr(array)) {
                error = std::string("Channel JSON must include array: ") + name;
                return false;
            }
            size_t idx = 0;
            size_t max = 0;
            yyjson_val *value = nullptr;
            yyjson_arr_foreach(array, idx, max, value) {
                if (!yyjson_is_str(value)) {
                    error = std::string("Channel JSON array contains non-string value: ") + name;
                    return false;
                }
                out.emplace_back(yyjson_get_str(value));
            }
            return true;
        }
    } // namespace

    std::string FileNameFromUrl(std::string url) {
        const size_t query = url.find_first_of("?#");
        if (query != std::string::npos) {
            url.resize(query);
        }
        const size_t slash = url.find_last_of('/');
        return slash == std::string::npos ? url : url.substr(slash + 1);
    }

    bool ContainsAsciiCaseInsensitive(const std::vector<std::string> &values, std::string_view needle) {
        const std::string lowerNeedle = ToLowerAscii(std::string(needle));
        for (const std::string &value : values) {
            if (ToLowerAscii(value) == lowerNeedle) {
                return true;
            }
        }
        return false;
    }

    Result ValidateRemoteHttpsUrl(const std::string &url, std::string_view fieldName) {
        if (!url.starts_with("https://")) {
            return Result::Failure(std::string(fieldName) + " must use HTTPS");
        }
        for (const char c : url) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                return Result::Failure(std::string(fieldName) + " must not contain whitespace");
            }
        }
        return Result::Success();
    }

    Result ValidateChannelName(const std::string &channel) {
        if (channel == "stable" || channel == "beta") {
            return Result::Success();
        }
        return Result::Failure("channel must be stable or beta");
    }

    Result ParseRemoteChannelJson(std::string_view channelJson,
                                  const std::string &selectedChannel,
                                  const std::string &channelUrl,
                                  const std::string &channelSignatureUrl,
                                  bool force,
                                  const std::string &trustedInstalledVersion,
                                  RemoteUpdateInfo &info) {
        info = {};
        std::string error;
        utils::JsonDocument channelDoc = utils::JsonDocument::Parse(channelJson, error);
        if (!channelDoc.IsValid() || !yyjson_is_obj(channelDoc.Root())) {
            return Result::Failure("Unable to parse channel JSON: " + error);
        }
        const std::string latestVersion = utils::JsonGetString(channelDoc.Root(), "version", "");
        const std::string packageUrl = utils::JsonGetString(channelDoc.Root(), "packageUrl", "");
        const std::string manifestUrl = utils::JsonGetString(channelDoc.Root(), "manifestUrl", "");
        if (latestVersion.empty() || packageUrl.empty() || manifestUrl.empty()) {
            return Result::Failure("Channel JSON must include version, packageUrl, and manifestUrl");
        }
        Result packageUrlResult = ValidateRemoteHttpsUrl(packageUrl, "packageUrl");
        if (!packageUrlResult.ok) {
            return packageUrlResult;
        }
        Result manifestUrlResult = ValidateRemoteHttpsUrl(manifestUrl, "manifestUrl");
        if (!manifestUrlResult.ok) {
            return manifestUrlResult;
        }
        if (FileNameFromUrl(packageUrl).empty() || FileNameFromUrl(manifestUrl).empty()) {
            return Result::Failure("Channel packageUrl and manifestUrl must include file names");
        }

        info.channel = selectedChannel;
        info.channelUrl = channelUrl;
        info.channelSignatureUrl = channelSignatureUrl;
        info.latestVersion = latestVersion;
        info.packageUrl = packageUrl;
        info.manifestUrl = manifestUrl;
        info.manifestSignatureUrl = manifestUrl + ".sig";
        if (!ReadRequiredStringArray(channelDoc.Root(), "revokedVersions", info.revokedVersions, error) ||
            !ReadRequiredStringArray(channelDoc.Root(), "revokedManifestHashes", info.revokedManifestHashes, error)) {
            return Result::Failure(error);
        }
        if (ContainsAsciiCaseInsensitive(info.revokedVersions, latestVersion)) {
            return Result::Failure("Latest channel version is revoked: " + latestVersion);
        }

        info.sameTrustedVersion = !trustedInstalledVersion.empty() && trustedInstalledVersion == latestVersion;
        info.updateAvailable = force || !info.sameTrustedVersion;
        return Result::Success("remote channel parsed");
    }
} // namespace bmlupdater
