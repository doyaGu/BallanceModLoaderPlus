#ifndef BML_IVP_FORCEFIELD_H
#define BML_IVP_FORCEFIELD_H

#include "BML/IVP/Controller.h"
#include "BML/IVP/Set.h"

#include <cstddef>

// Forcefields are Mod-defined independent controllers attached to a dynamic
// set of cores. Ballance does not retain a Forcefield-specific body or vtable,
// but its imported 0x10 UDT and the nearby public declaration agree on both
// base-subobject offsets and the two-field tail. The engine-facing bases use
// the already verified retail listener/controller ABIs.
class IVP_Forcefield
    : protected IVP_Listener_Set_Active<IVP_Core>,
      protected IVP_Controller_Independent {
protected:
    IVP_U_Set_Active<IVP_Core> *set_of_cores;
    IVP_BOOL i_am_owner_of_set_of_cores;

    void element_added(
        IVP_U_Set_Active<IVP_Core> *, IVP_Core *core) override {
        IVP_Controller_Manager::add_controller_to_core(this, core);
    }

    void element_removed(
        IVP_U_Set_Active<IVP_Core> *, IVP_Core *core) override {
        IVP_Controller_Manager::remove_controller_from_core(this, core);
    }

    void pset_is_going_to_be_deleted(
        IVP_U_Set_Active<IVP_Core> *) override {
        if (i_am_owner_of_set_of_cores == IVP_TRUE)
            delete this;
    }

    void core_is_going_to_be_deleted_event(IVP_Core *core) override {
        IVP_Controller_Manager::remove_controller_from_core(this, core);
    }

    virtual void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *coreList) override = 0;

    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_ACTUATOR;
    }

    IVP_Forcefield(
        IVP_Environment *, IVP_U_Set_Active<IVP_Core> *cores,
        IVP_BOOL ownerOfSet)
        : set_of_cores(cores),
          i_am_owner_of_set_of_cores(ownerOfSet) {
        IVP_U_Set_Enumerator<IVP_Core> allCores(set_of_cores);
        while (IVP_Core *core = allCores.get_next_element(set_of_cores))
            element_added(set_of_cores, core);
        set_of_cores->add_listener_set_active(this);
    }

public:
    // Controller is a protected secondary base here. Republish its allocator
    // at the public complete-object level so a Mod can use `new Derived` and
    // the set/core deletion callbacks still return memory to the retail heap.
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    ~IVP_Forcefield() override {
        if (!set_of_cores)
            return;
        IVP_U_Set_Enumerator<IVP_Core> allCores(set_of_cores);
        while (IVP_Core *core = allCores.get_next_element(set_of_cores))
            element_removed(set_of_cores, core);
        set_of_cores->remove_listener_set_active(this);
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Forcefield) == 0x10);
#endif

#endif // BML_IVP_FORCEFIELD_H
