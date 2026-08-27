#include "CKIdentityRegistry.h"

#include "CKGlobals.h"
#include "CKObject.h"

#include <array>
#include <limits>
#include <new>

namespace BML {

namespace {

constexpr uint32_t kDomain = BML_OBJECT_DOMAIN_VIRTOOLS;
constexpr uint32_t kRootBits = 10;
constexpr uint32_t kBranchBits = 11;
constexpr uint32_t kPageBits = 11;
constexpr uint32_t kRootSize = 1u << kRootBits;
constexpr uint32_t kBranchSize = 1u << kBranchBits;
constexpr uint32_t kPageSize = 1u << kPageBits;
constexpr uint32_t kBranchMask = kBranchSize - 1;
constexpr uint32_t kPageMask = kPageSize - 1;
static_assert(kRootBits + kBranchBits + kPageBits == 32);

struct Entry {
    CKObject *Object = nullptr;
    uint32_t Serial = 0;
};

struct Page {
    std::array<Entry, kPageSize> Entries{};
};

struct Branch {
    std::array<std::unique_ptr<Page>, kBranchSize> Pages{};
};

} // namespace

struct CKIdentityRegistry::Storage {
    std::array<std::unique_ptr<Branch>, kRootSize> Branches{};

    [[nodiscard]] Entry *Find(uint32_t slot) noexcept {
        const auto &branch = Branches[slot >> (kBranchBits + kPageBits)];
        if (!branch)
            return nullptr;
        const auto &page = branch->Pages[(slot >> kPageBits) & kBranchMask];
        return page ? &page->Entries[slot & kPageMask] : nullptr;
    }

    [[nodiscard]] const Entry *Find(uint32_t slot) const noexcept {
        const auto &branch = Branches[slot >> (kBranchBits + kPageBits)];
        if (!branch)
            return nullptr;
        const auto &page = branch->Pages[(slot >> kPageBits) & kBranchMask];
        return page ? &page->Entries[slot & kPageMask] : nullptr;
    }

    [[nodiscard]] Entry *FindOrCreate(uint32_t slot) noexcept {
        auto &branch = Branches[slot >> (kBranchBits + kPageBits)];
        if (!branch) {
            branch.reset(new (std::nothrow) Branch);
            if (!branch)
                return nullptr;
        }

        auto &page = branch->Pages[(slot >> kPageBits) & kBranchMask];
        if (!page) {
            page.reset(new (std::nothrow) Page);
            if (!page)
                return nullptr;
        }
        return &page->Entries[slot & kPageMask];
    }

    void Clear() noexcept {
        for (auto &branch : Branches)
            branch.reset();
    }
};

CKIdentityRegistry::CKIdentityRegistry(CKContext *context) noexcept
    : m_Context(context), m_Storage(new (std::nothrow) Storage) {}

CKIdentityRegistry::~CKIdentityRegistry() = default;

BML_ObjectRef CKIdentityRegistry::Make(CKObject *object) noexcept {
    if (!object || !m_Storage)
        return {};

    const CK_ID id = object->GetID();
    if (id == 0 || object->GetCKContext() != m_Context || object->IsToBeDeleted() ||
        CKGetObject(m_Context, id) != object) {
        return {};
    }

    const uint32_t slot = static_cast<uint32_t>(id);
    if (const Entry *entry = m_Storage->Find(slot); entry && entry->Object == object)
        return {kDomain, slot, entry->Serial};

    const uint32_t serial = NextSerial();
    Entry *entry = m_Storage->FindOrCreate(slot);
    if (!entry)
        return {};

    *entry = {object, serial};
    return {kDomain, slot, serial};
}

CKObject *CKIdentityRegistry::Resolve(BML_ObjectRef reference) const noexcept {
    if (!m_Context || !m_Storage || reference.Domain != kDomain || reference.Slot == 0 ||
        reference.Generation == 0) {
        return nullptr;
    }

    const Entry *entry = m_Storage->Find(reference.Slot);
    if (!entry || entry->Serial != reference.Generation || !entry->Object)
        return nullptr;

    CKObject *current = CKGetObject(m_Context, static_cast<CK_ID>(reference.Slot));
    return current == entry->Object && !current->IsToBeDeleted() ? current : nullptr;
}

void CKIdentityRegistry::Invalidate(const CK_ID *ids, int count) noexcept {
    if (!m_Storage || !ids || count <= 0)
        return;

    for (int index = 0; index < count; ++index) {
        if (Entry *entry = m_Storage->Find(static_cast<uint32_t>(ids[index])))
            *entry = {};
    }
}

void CKIdentityRegistry::ResetWorld() noexcept {
    if (m_Storage)
        m_Storage->Clear();
}

uint32_t CKIdentityRegistry::NextSerial() noexcept {
    if (m_NextSerial == 0) {
        ResetWorld();
        m_NextSerial = 1;
    }

    const uint32_t serial = m_NextSerial;
    m_NextSerial = serial == (std::numeric_limits<uint32_t>::max)() ? 0 : serial + 1;
    return serial;
}

} // namespace BML
