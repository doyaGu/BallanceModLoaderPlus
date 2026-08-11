// The older of the two ways to draw, kept for the Mods written against it. Everything
// in namespace BGui is made of the game's own objects: an element is a CK2dEntity in
// the CKContext, a label draws through a 2D Text building block added to the game's
// Level_Init script, and a button wears the menu's own materials. So it renders exactly
// like the game and needs nothing of ImGui, at the price of putting Virtools objects
// into the running level for as long as it exists.
//
// Bui.h is the other one, and it is what the loader draws its own windows and menus
// with. Prefer it for anything new: it needs no CK objects, survives a level change
// without help, and its widgets are the ones the loader keeps up to date. Reach for
// BGui when a Mod already uses it, or when something has to sit inside the game's own
// 2D layer at a chosen Z order rather than on top of everything.
//
// Nothing here draws or reacts by itself. A Mod owns a BGui::Gui, calls its Process
// once a frame from IMod::OnProcess, and deletes it when its screen goes away; Process
// is what reads the keyboard and mouse, calls the buttons' callbacks, and re-executes
// the text blocks. Miss it for a frame and the elements are still there but nothing
// moves.
//
// The materials, the sound, and the font all come from the game's menu, and the loader
// picks them up when 3D Entities\Menu.nmo loads. Elements built before that have
// nothing to draw with, so build them from OnLoadObject for that file at the earliest,
// and later than that in practice.
#ifndef BML_GUI_H
#define BML_GUI_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

#include "BML/Gui/Element.h"
#include "BML/Gui/Text.h"
#include "BML/Gui/Panel.h"
#include "BML/Gui/Label.h"
#include "BML/Gui/Button.h"
#include "BML/Gui/Input.h"
#include "BML/Gui/KeyInput.h"
#include "BML/Gui/Gui.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // BML_GUI_H