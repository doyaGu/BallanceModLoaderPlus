#ifndef BML_CKIDENTITYREGISTRY_H
#define BML_CKIDENTITYREGISTRY_H

#include "BML/Types.h"
#include "CKTypes.h"

#include <cstdint>
#include <memory>

class CKContext;
class CKObject;

namespace BML {

// Owns the stale-safe identity of CK objects for one CKContext. All operations
// run on the game thread. Resolve is O(1), takes no lock, and never allocates.
class CKIdentityRegistry final {
public:
    explicit CKIdentityRegistry(CKContext *context) noexcept;
    ~CKIdentityRegistry();

    CKIdentityRegistry(const CKIdentityRegistry &) = delete;
    CKIdentityRegistry &operator=(const CKIdentityRegistry &) = delete;

    [[nodiscard]] BML_ObjectRef Make(CKObject *object) noexcept;
    [[nodiscard]] CKObject *Resolve(BML_ObjectRef reference) const noexcept;

    void Invalidate(const CK_ID *ids, int count) noexcept;
    void ResetWorld() noexcept;

private:
    struct Storage;

    [[nodiscard]] uint32_t NextSerial() noexcept;

    CKContext *m_Context = nullptr;
    std::unique_ptr<Storage> m_Storage;
    uint32_t m_NextSerial = 1;
};

} // namespace BML

#endif // BML_CKIDENTITYREGISTRY_H
