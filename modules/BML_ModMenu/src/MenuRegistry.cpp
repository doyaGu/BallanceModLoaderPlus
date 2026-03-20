#include "MenuRegistry.h"

#include <cstdio>
#include <sstream>

#include "CKBehavior.h"

#include "Config.h"

#include "src/Core/ModHandle.h"
#include "src/Core/ModManifest.h"

namespace {
std::string JoinAuthors(const BML::Core::ModManifest *manifest) {
    if (!manifest || manifest->package.authors.empty()) {
        return {};
    }

    std::ostringstream oss;
    for (size_t index = 0; index < manifest->package.authors.size(); ++index) {
        if (index != 0) {
            oss << " & ";
        }
        oss << manifest->package.authors[index];
    }
    return oss.str();
}
} // namespace

LoadedMod::LoadedMod(BML_Mod handle)
    : m_Handle(handle) {
    auto *internal = reinterpret_cast<BML_Mod_T *>(handle);
    if (!internal) {
        return;
    }

    m_Id = internal->id;

    char version[32] = {};
    std::snprintf(version, sizeof(version), "%u.%u.%u", internal->version.major, internal->version.minor,
                  internal->version.patch);
    m_Version = version;

    if (internal->manifest) {
        const auto &pkg = internal->manifest->package;
        m_Name = pkg.name.empty() ? internal->id : pkg.name;
        m_Author = JoinAuthors(internal->manifest);
        m_Description = pkg.description;
    }

    if (m_Name.empty()) {
        m_Name = m_Id;
    }
}

MenuRegistry &MenuRegistry::Instance() {
    static MenuRegistry registry;
    return registry;
}

void MenuRegistry::Initialize(CKContext *context,
                              const BML_CoreModuleInterface *moduleInterface,
                              const BML_CoreConfigInterface *configInterface) {
    m_Context = context;
    m_ModuleInterface = moduleInterface;
    m_ConfigInterface = configInterface;
    Refresh();
}

void MenuRegistry::Reset() {
    m_Context = nullptr;
    m_ModuleInterface = nullptr;
    m_ConfigInterface = nullptr;
    m_Records.clear();
}

void MenuRegistry::Refresh() {
    m_Records.clear();
    if (!m_ModuleInterface || !m_ModuleInterface->GetLoadedModuleCount || !m_ModuleInterface->GetLoadedModuleAt) {
        return;
    }

    const uint32_t count = m_ModuleInterface->GetLoadedModuleCount();
    m_Records.reserve(count);
    for (uint32_t index = 0; index < count; ++index) {
        BML_Mod handle = m_ModuleInterface->GetLoadedModuleAt(index);
        if (!handle) {
            continue;
        }

        Record record;
        record.mod = std::make_unique<LoadedMod>(handle);
        m_Records.push_back(std::move(record));
    }
}

CKBehavior *MenuRegistry::GetScriptByName(const char *name) const {
    if (!m_Context || !name || !*name) {
        return nullptr;
    }
    return reinterpret_cast<CKBehavior *>(m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_BEHAVIOR));
}

int MenuRegistry::GetModCount() {
    if (m_Records.empty()) {
        Refresh();
    }
    return static_cast<int>(m_Records.size());
}

LoadedMod *MenuRegistry::GetMod(int index) {
    if (index < 0) {
        return nullptr;
    }
    if (m_Records.empty()) {
        Refresh();
    }
    if (index >= static_cast<int>(m_Records.size())) {
        return nullptr;
    }
    return m_Records[static_cast<size_t>(index)].mod.get();
}

Config *MenuRegistry::GetConfig(LoadedMod *mod) {
    if (!mod) {
        return nullptr;
    }

    for (auto &record : m_Records) {
        if (record.mod.get() == mod) {
            if (!record.config) {
                record.config = std::make_unique<Config>(mod->GetHandle(), m_ConfigInterface);
            } else {
                record.config->Reload();
            }
            return record.config.get();
        }
    }

    return nullptr;
}
