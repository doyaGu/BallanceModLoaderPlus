#ifndef BML_CONFIGSTORE_H
#define BML_CONFIGSTORE_H

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Config;
class IMod;

// Owns the config registry and keeps its id index consistent. ModContext
// serializes access with m_Mutex; this class enforces owner identity and
// container invariants, not synchronization.
class ConfigStore {
public:
    bool Contains(const std::string &modId) const;
    Config *Add(const std::string &modId, IMod *owner, std::unique_ptr<Config> config);
    std::unique_ptr<Config> Remove(const std::string &modId, IMod *owner, Config *config);
    Config *Find(const std::string &modId, const IMod *owner) const;
    std::vector<Config *> Snapshot() const;

private:
    std::vector<std::unique_ptr<Config>> m_Configs;
    std::unordered_map<std::string, size_t> m_Index;
};

#endif // BML_CONFIGSTORE_H
