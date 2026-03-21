#include "ReloadableModuleSlot.h"
#include "ModManifest.h"

namespace BML::Core {
    ReloadableModuleSlot::ReloadableModuleSlot(Context *ctx) : m_Context(ctx) {}

    ReloadableModuleSlot::~ReloadableModuleSlot() = default;

    bool ReloadableModuleSlot::Initialize(const ReloadableSlotConfig &config) {
        m_Config = config;
        m_LastFailure = ReloadFailure::SystemError;
        m_LastSystemError = 0;
        return false;
    }

    void ReloadableModuleSlot::Shutdown() {
        m_Config = {};
        m_Handle = nullptr;
        m_Entrypoint = nullptr;
        m_ModHandle.reset();
        m_UserData = nullptr;
        m_Version = 0;
        m_NextVersion = 1;
        m_LastWorkingVersion = 0;
    }

    bool ReloadableModuleSlot::HasChanged() const {
        return false;
    }

    ReloadResult ReloadableModuleSlot::Reload() {
        m_LastFailure = ReloadFailure::SystemError;
        return ReloadResult::LoadFailed;
    }

    ReloadResult ReloadableModuleSlot::ForceReload() {
        m_LastFailure = ReloadFailure::SystemError;
        return ReloadResult::LoadFailed;
    }

    const std::string &ReloadableModuleSlot::GetModId() const {
        static const std::string kEmpty;
        if (!m_Config.mod_id.empty()) {
            return m_Config.mod_id;
        }
        if (!m_Config.manifest) {
            return kEmpty;
        }
        return m_Config.manifest->package.id;
    }
} // namespace BML::Core
