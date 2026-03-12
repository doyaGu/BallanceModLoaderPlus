/**
 * @file bml_ui_export.h
 * @brief BML_UI Module Export Definitions
 * 
 * Defines export/import macros for BML_UI public API.
 */

#ifndef BML_UI_EXPORT_H
#define BML_UI_EXPORT_H

#ifdef _WIN32
    #ifdef BML_UI_EXPORTS
        #define BML_UI_API __declspec(dllexport)
    #else
        #define BML_UI_API __declspec(dllimport)
    #endif
#else
    #ifdef BML_UI_EXPORTS
        #define BML_UI_API __attribute__((visibility("default")))
    #else
        #define BML_UI_API
    #endif
#endif

// For classes that need to be exported
#define BML_UI_CLASS BML_UI_API

#endif // BML_UI_EXPORT_H
