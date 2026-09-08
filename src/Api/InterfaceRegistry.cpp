#include "Api/InterfaceRegistry.h"

#include <algorithm>
#include <cstring>

namespace BML::Api {

int InterfaceRegistry::Register(
    const std::string &ownerId, const void *interfacePtr) {
    if (ownerId.empty() || !interfacePtr)
        return BML_ERROR_INVALID_PARAMETER;

    const auto *header = static_cast<const BML_InterfaceHeader *>(interfacePtr);
    if (header->StructSize < sizeof(BML_InterfaceHeader) ||
        header->MajorVersion == 0 || !header->InterfaceId ||
        header->InterfaceId[0] == '\0') {
        return BML_ERROR_INVALID_PARAMETER;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto existing = std::find_if(
        m_Entries.begin(), m_Entries.end(), [header](const Entry &entry) {
            return entry.MajorVersion == header->MajorVersion &&
                   entry.InterfaceId == header->InterfaceId;
        });
    if (existing != m_Entries.end())
        return BML_ERROR_ALREADY_EXISTS;

    m_Entries.push_back({
        ownerId, header->InterfaceId, header->MajorVersion, interfacePtr});
    return BML_OK;
}

int InterfaceRegistry::Unregister(
    const std::string &ownerId, const char *interfaceId,
    std::uint16_t majorVersion) {
    if (ownerId.empty() || !interfaceId || interfaceId[0] == '\0' ||
        majorVersion == 0) {
        return BML_ERROR_INVALID_PARAMETER;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto existing = std::find_if(
        m_Entries.begin(), m_Entries.end(),
        [interfaceId, majorVersion](const Entry &entry) {
            return entry.MajorVersion == majorVersion &&
                   entry.InterfaceId == interfaceId;
        });
    if (existing == m_Entries.end())
        return BML_ERROR_NOT_FOUND;
    if (existing->OwnerId != ownerId)
        return BML_ERROR_ACCESS_DENIED;

    m_Entries.erase(existing);
    return BML_OK;
}

int InterfaceRegistry::Find(
    const char *interfaceId, std::uint16_t majorVersion,
    const void **out) const {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (!interfaceId || interfaceId[0] == '\0')
        return BML_ERROR_INVALID_PARAMETER;

    bool idExists = false;
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const Entry &entry : m_Entries) {
        if (entry.InterfaceId != interfaceId)
            continue;
        idExists = true;
        if (entry.MajorVersion != majorVersion)
            continue;
        *out = entry.Interface;
        return BML_OK;
    }
    return idExists ? BML_ERROR_VERSION_MISMATCH : BML_ERROR_NOT_FOUND;
}

std::size_t InterfaceRegistry::CleanupOwner(const std::string &ownerId) {
    if (ownerId.empty())
        return 0;
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::size_t before = m_Entries.size();
    m_Entries.erase(
        std::remove_if(m_Entries.begin(), m_Entries.end(),
                       [&ownerId](const Entry &entry) {
                           return entry.OwnerId == ownerId;
                       }),
        m_Entries.end());
    return before - m_Entries.size();
}

} // namespace BML::Api
