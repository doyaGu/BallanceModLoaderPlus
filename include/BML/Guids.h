// The GUIDs of the Virtools building blocks, parameter types, and attributes, one header
// per plugin, and this one pulling in all of them. Nothing here is code or is anything of
// the loader's: these are the numbers the engine identifies its own pieces by, written out
// so that a Mod can name one instead of pasting a pair of hex words.
//
// They are what ScriptHelper::CreateBB and the CreateParam family are given: a
// VT_<plugin>_<block> for a block, a CKPGUID_ for the type of a parameter. The plugin
// names are the Virtools ones, so a block found in Virtools Dev is looked up in the header
// for the plugin it came from, and BML/Guids/Hooks.h holds the one block that is the
// loader's own, the hook block ExecuteBB::CreateHookBlock builds.
//
// Include the one header a Mod needs rather than this file. A GUID being listed says only
// that the block exists in Virtools, not that the game has that plugin loaded:
// CKBehavior::InitFromGuid fails for one the running game does not have.
#ifndef BML_GUIDS_H
#define BML_GUIDS_H

#include "BML/Guids/3DTransfo.h"
#include "BML/Guids/BuildingBlocksAddons1.h"
#include "BML/Guids/Cameras.h"
#include "BML/Guids/Collisions.h"
#include "BML/Guids/Controllers.h"
#include "BML/Guids/Grids.h"
#include "BML/Guids/Hooks.h"
#include "BML/Guids/Interface.h"
#include "BML/Guids/Lights.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/Materials.h"
#include "BML/Guids/MeshModifiers.h"
#include "BML/Guids/MidiManager.h"
#include "BML/Guids/Narratives.h"
#include "BML/Guids/physics_RT.h"
#include "BML/Guids/Sounds.h"
#include "BML/Guids/TT_DatabaseManager_RT.h"
#include "BML/Guids/TT_Gravity_RT.h"
#include "BML/Guids/TT_InterfaceManager_RT.h"
#include "BML/Guids/TT_ParticleSystems_RT.h"
#include "BML/Guids/TT_Toolbox_RT.h"
#include "BML/Guids/Visuals.h"
#include "BML/Guids/WorldEnvironments.h"

#endif // BML_GUIDS_H
