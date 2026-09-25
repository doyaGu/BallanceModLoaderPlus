#include "Hooks/VTablePatch.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace {
    bool IsReadableProtection(DWORD protection) {
        if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;

        switch (protection & 0xff) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }

    bool IsExecutableProtection(DWORD protection) {
        if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;

        switch (protection & 0xff) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }

    bool IsReadableRange(const void *address, std::size_t size) {
        if (!address || size == 0)
            return false;

        const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(address);
        if (size > (std::numeric_limits<std::uintptr_t>::max)() - begin)
            return false;
        const std::uintptr_t end = begin + size;

        std::uintptr_t cursor = begin;
        while (cursor < end) {
            MEMORY_BASIC_INFORMATION memory = {};
            if (::VirtualQuery(reinterpret_cast<const void *>(cursor), &memory, sizeof(memory)) == 0 ||
                memory.State != MEM_COMMIT || !IsReadableProtection(memory.Protect)) {
                return false;
            }

            const std::uintptr_t regionBegin = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
            if (memory.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - regionBegin)
                return false;
            const std::uintptr_t regionEnd = regionBegin + memory.RegionSize;
            if (regionEnd <= cursor)
                return false;
            cursor = (std::min)(regionEnd, end);
        }

        return true;
    }

    bool IsExecutableAddress(const void *address) {
        if (!address)
            return false;

        MEMORY_BASIC_INFORMATION memory = {};
        return ::VirtualQuery(address, &memory, sizeof(memory)) != 0 &&
               memory.State == MEM_COMMIT && IsExecutableProtection(memory.Protect);
    }

    bool HasUniformProtection(const void *address, std::size_t size) {
        if (!address || size == 0)
            return false;

        const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(address);
        if (size > (std::numeric_limits<std::uintptr_t>::max)() - begin)
            return false;
        const std::uintptr_t end = begin + size;

        MEMORY_BASIC_INFORMATION first = {};
        if (::VirtualQuery(address, &first, sizeof(first)) == 0 || first.State != MEM_COMMIT ||
            !IsReadableProtection(first.Protect)) {
            return false;
        }

        const void *allocationBase = first.AllocationBase;
        const DWORD protection = first.Protect;
        std::uintptr_t cursor = begin;
        while (cursor < end) {
            MEMORY_BASIC_INFORMATION memory = {};
            if (::VirtualQuery(reinterpret_cast<const void *>(cursor), &memory, sizeof(memory)) == 0 ||
                memory.State != MEM_COMMIT || memory.AllocationBase != allocationBase ||
                memory.Protect != protection || !IsReadableProtection(memory.Protect)) {
                return false;
            }

            const std::uintptr_t regionBegin = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
            if (memory.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - regionBegin)
                return false;
            const std::uintptr_t regionEnd = regionBegin + memory.RegionSize;
            if (regionEnd <= cursor)
                return false;
            cursor = (std::min)(regionEnd, end);
        }

        return true;
    }

    VTablePatchResult MakeResult(VTablePatchError code, std::size_t entryIndex = static_cast<std::size_t>(-1)) {
        VTablePatchResult result;
        result.Code = code;
        result.EntryIndex = entryIndex;
        return result;
    }
}

VTablePatch::~VTablePatch() {
    Remove();
}

VTablePatchResult VTablePatch::Install(void *instance, const Request *requests, std::size_t count) {
    if (IsInstalled())
        return MakeResult(VTablePatchError::AlreadyInstalled);
    if (!requests || count == 0)
        return MakeResult(VTablePatchError::InvalidRequest);
    if (!IsReadableRange(instance, sizeof(void *)))
        return MakeResult(VTablePatchError::InvalidInstance);

    void **vtable = *static_cast<void ***>(instance);
    if (!IsReadableRange(vtable, sizeof(void *)))
        return MakeResult(VTablePatchError::InvalidVTable);

    std::size_t minimumSlot = (std::numeric_limits<std::size_t>::max)();
    std::size_t maximumSlot = 0;
    std::vector<Entry> entries;
    entries.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const Request &request = requests[i];
        if (request.Slot > (std::numeric_limits<std::size_t>::max)() / sizeof(void *))
            return MakeResult(VTablePatchError::InvalidSlot, i);
        if (!request.Replacement)
            return MakeResult(VTablePatchError::InvalidReplacement, i);
        if (!IsExecutableAddress(request.Replacement))
            return MakeResult(VTablePatchError::NonExecutableReplacement, i);

        for (std::size_t j = 0; j < i; ++j) {
            if (requests[j].Slot == request.Slot)
                return MakeResult(VTablePatchError::DuplicateSlot, i);
        }

        minimumSlot = (std::min)(minimumSlot, request.Slot);
        maximumSlot = (std::max)(maximumSlot, request.Slot);
    }

    if (maximumSlot == (std::numeric_limits<std::size_t>::max)())
        return MakeResult(VTablePatchError::InvalidSlot);
    const std::size_t regionSlots = maximumSlot - minimumSlot + 1;
    if (regionSlots > (std::numeric_limits<std::size_t>::max)() / sizeof(void *))
        return MakeResult(VTablePatchError::InvalidSlot);

    const std::uintptr_t vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    const std::size_t regionOffset = minimumSlot * sizeof(void *);
    if (regionOffset > (std::numeric_limits<std::uintptr_t>::max)() - vtableAddress)
        return MakeResult(VTablePatchError::InvalidSlot);

    void *region = reinterpret_cast<void *>(vtableAddress + regionOffset);
    const std::size_t regionSize = regionSlots * sizeof(void *);
    if (!IsReadableRange(region, regionSize))
        return MakeResult(VTablePatchError::InvalidVTable);
    if (!HasUniformProtection(region, regionSize))
        return MakeResult(VTablePatchError::UnsupportedProtectionLayout);

    for (std::size_t i = 0; i < count; ++i) {
        void *original = vtable[requests[i].Slot];
        if (!original)
            return MakeResult(VTablePatchError::InvalidOriginal, i);
        if (!IsExecutableAddress(original))
            return MakeResult(VTablePatchError::NonExecutableOriginal, i);
        entries.push_back({requests[i].Slot, original, requests[i].Replacement});
    }

    DWORD originalProtection = 0;
    if (!::VirtualProtect(region, regionSize, PAGE_EXECUTE_READWRITE, &originalProtection))
        return MakeResult(VTablePatchError::ProtectionChangeFailed);

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (vtable[entries[i].Slot] != entries[i].Original) {
            const bool restored = ::VirtualProtect(region, regionSize, originalProtection, &originalProtection) != FALSE;
            return MakeResult(restored ? VTablePatchError::SlotChanged :
                                         VTablePatchError::ProtectionRestoreFailed,
                              i);
        }
    }

    for (const Entry &entry : entries)
        vtable[entry.Slot] = entry.Replacement;

    DWORD writableProtection = 0;
    if (!::VirtualProtect(region, regionSize, originalProtection, &writableProtection)) {
        for (const Entry &entry : entries)
            vtable[entry.Slot] = entry.Original;
        ::VirtualProtect(region, regionSize, originalProtection, &writableProtection);
        return MakeResult(VTablePatchError::ProtectionRestoreFailed);
    }

    m_VTable = vtable;
    m_Region = region;
    m_RegionSize = regionSize;
    m_Entries = std::move(entries);
    return {};
}

VTablePatchResult VTablePatch::Remove() {
    if (!IsInstalled())
        return {};
    if (!IsReadableRange(m_Region, m_RegionSize))
        return MakeResult(VTablePatchError::InvalidVTable);
    if (!HasUniformProtection(m_Region, m_RegionSize))
        return MakeResult(VTablePatchError::UnsupportedProtectionLayout);

    std::size_t firstConflict = static_cast<std::size_t>(-1);
    bool hasOwnedSlot = false;
    for (std::size_t i = 0; i < m_Entries.size(); ++i) {
        if (m_VTable[m_Entries[i].Slot] == m_Entries[i].Replacement) {
            hasOwnedSlot = true;
        } else if (firstConflict == static_cast<std::size_t>(-1)) {
            firstConflict = i;
        }
    }

    if (!hasOwnedSlot) {
        Clear();
        return MakeResult(VTablePatchError::OwnershipLost, firstConflict);
    }

    DWORD originalProtection = 0;
    if (!::VirtualProtect(m_Region, m_RegionSize, PAGE_EXECUTE_READWRITE, &originalProtection))
        return MakeResult(VTablePatchError::ProtectionChangeFailed);

    for (std::size_t i = 0; i < m_Entries.size(); ++i) {
        const Entry &entry = m_Entries[i];
        if (m_VTable[entry.Slot] == entry.Replacement) {
            m_VTable[entry.Slot] = entry.Original;
        } else if (firstConflict == static_cast<std::size_t>(-1)) {
            firstConflict = i;
        }
    }

    DWORD writableProtection = 0;
    const bool restored = ::VirtualProtect(m_Region, m_RegionSize, originalProtection, &writableProtection) != FALSE;
    Clear();
    if (!restored)
        return MakeResult(VTablePatchError::ProtectionRestoreFailed);
    if (firstConflict != static_cast<std::size_t>(-1))
        return MakeResult(VTablePatchError::OwnershipLost, firstConflict);
    return {};
}

void *VTablePatch::GetOriginal(std::size_t slot) const {
    for (const Entry &entry : m_Entries) {
        if (entry.Slot == slot)
            return entry.Original;
    }
    return nullptr;
}

const char *VTablePatch::GetErrorName(VTablePatchError error) {
    switch (error) {
    case VTablePatchError::None: return "none";
    case VTablePatchError::AlreadyInstalled: return "already installed";
    case VTablePatchError::InvalidRequest: return "invalid request";
    case VTablePatchError::InvalidInstance: return "invalid instance";
    case VTablePatchError::InvalidVTable: return "invalid vtable";
    case VTablePatchError::InvalidSlot: return "invalid slot";
    case VTablePatchError::DuplicateSlot: return "duplicate slot";
    case VTablePatchError::InvalidOriginal: return "invalid original function";
    case VTablePatchError::InvalidReplacement: return "invalid replacement function";
    case VTablePatchError::NonExecutableOriginal: return "original function is not executable";
    case VTablePatchError::NonExecutableReplacement: return "replacement function is not executable";
    case VTablePatchError::UnsupportedProtectionLayout: return "unsupported protection layout";
    case VTablePatchError::ProtectionChangeFailed: return "failed to change memory protection";
    case VTablePatchError::SlotChanged: return "vtable slot changed during installation";
    case VTablePatchError::ProtectionRestoreFailed: return "failed to restore memory protection";
    case VTablePatchError::OwnershipLost: return "vtable slot ownership changed";
    }
    return "unknown";
}

void VTablePatch::Clear() {
    m_VTable = nullptr;
    m_Region = nullptr;
    m_RegionSize = 0;
    m_Entries.clear();
}
