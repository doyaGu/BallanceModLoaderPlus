/**
 * @file bml_ui_api_ids.h
 * @brief Stable API identifiers for BML_UI module
 * 
 * ============================================================================
 * CRITICAL: THESE IDS ARE PERMANENT AND MUST NEVER CHANGE
 * ============================================================================
 * 
 * BML_UI API ID Range: 10000-10999
 * 
 * ID Allocation:
 * - 10000-10099: Core (GetImGuiContext only - other lifecycle APIs are internal)
 * - 10200-10299: ANSI Palette
 * - 10300-10399: ANSI Text rendering
 * - 10400-10999: Reserved for future use
 * 
 * Note: ImGui functions are exported directly from DLL, not via API registry.
 */

#ifndef BML_UI_API_IDS_H
#define BML_UI_API_IDS_H

#include "bml_types.h"

/* ========================================================================
 * Core APIs (10000-10099)
 * ======================================================================== */

/** @brief Get the ImGui context (for mods that need to use ImGui) */
#define BML_API_ID_bmlUIGetImGuiContext         10001u

/* ========================================================================
 * ANSI Palette APIs (10200-10299)
 * ======================================================================== */

/** @brief Get global palette instance */
#define BML_API_ID_bmlUIGetGlobalPalette        10200u

/** @brief Ensure palette is initialized */
#define BML_API_ID_bmlUIPaletteEnsureInit       10201u

/** @brief Reload palette from file */
#define BML_API_ID_bmlUIPaletteReload           10202u

/** @brief Get color by ANSI index */
#define BML_API_ID_bmlUIPaletteGetColor         10203u

/** @brief Check if palette is active */
#define BML_API_ID_bmlUIPaletteIsActive         10204u

/** @brief Set loader directory provider */
#define BML_API_ID_bmlUIPaletteSetDirProvider   10210u

/** @brief Set logger provider */
#define BML_API_ID_bmlUIPaletteSetLogProvider   10211u

/* ========================================================================
 * ANSI Text Rendering APIs (10300-10399)
 * ======================================================================== */

/** @brief Render ANSI text */
#define BML_API_ID_bmlUIRenderAnsiText          10300u

/** @brief Render ANSI text with custom palette */
#define BML_API_ID_bmlUIRenderAnsiTextEx        10301u

/** @brief Calculate ANSI text size */
#define BML_API_ID_bmlUICalcAnsiTextSize        10302u

/** @brief Parse ANSI text to segments */
#define BML_API_ID_bmlUIParseAnsiText           10303u

/** @brief Free parsed segments */
#define BML_API_ID_bmlUIFreeAnsiSegments        10304u

/* ========================================================================
 * UI Capabilities (for bmlQueryCapabilities)
 * ======================================================================== */

/** @brief BML_UI module capability flags */
#define BML_CAP_UI_IMGUI        (1u << 0)   /**< ImGui available */
#define BML_CAP_UI_ANSI_TEXT    (1u << 1)   /**< ANSI text rendering */
#define BML_CAP_UI_ANSI_PALETTE (1u << 2)   /**< Customizable palette */

#endif /* BML_UI_API_IDS_H */
