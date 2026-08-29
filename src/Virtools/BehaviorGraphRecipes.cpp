#include "Virtools/BehaviorGraphRecipes.h"

#include <vector>

#include "BML/Guids/Hooks.h"
#include "BML/Guids/Interface.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/Narratives.h"
#include "BML/Guids/physics_RT.h"
#include "BML/ScriptHelper.h"

namespace BML::BehaviorGraphRecipes {
namespace {

void RemoveRecipeBlock(CKBehavior *ownerScript, CKBehavior *behavior) {
    if (!ownerScript || !behavior || !ownerScript->GetCKContext())
        return;

    std::vector<CK_ID> parameterIds;
    if (CKParameterIn *target = behavior->GetTargetParameter()) {
        if (CKParameter *source = target->GetDirectSource())
            parameterIds.push_back(source->GetID());
    }
    for (int i = 0; i < behavior->GetInputParameterCount(); ++i) {
        if (CKParameter *source = behavior->GetInputParameter(i)->GetDirectSource())
            parameterIds.push_back(source->GetID());
    }

    CKContext *context = ownerScript->GetCKContext();
    ownerScript->RemoveSubBehavior(behavior);
    if (context->GetObject(behavior->GetID()))
        context->DestroyObject(behavior->GetID());

    for (int i = ownerScript->GetLocalParameterCount() - 1; i >= 0; --i) {
        CKParameterLocal *parameter = ownerScript->GetLocalParameter(i);
        if (!parameter)
            continue;
        bool ownedByRecipe = false;
        for (CK_ID id : parameterIds) {
            if (parameter->GetID() == id) {
                ownedByRecipe = true;
                break;
            }
        }
        if (!ownedByRecipe)
            continue;
        const CK_ID id = parameter->GetID();
        ownerScript->RemoveLocalParameter(i);
        if (context->GetObject(id))
            context->DestroyObject(id);
    }
}

CKBehavior *AddCheckedBlock(CKBehavior *ownerScript, CKGUID guid, bool useTarget,
                            int inputParameterCount, int outputParameterCount, int localParameterCount) {
    if (!ownerScript || !ownerScript->GetCKContext())
        return nullptr;

    CKBehavior *behavior = ScriptHelper::CreateBB(ownerScript, guid, useTarget);
    if (behavior &&
        behavior->GetInputParameterCount() >= inputParameterCount &&
        behavior->GetOutputParameterCount() >= outputParameterCount &&
        behavior->GetLocalParameterCount() >= localParameterCount) {
        return behavior;
    }

    if (behavior)
        RemoveRecipeBlock(ownerScript, behavior);
    return nullptr;
}

CKBehavior *AddPhysicalizeBase(CKBehavior *ownerScript, const PhysicalizeDefinition &definition,
                               int inputParameterCount) {
    CKBehavior *behavior = AddCheckedBlock(
        ownerScript, PHYSICS_RT_PHYSICALIZE, true, inputParameterCount, 0, 4);
    if (!behavior)
        return nullptr;

    behavior->GetTargetParameter()->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Target", CKPGUID_3DENTITY, definition.Target));
    behavior->GetInputParameter(0)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Fixed", CKPGUID_BOOL, definition.Fixed));
    behavior->GetInputParameter(1)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Friction", CKPGUID_FLOAT, definition.Friction));
    behavior->GetInputParameter(2)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Elasticity", CKPGUID_FLOAT, definition.Elasticity));
    behavior->GetInputParameter(3)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Mass", CKPGUID_FLOAT, definition.Mass));
    behavior->GetInputParameter(4)->SetDirectSource(
        ScriptHelper::CreateParamString(ownerScript, "Collision Group", definition.CollisionGroup.c_str()));
    behavior->GetInputParameter(5)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Start Frozen", CKPGUID_BOOL, definition.StartFrozen));
    behavior->GetInputParameter(6)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Enable Collision", CKPGUID_BOOL, definition.EnableCollision));
    behavior->GetInputParameter(7)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Calculate Mass Center", CKPGUID_BOOL,
                                       definition.CalculateMassCenter));
    behavior->GetInputParameter(8)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Linear Speed Damp", CKPGUID_FLOAT,
                                       definition.LinearDamping));
    behavior->GetInputParameter(9)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Rot Speed Damp", CKPGUID_FLOAT,
                                       definition.RotationalDamping));
    behavior->GetInputParameter(10)->SetDirectSource(
        ScriptHelper::CreateParamString(ownerScript, "Collision Surface", definition.CollisionSurface.c_str()));
    ScriptHelper::SetParamValue(behavior->GetLocalParameter(3), definition.MassCenter);
    return behavior;
}

} // namespace

CKBehavior *Add2DText(CKBehavior *ownerScript, const Text2DDefinition &definition) {
    CKBehavior *behavior = AddCheckedBlock(ownerScript, VT_INTERFACE_2DTEXT, true, 9, 0, 1);
    if (!behavior)
        return nullptr;

    behavior->GetTargetParameter()->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Target", CKPGUID_2DENTITY, definition.Target));
    behavior->GetInputParameter(0)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Font", CKPGUID_FONT, definition.FontIndex));
    behavior->GetInputParameter(1)->SetDirectSource(
        ScriptHelper::CreateParamString(ownerScript, "Text", definition.Text.c_str()));
    behavior->GetInputParameter(2)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Alignment", CKPGUID_ALIGNMENT, definition.Alignment));
    behavior->GetInputParameter(3)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Margins", CKPGUID_RECT, definition.Margin));
    behavior->GetInputParameter(4)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Offset", CKPGUID_2DVECTOR, definition.Offset));
    behavior->GetInputParameter(5)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Paragraph Indentation", CKPGUID_2DVECTOR,
                                       definition.ParagraphIndentation));
    behavior->GetInputParameter(6)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Background Material", CKPGUID_MATERIAL,
                                        definition.BackgroundMaterial));
    behavior->GetInputParameter(7)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Caret Size", CKPGUID_PERCENTAGE, definition.CaretSize));
    behavior->GetInputParameter(8)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Caret Material", CKPGUID_MATERIAL,
                                        definition.CaretMaterial));
    ScriptHelper::SetParamValue(behavior->GetLocalParameter(0), definition.Flags);
    return behavior;
}

CKBehavior *AddPhysicalizeConvex(CKBehavior *ownerScript, const PhysicalizeDefinition &definition, CKMesh *mesh) {
    CKBehavior *behavior = AddPhysicalizeBase(ownerScript, definition, 12);
    if (!behavior)
        return nullptr;
    behavior->GetInputParameter(11)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Mesh", CKPGUID_MESH, mesh));
    return behavior;
}

CKBehavior *AddPhysicalizeBall(CKBehavior *ownerScript, const PhysicalizeDefinition &definition,
                               VxVector ballCenter, float ballRadius) {
    CKBehavior *behavior = AddPhysicalizeBase(ownerScript, definition, 11);
    if (!behavior)
        return nullptr;

    ScriptHelper::SetParamValue(behavior->GetLocalParameter(0), 0);
    ScriptHelper::SetParamValue(behavior->GetLocalParameter(1), 1);
    behavior->CallCallbackFunction(CKM_BEHAVIORSETTINGSEDITED);
    if (behavior->GetInputParameterCount() < 13) {
        RemoveRecipeBlock(ownerScript, behavior);
        return nullptr;
    }
    behavior->GetInputParameter(11)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Ball Position", CKPGUID_VECTOR, ballCenter));
    behavior->GetInputParameter(12)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Ball Radius", CKPGUID_FLOAT, ballRadius));
    return behavior;
}

CKBehavior *AddPhysicalizeConcave(CKBehavior *ownerScript, const PhysicalizeDefinition &definition, CKMesh *mesh) {
    CKBehavior *behavior = AddPhysicalizeBase(ownerScript, definition, 11);
    if (!behavior)
        return nullptr;

    ScriptHelper::SetParamValue(behavior->GetLocalParameter(0), 0);
    ScriptHelper::SetParamValue(behavior->GetLocalParameter(2), 1);
    behavior->CallCallbackFunction(CKM_BEHAVIORSETTINGSEDITED);
    if (behavior->GetInputParameterCount() < 12) {
        RemoveRecipeBlock(ownerScript, behavior);
        return nullptr;
    }
    behavior->GetInputParameter(11)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Mesh", CKPGUID_MESH, mesh));
    return behavior;
}

CKBehavior *AddPhysicsForce(CKBehavior *ownerScript, const ForceDefinition &definition) {
    CKBehavior *behavior = AddCheckedBlock(ownerScript, PHYSICS_RT_PHYSICSFORCE, true, 5, 0, 0);
    if (!behavior)
        return nullptr;
    behavior->GetTargetParameter()->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Target", CKPGUID_3DENTITY, definition.Target));
    behavior->GetInputParameter(0)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Position", CKPGUID_VECTOR, definition.Position));
    behavior->GetInputParameter(1)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Pos Referential", CKPGUID_3DENTITY,
                                        definition.PositionReference));
    behavior->GetInputParameter(2)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Direction", CKPGUID_VECTOR, definition.Direction));
    behavior->GetInputParameter(3)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Direction Ref", CKPGUID_3DENTITY,
                                        definition.DirectionReference));
    behavior->GetInputParameter(4)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Force Value", CKPGUID_FLOAT, definition.Magnitude));
    return behavior;
}

CKBehavior *AddPhysicsImpulse(CKBehavior *ownerScript, const ForceDefinition &definition) {
    CKBehavior *behavior = AddCheckedBlock(ownerScript, PHYSICS_RT_PHYSICSIMPULSE, true, 5, 0, 0);
    if (!behavior)
        return nullptr;
    behavior->GetTargetParameter()->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Target", CKPGUID_3DENTITY, definition.Target));
    behavior->GetInputParameter(0)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Position", CKPGUID_VECTOR, definition.Position));
    behavior->GetInputParameter(1)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "PosRef", CKPGUID_3DENTITY, definition.PositionReference));
    behavior->GetInputParameter(2)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Direction", CKPGUID_VECTOR, definition.Direction));
    behavior->GetInputParameter(3)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "DirRef", CKPGUID_3DENTITY, definition.DirectionReference));
    behavior->GetInputParameter(4)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Impulse", CKPGUID_FLOAT, definition.Magnitude));
    return behavior;
}

CKBehavior *AddPhysicsWakeUp(CKBehavior *ownerScript, CK3dEntity *target) {
    CKBehavior *behavior = AddCheckedBlock(ownerScript, PHYSICS_RT_PHYSICSWAKEUP, true, 0, 0, 0);
    if (!behavior)
        return nullptr;
    behavior->GetTargetParameter()->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Target", CKPGUID_3DENTITY, target));
    return behavior;
}

CKBehavior *AddObjectLoad(CKBehavior *ownerScript, const ObjectLoadDefinition &definition) {
    CKBehavior *behavior = AddCheckedBlock(ownerScript, VT_NARRATIVES_OBJECTLOAD, false, 6, 2, 1);
    if (!behavior)
        return nullptr;
    behavior->GetInputParameter(0)->SetDirectSource(
        ScriptHelper::CreateParamString(ownerScript, "File", definition.File.c_str()));
    behavior->GetInputParameter(1)->SetDirectSource(
        ScriptHelper::CreateParamString(ownerScript, "Master Name", definition.MasterName.c_str()));
    behavior->GetInputParameter(2)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Filter", CKPGUID_CLASSID, definition.FilterClass));
    behavior->GetInputParameter(3)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Add to Scene", CKPGUID_BOOL, definition.AddToScene));
    behavior->GetInputParameter(4)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Reuse Mesh", CKPGUID_BOOL, definition.ReuseMeshes));
    behavior->GetInputParameter(5)->SetDirectSource(
        ScriptHelper::CreateParamValue(ownerScript, "Reuse Material", CKPGUID_BOOL, definition.ReuseMaterials));
    ScriptHelper::SetParamValue(behavior->GetLocalParameter(0), definition.Dynamic);
    return behavior;
}

CKBehavior *AddSendMessage(CKBehavior *ownerScript, const char *message, CKBeObject *destination) {
    CKBehavior *behavior = AddCheckedBlock(ownerScript, VT_LOGICS_SENDMESSAGE, false, 2, 0, 0);
    if (!behavior)
        return nullptr;
    behavior->GetInputParameter(0)->SetDirectSource(
        ScriptHelper::CreateParamString(ownerScript, "Message", message ? message : ""));
    behavior->GetInputParameter(1)->SetDirectSource(
        ScriptHelper::CreateParamObject(ownerScript, "Dest", CKPGUID_BEOBJECT, destination));
    return behavior;
}

CKBehavior *AddHookBlock(CKBehavior *ownerScript, BehaviorCallback callback, void *argument,
                         int inputCount, int outputCount) {
    if (!callback || inputCount < 0 || outputCount < 0)
        return nullptr;
    CKBehavior *behavior = AddCheckedBlock(ownerScript, HOOKS_HOOKBLOCK_GUID, false, 0, 0, 2);
    if (!behavior)
        return nullptr;

    behavior->SetLocalParameterValue(0, &callback);
    behavior->SetLocalParameterValue(1, &argument);
    if (behavior->GetLocalParameterCount() > 2) {
        CKBOOL autoActivateOutputs = TRUE;
        behavior->SetLocalParameterValue(2, &autoActivateOutputs);
    }

    XString inputName = "In ";
    for (int i = 0; i < inputCount; ++i)
        behavior->CreateInput((inputName << i).Str());

    XString outputName = "Out ";
    for (int i = 0; i < outputCount; ++i)
        behavior->CreateOutput((outputName << i).Str());
    return behavior;
}

} // namespace BML::BehaviorGraphRecipes
