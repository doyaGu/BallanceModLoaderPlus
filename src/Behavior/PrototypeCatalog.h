#ifndef BML_BEHAVIOR_PROTOTYPECATALOG_H
#define BML_BEHAVIOR_PROTOTYPECATALOG_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Behavior/Status.h"

namespace BML::Behavior {

struct ProviderInfo {
    CKGUID Guid = CKGUID();
    std::string Key;
    std::string Name;
    std::string Path;
    std::string Author;
    std::string Description;
};

struct PrototypeRef {
    CKGUID Guid = CKGUID();
    std::uint64_t Generation = 0;
};

struct PrototypeInfo {
    PrototypeRef Ref;
    ProviderInfo Provider;
    std::string Name;
    std::string Category;
    std::string Author;
    std::string Description;
    CKDWORD Version = 0;
    CK_CLASSID CompatibleClass = CKCID_BEOBJECT;
    std::vector<ManagerRequirement> Managers;
};

struct PrototypeQuery {
    bool MatchGuid = false;
    CKGUID Guid = CKGUID();
    bool MatchName = false;
    std::string Name;
    bool MatchCategory = false;
    std::string Category;
    bool MatchProvider = false;
    std::string Provider;
    bool MatchProviderGuid = false;
    CKGUID ProviderGuid = CKGUID();
    bool MatchCompatibleClass = false;
    CK_CLASSID CompatibleClass = 0;
    std::vector<CKGUID> RequiredManagers;
};

class PrototypeSource {
public:
    virtual ~PrototypeSource() = default;

    virtual Status ReadDeclarations(std::vector<PrototypeInfo> &out) = 0;
    virtual Status PrototypeCount(std::size_t &out) const = 0;
    virtual Status ReadDeclaredLayout(CKGUID prototype, Layout &out) = 0;
    virtual void TakeRetirements(std::vector<CKGUID> &out,
                                 bool &retireAll) = 0;
    [[nodiscard]] virtual bool TracksRetirement() const noexcept = 0;
};

[[nodiscard]] std::unique_ptr<PrototypeSource>
MakeCKPrototypeSource(CKContext *context);

class PrototypeCatalog final {
public:
    explicit PrototypeCatalog(std::unique_ptr<PrototypeSource> source);

    [[nodiscard]] bool TracksRetirement() const noexcept;
    Status ProcessFrame();
    Status Find(const PrototypeQuery &query,
                std::vector<PrototypeInfo> &out);
    Status DeclaredLayout(PrototypeRef prototype, Layout &out);
    Status Validate(PrototypeRef prototype);

private:
    struct GuidHash {
        std::size_t operator()(CKGUID guid) const noexcept {
            const std::uint64_t value =
                (static_cast<std::uint64_t>(
                     static_cast<std::uint32_t>(guid.d1)) << 32u) |
                static_cast<std::uint32_t>(guid.d2);
            return std::hash<std::uint64_t>{}(value);
        }
    };

    struct ProviderState {
        std::uint64_t Generation = 0;
        std::string Fingerprint;
    };

    struct Entry {
        PrototypeInfo Info;
        std::string ProviderKey;
    };

    struct CachedLayout {
        std::uint64_t Generation = 0;
        Layout Value;
    };

    Status Refresh();
    void Retire(CKGUID prototype);
    [[nodiscard]] std::uint64_t NextGeneration();
    [[nodiscard]] const Entry *FindEntry(CKGUID prototype) const;

    std::unique_ptr<PrototypeSource> m_Source;
    bool m_Initialized = false;
    std::size_t m_PrototypeCount = 0;
    std::uint64_t m_NextGeneration = 1;
    std::unordered_map<std::string, ProviderState> m_Providers;
    std::unordered_map<CKGUID, Entry, GuidHash> m_Entries;
    std::unordered_map<CKGUID, CachedLayout, GuidHash> m_Layouts;
    std::unordered_set<std::string> m_RetiredProviders;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PROTOTYPECATALOG_H
