#ifndef BML_API_GAMEPLAYREGISTRATIONAPI_H
#define BML_API_GAMEPLAYREGISTRATIONAPI_H

#include "BML/Gameplay.h"
#include "BML/Scene.h"
#include "BML/UI.h"

namespace BML::Api {
int BML_CDECL RegisterBallType(BML_ModContextHandle, const BML_BallTypeDefinition *);
int BML_CDECL RegisterFloorType(BML_ModContextHandle, const BML_FloorTypeDefinition *);
int BML_CDECL RegisterModuleBall(BML_ModContextHandle, const BML_ModuleBallDefinition *);
int BML_CDECL RegisterModuleConvex(BML_ModContextHandle, const BML_ModuleConvexDefinition *);
int BML_CDECL RegisterTrafo(BML_ModContextHandle, const BML_TrafoDefinition *);
int BML_CDECL RegisterModule(BML_ModContextHandle, const BML_ModuleDefinition *);
int BML_CDECL ReadGameplayState(BML_GameplayState *);
int BML_CDECL EnableCheat(BML_ModContextHandle, uint32_t);
int BML_CDECL ExitGame(BML_ModContextHandle);
int BML_CDECL SetIC(BML_ModContextHandle, BML_ObjectRef, uint32_t);
int BML_CDECL RestoreIC(BML_ModContextHandle, BML_ObjectRef, uint32_t);
int BML_CDECL Show(BML_ModContextHandle, BML_ObjectRef, uint32_t, uint32_t);
int BML_CDECL SkipNextRender(BML_ModContextHandle);
int BML_CDECL SetWidescreenAdjustment(BML_ModContextHandle, uint32_t);
} // namespace BML::Api
#endif
