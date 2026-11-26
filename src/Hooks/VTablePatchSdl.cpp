#include "Hooks/VTablePatch.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

namespace {
enum MemoryAccess { Read = 1, Write = 2, Execute = 4 };

int Access(int protection) {
#if defined(_WIN32)
    if (protection & (PAGE_GUARD | PAGE_NOACCESS)) return 0;
    switch (protection & 0xff) {
    case PAGE_READONLY: return Read;
    case PAGE_READWRITE: case PAGE_WRITECOPY: return Read | Write;
    case PAGE_EXECUTE: return Execute;
    case PAGE_EXECUTE_READ: return Read | Execute;
    case PAGE_EXECUTE_READWRITE: case PAGE_EXECUTE_WRITECOPY: return Read | Write | Execute;
    default: return 0;
    }
#else
    return protection;
#endif
}

int Writable(int protection) {
#if defined(_WIN32)
    return (Access(protection) & Execute) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
#else
    return protection | PROT_WRITE;
#endif
}

struct MemoryRegion {
    std::uintptr_t Begin = 0;
    std::uintptr_t End = 0;
    int Protection = 0;
};

bool Query(const void *address, MemoryRegion &region) {
    const auto target = reinterpret_cast<std::uintptr_t>(address);
#if defined(_WIN32)
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT)
        return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin)
        return false;
    region = {begin, begin + info.RegionSize, static_cast<int>(info.Protect)};
    return true;
#elif defined(__APPLE__)
    mach_vm_address_t begin = target;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    const auto result = mach_vm_region(mach_task_self(), &begin, &size,
        VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object);
    if (object != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), object);
    if (result != KERN_SUCCESS || target < begin || target - begin >= size)
        return false;
    region = {static_cast<std::uintptr_t>(begin), static_cast<std::uintptr_t>(begin + size),
              static_cast<int>(info.protection)};
    return true;
#elif defined(__linux__)
    // PT_LOAD flags do not describe the final RELRO protection. Stream the
    // actual mappings because procfs reports a file size of zero.
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(
        SDL_IOFromFile("/proc/self/maps", "rb"), SDL_CloseIO);
    if (!stream)
        return false;
    std::string mappings;
    char buffer[4096];
    while (const auto size = SDL_ReadIO(stream.get(), buffer, sizeof(buffer)))
        mappings.append(buffer, size);
    if (SDL_GetIOStatus(stream.get()) != SDL_IO_STATUS_EOF)
        return false;
    const char *line = mappings.c_str();
    while (*line) {
        unsigned long long begin = 0, end = 0;
        char flags[5]{};
        if (std::sscanf(line, "%llx-%llx %4s", &begin, &end, flags) == 3 &&
            target >= begin && target < end) {
            region = {static_cast<std::uintptr_t>(begin), static_cast<std::uintptr_t>(end),
                (flags[0] == 'r' ? PROT_READ : 0) | (flags[1] == 'w' ? PROT_WRITE : 0) |
                (flags[2] == 'x' ? PROT_EXEC : 0)};
            return true;
        }
        const char *next = SDL_strchr(line, '\n');
        if (!next)
            break;
        line = next + 1;
    }
    return false;
#else
#error Unsupported SDL vtable protection backend
#endif
}

bool Range(const void *address, std::size_t size, int required, int *uniform = nullptr) {
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    if (!address || size == 0 || size > (std::numeric_limits<std::uintptr_t>::max)() - begin)
        return false;
    const auto end = begin + size;
    bool first = true;
    for (auto cursor = begin; cursor < end;) {
        MemoryRegion region;
        if (!Query(reinterpret_cast<const void *>(cursor), region) || region.End <= cursor ||
            (Access(region.Protection) & required) != required)
            return false;
        if (uniform) {
            if (!first && *uniform != region.Protection)
                return false;
            *uniform = region.Protection;
        }
        first = false;
        cursor = (std::min)(region.End, end);
    }
    return true;
}

bool Protect(void *address, std::size_t size, int protection) {
#if defined(_WIN32)
    DWORD oldProtection = 0;
    return VirtualProtect(address, size, static_cast<DWORD>(protection), &oldProtection) != FALSE;
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0)
        return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto page = begin - begin % static_cast<std::uintptr_t>(pageSize);
    if (size > (std::numeric_limits<std::size_t>::max)() - (begin - page))
        return false;
#if defined(__APPLE__)
    // Dyld may map __DATA_CONST read-only with copy-on-write maximum access.
    return mach_vm_protect(mach_task_self(), page, size + begin - page, FALSE,
        protection | ((protection & VM_PROT_WRITE) ? VM_PROT_COPY : 0)) == KERN_SUCCESS;
#else
    return mprotect(reinterpret_cast<void *>(page), size + begin - page, protection) == 0;
#endif
#endif
}

VTablePatchResult Failure(VTablePatchError code, std::size_t index = static_cast<std::size_t>(-1)) {
    return {code, index};
}
} // namespace

VTablePatch::~VTablePatch() { assert(!IsInstalled()); }

VTablePatchResult VTablePatch::Install(void *instance, const Request *requests, std::size_t count) {
    if (IsInstalled()) return Failure(VTablePatchError::AlreadyInstalled);
    if (!requests || count == 0) return Failure(VTablePatchError::InvalidRequest);
    if (reinterpret_cast<std::uintptr_t>(instance) % alignof(void *) != 0 ||
        !Range(instance, sizeof(void *), Read)) return Failure(VTablePatchError::InvalidInstance);
    void **vtable = *static_cast<void ***>(instance);
    if (!vtable || reinterpret_cast<std::uintptr_t>(vtable) % alignof(void *) != 0)
        return Failure(VTablePatchError::InvalidVTable);
    std::size_t first = (std::numeric_limits<std::size_t>::max)(), last = 0;
    std::vector<Entry> entries;
    entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto &request = requests[i];
        if (request.Slot >= (std::numeric_limits<std::size_t>::max)() / sizeof(void *))
            return Failure(VTablePatchError::InvalidSlot, i);
        if (!request.Replacement) return Failure(VTablePatchError::InvalidReplacement, i);
        if (!Range(request.Replacement, 1, Execute))
            return Failure(VTablePatchError::NonExecutableReplacement, i);
        for (std::size_t j = 0; j < i; ++j)
            if (requests[j].Slot == request.Slot) return Failure(VTablePatchError::DuplicateSlot, i);
        first = (std::min)(first, request.Slot);
        last = (std::max)(last, request.Slot);
    }
    const auto base = reinterpret_cast<std::uintptr_t>(vtable);
    const auto offset = first * sizeof(void *);
    if (offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
        return Failure(VTablePatchError::InvalidSlot);
    void *region = reinterpret_cast<void *>(base + offset);
    const std::size_t size = (last - first + 1) * sizeof(void *);
    int protection = 0;
    if (!Range(region, size, Read, &protection))
        return Failure(VTablePatchError::UnsupportedProtectionLayout);
    for (std::size_t i = 0; i < count; ++i) {
        void *original = vtable[requests[i].Slot];
        if (!original) return Failure(VTablePatchError::InvalidOriginal, i);
        if (!Range(original, 1, Execute)) return Failure(VTablePatchError::NonExecutableOriginal, i);
        entries.push_back({requests[i].Slot, original, requests[i].Replacement});
    }
    if (!Protect(region, size, Writable(protection)))
        return Failure(VTablePatchError::ProtectionChangeFailed);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (vtable[entries[i].Slot] != entries[i].Original) {
            const bool restored = Protect(region, size, protection);
            return Failure(restored ? VTablePatchError::SlotChanged :
                                      VTablePatchError::ProtectionRestoreFailed, i);
        }
    }
    for (const auto &entry : entries) vtable[entry.Slot] = entry.Replacement;
    if (!Protect(region, size, protection)) {
        for (const auto &entry : entries) vtable[entry.Slot] = entry.Original;
        Protect(region, size, protection);
        return Failure(VTablePatchError::ProtectionRestoreFailed);
    }
    m_VTable = vtable;
    m_Region = region;
    m_RegionSize = size;
    m_OriginalProtection = protection;
    m_Entries = std::move(entries);
    return {};
}

VTablePatchResult VTablePatch::Remove() {
    if (!IsInstalled()) return {};
    if (!Range(m_Region, m_RegionSize, Read)) return Failure(VTablePatchError::InvalidVTable);
    // Do not tear down a partial chain or overwrite a later owner's slot. The
    // loader retains its hook state/module until the owner restores our entry.
    for (std::size_t i = 0; i < m_Entries.size(); ++i) {
        const auto &entry = m_Entries[i];
        if (m_VTable[entry.Slot] != entry.Replacement && m_VTable[entry.Slot] != entry.Original)
            return Failure(VTablePatchError::OwnershipLost, i);
    }
    if (!Protect(m_Region, m_RegionSize, Writable(m_OriginalProtection)))
        return Failure(VTablePatchError::ProtectionChangeFailed);
    for (const auto &entry : m_Entries)
        if (m_VTable[entry.Slot] == entry.Replacement) m_VTable[entry.Slot] = entry.Original;
    if (!Protect(m_Region, m_RegionSize, m_OriginalProtection))
        return Failure(VTablePatchError::ProtectionRestoreFailed);
    Clear();
    return {};
}

void *VTablePatch::GetOriginal(std::size_t slot) const {
    for (const auto &entry : m_Entries) if (entry.Slot == slot) return entry.Original;
    return nullptr;
}

void VTablePatch::Clear() {
    m_VTable = nullptr;
    m_Region = nullptr;
    m_RegionSize = 0;
    m_OriginalProtection = 0;
    m_Entries.clear();
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
