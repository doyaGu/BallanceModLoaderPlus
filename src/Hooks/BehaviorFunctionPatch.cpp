#include "Hooks/BehaviorFunctionPatch.h"

#include <cassert>

#include "CKBehaviorPrototype.h"

BehaviorFunctionPatch::~BehaviorFunctionPatch() {
    assert(!IsInstalled());
}

BehaviorFunctionPatchResult BehaviorFunctionPatch::Install(
    CKGUID prototypeGuid, CKBEHAVIORFCT replacement) {
    if (!replacement)
        return {BehaviorFunctionPatchError::InvalidReplacement};

    if (IsInstalled()) {
        if (prototypeGuid != m_PrototypeGuid || replacement != m_Replacement)
            return {BehaviorFunctionPatchError::AlreadyInstalled};

        CKBehaviorPrototype *installedPrototype = CKGetPrototypeFromGuid(m_PrototypeGuid);
        if (!installedPrototype)
            return {BehaviorFunctionPatchError::PrototypeUnavailable};
        if (installedPrototype->GetFunction() == m_Replacement)
            return {};

        Clear();
        return {BehaviorFunctionPatchError::OwnershipLost};
    }

    CKBehaviorPrototype *prototype = CKGetPrototypeFromGuid(prototypeGuid);
    if (!prototype)
        return {BehaviorFunctionPatchError::PrototypeUnavailable};

    CKBEHAVIORFCT original = prototype->GetFunction();
    if (!original)
        return {BehaviorFunctionPatchError::InvalidOriginal};

    prototype->SetFunction(replacement);
    m_PrototypeGuid = prototypeGuid;
    m_Original = original;
    m_Replacement = replacement;
    return {};
}

BehaviorFunctionPatchResult BehaviorFunctionPatch::Remove() {
    if (!IsInstalled())
        return {};

    CKBehaviorPrototype *prototype = CKGetPrototypeFromGuid(m_PrototypeGuid);
    if (!prototype)
        return {BehaviorFunctionPatchError::PrototypeUnavailable};

    if (prototype->GetFunction() != m_Replacement) {
        Clear();
        return {BehaviorFunctionPatchError::OwnershipLost};
    }

    prototype->SetFunction(m_Original);
    Clear();
    return {};
}

const char *BehaviorFunctionPatch::GetErrorName(BehaviorFunctionPatchError error) {
    switch (error) {
    case BehaviorFunctionPatchError::None: return "none";
    case BehaviorFunctionPatchError::AlreadyInstalled: return "already installed";
    case BehaviorFunctionPatchError::PrototypeUnavailable: return "prototype unavailable";
    case BehaviorFunctionPatchError::InvalidOriginal: return "invalid behavior function";
    case BehaviorFunctionPatchError::InvalidReplacement: return "invalid replacement function";
    case BehaviorFunctionPatchError::OwnershipLost: return "behavior function ownership changed";
    }
    return "unknown";
}

void BehaviorFunctionPatch::Clear() {
    m_PrototypeGuid = CKGUID(0, 0);
    m_Original = nullptr;
    m_Replacement = nullptr;
}
