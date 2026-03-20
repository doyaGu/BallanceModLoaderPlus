#ifndef BML_BEHAVIOREDITOR_H
#define BML_BEHAVIOREDITOR_H

#include "BML/BehaviorView.h"
#include "BML/Behavior.h"
#include "CKAll.h"

namespace bml {

// ============================================================================
// BehaviorCallback -- typed enum for CK2 behavior callback messages
// ============================================================================

enum class BehaviorCallback : CKDWORD {
    PreSave            = CKM_BEHAVIORPRESAVE,
    Delete             = CKM_BEHAVIORDELETE,
    Attach             = CKM_BEHAVIORATTACH,
    Detach             = CKM_BEHAVIORDETACH,
    Pause              = CKM_BEHAVIORPAUSE,
    Resume             = CKM_BEHAVIORRESUME,
    Create             = CKM_BEHAVIORCREATE,
    Reset              = CKM_BEHAVIORRESET,
    PostSave           = CKM_BEHAVIORPOSTSAVE,
    Load               = CKM_BEHAVIORLOAD,
    Edited             = CKM_BEHAVIOREDITED,
    SettingsEdited     = CKM_BEHAVIORSETTINGSEDITED,
    ReadState          = CKM_BEHAVIORREADSTATE,
    NewScene           = CKM_BEHAVIORNEWSCENE,
    ActivateScript     = CKM_BEHAVIORACTIVATESCRIPT,
    DeactivateScript   = CKM_BEHAVIORDEACTIVATESCRIPT,
};

// ============================================================================
// BehaviorEditor -- structural and state mutation for a behavior
//
// Non-owning handle. Methods are const (handle-const pattern: the handle is
// immutable, the pointed-to CK object is what gets mutated).
// ============================================================================

class BehaviorEditor {
public:
    explicit BehaviorEditor(CKBehavior *beh = nullptr) : m_Beh(beh) {}

    explicit operator bool() const { return m_Beh != nullptr; }
    CKBehavior *Raw() const { return m_Beh; }
    BehaviorView View() const { return BehaviorView(m_Beh); }

    // -- Flags and metadata --

    void SetFlags(CK_BEHAVIOR_FLAGS flags) const {
        if (m_Beh) m_Beh->SetFlags(flags);
    }

    void ModifyFlags(CKDWORD add, CKDWORD remove) const {
        if (!m_Beh) return;
        auto flags = static_cast<CKDWORD>(m_Beh->GetFlags());
        flags = (flags & ~remove) | add;
        m_Beh->SetFlags(static_cast<CK_BEHAVIOR_FLAGS>(flags));
    }

    void SetPriority(int priority) const {
        if (m_Beh) m_Beh->SetPriority(priority);
    }

    void SetCompatibleClassId(CK_CLASSID cid) const {
        if (m_Beh) m_Beh->SetCompatibleClassID(cid);
    }

    // -- Structure mode --

    void UseGraph() const {
        if (m_Beh) m_Beh->UseGraph();
    }

    void UseFunction() const {
        if (m_Beh) m_Beh->UseFunction();
    }

    // -- Activation and lifecycle --

    void Activate(bool active, bool reset = false) const {
        if (m_Beh) m_Beh->Activate(active ? TRUE : FALSE, reset ? TRUE : FALSE);
    }

    void ActivateInput(int slot, bool active = true) const {
        if (m_Beh) m_Beh->ActivateInput(slot, active ? TRUE : FALSE);
    }

    void ActivateOutput(int slot, bool active = true) const {
        if (m_Beh) m_Beh->ActivateOutput(slot, active ? TRUE : FALSE);
    }

    int SendCallback(CKDWORD msg) const {
        return m_Beh ? m_Beh->CallCallbackFunction(msg) : 0;
    }

    int SendCallback(BehaviorCallback msg) const {
        return SendCallback(static_cast<CKDWORD>(msg));
    }

    void NotifyEdited() const {
        SendCallback(BehaviorCallback::Edited);
    }

    void NotifySettingsEdited() const {
        SendCallback(BehaviorCallback::SettingsEdited);
    }

    // -- IO mutation --

    CKBehaviorIO *AddInput(const char *name) const {
        if (!m_Beh) return nullptr;
        return m_Beh->CreateInput((CKSTRING)name);
    }

    CKBehaviorIO *AddOutput(const char *name) const {
        if (!m_Beh) return nullptr;
        return m_Beh->CreateOutput((CKSTRING)name);
    }

    void DeleteInput(int index) const {
        if (m_Beh) m_Beh->DeleteInput(index);
    }

    void DeleteOutput(int index) const {
        if (m_Beh) m_Beh->DeleteOutput(index);
    }

    // -- Parameter mutation --

    CKParameterIn *AddInputParam(const char *name, CKGUID guid) const {
        if (!m_Beh) return nullptr;
        return m_Beh->CreateInputParameter((CKSTRING)name, guid);
    }

    CKParameterIn *AddInputParam(const char *name, CKGUID guid,
                                 CKParameter *source) const {
        auto *pin = AddInputParam(name, guid);
        if (pin && source) pin->SetDirectSource(source);
        return pin;
    }

    CKParameterOut *AddOutputParam(const char *name, CKGUID guid) const {
        if (!m_Beh) return nullptr;
        return m_Beh->CreateOutputParameter((CKSTRING)name, guid);
    }

    CKParameterLocal *AddLocalParam(const char *name, CKGUID guid) const {
        if (!m_Beh) return nullptr;
        return m_Beh->CreateLocalParameter((CKSTRING)name, guid);
    }

    CKParameterIn *RemoveInputParam(int index) const {
        return m_Beh ? m_Beh->RemoveInputParameter(index) : nullptr;
    }

    CKParameterOut *RemoveOutputParam(int index) const {
        return m_Beh ? m_Beh->RemoveOutputParameter(index) : nullptr;
    }

    CKParameterLocal *RemoveLocalParam(int index) const {
        return m_Beh ? m_Beh->RemoveLocalParameter(index) : nullptr;
    }

    CKERROR ExportInputParam(CKParameterIn *param) const {
        return m_Beh ? m_Beh->ExportInputParameter(param) : CKERR_INVALIDPARAMETER;
    }

    CKERROR ExportOutputParam(CKParameterOut *param) const {
        return m_Beh ? m_Beh->ExportOutputParameter(param) : CKERR_INVALIDPARAMETER;
    }

    // -- Effective value write helpers --
    // For inputs and target: write through the pin's DirectSource when one
    // exists, otherwise fall back to the pin object.
    // For outputs and locals: write directly to the parameter object (these
    // types do not have a DirectSource chain).

    template <typename T>
    void WriteInputValue(int index, T value) const {
        if (!m_Beh) return;
        auto *pin = m_Beh->GetInputParameter(index);
        if (!pin) return;
        auto *src = pin->GetDirectSource();
        if (src) detail::WriteParam(src, value);
    }

    template <typename T>
    void WriteOutputValue(int index, T value) const {
        if (!m_Beh) return;
        auto *param = m_Beh->GetOutputParameter(index);
        if (param) detail::WriteParam(param, value);
    }

    template <typename T>
    void WriteLocalValue(int index, T value) const {
        if (!m_Beh) return;
        auto *param = m_Beh->GetLocalParameter(index);
        if (param) detail::WriteParam(param, value);
    }

    template <typename T>
    void WriteTargetValue(T value) const {
        if (!m_Beh) return;
        auto *pin = m_Beh->GetTargetParameter();
        if (!pin) return;
        auto *src = pin->GetDirectSource();
        if (src) detail::WriteParam(src, value);
    }

    // -- Parameter source wiring --

    void SetInputSource(int index, CKParameter *source) const {
        if (!m_Beh) return;
        auto *pin = m_Beh->GetInputParameter(index);
        if (pin) pin->SetDirectSource(source);
    }

    void SetTargetSource(CKParameter *source) const {
        if (!m_Beh) return;
        auto *pin = m_Beh->GetTargetParameter();
        if (pin) pin->SetDirectSource(source);
    }

    void ShareInput(int index, CKParameterIn *shareWith) const {
        if (!m_Beh || !shareWith) return;
        auto *pin = m_Beh->GetInputParameter(index);
        if (pin) pin->ShareSourceWith(shareWith);
    }

    void AddOutputDestination(int index, CKParameter *dest) const {
        if (!m_Beh || !dest) return;
        auto *param = m_Beh->GetOutputParameter(index);
        if (param) param->AddDestination(dest);
    }

    void SetInputEnabled(int index, bool enabled) const {
        if (!m_Beh) return;
        auto *pin = m_Beh->GetInputParameter(index);
        if (pin) pin->Enable(enabled ? TRUE : FALSE);
    }

    // -- Parameter operation mutation --

    CKParameterOperation *AddOperation(const char *name, CKGUID opGuid,
                                       CKParameter *input1,
                                       CKParameter *input2,
                                       CKParameterOut *output) const {
        if (!m_Beh) return nullptr;
        auto *ctx = m_Beh->GetCKContext();
        if (!ctx) return nullptr;

        // Determine type GUIDs from the provided parameters
        CKGUID resGuid = output ? output->GetGUID() : CKGUID(0, 0);
        CKGUID p1Guid = input1 ? input1->GetGUID() : CKGUID(0, 0);
        CKGUID p2Guid = input2 ? input2->GetGUID() : CKGUID(0, 0);

        auto *op = ctx->CreateCKParameterOperation((CKSTRING)name, opGuid, resGuid, p1Guid, p2Guid);
        if (!op) return nullptr;

        m_Beh->AddParameterOperation(op);

        // Wire inputs
        if (input1) {
            auto *in1 = op->GetInParameter1();
            if (in1) in1->SetDirectSource(input1);
        }
        if (input2) {
            auto *in2 = op->GetInParameter2();
            if (in2) in2->SetDirectSource(input2);
        }
        // Wire output
        if (output) {
            auto *out = op->GetOutParameter();
            if (out) out->AddDestination(output);
        }

        return op;
    }

    void RemoveOperation(CKParameterOperation *op) const {
        if (!m_Beh || !op) return;
        m_Beh->RemoveParameterOperation(op);
        auto *ctx = m_Beh->GetCKContext();
        if (ctx) ctx->DestroyObject(op);
    }

    void EvaluateOperation(CKParameterOperation *op) const {
        if (op) op->DoOperation();
    }

private:
    CKBehavior *m_Beh;
};

// ============================================================================
// Link utility free functions (Layer 4)
// ============================================================================

inline void SetLinkDelay(CKBehaviorLink *link, int delay) {
    if (link) link->SetActivationDelay(delay);
}

inline void SetLinkInitialDelay(CKBehaviorLink *link, int delay) {
    if (link) link->SetInitialActivationDelay(delay);
}

inline void ResetLinkDelay(CKBehaviorLink *link) {
    if (link) link->ResetActivationDelay();
}

// ============================================================================
// Message wait free functions (Layer 5)
// ============================================================================

inline CKERROR RegisterMessageWait(CKMessageManager *mm,
                                   CKMessageType type,
                                   CKBehavior *behavior,
                                   int outputToActivate,
                                   CKBeObject *owner) {
    if (!mm || !behavior) return CKERR_INVALIDPARAMETER;
    return mm->RegisterWait(type, behavior, outputToActivate, owner);
}

inline CKERROR UnregisterMessageWait(CKMessageManager *mm,
                                     CKMessageType type,
                                     CKBehavior *behavior,
                                     int outputToActivate) {
    if (!mm || !behavior) return CKERR_INVALIDPARAMETER;
    return mm->UnRegisterWait(type, behavior, outputToActivate);
}

} // namespace bml

#endif // BML_BEHAVIOREDITOR_H
