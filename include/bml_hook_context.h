#ifndef BML_HOOK_CONTEXT_H
#define BML_HOOK_CONTEXT_H

/**
 * @file bml_hook_context.h
 * @brief Lightweight service context for hook files
 *
 * Hook callbacks run from within MinHook/VTable trampolines where the owning
 * module's C++ state may be unavailable. Instead of storing a raw pointer to
 * the full ModuleServices object (which contains C++ types and has a lifetime
 * tied to the module DLL), hooks receive a plain-C HookContext that holds only
 * stable vtable pointers.
 *
 * These pointers reference static globals inside BML.dll that are registered
 * with BML_INTERFACE_FLAG_IMMUTABLE and outlive all modules, so there is no
 * use-after-free risk even if the owning module is unloaded.
 *
 * Usage in hook files:
 * @code
 * static BML_HookContext s_Hook = {};
 *
 * bool InitMyHook(const BML_HookContext *ctx) {
 *     s_Hook = *ctx;
 *     // use s_Hook.imc_bus->Publish(...)
 * }
 *
 * void ShutdownMyHook() {
 *     s_Hook = {};
 * }
 * @endcode
 */

#include "bml_export.h"
#include "bml_logging.h"
#include "bml_imc.h"

BML_BEGIN_CDECLS

/**
 * @brief Plain-C service context for hook files.
 *
 * All pointers reference immutable static vtables in BML.dll.
 * Copying this struct is safe and cheap.
 */
typedef struct BML_HookContext {
    const BML_ImcBusInterface *imc_bus;
    const BML_CoreLoggingInterface *logging;
    BML_Context global_context;
    BML_Mod owner;
    const char *log_category;
} BML_HookContext;

/** @brief Zero-initialized HookContext literal. */
#define BML_HOOK_CONTEXT_INIT { NULL, NULL, NULL, NULL, NULL }

BML_END_CDECLS

#ifdef BML_SERVICES_HPP
/**
 * @brief Build a HookContext from ModuleServices.
 *
 * Only available when bml_services.hpp has been included before this header.
 * Call from the module's OnAttach or Engine/Init handler and pass the result
 * to the hook's Init function.
 */
inline BML_HookContext BML_MakeHookContext(const bml::ModuleServices &services,
                                           const char *log_category = nullptr) {
    BML_HookContext ctx = BML_HOOK_CONTEXT_INIT;
    ctx.imc_bus = services.Interfaces().ImcBus;
    ctx.logging = services.Interfaces().Logging;
    ctx.global_context = services.GlobalContext();
    ctx.owner = services.Handle();
    ctx.log_category = log_category;
    return ctx;
}
#endif /* BML_SERVICES_HPP */

#endif /* BML_HOOK_CONTEXT_H */
