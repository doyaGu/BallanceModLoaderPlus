#ifndef BML_BEHAVIORVIEW_H
#define BML_BEHAVIORVIEW_H

#include <cstring>
#include <type_traits>

#include "CKAll.h"

namespace bml {

// Forward declarations
class BehaviorView;
class PrototypeView;
class IoView;
class LinkView;
class ParamView;
class InputParamView;
class OutputParamView;
class LocalParamView;
class OperationView;

// ============================================================================
// Descriptor structs for prototype reflection
// ============================================================================

struct ParamDesc {
    const char *Name = nullptr;
    CKGUID TypeGuid = CKGUID(0, 0);

    ParamDesc() : Name(nullptr), TypeGuid(0, 0) {}
    ParamDesc(const char *n, CKGUID g) : Name(n), TypeGuid(g) {}
};

struct IoDesc {
    const char *Name = nullptr;

    IoDesc() = default;
    explicit IoDesc(const char *n) : Name(n) {}
};

// ============================================================================
// IoView
// ============================================================================

class IoView {
public:
    explicit IoView(CKBehaviorIO *io = nullptr) : m_IO(io) {}

    explicit operator bool() const { return m_IO != nullptr; }
    CKBehaviorIO *Raw() const { return m_IO; }

    const char *Name() const { return m_IO ? m_IO->GetName() : nullptr; }

    bool IsInput() const {
        return m_IO && m_IO->GetType() == CK_BEHAVIORIO_IN;
    }

    bool IsOutput() const {
        return m_IO && m_IO->GetType() == CK_BEHAVIORIO_OUT;
    }

    bool IsActive() const {
        return m_IO && m_IO->IsActive();
    }

    CKBehavior *Owner() const {
        return m_IO ? m_IO->GetOwner() : nullptr;
    }

private:
    CKBehaviorIO *m_IO;
};

// ============================================================================
// LinkView
// ============================================================================

class LinkView {
public:
    explicit LinkView(CKBehaviorLink *link = nullptr) : m_Link(link) {}

    explicit operator bool() const { return m_Link != nullptr; }
    CKBehaviorLink *Raw() const { return m_Link; }

    IoView Source() const {
        return IoView(m_Link ? m_Link->GetInBehaviorIO() : nullptr);
    }

    IoView Destination() const {
        return IoView(m_Link ? m_Link->GetOutBehaviorIO() : nullptr);
    }

    int ActivationDelay() const {
        return m_Link ? m_Link->GetActivationDelay() : 0;
    }

    int InitialActivationDelay() const {
        return m_Link ? m_Link->GetInitialActivationDelay() : 0;
    }

private:
    CKBehaviorLink *m_Link;
};

// ============================================================================
// ParamView -- wraps CKParameter* (base for CKParameterOut, CKParameterLocal)
//
// Note: CKParameterIn does NOT derive from CKParameter in the CK2 SDK,
// so InputParamView is a standalone type rather than a ParamView subclass.
// ============================================================================

class ParamView {
public:
    explicit ParamView(CKParameter *param = nullptr) : m_Param(param) {}

    explicit operator bool() const { return m_Param != nullptr; }
    CKParameter *Raw() const { return m_Param; }

    const char *Name() const { return m_Param ? m_Param->GetName() : nullptr; }

    CKGUID TypeGuid() const {
        return m_Param ? m_Param->GetGUID() : CKGUID(0, 0);
    }

    CKParameterType Type() const {
        return m_Param ? m_Param->GetType() : -1;
    }

    int DataSize() const {
        return m_Param ? m_Param->GetDataSize() : 0;
    }

    template <typename T>
    T Read() const {
        T val{};
        if (m_Param) m_Param->GetValue(&val);
        return val;
    }

    CKObject *ReadObject() const {
        return m_Param ? m_Param->GetValueObject() : nullptr;
    }

    const char *ReadString() const {
        return m_Param ? static_cast<const char *>(m_Param->GetReadDataPtr()) : nullptr;
    }

private:
    CKParameter *m_Param;
};

// ============================================================================
// InputParamView -- wraps CKParameterIn* (standalone, not derived from ParamView)
//
// CKParameterIn inherits from CKObject, not CKParameter. It has its own
// GetGUID(), GetType(), GetValue(), GetReadDataPtr() that read through the
// source chain. This type mirrors the ParamView interface where applicable.
// ============================================================================

class InputParamView {
public:
    explicit InputParamView(CKParameterIn *param = nullptr) : m_Pin(param) {}

    explicit operator bool() const { return m_Pin != nullptr; }
    CKParameterIn *Raw() const { return m_Pin; }

    const char *Name() const { return m_Pin ? m_Pin->GetName() : nullptr; }

    CKGUID TypeGuid() const {
        return m_Pin ? m_Pin->GetGUID() : CKGUID(0, 0);
    }

    CKParameterType Type() const {
        return m_Pin ? m_Pin->GetType() : -1;
    }

    int DataSize() const {
        if (!m_Pin) return 0;
        CKParameter *src = m_Pin->GetDirectSource();
        return src ? src->GetDataSize() : 0;
    }

    template <typename T>
    T Read() const {
        T val{};
        if (m_Pin) m_Pin->GetValue(&val);
        return val;
    }

    CKObject *ReadObject() const {
        if (!m_Pin) return nullptr;
        CKParameter *src = m_Pin->GetRealSource();
        return src ? src->GetValueObject() : nullptr;
    }

    const char *ReadString() const {
        return m_Pin ? static_cast<const char *>(m_Pin->GetReadDataPtr()) : nullptr;
    }

    CKObject *Owner() const {
        return m_Pin ? m_Pin->GetOwner() : nullptr;
    }

    bool IsEnabled() const {
        return m_Pin && m_Pin->IsEnabled();
    }

    ParamView DirectSource() const {
        return ParamView(m_Pin ? m_Pin->GetDirectSource() : nullptr);
    }

    ParamView RealSource() const {
        return ParamView(m_Pin ? m_Pin->GetRealSource() : nullptr);
    }

    InputParamView SharedSource() const {
        return InputParamView(m_Pin ? m_Pin->GetSharedSource() : nullptr);
    }

private:
    CKParameterIn *m_Pin;
};

// ============================================================================
// OutputParamView
// ============================================================================

class OutputParamView : public ParamView {
public:
    explicit OutputParamView(CKParameterOut *param = nullptr)
        : ParamView(param), m_Out(param) {}

    CKParameterOut *Raw() const { return m_Out; }

    int DestinationCount() const {
        return m_Out ? m_Out->GetDestinationCount() : 0;
    }

    ParamView Destination(int index) const {
        return ParamView(m_Out ? m_Out->GetDestination(index) : nullptr);
    }

    template <typename Callback>
    bool ForEachDestination(Callback &&cb) const {
        static_assert(std::is_invocable_r_v<bool, Callback, ParamView>,
                      "Callback must be callable as bool(ParamView)");
        if (!m_Out) return true;
        int cnt = m_Out->GetDestinationCount();
        for (int i = 0; i < cnt; i++) {
            if (!cb(ParamView(m_Out->GetDestination(i))))
                return false;
        }
        return true;
    }

private:
    CKParameterOut *m_Out;
};

// ============================================================================
// LocalParamView
// ============================================================================

class LocalParamView : public ParamView {
public:
    explicit LocalParamView(CKParameterLocal *param = nullptr,
                            CKBehavior *owner = nullptr, int index = -1)
        : ParamView(param), m_Local(param), m_Owner(owner), m_Index(index) {}

    CKParameterLocal *Raw() const { return m_Local; }

    bool IsSetting() const {
        return m_Owner && m_Index >= 0 && m_Owner->IsLocalParameterSetting(m_Index);
    }

private:
    CKParameterLocal *m_Local;
    CKBehavior *m_Owner;
    int m_Index;
};

// ============================================================================
// OperationView
// ============================================================================

class OperationView {
public:
    explicit OperationView(CKParameterOperation *op = nullptr) : m_Op(op) {}

    explicit operator bool() const { return m_Op != nullptr; }
    CKParameterOperation *Raw() const { return m_Op; }

    const char *Name() const { return m_Op ? m_Op->GetName() : nullptr; }

    CKGUID OperationGuid() const {
        return m_Op ? m_Op->GetOperationGuid() : CKGUID(0, 0);
    }

    CKBehavior *Owner() const {
        return m_Op ? m_Op->GetOwner() : nullptr;
    }

    InputParamView Input1() const {
        return InputParamView(m_Op ? m_Op->GetInParameter1() : nullptr);
    }

    InputParamView Input2() const {
        return InputParamView(m_Op ? m_Op->GetInParameter2() : nullptr);
    }

    OutputParamView Output() const {
        return OutputParamView(m_Op ? m_Op->GetOutParameter() : nullptr);
    }

private:
    CKParameterOperation *m_Op;
};

// ============================================================================
// PrototypeView
// ============================================================================

class PrototypeView {
public:
    explicit PrototypeView(CKBehaviorPrototype *proto = nullptr) : m_Proto(proto) {}

    explicit operator bool() const { return m_Proto != nullptr; }
    CKBehaviorPrototype *Raw() const { return m_Proto; }

    const char *Name() const { return m_Proto ? m_Proto->GetName() : nullptr; }

    CKGUID Guid() const {
        return m_Proto ? m_Proto->GetGuid() : CKGUID(0, 0);
    }

    CK_BEHAVIORPROTOTYPE_FLAGS Flags() const {
        return m_Proto ? m_Proto->GetFlags() : static_cast<CK_BEHAVIORPROTOTYPE_FLAGS>(0);
    }

    CK_BEHAVIOR_FLAGS BehaviorFlags() const {
        return m_Proto ? m_Proto->GetBehaviorFlags() : static_cast<CK_BEHAVIOR_FLAGS>(0);
    }

    CK_CLASSID ApplyToClassId() const {
        return m_Proto ? m_Proto->GetApplyToClassID() : 0;
    }

    int InputCount() const { return m_Proto ? m_Proto->GetInputCount() : 0; }
    int OutputCount() const { return m_Proto ? m_Proto->GetOutputCount() : 0; }
    int InputParamCount() const { return m_Proto ? m_Proto->GetInParameterCount() : 0; }
    int OutputParamCount() const { return m_Proto ? m_Proto->GetOutParameterCount() : 0; }
    int LocalParamCount() const { return m_Proto ? m_Proto->GetLocalParameterCount() : 0; }

    IoDesc Input(int index) const {
        if (!m_Proto) return IoDesc();
        auto **list = m_Proto->GetInIOList();
        if (!list || index < 0 || index >= m_Proto->GetInputCount()) return IoDesc();
        return IoDesc(list[index] ? list[index]->Name : nullptr);
    }

    IoDesc Output(int index) const {
        if (!m_Proto) return IoDesc();
        auto **list = m_Proto->GetOutIOList();
        if (!list || index < 0 || index >= m_Proto->GetOutputCount()) return IoDesc();
        return IoDesc(list[index] ? list[index]->Name : nullptr);
    }

    ParamDesc InputParam(int index) const {
        if (!m_Proto) return ParamDesc();
        auto **list = m_Proto->GetInParameterList();
        if (!list || index < 0 || index >= m_Proto->GetInParameterCount()) return ParamDesc();
        return list[index] ? ParamDesc(list[index]->Name, list[index]->Guid) : ParamDesc();
    }

    ParamDesc OutputParam(int index) const {
        if (!m_Proto) return ParamDesc();
        auto **list = m_Proto->GetOutParameterList();
        if (!list || index < 0 || index >= m_Proto->GetOutParameterCount()) return ParamDesc();
        return list[index] ? ParamDesc(list[index]->Name, list[index]->Guid) : ParamDesc();
    }

    ParamDesc LocalParam(int index) const {
        if (!m_Proto) return ParamDesc();
        auto **list = m_Proto->GetLocalParameterList();
        if (!list || index < 0 || index >= m_Proto->GetLocalParameterCount()) return ParamDesc();
        return list[index] ? ParamDesc(list[index]->Name, list[index]->Guid) : ParamDesc();
    }

    int FindInput(const char *name) const {
        return (m_Proto && name) ? m_Proto->GetInIOIndex((CKSTRING)name) : -1;
    }

    int FindOutput(const char *name) const {
        return (m_Proto && name) ? m_Proto->GetOutIOIndex((CKSTRING)name) : -1;
    }

    int FindInputParam(const char *name) const {
        return (m_Proto && name) ? m_Proto->GetInParamIndex((CKSTRING)name) : -1;
    }

    int FindOutputParam(const char *name) const {
        return (m_Proto && name) ? m_Proto->GetOutParamIndex((CKSTRING)name) : -1;
    }

    int FindLocalParam(const char *name) const {
        return (m_Proto && name) ? m_Proto->GetLocalParamIndex((CKSTRING)name) : -1;
    }

private:
    CKBehaviorPrototype *m_Proto;
};

// ============================================================================
// BehaviorView
// ============================================================================

class BehaviorView {
public:
    explicit BehaviorView(CKBehavior *beh = nullptr) : m_Beh(beh) {}

    explicit operator bool() const { return m_Beh != nullptr; }
    CKBehavior *Raw() const { return m_Beh; }

    // Identity
    const char *Name() const { return m_Beh ? m_Beh->GetName() : nullptr; }

    CKGUID PrototypeGuid() const {
        return m_Beh ? m_Beh->GetPrototypeGuid() : CKGUID(0, 0);
    }

    const char *PrototypeName() const {
        return m_Beh ? m_Beh->GetPrototypeName() : nullptr;
    }

    PrototypeView Prototype() const {
        return PrototypeView(m_Beh ? m_Beh->GetPrototype() : nullptr);
    }

    // Metadata
    CK_BEHAVIOR_TYPE Type() const {
        return m_Beh ? m_Beh->GetType() : static_cast<CK_BEHAVIOR_TYPE>(0);
    }

    CK_BEHAVIOR_FLAGS Flags() const {
        return m_Beh ? m_Beh->GetFlags() : static_cast<CK_BEHAVIOR_FLAGS>(0);
    }

    int Priority() const {
        return m_Beh ? m_Beh->GetPriority() : 0;
    }

    CKDWORD Version() const {
        return m_Beh ? m_Beh->GetVersion() : 0;
    }

    CK_CLASSID CompatibleClassId() const {
        return m_Beh ? m_Beh->GetCompatibleClassID() : 0;
    }

    // State
    bool IsActive() const { return m_Beh && m_Beh->IsActive(); }
    bool IsUsingFunction() const { return m_Beh && m_Beh->IsUsingFunction(); }
    bool IsUsingTarget() const { return m_Beh && m_Beh->IsUsingTarget(); }

    bool IsTargetable() const {
        return m_Beh && m_Beh->GetTargetParameter() != nullptr;
    }

    // Hierarchy
    CKBeObject *Owner() const { return m_Beh ? m_Beh->GetOwner() : nullptr; }
    CKBehavior *Parent() const { return m_Beh ? m_Beh->GetParent() : nullptr; }
    CKBehavior *Script() const { return m_Beh ? m_Beh->GetOwnerScript() : nullptr; }

    // Counts
    int InputCount() const { return m_Beh ? m_Beh->GetInputCount() : 0; }
    int OutputCount() const { return m_Beh ? m_Beh->GetOutputCount() : 0; }
    int InputParamCount() const { return m_Beh ? m_Beh->GetInputParameterCount() : 0; }
    int OutputParamCount() const { return m_Beh ? m_Beh->GetOutputParameterCount() : 0; }
    int LocalParamCount() const { return m_Beh ? m_Beh->GetLocalParameterCount() : 0; }
    int OperationCount() const { return m_Beh ? m_Beh->GetParameterOperationCount() : 0; }
    int SubBehaviorCount() const { return m_Beh ? m_Beh->GetSubBehaviorCount() : 0; }

    // Indexed access
    IoView Input(int index) const {
        return IoView(m_Beh ? m_Beh->GetInput(index) : nullptr);
    }

    IoView Output(int index) const {
        return IoView(m_Beh ? m_Beh->GetOutput(index) : nullptr);
    }

    InputParamView InputParam(int index) const {
        return InputParamView(m_Beh ? m_Beh->GetInputParameter(index) : nullptr);
    }

    OutputParamView OutputParam(int index) const {
        return OutputParamView(m_Beh ? m_Beh->GetOutputParameter(index) : nullptr);
    }

    LocalParamView LocalParam(int index) const {
        return LocalParamView(
            m_Beh ? m_Beh->GetLocalParameter(index) : nullptr,
            m_Beh, index);
    }

    InputParamView TargetParam() const {
        return InputParamView(m_Beh ? m_Beh->GetTargetParameter() : nullptr);
    }

    OperationView Operation(int index) const {
        return OperationView(m_Beh ? m_Beh->GetParameterOperation(index) : nullptr);
    }

    // Name-based lookup (returns -1 on not found)
    int FindInput(const char *name) const {
        if (!m_Beh || !name) return -1;
        int cnt = m_Beh->GetInputCount();
        for (int i = 0; i < cnt; i++) {
            CKBehaviorIO *io = m_Beh->GetInput(i);
            if (io && NameEq(io->GetName(), name))
                return i;
        }
        return -1;
    }

    int FindOutput(const char *name) const {
        if (!m_Beh || !name) return -1;
        int cnt = m_Beh->GetOutputCount();
        for (int i = 0; i < cnt; i++) {
            CKBehaviorIO *io = m_Beh->GetOutput(i);
            if (io && NameEq(io->GetName(), name))
                return i;
        }
        return -1;
    }

    int FindInputParam(const char *name) const {
        if (!m_Beh || !name) return -1;
        int cnt = m_Beh->GetInputParameterCount();
        for (int i = 0; i < cnt; i++) {
            CKParameterIn *pin = m_Beh->GetInputParameter(i);
            if (pin && NameEq(pin->GetName(), name))
                return i;
        }
        return -1;
    }

    int FindOutputParam(const char *name) const {
        if (!m_Beh || !name) return -1;
        int cnt = m_Beh->GetOutputParameterCount();
        for (int i = 0; i < cnt; i++) {
            CKParameterOut *pout = m_Beh->GetOutputParameter(i);
            if (pout && NameEq(pout->GetName(), name))
                return i;
        }
        return -1;
    }

    int FindLocalParam(const char *name) const {
        if (!m_Beh || !name) return -1;
        int cnt = m_Beh->GetLocalParameterCount();
        for (int i = 0; i < cnt; i++) {
            CKParameterLocal *local = m_Beh->GetLocalParameter(i);
            if (local && NameEq(local->GetName(), name))
                return i;
        }
        return -1;
    }

    // Callback enumeration (flat, direct children only).
    // For hierarchical or name-filtered search, use Graph::FindAll.
    template <typename Callback>
    bool ForEachSubBehavior(Callback &&cb) const {
        static_assert(std::is_invocable_r_v<bool, Callback, BehaviorView>,
                      "Callback must be callable as bool(BehaviorView)");
        if (!m_Beh) return true;
        int cnt = m_Beh->GetSubBehaviorCount();
        for (int i = 0; i < cnt; i++) {
            if (!cb(BehaviorView(m_Beh->GetSubBehavior(i))))
                return false;
        }
        return true;
    }

    template <typename Callback>
    bool ForEachOperation(Callback &&cb) const {
        static_assert(std::is_invocable_r_v<bool, Callback, OperationView>,
                      "Callback must be callable as bool(OperationView)");
        if (!m_Beh) return true;
        int cnt = m_Beh->GetParameterOperationCount();
        for (int i = 0; i < cnt; i++) {
            if (!cb(OperationView(m_Beh->GetParameterOperation(i))))
                return false;
        }
        return true;
    }

private:
    static bool NameEq(const char *a, const char *b) {
        return a && b && std::strcmp(a, b) == 0;
    }

    CKBehavior *m_Beh;
};

} // namespace bml

#endif // BML_BEHAVIORVIEW_H
