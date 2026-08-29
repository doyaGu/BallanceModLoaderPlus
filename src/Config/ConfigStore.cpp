#include "Config/ConfigStore.h"

#include "Config/Config.h"

bool ConfigStore::Contains(const std::string &modId) const {
    return m_Index.find(modId) != m_Index.end();
}

Config *ConfigStore::Add(const std::string &modId, IMod *owner, std::unique_ptr<Config> config) {
    if (!owner || !config || config->GetMod() != owner || Contains(modId))
        return nullptr;

    Config *rawConfig = config.get();
    const size_t position = m_Configs.size();
    m_Configs.push_back(std::move(config));
    try {
        const bool inserted = m_Index.emplace(modId, position).second;
        if (!inserted) {
            m_Configs.pop_back();
            return nullptr;
        }
    } catch (...) {
        m_Configs.pop_back();
        throw;
    }
    return rawConfig;
}

std::unique_ptr<Config> ConfigStore::Remove(
    const std::string &modId, IMod *owner, Config *config) {
    const auto it = m_Index.find(modId);
    if (it == m_Index.end() || it->second >= m_Configs.size() ||
        m_Configs[it->second].get() != config ||
        m_Configs[it->second]->GetMod() != owner) {
        return nullptr;
    }

    const size_t position = it->second;
    std::unique_ptr<Config> removed = std::move(m_Configs[position]);
    m_Configs.erase(m_Configs.begin() + static_cast<std::ptrdiff_t>(position));
    m_Index.erase(it);
    for (auto &entry : m_Index) {
        if (entry.second > position)
            --entry.second;
    }
    return removed;
}

Config *ConfigStore::Find(const std::string &modId, const IMod *owner) const {
    const auto it = m_Index.find(modId);
    if (it == m_Index.end() || it->second >= m_Configs.size() ||
        m_Configs[it->second]->GetMod() != owner) {
        return nullptr;
    }
    return m_Configs[it->second].get();
}

std::vector<Config *> ConfigStore::Snapshot() const {
    std::vector<Config *> configs;
    configs.reserve(m_Configs.size());
    for (const auto &config : m_Configs)
        configs.push_back(config.get());
    return configs;
}
