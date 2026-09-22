#ifndef BML_IVP_RUNTIME_H
#define BML_IVP_RUNTIME_H

#include "BML/IVP.h"

#include <memory>

class CK3dEntity;
class CKContext;

namespace BML::IVP {

// Version lock and native-object discovery for Ballance's IVP build.
class Runtime final {
public:
    explicit Runtime(CKContext *context) noexcept;
    ~Runtime();

    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

    int ReadApiInfo(BML_IvpApiInfo &out) noexcept;
    int GetManager(uintptr_t &out) noexcept;
    int GetEnvironment(uintptr_t &out) noexcept;
    int GetPhysicsObject(CK3dEntity *entity, uintptr_t &out) noexcept;
    int GetRealObject(CK3dEntity *entity, uintptr_t &out) noexcept;
    int GetCore(CK3dEntity *entity, uintptr_t &out) noexcept;
    int GetMaterial(CK3dEntity *entity, uintptr_t &out) noexcept;
    int ResolveSymbol(const char *name, uintptr_t &out) noexcept;
    int ResolveRva(uint32_t rva, uintptr_t &out) noexcept;
    uint32_t GetSymbolCount() const noexcept;
    int GetSymbol(uint32_t index, BML_IvpSymbol &out) const noexcept;

private:
    struct Storage;
    std::unique_ptr<Storage> m_Storage;
};

} // namespace BML::IVP

#endif // BML_IVP_RUNTIME_H
