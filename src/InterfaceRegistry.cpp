#include "InterfaceRegistry.h"

#include <cstring>

namespace BML {

int FindInterface(const InterfaceEntry *entries, std::size_t count, const char *interfaceId,
                  uint16_t majorVersion, const void **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (!interfaceId || interfaceId[0] == '\0')
        return BML_ERROR_INVALID_PARAMETER;
    if (!entries)
        return BML_ERROR_NOT_FOUND;

    // A wrong major version is reported apart from an unknown id, because the
    // two mean different things to a Mod: one loader is too old to carry the
    // capability at all, the other carries it in a shape this Mod cannot use.
    bool idExists = false;
    for (std::size_t index = 0; index < count; ++index) {
        const InterfaceEntry &entry = entries[index];
        if (!entry.Id || !entry.Interface || std::strcmp(entry.Id, interfaceId) != 0)
            continue;
        idExists = true;
        if (entry.MajorVersion != majorVersion)
            continue;
        *out = entry.Interface;
        return BML_OK;
    }
    return idExists ? BML_ERROR_VERSION_MISMATCH : BML_ERROR_NOT_FOUND;
}

} // namespace BML
