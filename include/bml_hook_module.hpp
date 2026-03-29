/**
 * @file bml_hook_module.hpp
 * @brief Base class for hook-based BML modules
 *
 * Eliminates the repeated Engine/Init subscription + init/shutdown boilerplate
 * that every hook-using module duplicates. Subclasses override three methods:
 *
 *   - HookLogCategory() --return a static string for log tagging
 *   - InitHook()        --perform the actual hook installation
 *   - ShutdownHook()    --tear down hooks (must be safe after partial init)
 *
 * And optionally:
 *   - OnModuleAttach()      --add extra subscriptions, publish interfaces, etc.
 *   - OnModuleDetach()      --extra cleanup before ShutdownHook runs
 *   - OnModulePrepareDetach() --release dependencies or abort unload
 *
 * Usage:
 * @code
 *   #define BML_LOADER_IMPLEMENTATION
 *   #include <bml_hook_module.hpp>
 *
 *   class MyMod : public bml::HookModule {
 *       static MyMod *s_Instance;
 *       const char *HookLogCategory() const override { return "MyMod"; }
 *       bool InitHook(CKContext *ctx) override { ... }
 *       void ShutdownHook() override { ... }
 *   };
 *   MyMod *MyMod::s_Instance = nullptr;
 *   BML_DEFINE_MODULE(MyMod)
 * @endcode
 */

#ifndef BML_HOOK_MODULE_HPP
#define BML_HOOK_MODULE_HPP

#include "bml_module.hpp"
#include "bml_hook.hpp"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_topics.h"

#include <vector>

namespace bml {

class HookModule : public Module {
protected:
    imc::SubscriptionManager m_Subs;
    bool m_HookReady = false;
    std::vector<HookRegistration> m_HookRegistrations;

    /** @brief Return a static log category string (e.g. "BML_Physics"). */
    virtual const char *HookLogCategory() const = 0;

    /**
     * @brief Install hooks. Called from Engine/Init or TryInitHook().
     *
     * If this returns false, ShutdownHook() is called automatically to roll
     * back any partially installed hooks. Implementations must therefore be
     * safe to call ShutdownHook() after partial initialization.
     *
     * @param ctx  The CKContext from the engine event.
     * @return true if hooks were installed successfully.
     */
    virtual bool InitHook(CKContext *ctx) = 0;

    /** @brief Tear down hooks. Must be safe after partial init. */
    virtual void ShutdownHook() = 0;

    /**
     * @brief Extra work during attach (add subscriptions, publish interfaces).
     * Called after the built-in Engine/Init subscription is registered.
     * m_Subs is already initialized and available.
     * @return BML_RESULT_OK to continue, any other value aborts attach.
     */
    virtual BML_Result OnModuleAttach() { return BML_RESULT_OK; }

    /** @brief Extra cleanup before ShutdownHook runs. */
    virtual void OnModuleDetach() {}

    /**
     * @brief Release outbound dependencies before detach.
     * Return an error to abort unload/reload. Default: OK.
     */
    virtual BML_Result OnModulePrepareDetach() { return BML_RESULT_OK; }

    /**
     * @brief Register a hook with the core registry for visibility/conflict detection.
     *
     * Call from InitHook() after installing each hook. Registrations are
     * automatically unregistered when ShutdownHook() runs or the module detaches.
     *
     * @param name    Human-readable hook target (e.g. "CKInputManager::IsKeyDown")
     * @param address The original (pre-hook) address of the hooked function
     */
    void RegisterHook(const char *name, void *address) {
        auto hooks = Services().Hooks();
        if (hooks && address) {
            m_HookRegistrations.push_back(hooks.Register(name, address));
        }
    }

    /**
     * @brief Attempt hook initialization with a CKContext.
     *
     * Safe to call multiple times --returns immediately if already initialized.
     * On failure, ShutdownHook() is called to roll back partial installation.
     */
    bool TryInitHook(CKContext *ctx) {
        if (m_HookReady || !ctx) return m_HookReady;
        if (InitHook(ctx)) {
            m_HookReady = true;
            Services().Log().Info("%s hooks initialized", HookLogCategory());
        } else {
            ShutdownHook();
        }
        return m_HookReady;
    }

public:
    BML_Result OnAttach() override {
        m_Subs = Services().CreateSubscriptions();

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const imc::Message &msg) {
            if (m_HookReady) return;
            auto *payload = ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!payload) return;
            TryInitHook(payload->context);
        });

        return OnModuleAttach();
    }

    BML_Result OnPrepareDetach() override {
        return OnModulePrepareDetach();
    }

    void OnDetach() override {
        OnModuleDetach();
        m_HookRegistrations.clear();
        if (m_HookReady) {
            ShutdownHook();
            m_HookReady = false;
        }
    }
};

} // namespace bml

#endif /* BML_HOOK_MODULE_HPP */
