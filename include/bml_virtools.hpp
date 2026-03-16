/**
 * @file bml_virtools.hpp
 * @brief C++ convenience wrappers for Virtools engine object retrieval
 *
 * Header-only inline helpers that wrap the repetitive GetUserData pattern
 * into one-line calls. Companion to bml_virtools.h.
 */

#ifndef BML_VIRTOOLS_HPP
#define BML_VIRTOOLS_HPP

#include "bml_virtools.h"
#include "bml_services.hpp"

class CKContext;
class CKRenderContext;
class CKInputManager;
class CKTimeManager;
class CKMessageManager;
class CKAttributeManager;
class CKPathManager;

namespace bml {
namespace virtools {

    template <typename T>
    inline T *GetUserData(const ModuleServices &svc, const char *key) {
        void *value = nullptr;
        if (svc.Builtins().Context->GetUserData(svc.GlobalContext(), key, &value) != BML_RESULT_OK) {
            return nullptr;
        }
        return static_cast<T *>(value);
    }

    inline CKContext *GetCKContext(const ModuleServices &svc) {
        return GetUserData<CKContext>(svc, BML_VIRTOOLS_KEY_CKCONTEXT);
    }

    inline CKRenderContext *GetRenderContext(const ModuleServices &svc) {
        return GetUserData<CKRenderContext>(svc, BML_VIRTOOLS_KEY_RENDERCONTEXT);
    }

    inline CKInputManager *GetInputManager(const ModuleServices &svc) {
        return GetUserData<CKInputManager>(svc, BML_VIRTOOLS_KEY_INPUTMANAGER);
    }

    inline CKTimeManager *GetTimeManager(const ModuleServices &svc) {
        return GetUserData<CKTimeManager>(svc, BML_VIRTOOLS_KEY_TIMEMANAGER);
    }

    inline CKMessageManager *GetMessageManager(const ModuleServices &svc) {
        return GetUserData<CKMessageManager>(svc, BML_VIRTOOLS_KEY_MESSAGEMANAGER);
    }

    inline CKAttributeManager *GetAttributeManager(const ModuleServices &svc) {
        return GetUserData<CKAttributeManager>(svc, BML_VIRTOOLS_KEY_ATTRIBUTEMANAGER);
    }

    inline CKPathManager *GetPathManager(const ModuleServices &svc) {
        return GetUserData<CKPathManager>(svc, BML_VIRTOOLS_KEY_PATHMANAGER);
    }

    inline void *GetMainHWND(const ModuleServices &svc) {
        return GetUserData<void>(svc, BML_VIRTOOLS_KEY_MAINHWND);
    }

    inline void *GetRenderHWND(const ModuleServices &svc) {
        return GetUserData<void>(svc, BML_VIRTOOLS_KEY_RENDERHWND);
    }

} // namespace virtools
} // namespace bml

#endif /* BML_VIRTOOLS_HPP */
