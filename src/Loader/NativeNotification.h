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
    void Prepare(ModEvent event);
    void Prepare(ModEvent event, CK_RENDER_FLAGS flags);
    void Prepare(ModEvent event, bool enabled);
    void Prepare(ModEvent event, const char *, CKBehavior *);
    void Prepare(ModEvent event, CK3dEntity *);
    void Prepare(ModEvent event, BML::CommandRecord *, const std::vector<std::string> &);
    void Prepare(ModEvent event, const char *, CKBOOL, const char *, CK_CLASSID, CKBOOL, CKBOOL,
                 CKBOOL, CKBOOL, XObjectArray *, CKObject *);
    void Prepare(ModEvent event, CK3dEntity *, CKBOOL, float, float, float, const char *, CKBOOL,
                 CKBOOL, CKBOOL, float, float, const char *, VxVector, int, CKMesh **, int,
                 VxVector *, float *, int, CKMesh **);
    int Dispatch(NativeMod &mod) const;
    const char *Name() const noexcept;
    struct Storage;

private:
    ModContext &m_Context;
    Storage *m_Storage;
};
#endif
