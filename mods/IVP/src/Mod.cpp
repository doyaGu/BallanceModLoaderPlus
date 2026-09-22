#include "IVP/Runtime.h"

#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <new>

namespace {

std::atomic<BML::IVP::Runtime *> g_Runtime{nullptr};
DWORD g_GameThreadId = 0;

template <typename Body>
int Serve(bool gameThreadOnly, Body &&body) noexcept {
    BML::IVP::Runtime *runtime = g_Runtime.load(std::memory_order_acquire);
    if (!runtime)
        return BML_ERROR_FAIL;
    if (gameThreadOnly && GetCurrentThreadId() != g_GameThreadId)
        return BML_ERROR_WRONG_THREAD;
    try {
        return body(*runtime);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int BML_CDECL ReadApiInfo(BML_IvpApiInfo *out) noexcept {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(true, [out](BML::IVP::Runtime &runtime) {
        return runtime.ReadApiInfo(*out);
    });
}

int BML_CDECL GetManager(uintptr_t *out) noexcept {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(true, [out](BML::IVP::Runtime &runtime) {
        return runtime.GetManager(*out);
    });
}

int BML_CDECL GetEnvironment(uintptr_t *out) noexcept {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(true, [out](BML::IVP::Runtime &runtime) {
        return runtime.GetEnvironment(*out);
    });
}

template <int (BML::IVP::Runtime::*Member)(CK3dEntity *, uintptr_t &) noexcept>
int BML_CDECL GetObjectPart(void *entity, uintptr_t *out) noexcept {
    if (!entity || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(true, [entity, out](BML::IVP::Runtime &runtime) {
        return (runtime.*Member)(static_cast<CK3dEntity *>(entity), *out);
    });
}

int BML_CDECL GetPhysicsObject(void *entity, uintptr_t *out) noexcept {
    return GetObjectPart<&BML::IVP::Runtime::GetPhysicsObject>(entity, out);
}

int BML_CDECL GetRealObject(void *entity, uintptr_t *out) noexcept {
    return GetObjectPart<&BML::IVP::Runtime::GetRealObject>(entity, out);
}

int BML_CDECL GetCore(void *entity, uintptr_t *out) noexcept {
    return GetObjectPart<&BML::IVP::Runtime::GetCore>(entity, out);
}

int BML_CDECL GetMaterial(void *entity, uintptr_t *out) noexcept {
    return GetObjectPart<&BML::IVP::Runtime::GetMaterial>(entity, out);
}

int BML_CDECL ResolveSymbol(const char *name, uintptr_t *out) noexcept {
    if (!name || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(false, [name, out](BML::IVP::Runtime &runtime) {
        return runtime.ResolveSymbol(name, *out);
    });
}

int BML_CDECL ResolveRva(uint32_t rva, uintptr_t *out) noexcept {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(false, [rva, out](BML::IVP::Runtime &runtime) {
        return runtime.ResolveRva(rva, *out);
    });
}

uint32_t BML_CDECL GetSymbolCount() noexcept {
    BML::IVP::Runtime *runtime = g_Runtime.load(std::memory_order_acquire);
    return runtime ? runtime->GetSymbolCount() : 0;
}

int BML_CDECL GetSymbol(uint32_t index, BML_IvpSymbol *out) noexcept {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(false, [index, out](BML::IVP::Runtime &runtime) {
        return runtime.GetSymbol(index, *out);
    });
}

const BML_IvpInterface kInterface = {
    BML_IFACE_HEADER(BML_IvpInterface, BML_IVP_INTERFACE_ID,
                     BML_IVP_INTERFACE_MAJOR, BML_IVP_INTERFACE_MINOR),
    &ReadApiInfo,
    &GetManager,
    &GetEnvironment,
    &GetPhysicsObject,
    &GetRealObject,
    &GetCore,
    &GetMaterial,
    &ResolveSymbol,
    &ResolveRva,
    &GetSymbolCount,
    &GetSymbol,
};

class IvpMod final : public IMod {
public:
    explicit IvpMod(IBML *bml)
        : IMod(bml), m_Runtime(bml ? bml->GetCKContext() : nullptr) {
        AddDependency("BML");
        g_GameThreadId = GetCurrentThreadId();
        g_Runtime.store(&m_Runtime, std::memory_order_release);
    }

    ~IvpMod() override {
        BML::IVP::Runtime *expected = &m_Runtime;
        g_Runtime.compare_exchange_strong(expected, nullptr,
                                          std::memory_order_acq_rel);
    }

    const char *GetID() override { return IVP_MOD_ID; }
    const char *GetVersion() override { return IVP_VERSION; }
    const char *GetName() override { return "IVP"; }
    const char *GetAuthor() override { return "Kakuty"; }
    const char *GetDescription() override {
        return "Retail-locked IVP API and native physics object bridge";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML_IvpApiInfo info{};
        const int status = m_Runtime.ReadApiInfo(info);
        if (status == BML_OK) {
            const int registration =
                BML_RegisterInterface(IVP_MOD_ID, &kInterface);
            if (registration != BML_OK) {
                GetLogger()->Error("Cannot publish %s through BML: %s",
                                   BML_IVP_INTERFACE_ID,
                                   BML_GetErrorString(registration));
                return;
            }
            m_InterfaceRegistered = true;
            GetLogger()->Info("Bound retail physics_RT.dll: timestamp=%08X size=%u symbols=%u",
                              info.ImageTimestamp, info.ImageSize,
                              info.FunctionSymbolCount);
        } else {
            GetLogger()->Error("Cannot bind the supported retail physics_RT.dll: %s",
                               BML_GetErrorString(status));
        }
    }

    void OnUnload() override {
        if (m_InterfaceRegistered) {
            const int status = BML_UnregisterInterface(
                IVP_MOD_ID, BML_IVP_INTERFACE_ID, BML_IVP_INTERFACE_MAJOR);
            if (status != BML_OK && status != BML_ERROR_NOT_FOUND) {
                GetLogger()->Warn("Cannot unpublish %s from BML: %s",
                                  BML_IVP_INTERFACE_ID,
                                  BML_GetErrorString(status));
            }
            m_InterfaceRegistered = false;
        }
        BML::IVP::Runtime *expected = &m_Runtime;
        g_Runtime.compare_exchange_strong(expected, nullptr,
                                          std::memory_order_acq_rel);
    }

private:
    BML::IVP::Runtime m_Runtime;
    bool m_InterfaceRegistered = false;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new (std::nothrow) IvpMod(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
