#ifndef BML_UPDATER_NETWORK_H
#define BML_UPDATER_NETWORK_H

#include <string>

#include "UpdaterTypes.h"

namespace bmlupdater {
    [[nodiscard]] Result HttpGetUtf8(const std::wstring &url, std::string &body);
    [[nodiscard]] Result HttpDownloadFile(const std::wstring &url,
                                          const std::wstring &destination,
                                          ProgressCallback progress = {});
}

#endif // BML_UPDATER_NETWORK_H
