#include "Loader/NativeModRegistry.h"

#include <algorithm>

bool NativeModRegistry::Add(
    const std::shared_ptr<void> &dllHandle, const std::string &modId) {
    if (!dllHandle || modId.empty())
        return false;

    for (const auto &dll : m_Dlls) {
        const auto &ids = dll.second.ModIds;
        if (std::find(ids.begin(), ids.end(), modId) != ids.end())
            return false;
    }

    void *rawHandle = dllHandle.get();
    auto existing = m_Dlls.find(rawHandle);
    if (existing != m_Dlls.end()) {
        // LoadLibrary may return the same HMODULE through a fresh shared_ptr
        // control block. Keep the first owning reference; the caller's extra
        // reference may release its matching LoadLibrary count normally.
        existing->second.ModIds.push_back(modId);
        return true;
    }

    auto [entry, inserted] = m_Dlls.try_emplace(rawHandle);
    if (!inserted)
        return false;

    try {
        entry->second.Handle = dllHandle;
        entry->second.ModIds.push_back(modId);
    } catch (...) {
        m_Dlls.erase(entry);
        throw;
    }
    return true;
}

bool NativeModRegistry::Remove(const std::string &modId) {
    if (modId.empty())
        return false;

    for (auto dll = m_Dlls.begin(); dll != m_Dlls.end(); ++dll) {
        auto &ids = dll->second.ModIds;
        const auto id = std::find(ids.begin(), ids.end(), modId);
        if (id == ids.end())
            continue;

        ids.erase(id);
        if (ids.empty())
            m_Dlls.erase(dll);
        return true;
    }
    return false;
}

std::shared_ptr<void> NativeModRegistry::FindDllForMod(const std::string &modId) const {
    if (modId.empty())
        return {};

    for (const auto &dll : m_Dlls) {
        const auto &ids = dll.second.ModIds;
        if (std::find(ids.begin(), ids.end(), modId) != ids.end())
            return dll.second.Handle;
    }
    return {};
}

std::vector<std::string> NativeModRegistry::SnapshotMods(void *dllHandle) const {
    const auto dll = m_Dlls.find(dllHandle);
    return dll == m_Dlls.end() ? std::vector<std::string>() : dll->second.ModIds;
}

bool NativeModRegistry::Owns(void *dllHandle, const std::string &modId) const {
    const auto dll = m_Dlls.find(dllHandle);
    if (dll == m_Dlls.end())
        return false;
    const auto &ids = dll->second.ModIds;
    return std::find(ids.begin(), ids.end(), modId) != ids.end();
}

std::string NativeModRegistry::GetUniqueModId(void *dllHandle) const {
    const auto dll = m_Dlls.find(dllHandle);
    if (dll == m_Dlls.end() || dll->second.ModIds.size() != 1)
        return {};
    return dll->second.ModIds.front();
}
