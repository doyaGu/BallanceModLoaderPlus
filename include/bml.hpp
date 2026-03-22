/**
 * @file bml.hpp
 * @brief BML v2 C++ Unified Header
 * 
 * Provides RAII-friendly, exception-safe, and idiomatic C++ interface
 * over the C API. Use this in C++ code for better ergonomics.
 * 
 * This header includes the default BML C++ wrapper components:
 *   - bml_context.hpp    - Context wrapper and API loading
 *   - bml_core.hpp       - Core utilities (version, mod, shutdown hooks)
 *   - bml_config.hpp     - Configuration access
 *   - bml_logger.hpp     - Logging utilities
 *   - bml_imc.hpp        - Inter-Mod Communication
 *   - bml_interface.hpp  - Interface lease and owned acquisition traits
 *   - bml_services.hpp   - BuiltinServices, RuntimeServiceHub, ModuleServices
 *   - bml_result.hpp     - Result type for error handling
 *   - bml_memory.hpp     - Memory allocation utilities
 *   - bml_resource.hpp   - Resource handle management
 *   - bml_locale.hpp     - Localization
 *   - bml_timer.hpp      - Timer scheduling
 *   - bml_hook.hpp       - Hook registry
 *   - bml_hot_reload.hpp - Hot reload lifecycle events
 *
 * Usage:
 *   #include <bml.hpp>
 *   
 *   // Initialize the bootstrap minimum (typically in BMLPlus or host)
 *   if (!bml::LoadAPI(get_proc_fn)) {
 *       // Handle error
 *   }
 *   
 *   auto configApi = bml::AcquireInterface<BML_CoreConfigInterface>(
 *       owner, BML_CORE_CONFIG_INTERFACE_ID, 1);
 *   bml::Config config(mod, configApi.Get());
 *   config.SetString("mymod", "key", "value");
 *   
 *   auto value = config.GetString("mymod", "key").value_or("default");
 * 
 * For selective inclusion, include individual headers instead:
 *   #include <bml_context.hpp>
 *   #include <bml_logger.hpp>
 */

#ifndef BML_HPP
#define BML_HPP

// Core components
#include "bml_context.hpp"    // Context, ScopedContext, LoadAPI, UnloadAPI, IsApiLoaded
#include "bml_core.hpp"       // Version utilities, Mod, ShutdownHook
#include "bml_config.hpp"     // Config
#include "bml_logger.hpp"     // Logger, LogLevel

// Communication
#include "bml_imc.hpp"        // Imc, Imc::Subscription, ImcCallback
#include "bml_interface.hpp"  // InterfaceLease, AcquireInterface, Acquire

// Services
#include "bml_services.hpp"   // BuiltinServices, RuntimeServiceHub, ModuleServices

// Error Handling
#include "bml_result.hpp"     // Result<T>, Ok, Err, BML_TRY, BML_TRY_ASSIGN

// Memory
#include "bml_memory.hpp"     // Alloc, Free, MemoryPool, UniquePtr

// Resources
#include "bml_resource.hpp"   // Handle, SharedHandle

// Localization
#include "bml_locale.hpp"     // Locale

// Timer
#include "bml_timer.hpp"      // TimerService

// Hook Registry
#include "bml_hook.hpp"       // HookRegistration, HookRegistry, HookInfo

// Hot Reload
#include "bml_hot_reload.hpp" // ModLifecycleEvent, ModLifecycleEventBuilder

#endif /* BML_HPP */
