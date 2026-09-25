#ifndef BML_UPDATER_DOCTOR_H
#define BML_UPDATER_DOCTOR_H

#include <string>
#include <vector>

#include "UpdaterTypes.h"

namespace bmlupdater {
    [[nodiscard]] Result RunDoctorChecks(const UpdaterContext &context,
                                         std::vector<std::string> &diagnostics);
}

#endif // BML_UPDATER_DOCTOR_H
