#ifndef BML_NATIVENOTIFICATION_H
#define BML_NATIVENOTIFICATION_H

#include "Loader/NativeMod.h"

class ModContext;

// One immutable payload per broadcast, shared by all native subscribers. Nested
// broadcasts borrow another frame so they cannot overwrite an outer payload.
class NativeNotification {
public:
    explicit NativeNotification(ModContext &context);
    ~NativeNotification();
    NativeNotification(const NativeNotification &) = delete;
    NativeNotification &operator=(const NativeNotification &) = delete;
    void Prepare(void (IMessageReceiver::*callback)());
    void Prepare(void (IMod::*callback)());
    void Prepare(void (IMod::*)(CK_RENDER_FLAGS), CK_RENDER_FLAGS flags);
    void Prepare(void (IMod::*)(bool), bool enabled);
    void Prepare(void (IMod::*)(const char *, CKBehavior *), const char *, CKBehavior *);
    void Prepare(void (IMod::*)(CK3dEntity *), CK3dEntity *);
    void Prepare(void (IMod::*)(ICommand *, const std::vector<std::string> &), ICommand *,
                 const std::vector<std::string> &);
    void Prepare(void (IMod::*)(const char *, CKBOOL, const char *, CK_CLASSID, CKBOOL, CKBOOL,
                                CKBOOL, CKBOOL, XObjectArray *, CKObject *),
                 const char *, CKBOOL, const char *, CK_CLASSID, CKBOOL, CKBOOL, CKBOOL, CKBOOL,
                 XObjectArray *, CKObject *);
    void Prepare(void (IMod::*)(CK3dEntity *, CKBOOL, float, float, float, const char *, CKBOOL,
                                CKBOOL, CKBOOL, float, float, const char *, VxVector, int,
                                CKMesh **, int, VxVector *, float *, int, CKMesh **),
                 CK3dEntity *, CKBOOL, float, float, float, const char *, CKBOOL, CKBOOL, CKBOOL,
                 float, float, const char *, VxVector, int, CKMesh **, int, VxVector *, float *,
                 int, CKMesh **);
    int Dispatch(NativeMod &mod) const;
    const char *Name() const noexcept;
    struct Storage;

private:
    ModContext &m_Context;
    Storage *m_Storage;
};
#endif
