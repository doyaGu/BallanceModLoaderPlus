#ifndef BML_DIAGNOSTICS_SYSTEM_REPORT_H
#define BML_DIAGNOSTICS_SYSTEM_REPORT_H

#include <string>
#include <vector>

class ModContext;

namespace BML::Diagnostics {
    std::vector<std::string> BuildSystemReport(ModContext &context);
}

#endif // BML_DIAGNOSTICS_SYSTEM_REPORT_H
