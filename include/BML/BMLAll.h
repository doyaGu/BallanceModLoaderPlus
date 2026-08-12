// Every public header at once, for a Mod that would rather not track which one
// declares what. It pulls in CKAll.h from the Virtools SDK and imgui.h through
// Bui.h, so both have to be on the include path, which bml_add_mod arranges.
//
// This is the slow way to compile. A Mod that knows what it needs includes those
// headers instead: IMod.h for the class to derive from, which already brings IBML.h,
// IConfig.h, ILogger.h, and IMessageReceiver.h with it, then Bui.h or Gui.h for a
// user interface, InputHook.h for the keyboard and mouse, and Runtime.h, Scene.h,
// Gameplay.h, UI.h, Speedrun.h, or Events.h for what the legacy interfaces do not
// cover.
#ifndef BMLALL_H
#define BMLALL_H

#include "BML/Version.h"
#include "BML/Defines.h"
#include "BML/Interface.h"
#include "BML/BML.h"
#include "BML/Guids.h"

#include "BML/IBML.h"
#include "BML/ILogger.h"
#include "BML/ICommand.h"
#include "BML/IConfig.h"
#include "BML/IMessageReceiver.h"
#include "BML/IMod.h"
#include "BML/DataShare.h"
#include "BML/Imc.h"
#include "BML/ImcTypes.h"
#include "BML/ImcWire.hpp"
#include "BML/ImcCpp.hpp"
#include "BML/ImcMath.h"
#include "BML/EventKinds.h"
#include "BML/Runtime.h"
#include "BML/Scene.h"
#include "BML/Gameplay.h"
#include "BML/UI.h"
#include "BML/Speedrun.h"
#include "BML/Events.h"

#include "BML/Bui.h"
#include "BML/Gui.h"
#include "BML/InputHook.h"
#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"

#endif // BMLALL_H
