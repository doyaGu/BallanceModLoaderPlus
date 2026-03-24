/**
 * @file bml_virtools.hpp
 * @brief C++ convenience wrappers for Virtools engine object retrieval
 *
 * Header-only inline helpers that wrap the repetitive GetUserData pattern
 * into one-line calls. Companion to bml_virtools.h.
 *
 * Also provides typed object-lookup helpers that require CKAll.h to be
 * included *before* this header:
 *
 *   #include "CKAll.h"           // or at minimum CKContext.h
 *   #include "bml_virtools.hpp"
 *
 * The lookup helpers are only defined when CKContext is a complete type
 * (detected via CKCONTEXT_H / CK_DEFINES_H).
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
        const auto *ctx = svc.Interfaces().Context;
        if (!ctx || !ctx->GetUserData) return nullptr;
        void *value = nullptr;
        if (ctx->GetUserData(svc.GlobalContext(), key, &value) != BML_RESULT_OK) {
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

    /* ====================================================================
     * Object Lookup Helpers
     *
     * These require a CKContext pointer (obtained via GetCKContext above)
     * and call the Virtools SDK directly. They are available only when
     * CKContext is a complete type -- include CKAll.h before this header.
     * ==================================================================== */

#if defined(CKCONTEXT_H) || defined(CK_DEFINES_H)

    /**
     * @brief Find a CK object by name and class ID.
     * @return Typed pointer, or nullptr if not found.
     */
    template <typename T = CKObject>
    inline T *FindObject(CKContext *ctx, const char *name, CK_CLASSID cid = CKCID_OBJECT) {
        if (!ctx || !name) return nullptr;
        return static_cast<T *>(ctx->GetObjectByNameAndClass((CKSTRING)name, cid));
    }

    /**
     * @brief Find a CK object by CK_ID.
     * @return Typed pointer, or nullptr if ID is invalid.
     */
    template <typename T = CKObject>
    inline T *GetObject(CKContext *ctx, CK_ID id) {
        if (!ctx) return nullptr;
        return static_cast<T *>(ctx->GetObject(id));
    }

    // --- Typed shortcuts for common Ballance object types ---

    inline CK3dObject *Find3dObject(CKContext *ctx, const char *name) {
        return FindObject<CK3dObject>(ctx, name, CKCID_3DOBJECT);
    }

    inline CK3dEntity *Find3dEntity(CKContext *ctx, const char *name) {
        return FindObject<CK3dEntity>(ctx, name, CKCID_3DENTITY);
    }

    inline CK2dEntity *Find2dEntity(CKContext *ctx, const char *name) {
        return FindObject<CK2dEntity>(ctx, name, CKCID_2DENTITY);
    }

    inline CKGroup *FindGroup(CKContext *ctx, const char *name) {
        return FindObject<CKGroup>(ctx, name, CKCID_GROUP);
    }

    inline CKDataArray *FindArray(CKContext *ctx, const char *name) {
        return FindObject<CKDataArray>(ctx, name, CKCID_DATAARRAY);
    }

    inline CKMaterial *FindMaterial(CKContext *ctx, const char *name) {
        return FindObject<CKMaterial>(ctx, name, CKCID_MATERIAL);
    }

    inline CKTexture *FindTexture(CKContext *ctx, const char *name) {
        return FindObject<CKTexture>(ctx, name, CKCID_TEXTURE);
    }

    inline CKMesh *FindMesh(CKContext *ctx, const char *name) {
        return FindObject<CKMesh>(ctx, name, CKCID_MESH);
    }

    inline CKBehavior *FindScript(CKContext *ctx, const char *name) {
        return FindObject<CKBehavior>(ctx, name, CKCID_BEHAVIOR);
    }

    inline CKSound *FindSound(CKContext *ctx, const char *name) {
        return FindObject<CKSound>(ctx, name, CKCID_WAVSOUND);
    }

    inline CKCamera *FindCamera(CKContext *ctx, const char *name) {
        return FindObject<CKCamera>(ctx, name, CKCID_CAMERA);
    }

    inline CKLight *FindLight(CKContext *ctx, const char *name) {
        return FindObject<CKLight>(ctx, name, CKCID_LIGHT);
    }

    // --- Overloads taking ModuleServices for convenience ---

    template <typename T = CKObject>
    inline T *FindObject(const ModuleServices &svc, const char *name, CK_CLASSID cid = CKCID_OBJECT) {
        return FindObject<T>(GetCKContext(svc), name, cid);
    }

    template <typename T = CKObject>
    inline T *GetObject(const ModuleServices &svc, CK_ID id) {
        return GetObject<T>(GetCKContext(svc), id);
    }

#endif // CKContext complete type guard

} // namespace virtools
} // namespace bml

#endif /* BML_VIRTOOLS_HPP */
