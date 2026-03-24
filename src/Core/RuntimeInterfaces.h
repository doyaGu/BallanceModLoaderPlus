#ifndef BML_CORE_RUNTIME_INTERFACES_H
#define BML_CORE_RUNTIME_INTERFACES_H

#include "bml_services.hpp"

namespace BML::Core {
    struct KernelServices;

    void RegisterRuntimeInterfaces(KernelServices &kernel);
    void RefreshRuntimeInterfaces(KernelServices &kernel);
} // namespace BML::Core

#endif /* BML_CORE_RUNTIME_INTERFACES_H */
