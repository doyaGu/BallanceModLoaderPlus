/**
 * @file UIMod.h
 * @brief BML_UI Module - ImGui Overlay and UI Infrastructure
 *
 * This module manages the ImGui overlay lifecycle and provides
 * base UI infrastructure for BML. It subscribes to engine lifecycle
 * and render events to coordinate ImGui frame management.
 */

#ifndef BML_UI_MOD_H
#define BML_UI_MOD_H

#include "bml_imc.h"

namespace BML_UI {

/**
 * @brief Initialize the UI module
 * @return true if initialization was successful
 */
bool Initialize();

/**
 * @brief Shutdown the UI module
 */
void Shutdown();

/**
 * @brief Check if the UI module is initialized
 * @return true if the module is initialized and ready
 */
bool IsInitialized();

} // namespace BML_UI

#endif // BML_UI_MOD_H
