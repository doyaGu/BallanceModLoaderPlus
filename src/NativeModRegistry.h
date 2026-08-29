#ifndef BML_NATIVEMODREGISTRY_H
#define BML_NATIVEMODREGISTRY_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Owns each native DLL handle once and records the Mod ids exported by it.
// The DLL handle is the primary identity; there is deliberately no reverse
// pointer index that can drift out of sync with this registry.
class NativeModRegistry {
public:
    bool Add(const std::shared_ptr<void> &dllHandle, const std::string &modId);
    bool Remove(const std::string &modId);

    std::shared_ptr<void> FindDllForMod(const std::string &modId) const;
    std::vector<std::string> SnapshotMods(void *dllHandle) const;
    bool Owns(void *dllHandle, const std::string &modId) const;
    std::string GetUniqueModId(void *dllHandle) const;

private:
    struct Registration {
        std::shared_ptr<void> Handle;
        std::vector<std::string> ModIds;
    };

    std::unordered_map<void *, Registration> m_Dlls;
};

#endif // BML_NATIVEMODREGISTRY_H
