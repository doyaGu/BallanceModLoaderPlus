#include "ModuleLifecycle.h"

#include <exception>
#include <string>

#include "ConfigStore.h"
#include "Context.h"
#include "HookRegistry.h"
#include "ImcBus.h"
#include "InterfaceRegistry.h"
#include "LeaseManager.h"
#include "LocaleManager.h"
#include "Logging.h"
#include "TimerManager.h"

namespace BML::Core {
    namespace {
        constexpr char kLogCategory[] = "module.lifecycle";

        class ModuleScope {
        public:
            explicit ModuleScope(BML_Mod mod)
                : m_Previous(Context::GetCurrentModule()) {
                Context::SetCurrentModule(mod);
            }

            ~ModuleScope() {
                Context::SetCurrentModule(m_Previous);
            }

            ModuleScope(const ModuleScope &) = delete;
            ModuleScope &operator=(const ModuleScope &) = delete;

        private:
            BML_Mod m_Previous;
        };
    } // namespace

    BML_Result PrepareModuleForDetach(const std::string &module_id,
                                      BML_Mod mod,
                                      ModuleEntrypoint entrypoint,
                                      std::string *out_diagnostic) {
        if (module_id.empty() || !mod || !entrypoint) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ModuleScope scope(mod);

        BML_ModDetachArgs detach{};
        detach.struct_size = sizeof(detach);
        detach.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
        detach.mod = mod;

        BML_Result result = BML_RESULT_INTERNAL_ERROR;
        try {
            result = entrypoint(BML_MOD_ENTRYPOINT_PREPARE_DETACH, &detach);
        } catch (const std::exception &ex) {
            if (out_diagnostic) {
                *out_diagnostic = std::string("prepare detach threw exception: ") + ex.what();
            }
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Prepare detach for '%s' threw exception: %s",
                    module_id.c_str(), ex.what());
            return BML_RESULT_INTERNAL_ERROR;
        } catch (...) {
            if (out_diagnostic) {
                *out_diagnostic = "prepare detach threw unknown exception";
            }
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Prepare detach for '%s' threw unknown exception",
                    module_id.c_str());
            return BML_RESULT_INTERNAL_ERROR;
        }

        if (result != BML_RESULT_OK) {
            if (out_diagnostic) {
                *out_diagnostic = "prepare detach returned " + std::to_string(result);
            }
            return result;
        }

        auto &leases = LeaseManager::Instance();
        leases.SetProviderBlocked(module_id, true);

        std::string detail;
        if (leases.HasOutboundDependencies(module_id, &detail)) {
            if (out_diagnostic) {
                *out_diagnostic = detail;
            }
            leases.SetProviderBlocked(module_id, false);
            return BML_RESULT_BUSY;
        }

        if (leases.HasInboundDependencies(module_id, &detail)) {
            if (out_diagnostic) {
                *out_diagnostic = detail;
            }
            leases.SetProviderBlocked(module_id, false);
            return BML_RESULT_BUSY;
        }

        return BML_RESULT_OK;
    }

    void CancelModuleDetachPreparation(const std::string &module_id) {
        if (module_id.empty()) {
            return;
        }
        LeaseManager::Instance().SetProviderBlocked(module_id, false);
    }

    void CleanupModuleKernelState(const std::string &module_id, BML_Mod mod) {
        if (module_id.empty()) {
            return;
        }

        HookRegistry::Instance().CleanupOwner(module_id);
        LocaleManager::Instance().CleanupModule(module_id);
        TimerManager::Instance().CancelAllForModule(module_id);
        ImcCleanupOwner(mod);
        InterfaceRegistry::Instance().UnregisterByProvider(module_id);
        LeaseManager::Instance().CleanupConsumer(module_id);
        LeaseManager::Instance().CleanupProvider(module_id);
        CleanupConfigHooksForModule(mod);
    }
} // namespace BML::Core
