#ifndef BML_CAPABILITIES_H
#define BML_CAPABILITIES_H

/**
 * @file bml_capabilities.h
 * @brief API capability flags for runtime feature detection
 * 
 * This file defines capability bits that allow mods to query available
 * features at runtime and enable graceful degradation when features
 * are unavailable.
 * 
 * Usage:
 *   if (bmlHasCapability(BML_CAP_IMC_RPC)) {
 *       // Use RPC features
 *   } else {
 *       // Fallback to basic pub/sub
 *   }
 * 
 * @see docs/api-improvements/03-feature-extensions.md
 */

#include "bml_types.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Capability Bit Flags (64-bit bitmask)
 * ======================================================================== */

/**
 * @brief API capability flags for feature detection
 * 
 * Capabilities are organized into categories:
 * - Bits 0-15:  Core IMC capabilities
 * - Bits 16-23: Synchronization capabilities
 * - Bits 24-31: Extension capabilities
 * - Bits 32-47: Resource/Memory capabilities
 * - Bits 48-63: Reserved for future use
 */
typedef uint64_t BML_ApiCapability;

/* IMC Capabilities (0-15) */
#define BML_CAP_IMC_BASIC       (1ULL << 0)   /**< Basic Pub/Sub messaging */
#define BML_CAP_IMC_BUFFER      (1ULL << 1)   /**< Zero-copy buffer support */
#define BML_CAP_IMC_RPC         (1ULL << 2)   /**< RPC call support */
#define BML_CAP_IMC_FUTURE      (1ULL << 3)   /**< Async Future support */
#define BML_CAP_IMC_ID_BASED    (1ULL << 4)   /**< ID-based fast path APIs */
#define BML_CAP_IMC_DISPATCH    (1ULL << 5)   /**< Message dispatch support */

/* Synchronization Capabilities (16-23) */
#define BML_CAP_SYNC_MUTEX      (1ULL << 16)  /**< Mutex support */
#define BML_CAP_SYNC_RWLOCK     (1ULL << 17)  /**< Read-write lock support */
#define BML_CAP_SYNC_SEMAPHORE  (1ULL << 18)  /**< Semaphore support */
#define BML_CAP_SYNC_ATOMIC     (1ULL << 19)  /**< Atomic operations */
#define BML_CAP_SYNC_TLS        (1ULL << 20)  /**< Thread-local storage */

/* Extension Capabilities (24-31) */
#define BML_CAP_EXTENSION_BASIC (1ULL << 24)  /**< Basic extension system */
#define BML_CAP_EXTENSION_VERSIONED (1ULL << 25) /**< Versioned extension loading */
#define BML_CAP_EXTENSION_IMGUI (1ULL << 26)  /**< ImGui extension available */
#define BML_CAP_CONTEXT         (1ULL << 27)  /**< Context management APIs */
#define BML_CAP_RUNTIME         (1ULL << 28)  /**< Runtime query APIs */
#define BML_CAP_MOD_INFO        (1ULL << 29)  /**< Mod metadata APIs */
#define BML_CAP_LIFECYCLE       (1ULL << 30)  /**< Lifecycle management (shutdown hooks) */

/* Resource/Memory Capabilities (32-47) */
#define BML_CAP_MEMORY_POOL     (1ULL << 32)  /**< Memory pool support */
#define BML_CAP_MEMORY_ALIGNED  (1ULL << 33)  /**< Aligned allocation support */
#define BML_CAP_HANDLE_SYSTEM   (1ULL << 34)  /**< Handle management system */

/* Profiling Capabilities (48-55) */
#define BML_CAP_PROFILING_TRACE (1ULL << 48)  /**< Tracing support */
#define BML_CAP_PROFILING_STATS (1ULL << 49)  /**< Statistics collection */
#define BML_CAP_API_TRACING     (1ULL << 50)  /**< API call tracing */
#define BML_CAP_DIAGNOSTICS     (1ULL << 51)  /**< Diagnostics/error handling */
#define BML_CAP_CAPABILITY_QUERY (1ULL << 52) /**< Capability query system */

/* Configuration Capabilities (56-63) */
#define BML_CAP_CONFIG_BASIC    (1ULL << 56)  /**< Basic configuration */
#define BML_CAP_LOGGING         (1ULL << 57)  /**< Logging support */
#define BML_CAP_MEMORY_BASIC    (1ULL << 58)  /**< Basic memory allocation */

/* ========================================================================
 * API Type Classification
 * ======================================================================== */

/**
 * @brief Classification of API types
 * 
 * Used to distinguish between core BML APIs, official extensions,
 * and third-party mod-provided extensions.
 */
typedef enum BML_ApiType {
    BML_API_TYPE_CORE        = 0,  /**< Core BML API (id < 50000) */
    BML_API_TYPE_EXTENSION   = 1,  /**< Official BML extension (BML_EXT_*) */
    BML_API_TYPE_THIRD_PARTY = 2,  /**< Third-party mod extension */
} BML_ApiType;

/* ========================================================================
 * API ID Ranges
 * ======================================================================== */

/**
 * @brief Starting ID for extension APIs
 * 
 * Core APIs use IDs 1-49999, extensions use 50000+
 */
#define BML_EXTENSION_ID_START 50000u

/**
 * @brief Maximum API ID supported
 */
#define BML_MAX_API_ID 100000u

/* ========================================================================
 * Version Requirement Structure
 * ======================================================================== */

/**
 * @brief Version compatibility requirements
 * 
 * Used by mods to declare minimum version requirements and
 * required capabilities.
 */
typedef struct BML_VersionRequirement {
    size_t struct_size;       /**< sizeof(BML_VersionRequirement), must be first field */
    uint16_t min_major;       /**< Minimum required major version */
    uint16_t min_minor;       /**< Minimum required minor version */
    uint16_t min_patch;       /**< Minimum required patch version */
    uint16_t reserved;        /**< Reserved for future use */
    uint64_t required_caps;   /**< Required capability flags */
} BML_VersionRequirement;

/**
 * @def BML_VERSION_REQUIREMENT_INIT
 * @brief Static initializer for BML_VersionRequirement
 */
#define BML_VERSION_REQUIREMENT_INIT(maj, min, pat) \
    { sizeof(BML_VersionRequirement), (maj), (min), (pat), 0, 0 }

/* ========================================================================
 * API Descriptor Structure
 * ======================================================================== */

/**
 * @brief Unified API descriptor for both core and extension APIs
 * 
 * This structure provides comprehensive metadata about any registered API,
 * enabling runtime discovery, capability querying, and version negotiation.
 */
typedef struct BML_ApiDescriptor {
    size_t struct_size;           /**< sizeof(BML_ApiDescriptor), must be first field */
    uint32_t id;                  /**< Stable API ID */
    const char* name;             /**< API name (e.g., "bmlImcPublish") */
    BML_ApiType type;             /**< API type classification */
    
    /* Version information */
    uint16_t version_major;       /**< Major version when introduced */
    uint16_t version_minor;       /**< Minor version when introduced */
    uint16_t version_patch;       /**< Patch version when introduced */
    uint16_t reserved;            /**< Reserved for alignment */
    
    /* Capability and threading */
    uint64_t capabilities;        /**< Capability flags this API provides */
    BML_ThreadingModel threading; /**< Thread safety model */
    
    /* Provider information */
    const char* provider_mod;     /**< Provider ("BML" for core APIs) */
    const char* description;      /**< Human-readable description */
    
    /* Runtime statistics */
    uint64_t call_count;          /**< Number of times called */
} BML_ApiDescriptor;

/**
 * @def BML_API_DESCRIPTOR_INIT
 * @brief Static initializer for BML_ApiDescriptor
 */
#define BML_API_DESCRIPTOR_INIT \
    { sizeof(BML_ApiDescriptor), 0, NULL, BML_API_TYPE_CORE, 0, 0, 0, 0, 0, BML_THREADING_SINGLE, NULL, NULL, 0 }

/* ========================================================================
 * Function Pointer Types - Capability Query
 * ======================================================================== */

/**
 * @brief Query all available capabilities
 * @return Bitmask of available capabilities
 */
typedef uint64_t (*PFN_BML_QueryCapabilities)(void);

/**
 * @brief Check if a specific capability is available
 * @param cap Capability flag to check
 * @return BML_TRUE if capability is available, BML_FALSE otherwise
 */
typedef BML_Bool (*PFN_BML_HasCapability)(uint64_t cap);

/**
 * @brief Check version compatibility
 * @param requirement Pointer to version requirement structure
 * @return BML_OK if compatible, error code otherwise
 */
typedef int (*PFN_BML_CheckCompatibility)(const BML_VersionRequirement* requirement);

/* ========================================================================
 * Function Pointer Types - API Discovery
 * ======================================================================== */

/**
 * @brief Get API descriptor by ID
 * @param id API ID to query
 * @param out_desc Pointer to receive descriptor
 * @return BML_TRUE if found, BML_FALSE otherwise
 */
typedef BML_Bool (*PFN_BML_GetApiDescriptor)(uint32_t id, BML_ApiDescriptor* out_desc);

/**
 * @brief Get API descriptor by name
 * @param name API name to query
 * @param out_desc Pointer to receive descriptor
 * @return BML_TRUE if found, BML_FALSE otherwise
 */
typedef BML_Bool (*PFN_BML_GetApiDescriptorByName)(const char* name, BML_ApiDescriptor* out_desc);

/**
 * @brief Callback type for API enumeration
 * 
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] desc Pointer to API descriptor
 * @param[in] user_data User-provided context (always last parameter)
 * @return BML_TRUE to continue enumeration, BML_FALSE to stop
 */
typedef BML_Bool (*PFN_BML_ApiEnumerator)(
    BML_Context ctx,
    const BML_ApiDescriptor* desc, 
    void* user_data
);

/**
 * @brief Enumerate all registered APIs
 * @param callback Function called for each API
 * @param user_data User context passed to callback
 * @param type_filter Filter by API type (-1 = all types)
 */
typedef void (*PFN_BML_EnumerateApis)(
    PFN_BML_ApiEnumerator callback, 
    void* user_data,
    BML_ApiType type_filter
);

/**
 * @brief Get the version when an API was introduced
 * @param id API ID to query
 * @return Encoded version (major<<16 | minor<<8 | patch), or 0 if not found
 */
typedef uint32_t (*PFN_BML_GetApiIntroducedVersion)(uint32_t id);

/* ========================================================================
 * Function Pointer Types - Extension Registration
 * ======================================================================== */

/**
 * @brief Register an extension API
 * @param name Extension name (e.g., "MyMod_EXT_Physics")
 * @param version_major Major version number
 * @param version_minor Minor version number
 * @param api_table Pointer to API function table
 * @param api_size Size of API table in bytes
 * @return Assigned API ID, or 0 on failure
 */
typedef uint32_t (*PFN_BML_RegisterExtensionApi)(
    const char* name,
    uint32_t version_major,
    uint32_t version_minor,
    const void* api_table,
    size_t api_size
);

/* ========================================================================
 * Global Function Pointers
 * ======================================================================== */

extern PFN_BML_QueryCapabilities       bmlQueryCapabilities;
extern PFN_BML_HasCapability           bmlHasCapability;
extern PFN_BML_CheckCompatibility      bmlCheckCompatibility;
extern PFN_BML_GetApiDescriptor        bmlGetApiDescriptor;
extern PFN_BML_GetApiDescriptorByName  bmlGetApiDescriptorByName;
extern PFN_BML_EnumerateApis           bmlEnumerateApis;
extern PFN_BML_GetApiIntroducedVersion bmlGetApiIntroducedVersion;

BML_END_CDECLS

#endif /* BML_CAPABILITIES_H */
