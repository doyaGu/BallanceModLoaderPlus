/**
 * @file bml_hook_module.hpp
 * @brief Base class for hook-based BML modules
 *
 * Eliminates the repeated Engine/Init subscription + HookContext + init/shutdown
 * boilerplate that every hook-using module duplicates. Subclasses override three
 * methods:
 *
 *   - HookLogCategory() -return a static string for log/hook tagging
 *   - InitHook()        -perform the actual hook installation
 *   - ShutdownHook()    -tear down hooks
 *
 * And optionally:
 *   - OnModuleAttach()  -add extra subscriptions, publish interfaces, etc.
 *   - OnModuleDetach()  -extra cleanup before ShutdownHook runs
 *
 * Usage:
 * @code
 *   #define BML_LOADER_IMPLEMENTATION
 *   #include <bml_hook_module.hpp>
 *   #include "MyHook.h"
 *
 *   class MyMod : public bml::HookModule {
 *       const char *HookLogCategory() const override { return "MyMod"; }
 *       bool InitHook(CKContext *ctx, const BML_HookContext *hctx) override {
 *           return MyHook::Init(ctx, hctx);
 *       }
 *       void ShutdownHook() override { MyHook::Shutdown(); }
 *   };
 *   BML_DEFINE_MODULE(MyMod)
 * @endcode
 */

#ifndef BML_HOOK_MODULE_HPP
#define BML_HOOK_MODULE_HPP

#include "bml_module.hpp"
#include "bml_hook_context.h"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_topics.h"

namespace bml {

class HookModule : public Module {
protected:
    imc::SubscriptionManager m_Subs;
    bool m_HookReady = false;

    /** @brief Return a static log category string (e.g. "BML_Physics"). */
    virtual const char *HookLogCategory() const = 0;

    /**
     * @brief Install hooks. Called from Engine/Init or TryInitHook().
     * @param ctx  The CKContext from the engine event.
     * @param hctx Pre-built HookContext with stable vtable pointers.
     * @return true if hooks were installed successfully.
     */
    virtual bool InitHook(CKContext *ctx, const BML_HookContext *hctx) = 0;

    /** @brief Tear down hooks. Called from OnDetach. */
    virtual void ShutdownHook() = 0;

    /**
     * @brief Extra work during attach (add subscriptions, publish interfaces).
     * Called after the built-in Engine/Init subscription is registered.
     * m_Subs is already initialized and available.
     * @return BML_RESULT_OK to continue, any other value aborts attach.
     */
    virtual BML_Result OnModuleAttach(ModuleServices &services) {
        (void)services;
        return BML_RESULT_OK;
    }

    /** @brief Extra cleanup before ShutdownHook runs. */
    virtual void OnModuleDetach() {}

    /**
     * @brief Release outbound dependencies before detach.
     * Return an error to abort unload/reload. Default: OK.
     */
    virtual BML_Result OnModulePrepareDetach() { return BML_RESULT_OK; }

    /**
     * @brief Attempt hook initialization with a CKContext.
     *
     * Safe to call multiple times -returns immediately if already initialized.
     * Useful for eager init in OnModuleAttach or retry from Engine/Play.
     */
    bool TryInitHook(CKContext *ctx) {
        if (m_HookReady || !ctx) return m_HookReady;
        auto hctx = BML_MakeHookContext(Services(), HookLogCategory());
        if (InitHook(ctx, &hctx)) {
            m_HookReady = true;
            Services().Log().Info("%s hooks initialized", HookLogCategory());
        }
        return m_HookReady;
    }

public:
    BML_Result OnAttach(ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const imc::Message &msg) {
            if (m_HookReady) return;
            auto *payload = ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!payload) return;
            TryInitHook(payload->context);
        });

        BML_Result result = OnModuleAttach(services);
        if (result != BML_RESULT_OK) return result;

        if (m_Subs.Empty()) {
            services.Log().Error("No subscriptions registered for %s", HookLogCategory());
            return BML_RESULT_FAIL;
        }

        return BML_RESULT_OK;
    }

    BML_Result OnPrepareDetach() override {
        return OnModulePrepareDetach();
    }

    void OnDetach() override {
        OnModuleDetach();
        if (m_HookReady) {
            ShutdownHook();
            m_HookReady = false;
        }
    }
};

} // namespace bml

#endif /* BML_HOOK_MODULE_HPP */
