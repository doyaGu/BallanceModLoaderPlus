#ifndef BML_API_OBJECTREFS_H
#define BML_API_OBJECTREFS_H

#include "BML/Types.h"
#include "CKTypes.h"

#include <cstdint>
#include <memory>

class CKContext;
class CKObject;

namespace BML {

// Owns the opaque CK object references exposed through the C and script
// interfaces. Objects destroyed through normal CK notifications are
// invalidated before their IDs can be reused; NONOTIFY objects are outside
// this interface because CK does not expose their lifetime to managers.
class ObjectRefs final {
public:
    explicit ObjectRefs(CKContext *context) noexcept;
    ~ObjectRefs();

    ObjectRefs(const ObjectRefs &) = delete;
    ObjectRefs &operator=(const ObjectRefs &) = delete;

    [[nodiscard]] BML_ObjectRef Issue(CKObject *object) noexcept;
    [[nodiscard]] CKObject *Resolve(BML_ObjectRef reference) const noexcept;

    void Invalidate(const CK_ID *ids, int count) noexcept;
    void Reset() noexcept;

private:
    struct Storage;

    [[nodiscard]] std::uint32_t NextGeneration() noexcept;

    CKContext *m_Context = nullptr;
    std::unique_ptr<Storage> m_Storage;
    std::uint32_t m_NextGeneration = 1;
};

} // namespace BML

#endif // BML_API_OBJECTREFS_H
