#ifndef BML_API_INTERFACE_REGISTRY_H
#define BML_API_INTERFACE_REGISTRY_H

#include "BML/Interface.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace BML::Api {

// Stores provider-owned interface structs. The registry copies identity and
// ownership strings but never copies or frees the interface itself.
class InterfaceRegistry final {
public:
    int Register(const std::string &ownerId, const void *interfacePtr);
    int Unregister(const std::string &ownerId, const char *interfaceId,
                   std::uint16_t majorVersion);
    int Find(const char *interfaceId, std::uint16_t majorVersion,
             const void **out) const;
    std::size_t CleanupOwner(const std::string &ownerId);

private:
    struct Entry {
        std::string OwnerId;
        std::string InterfaceId;
        std::uint16_t MajorVersion = 0;
        const void *Interface = nullptr;
    };

    mutable std::mutex m_Mutex;
    std::vector<Entry> m_Entries;
};

// ModContext calls this before releasing a provider DLL. It is intentionally
// internal: public callers unregister through BML_UnregisterInterface, where
// their DLL ownership is verified.
void UnregisterInterfacesForOwner(const std::string &ownerId) noexcept;

} // namespace BML::Api

#endif // BML_API_INTERFACE_REGISTRY_H
