#include "ExecuteBB.h"

#include "Defines.h"
#include "ScriptHelper.h"
#include "ModLoader.h"

using namespace ScriptHelper;

namespace ExecuteBB {
    int GameFonts[8] = {0};

    CKBehavior *bbPhysConv, *bbPhysBall, *bbPhysConc;
    CKBehavior *bbObjLoad;
    CKBehavior *bbPhyImpul;
    CKBehavior *bbSetForce;
    CKBehavior *bbPhysicsWakeUp;

    void Init() {
        for (int i = 0; i < 8; i++)
            GameFonts[i] = i;
        CKBehavior *ownerScript = ModLoader::GetInstance().GetScriptByName("Level_Init");

        bbPhysConv = CreatePhysicalizeConvex(ownerScript);
        bbPhysBall = CreatePhysicalizeBall(ownerScript);
        bbPhysConc = CreatePhysicalizeConcave(ownerScript);
        bbObjLoad = CreateObjectLoad(ownerScript);
        bbPhyImpul = CreatePhysicsImpulse(ownerScript);
        bbSetForce = CreateSetPhysicsForce(ownerScript);
        bbPhysicsWakeUp = CreatePhysicsWakeUp(ownerScript);
    }

    void InitFont(FontType type, int fontIndex) {
        GameFonts[type] = fontIndex;
    }

    int GetFont(FontType type) {
        return GameFonts[type];
    }

    FontType GetFontType(int font) {
        for (int i = 0; i < 8; i++)
            if (GameFonts[i] == font)
                return static_cast<FontType>(i);
        return NOFONT;
    }

    void PhysicalizeParam(CKBehavior *beh, CK3dEntity *target, CKBOOL fixed, float friction, float elasticity,
                          float mass, const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl,
                          CKBOOL calcMassCenter, float linearDamp, float rotDamp, const char *collSurface,
                          VxVector massCenter) {
        SetParamObject(beh->GetTargetParameter()->GetDirectSource(), target);
        SetParamValue(beh->GetInputParameter(0)->GetDirectSource(), fixed);
        SetParamValue(beh->GetInputParameter(1)->GetDirectSource(), friction);
        SetParamValue(beh->GetInputParameter(2)->GetDirectSource(), elasticity);
        SetParamValue(beh->GetInputParameter(3)->GetDirectSource(), mass);
        SetParamString(beh->GetInputParameter(4)->GetDirectSource(), collGroup);
        SetParamValue(beh->GetInputParameter(5)->GetDirectSource(), startFrozen);
        SetParamValue(beh->GetInputParameter(6)->GetDirectSource(), enableColl);
        SetParamValue(beh->GetInputParameter(7)->GetDirectSource(), calcMassCenter);
        SetParamValue(beh->GetInputParameter(8)->GetDirectSource(), linearDamp);
        SetParamValue(beh->GetInputParameter(9)->GetDirectSource(), rotDamp);
        SetParamString(beh->GetInputParameter(10)->GetDirectSource(), collSurface);
        SetParamValue(beh->GetLocalParameter(3), massCenter);
    }

    void PhysicalizeConvex(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                           const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                           float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                           CKMesh *mesh) {
        PhysicalizeParam(bbPhysConv, target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                         enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter);
        SetParamObject(bbPhysConv->GetInputParameter(11)->GetDirectSource(), mesh);
        bbPhysConv->ActivateInput(0);
        bbPhysConv->Execute(0);
    }

    void PhysicalizeBall(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                         const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                         float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                         VxVector ballCenter, float ballRadius) {
        PhysicalizeParam(bbPhysBall, target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                         enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter);
        SetParamValue(bbPhysBall->GetInputParameter(11)->GetDirectSource(), ballCenter);
        SetParamValue(bbPhysBall->GetInputParameter(12)->GetDirectSource(), ballRadius);
        bbPhysBall->ActivateInput(0);
        bbPhysBall->Execute(0);
    }

    void PhysicalizeConcave(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                            const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                            float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                            CKMesh *mesh) {
        PhysicalizeParam(bbPhysConc, target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                         enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter);
        SetParamObject(bbPhysConc->GetInputParameter(11)->GetDirectSource(), mesh);
        bbPhysConc->ActivateInput(0);
        bbPhysConc->Execute(0);
    }

    void Unphysicalize(CK3dEntity *target) {
        SetParamObject(bbPhysConv->GetTargetParameter()->GetDirectSource(), target);
        bbPhysConv->ActivateInput(1);
        bbPhysConv->Execute(0);
    }

    void SetPhysicsForce(CK3dEntity *target, VxVector position, CK3dEntity *posRef, VxVector direction,
                         CK3dEntity *directionRef, float force) {
        SetParamObject(bbSetForce->GetTargetParameter()->GetDirectSource(), target);
        SetParamValue(bbSetForce->GetInputParameter(0)->GetDirectSource(), position);
        SetParamObject(bbSetForce->GetInputParameter(1)->GetDirectSource(), posRef);
        SetParamValue(bbSetForce->GetInputParameter(2)->GetDirectSource(), direction);
        SetParamObject(bbSetForce->GetInputParameter(3)->GetDirectSource(), directionRef);
        SetParamValue(bbSetForce->GetInputParameter(4)->GetDirectSource(), force);
        bbSetForce->ActivateInput(0);
        bbSetForce->Execute(0);
    }

    void UnsetPhysicsForce(CK3dEntity *target) {
        SetParamObject(bbSetForce->GetTargetParameter()->GetDirectSource(), target);
        bbSetForce->ActivateInput(1);
        bbSetForce->Execute(0);
    }

    void PhysicsWakeUp(CK3dEntity *target) {
        SetParamObject(bbPhysicsWakeUp->GetTargetParameter()->GetDirectSource(), target);
        bbPhysicsWakeUp->ActivateInput(0);
        bbPhysicsWakeUp->Execute(0);
    }

    void PhysicsImpulse(CK3dEntity *target, VxVector position, CK3dEntity *posRef, VxVector direction,
                        CK3dEntity *dirRef, float impulse) {
        SetParamObject(bbPhyImpul->GetTargetParameter()->GetDirectSource(), target);
        SetParamValue(bbPhyImpul->GetInputParameter(0)->GetDirectSource(), position);
        SetParamObject(bbPhyImpul->GetInputParameter(1)->GetDirectSource(), posRef);
        SetParamValue(bbPhyImpul->GetInputParameter(2)->GetDirectSource(), direction);
        SetParamObject(bbPhyImpul->GetInputParameter(3)->GetDirectSource(), dirRef);
        SetParamValue(bbPhyImpul->GetInputParameter(4)->GetDirectSource(), impulse);
        bbPhyImpul->ActivateInput(0);
        bbPhyImpul->Execute(0);
    }

    std::pair<XObjectArray *, CKObject *> ObjectLoad(const char *file, bool rename, const char *mastername,
                                                     CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                                                     CKBOOL reuseMtl, CKBOOL dynamic) {
        SetParamString(bbObjLoad->GetInputParameter(0)->GetDirectSource(), file);
        SetParamString(bbObjLoad->GetInputParameter(1)->GetDirectSource(), mastername);
        SetParamValue(bbObjLoad->GetInputParameter(2)->GetDirectSource(), filter);
        SetParamValue(bbObjLoad->GetInputParameter(3)->GetDirectSource(), addToScene);
        SetParamValue(bbObjLoad->GetInputParameter(4)->GetDirectSource(), reuseMesh);
        SetParamValue(bbObjLoad->GetInputParameter(5)->GetDirectSource(), reuseMtl);
        SetParamValue(bbObjLoad->GetLocalParameter(0), dynamic);
        bbObjLoad->ActivateInput(0);
        bbObjLoad->Execute(0);

        XObjectArray *array = *static_cast<XObjectArray **>(bbObjLoad->GetOutputParameterWriteDataPtr(0));
        if (rename) {
            static int count = 0;
            count++;
            for (CK_ID *id = array->Begin(); id != array->End(); id++) {
                CKObject *obj = bbObjLoad->GetCKContext()->GetObject(*id);
                if (CKIsChildClassOf(obj, CKCID_BEOBJECT)) {
                    obj->SetName(TOCKSTRING((obj->GetName() + std::string("_BMLLoad_") + std::to_string(count)).c_str()));
                }
            }
        }

        return {array, bbObjLoad->GetOutputParameterObject(1)};
    }

    CKBehavior *Create2DText(CKBehavior *script, CK2dEntity *target, FontType font, const char *text, int align, VxRect margin,
                             Vx2DVector offset, Vx2DVector pindent, CKMaterial *bgmat, float caretsize,
                             CKMaterial *caretmat, int flags) {
        CKBehavior *beh = CreateBB(script, VT_INTERFACE_2DTEXT, true);
        beh->GetTargetParameter()->SetDirectSource(CreateParamObject(script, "Target", CKPGUID_2DENTITY, target));
        beh->GetInputParameter(0)->SetDirectSource(CreateParamValue(script, "Font", CKPGUID_FONT, GetFont(font)));
        beh->GetInputParameter(1)->SetDirectSource(CreateParamString(script, "Text", text));
        beh->GetInputParameter(2)->SetDirectSource(CreateParamValue(script, "Alignment", CKPGUID_ALIGNMENT, align));
        beh->GetInputParameter(3)->SetDirectSource(CreateParamValue(script, "Margins", CKPGUID_RECT, margin));
        beh->GetInputParameter(4)->SetDirectSource(CreateParamValue(script, "Offset", CKPGUID_2DVECTOR, offset));
        beh->GetInputParameter(5)->SetDirectSource(CreateParamValue(script, "Paragraph Indentation", CKPGUID_2DVECTOR, pindent));
        beh->GetInputParameter(6)->SetDirectSource(CreateParamObject(script, "Background Material", CKPGUID_MATERIAL, bgmat));
        beh->GetInputParameter(7)->SetDirectSource(CreateParamValue(script, "Caret Size", CKPGUID_PERCENTAGE, caretsize));
        beh->GetInputParameter(8)->SetDirectSource(CreateParamObject(script, "Caret Material", CKPGUID_MATERIAL, caretmat));
        SetParamValue(beh->GetLocalParameter(0), flags);
        return beh;
    }

    CKBehavior *CreatePhysicalize(CKBehavior *script, CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                                  const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                  float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter) {
        CKBehavior *beh = CreateBB(script, PHYSICS_RT_PHYSICALIZE, true);
        beh->GetTargetParameter()->SetDirectSource(CreateParamObject(script, "Target", CKPGUID_3DENTITY, target));
        beh->GetInputParameter(0)->SetDirectSource(CreateParamValue(script, "Fixed", CKPGUID_BOOL, fixed));
        beh->GetInputParameter(1)->SetDirectSource(CreateParamValue(script, "Friction", CKPGUID_FLOAT, friction));
        beh->GetInputParameter(2)->SetDirectSource(CreateParamValue(script, "Elasticity", CKPGUID_FLOAT, elasticity));
        beh->GetInputParameter(3)->SetDirectSource(CreateParamValue(script, "Mass", CKPGUID_FLOAT, mass));
        beh->GetInputParameter(4)->SetDirectSource(CreateParamString(script, "Collision Group", collGroup));
        beh->GetInputParameter(5)->SetDirectSource(CreateParamValue(script, "Start Frozen", CKPGUID_BOOL, startFrozen));
        beh->GetInputParameter(6)->SetDirectSource(CreateParamValue(script, "Enable Collision", CKPGUID_BOOL, enableColl));
        beh->GetInputParameter(7)->SetDirectSource(CreateParamValue(script, "Calculate Mass Center", CKPGUID_BOOL, calcMassCenter));
        beh->GetInputParameter(8)->SetDirectSource(CreateParamValue(script, "Linear Speed Damp", CKPGUID_FLOAT, linearDamp));
        beh->GetInputParameter(9)->SetDirectSource(CreateParamValue(script, "Rot Speed Damp", CKPGUID_FLOAT, rotDamp));
        beh->GetInputParameter(10)->SetDirectSource(CreateParamString(script, "Collision Surface", collSurface));
        SetParamValue(beh->GetLocalParameter(3), massCenter);
        return beh;
    }

    CKBehavior *CreatePhysicalizeConvex(CKBehavior *script, CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                                        const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl,
                                        CKBOOL calcMassCenter, float linearDamp, float rotDamp, const char *collSurface,
                                        VxVector massCenter, CKMesh *mesh) {
        CKBehavior *beh = CreatePhysicalize(script, target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                                            enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter);
        beh->GetInputParameter(11)->SetDirectSource(CreateParamObject(script, "Mesh", CKPGUID_MESH, mesh));
        return beh;
    }

    CKBehavior *CreatePhysicalizeBall(CKBehavior *script, CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                                      const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl,
                                      CKBOOL calcMassCenter, float linearDamp, float rotDamp, const char *collSurface,
                                      VxVector massCenter, VxVector ballCenter, float ballRadius) {
        CKBehavior *beh = CreatePhysicalize(script, target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                                            enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter);
        SetParamValue(beh->GetLocalParameter(0), 0);
        SetParamValue(beh->GetLocalParameter(1), 1);
        beh->CallCallbackFunction(CKM_BEHAVIORSETTINGSEDITED);
        beh->GetInputParameter(11)->SetDirectSource(CreateParamValue(script, "Ball Position", CKPGUID_VECTOR, ballCenter));
        beh->GetInputParameter(12)->SetDirectSource(CreateParamValue(script, "Ball Radius", CKPGUID_FLOAT, ballRadius));
        return beh;
    }

    CKBehavior *CreatePhysicalizeConcave(CKBehavior *script, CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                                         const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl,
                                         CKBOOL calcMassCenter, float linearDamp, float rotDamp,
                                         const char *collSurface, VxVector massCenter, CKMesh *mesh) {
        CKBehavior *beh = CreatePhysicalize(script, target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                                            enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter);
        SetParamValue(beh->GetLocalParameter(0), 0);
        SetParamValue(beh->GetLocalParameter(2), 1);
        beh->CallCallbackFunction(CKM_BEHAVIORSETTINGSEDITED);
        beh->GetInputParameter(11)->SetDirectSource(CreateParamObject(script, "Mesh", CKPGUID_MESH, mesh));
        return beh;
    }

    CKBehavior *CreateSetPhysicsForce(CKBehavior *script, CK3dEntity *target, VxVector position, CK3dEntity *posRef, VxVector direction,
                                      CK3dEntity *directionRef, float force) {
        CKBehavior *beh = CreateBB(script, PHYSICS_RT_PHYSICSFORCE, true);
        beh->GetTargetParameter()->SetDirectSource(CreateParamObject(script, "Target", CKPGUID_3DENTITY, target));
        beh->GetInputParameter(0)->SetDirectSource(CreateParamValue(script, "Position", CKPGUID_VECTOR, position));
        beh->GetInputParameter(1)->SetDirectSource(CreateParamObject(script, "Pos Referential", CKPGUID_3DENTITY, posRef));
        beh->GetInputParameter(2)->SetDirectSource(CreateParamValue(script, "Direction", CKPGUID_VECTOR, direction));
        beh->GetInputParameter(3)->SetDirectSource(CreateParamObject(script, "Direction Ref", CKPGUID_3DENTITY, directionRef));
        beh->GetInputParameter(4)->SetDirectSource(CreateParamValue(script, "Force Value", CKPGUID_FLOAT, force));
        return beh;
    }

    CKBehavior *CreatePhysicsWakeUp(CKBehavior *script, CK3dEntity *target) {
        CKBehavior *beh = CreateBB(script, PHYSICS_RT_PHYSICSWAKEUP, true);
        beh->GetTargetParameter()->SetDirectSource(CreateParamObject(script, "Target", CKPGUID_3DENTITY, target));
        return beh;
    }

    CKBehavior *CreatePhysicsImpulse(CKBehavior *script, CK3dEntity *target, VxVector position, CK3dEntity *posRef, VxVector direction,
                                     CK3dEntity *dirRef, float impulse) {
        CKBehavior *beh = CreateBB(script, PHYSICS_RT_PHYSICSIMPULSE, true);
        beh->GetTargetParameter()->SetDirectSource(CreateParamObject(script, "Target", CKPGUID_3DENTITY, target));
        beh->GetInputParameter(0)->SetDirectSource(CreateParamValue(script, "Position", CKPGUID_VECTOR, position));
        beh->GetInputParameter(1)->SetDirectSource(CreateParamObject(script, "PosRef", CKPGUID_3DENTITY, posRef));
        beh->GetInputParameter(2)->SetDirectSource(CreateParamValue(script, "Direction", CKPGUID_VECTOR, direction));
        beh->GetInputParameter(3)->SetDirectSource(CreateParamObject(script, "DirRef", CKPGUID_3DENTITY, dirRef));
        beh->GetInputParameter(4)->SetDirectSource(CreateParamValue(script, "Impulse", CKPGUID_FLOAT, impulse));
        return beh;
    }

    CKBehavior *CreateObjectLoad(CKBehavior *script, const char *file, const char *mastername, CK_CLASSID filter, CKBOOL addToScene,
                                 CKBOOL reuseMesh, CKBOOL reuseMtl, CKBOOL dynamic) {
        CKBehavior *beh = CreateBB(script, VT_NARRATIVES_OBJECTLOAD);
        beh->GetInputParameter(0)->SetDirectSource(CreateParamString(script, "File", file));
        beh->GetInputParameter(1)->SetDirectSource(CreateParamString(script, "Master Name", mastername));
        beh->GetInputParameter(2)->SetDirectSource(CreateParamValue(script, "Filter", CKPGUID_CLASSID, filter));
        beh->GetInputParameter(3)->SetDirectSource(CreateParamValue(script, "Add to Scene", CKPGUID_BOOL, addToScene));
        beh->GetInputParameter(4)->SetDirectSource(CreateParamValue(script, "Reuse Mesh", CKPGUID_BOOL, reuseMesh));
        beh->GetInputParameter(5)->SetDirectSource(CreateParamValue(script, "Reuse Material", CKPGUID_BOOL, reuseMtl));
        SetParamValue(beh->GetLocalParameter(0), dynamic);
        return beh;
    }

    CKBehavior *CreateSendMessage(CKBehavior *script, const char *msg, CKBeObject *dest) {
        CKBehavior *beh = CreateBB(script, VT_LOGICS_SENDMESSAGE);
        beh->GetInputParameter(0)->SetDirectSource(CreateParamString(script, "Message", msg));
        beh->GetInputParameter(1)->SetDirectSource(CreateParamObject(script, "Dest", CKPGUID_BEOBJECT, dest));
        return beh;
    }

    CKBehavior *CreateHookBlock(CKBehavior *script, void (*callback)(void *), void *arg, int inCount, int outCount) {
        CKBehavior *beh = CreateBB(script, BML_HOOKBLOCK_GUID);
        beh->SetLocalParameterValue(0, &callback);
        beh->SetLocalParameterValue(1, &arg);

        XString pinName = "In ";
        for (int i = 0; i < inCount; i++) {
            beh->CreateInput((pinName << i).Str());
        }

        XString poutName = "Out ";
        for (int i = 0; i < outCount; i++) {
            beh->CreateOutput((poutName << i).Str());
        }

        return beh;
    }
}