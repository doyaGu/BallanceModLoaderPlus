#ifndef BML_CORE_BUILTIN_INTERFACES_H
#define BML_CORE_BUILTIN_INTERFACES_H

#include "bml_services.hpp"

namespace BML::Core {
    void RegisterBuiltinInterfaces();
    void PopulateBuiltinServices(bml::BuiltinServices &out);
} // namespace BML::Core

#endif /* BML_CORE_BUILTIN_INTERFACES_H */
