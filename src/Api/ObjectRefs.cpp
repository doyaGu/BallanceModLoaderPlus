#include "Api/ObjectRefs.h"

#include "CKGlobals.h"
#include "CKObject.h"

#include <limits>
#include <new>
#include <unordered_map>

namespace BML {
namespace {

constexpr std::uint32_t kDomain = BML_OBJECT_DOMAIN_VIRTOOLS;

struct Entry {
    CKObject *Object = nullptr;
    std::uint32_t Generation = 0;
};

} // namespace

struct ObjectRefs::Storage {
    std::unordered_map<std::uint32_t, Entry> Entries;
};

ObjectRefs::ObjectRefs(CKContext *context) noexcept
    : m_Context(context), m_Storage(new (std::nothrow) Storage) {}

ObjectRefs::~ObjectRefs() = default;

BML_ObjectRef ObjectRefs::Issue(CKObject *object) noexcept {
    if (!object || !m_Storage)
        return {};

    const CK_ID id = object->GetID();
    if (id == 0 || object->GetCKContext() != m_Context || object->IsToBeDeleted() ||
        CKGetObject(m_Context, id) != object) {
        return {};
    }

    const std::uint32_t slot = static_cast<std::uint32_t>(id);
    const auto existing = m_Storage->Entries.find(slot);
    if (existing != m_Storage->Entries.end() && existing->second.Object == object)
        return {kDomain, slot, existing->second.Generation};

    const std::uint32_t generation = NextGeneration();
    try {
        m_Storage->Entries.insert_or_assign(slot, Entry{object, generation});
    } catch (...) {
        return {};
    }
    return {kDomain, slot, generation};
}

CKObject *ObjectRefs::Resolve(BML_ObjectRef reference) const noexcept {
    if (!m_Context || !m_Storage || reference.Domain != kDomain || reference.Slot == 0 ||
        reference.Generation == 0) {
        return nullptr;
    }

    const auto entry = m_Storage->Entries.find(reference.Slot);
    if (entry == m_Storage->Entries.end() ||
        entry->second.Generation != reference.Generation || !entry->second.Object) {
        return nullptr;
    }

    CKObject *current = CKGetObject(m_Context, static_cast<CK_ID>(reference.Slot));
    return current == entry->second.Object && !current->IsToBeDeleted() ? current : nullptr;
}

void ObjectRefs::Invalidate(const CK_ID *ids, int count) noexcept {
    if (!m_Storage || !ids || count <= 0)
        return;
    for (int index = 0; index < count; ++index)
        m_Storage->Entries.erase(static_cast<std::uint32_t>(ids[index]));
}

void ObjectRefs::Reset() noexcept {
    if (m_Storage)
        m_Storage->Entries.clear();
}

std::uint32_t ObjectRefs::NextGeneration() noexcept {
    if (m_NextGeneration == 0) {
        Reset();
        m_NextGeneration = 1;
    }
    const std::uint32_t generation = m_NextGeneration;
    m_NextGeneration = generation == (std::numeric_limits<std::uint32_t>::max)()
        ? 0 : generation + 1;
    return generation;
}

} // namespace BML
