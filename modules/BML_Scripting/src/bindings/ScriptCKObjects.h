#ifndef CK_SCRIPTCKOBJECTS_H
#define CK_SCRIPTCKOBJECTS_H

#include <angelscript.h>

void RegisterCKObject(asIScriptEngine *engine);
void RegisterCKInterfaceObjectManager(asIScriptEngine *engine);
void RegisterCKParameterIn(asIScriptEngine *engine);
void RegisterCKParameter(asIScriptEngine *engine);
void RegisterCKParameterOut(asIScriptEngine *engine);
void RegisterCKParameterLocal(asIScriptEngine *engine);
void RegisterCKParameterOperation(asIScriptEngine *engine);
void RegisterCKBehaviorLink(asIScriptEngine *engine);
void RegisterCKBehaviorIO(asIScriptEngine *engine);
void RegisterCKRenderContext(asIScriptEngine *engine);
void RegisterCKSynchroObject(asIScriptEngine *engine);
void RegisterCKStateObject(asIScriptEngine *engine);
void RegisterCKCriticalSectionObject(asIScriptEngine *engine);
void RegisterCKKinematicChain(asIScriptEngine *engine);
void RegisterCKLayer(asIScriptEngine *engine);
void RegisterCKSceneObject(asIScriptEngine *engine);
void RegisterCKBehavior(asIScriptEngine *engine);
void RegisterCKObjectAnimation(asIScriptEngine *engine);
void RegisterCKAnimation(asIScriptEngine *engine);
void RegisterCKKeyedAnimation(asIScriptEngine *engine);
void RegisterCKBeObject(asIScriptEngine *engine);
void RegisterCKScene(asIScriptEngine *engine);
void RegisterCKLevel(asIScriptEngine *engine);
void RegisterCKGroup(asIScriptEngine *engine);
void RegisterCKMaterial(asIScriptEngine *engine);
void RegisterCKTexture(asIScriptEngine *engine);
void RegisterCKMesh(asIScriptEngine *engine);
void RegisterCKPatchMesh(asIScriptEngine *engine);
void RegisterCKDataArray(asIScriptEngine *engine);
void RegisterCKSound(asIScriptEngine *engine);
void RegisterCKWaveSound(asIScriptEngine *engine);
void RegisterCKMidiSound(asIScriptEngine *engine);
void RegisterCKRenderObject(asIScriptEngine *engine);
void RegisterCK2dEntity(asIScriptEngine *engine);
void RegisterCKSprite(asIScriptEngine *engine);
void RegisterCKSpriteText(asIScriptEngine *engine);
void RegisterCK3dEntity(asIScriptEngine *engine);
void RegisterCKCamera(asIScriptEngine *engine);
void RegisterCKTargetCamera(asIScriptEngine *engine);
void RegisterCKPlace(asIScriptEngine *engine);
void RegisterCKCurvePoint(asIScriptEngine *engine);
void RegisterCKSprite3D(asIScriptEngine *engine);
void RegisterCKLight(asIScriptEngine *engine);
void RegisterCKTargetLight(asIScriptEngine *engine);
void RegisterCKCharacter(asIScriptEngine *engine);
void RegisterCK3dObject(asIScriptEngine *engine);
void RegisterCKBodyPart(asIScriptEngine *engine);
void RegisterCKCurve(asIScriptEngine *engine);
void RegisterCKGrid(asIScriptEngine *engine);

#endif // CK_SCRIPTCKOBJECTS_H
