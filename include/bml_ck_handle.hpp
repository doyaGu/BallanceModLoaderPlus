#ifndef BML_CK_HANDLE_HPP
#define BML_CK_HANDLE_HPP

#include "CKObject.h"
#include "CKContext.h"

namespace bml {

/// Lightweight witness-validated handle to a CK object.
///
/// Stores CKContext* + CK_ID + T* (witness pointer). Get() validates via
/// GetObject(id)==witness && !IsToBeDeleted(). The witness pointer is never
/// dereferenced -- only compared as a unique identity token.
///
/// sizeof = 12 bytes, trivially copyable, no heap allocation.
template <typename T>
class CKHandle {
    CKContext *m_Ctx = nullptr;
    CK_ID m_Id = 0;
    T *m_Original = nullptr;

public:
    CKHandle() = default;

    CKHandle(CKContext *ctx, T *obj)
        : m_Ctx(ctx)
        , m_Id(obj ? obj->GetID() : 0)
        , m_Original(obj) {}

    T *Get() const {
        if (!m_Ctx || !m_Id) return nullptr;
        CKObject *obj = m_Ctx->GetObject(m_Id);
        if (obj != m_Original) return nullptr;
        if (obj->IsToBeDeleted()) return nullptr;
        return static_cast<T *>(obj);
    }

    T *operator->() const { return Get(); }
    T &operator*() const { return *Get(); }
    explicit operator bool() const { return Get() != nullptr; }

    CK_ID Id() const { return m_Id; }
    CKContext *Context() const { return m_Ctx; }
    bool IsNull() const { return m_Id == 0; }

    void Reset() { m_Ctx = nullptr; m_Id = 0; m_Original = nullptr; }
    void Reset(CKContext *ctx, T *obj) {
        m_Ctx = ctx;
        m_Id = obj ? obj->GetID() : 0;
        m_Original = obj;
    }

    bool operator==(const CKHandle &o) const { return m_Id == o.m_Id; }
    bool operator!=(const CKHandle &o) const { return m_Id != o.m_Id; }
};

namespace ck {

template <typename T>
CKHandle<T> Wrap(CKContext *ctx, T *raw) {
    return CKHandle<T>(ctx, raw);
}

template <typename T>
CKHandle<T> Find(CKContext *ctx, const char *name, CK_CLASSID cid) {
    auto *obj = static_cast<T *>(
        ctx->GetObjectByNameAndClass((CKSTRING)name, cid));
    return CKHandle<T>(ctx, obj);
}

} // namespace ck
} // namespace bml

#endif // BML_CK_HANDLE_HPP
