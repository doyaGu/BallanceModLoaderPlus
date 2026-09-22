// Native access to the Ipion Virtual Physics engine embedded in Ballance's
// original physics_RT.dll. The independent IVP base Mod owns this interface
// and publishes it through BML_GetInterface. This API deliberately exposes
// borrowed engine objects and binary addresses. It is tied to the verified
// 32-bit retail DLL and must be used on the game thread.
#ifndef BML_IVP_H
#define BML_IVP_H

#include "BML/Interface.h"

BML_BEGIN_CDECLS

#define BML_IVP_INTERFACE_ID "bml.ivp"
#define BML_IVP_INTERFACE_MAJOR 1
#define BML_IVP_INTERFACE_MINOR 0
#define IVP_MOD_ID "IVP"

#define BML_IVP_ABI_REVISION             1u

#define BML_IVP_CAP_NATIVE_OBJECTS       0x00000001u
#define BML_IVP_CAP_IDA_SYMBOLS          0x00000002u
#define BML_IVP_CAP_EXECUTABLE_RVAS      0x00000004u
#define BML_IVP_CAP_RECONSTRUCTED_TYPES  0x00000008u
#define BML_IVP_CAP_RECONSTRUCTED_CALLS  0x00000010u
#define BML_IVP_CAP_ORIGINAL_DLL_LOCK    0x00000020u

#define BML_IVP_ARCH_X86 1u

#define BML_IVP_SYMBOL_FUNCTION          0x00000001u
#define BML_IVP_SYMBOL_IDA_NAMED         0x00000002u
#define BML_IVP_SYMBOL_DECORATED         0x00000004u
#define BML_IVP_SYMBOL_ENGINE            0x00000008u
#define BML_IVP_SYMBOL_MANUAL_NAME       0x00000010u

typedef struct BML_IvpApiInfo {
    uint32_t Capabilities;
    uint32_t Architecture;
    uint32_t AbiRevision;
    uint32_t ImageTimestamp;
    uint32_t ImageSize;
    uint32_t FunctionSymbolCount;
    // Upper-case SHA-256 of the supported retail DLL, followed by a null.
    char ImageSha256[65];
    char Reserved[3];
} BML_IvpApiInfo;

typedef struct BML_IvpSymbol {
    // Loader-owned IDA name, valid for the process lifetime. It can be an
    // original decorated name or a manually assigned analysis name.
    const char *Name;
    uint32_t Rva;
    uint32_t Flags;
} BML_IvpSymbol;

typedef struct BML_IvpInterface {
    BML_InterfaceHeader Header;

    int (BML_CDECL *ReadApiInfo)(BML_IvpApiInfo *out);

    // All returned pointers are borrowed. Never delete them or retain them
    // across Unphysicalize, object deletion, level reset, or environment reset.
    int (BML_CDECL *GetManager)(uintptr_t *out);
    int (BML_CDECL *GetEnvironment)(uintptr_t *out);
    int (BML_CDECL *GetPhysicsObject)(void *entity, uintptr_t *out);
    int (BML_CDECL *GetRealObject)(void *entity, uintptr_t *out);
    int (BML_CDECL *GetCore)(void *entity, uintptr_t *out);
    int (BML_CDECL *GetMaterial)(void *entity, uintptr_t *out);

    // Name uses the exact spelling in the IDA-derived manifest. ResolveRva
    // accepts any address in an executable PE section after the retail image
    // fingerprint and independent analysis anchors have matched.
    int (BML_CDECL *ResolveSymbol)(const char *name, uintptr_t *out);
    int (BML_CDECL *ResolveRva)(uint32_t rva, uintptr_t *out);

    uint32_t (BML_CDECL *GetSymbolCount)(void);
    int (BML_CDECL *GetSymbol)(uint32_t index, BML_IvpSymbol *out);
} BML_IvpInterface;

BML_END_CDECLS

#ifdef __cplusplus

#include <atomic>
#include <type_traits>

class CK3dEntity;
class CKBaseManager;
class IVP_Environment;
class IVP_Real_Object;
class IVP_Core;
class IVP_Material;
struct BML_IVP_PhysicsObject;

namespace BML::IVP {

using ApiInfo = BML_IvpApiInfo;
using Symbol = BML_IvpSymbol;

namespace Detail {

inline const BML_IvpInterface *Interface() {
    static std::atomic<const BML_IvpInterface *> found{nullptr};
    const BML_IvpInterface *value = found.load(std::memory_order_acquire);
    if (value)
        return value;

    value = BML::FindInterface<BML_IvpInterface>(
        BML_IVP_INTERFACE_ID, BML_IVP_INTERFACE_MAJOR);
    if (value)
        found.store(value, std::memory_order_release);
    return value;
}

inline uintptr_t PointerResult(
    int (BML_CDECL *function)(uintptr_t *)) {
    uintptr_t value = 0;
    return function && function(&value) == BML_OK ? value : 0;
}

inline uintptr_t ObjectResult(
    int (BML_CDECL *function)(void *, uintptr_t *), CK3dEntity *entity) {
    uintptr_t value = 0;
    return function && entity && function(entity, &value) == BML_OK ? value : 0;
}

} // namespace Detail

[[nodiscard]] inline const BML_IvpInterface *GetInterface() {
    return Detail::Interface();
}

[[nodiscard]] inline int ReadApiInfo(ApiInfo &out) {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, ReadApiInfo)
        ? ivp->ReadApiInfo(&out)
        : BML_ERROR_NOT_FOUND;
}

[[nodiscard]] inline CKBaseManager *Manager() {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetManager)
        ? reinterpret_cast<CKBaseManager *>(Detail::PointerResult(ivp->GetManager))
        : nullptr;
}

[[nodiscard]] inline IVP_Environment *Environment() {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetEnvironment)
        ? reinterpret_cast<IVP_Environment *>(Detail::PointerResult(ivp->GetEnvironment))
        : nullptr;
}

[[nodiscard]] inline BML_IVP_PhysicsObject *PhysicsObject(CK3dEntity *entity) {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetPhysicsObject)
        ? reinterpret_cast<BML_IVP_PhysicsObject *>(
              Detail::ObjectResult(ivp->GetPhysicsObject, entity))
        : nullptr;
}

[[nodiscard]] inline IVP_Real_Object *RealObject(CK3dEntity *entity) {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetRealObject)
        ? reinterpret_cast<IVP_Real_Object *>(
              Detail::ObjectResult(ivp->GetRealObject, entity))
        : nullptr;
}

[[nodiscard]] inline IVP_Core *Core(CK3dEntity *entity) {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetCore)
        ? reinterpret_cast<IVP_Core *>(Detail::ObjectResult(ivp->GetCore, entity))
        : nullptr;
}

[[nodiscard]] inline IVP_Material *Material(CK3dEntity *entity) {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetMaterial)
        ? reinterpret_cast<IVP_Material *>(
              Detail::ObjectResult(ivp->GetMaterial, entity))
        : nullptr;
}

template <typename Function>
[[nodiscard]] inline Function Resolve(const char *name) {
    static_assert(std::is_pointer_v<Function>, "Function must be a function pointer");
    const BML_IvpInterface *ivp = Detail::Interface();
    uintptr_t address = 0;
    if (!BML_IFACE_HAS(ivp, BML_IvpInterface, ResolveSymbol) ||
        ivp->ResolveSymbol(name, &address) != BML_OK) {
        return nullptr;
    }
    return reinterpret_cast<Function>(address);
}

template <typename Function>
[[nodiscard]] inline Function ResolveRva(uint32_t rva) {
    static_assert(std::is_pointer_v<Function>, "Function must be a function pointer");
    const BML_IvpInterface *ivp = Detail::Interface();
    uintptr_t address = 0;
    if (!BML_IFACE_HAS(ivp, BML_IvpInterface, ResolveRva) ||
        ivp->ResolveRva(rva, &address) != BML_OK) {
        return nullptr;
    }
    return reinterpret_cast<Function>(address);
}

[[nodiscard]] inline uint32_t SymbolCount() {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetSymbolCount)
        ? ivp->GetSymbolCount()
        : 0;
}

[[nodiscard]] inline int ReadSymbol(uint32_t index, Symbol &out) {
    const BML_IvpInterface *ivp = Detail::Interface();
    return BML_IFACE_HAS(ivp, BML_IvpInterface, GetSymbol)
        ? ivp->GetSymbol(index, &out)
        : BML_ERROR_NOT_FOUND;
}

} // namespace BML::IVP

#endif // __cplusplus

#endif // BML_IVP_H
