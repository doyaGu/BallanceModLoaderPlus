/**
 * @file bml_ui.h
 * @brief BML_UI Module Public API
 * 
 * This header defines the public API for the BML_UI module.
 * 
 * API Access Methods:
 * 1. ImGui functions - Directly exported from BML_UI.dll, use ImGui normally
 * 2. BML_UI APIs - Via API registry (bmlGetProcAddressById)
 * 
 * Usage:
 * @code
 * // Get ImGui context for your mod
 * PFN_bmlUIGetImGuiContext getCtx = 
 *     (PFN_bmlUIGetImGuiContext)bmlGetProcAddressById(BML_API_ID_bmlUIGetImGuiContext);
 * ImGuiContext* ctx = (ImGuiContext*)getCtx();
 * ImGui::SetCurrentContext(ctx);
 * 
 * // Now you can use ImGui directly
 * ImGui::Begin("My Window");
 * ImGui::End();
 * @endcode
 */

#ifndef BML_UI_H
#define BML_UI_H

#include "bml_types.h"
#include "bml_ui_api_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Opaque Types
 *=============================================================================*/

/** @brief Opaque handle to ImGui context */
typedef void* BML_ImGuiContext;

/** @brief Opaque handle to ANSI palette */
typedef void* BML_AnsiPalette;

/** @brief Opaque handle to parsed ANSI segments */
typedef void* BML_AnsiSegments;

/*=============================================================================
 * Callback Types
 *=============================================================================*/

/** @brief Loader directory provider callback */
typedef const wchar_t* (*PFN_BML_UI_LoaderDirProvider)(void);

/** @brief Logger callback (level: 0=info, 1=warn, 2=error) */
typedef void (*PFN_BML_UI_LogProvider)(int level, const char* message);

/*=============================================================================
 * API Function Pointer Types
 *=============================================================================*/

/* ---- Core ---- */

/** @brief Get the ImGui context managed by BML_UI */
typedef BML_ImGuiContext (*PFN_bmlUIGetImGuiContext)(void);

/* ---- ANSI Palette ---- */

/** @brief Get global palette instance */
typedef BML_AnsiPalette (*PFN_bmlUIGetGlobalPalette)(void);

/** @brief Ensure palette is initialized */
typedef void (*PFN_bmlUIPaletteEnsureInit)(BML_AnsiPalette palette);

/** @brief Reload palette from file (returns non-zero on success) */
typedef int (*PFN_bmlUIPaletteReload)(BML_AnsiPalette palette);

/** @brief Get color by ANSI index (0-255) */
typedef int (*PFN_bmlUIPaletteGetColor)(BML_AnsiPalette palette, int index, uint32_t* out_color);

/** @brief Check if palette is active */
typedef int (*PFN_bmlUIPaletteIsActive)(BML_AnsiPalette palette);

/** @brief Set loader directory provider */
typedef void (*PFN_bmlUIPaletteSetDirProvider)(PFN_BML_UI_LoaderDirProvider provider);

/** @brief Set logger provider */
typedef void (*PFN_bmlUIPaletteSetLogProvider)(PFN_BML_UI_LogProvider provider);

/* ---- ANSI Text Rendering ---- */

/** @brief Render ANSI text using global palette */
typedef void (*PFN_bmlUIRenderAnsiText)(const char* text);

/** @brief Render ANSI text with custom palette */
typedef void (*PFN_bmlUIRenderAnsiTextEx)(const char* text, BML_AnsiPalette palette);

/** @brief Calculate size of ANSI text */
typedef void (*PFN_bmlUICalcAnsiTextSize)(const char* text, float* out_width, float* out_height);

/** @brief Parse ANSI text to segments for custom rendering */
typedef BML_AnsiSegments (*PFN_bmlUIParseAnsiText)(const char* text);

/** @brief Free parsed segments */
typedef void (*PFN_bmlUIFreeAnsiSegments)(BML_AnsiSegments segments);

/*=============================================================================
 * API Table Structure (for bulk loading)
 *=============================================================================*/

/**
 * @brief BML_UI API table for bulk loading
 * 
 * Contains function pointers for BML_UI public APIs.
 * Does NOT include ImGui functions (use ImGui directly).
 */
typedef struct BML_UI_ApiTable {
    uint32_t struct_size;           /**< sizeof(BML_UI_ApiTable) for versioning */
    uint32_t version;               /**< API version (major << 16 | minor) */
    
    /* Core */
    PFN_bmlUIGetImGuiContext        GetImGuiContext;
    
    /* ANSI Palette */
    PFN_bmlUIGetGlobalPalette       GetGlobalPalette;
    PFN_bmlUIPaletteEnsureInit      PaletteEnsureInit;
    PFN_bmlUIPaletteReload          PaletteReload;
    PFN_bmlUIPaletteGetColor        PaletteGetColor;
    PFN_bmlUIPaletteIsActive        PaletteIsActive;
    PFN_bmlUIPaletteSetDirProvider  PaletteSetDirProvider;
    PFN_bmlUIPaletteSetLogProvider  PaletteSetLogProvider;
    
    /* ANSI Text */
    PFN_bmlUIRenderAnsiText         RenderAnsiText;
    PFN_bmlUIRenderAnsiTextEx       RenderAnsiTextEx;
    PFN_bmlUICalcAnsiTextSize       CalcAnsiTextSize;
    PFN_bmlUIParseAnsiText          ParseAnsiText;
    PFN_bmlUIFreeAnsiSegments       FreeAnsiSegments;
    
} BML_UI_ApiTable;

/** @brief Current API table version */
#define BML_UI_API_VERSION ((0u << 16) | 4u)  /* v0.4 */

/** @brief Extension name for registration */
#define BML_UI_EXTENSION_NAME "BML_UI"

#ifdef __cplusplus
}
#endif

/*=============================================================================
 * C++ Convenience Wrapper (optional)
 *=============================================================================*/

#ifdef __cplusplus

#include "bml_export.h"

namespace BML_UI {

/**
 * @brief Load BML_UI API table from BML registry
 */
inline bool LoadApi(PFN_BML_GetProcAddressById get_proc_by_id, BML_UI_ApiTable* out_table) {
    if (!get_proc_by_id || !out_table) return false;
    
    out_table->struct_size = sizeof(BML_UI_ApiTable);
    out_table->version = BML_UI_API_VERSION;
    
    #define LOAD_API(name, id) \
        out_table->name = (PFN_bmlUI##name)get_proc_by_id(id); \
        if (!out_table->name) return false
    
    LOAD_API(GetImGuiContext, BML_API_ID_bmlUIGetImGuiContext);
    LOAD_API(GetGlobalPalette, BML_API_ID_bmlUIGetGlobalPalette);
    LOAD_API(PaletteEnsureInit, BML_API_ID_bmlUIPaletteEnsureInit);
    LOAD_API(PaletteReload, BML_API_ID_bmlUIPaletteReload);
    LOAD_API(PaletteGetColor, BML_API_ID_bmlUIPaletteGetColor);
    LOAD_API(PaletteIsActive, BML_API_ID_bmlUIPaletteIsActive);
    LOAD_API(PaletteSetDirProvider, BML_API_ID_bmlUIPaletteSetDirProvider);
    LOAD_API(PaletteSetLogProvider, BML_API_ID_bmlUIPaletteSetLogProvider);
    LOAD_API(RenderAnsiText, BML_API_ID_bmlUIRenderAnsiText);
    LOAD_API(RenderAnsiTextEx, BML_API_ID_bmlUIRenderAnsiTextEx);
    LOAD_API(CalcAnsiTextSize, BML_API_ID_bmlUICalcAnsiTextSize);
    LOAD_API(ParseAnsiText, BML_API_ID_bmlUIParseAnsiText);
    LOAD_API(FreeAnsiSegments, BML_API_ID_bmlUIFreeAnsiSegments);
    
    #undef LOAD_API
    return true;
}

} // namespace BML_UI

#endif /* __cplusplus */

#endif /* BML_UI_H */
