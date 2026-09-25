#ifndef BML_HOOKS_BEHAVIOR_FUNCTION_PATCH_H
#define BML_HOOKS_BEHAVIOR_FUNCTION_PATCH_H

#include "CKBehavior.h"

enum class BehaviorFunctionPatchError {
    None,
    AlreadyInstalled,
    PrototypeUnavailable,
    InvalidOriginal,
    InvalidReplacement,
    OwnershipLost,
};

struct BehaviorFunctionPatchResult {
    BehaviorFunctionPatchError Code = BehaviorFunctionPatchError::None;

    explicit operator bool() const { return Code == BehaviorFunctionPatchError::None; }
};

class BehaviorFunctionPatch {
public:
    BehaviorFunctionPatch() = default;
    ~BehaviorFunctionPatch();

    BehaviorFunctionPatch(const BehaviorFunctionPatch &) = delete;
    BehaviorFunctionPatch &operator=(const BehaviorFunctionPatch &) = delete;

    BehaviorFunctionPatchResult Install(CKGUID prototypeGuid, CKBEHAVIORFCT replacement);
    BehaviorFunctionPatchResult Remove();

    bool IsInstalled() const { return m_Replacement != nullptr; }
    CKBEHAVIORFCT Original() const { return m_Original; }

    static const char *GetErrorName(BehaviorFunctionPatchError error);

private:
    void Clear();

    CKGUID m_PrototypeGuid = CKGUID(0, 0);
    CKBEHAVIORFCT m_Original = nullptr;
    CKBEHAVIORFCT m_Replacement = nullptr;
};

#endif // BML_HOOKS_BEHAVIOR_FUNCTION_PATCH_H
