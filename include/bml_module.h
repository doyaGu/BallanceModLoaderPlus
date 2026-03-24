/**
 * @file bml_module.h
 * @brief C module entry point helper macro
 *
 * Provides BML_MODULE_ENTRY macro that generates the standard
 * module entrypoint function with correct export attributes.
 */

#ifndef BML_MODULE_H
#define BML_MODULE_H

#include "bml_export.h"

/**
 * @brief Define this before including module headers to enable entry point definitions
 * 
 * This prevents bml_export.h from trying to import module entry points.
 */
#define BML_DEFINING_MODULE_ENTRY_POINTS

/**
 * @brief Declare module entry points with correct linkage
 * 
 * For C++ modules, prefer bml_module.hpp with BML_DEFINE_MODULE(). For raw
 * C modules:
 * ```c
 * #include "bml_module.h"
 * #include "bml_core.h"
 *
 * static BML_Result OnAttach(const BML_ModAttachArgs *args) {
 *     if (args->struct_size < sizeof(BML_ModAttachArgs)) return BML_RESULT_INVALID_ARGUMENT;
 *     if (args->api_version < BML_MOD_ENTRYPOINT_API_VERSION) return BML_RESULT_VERSION_MISMATCH;
 *     if (!args->services) return BML_RESULT_INVALID_ARGUMENT;
 *     // args->context is the runtime-owned context handle for this attach
 *     return BML_RESULT_OK;
 * }
 *
 * BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand cmd, void *data) {
 *     switch (cmd) {
 *     case BML_MOD_ENTRYPOINT_ATTACH:
 *         return OnAttach((const BML_ModAttachArgs *)data);
 *     default:
 *         return BML_RESULT_INVALID_ARGUMENT;
 *     }
 * }
 * ```
 */
#ifdef __cplusplus
#   define BML_MODULE_ENTRY extern "C" __declspec(dllexport)
#else
#   define BML_MODULE_ENTRY __declspec(dllexport)
#endif

#endif /* BML_MODULE_H */
