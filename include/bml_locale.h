#ifndef BML_LOCALE_H
#define BML_LOCALE_H

/**
 * @file bml_locale.h
 * @brief Per-mod localization / string table API
 *
 * Each module can ship TOML locale files under `<mod_dir>/locale/<code>.toml`.
 * The framework loads the one matching the active language code, with fallback
 * to "en". Locale files use flat key=value format:
 *
 *     greeting = "Hello"
 *     config.speed = "Ball Speed"
 *
 * Dots in key names are literal (no nested table interpretation).
 *
 * Resolve these function pointers through `bmlGetProcAddress(...)` or
 * acquire `bml.core.locale` in module code.
 */

#include "bml_types.h"
#include "bml_errors.h"

BML_BEGIN_CDECLS

/** @brief Opaque handle to a module's bound string table (fast-path lookup). */
typedef const void *BML_LocaleTable;

/**
 * @brief Load the locale string table for the calling module.
 *
 * Looks for `<mod_dir>/locale/<locale_code>.toml` and parses it as a flat
 * key=value TOML table. Replaces any previously loaded locale data for
 * the calling module.
 *
 * If the requested locale file does not exist, falls back to "en".
 * If neither exists, returns BML_RESULT_NOT_FOUND (not a fatal error).
 *
 * @param locale_code  Language code (e.g. "en", "zh-CN", "ja"). If NULL,
 *                     the current global language is used.
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if no locale file found
 * @return BML_RESULT_INVALID_CONTEXT if caller module cannot be resolved
 */
typedef BML_Result (*PFN_BML_LocaleLoad)(const char *locale_code);
typedef BML_Result (*PFN_BML_LocaleLoadOwned)(BML_Mod owner, const char *locale_code);

/**
 * @brief Get a localized string by key for the calling module.
 *
 * @param key  String table key (e.g. "greeting", "config.speed")
 * @return The localized string, or the key itself if not found.
 *         The returned pointer is valid until the module's locale data
 *         is reloaded or the module is detached.
 * @return NULL if key is NULL
 */
typedef const char *(*PFN_BML_LocaleGet)(const char *key);
typedef const char *(*PFN_BML_LocaleGetOwned)(BML_Mod owner, const char *key);

/**
 * @brief Set the active language globally.
 *
 * This automatically reloads locale data for modules that previously loaded
 * locale tables.
 *
 * @param language_code  Language code (e.g. "en", "zh-CN"). Must not be NULL.
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if language_code is NULL
 */
typedef BML_Result (*PFN_BML_LocaleSetLanguage)(const char *language_code);

/**
 * @brief Get the active language code.
 *
 * @param out_code  Receives a pointer to the language code string. The pointer
 *                  is valid until the next SetLanguage call.
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if out_code is NULL
 */
typedef BML_Result (*PFN_BML_LocaleGetLanguage)(const char **out_code);

/* ========================================================================
 * Fast-Path Lookup (zero allocation, no mutex, no module resolution)
 * ======================================================================== */

/**
 * @brief Bind the calling module's current string table for fast lookup.
 *
 * Returns an opaque handle that can be passed to LocaleLookup for
 * direct table access without per-call module ID resolution.
 * The handle is invalidated when LocaleLoad is called or when SetLanguage()
 * triggers a reload (re-bind after either operation).
 *
 * @param[out] out_table  Receives the table handle (NULL if no locale loaded)
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_LocaleBindTable)(BML_LocaleTable *out_table);
typedef BML_Result (*PFN_BML_LocaleBindTableOwned)(BML_Mod owner, BML_LocaleTable *out_table);

/**
 * @brief Look up a string directly from a bound table (fast path).
 *
 * No module ID resolution, no mutex. The table must have been obtained
 * from LocaleBindTable and must still be valid (i.e., no LocaleLoad
 * since the bind).
 *
 * @param table  Table handle from LocaleBindTable
 * @param key    String key
 * @return Localized string, or the key itself if not found. NULL if key is NULL.
 */
typedef const char *(*PFN_BML_LocaleLookup)(BML_LocaleTable table, const char *key);

BML_END_CDECLS

#endif /* BML_LOCALE_H */
