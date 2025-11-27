/**
 * @file bml.hpp
 * @brief BML v2 C++ Unified Header
 * 
 * Provides RAII-friendly, exception-safe, and idiomatic C++ interface
 * over the C API. Use this in C++ code for better ergonomics.
 * 
 * This header includes all BML C++ wrapper components:
 *   - bml_context.hpp   - Context wrapper and API loading
 *   - bml_config.hpp    - Configuration access
 *   - bml_imc.hpp       - Inter-Mod Communication
 *   - bml_extension.hpp - Extension management
 *   - bml_logger.hpp    - Logging utilities
 *   - bml_capability.hpp - Capability queries and API discovery
 *   - bml_result.hpp    - Result type for error handling
 *   - bml_core.hpp      - Core utilities (version, mod, shutdown hooks)
 *   - bml_sync.hpp      - Synchronization primitives (mutex, rwlock, etc.)
 *   - bml_memory.hpp    - Memory allocation utilities
 *   - bml_resource.hpp  - Resource handle management
 *   - bml_profiling.hpp - Profiling and tracing utilities
 *   - bml_hot_reload.hpp - Hot reload lifecycle events
 * 
 * Usage:
 *   #include <bml.hpp>
 *   
 *   // Initialize (typically in BMLPlus or host)
 *   if (!bml::LoadAPI(get_proc_fn)) {
 *       // Handle error
 *   }
 *   
 *   // Use wrapper classes
 *   bml::Context ctx = bml::GetGlobalContext();
 *   bml::Config config(mod);
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

// Extensions
#include "bml_extension.hpp"  // Extension

// Capabilities and Discovery
#include "bml_capability.hpp" // QueryCapabilities, HasCapability, CheckCompatibility, ApiDescriptor

// Error Handling
#include "bml_result.hpp"     // Result<T>, Ok, Err, BML_TRY, BML_TRY_ASSIGN

// Synchronization
#include "bml_sync.hpp"       // Mutex, RwLock, Semaphore, CondVar, SpinLock, ThreadLocal

// Memory
#include "bml_memory.hpp"     // Alloc, Free, MemoryPool, UniquePtr

// Resources
#include "bml_resource.hpp"   // Handle, SharedHandle

// Profiling
#include "bml_profiling.hpp"  // ScopedTrace, Timer, trace functions

// Hot Reload
#include "bml_hot_reload.hpp" // ModLifecycleEvent, ModLifecycleEventBuilder

#endif /* BML_HPP */
