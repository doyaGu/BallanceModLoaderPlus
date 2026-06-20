#ifndef BML_UPDATER_REMOTE_H
#define BML_UPDATER_REMOTE_H

#include <string>
#include <string_view>
#include <vector>

#include "UpdaterTypes.h"

namespace bmlupdater {
    [[nodiscard]] std::string FileNameFromUrl(std::string url);
    [[nodiscard]] bool ContainsAsciiCaseInsensitive(const std::vector<std::string> &values, std::string_view needle);
    [[nodiscard]] Result ValidateRemoteHttpsUrl(const std::string &url, std::string_view fieldName);
    [[nodiscard]] Result ValidateChannelName(const std::string &channel);
    [[nodiscard]] Result ParseRemoteChannelJson(std::string_view channelJson,
                                                const std::string &selectedChannel,
                                                const std::string &channelUrl,
                                                const std::string &channelSignatureUrl,
                                                bool force,
                                                const std::string &trustedInstalledVersion,
                                                RemoteUpdateInfo &info);
} // namespace bmlupdater

#endif // BML_UPDATER_REMOTE_H
