#include "ScriptCKTypes.h"

#include <cassert>

#include "CKAll.h"

#include "ScriptXArray.h"
#include "ScriptXHashTable.h"

void RegisterCKTypedefs(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterTypedef("CK_CLASSID", "int"); assert(r >= 0);

    r = engine->RegisterTypedef("CK_ID", "uint"); assert(r >= 0);

    r = engine->RegisterTypedef("CKCHAR", "int8"); assert(r >= 0);
    r = engine->RegisterTypedef("CKBOOL", "bool"); assert(r >= 0);
    r = engine->RegisterTypedef("CKBYTE", "uint8"); assert(r >= 0);
    r = engine->RegisterTypedef("CKUINT", "uint"); assert(r >= 0);
    r = engine->RegisterTypedef("CKDWORD", "uint"); assert(r >= 0);
    r = engine->RegisterTypedef("CKWORD", "uint16"); assert(r >= 0);
    // r = engine->RegisterTypedef("CKERROR", "int"); assert(r >= 0);

    r = engine->RegisterTypedef("CKParameterType", "int"); assert(r >= 0);
    r = engine->RegisterTypedef("CKOperationType", "int"); assert(r >= 0);
    r = engine->RegisterTypedef("CKMessageType", "int"); assert(r >= 0);
    r = engine->RegisterTypedef("CKAttributeType", "int"); assert(r >= 0);
    r = engine->RegisterTypedef("CKAttributeCategory", "int"); assert(r >= 0);

    if constexpr (sizeof(void *) == 4) {
        // r = engine->RegisterTypedef("CKSTRING", "uint"); assert(r >= 0);
        r = engine->RegisterTypedef("CKSOUNDHANDLE", "uint"); assert(r >= 0);
    } else {
        // r = engine->RegisterTypedef("CKSTRING", "uint64"); assert(r >= 0);
        r = engine->RegisterTypedef("CKSOUNDHANDLE", "uint64"); assert(r >= 0);
    }
}

void RegisterCKFuncdefs(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterFuncdef("VX_EFFECTCALLBACK_RETVAL CK_EFFECTCALLBACK(CKRenderContext@ dev, CKMaterial@ mat, int stage)"); assert(r >= 0);

    r = engine->RegisterFuncdef("CKERROR CK_INITINSTANCEFCT(CKContext@ context)"); assert(r >= 0);
    r = engine->RegisterFuncdef("CKERROR CK_EXITINSTANCEFCT(CKContext@ context)"); assert(r >= 0);

    r = engine->RegisterFuncdef("int CKBEHAVIORFCT(const CKBehaviorContext &in context)"); assert(r >= 0);
    r = engine->RegisterFuncdef("CKERROR CKBEHAVIORCALLBACKFCT(const CKBehaviorContext &in context)"); assert(r >= 0);

    r = engine->RegisterFuncdef("void CK_PARAMETEROPERATION(CKContext@ context, CKParameterOut@ res, CKParameterIn@ p1, CKParameterIn@ p2)"); assert(r >= 0);

    r = engine->RegisterFuncdef("void CKDLL_OBJECTDECLARATIONFUNCTION(XObjectDeclarationArray &out declArray)"); assert(r >= 0);
    r = engine->RegisterFuncdef("CKERROR CKDLL_CREATEPROTOFUNCTION(CKBehaviorPrototype@ &out prototype)"); assert(r >= 0);

    r = engine->RegisterFuncdef("void CK_RENDERCALLBACK(CKRenderContext@ dev)"); assert(r >= 0);
    r = engine->RegisterFuncdef("CKBOOL CK_RENDEROBJECT_CALLBACK(CKRenderContext@ dev, CKRenderObject@ entity)"); assert(r >= 0);
    r = engine->RegisterFuncdef("void CK_MESHRENDERCALLBACK(CKRenderContext@ dev, CK3dEntity@ mov, CKMesh@ object)"); assert(r >= 0);
    r = engine->RegisterFuncdef("void CK_SUBMESHRENDERCALLBACK(CKRenderContext@ dev, CK3dEntity@ mov, CKMesh@ object, CKMaterial@ mat)"); assert(r >= 0);
#if CKVERSION != 0x26052005
    r = engine->RegisterFuncdef("int CK_MATERIALCALLBACK(CKRenderContext@ dev, CKMaterial@ mat)"); assert(r >= 0);
#endif

    r = engine->RegisterFuncdef("CKERROR CKUICALLBACKFCT(CKUICallbackStruct &in param)"); assert(r >= 0);

    // r = engine->RegisterFuncdef("CK_LOADMODE CK_USERLOADCALLBACK(CK_CLASSID cid, string oldName, string newName, CKObject@ &out newObj)"); assert(r >= 0);
    // r = engine->RegisterFuncdef("CK_LOADMODE CK_LOADRENAMECALLBACK(CK_CLASSID cid, string oldName, string newName, CKObject@ &out newObj)"); assert(r >= 0);

    r = engine->RegisterFuncdef("CKERROR CK_PARAMETERCREATEDEFAULTFUNCTION(CKParameter@ param)"); assert(r >= 0);
    r = engine->RegisterFuncdef("void CK_PARAMETERDELETEFUNCTION(CKParameter@ param)"); assert(r >= 0);
    r = engine->RegisterFuncdef("void CK_PARAMETERCHECKFUNCTION(CKParameter@ param)"); assert(r >= 0);
    r = engine->RegisterFuncdef("void CK_PARAMETERREMAPFUNCTION(CKParameter@ param, CKDependenciesContext@ context)"); assert(r >= 0);
    r = engine->RegisterFuncdef("void CK_PARAMETERCOPYFUNCTION(CKParameter@ srcParam, CKParameter@ destParam)"); assert(r >= 0);
    r = engine->RegisterFuncdef("void CK_PARAMETERSAVELOADFUNCTION(CKParameter@ param, CKStateChunk@ &out chunk, bool load)"); assert(r >= 0);
    r = engine->RegisterFuncdef("int CK_PARAMETERSTRINGFUNCTION(CKParameter@ param, string valueString, bool readFromString)"); assert(r >= 0);
    r = engine->RegisterFuncdef("WIN_HANDLE CK_PARAMETERUICREATORFUNCTION(CKParameter@ param, WIN_HANDLE parentWindow, CKRECT &in rect)"); assert(r >= 0);
}

void RegisterCKObjectTypes(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectType("CKFileInfo", sizeof(CKFileInfo), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<CKFileInfo>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKStats", sizeof(CKStats), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKFileInfo>()); assert(r >= 0);
    r = engine->RegisterObjectType("VxDriverDesc", sizeof(VxDriverDesc), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<VxDriverDesc>()); assert(r >= 0);
    r = engine->RegisterObjectType("VxIntersectionDesc", sizeof(VxIntersectionDesc), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<VxIntersectionDesc>()); assert(r >= 0);
    r = engine->RegisterObjectType("VxStats", sizeof(VxStats), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<VxStats>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKGUID", sizeof(CKGUID), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKGUID>()); assert(r >= 0);
    r = engine->RegisterObjectType("VxEffectDescription", sizeof(VxEffectDescription), asOBJ_VALUE | asGetTypeTraits<VxEffectDescription>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKBehaviorContext", sizeof(CKBehaviorContext), asOBJ_VALUE | asGetTypeTraits<CKBehaviorContext>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKUICallbackStruct", sizeof(CKUICallbackStruct), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKUICallbackStruct>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKClassDesc", sizeof(CKClassDesc), asOBJ_VALUE | asGetTypeTraits<CKClassDesc>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKPluginInfo", sizeof(CKPluginInfo), asOBJ_VALUE | asGetTypeTraits<CKPluginInfo>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKEnumStruct", sizeof(CKEnumStruct), asOBJ_VALUE | asGetTypeTraits<CKEnumStruct>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKFlagsStruct", sizeof(CKFlagsStruct), asOBJ_VALUE | asGetTypeTraits<CKFlagsStruct>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKStructStruct", sizeof(CKStructStruct), asOBJ_VALUE | asGetTypeTraits<CKStructStruct>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKParameterTypeDesc", sizeof(CKParameterTypeDesc), asOBJ_VALUE | asGetTypeTraits<CKParameterTypeDesc>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKBitmapProperties", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMovieProperties", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKDataReader", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKModelReader", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBitmapReader", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSoundReader", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMovieReader", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKPluginDll", sizeof(CKPluginDll), asOBJ_VALUE | asGetTypeTraits<CKPluginDll>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKPluginEntryReadersData", sizeof(CKPluginEntryReadersData), asOBJ_VALUE | asGetTypeTraits<CKPluginEntryReadersData>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKPluginEntryBehaviorsData", sizeof(CKPluginEntryBehaviorsData), asOBJ_VALUE | asGetTypeTraits<CKPluginEntryBehaviorsData>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKPluginEntry", sizeof(CKPluginEntry), asOBJ_VALUE | asGetTypeTraits<CKPluginEntry>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKPluginCategory", sizeof(CKPluginCategory), asOBJ_VALUE | asGetTypeTraits<CKPluginCategory>()); assert(r >= 0);

    // r = engine->RegisterObjectType("CKStructHelper", sizeof(CKStructHelper), asOBJ_VALUE | asGetTypeTraits<CKStructHelper>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKOperationDesc", sizeof(CKOperationDesc), asOBJ_VALUE | asGetTypeTraits<CKOperationDesc>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKAttributeVal", sizeof(CKAttributeVal), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<CKAttributeVal>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKTimeProfiler", sizeof(CKTimeProfiler), asOBJ_VALUE | asGetTypeTraits<CKTimeProfiler>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKMessage", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectType("CKWaitingObject", sizeof(CKWaitingObject), asOBJ_VALUE | asGetTypeTraits<CKWaitingObject>()); assert(r >= 0);
#endif

    r = engine->RegisterObjectType("CKPATHCATEGORY", sizeof(CKPATHCATEGORY), asOBJ_VALUE | asGetTypeTraits<CKPATHCATEGORY>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKPARAMETER_DESC", sizeof(CKPARAMETER_DESC), asOBJ_VALUE | asGetTypeTraits<CKPARAMETER_DESC>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKBEHAVIORIO_DESC", sizeof(CKBEHAVIORIO_DESC), asOBJ_VALUE | asGetTypeTraits<CKBEHAVIORIO_DESC>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKBehaviorPrototype", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKBitmapSlot", sizeof(CKBitmapSlot), asOBJ_VALUE | asGetTypeTraits<CKBitmapSlot>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKMovieInfo", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBitmapData", sizeof(CKBitmapData), asOBJ_VALUE | asGetTypeTraits<CKBitmapData>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKVertexBuffer", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKFloorPoint", sizeof(CKFloorPoint), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKFloorPoint>()); assert(r >= 0);

    r = engine->RegisterObjectType("SoundMinion", sizeof(SoundMinion), asOBJ_VALUE | asGetTypeTraits<SoundMinion>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKWaveSoundSettings", sizeof(CKWaveSoundSettings), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKWaveSoundSettings>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKWaveSound3DSettings", sizeof(CKWaveSound3DSettings), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKWaveSound3DSettings>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKListenerSettings", sizeof(CKListenerSettings), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKListenerSettings>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKWaveFormat", sizeof(CKWaveFormat), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKWaveFormat>()); assert(r >= 0);

    r = engine->RegisterObjectType("ImpactDesc", sizeof(ImpactDesc), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ImpactDesc>()); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectType("CKPICKRESULT", sizeof(CKPICKRESULT), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKPICKRESULT>()); assert(r >= 0);
#endif

    r = engine->RegisterObjectType("CKSquare", sizeof(CKSquare), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKSquare>()); assert(r >= 0);

    r = engine->RegisterObjectType("CK2dCurvePoint", sizeof(CK2dCurvePoint), asOBJ_VALUE | asGetTypeTraits<CK2dCurvePoint>()); assert(r >= 0);
    r = engine->RegisterObjectType("CK2dCurve", sizeof(CK2dCurve), asOBJ_VALUE | asGetTypeTraits<CK2dCurve>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKKey", sizeof(CKKey), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKKey>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKRotationKey", sizeof(CKRotationKey), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKRotationKey>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKPositionKey", sizeof(CKPositionKey), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKPositionKey>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKTCBPositionKey", sizeof(CKTCBPositionKey), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKTCBPositionKey>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKTCBRotationKey", sizeof(CKTCBRotationKey), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKTCBRotationKey>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKBezierKeyFlags", sizeof(CKBezierKeyFlags), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKBezierKeyFlags>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKBezierPositionKey", sizeof(CKBezierPositionKey), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKBezierPositionKey>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKMorphKey", sizeof(CKMorphKey), asOBJ_VALUE | asGetTypeTraits<CKMorphKey>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKAnimController", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMorphController", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKAnimKey", sizeof(CKAnimKey), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKAnimKey>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKSceneObjectDesc", sizeof(CKSceneObjectDesc), asOBJ_VALUE | asGetTypeTraits<CKSceneObjectDesc>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKPatch", sizeof(CKPatch), asOBJ_VALUE | asGetTypeTraits<CKPatch>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKPatchEdge", sizeof(CKPatchEdge), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<CKPatchEdge>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKTVPatch", sizeof(CKTVPatch), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<CKTVPatch>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKSkinBoneData", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSkinVertexData", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSkin", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKIkJoint", sizeof(CKIkJoint), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CKIkJoint>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKFileManagerData", sizeof(CKFileManagerData), asOBJ_VALUE | asGetTypeTraits<CKFileManagerData>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKFilePluginDependencies", sizeof(CKFilePluginDependencies), asOBJ_VALUE | asGetTypeTraits<CKFilePluginDependencies>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKFileObject", sizeof(CKFileObject), asOBJ_VALUE | asGetTypeTraits<CKFileObject>()); assert(r >= 0);

    r = engine->RegisterObjectType("CKStateChunk", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKFile", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    // Registered at other place
    // r = engine->RegisterObjectType("CKDependencies", sizeof(CKDependencies), asOBJ_VALUE | asGetTypeTraits<CKDependencies>()); assert(r >= 0);
    r = engine->RegisterObjectType("CKDependenciesContext", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKDebugContext", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKObjectArray", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKObjectDeclaration", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKContext", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKPluginManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBaseManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKParameterManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKAttributeManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKTimeManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMessageManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBehaviorManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKPathManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKRenderManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKFloorManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKGridManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKInterfaceManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSoundManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMidiManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKInputManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKCollisionManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectType("CKObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKInterfaceObjectManager", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKRenderContext", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKParameterIn", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKParameter", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKParameterOut", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKParameterLocal", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKParameterOperation", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBehaviorLink", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBehaviorIO", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSynchroObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKStateObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKCriticalSectionObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKKinematicChain", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKObjectAnimation", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKLayer", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSceneObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBehavior", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKAnimation", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKKeyedAnimation", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBeObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKScene", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKLevel", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKPlace", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKGroup", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMaterial", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKTexture", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMesh", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKPatchMesh", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKProgressiveMesh", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKDataArray", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSound", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKMidiSound", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKWaveSound", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKRenderObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CK2dEntity", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSprite", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSpriteText", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CK3dEntity", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKCamera", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKTargetCamera", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKCurvePoint", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKSprite3D", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKLight", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKTargetLight", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKCharacter", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CK3dObject", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKBodyPart", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKCurve", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = engine->RegisterObjectType("CKGrid", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
}

void RegisterCKContainers(asIScriptEngine *engine) {
    RegisterXArray<XIntArray, int>(engine, "XIntArray", "int");
    RegisterXArray<XArray<CKGUID>, CKGUID>(engine, "XGUIDArray", "CKGUID");
    RegisterXArray<XObjectDeclarationArray, CKObjectDeclaration *>(engine, "XObjectDeclarationArray", "CKObjectDeclaration@");

    RegisterXClassArray<XClassInfoArray, CKClassDesc>(engine, "XClassInfoArray", "CKClassDesc");
    RegisterXArray<XManagerArray, CKBaseManager *>(engine, "XManagerArray", "CKBaseManager@");

    RegisterXSArray<XSArray<CKDWORD>, CKDWORD>(engine, "XDwordArray", "CKDWORD");
    RegisterXSArray<CKDataRow, CKDWORD>(engine, "CKDataRow", "CKDWORD");
    RegisterXSArray<XSArray<CK_CLASSID>, CK_CLASSID>(engine, "XClassIDArray", "CK_CLASSID");
    RegisterXSArray<XSArray<VxImageDescEx>, VxImageDescEx>(engine, "XImageArray", "VxImageDescEx");

    RegisterXClassArray<XClassArray<XString>, XString>(engine, "XStringArray", "XString");
    RegisterXClassArray<XClassArray<CKFilePluginDependencies>, CKFilePluginDependencies>(engine, "XFilePluginDependenciesArray", "CKFilePluginDependencies");

    RegisterXClassArray<CKPATHENTRYVECTOR, XString>(engine, "CKPATHENTRYVECTOR", "XString");
    RegisterXClassArray<CKPATHCATEGORYVECTOR, XString>(engine, "CKPATHCATEGORYVECTOR", "CKPATHCATEGORY");

    RegisterXHashTable<XManagerHashTable, CKBaseManager *, CKGUID>(engine, "XManagerHashTable", "CKGUID", "CKBaseManager@");
    RegisterXHashTable<XObjDeclHashTable, CKObjectDeclaration *, CKGUID>(engine, "XObjDeclHashTable", "CKGUID", "CKObjectDeclaration@");
    RegisterXHashTable<XFileObjectsTable, int, CK_ID>(engine, "XFileObjectsTable", "CK_ID", "int");
    RegisterXHashTable<CKSODHash, CKSceneObjectDesc, CK_ID>(engine, "CKSODHash", "CK_ID", "CKSceneObjectDesc");

    RegisterXNHashTable<XHashID, CK_ID, CK_ID>(engine, "XHashID", "CK_ID", "CK_ID");
    RegisterXNHashTable<XHashGuidToType, int, CKGUID>(engine, "XHashGuidToType", "CKGUID", "int");
    RegisterXNHashTable<XAttributeList, CKAttributeVal, CK_ID>(engine, "XAttributeList", "CK_ID", "CKAttributeVal");
}

