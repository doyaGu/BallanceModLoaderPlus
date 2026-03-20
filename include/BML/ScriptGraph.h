#ifndef BML_SCRIPTGRAPH_H
#define BML_SCRIPTGRAPH_H

#include <cstring>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "BML/Behavior.h"
#include "BML/BehaviorEditor.h"
#include "BML/BehaviorView.h"
#include "CKAll.h"

namespace bml {

class Graph;
class Node;

// --- Parameter helpers (graph-independent) ---
// detail::WriteParam, SetParam, GetParam are defined in Behavior.h

inline CKObject *GetParamObject(CKParameter *param) {
    return param ? param->GetValueObject() : nullptr;
}

inline const char *GetParamString(CKParameter *param) {
    return param ? static_cast<const char *>(param->GetReadDataPtr()) : nullptr;
}

namespace detail {

inline bool NameEquals(const char *lhs, const char *rhs) {
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

inline bool MatchBehavior(CKBehavior *beh, const char *name,
                          int inputCnt, int outputCnt,
                          int inputParamCnt, int outputParamCnt) {
    if (!beh) {
        return false;
    }

    return (!name || NameEquals(beh->GetName(), name))
        && (inputCnt < 0 || beh->GetInputCount() == inputCnt)
        && (outputCnt < 0 || beh->GetOutputCount() == outputCnt)
        && (inputParamCnt < 0 || beh->GetInputParameterCount() == inputParamCnt)
        && (outputParamCnt < 0 || beh->GetOutputParameterCount() == outputParamCnt);
}

inline bool FindBehaviorImpl(CKBehavior *script,
                             const std::function<bool(CKBehavior *)> &callback,
                             const char *name, bool hierarchically,
                             int inputCnt, int outputCnt,
                             int inputParamCnt, int outputParamCnt) {
    int cnt = script->GetSubBehaviorCount();
    for (int i = 0; i < cnt; i++) {
        CKBehavior *beh = script->GetSubBehavior(i);
        if (hierarchically && beh->GetSubBehaviorCount() > 0) {
            if (!FindBehaviorImpl(beh, callback, name, hierarchically,
                                  inputCnt, outputCnt, inputParamCnt, outputParamCnt))
                return false;
        }
        if (MatchBehavior(beh, name, inputCnt, outputCnt, inputParamCnt, outputParamCnt))
            if (!callback(beh))
                return false;
    }
    return true;
}

inline CKBehavior *FindFirstBehaviorImpl(CKBehavior *script, const char *name,
                                         bool hierarchically,
                                         int inputCnt, int outputCnt,
                                         int inputParamCnt, int outputParamCnt) {
    CKBehavior *res = nullptr;
    FindBehaviorImpl(script, [&res](CKBehavior *beh) {
        res = beh;
        return false;
    }, name, hierarchically, inputCnt, outputCnt, inputParamCnt, outputParamCnt);
    return res;
}

inline CKBehavior *FindBehaviorByTargetImpl(CKBehavior *script, const char *name,
                                            const char *targetName) {
    if (!script || !name || !targetName) {
        return nullptr;
    }

    int cnt = script->GetSubBehaviorCount();
    for (int i = 0; i < cnt; i++) {
        CKBehavior *beh = script->GetSubBehavior(i);
        if (NameEquals(beh->GetName(), name)) {
            CKObject *target = beh->GetTarget();
            if (target && NameEquals(target->GetName(), targetName)) {
                return beh;
            }
        }
    }
    return nullptr;
}

inline CKBehaviorLink *FindNextLinkFromBeh(CKBehavior *script, CKBehavior *beh,
                                           const char *name, int inPos, int outPos,
                                           int inputCnt, int outputCnt,
                                           int inputParamCnt, int outputParamCnt) {
    int linkCnt = script->GetSubBehaviorLinkCount();
    for (int i = 0; i < linkCnt; i++) {
        CKBehaviorLink *link = script->GetSubBehaviorLink(i);
        CKBehaviorIO *in = link->GetInBehaviorIO();
        if (in->GetOwner() == beh && (inPos < 0 || beh->GetOutput(inPos) == in)) {
            CKBehaviorIO *out = link->GetOutBehaviorIO();
            CKBehavior *outBeh = out->GetOwner();
            if (MatchBehavior(outBeh, name, inputCnt, outputCnt, inputParamCnt, outputParamCnt)
                && (outPos < 0 || outBeh->GetInput(outPos) == out))
                return link;
        }
    }
    return nullptr;
}

inline CKBehaviorLink *FindNextLinkFromIO(CKBehavior *script, CKBehaviorIO *io,
                                          const char *name, int outPos,
                                          int inputCnt, int outputCnt,
                                          int inputParamCnt, int outputParamCnt) {
    int linkCnt = script->GetSubBehaviorLinkCount();
    for (int i = 0; i < linkCnt; i++) {
        CKBehaviorLink *link = script->GetSubBehaviorLink(i);
        if (link->GetInBehaviorIO() == io) {
            CKBehaviorIO *out = link->GetOutBehaviorIO();
            CKBehavior *outBeh = out->GetOwner();
            if (MatchBehavior(outBeh, name, inputCnt, outputCnt, inputParamCnt, outputParamCnt)
                && (outPos < 0 || outBeh->GetInput(outPos) == out))
                return link;
        }
    }
    return nullptr;
}

inline CKBehaviorLink *FindPrevLinkFromBeh(CKBehavior *script, CKBehavior *beh,
                                           const char *name, int inPos, int outPos,
                                           int inputCnt, int outputCnt,
                                           int inputParamCnt, int outputParamCnt) {
    int linkCnt = script->GetSubBehaviorLinkCount();
    for (int i = 0; i < linkCnt; i++) {
        CKBehaviorLink *link = script->GetSubBehaviorLink(i);
        CKBehaviorIO *out = link->GetOutBehaviorIO();
        if (out->GetOwner() == beh && (outPos < 0 || beh->GetInput(outPos) == out)) {
            CKBehaviorIO *in = link->GetInBehaviorIO();
            CKBehavior *inBeh = in->GetOwner();
            if (MatchBehavior(inBeh, name, inputCnt, outputCnt, inputParamCnt, outputParamCnt)
                && (inPos < 0 || inBeh->GetOutput(inPos) == in))
                return link;
        }
    }
    return nullptr;
}

inline CKBehaviorLink *FindPrevLinkFromIO(CKBehavior *script, CKBehaviorIO *io,
                                          const char *name, int inPos,
                                          int inputCnt, int outputCnt,
                                          int inputParamCnt, int outputParamCnt) {
    int linkCnt = script->GetSubBehaviorLinkCount();
    for (int i = 0; i < linkCnt; i++) {
        CKBehaviorLink *link = script->GetSubBehaviorLink(i);
        if (link->GetOutBehaviorIO() == io) {
            CKBehaviorIO *in = link->GetInBehaviorIO();
            CKBehavior *inBeh = in->GetOwner();
            if (MatchBehavior(inBeh, name, inputCnt, outputCnt, inputParamCnt, outputParamCnt)
                && (inPos < 0 || inBeh->GetOutput(inPos) == in))
                return link;
        }
    }
    return nullptr;
}

template <typename Callback>
inline bool FindBehaviorByGuidImpl(CKBehavior *script, CKGUID guid,
                                   bool hierarchically, Callback &&cb) {
    int cnt = script->GetSubBehaviorCount();
    for (int i = 0; i < cnt; i++) {
        CKBehavior *beh = script->GetSubBehavior(i);
        if (!beh) continue;
        if (hierarchically && beh->GetSubBehaviorCount() > 0) {
            if (!FindBehaviorByGuidImpl(beh, guid, hierarchically, cb))
                return false;
        }
        if (beh->GetPrototypeGuid() == guid) {
            if (!cb(beh))
                return false;
        }
    }
    return true;
}

inline CKBehavior *FindFirstBehaviorByGuidImpl(CKBehavior *script, CKGUID guid,
                                               bool hierarchically) {
    CKBehavior *res = nullptr;
    FindBehaviorByGuidImpl(script, guid, hierarchically, [&res](CKBehavior *beh) {
        res = beh;
        return false;
    });
    return res;
}

} // namespace detail

// ============================================================================
// Node - lightweight traversal handle with null propagation
// ============================================================================

class Node {
public:
    Node() = default;
    Node(CKBehavior *container, CKBehavior *beh)
        : m_Container(container), m_Beh(beh) {}
    Node(CKBehavior *container, CKBehaviorIO *io)
        : m_Container(container), m_IO(io) {}

    explicit operator bool() const { return m_Beh != nullptr || m_IO != nullptr; }
    CKBehavior *Get() const { return m_Beh; }
    CKBehaviorIO *GetIO() const { return m_IO; }
    operator CKBehavior *() const { return m_Beh; }
    CKBehavior *operator->() const { return m_Beh; }

    // -- IO pin access --

    Node Out(int slot) const {
        if (!m_Beh) return {};
        return {m_Container, m_Beh->GetOutput(slot)};
    }

    Node In(int slot) const {
        if (!m_Beh) return {};
        return {m_Container, m_Beh->GetInput(slot)};
    }

    // -- Link traversal --

    CKBehaviorLink *NextLink(const char *name = nullptr, int inPos = -1, int outPos = -1,
                             int inputCnt = -1, int outputCnt = -1,
                             int inputParamCnt = -1, int outputParamCnt = -1) const {
        if (!m_Container) return nullptr;
        if (m_IO)
            return detail::FindNextLinkFromIO(m_Container, m_IO, name, outPos,
                                              inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        if (m_Beh)
            return detail::FindNextLinkFromBeh(m_Container, m_Beh, name, inPos, outPos,
                                               inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        return nullptr;
    }

    CKBehaviorLink *PrevLink(const char *name = nullptr, int inPos = -1, int outPos = -1,
                             int inputCnt = -1, int outputCnt = -1,
                             int inputParamCnt = -1, int outputParamCnt = -1) const {
        if (!m_Container) return nullptr;
        if (m_IO)
            return detail::FindPrevLinkFromIO(m_Container, m_IO, name, inPos,
                                              inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        if (m_Beh)
            return detail::FindPrevLinkFromBeh(m_Container, m_Beh, name, inPos, outPos,
                                               inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        return nullptr;
    }

    Node Next(const char *name = nullptr, int inPos = -1, int outPos = -1,
              int inputCnt = -1, int outputCnt = -1,
              int inputParamCnt = -1, int outputParamCnt = -1) const {
        CKBehaviorLink *link = NextLink(name, inPos, outPos, inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        if (!link) return {};
        return {m_Container, link->GetOutBehaviorIO()->GetOwner()};
    }

    Node Prev(const char *name = nullptr, int inPos = -1, int outPos = -1,
              int inputCnt = -1, int outputCnt = -1,
              int inputParamCnt = -1, int outputParamCnt = -1) const {
        CKBehaviorLink *link = PrevLink(name, inPos, outPos, inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        if (!link) return {};
        return {m_Container, link->GetInBehaviorIO()->GetOwner()};
    }

    Node Skip(int n) const {
        Node cur = *this;
        for (int i = 0; i < n && cur; i++)
            cur = cur.Next();
        return cur;
    }

    Node SkipBack(int n) const {
        Node cur = *this;
        for (int i = 0; i < n && cur; i++)
            cur = cur.Prev();
        return cur;
    }

    Node End() const {
        if (!m_Container) return {};
        CKBehavior *beh = m_Beh;
        if (!beh && m_IO) {
            CKBehaviorLink *first = detail::FindNextLinkFromIO(
                m_Container, m_IO, nullptr, -1, -1, -1, -1, -1);
            if (!first) return {};
            auto *out = first->GetOutBehaviorIO();
            if (!out) return {};
            beh = out->GetOwner();
        }
        if (!beh) return {};

        std::unordered_set<CKBehavior *> visited;
        visited.insert(beh);

        while (true) {
            CKBehaviorLink *link = detail::FindNextLinkFromBeh(
                m_Container, beh, nullptr, -1, -1, -1, -1, -1, -1);
            if (!link) break;
            auto *out = link->GetOutBehaviorIO();
            if (!out) break;

            CKBehavior *next = out->GetOwner();
            if (!next || visited.find(next) != visited.end()) {
                break;
            }

            visited.insert(next);
            beh = next;
        }
        return {m_Container, beh};
    }

    // -- Sub-behavior search (shifts container to this node's behavior) --

    Node Find(const char *name, bool hierarchically = false,
              int inputCnt = -1, int outputCnt = -1,
              int inputParamCnt = -1, int outputParamCnt = -1) const {
        if (!m_Beh) return {};
        CKBehavior *found = detail::FindFirstBehaviorImpl(
            m_Beh, name, hierarchically, inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        return {m_Beh, found};
    }

    Node FindByTarget(const char *name, const char *targetName) const {
        if (!m_Beh || !name || !targetName) return {};
        CKBehavior *found = detail::FindBehaviorByTargetImpl(m_Beh, name, targetName);
        return {m_Beh, found};
    }

    bool FindAll(const std::function<bool(CKBehavior *)> &callback,
                 const char *name = nullptr, bool hierarchically = false,
                 int inputCnt = -1, int outputCnt = -1,
                 int inputParamCnt = -1, int outputParamCnt = -1) const {
        if (!m_Beh) return true;
        return detail::FindBehaviorImpl(m_Beh, callback, name, hierarchically,
                                        inputCnt, outputCnt, inputParamCnt, outputParamCnt);
    }

    Node FindByGuid(CKGUID prototypeGuid, bool hierarchically = false) const {
        if (!m_Beh) return {};
        CKBehavior *found = detail::FindFirstBehaviorByGuidImpl(
            m_Beh, prototypeGuid, hierarchically);
        return {m_Beh, found};
    }

    template <typename Callback>
    bool FindAllByGuid(CKGUID prototypeGuid, Callback &&cb,
                       bool hierarchically = false) const {
        static_assert(std::is_invocable_r_v<bool, Callback, CKBehavior *>,
                      "Callback must be callable as bool(CKBehavior*)");
        if (!m_Beh) return true;
        return detail::FindBehaviorByGuidImpl(m_Beh, prototypeGuid, hierarchically, cb);
    }

    Graph AsGraph() const;

    // View/Editor access (see BehaviorView.h, BehaviorEditor.h).
    // Returns a null handle when the Node is in IO-mode (m_Beh == nullptr).
    BehaviorView View() const { return BehaviorView(m_Beh); }
    BehaviorEditor Edit() const { return BehaviorEditor(m_Beh); }

private:
    CKBehavior *m_Container = nullptr;
    CKBehavior *m_Beh = nullptr;
    CKBehaviorIO *m_IO = nullptr;
};

// ============================================================================
// Graph - script context wrapper for manipulation
// ============================================================================

class Graph {
public:
    explicit Graph(CKBehavior *script) : m_Script(script) {}
    explicit Graph(Node node) : m_Script(node.Get()) {}

    CKBehavior *Raw() const { return m_Script; }
    CKContext *Context() const { return m_Script ? m_Script->GetCKContext() : nullptr; }
    operator CKBehavior *() const { return m_Script; }

    // -- Search --

    Node Find(const char *name, bool hierarchically = false,
              int inputCnt = -1, int outputCnt = -1,
              int inputParamCnt = -1, int outputParamCnt = -1) const {
        if (!m_Script) return {};
        CKBehavior *found = detail::FindFirstBehaviorImpl(
            m_Script, name, hierarchically, inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        return {m_Script, found};
    }

    Node FindByTarget(const char *name, const char *targetName) const {
        if (!m_Script || !name || !targetName) return {};
        CKBehavior *found = detail::FindBehaviorByTargetImpl(m_Script, name, targetName);
        return {m_Script, found};
    }

    bool FindAll(const std::function<bool(CKBehavior *)> &callback,
                 const char *name = nullptr, bool hierarchically = false,
                 int inputCnt = -1, int outputCnt = -1,
                 int inputParamCnt = -1, int outputParamCnt = -1) const {
        if (!m_Script) return true;
        return detail::FindBehaviorImpl(m_Script, callback, name, hierarchically,
                                        inputCnt, outputCnt, inputParamCnt, outputParamCnt);
    }

    // -- GUID-based search --

    Node FindByGuid(CKGUID prototypeGuid, bool hierarchically = false) const {
        if (!m_Script) return {};
        CKBehavior *found = detail::FindFirstBehaviorByGuidImpl(
            m_Script, prototypeGuid, hierarchically);
        return {m_Script, found};
    }

    template <typename Callback>
    bool FindAllByGuid(CKGUID prototypeGuid, Callback &&cb,
                       bool hierarchically = false) const {
        static_assert(std::is_invocable_r_v<bool, Callback, CKBehavior *>,
                      "Callback must be callable as bool(CKBehavior*)");
        if (!m_Script) return true;
        return detail::FindBehaviorByGuidImpl(m_Script, prototypeGuid, hierarchically, cb);
    }

    // -- Wrap existing pointers --

    Node From(CKBehavior *beh) const { return {m_Script, beh}; }
    Node From(CKBehaviorIO *io) const { return {m_Script, io}; }

    // -- Behavior construction --

    Node CreateBehavior(CKGUID guid, bool useTarget = false) {
        if (!m_Script) return {};
        auto *beh = static_cast<CKBehavior *>(m_Script->GetCKContext()->CreateObject(CKCID_BEHAVIOR));
        beh->InitFromGuid(guid);
        if (useTarget) beh->UseTarget();
        m_Script->AddSubBehavior(beh);
        return {m_Script, beh};
    }

    // -- Link creation --

    CKBehaviorLink *Link(CKBehaviorIO *in, CKBehaviorIO *out, int delay = 0) {
        if (!m_Script || !in || !out) return nullptr;
        auto *link = static_cast<CKBehaviorLink *>(m_Script->GetCKContext()->CreateObject(CKCID_BEHAVIORLINK));
        link->SetInitialActivationDelay(delay);
        link->ResetActivationDelay();
        link->SetInBehaviorIO(in);
        link->SetOutBehaviorIO(out);
        m_Script->AddSubBehaviorLink(link);
        return link;
    }

    CKBehaviorLink *Link(CKBehavior *in, CKBehavior *out,
                         int inPos = 0, int outPos = 0, int delay = 0) {
        if (!in || !out) return nullptr;
        return Link(in->GetOutput(inPos), out->GetInput(outPos), delay);
    }

    CKBehaviorLink *Link(CKBehavior *in, CKBehaviorIO *out, int inPos = 0, int delay = 0) {
        if (!in) return nullptr;
        return Link(in->GetOutput(inPos), out, delay);
    }

    CKBehaviorLink *Link(CKBehaviorIO *in, CKBehavior *out, int outPos = 0, int delay = 0) {
        if (!out) return nullptr;
        return Link(in, out->GetInput(outPos), delay);
    }

    // -- Link lookup --

    CKBehaviorLink *GetLink(CKBehaviorIO *in, CKBehaviorIO *out) const {
        if (!m_Script || !in || !out) return nullptr;
        int cnt = m_Script->GetSubBehaviorLinkCount();
        for (int i = 0; i < cnt; i++) {
            CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
            if (link->GetInBehaviorIO() == in && link->GetOutBehaviorIO() == out)
                return link;
        }
        return nullptr;
    }

    CKBehaviorLink *GetLink(CKBehavior *in, CKBehavior *out,
                            int inPos = 0, int outPos = 0) const {
        if (!in || !out) return nullptr;
        return GetLink(in->GetOutput(inPos), out->GetInput(outPos));
    }

    // -- Link mutation --

    void Relink(CKBehaviorLink *link, CKBehaviorIO *in, CKBehaviorIO *out) {
        if (!link) return;
        if (in) link->SetInBehaviorIO(in);
        if (out) link->SetOutBehaviorIO(out);
    }

    void Relink(CKBehaviorLink *link, CKBehavior *in, CKBehavior *out,
                int inPos = 0, int outPos = 0) {
        if (!link) return;
        if (in) link->SetInBehaviorIO(in->GetOutput(inPos));
        if (out) link->SetOutBehaviorIO(out->GetInput(outPos));
    }

    void Insert(CKBehaviorLink *link, CKBehavior *beh, int inPos = 0, int outPos = 0) {
        if (!link || !beh) return;
        Link(beh, link->GetOutBehaviorIO(), outPos);
        link->SetOutBehaviorIO(beh->GetInput(inPos));
    }

    // -- Parameters --

    CKParameterLocal *Param(const char *name, CKGUID type) {
        if (!m_Script) return nullptr;
        return m_Script->CreateLocalParameter((CKSTRING) name, type);
    }

    template <typename T>
    CKParameterLocal *Param(const char *name, CKGUID type, T value) {
        CKParameterLocal *param = Param(name, type);
        if (param) detail::WriteParam(param, value);
        return param;
    }

    CKParameterLocal *ParamString(const char *name, const char *value) {
        return Param(name, CKPGUID_STRING, value);
    }

    CKParameterLocal *GenerateInput(CKBehavior *beh, int inPos) {
        if (!m_Script || !beh) return nullptr;
        CKParameterIn *pin = beh->GetInputParameter(inPos);
        if (!pin) return nullptr;
        auto *pm = beh->GetCKContext()->GetParameterManager();
        CKParameterLocal *param = Param(pin->GetName(), pm->ParameterTypeToGuid(pin->GetType()));
        pin->SetDirectSource(param);
        return param;
    }

    template <typename T>
    CKParameterLocal *GenerateInput(CKBehavior *beh, int inPos, T value) {
        CKParameterLocal *param = GenerateInput(beh, inPos);
        if (param) detail::WriteParam(param, value);
        return param;
    }

    CKParameterLocal *GenerateTarget(CKBehavior *beh) {
        if (!m_Script || !beh) return nullptr;
        CKParameterIn *pin = beh->GetTargetParameter();
        if (!pin) return nullptr;
        auto *pm = beh->GetCKContext()->GetParameterManager();
        CKParameterLocal *param = Param(pin->GetName(), pm->ParameterTypeToGuid(pin->GetType()));
        pin->SetDirectSource(param);
        return param;
    }

    template <typename T>
    CKParameterLocal *GenerateTarget(CKBehavior *beh, T value) {
        CKParameterLocal *param = GenerateTarget(beh);
        if (param) detail::WriteParam(param, value);
        return param;
    }

    // -- Deletion --

    void Unlink(CKBehaviorLink *link) {
        if (!link || !m_Script) return;
        m_Script->RemoveSubBehaviorLink(link);
        m_Script->GetCKContext()->DestroyObject(link);
    }

    void Remove(CKBehavior *beh) {
        if (!beh || !m_Script) return;
        beh->Activate(false);
        for (int i = m_Script->GetSubBehaviorLinkCount() - 1; i >= 0; --i) {
            CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
            if (link && (link->GetInBehaviorIO()->GetOwner() == beh ||
                         link->GetOutBehaviorIO()->GetOwner() == beh))
                Unlink(link);
        }
        m_Script->RemoveSubBehavior(beh);
        m_Script->GetCKContext()->DestroyObject(beh);
    }

    // -- Move between graphs --

    void Move(CKBehavior *beh, Graph &dest) {
        if (!beh || !m_Script || !dest.m_Script) return;
        beh->Activate(false);
        for (int i = m_Script->GetSubBehaviorLinkCount() - 1; i >= 0; --i) {
            CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
            if (link && (link->GetInBehaviorIO()->GetOwner() == beh ||
                         link->GetOutBehaviorIO()->GetOwner() == beh))
                Unlink(link);
        }
        m_Script->RemoveSubBehavior(beh);
        dest.m_Script->AddSubBehavior(beh);
    }

    // -- Link enumeration --

    int LinkCount() const {
        return m_Script ? m_Script->GetSubBehaviorLinkCount() : 0;
    }

    LinkView LinkAt(int index) const {
        return LinkView(m_Script ? m_Script->GetSubBehaviorLink(index) : nullptr);
    }

    template <typename Callback>
    bool ForEachLink(Callback &&cb) const {
        static_assert(std::is_invocable_r_v<bool, Callback, LinkView>,
                      "Callback must be callable as bool(LinkView)");
        if (!m_Script) return true;
        int cnt = m_Script->GetSubBehaviorLinkCount();
        for (int i = 0; i < cnt; i++) {
            if (!cb(LinkView(m_Script->GetSubBehaviorLink(i))))
                return false;
        }
        return true;
    }

    template <typename Callback>
    bool ForEachOutgoingLink(CKBehavior *beh, Callback &&cb,
                             int outputSlot = -1) const {
        static_assert(std::is_invocable_r_v<bool, Callback, LinkView>,
                      "Callback must be callable as bool(LinkView)");
        if (!m_Script || !beh) return true;
        CKBehaviorIO *slotIO = (outputSlot >= 0) ? beh->GetOutput(outputSlot) : nullptr;
        int cnt = m_Script->GetSubBehaviorLinkCount();
        for (int i = 0; i < cnt; i++) {
            CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
            if (!link) continue;
            CKBehaviorIO *in = link->GetInBehaviorIO();
            if (in && in->GetOwner() == beh && (!slotIO || in == slotIO)) {
                if (!cb(LinkView(link)))
                    return false;
            }
        }
        return true;
    }

    template <typename Callback>
    bool ForEachIncomingLink(CKBehavior *beh, Callback &&cb,
                             int inputSlot = -1) const {
        static_assert(std::is_invocable_r_v<bool, Callback, LinkView>,
                      "Callback must be callable as bool(LinkView)");
        if (!m_Script || !beh) return true;
        CKBehaviorIO *slotIO = (inputSlot >= 0) ? beh->GetInput(inputSlot) : nullptr;
        int cnt = m_Script->GetSubBehaviorLinkCount();
        for (int i = 0; i < cnt; i++) {
            CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
            if (!link) continue;
            CKBehaviorIO *out = link->GetOutBehaviorIO();
            if (out && out->GetOwner() == beh && (!slotIO || out == slotIO)) {
                if (!cb(LinkView(link)))
                    return false;
            }
        }
        return true;
    }

    template <typename Callback>
    bool ForEachOutgoingLink(CKBehaviorIO *io, Callback &&cb) const {
        static_assert(std::is_invocable_r_v<bool, Callback, LinkView>,
                      "Callback must be callable as bool(LinkView)");
        if (!m_Script || !io) return true;
        int cnt = m_Script->GetSubBehaviorLinkCount();
        for (int i = 0; i < cnt; i++) {
            CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
            if (link && link->GetInBehaviorIO() == io) {
                if (!cb(LinkView(link)))
                    return false;
            }
        }
        return true;
    }

    template <typename Callback>
    bool ForEachIncomingLink(CKBehaviorIO *io, Callback &&cb) const {
        static_assert(std::is_invocable_r_v<bool, Callback, LinkView>,
                      "Callback must be callable as bool(LinkView)");
        if (!m_Script || !io) return true;
        int cnt = m_Script->GetSubBehaviorLinkCount();
        for (int i = 0; i < cnt; i++) {
            CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
            if (link && link->GetOutBehaviorIO() == io) {
                if (!cb(LinkView(link)))
                    return false;
            }
        }
        return true;
    }

    // -- Graph analysis --

    // Returns the minimum sum of InitialActivationDelay across all paths
    // from `from` to `to` within this graph. Returns -1 if no path exists.
    int ShortestDelay(CKBehavior *from, CKBehavior *to) const {
        if (!m_Script || !from || !to) return -1;
        if (from == to) return 0;

        int linkCnt = m_Script->GetSubBehaviorLinkCount();
        if (linkCnt == 0) return -1;

        // Bellman-Ford relaxation (correct for non-negative delays,
        // simple for the small graphs typical in Ballance scripts)
        std::unordered_map<CKBehavior *, int> dist;
        dist[from] = 0;

        bool changed = true;
        for (int round = 0; changed; round++) {
            changed = false;
            for (int i = 0; i < linkCnt; i++) {
                CKBehaviorLink *link = m_Script->GetSubBehaviorLink(i);
                if (!link) continue;
                CKBehaviorIO *inIO = link->GetInBehaviorIO();
                CKBehaviorIO *outIO = link->GetOutBehaviorIO();
                if (!inIO || !outIO) continue;

                CKBehavior *src = inIO->GetOwner();
                CKBehavior *dst = outIO->GetOwner();
                auto it = dist.find(src);
                if (it == dist.end()) continue;

                int nd = it->second + link->GetInitialActivationDelay();
                auto dit = dist.find(dst);
                if (dit == dist.end() || nd < dit->second) {
                    dist[dst] = nd;
                    changed = true;
                }
            }
            // Guard against cycles: at most V-1 useful rounds
            if (round >= static_cast<int>(dist.size()))
                break;
        }

        auto it = dist.find(to);
        return it != dist.end() ? it->second : -1;
    }

private:
    CKBehavior *m_Script;
};

inline Graph Node::AsGraph() const { return Graph(m_Beh); }

} // namespace bml

#endif // BML_SCRIPTGRAPH_H
