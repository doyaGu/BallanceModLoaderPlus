#ifndef BML_INTERFACEREGISTRY_H
#define BML_INTERFACEREGISTRY_H

#include "BML/Interface.h"

#include <cstddef>

namespace BML {

// One row of the loader's interface table. Interface points at a static const
// interface struct, so a row costs nothing at runtime and can never go stale.
// Two rows may carry the same Id with different MajorVersion values, which is how
// an old major version stays reachable beside a new one.
struct InterfaceEntry {
    const char *Id;
    uint16_t MajorVersion;
    const void *Interface;
};

// The body of BML_GetInterface, kept apart from the table so it can be tested
// against a table of its own. Ids are compared by content, since the caller's
// string literal lives in the caller's own DLL.
int FindInterface(const InterfaceEntry *entries, std::size_t count, const char *interfaceId,
                  uint16_t majorVersion, const void **out);

} // namespace BML

#endif // BML_INTERFACEREGISTRY_H
