#ifndef BML_MODDLL_H
#define BML_MODDLL_H

#include <string>

#include "CKDefines.h"

#include "IBML.h"
#include "IMod.h"

typedef IMod *(*GetBMLEntryFunction)(IBML *);
typedef void (*GetBMLExitFunction)(IMod *);
typedef void (*RegisterBBFunction)(XObjectDeclarationArray *);

struct ModDll {
    std::string dllFileName;
    std::string dllPath;
    INSTANCE_HANDLE dllInstance;
    GetBMLEntryFunction entry;
    GetBMLExitFunction exit;
    RegisterBBFunction registerBB;

    bool Load();

    INSTANCE_HANDLE LoadDll();

    template<typename T>
    T GetFunction(const char *func) const {
        return reinterpret_cast<T>(GetFunctionPtr(func));
    }

    void *GetFunctionPtr(const char *func) const;
};

#endif // BML_MODDLL_H
