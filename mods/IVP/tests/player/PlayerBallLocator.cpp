#include "PlayerBallLocator.h"

#include <BML/IBML.h>

#include "CKAll.h"

namespace BML::PlayerTest {

CK3dEntity *ResolveRetailBall(IBML *bml) {
    CKDataArray *currentLevel = bml ? bml->GetArrayByName("CurrentLevel")
                                    : nullptr;
    if (!currentLevel || currentLevel->GetRowCount() < 1 ||
        currentLevel->GetColumnCount() < 2)
        return nullptr;
    CKObject *cell = currentLevel->GetElementObject(0, 1);
    CKParameter *parameter = cell && CKIsChildClassOf(cell, CKCID_PARAMETER)
        ? static_cast<CKParameter *>(cell) : nullptr;
    CKObject *object = parameter ? parameter->GetValueObject() : cell;
    return CK3dEntity::Cast(object);
}

} // namespace BML::PlayerTest
