#ifndef BML_UPDATER_NETWORK_H
#define BML_UPDATER_NETWORK_H

#include <chrono>
#include <string>

#include "UpdaterTypes.h"

namespace bmlupdater {
    inline constexpr std::chrono::milliseconds DefaultHttpTimeout = std::chrono::minutes(10);

    [[nodiscard]] Result HttpGetUtf8(const std::wstring &url, std::string &body);
    [[nodiscard]] Result HttpDownloadFile(const std::wstring &url,
                                          const std::wstring &destination,
                                          ProgressCallback progress = {},
                                          std::chrono::milliseconds timeout = DefaultHttpTimeout);
}

#endif // BML_UPDATER_NETWORK_H
