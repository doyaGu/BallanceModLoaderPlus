#ifndef BML_BUILTIN_MODULE_PROBE_H
#define BML_BUILTIN_MODULE_PROBE_H

#include "bml_types.h"
#include "bml_errors.h"

class BuiltinModuleProbe {
public:
    static bool IsEnabled();

    explicit BuiltinModuleProbe(BML_Mod handle);
    ~BuiltinModuleProbe();

    BML_Result OnAttach();
    void OnDetach();

private:
    class Impl;
    Impl *m_Impl;
};

#endif // BML_BUILTIN_MODULE_PROBE_H
