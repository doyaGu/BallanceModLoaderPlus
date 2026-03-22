/**
 * @file bml_config.h
 * @brief Configuration storage API
 *
 * Config APIs are runtime services. Resolve these function pointers through
 * `bmlGetProcAddress(...)` in C code or acquire `bml.core.config` in module
 * code. The bootstrap loader does not publish config globals.
 */

#ifndef BML_CONFIG_H
#define BML_CONFIG_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

/**
 * @brief Configuration key identifier
 */
typedef struct BML_ConfigKey {
    size_t struct_size;   /**< sizeof(BML_ConfigKey), must be first field */
    const char *category; /**< Config category (e.g., "video", "audio") */
    const char *name;     /**< Key name within category */
} BML_ConfigKey;

/**
 * @def BML_CONFIG_KEY_INIT
 * @brief Static initializer for BML_ConfigKey
 */
#define BML_CONFIG_KEY_INIT(cat, n) { sizeof(BML_ConfigKey), (cat), (n) }

typedef enum BML_ConfigType {
    BML_CONFIG_BOOL              = 0,
    BML_CONFIG_INT               = 1,
    BML_CONFIG_FLOAT             = 2,
    BML_CONFIG_STRING            = 3,
    BML_CONFIG_BLOB              = 4, /**< Binary blob (base64 in TOML) */
    _BML_CONFIG_TYPE_FORCE_32BIT = 0x7FFFFFFF /**< Force 32-bit enum */
} BML_ConfigType;

/** @brief Config entry flags */
#define BML_CONFIG_FLAG_NONE     0u
/** @brief Entry is internal (not shown in ModMenu UI) */
#define BML_CONFIG_FLAG_INTERNAL (1u << 0)

/**
 * @brief Configuration value container
 *
 * New fields (blob_size, flags) are appended at the end for ABI compatibility.
 * Old code with smaller struct_size simply doesn't see them, and the runtime
 * treats absent fields as zero (no blob, no flags).
 */
typedef struct BML_ConfigValue {
    size_t struct_size;  /**< sizeof(BML_ConfigValue), must be first field */
    BML_ConfigType type; /**< Value type discriminator */
    union {
        BML_Bool bool_value;
        int32_t int_value;
        float float_value;
        const char *string_value;
        const void *blob_value;  /**< Pointer to blob data (type == BML_CONFIG_BLOB) */
    } data;
    size_t blob_size;    /**< Blob size in bytes (only when type == BML_CONFIG_BLOB) */
    uint32_t flags;      /**< BML_CONFIG_FLAG_* */
} BML_ConfigValue;

/**
 * @def BML_CONFIG_VALUE_INIT_BOOL
 * @brief Static initializer for boolean config value
 */
#define BML_CONFIG_VALUE_INIT_BOOL(v) { sizeof(BML_ConfigValue), BML_CONFIG_BOOL, { .bool_value = (v) }, 0, 0 }

/**
 * @def BML_CONFIG_VALUE_INIT_INT
 * @brief Static initializer for integer config value
 */
#define BML_CONFIG_VALUE_INIT_INT(v) { sizeof(BML_ConfigValue), BML_CONFIG_INT, { .int_value = (v) }, 0, 0 }

/**
 * @def BML_CONFIG_VALUE_INIT_FLOAT
 * @brief Static initializer for float config value
 */
#define BML_CONFIG_VALUE_INIT_FLOAT(v) { sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, { .float_value = (v) }, 0, 0 }

/**
 * @def BML_CONFIG_VALUE_INIT_STRING
 * @brief Static initializer for string config value
 */
#define BML_CONFIG_VALUE_INIT_STRING(v) { sizeof(BML_ConfigValue), BML_CONFIG_STRING, { .string_value = (v) }, 0, 0 }

/**
 * @def BML_CONFIG_VALUE_INIT_BLOB
 * @brief Static initializer for binary blob config value
 */
#define BML_CONFIG_VALUE_INIT_BLOB(ptr, sz) { sizeof(BML_ConfigValue), BML_CONFIG_BLOB, { .blob_value = (ptr) }, (sz), 0 }

/**
 * @def BML_CONFIG_VALUE_INIT_INTERNAL_INT
 * @brief Static initializer for internal (hidden from UI) integer config value
 */
#define BML_CONFIG_VALUE_INIT_INTERNAL_INT(v) { sizeof(BML_ConfigValue), BML_CONFIG_INT, { .int_value = (v) }, 0, BML_CONFIG_FLAG_INTERNAL }

/**
 * @def BML_CONFIG_VALUE_INIT_INTERNAL_STRING
 * @brief Static initializer for internal (hidden from UI) string config value
 */
#define BML_CONFIG_VALUE_INIT_INTERNAL_STRING(v) { sizeof(BML_ConfigValue), BML_CONFIG_STRING, { .string_value = (v) }, 0, BML_CONFIG_FLAG_INTERNAL }

#define BML_CONFIG_TYPE_MASK(type) (1u << (type))

/* ========================================================================
 * Config Batch Operations
 *
 * Allows grouping multiple config changes into an atomic transaction.
 * Either all changes are committed together, or all are discarded.
 * ======================================================================== */

/**
 * @brief Config batch handle for atomic multi-key updates
 * @threadsafe Batch operations are NOT thread-safe; use one batch per thread
 */
BML_DECLARE_HANDLE(BML_ConfigBatch);

/**
 * @brief Begin a configuration batch transaction
 *
 * Creates a new batch for collecting config changes. Multiple Set operations
 * can be queued before atomically committing or discarding all changes.
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[out] out_batch Receives the batch handle. Must not be NULL.
 *
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if out_batch is NULL
 * @return BML_RESULT_NOT_SUPPORTED if batch operations are not available
 * @return BML_RESULT_OUT_OF_MEMORY if batch cannot be allocated
 *
 * @threadsafe No (each batch must be used from a single thread)
 *
 * @code
 * BML_ConfigBatch batch;
 * if (configBatchBegin(mod, &batch) == BML_RESULT_OK) {
 *     configBatchSet(batch, &key1, &value1);
 *     configBatchSet(batch, &key2, &value2);
 *     configBatchCommit(batch);  // Atomic commit
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ConfigBatchBegin)(BML_Mod mod, BML_ConfigBatch *out_batch);

/**
 * @brief Queue a configuration value change in a batch
 *
 * Adds a key-value pair to the batch. The change is not applied until
 * PFN_BML_ConfigBatchCommit is called.
 *
 * @param[in] batch Batch handle from bmlConfigBatchBegin. Must not be NULL.
 * @param[in] key Configuration key. Must not be NULL. struct_size must be initialized.
 * @param[in] value Configuration value. Must not be NULL. struct_size must be initialized.
 *
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if batch, key, or value is NULL
 * @return BML_RESULT_INVALID_STATE if batch was already committed or discarded
 * @return BML_RESULT_OUT_OF_MEMORY if batch storage is exhausted
 *
 * @threadsafe No
 */
typedef BML_Result (*PFN_BML_ConfigBatchSet)(BML_ConfigBatch batch,
                                             const BML_ConfigKey *key,
                                             const BML_ConfigValue *value);

/**
 * @brief Atomically commit all changes in a batch
 *
 * Applies all queued changes at once. If successful, the batch is consumed
 * and the handle becomes invalid. On failure, the batch remains valid and
 * can be retried or discarded.
 *
 * @param[in] batch Batch handle from bmlConfigBatchBegin. Must not be NULL.
 *
 * @return BML_RESULT_OK on success (batch is consumed)
 * @return BML_RESULT_INVALID_ARGUMENT if batch is NULL
 * @return BML_RESULT_INVALID_STATE if batch was already committed or discarded
 * @return BML_RESULT_IO_ERROR if config file cannot be written
 *
 * @threadsafe No
 * @note After successful commit, the batch handle is invalidated.
 */
typedef BML_Result (*PFN_BML_ConfigBatchCommit)(BML_ConfigBatch batch);

/**
 * @brief Discard a batch without applying changes
 *
 * Releases all resources associated with the batch without applying any
 * queued changes. The batch handle becomes invalid after this call.
 *
 * @param[in] batch Batch handle from bmlConfigBatchBegin. Must not be NULL.
 *
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if batch is NULL
 * @return BML_RESULT_INVALID_STATE if batch was already committed or discarded
 *
 * @threadsafe No
 * @note Always succeeds for valid uncommitted batches.
 */
typedef BML_Result (*PFN_BML_ConfigBatchDiscard)(BML_ConfigBatch batch);

/**
 * @brief Get a configuration value
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[in] key Configuration key. Must not be NULL. struct_size must be initialized.
 * @param[out] out_value Receives the configuration value. Must not be NULL.
 *                       struct_size should be pre-initialized.
 *
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if key or out_value is NULL
 * @return BML_RESULT_NOT_FOUND if key doesn't exist
 *
 * @threadsafe Yes (read operation)
 */
typedef BML_Result (*PFN_BML_ConfigGet)(BML_Mod mod, const BML_ConfigKey *key, BML_ConfigValue *out_value);

/**
 * @brief Set a configuration value
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[in] key Configuration key. Must not be NULL. struct_size must be initialized.
 * @param[in] value Configuration value to set. Must not be NULL.
 *                  struct_size must be initialized.
 *
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if key or value is NULL
 * @return BML_RESULT_IO_ERROR if config file cannot be written
 *
 * @threadsafe No (use external synchronization for concurrent writes)
 */
typedef BML_Result (*PFN_BML_ConfigSet)(BML_Mod mod, const BML_ConfigKey *key, const BML_ConfigValue *value);

/**
 * @brief Reset a configuration key to its default value
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[in] key Configuration key. Must not be NULL. struct_size must be initialized.
 *
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if key is NULL
 * @return BML_RESULT_NOT_FOUND if key doesn't exist
 *
 * @threadsafe No (use external synchronization for concurrent modifications)
 */
typedef BML_Result (*PFN_BML_ConfigReset)(BML_Mod mod, const BML_ConfigKey *key);
/**
 * @brief Config enumeration callback
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] key Configuration key
 * @param[in] value Configuration value
 * @param[in] user_data User-provided context (always last parameter)
 */
typedef void (*BML_ConfigEnumCallback)(BML_Context ctx,
                                       const BML_ConfigKey *key,
                                       const BML_ConfigValue *value,
                                       void *user_data);

typedef BML_Result (*PFN_BML_ConfigEnumerate)(BML_Mod mod,
                                              BML_ConfigEnumCallback callback,
                                              void *user_data);

/* ========================================================================
 * Config Typed Shortcuts
 *
 * Convenience wrappers that combine key lookup + type extraction in one call.
 * If the key is not found, the default_value is returned with BML_RESULT_OK.
 * ======================================================================== */

/**
 * @brief Get an integer config value with a default value
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[in] category Config category. Must not be NULL.
 * @param[in] name Config key name. Must not be NULL.
 * @param[in] default_value Value returned if key not found
 * @param[out] out_value Receives the value. Must not be NULL.
 *
 * @return BML_RESULT_OK on success (key found or default returned)
 * @return BML_RESULT_INVALID_ARGUMENT if category, name, or out_value is NULL
 * @return BML_RESULT_CONFIG_TYPE_MISMATCH if key exists but is not an integer
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ConfigGetInt)(BML_Mod mod, const char *category,
                                           const char *name, int32_t default_value,
                                           int32_t *out_value);

/**
 * @brief Get a float config value with a default value
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[in] category Config category. Must not be NULL.
 * @param[in] name Config key name. Must not be NULL.
 * @param[in] default_value Value returned if key not found
 * @param[out] out_value Receives the value. Must not be NULL.
 *
 * @return BML_RESULT_OK on success (key found or default returned)
 * @return BML_RESULT_INVALID_ARGUMENT if category, name, or out_value is NULL
 * @return BML_RESULT_CONFIG_TYPE_MISMATCH if key exists but is not a float
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ConfigGetFloat)(BML_Mod mod, const char *category,
                                              const char *name, float default_value,
                                              float *out_value);

/**
 * @brief Get a boolean config value with a default value
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[in] category Config category. Must not be NULL.
 * @param[in] name Config key name. Must not be NULL.
 * @param[in] default_value Value returned if key not found
 * @param[out] out_value Receives the value. Must not be NULL.
 *
 * @return BML_RESULT_OK on success (key found or default returned)
 * @return BML_RESULT_INVALID_ARGUMENT if category, name, or out_value is NULL
 * @return BML_RESULT_CONFIG_TYPE_MISMATCH if key exists but is not a boolean
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ConfigGetBool)(BML_Mod mod, const char *category,
                                             const char *name, BML_Bool default_value,
                                             BML_Bool *out_value);

/**
 * @brief Get a string config value with a default value
 *
 * @param[in] mod Mod handle. Must not be NULL.
 * @param[in] category Config category. Must not be NULL.
 * @param[in] name Config key name. Must not be NULL.
 * @param[in] default_value Value returned if key not found (may be NULL)
 * @param[out] out_value Receives pointer to the string. Must not be NULL.
 *                       If default returned, points to default_value.
 *
 * @return BML_RESULT_OK on success (key found or default returned)
 * @return BML_RESULT_INVALID_ARGUMENT if category, name, or out_value is NULL
 * @return BML_RESULT_CONFIG_TYPE_MISMATCH if key exists but is not a string
 *
 * @threadsafe Yes (internally synchronized read)
 * @warning The returned string pointer for found keys points into the config
 *          store's internal storage. It is valid until the next ConfigSet or
 *          ConfigReset on the same key. Copy immediately if needed across
 *          mutation boundaries. When the default is returned, the pointer
 *          equals default_value and has whatever lifetime the caller gave it.
 */
typedef BML_Result (*PFN_BML_ConfigGetString)(BML_Mod mod, const char *category,
                                               const char *name, const char *default_value,
                                               const char **out_value);

/* ========================================================================
 * Config Load Hooks (for observing config file loading)
 * ======================================================================== */

/**
 * @brief Context information for config load callbacks
 */
typedef struct BML_ConfigLoadContext {
    size_t struct_size;      /**< sizeof(BML_ConfigLoadContext), must be first */
    BML_Version api_version; /**< API version */
    BML_Mod mod;             /**< Mod whose config is being loaded */
    const char *mod_id;      /**< Mod ID string (may be NULL) */
    const char *config_path; /**< Path to config file (may be NULL) */
} BML_ConfigLoadContext;

#define BML_CONFIG_LOAD_CONTEXT_INIT { sizeof(BML_ConfigLoadContext), BML_VERSION_INIT(0,0,0), NULL, NULL, NULL }

/**
 * @brief Callback for config file load events
 * @param ctx BML context
 * @param load_ctx Config load context with mod and file info
 * @param user_data User context from registration
 */
typedef void (*BML_ConfigLoadCallback)(BML_Context ctx, const BML_ConfigLoadContext *load_ctx, void *user_data);

/**
 * @brief Descriptor for registering config load hooks
 */
typedef struct BML_ConfigLoadHooks {
    size_t struct_size;                  /**< sizeof(BML_ConfigLoadHooks), must be first */
    BML_ConfigLoadCallback on_pre_load;  /**< Called before config file is loaded */
    BML_ConfigLoadCallback on_post_load; /**< Called after config file is loaded */
    void *user_data;                     /**< User context passed to callbacks */
} BML_ConfigLoadHooks;

#define BML_CONFIG_LOAD_HOOKS_INIT { sizeof(BML_ConfigLoadHooks), NULL, NULL, NULL }

/**
 * @brief Register config load hooks to observe config file loading
 * @param hooks Hook descriptor
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if hooks is invalid
 */
typedef BML_Result (*PFN_BML_RegisterConfigLoadHooks)(BML_Mod owner,
                                                      const BML_ConfigLoadHooks *hooks);

BML_END_CDECLS

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * ======================================================================== */

#ifdef __cplusplus
#include <cstddef>
#define BML_CONFIG_OFFSETOF(type, member) offsetof(type, member)
#else
#include <stddef.h>
#define BML_CONFIG_OFFSETOF(type, member) offsetof(type, member)
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
#define BML_CONFIG_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define BML_CONFIG_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define BML_CONFIG_STATIC_ASSERT_CONCAT_(a, b) a##b
#define BML_CONFIG_STATIC_ASSERT_CONCAT(a, b) BML_CONFIG_STATIC_ASSERT_CONCAT_(a, b)
#define BML_CONFIG_STATIC_ASSERT(cond, msg) \
        typedef char BML_CONFIG_STATIC_ASSERT_CONCAT(bml_config_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* Verify struct_size is at offset 0 */
BML_CONFIG_STATIC_ASSERT(BML_CONFIG_OFFSETOF(BML_ConfigKey, struct_size) == 0,
                         "BML_ConfigKey.struct_size must be at offset 0");
BML_CONFIG_STATIC_ASSERT(BML_CONFIG_OFFSETOF(BML_ConfigValue, struct_size) == 0,
                         "BML_ConfigValue.struct_size must be at offset 0");

/* Verify enum sizes are 32-bit */
BML_CONFIG_STATIC_ASSERT(sizeof(BML_ConfigType) == sizeof(int32_t),
                         "BML_ConfigType must be 32-bit");

#endif /* BML_CONFIG_H */
