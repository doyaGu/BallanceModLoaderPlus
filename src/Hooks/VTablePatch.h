#ifndef BML_HOOKS_VTABLE_PATCH_H
#define BML_HOOKS_VTABLE_PATCH_H

#include <cstddef>
#include <vector>

enum class VTablePatchError {
    None,
    AlreadyInstalled,
    InvalidRequest,
    InvalidInstance,
    InvalidVTable,
    InvalidSlot,
    DuplicateSlot,
    InvalidOriginal,
    InvalidReplacement,
    NonExecutableOriginal,
    NonExecutableReplacement,
    UnsupportedProtectionLayout,
    ProtectionChangeFailed,
    SlotChanged,
    ProtectionRestoreFailed,
    OwnershipLost,
};

struct VTablePatchResult {
    VTablePatchError Code = VTablePatchError::None;
    std::size_t EntryIndex = static_cast<std::size_t>(-1);

    explicit operator bool() const { return Code == VTablePatchError::None; }
};

class VTablePatch {
public:
    struct Request {
        std::size_t Slot = 0;
        void *Replacement = nullptr;
    };

    VTablePatch() = default;
    ~VTablePatch();

    VTablePatch(const VTablePatch &) = delete;
    VTablePatch &operator=(const VTablePatch &) = delete;

    VTablePatchResult Install(void *instance, const Request *requests, std::size_t count);
    VTablePatchResult Remove();

    bool IsInstalled() const { return m_VTable != nullptr; }
    void *GetOriginal(std::size_t slot) const;

    static const char *GetErrorName(VTablePatchError error);

private:
    struct Entry {
        std::size_t Slot;
        void *Original;
        void *Replacement;
    };

    void Clear();

    void **m_VTable = nullptr;
    void *m_Region = nullptr;
    std::size_t m_RegionSize = 0;
    std::vector<Entry> m_Entries;
};

#endif // BML_HOOKS_VTABLE_PATCH_H
