#ifndef BML_OBJECT_REFERENCE_REGISTRY_H
#define BML_OBJECT_REFERENCE_REGISTRY_H

#include <cstdint>
#include <cstddef>
#include <limits>
#include <unordered_map>

#include "BML/Types.h"

/*
 * Private bookkeeping for object identities emitted by the built-in scene
 * reads.  CK_ID alone is not a lifetime token: a reset/load can recycle a
 * slot.  The ModManager invalidates a slot from Virtools' deletion callback
 * before its storage can be reused, so the next Make() receives a new
 * generation even when the allocator gives the replacement the same address.
 */
class ObjectReferenceRegistry {
public:
    BML_ObjectRef Make(uint32_t domain, uint32_t slot, const void *identity) {
        if (domain == 0 || slot == 0 || !identity)
            return {};

        Entry &entry = m_Entries[slot];
        if (entry.Identity != identity)
            entry = {identity, NextGeneration()};
        return {domain, slot, entry.Generation};
    }

    void Invalidate(uint32_t slot) {
        const auto found = m_Entries.find(slot);
        if (slot == 0 || found == m_Entries.end())
            return;
        m_Entries.erase(found);
    }

    void InvalidateAll() {
        m_Entries.clear();
    }

    bool Matches(uint32_t slot, uint32_t generation, const void *identity) const {
        const auto found = m_Entries.find(slot);
        return slot != 0 && generation != 0 && identity && found != m_Entries.end() &&
               found->second.Generation == generation && found->second.Identity == identity;
    }

    size_t Size() const { return m_Entries.size(); }

private:
    struct Entry {
        const void *Identity = nullptr;
        uint32_t Generation = 0;
    };

    uint32_t NextGeneration() {
        const uint32_t result = m_NextGeneration;
        m_NextGeneration = m_NextGeneration == (std::numeric_limits<uint32_t>::max)()
                               ? 1
                               : m_NextGeneration + 1;
        return result;
    }

    std::unordered_map<uint32_t, Entry> m_Entries;
    uint32_t m_NextGeneration = 1;
};

#endif // BML_OBJECT_REFERENCE_REGISTRY_H
