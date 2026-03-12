#ifndef BML_MAPMENU_SCRIPTHELPER_H
#define BML_MAPMENU_SCRIPTHELPER_H

#include <functional>

#include "CKAll.h"

namespace BML_MapMenu {
namespace ScriptHelper {

inline bool FindBB(CKBehavior *script, std::function<bool(CKBehavior *)> callback, const char *name = nullptr,
                   bool hierarchically = false,
                   int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1, int outputParamCnt = -1) {
    int count = script->GetSubBehaviorCount();
    for (int index = 0; index < count; ++index) {
        CKBehavior *behavior = script->GetSubBehavior(index);
        if (hierarchically && behavior->GetSubBehaviorCount() > 0) {
            if (!FindBB(behavior, callback, name, hierarchically, inputCnt, outputCnt, inputParamCnt, outputParamCnt)) {
                return false;
            }
        }

        if ((!name || !strcmp(behavior->GetName(), name)) &&
            (inputCnt < 0 || behavior->GetInputCount() == inputCnt) &&
            (outputCnt < 0 || behavior->GetOutputCount() == outputCnt) &&
            (inputParamCnt < 0 || behavior->GetInputParameterCount() == inputParamCnt) &&
            (outputParamCnt < 0 || behavior->GetOutputParameterCount() == outputParamCnt)) {
            if (!callback(behavior)) {
                return false;
            }
        }
    }

    return true;
}

inline CKBehavior *FindFirstBB(CKBehavior *script, const char *name = nullptr, bool hierarchically = false,
                               int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                               int outputParamCnt = -1) {
    CKBehavior *result = nullptr;
    FindBB(script, [&result](CKBehavior *behavior) {
        result = behavior;
        return false;
    }, name, hierarchically, inputCnt, outputCnt, inputParamCnt, outputParamCnt);
    return result;
}

inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehaviorIO *out, int delay = 0) {
    auto *link = static_cast<CKBehaviorLink *>(script->GetCKContext()->CreateObject(CKCID_BEHAVIORLINK));
    link->SetInitialActivationDelay(delay);
    link->ResetActivationDelay();
    link->SetInBehaviorIO(in);
    link->SetOutBehaviorIO(out);
    script->AddSubBehaviorLink(link);
    return link;
}

inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehavior *outBeh,
                                  int inPos = 0, int outPos = 0, int delay = 0) {
    return CreateLink(script, inBeh->GetOutput(inPos), outBeh->GetInput(outPos), delay);
}

inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehaviorIO *out,
                                  int inPos = 0, int delay = 0) {
    return CreateLink(script, inBeh->GetOutput(inPos), out, delay);
}

inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehavior *outBeh,
                                  int outPos = 0, int delay = 0) {
    return CreateLink(script, in, outBeh->GetInput(outPos), delay);
}

inline CKBehavior *CreateBB(CKBehavior *script, CKGUID guid, bool target = false) {
    auto *behavior = static_cast<CKBehavior *>(script->GetCKContext()->CreateObject(CKCID_BEHAVIOR));
    behavior->InitFromGuid(guid);
    if (target) {
        behavior->UseTarget();
    }
    script->AddSubBehavior(behavior);
    return behavior;
}

inline void InsertBB(CKBehavior *script, CKBehaviorLink *link, CKBehavior *behavior, int inPos = 0, int outPos = 0) {
    CreateLink(script, behavior, link->GetOutBehaviorIO(), outPos);
    link->SetOutBehaviorIO(behavior->GetInput(inPos));
}

inline CKParameterLocal *CreateLocalParameter(CKBehavior *script, const char *name, CKGUID type) {
    return script->CreateLocalParameter((CKSTRING) name, type);
}

template <typename T>
inline void SetParamValue(CKParameter *parameter, T value) {
    parameter->SetValue(&value, sizeof(T));
}

inline void SetParamString(CKParameter *parameter, const char *value) {
    parameter->SetStringValue((CKSTRING) value);
}

inline CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                    int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                    int inputParamCnt = -1, int outputParamCnt = -1) {
    int linkCount = script->GetSubBehaviorLinkCount();
    for (int index = 0; index < linkCount; ++index) {
        CKBehaviorLink *link = script->GetSubBehaviorLink(index);
        CKBehaviorIO *in = link->GetInBehaviorIO();
        if (in != io) {
            continue;
        }

        CKBehaviorIO *out = link->GetOutBehaviorIO();
        CKBehavior *outBehavior = out->GetOwner();
        if ((!name || !strcmp(outBehavior->GetName(), name)) &&
            (outPos < 0 || outBehavior->GetInput(outPos) == out) &&
            (inputCnt < 0 || outBehavior->GetInputCount() == inputCnt) &&
            (outputCnt < 0 || outBehavior->GetOutputCount() == outputCnt) &&
            (inputParamCnt < 0 || outBehavior->GetInputParameterCount() == inputParamCnt) &&
            (outputParamCnt < 0 || outBehavior->GetOutputParameterCount() == outputParamCnt)) {
            return link;
        }
    }

    return nullptr;
}

inline CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehavior *behavior, const char *name = nullptr,
                                    int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                    int inputParamCnt = -1, int outputParamCnt = -1) {
    int linkCount = script->GetSubBehaviorLinkCount();
    for (int index = 0; index < linkCount; ++index) {
        CKBehaviorLink *link = script->GetSubBehaviorLink(index);
        CKBehaviorIO *in = link->GetInBehaviorIO();
        if (in->GetOwner() != behavior || (inPos >= 0 && behavior->GetOutput(inPos) != in)) {
            continue;
        }

        CKBehaviorIO *out = link->GetOutBehaviorIO();
        CKBehavior *outBehavior = out->GetOwner();
        if ((!name || !strcmp(outBehavior->GetName(), name)) &&
            (outPos < 0 || outBehavior->GetInput(outPos) == out) &&
            (inputCnt < 0 || outBehavior->GetInputCount() == inputCnt) &&
            (outputCnt < 0 || outBehavior->GetOutputCount() == outputCnt) &&
            (inputParamCnt < 0 || outBehavior->GetInputParameterCount() == inputParamCnt) &&
            (outputParamCnt < 0 || outBehavior->GetOutputParameterCount() == outputParamCnt)) {
            return link;
        }
    }

    return nullptr;
}

inline CKBehaviorLink *FindPreviousLink(CKBehavior *script, CKBehavior *behavior, const char *name = nullptr,
                                        int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                        int inputParamCnt = -1, int outputParamCnt = -1) {
    int linkCount = script->GetSubBehaviorLinkCount();
    for (int index = 0; index < linkCount; ++index) {
        CKBehaviorLink *link = script->GetSubBehaviorLink(index);
        CKBehaviorIO *out = link->GetOutBehaviorIO();
        if (out->GetOwner() != behavior || (outPos >= 0 && behavior->GetInput(outPos) != out)) {
            continue;
        }

        CKBehaviorIO *in = link->GetInBehaviorIO();
        CKBehavior *inBehavior = in->GetOwner();
        if ((!name || !strcmp(inBehavior->GetName(), name)) &&
            (inPos < 0 || inBehavior->GetOutput(inPos) == in) &&
            (inputCnt < 0 || inBehavior->GetInputCount() == inputCnt) &&
            (outputCnt < 0 || inBehavior->GetOutputCount() == outputCnt) &&
            (inputParamCnt < 0 || inBehavior->GetInputParameterCount() == inputParamCnt) &&
            (outputParamCnt < 0 || inBehavior->GetOutputParameterCount() == outputParamCnt)) {
            return link;
        }
    }

    return nullptr;
}

inline CKBehavior *FindNextBB(CKBehavior *script, CKBehavior *behavior, const char *name = nullptr,
                              int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                              int inputParamCnt = -1, int outputParamCnt = -1) {
    CKBehaviorLink *link = FindNextLink(script, behavior, name, inPos, outPos, inputCnt, outputCnt, inputParamCnt, outputParamCnt);
    return link ? link->GetOutBehaviorIO()->GetOwner() : nullptr;
}

} // namespace ScriptHelper
} // namespace BML_MapMenu

#endif // BML_MAPMENU_SCRIPTHELPER_H