#ifndef BML_RESOURCE_H
#define BML_RESOURCE_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

typedef uint32_t BML_HandleType;

/**
 * @brief Handle descriptor
 */
typedef struct BML_HandleDesc {
    size_t struct_size;    /**< sizeof(BML_HandleDesc), must be first field */
    BML_HandleType type;
    uint32_t generation;
    uint32_t slot;
} BML_HandleDesc;

/**
 * @def BML_HANDLE_DESC_INIT
 * @brief Static initializer for BML_HandleDesc
 */
#define BML_HANDLE_DESC_INIT { sizeof(BML_HandleDesc), 0, 0, 0 }

/* ========================================================================
 * Custom Resource Type Registration
 * ======================================================================== */

/**
 * @brief Callback for finalizing a resource handle when released
 * @param ctx BML context
 * @param desc The handle being finalized
 * @param user_data User data from resource type registration
 */
typedef void (*BML_ResourceHandleFinalize)(BML_Context ctx, const BML_HandleDesc* desc, void* user_data);

/**
 * @brief Descriptor for registering a custom resource type
 */
typedef struct BML_ResourceTypeDesc {
    size_t struct_size;                  /**< sizeof(BML_ResourceTypeDesc), must be first */
    const char* name;                    /**< Human-readable type name (required) */
    BML_ResourceHandleFinalize on_finalize; /**< Called when handle ref count reaches 0 */
    void* user_data;                     /**< User context for callbacks */
} BML_ResourceTypeDesc;

#define BML_RESOURCE_TYPE_DESC_INIT { sizeof(BML_ResourceTypeDesc), NULL, NULL, NULL }

/**
 * @brief Register a custom resource type
 * @param desc Resource type descriptor
 * @param out_type Receives the allocated type ID
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_RegisterResourceType)(const BML_ResourceTypeDesc* desc, BML_HandleType* out_type);

/* ========================================================================
 * Capability Flags
 * ======================================================================== */

typedef enum BML_ResourceCapabilityFlags {
    BML_RESOURCE_CAP_STRONG_REFERENCES  = 1u << 0,
    BML_RESOURCE_CAP_USER_DATA          = 1u << 1,
    BML_RESOURCE_CAP_THREAD_SAFE        = 1u << 2,
    BML_RESOURCE_CAP_TYPE_ISOLATION     = 1u << 3,
    _BML_RESOURCE_CAP_FORCE_32BIT       = 0x7FFFFFFF  /**< Force 32-bit enum */
} BML_ResourceCapabilityFlags;

/**
 * @brief Resource API capabilities
 */
typedef struct BML_ResourceCaps {
    size_t struct_size;           /**< sizeof(BML_ResourceCaps), must be first field */
    BML_Version api_version;
    uint32_t capability_flags;
    uint32_t active_handle_types;
    uint32_t user_data_alignment;
} BML_ResourceCaps;

/**
 * @def BML_RESOURCE_CAPS_INIT
 * @brief Static initializer for BML_ResourceCaps
 */
#define BML_RESOURCE_CAPS_INIT { sizeof(BML_ResourceCaps), BML_VERSION_INIT(0,0,0), 0, 0, 0 }

/**
 * @brief Creates a new resource handle of the specified type.
 *
 * Allocates a new handle slot and initializes the handle descriptor.
 * The handle starts with a reference count of 1.
 *
 * @param[in]  type     The type identifier for the handle (user-defined).
 * @param[out] out_desc Pointer to receive the initialized handle descriptor.
 *                      Must not be NULL. The struct_size field will be set.
 *
 * @return BML_OK on success.
 * @return BML_ERROR_INVALID_PARAM if out_desc is NULL.
 * @return BML_ERROR_OUT_OF_MEMORY if no handle slots are available.
 *
 * @threadsafe Yes, if BML_RESOURCE_CAP_THREAD_SAFE capability is present.
 *
 * @code
 * BML_HandleDesc desc = BML_HANDLE_DESC_INIT;
 * BML_Result r = bmlHandleCreate(MY_RESOURCE_TYPE, &desc);
 * if (r == BML_OK) {
 *     // Use handle...
 *     bmlHandleRelease(&desc);
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_HandleCreate)(BML_HandleType type, BML_HandleDesc *out_desc);

/**
 * @brief Increments the reference count of a handle.
 *
 * Call this when sharing a handle across multiple owners. Each call to
 * bmlHandleRetain must be balanced with a call to bmlHandleRelease.
 *
 * @param[in] desc Pointer to the handle descriptor to retain. Must not be NULL.
 *
 * @return BML_OK on success.
 * @return BML_ERROR_INVALID_PARAM if desc is NULL.
 * @return BML_ERROR_INVALID_HANDLE if the handle is invalid or already released.
 *
 * @threadsafe Yes, if BML_RESOURCE_CAP_THREAD_SAFE capability is present.
 */
typedef BML_Result (*PFN_BML_HandleRetain)(const BML_HandleDesc *desc);

/**
 * @brief Decrements the reference count of a handle.
 *
 * When the reference count reaches zero, the handle slot is freed and the
 * handle becomes invalid. Any attached user data is NOT automatically freed.
 *
 * @param[in] desc Pointer to the handle descriptor to release. Must not be NULL.
 *
 * @return BML_OK on success.
 * @return BML_ERROR_INVALID_PARAM if desc is NULL.
 * @return BML_ERROR_INVALID_HANDLE if the handle is invalid or already released.
 *
 * @threadsafe Yes, if BML_RESOURCE_CAP_THREAD_SAFE capability is present.
 */
typedef BML_Result (*PFN_BML_HandleRelease)(const BML_HandleDesc *desc);

/**
 * @brief Validates whether a handle is still valid.
 *
 * Checks that the handle slot exists, the generation matches, and the
 * reference count is greater than zero.
 *
 * @param[in]  desc      Pointer to the handle descriptor to validate. Must not be NULL.
 * @param[out] out_valid Pointer to receive the validation result. Must not be NULL.
 *                       Set to BML_TRUE if valid, BML_FALSE otherwise.
 *
 * @return BML_OK on success (even if handle is invalid).
 * @return BML_ERROR_INVALID_PARAM if desc or out_valid is NULL.
 *
 * @threadsafe Yes, if BML_RESOURCE_CAP_THREAD_SAFE capability is present.
 *
 * @code
 * BML_Bool valid = BML_FALSE;
 * if (bmlHandleValidate(&desc, &valid) == BML_OK && valid) {
 *     // Handle is valid, safe to use
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_HandleValidate)(const BML_HandleDesc *desc, BML_Bool *out_valid);

/**
 * @brief Attaches user data to a handle.
 *
 * Associates arbitrary user data with a handle. Only one user data pointer
 * can be attached at a time; calling this again replaces the previous value.
 * The user is responsible for managing the lifetime of the user data.
 *
 * @param[in] desc      Pointer to the handle descriptor. Must not be NULL.
 * @param[in] user_data User data pointer to attach (may be NULL to clear).
 *
 * @return BML_OK on success.
 * @return BML_ERROR_INVALID_PARAM if desc is NULL.
 * @return BML_ERROR_INVALID_HANDLE if the handle is invalid.
 * @return BML_ERROR_NOT_SUPPORTED if BML_RESOURCE_CAP_USER_DATA is not available.
 *
 * @threadsafe Yes, if BML_RESOURCE_CAP_THREAD_SAFE capability is present.
 */
typedef BML_Result (*PFN_BML_HandleAttachUserData)(const BML_HandleDesc *desc, void *user_data);

/**
 * @brief Retrieves user data attached to a handle.
 *
 * Gets the user data pointer previously attached via bmlHandleAttachUserData.
 *
 * @param[in]  desc          Pointer to the handle descriptor. Must not be NULL.
 * @param[out] out_user_data Pointer to receive the user data. Must not be NULL.
 *                           Set to NULL if no user data was attached.
 *
 * @return BML_OK on success.
 * @return BML_ERROR_INVALID_PARAM if desc or out_user_data is NULL.
 * @return BML_ERROR_INVALID_HANDLE if the handle is invalid.
 * @return BML_ERROR_NOT_SUPPORTED if BML_RESOURCE_CAP_USER_DATA is not available.
 *
 * @threadsafe Yes, if BML_RESOURCE_CAP_THREAD_SAFE capability is present.
 *
 * @code
 * void *data = NULL;
 * if (bmlHandleGetUserData(&desc, &data) == BML_OK && data) {
 *     MyData *my_data = (MyData*)data;
 *     // Use data...
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_HandleGetUserData)(const BML_HandleDesc *desc, void **out_user_data);

/**
 * @brief Queries the capabilities of the Resource API.
 *
 * Returns information about available features, API version, and limits.
 *
 * @param[out] out_caps Pointer to receive capability information. Must not be NULL.
 *                      Initialize with BML_RESOURCE_CAPS_INIT before calling.
 *
 * @return BML_OK on success.
 * @return BML_ERROR_INVALID_PARAM if out_caps is NULL.
 *
 * @threadsafe Yes
 *
 * @code
 * BML_ResourceCaps caps = BML_RESOURCE_CAPS_INIT;
 * if (bmlResourceGetCaps(&caps) == BML_OK) {
 *     if (caps.capability_flags & BML_RESOURCE_CAP_USER_DATA) {
 *         // User data feature is available
 *     }
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ResourceGetCaps)(BML_ResourceCaps *out_caps);

extern PFN_BML_HandleCreate        bmlHandleCreate;
extern PFN_BML_HandleRetain        bmlHandleRetain;
extern PFN_BML_HandleRelease       bmlHandleRelease;
extern PFN_BML_HandleValidate      bmlHandleValidate;
extern PFN_BML_HandleAttachUserData bmlHandleAttachUserData;
extern PFN_BML_HandleGetUserData    bmlHandleGetUserData;
extern PFN_BML_ResourceGetCaps           bmlResourceGetCaps;
extern PFN_BML_RegisterResourceType bmlRegisterResourceType;

BML_END_CDECLS

#endif /* BML_RESOURCE_H */
