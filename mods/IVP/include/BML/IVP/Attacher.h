#ifndef BML_IVP_ATTACHER_H
#define BML_IVP_ATTACHER_H

#include "BML/IVP/Set.h"

class IVP_Core;

template <class ATTACH_T>
struct BML_IvpAttacherToCoresLayoutCheck;

// Public source-compatible helper for maintaining one ATTACH_T per active
// core. ATTACH_T must remove itself through
// attachment_is_going_to_be_deleted() from its destructor, exactly as the IVP
// controller-side attachment classes do.
template <class ATTACH_T>
class IVP_Attacher_To_Cores
    : protected IVP_Listener_Set_Active<IVP_Core> {
    friend struct BML_IvpAttacherToCoresLayoutCheck<ATTACH_T>;

protected:
    // Used by wrappers whose complete derived constructor lives in the retail
    // DLL.  That constructor immediately replaces the temporary vptr and
    // reconstructs the hash at the same address.
    struct Retail_Construction_Tag {};
    explicit IVP_Attacher_To_Cores(Retail_Construction_Tag)
        : core_to_attachment_hash(nullptr, 0), set_of_cores(nullptr) {}

public:
    explicit IVP_Attacher_To_Cores(
        IVP_U_Set_Active<IVP_Core> *cores)
        : core_to_attachment_hash(16), set_of_cores(cores) {
        IVP_U_Set_Enumerator<IVP_Core> allCores(set_of_cores);
        while (IVP_Core *core = allCores.get_next_element(set_of_cores))
            element_added(nullptr, core);
        set_of_cores->add_listener_set_active(this);
    }

    void attachment_is_going_to_be_deleted(
        ATTACH_T *, IVP_Core *attachedCore) {
        core_to_attachment_hash.remove_elem(attachedCore);
    }

    IVP_VHash_Store core_to_attachment_hash;

protected:
    virtual ~IVP_Attacher_To_Cores() {
        set_of_cores->remove_listener_set_active(this);
    }

    void element_added(
        IVP_U_Set_Active<IVP_Core> *, IVP_Core *core) override {
        ATTACH_T *attachment = new ATTACH_T(this, core);
        core_to_attachment_hash.add_elem(core, attachment);
    }

    void element_removed(
        IVP_U_Set_Active<IVP_Core> *, IVP_Core *core) override {
        auto *attachment = static_cast<ATTACH_T *>(
            core_to_attachment_hash.find_elem(core));
        if (attachment)
            delete attachment;
    }

    void pset_is_going_to_be_deleted(
        IVP_U_Set_Active<IVP_Core> *cores) override {
        IVP_U_Set_Enumerator<IVP_Core> allCores(cores);
        while (IVP_Core *core = allCores.get_next_element(cores))
            element_removed(nullptr, core);
        delete this;
    }

    IVP_U_Set_Active<IVP_Core> *set_of_cores;
};

template <class ATTACH_T>
struct BML_IvpAttacherToCoresLayoutCheck {
    static constexpr std::size_t attachments =
        offsetof(IVP_Attacher_To_Cores<ATTACH_T>, core_to_attachment_hash);
    static constexpr std::size_t cores =
        offsetof(IVP_Attacher_To_Cores<ATTACH_T>, set_of_cores);
};

#endif // BML_IVP_ATTACHER_H
