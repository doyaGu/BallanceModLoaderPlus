#ifndef BML_MODULE_H
#define BML_MODULE_H

/**
 * @file bml_module.h
 * @brief Helper macros for BML module development
 * 
 * Include this header in your module source file to define entry points correctly.
 */

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
 * Usage in your module .cpp file:
 * ```cpp
 * #include "bml_module.h"
 * #define BML_LOADER_IMPLEMENTATION
 * #include "bml_loader.h"
 *
 * namespace {
 * BML_Result OnAttach(const BML_ModAttachArgs *args) {
 *     BML_Result res = bmlLoadAPI(args->get_proc);
 *     if (res != BML_RESULT_OK) {
 *         return res;
 *     }
 *     // ... initialization logic ...
 *     return BML_RESULT_OK;
 * }
 *
 * BML_Result OnDetach(const BML_ModDetachArgs *args) {
 *     // ... cleanup logic ...
 *     bmlUnloadAPI();
 *     return BML_RESULT_OK;
 * }
 * }
 *
 * BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand cmd, void *data) {
 *     switch (cmd) {
 *     case BML_MOD_ENTRYPOINT_ATTACH:
 *         return OnAttach(static_cast<const BML_ModAttachArgs *>(data));
 *     case BML_MOD_ENTRYPOINT_DETACH:
 *         return OnDetach(static_cast<const BML_ModDetachArgs *>(data));
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
