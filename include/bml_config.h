#ifndef BML_CONFIG_H
#define BML_CONFIG_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

/**
 * @brief Configuration key identifier (Task 1.2)
 */
typedef struct BML_ConfigKey {
    size_t struct_size;    /**< sizeof(BML_ConfigKey), must be first field */
    const char *category;  /**< Config category (e.g., "video", "audio") */
    const char *name;      /**< Key name within category */
} BML_ConfigKey;

/**
 * @def BML_CONFIG_KEY_INIT
 * @brief Static initializer for BML_ConfigKey
 */
#define BML_CONFIG_KEY_INIT(cat, n) { sizeof(BML_ConfigKey), (cat), (n) }

typedef enum BML_ConfigType {
    BML_CONFIG_BOOL = 0,
    BML_CONFIG_INT  = 1,
    BML_CONFIG_FLOAT = 2,
    BML_CONFIG_STRING = 3,
    _BML_CONFIG_TYPE_FORCE_32BIT = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_ConfigType;

/**
 * @brief Configuration value container (Task 1.2)
 */
typedef struct BML_ConfigValue {
    size_t struct_size;    /**< sizeof(BML_ConfigValue), must be first field */
    BML_ConfigType type;   /**< Value type discriminator */
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
    BML_CONFIG_CAP_GET            = 1u << 0,
    BML_CONFIG_CAP_SET            = 1u << 1,
    BML_CONFIG_CAP_RESET          = 1u << 2,
    BML_CONFIG_CAP_ENUMERATE      = 1u << 3,
    BML_CONFIG_CAP_PERSISTENCE    = 1u << 4,
    _BML_CONFIG_CAP_FORCE_32BIT   = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_ConfigCapabilityFlags;

/**
 * @brief Configuration store capabilities (Task 1.2)
 */
typedef struct BML_ConfigStoreCaps {
    size_t struct_size;              /**< sizeof(BML_ConfigStoreCaps), must be first field */
    BML_Version api_version;         /**< API version */
    uint32_t feature_flags;          /**< Bitmask of BML_ConfigCapabilityFlags */
    uint32_t supported_type_mask;    /**< Bitmask of supported BML_ConfigType values */
    uint32_t max_category_length;    /**< Maximum category name length */
    uint32_t max_name_length;        /**< Maximum key name length */
    uint32_t max_string_bytes;       /**< Maximum string value length */
    BML_ThreadingModel threading_model;  /**< Threading model of Config APIs */
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
 * @since 0.4.0
 * 
 * @code
 * BML_ConfigStoreCaps caps = BML_CONFIG_STORE_CAPS_INIT;
 * if (bmlGetConfigCaps(&caps) == BML_RESULT_OK) {
 *     printf("Max string: %u bytes\n", caps.max_string_bytes);
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_GetConfigCaps)(BML_ConfigStoreCaps *out_caps);

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
 * @since 0.4.0
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
 * @since 0.4.0
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
 * @since 0.4.0
 */
typedef BML_Result (*PFN_BML_ConfigReset)(BML_Mod mod, const BML_ConfigKey *key);
/**
 * @brief Config enumeration callback (Task 2.3 - unified signature)
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

/* ========== Type-safe accessors ========== */

/**
 * @brief Get integer config value
 * @threadsafe Yes
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_CONFIG_KEY_NOT_FOUND if key doesn't exist
 * @return BML_RESULT_CONFIG_TYPE_MISMATCH if key exists but is not an integer
 */
typedef BML_Result (*PFN_BML_ConfigGetInt)(BML_Mod mod, const BML_ConfigKey *key, int32_t *out_value);

/**
 * @brief Get float config value
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ConfigGetFloat)(BML_Mod mod, const BML_ConfigKey *key, float *out_value);

/**
 * @brief Get boolean config value
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ConfigGetBool)(BML_Mod mod, const BML_ConfigKey *key, BML_Bool *out_value);

/**
 * @brief Get string config value
 * @threadsafe Yes
 * @note Returned string pointer remains valid until config is modified
 */
typedef BML_Result (*PFN_BML_ConfigGetString)(BML_Mod mod, const BML_ConfigKey *key, const char **out_value);

/**
 * @brief Set integer config value
 * @threadsafe No (use external synchronization)
 */
typedef BML_Result (*PFN_BML_ConfigSetInt)(BML_Mod mod, const BML_ConfigKey *key, int32_t value);

/**
 * @brief Set float config value
 * @threadsafe No (use external synchronization)
 */
typedef BML_Result (*PFN_BML_ConfigSetFloat)(BML_Mod mod, const BML_ConfigKey *key, float value);

/**
 * @brief Set boolean config value
 * @threadsafe No (use external synchronization)
 */
typedef BML_Result (*PFN_BML_ConfigSetBool)(BML_Mod mod, const BML_ConfigKey *key, BML_Bool value);

/**
 * @brief Set string config value
 * @threadsafe No (use external synchronization)
 * @note String is copied internally
 */
typedef BML_Result (*PFN_BML_ConfigSetString)(BML_Mod mod, const BML_ConfigKey *key, const char *value);

typedef struct BML_ConfigApi {
    PFN_BML_ConfigGet        Get;
    PFN_BML_ConfigSet        Set;
    PFN_BML_ConfigReset      Reset;
    PFN_BML_ConfigEnumerate  Enumerate;
} BML_ConfigApi;

extern PFN_BML_ConfigGet        bmlConfigGet;
extern PFN_BML_ConfigSet        bmlConfigSet;
extern PFN_BML_ConfigReset      bmlConfigReset;
extern PFN_BML_ConfigEnumerate  bmlConfigEnumerate;
extern PFN_BML_GetConfigCaps    bmlGetConfigCaps;

extern PFN_BML_ConfigGetInt     bmlConfigGetInt;
extern PFN_BML_ConfigGetFloat   bmlConfigGetFloat;
extern PFN_BML_ConfigGetBool    bmlConfigGetBool;
extern PFN_BML_ConfigGetString  bmlConfigGetString;
extern PFN_BML_ConfigSetInt     bmlConfigSetInt;
extern PFN_BML_ConfigSetFloat   bmlConfigSetFloat;
extern PFN_BML_ConfigSetBool    bmlConfigSetBool;
extern PFN_BML_ConfigSetString  bmlConfigSetString;

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
