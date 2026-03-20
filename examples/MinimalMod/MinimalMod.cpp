/**
 * MinimalMod - Minimal BML v0.4 Module Example
 *
 * Demonstrates the simplest possible module using the bml::Module framework:
 * - Deriving from bml::Module
 * - Using SubscriptionManager for IMC events
 * - BML_DEFINE_MODULE macro for entry point generation
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_topics.h"

class MinimalMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        const BML_Version *ver = Services().Builtins().Context->GetRuntimeVersion
            ? Services().Builtins().Context->GetRuntimeVersion()
            : nullptr;
        Services().Log().Info("API loaded. Runtime version: %u.%u.%u",
                              ver ? ver->major : 0,
                              ver ? ver->minor : 0,
                              ver ? ver->patch : 0);

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &) {
            Services().Log().Info("Received BML/Engine/Init");
        });

        return m_Subs.Empty() ? BML_RESULT_FAIL : BML_RESULT_OK;
    }

    void OnDetach() override {
        Services().Log().Info("Shutting down");
    }
};

BML_DEFINE_MODULE(MinimalMod)
