#ifndef BML_CORE_RESOURCE_API_H
#define BML_CORE_RESOURCE_API_H

#include <string>

#include "bml_resource.h"
#include "bml_extension.h"

namespace BML::Core {
    void RegisterResourceApis();
    BML_Result RegisterResourceType(const BML_ResourceTypeDesc *desc, BML_HandleType *out_type);
    void UnregisterResourceTypesForProvider(const std::string &provider_id);
} // namespace BML::Core

#endif // BML_CORE_RESOURCE_API_H
