#ifndef CK_SCRIPTCKDEFINES_H
#define CK_SCRIPTCKDEFINES_H

#include <angelscript.h>

void RegisterCKGlobalVariables(asIScriptEngine *engine);
void RegisterCKGlobalFunctions(asIScriptEngine *engine);

void RegisterCKFileInfo(asIScriptEngine *engine);
void RegisterCKStats(asIScriptEngine *engine);
void RegisterVxDriverDesc(asIScriptEngine *engine);
void RegisterVxIntersectionDesc(asIScriptEngine *engine);
void RegisterVxStats(asIScriptEngine *engine);
void RegisterCKGUID(asIScriptEngine *engine);
void RegisterVxEffectDescription(asIScriptEngine *engine);
void RegisterCKBehaviorContext(asIScriptEngine *engine);
void RegisterCKUICallbackStruct(asIScriptEngine *engine);
void RegisterCKClassDesc(asIScriptEngine *engine);
void RegisterCKPluginInfo(asIScriptEngine *engine);
void RegisterCKEnumStruct(asIScriptEngine *engine);
void RegisterCKFlagsStruct(asIScriptEngine *engine);
void RegisterCKStructStruct(asIScriptEngine *engine);
void RegisterCKParameterTypeDesc(asIScriptEngine *engine);

void RegisterCKBitmapProperties(asIScriptEngine *engine);
void RegisterCKMovieProperties(asIScriptEngine *engine);

void RegisterCKDataReader(asIScriptEngine *engine);
void RegisterCKModelReader(asIScriptEngine *engine);
void RegisterCKBitmapReader(asIScriptEngine *engine);
void RegisterCKSoundReader(asIScriptEngine *engine);
void RegisterCKMovieReader(asIScriptEngine *engine);

void RegisterCKPluginDll(asIScriptEngine *engine);
void RegisterCKPluginEntryReadersData(asIScriptEngine *engine);
void RegisterCKPluginEntryBehaviorsData(asIScriptEngine *engine);
void RegisterCKPluginEntry(asIScriptEngine *engine);
void RegisterCKPluginCategory(asIScriptEngine *engine);

void RegisterCKOperationDesc(asIScriptEngine *engine);

void RegisterCKAttributeVal(asIScriptEngine *engine);

void RegisterCKTimeProfiler(asIScriptEngine *engine);

void RegisterCKMessage(asIScriptEngine *engine);
void RegisterCKWaitingObject(asIScriptEngine *engine);

void RegisterCKPATHCATEGORY(asIScriptEngine *engine);

void RegisterCKPARAMETER_DESC(asIScriptEngine *engine);
void RegisterCKBEHAVIORIO_DESC(asIScriptEngine *engine);
void RegisterCKBehaviorPrototype(asIScriptEngine *engine);

void RegisterCKBitmapSlot(asIScriptEngine *engine);
void RegisterCKMovieInfo(asIScriptEngine *engine);
void RegisterCKBitmapData(asIScriptEngine *engine);

void RegisterCKVertexBuffer(asIScriptEngine *engine);

void RegisterCKFloorPoint(asIScriptEngine *engine);

void RegisterSoundMinion(asIScriptEngine *engine);
void RegisterCKWaveSoundSettings(asIScriptEngine *engine);
void RegisterCKWaveSound3DSettings(asIScriptEngine *engine);
void RegisterCKListenerSettings(asIScriptEngine *engine);
void RegisterCKWaveFormat(asIScriptEngine *engine);

void RegisterImpactDesc(asIScriptEngine *engine);

void RegisterCKPICKRESULT(asIScriptEngine *engine);

void RegisterCKSquare(asIScriptEngine *engine);

void RegisterCK2dCurvePoint(asIScriptEngine *engine);
void RegisterCK2dCurve(asIScriptEngine *engine);

void RegisterCKKeyframeData(asIScriptEngine *engine);

void RegisterCKAnimKey(asIScriptEngine *engine);

void RegisterCKSceneObjectDesc(asIScriptEngine *engine);

void RegisterCKPatch(asIScriptEngine *engine);
void RegisterCKPatchEdge(asIScriptEngine *engine);
void RegisterCKTVPatch(asIScriptEngine *engine);

void RegisterCKSkinBoneData(asIScriptEngine *engine);
void RegisterCKSkinVertexData(asIScriptEngine *engine);
void RegisterCKSkin(asIScriptEngine *engine);

void RegisterCKIkJoint(asIScriptEngine *engine);

void RegisterCKFileManagerData(asIScriptEngine *engine);
void RegisterCKFilePluginDependencies(asIScriptEngine *engine);
void RegisterCKFileObject(asIScriptEngine *engine);

void RegisterCKStateChunk(asIScriptEngine *engine);
void RegisterCKFile(asIScriptEngine *engine);

void RegisterCKDependencies(asIScriptEngine *engine);
void RegisterCKDependenciesContext(asIScriptEngine *engine);

void RegisterCKDebugContext(asIScriptEngine *engine);

void RegisterCKObjectArray(asIScriptEngine *engine);
void RegisterCKObjectDeclaration(asIScriptEngine *engine);

#endif // CK_SCRIPTCKDEFINES_H
