#include "ScriptCK2.h"

#include <cassert>

#include "ScriptCKTypes.h"
#include "ScriptCKEnums.h"
#include "ScriptXObjectArray.h"
#include "ScriptCKDefines.h"
#include "ScriptCKContext.h"
#include "ScriptCKManagers.h"
#include "ScriptCKObjects.h"

void RegisterCK2(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKTypedefs(engine);
    RegisterCKEnums(engine);
    RegisterCKObjectTypes(engine);
    RegisterCKContainers(engine);

    // Array derived types need to be registered earlier
    RegisterXSObjectPointerArray(engine);
    RegisterXSObjectArray(engine);
    RegisterXObjectPointerArray(engine);
    RegisterXObjectArray(engine);
    RegisterCKDependencies(engine);

    RegisterCKFuncdefs(engine);
    RegisterCKGlobalVariables(engine);
    RegisterCKGlobalFunctions(engine);

    RegisterCKFileInfo(engine);
    RegisterCKStats(engine);
    RegisterVxDriverDesc(engine);
    RegisterVxIntersectionDesc(engine);
    RegisterVxStats(engine);
    RegisterCKGUID(engine);
    RegisterVxEffectDescription(engine);
    RegisterCKBehaviorContext(engine);
    RegisterCKUICallbackStruct(engine);
    RegisterCKClassDesc(engine);
    RegisterCKPluginInfo(engine);
    RegisterCKEnumStruct(engine);
    RegisterCKFlagsStruct(engine);
    RegisterCKStructStruct(engine);
    RegisterCKParameterTypeDesc(engine);

    RegisterCKBitmapProperties(engine);
    RegisterCKMovieProperties(engine);

    RegisterCKDataReader(engine);
    RegisterCKModelReader(engine);
    RegisterCKBitmapReader(engine);
    RegisterCKSoundReader(engine);
    RegisterCKMovieReader(engine);

    RegisterCKPluginDll(engine);
    RegisterCKPluginEntryReadersData(engine);
    RegisterCKPluginEntryBehaviorsData(engine);
    RegisterCKPluginEntry(engine);
    RegisterCKPluginCategory(engine);

    RegisterCKOperationDesc(engine);

    RegisterCKAttributeVal(engine);

    RegisterCKTimeProfiler(engine);

    RegisterCKMessage(engine);
    RegisterCKWaitingObject(engine);

    RegisterCKPATHCATEGORY(engine);

    RegisterCKPARAMETER_DESC(engine);
    RegisterCKBEHAVIORIO_DESC(engine);
    RegisterCKBehaviorPrototype(engine);

    RegisterCKBitmapSlot(engine);
    RegisterCKMovieInfo(engine);
    RegisterCKBitmapData(engine);

    RegisterCKVertexBuffer(engine);

    RegisterCKFloorPoint(engine);

    RegisterSoundMinion(engine);
    RegisterCKWaveSoundSettings(engine);
    RegisterCKWaveSound3DSettings(engine);
    RegisterCKListenerSettings(engine);
    RegisterCKWaveFormat(engine);

    RegisterImpactDesc(engine);

    RegisterCKPICKRESULT(engine);

    RegisterCKSquare(engine);

    RegisterCK2dCurvePoint(engine);
    RegisterCK2dCurve(engine);

    RegisterCKKeyframeData(engine);

    RegisterCKAnimKey(engine);

    RegisterCKSceneObjectDesc(engine);

    RegisterCKPatch(engine);
    RegisterCKPatchEdge(engine);
    RegisterCKTVPatch(engine);

    RegisterCKSkinBoneData(engine);
    RegisterCKSkinVertexData(engine);
    RegisterCKSkin(engine);

    RegisterCKIkJoint(engine);

    RegisterCKFileManagerData(engine);
    RegisterCKFilePluginDependencies(engine);
    RegisterCKFileObject(engine);

    RegisterCKStateChunk(engine);
    RegisterCKFile(engine);

    RegisterCKDependenciesContext(engine);

    RegisterCKDebugContext(engine);

    RegisterCKObjectArray(engine);
    RegisterCKObjectDeclaration(engine);

    RegisterCKContext(engine);

    RegisterCKPluginManager(engine);
    RegisterCKBaseManager(engine);
    RegisterCKParameterManager(engine);
    RegisterCKAttributeManager(engine);
    RegisterCKTimeManager(engine);
    RegisterCKMessageManager(engine);
    RegisterCKBehaviorManager(engine);
    RegisterCKPathManager(engine);
    RegisterCKRenderManager(engine);
    RegisterCKFloorManager(engine);
    RegisterCKGridManager(engine);
    RegisterCKInterfaceManager(engine);
    RegisterCKSoundManager(engine);
    RegisterCKMidiManager(engine);
    RegisterCKInputManager(engine);
    RegisterCKCollisionManager(engine);

    RegisterCKObject(engine);
    RegisterCKInterfaceObjectManager(engine);
    RegisterCKParameterIn(engine);
    RegisterCKParameter(engine);
    RegisterCKParameterOut(engine);
    RegisterCKParameterLocal(engine);
    RegisterCKParameterOperation(engine);
    RegisterCKBehaviorLink(engine);
    RegisterCKBehaviorIO(engine);
    RegisterCKRenderContext(engine);
    RegisterCKSynchroObject(engine);
    RegisterCKStateObject(engine);
    RegisterCKCriticalSectionObject(engine);
    RegisterCKKinematicChain(engine);
    RegisterCKLayer(engine);
    RegisterCKSceneObject(engine);
    RegisterCKBehavior(engine);
    RegisterCKObjectAnimation(engine);
    RegisterCKAnimation(engine);
    RegisterCKKeyedAnimation(engine);
    RegisterCKBeObject(engine);
    RegisterCKScene(engine);
    RegisterCKLevel(engine);
    RegisterCKGroup(engine);
    RegisterCKMaterial(engine);
    RegisterCKTexture(engine);
    RegisterCKMesh(engine);
    RegisterCKPatchMesh(engine);
    RegisterCKDataArray(engine);
    RegisterCKSound(engine);
    RegisterCKWaveSound(engine);
    RegisterCKMidiSound(engine);
    RegisterCKRenderObject(engine);
    RegisterCK2dEntity(engine);
    RegisterCKSprite(engine);
    RegisterCKSpriteText(engine);
    RegisterCK3dEntity(engine);
    RegisterCKCamera(engine);
    RegisterCKTargetCamera(engine);
    RegisterCKPlace(engine);
    RegisterCKCurvePoint(engine);
    RegisterCKSprite3D(engine);
    RegisterCKLight(engine);
    RegisterCKTargetLight(engine);
    RegisterCKCharacter(engine);
    RegisterCK3dObject(engine);
    RegisterCKBodyPart(engine);
    RegisterCKCurve(engine);
    RegisterCKGrid(engine);
}