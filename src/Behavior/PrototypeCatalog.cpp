#include "Behavior/PrototypeCatalog.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace BML::Behavior {
namespace {

std::string GuidText(CKGUID guid) {
    std::ostringstream out;
    out << std::hex << guid.d1 << ':' << guid.d2;
    return out.str();
}

Status Failure(Error error, std::string message, CKGUID prototype = CKGUID()) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = Phase::PrototypeResolution;
    status.Details.Prototype = prototype;
    return status;
}

bool ProviderNameEquals(const std::string &left,
                        const std::string &right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const unsigned char l = static_cast<unsigned char>(left[index]);
        const unsigned char r = static_cast<unsigned char>(right[index]);
        if (std::tolower(l) != std::tolower(r))
            return false;
    }
    return true;
}

bool ContainsManager(const PrototypeInfo &prototype, CKGUID manager) {
    return std::any_of(
        prototype.Managers.begin(), prototype.Managers.end(),
        [&](const ManagerRequirement &candidate) {
            return candidate.Guid == manager;
        });
}

bool CompatibleClass(CK_CLASSID candidate, CK_CLASSID declared) {
#if defined(BML_TEST)
    // Golden tests do not load CK2.dll. Equality is enough for their exact
    // class-filter cases; Player validation exercises Virtools inheritance.
    return candidate == declared;
#else
    return CKIsChildClassOf(candidate, declared) != FALSE;
#endif
}

std::string ProviderFingerprint(std::vector<const PrototypeInfo *> prototypes) {
    std::sort(prototypes.begin(), prototypes.end(),
              [](const PrototypeInfo *left, const PrototypeInfo *right) {
                  if (left->Ref.Guid.d1 != right->Ref.Guid.d1)
                      return left->Ref.Guid.d1 < right->Ref.Guid.d1;
                  return left->Ref.Guid.d2 < right->Ref.Guid.d2;
              });
    std::ostringstream out;
    for (const PrototypeInfo *prototype : prototypes) {
        out << GuidText(prototype->Ref.Guid) << '|' << prototype->Version
            << '|' << prototype->CompatibleClass << '|' << prototype->Name
            << '|' << prototype->Category << ';';
    }
    return out.str();
}

} // namespace

PrototypeCatalog::PrototypeCatalog(std::unique_ptr<PrototypeSource> source)
    : m_Source(std::move(source)) {}

bool PrototypeCatalog::TracksRetirement() const noexcept {
    return m_Source && m_Source->TracksRetirement();
}

std::uint64_t PrototypeCatalog::NextGeneration() {
    if (m_NextGeneration == (std::numeric_limits<std::uint64_t>::max)())
        return 0;
    return m_NextGeneration++;
}

void PrototypeCatalog::Retire(CKGUID prototype) {
    const auto found = m_Entries.find(prototype);
    if (found != m_Entries.end())
        m_RetiredProviders.insert(found->second.ProviderKey);
    m_Layouts.erase(prototype);
}

Status PrototypeCatalog::Refresh() {
    if (!m_Source)
        return Failure(Error::ContextExpired,
                       "The Behavior Prototype source is unavailable.");

    std::vector<CKGUID> retired;
    bool retireAll = false;
    m_Source->TakeRetirements(retired, retireAll);
    if (retireAll) {
        for (const auto &[key, state] : m_Providers)
            m_RetiredProviders.insert(key);
        m_Layouts.clear();
    } else {
        for (CKGUID prototype : retired)
            Retire(prototype);
    }

    std::size_t prototypeCount = 0;
    Status status = m_Source->PrototypeCount(prototypeCount);
    if (!status)
        return status;
    if (m_Initialized && !retireAll && retired.empty() &&
        m_RetiredProviders.empty() &&
        prototypeCount == m_PrototypeCount)
        return {};

    std::vector<PrototypeInfo> declarations;
    status = m_Source->ReadDeclarations(declarations);
    if (!status)
        return status;

    std::unordered_map<std::string, std::vector<const PrototypeInfo *>> groups;
    for (const PrototypeInfo &prototype : declarations)
        groups[prototype.Provider.Key].push_back(&prototype);

    std::unordered_map<std::string, ProviderState> nextProviders;
    for (auto &[key, prototypes] : groups) {
        const std::string fingerprint = ProviderFingerprint(prototypes);
        const auto previous = m_Providers.find(key);
        const bool changed = previous == m_Providers.end() ||
                             previous->second.Fingerprint != fingerprint ||
                             m_RetiredProviders.contains(key);
        const std::uint64_t generation = changed
            ? NextGeneration() : previous->second.Generation;
        if (!generation)
            return Failure(Error::InvalidState,
                           "Behavior provider generation space is exhausted.");
        nextProviders.emplace(key, ProviderState{generation, fingerprint});
    }

    std::unordered_map<CKGUID, Entry, GuidHash> nextEntries;
    for (PrototypeInfo &prototype : declarations) {
        const auto provider = nextProviders.find(prototype.Provider.Key);
        if (provider == nextProviders.end())
            continue;
        prototype.Ref.Generation = provider->second.Generation;
        DetachedCompatibility detached = DetachedCompatibility::Unverified;
        const auto previous = m_Entries.find(prototype.Ref.Guid);
        if (previous != m_Entries.end() &&
            previous->second.Info.Ref.Generation == provider->second.Generation)
            detached = previous->second.Detached;
        nextEntries.emplace(
            prototype.Ref.Guid,
            Entry{std::move(prototype), provider->first, detached});
    }

    for (auto it = m_Layouts.begin(); it != m_Layouts.end();) {
        const auto entry = nextEntries.find(it->first);
        if (entry == nextEntries.end() ||
            entry->second.Info.Ref.Generation != it->second.Generation)
            it = m_Layouts.erase(it);
        else
            ++it;
    }

    m_Providers = std::move(nextProviders);
    m_Entries = std::move(nextEntries);
    m_RetiredProviders.clear();
    m_PrototypeCount = prototypeCount;
    m_Initialized = true;
    return {};
}

Status PrototypeCatalog::ProcessFrame() {
    return Refresh();
}

const PrototypeCatalog::Entry *
PrototypeCatalog::FindEntry(CKGUID prototype) const {
    const auto found = m_Entries.find(prototype);
    return found == m_Entries.end() ? nullptr : &found->second;
}

Status PrototypeCatalog::Find(const PrototypeQuery &query,
                              std::vector<PrototypeInfo> &out) {
    out.clear();
    Status status = Refresh();
    if (!status)
        return status;

    for (const auto &[guid, entry] : m_Entries) {
        const PrototypeInfo &prototype = entry.Info;
        if (query.MatchGuid && prototype.Ref.Guid != query.Guid)
            continue;
        if (query.MatchName && prototype.Name != query.Name)
            continue;
        if (query.MatchCategory && prototype.Category != query.Category)
            continue;
        if (query.MatchProvider &&
            !ProviderNameEquals(prototype.Provider.Name, query.Provider))
            continue;
        if (query.MatchProviderGuid &&
            prototype.Provider.Guid != query.ProviderGuid)
            continue;
        if (query.MatchCompatibleClass &&
            !CompatibleClass(query.CompatibleClass,
                             prototype.CompatibleClass))
            continue;
        if (!std::all_of(
                query.RequiredManagers.begin(), query.RequiredManagers.end(),
                [&](CKGUID manager) {
                    return ContainsManager(prototype, manager);
                }))
            continue;
        out.push_back(prototype);
    }

    std::sort(out.begin(), out.end(),
              [](const PrototypeInfo &left, const PrototypeInfo &right) {
                  if (left.Ref.Guid.d1 != right.Ref.Guid.d1)
                      return left.Ref.Guid.d1 < right.Ref.Guid.d1;
                  return left.Ref.Guid.d2 < right.Ref.Guid.d2;
              });
    return {};
}

Status PrototypeCatalog::Validate(PrototypeRef prototype) {
    Status status = Refresh();
    if (!status)
        return status;
    const Entry *entry = FindEntry(prototype.Guid);
    if (!entry)
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype " + GuidText(prototype.Guid) +
                           " is not registered.",
                       prototype.Guid);
    if (prototype.Generation &&
        entry->Info.Ref.Generation != prototype.Generation)
        return Failure(Error::PrototypeChanged,
                       "The Building Block provider changed after discovery.",
                       prototype.Guid);
    return {};
}

Status PrototypeCatalog::Detached(PrototypeRef prototype,
                                  DetachedCompatibility &out) {
    out = DetachedCompatibility::Unverified;
    Status status = Validate(prototype);
    if (!status)
        return status;
    const Entry *entry = FindEntry(prototype.Guid);
    if (!entry)
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype is unavailable.",
                       prototype.Guid);
    out = entry->Detached;
    return {};
}

Status PrototypeCatalog::RecordDetached(
    PrototypeRef prototype, DetachedCompatibility compatibility) {
    Status status = Validate(prototype);
    if (!status)
        return status;
    const Entry *entry = FindEntry(prototype.Guid);
    if (!entry)
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype is unavailable.",
                       prototype.Guid);
    m_Entries[prototype.Guid].Detached = compatibility;
    return {};
}

Status PrototypeCatalog::DeclaredLayout(PrototypeRef prototype, Layout &out) {
    out = {};
    Status status = Validate(prototype);
    if (!status)
        return status;
    const Entry *entry = FindEntry(prototype.Guid);
    if (!entry)
        return Failure(Error::PrototypeNotFound,
                       "Building Block Prototype is unavailable.", prototype.Guid);
    const std::uint64_t generation = entry->Info.Ref.Generation;

    const auto cached = m_Layouts.find(prototype.Guid);
    if (cached != m_Layouts.end() && cached->second.Generation == generation) {
        out = cached->second.Value;
        out.MaterializedNow = false;
        return {};
    }

    Layout layout;
    status = m_Source->ReadDeclaredLayout(prototype.Guid, layout);
    if (!status)
        return status;
    status = Refresh();
    if (!status)
        return status;
    entry = FindEntry(prototype.Guid);
    if (!entry || entry->Info.Ref.Generation != generation)
        return Failure(Error::PrototypeChanged,
                       "The Building Block provider changed while its Prototype was materialized.",
                       prototype.Guid);

    layout.Origin = LayoutOrigin::Declared;
    layout.ProviderGeneration = generation;
    layout.Provider = entry->Info.Provider.Guid;
    layout.ProviderName = entry->Info.Provider.Name;
    layout.Author = entry->Info.Author;
    layout.Description = entry->Info.Description;
    layout.Version = entry->Info.Version;
    layout.Category = entry->Info.Category;
    if (layout.CompatibleClass <= 0)
        layout.CompatibleClass = entry->Info.CompatibleClass;
    layout.Managers = entry->Info.Managers;
    layout.RequiredManagers.clear();
    for (const ManagerRequirement &manager : layout.Managers)
        layout.RequiredManagers.push_back(manager.Guid);
    layout.MaterializedNow = true;
    m_Layouts[prototype.Guid] = CachedLayout{generation, layout};
    out = std::move(layout);
    return {};
}

} // namespace BML::Behavior
