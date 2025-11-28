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
    _BML_CONFIG_TYPE_FORCE_32BIT = 0x7FFFFFFF /**< Force 32-bit enum */
} BML_ConfigType;

/**
 * @brief Configuration value container
 */
typedef struct BML_ConfigValue {
    size_t struct_size;  /**< sizeof(BML_ConfigValue), must be first field */
    BML_ConfigType type; /**< Value type discriminator */
    union {
        BML_Bool bool_value;
        int32_t int_value;
        float float_value;
        const char *string_value;
    } data;
} BML_ConfigValue;

/**
 * @def BML_CONFIG_VALUE_INIT_BOOL
 * @brief Static initializer for boolean config value
 */
#define BML_CONFIG_VALUE_INIT_BOOL(v) { sizeof(BML_ConfigValue), BML_CONFIG_BOOL, { .bool_value = (v) } }

/**
 * @def BML_CONFIG_VALUE_INIT_INT
 * @brief Static initializer for integer config value
 */
#define BML_CONFIG_VALUE_INIT_INT(v) { sizeof(BML_ConfigValue), BML_CONFIG_INT, { .int_value = (v) } }

/**
 * @def BML_CONFIG_VALUE_INIT_FLOAT
 * @brief Static initializer for float config value
 */
#define BML_CONFIG_VALUE_INIT_FLOAT(v) { sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, { .float_value = (v) } }

/**
 * @def BML_CONFIG_VALUE_INIT_STRING
 * @brief Static initializer for string config value
 */
#define BML_CONFIG_VALUE_INIT_STRING(v) { sizeof(BML_ConfigValue), BML_CONFIG_STRING, { .string_value = (v) } }

#define BML_CONFIG_TYPE_MASK(type) (1u << (type))

typedef enum BML_ConfigCapabilityFlags {
    BML_CONFIG_CAP_GET          = 1u << 0,
    BML_CONFIG_CAP_SET          = 1u << 1,
    BML_CONFIG_CAP_RESET        = 1u << 2,
    BML_CONFIG_CAP_ENUMERATE    = 1u << 3,
    BML_CONFIG_CAP_PERSISTENCE  = 1u << 4,
    BML_CONFIG_CAP_BATCH        = 1u << 5,   /**< Batch operations supported */
    _BML_CONFIG_CAP_FORCE_32BIT = 0x7FFFFFFF /**< Force 32-bit enum */
} BML_ConfigCapabilityFlags;

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
 * @param[in] mod Mod handle. Can be NULL to use current module context.
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
 * if (bmlConfigBatchBegin(mod, &batch) == BML_RESULT_OK) {
 *     bmlConfigBatchSet(batch, &key1, &value1);
 *     bmlConfigBatchSet(batch, &key2, &value2);
 *     bmlConfigBatchCommit(batch);  // Atomic commit
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ConfigBatchBegin)(BML_Mod mod, BML_ConfigBatch *out_batch);

/**
 * @brief Queue a configuration value change in a batch
 *
 * Adds a key-value pair to the batch. The change is not applied until
 * bmlConfigBatchCommit is called.
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
 * @brief Configuration store capabilities
 */
typedef struct BML_ConfigStoreCaps {
    size_t struct_size;                 /**< sizeof(BML_ConfigStoreCaps), must be first field */
    BML_Version api_version;            /**< API version */
    uint32_t feature_flags;             /**< Bitmask of BML_ConfigCapabilityFlags */
    uint32_t supported_type_mask;       /**< Bitmask of supported BML_ConfigType values */
    uint32_t max_category_length;       /**< Maximum category name length */
    uint32_t max_name_length;           /**< Maximum key name length */
    uint32_t max_string_bytes;          /**< Maximum string value length */
    BML_ThreadingModel threading_model; /**< Threading model of Config APIs */
} BML_ConfigStoreCaps;

/**
 * @def BML_CONFIG_STORE_CAPS_INIT
 * @brief Static initializer for BML_ConfigStoreCaps
 */
#define BML_CONFIG_STORE_CAPS_INIT { sizeof(BML_ConfigStoreCaps), BML_VERSION_INIT(0,0,0), 0, 0, 0, 0, 0, BML_THREADING_SINGLE }

/**
 * @brief Get configuration store capabilities
 *
 * @param[out] out_caps Pointer to receive capabilities. Must not be NULL.
 *                      The struct_size field should be pre-initialized.
 *
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if out_caps is NULL
 *
 * @threadsafe Yes
 *
 * @code
 * BML_ConfigStoreCaps caps = BML_CONFIG_STORE_CAPS_INIT;
 * if (bmlConfigGetCaps(&caps) == BML_RESULT_OK) {
 *     printf("Max string: %u bytes\n", caps.max_string_bytes);
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ConfigGetCaps)(BML_ConfigStoreCaps *out_caps);

/**
 * @brief Get a configuration value
 *
 * @param[in] mod Mod handle. Can be NULL to use the current module context.
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
 * @param[in] mod Mod handle. Can be NULL to use the current module context.
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
 * @param[in] mod Mod handle. Can be NULL to use the current module context.
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

typedef struct BML_ConfigApi {
    PFN_BML_ConfigGet Get;
    PFN_BML_ConfigSet Set;
    PFN_BML_ConfigReset Reset;
    PFN_BML_ConfigEnumerate Enumerate;
} BML_ConfigApi;

extern PFN_BML_ConfigGet bmlConfigGet;
extern PFN_BML_ConfigSet bmlConfigSet;
extern PFN_BML_ConfigReset bmlConfigReset;
extern PFN_BML_ConfigEnumerate bmlConfigEnumerate;
extern PFN_BML_ConfigGetCaps bmlConfigGetCaps;

/* Batch operations */
extern PFN_BML_ConfigBatchBegin bmlConfigBatchBegin;
extern PFN_BML_ConfigBatchSet bmlConfigBatchSet;
extern PFN_BML_ConfigBatchCommit bmlConfigBatchCommit;
extern PFN_BML_ConfigBatchDiscard bmlConfigBatchDiscard;

/* ========================================================================
 * Type-Safe Config Accessors (Convenience Macros)
 *
 * These inline macros provide type-safe access without the overhead of
 * separate API function pointers. They expand to calls to bmlConfigGet/Set
 * with proper type handling.
 * ======================================================================== */

/**
 * @def BML_CONFIG_GET_INT
 * @brief Get an integer configuration value
 * @param mod Mod handle (can be NULL)
 * @param category Category string
 * @param name Key name string
 * @param out_value Pointer to int32_t to receive value
 * @return BML_Result
 */
#define BML_CONFIG_GET_INT(mod, category, name, out_value) \
    bml_config_get_int_inline((mod), (category), (name), (out_value))

/**
 * @def BML_CONFIG_GET_FLOAT
 * @brief Get a float configuration value
 */
#define BML_CONFIG_GET_FLOAT(mod, category, name, out_value) \
    bml_config_get_float_inline((mod), (category), (name), (out_value))

/**
 * @def BML_CONFIG_GET_BOOL
 * @brief Get a boolean configuration value
 */
#define BML_CONFIG_GET_BOOL(mod, category, name, out_value) \
    bml_config_get_bool_inline((mod), (category), (name), (out_value))

/**
 * @def BML_CONFIG_GET_STRING
 * @brief Get a string configuration value
 */
#define BML_CONFIG_GET_STRING(mod, category, name, out_value) \
    bml_config_get_string_inline((mod), (category), (name), (out_value))

/**
 * @def BML_CONFIG_SET_INT
 * @brief Set an integer configuration value
 */
#define BML_CONFIG_SET_INT(mod, category, name, value) \
    bml_config_set_int_inline((mod), (category), (name), (value))

/**
 * @def BML_CONFIG_SET_FLOAT
 * @brief Set a float configuration value
 */
#define BML_CONFIG_SET_FLOAT(mod, category, name, value) \
    bml_config_set_float_inline((mod), (category), (name), (value))

/**
 * @def BML_CONFIG_SET_BOOL
 * @brief Set a boolean configuration value
 */
#define BML_CONFIG_SET_BOOL(mod, category, name, value) \
    bml_config_set_bool_inline((mod), (category), (name), (value))

/**
 * @def BML_CONFIG_SET_STRING
 * @brief Set a string configuration value
 */
#define BML_CONFIG_SET_STRING(mod, category, name, value) \
    bml_config_set_string_inline((mod), (category), (name), (value))

/* Inline helper functions for the macros above.
 * These require the global function pointers (bmlConfigGet, bmlConfigSet) to be available.
 * They are typically only usable in mod code that has loaded the BML API.
 */
#if !defined(BML_NO_INLINE_HELPERS) && !defined(BML_TEST)
static inline BML_Result bml_config_get_int_inline(BML_Mod mod, const char *category, const char *name, int32_t *out_value) {
    if (!bmlConfigGet || !out_value) return BML_RESULT_INVALID_ARGUMENT;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {0}};
    BML_Result r = bmlConfigGet(mod, &key, &val);
    if (r == BML_RESULT_OK) {
        if (val.type != BML_CONFIG_INT) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = val.data.int_value;
    }
    return r;
}

static inline BML_Result bml_config_get_float_inline(BML_Mod mod, const char *category, const char *name, float *out_value) {
    if (!bmlConfigGet || !out_value) return BML_RESULT_INVALID_ARGUMENT;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {0}};
    BML_Result r = bmlConfigGet(mod, &key, &val);
    if (r == BML_RESULT_OK) {
        if (val.type != BML_CONFIG_FLOAT) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = val.data.float_value;
    }
    return r;
}

static inline BML_Result bml_config_get_bool_inline(BML_Mod mod, const char *category, const char *name, BML_Bool *out_value) {
    if (!bmlConfigGet || !out_value) return BML_RESULT_INVALID_ARGUMENT;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {0}};
    BML_Result r = bmlConfigGet(mod, &key, &val);
    if (r == BML_RESULT_OK) {
        if (val.type != BML_CONFIG_BOOL) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = val.data.bool_value;
    }
    return r;
}

static inline BML_Result bml_config_get_string_inline(BML_Mod mod, const char *category, const char *name, const char **out_value) {
    if (!bmlConfigGet || !out_value) return BML_RESULT_INVALID_ARGUMENT;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {0}};
    BML_Result r = bmlConfigGet(mod, &key, &val);
    if (r == BML_RESULT_OK) {
        if (val.type != BML_CONFIG_STRING) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = val.data.string_value;
    }
    return r;
}

static inline BML_Result bml_config_set_int_inline(BML_Mod mod, const char *category, const char *name, int32_t value) {
    if (!bmlConfigSet) return BML_RESULT_NOT_SUPPORTED;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {0}};
    val.data.int_value = value;
    return bmlConfigSet(mod, &key, &val);
}

static inline BML_Result bml_config_set_float_inline(BML_Mod mod, const char *category, const char *name, float value) {
    if (!bmlConfigSet) return BML_RESULT_NOT_SUPPORTED;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {0}};
    val.data.float_value = value;
    return bmlConfigSet(mod, &key, &val);
}

static inline BML_Result bml_config_set_bool_inline(BML_Mod mod, const char *category, const char *name, BML_Bool value) {
    if (!bmlConfigSet) return BML_RESULT_NOT_SUPPORTED;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {0}};
    val.data.bool_value = value;
    return bmlConfigSet(mod, &key, &val);
}

static inline BML_Result bml_config_set_string_inline(BML_Mod mod, const char *category, const char *name, const char *value) {
    if (!bmlConfigSet) return BML_RESULT_NOT_SUPPORTED;
    BML_ConfigKey key = {sizeof(BML_ConfigKey), category, name};
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {0}};
    val.data.string_value = value;
    return bmlConfigSet(mod, &key, &val);
}
#endif /* !BML_NO_INLINE_HELPERS && !BML_TEST */

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
typedef BML_Result (*PFN_BML_RegisterConfigLoadHooks)(const BML_ConfigLoadHooks *hooks);

extern PFN_BML_RegisterConfigLoadHooks bmlRegisterConfigLoadHooks;

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
BML_CONFIG_STATIC_ASSERT(BML_CONFIG_OFFSETOF(BML_ConfigStoreCaps, struct_size) == 0,
                         "BML_ConfigStoreCaps.struct_size must be at offset 0");

/* Verify enum sizes are 32-bit */
BML_CONFIG_STATIC_ASSERT(sizeof(BML_ConfigType) == sizeof(int32_t),
                         "BML_ConfigType must be 32-bit");

#endif /* BML_CONFIG_H */
