#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include "Hooks/VTablePatch.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
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
int Original() { return 7; }
int Replacement() { return 11; }
int LaterReplacement() { return 19; }
void *Address(int (*function)()) {
    void *result = nullptr;
    static_assert(sizeof(result) == sizeof(function));
    std::memcpy(&result, &function, sizeof(result));
    return result;
}

class Pages {
public:
    explicit Pages(std::size_t count = 1) {
#if defined(_WIN32)
        SYSTEM_INFO info{}; GetSystemInfo(&info);
        PageSize = info.dwPageSize;
        Size = PageSize * count;
        Data = VirtualAlloc(nullptr, Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        PageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        Size = PageSize * count;
        Data = mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (Data == MAP_FAILED) Data = nullptr;
#endif
    }
    ~Pages() {
        if (!Data) return;
#if defined(_WIN32)
        VirtualFree(Data, 0, MEM_RELEASE);
#else
        munmap(Data, Size);
#endif
    }
    bool Protect(bool write, std::size_t offset = 0, std::size_t size = 0) {
        void *address = static_cast<char *>(Data) + offset;
        if (!size) size = Size - offset;
#if defined(_WIN32)
        DWORD old = 0;
        return VirtualProtect(address, size, write ? PAGE_READWRITE : PAGE_READONLY, &old) != FALSE;
#else
        return mprotect(address, size, PROT_READ | (write ? PROT_WRITE : 0)) == 0;
#endif
    }
    int Protection() const {
#if defined(_WIN32)
        MEMORY_BASIC_INFORMATION info{};
        return VirtualQuery(Data, &info, sizeof(info)) ? static_cast<int>(info.Protect) : -1;
#elif defined(__APPLE__)
        mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(Data);
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        const auto result = mach_vm_region(mach_task_self(), &address, &size,
            VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object);
        if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
        return result == KERN_SUCCESS ? info.protection : -1;
#else
        std::ifstream maps("/proc/self/maps");
        const auto address = reinterpret_cast<std::uintptr_t>(Data);
        for (std::string line; std::getline(maps, line);) {
            unsigned long long begin = 0, end = 0;
            char flags[5]{};
            if (std::sscanf(line.c_str(), "%llx-%llx %4s", &begin, &end, flags) == 3 &&
                address >= begin && address < end)
                return (flags[0] == 'r' ? PROT_READ : 0) | (flags[1] == 'w' ? PROT_WRITE : 0) |
                       (flags[2] == 'x' ? PROT_EXEC : 0);
        }
        return -1;
#endif
    }
    void *Data = nullptr;
    std::size_t PageSize = 0;
    std::size_t Size = 0;
};

struct Instance { void **Table; };

TEST(SdlVTablePatchTest, ReadOnlySlotsAndTheirOriginalPageProtectionAreRestored) {
    Pages pages; ASSERT_NE(nullptr, pages.Data);
    auto **table = static_cast<void **>(pages.Data);
    table[0] = Address(Original); table[1] = Address(LaterReplacement);
    ASSERT_TRUE(pages.Protect(false));
    const int protection = pages.Protection(); ASSERT_NE(-1, protection);
    Instance object{table};
    VTablePatch patch;
    const VTablePatch::Request request{0, Address(Replacement)};
    ASSERT_TRUE(patch.Install(&object, &request, 1));
    EXPECT_EQ(Address(Original), patch.GetOriginal(0));
    EXPECT_EQ(Address(Replacement), table[0]);
    EXPECT_EQ(Address(LaterReplacement), table[1]);
    EXPECT_EQ(protection, pages.Protection());
    EXPECT_EQ(VTablePatchError::AlreadyInstalled, patch.Install(&object, &request, 1).Code);
    EXPECT_TRUE(patch.Remove()); EXPECT_TRUE(patch.Remove());
    EXPECT_EQ(Address(Original), table[0]);
    EXPECT_EQ(protection, pages.Protection());
}

TEST(SdlVTablePatchTest, AnInvalidRequestNeverWritesAnEarlierValidSlot) {
    Pages pages; ASSERT_NE(nullptr, pages.Data);
    auto **table = static_cast<void **>(pages.Data);
    table[0] = Address(Original); table[1] = Address(LaterReplacement);
    ASSERT_TRUE(pages.Protect(false));
    Instance object{table}; VTablePatch patch;
    int data = 0;
    const VTablePatch::Request requests[]{{0, Address(Replacement)}, {1, &data}};
    EXPECT_EQ(VTablePatchError::NonExecutableReplacement, patch.Install(&object, requests, 2).Code);
    EXPECT_FALSE(patch.IsInstalled());
    EXPECT_EQ(Address(Original), table[0]); EXPECT_EQ(Address(LaterReplacement), table[1]);
    const VTablePatch::Request duplicate[]{{0, Address(Replacement)}, {0, Address(LaterReplacement)}};
    EXPECT_EQ(VTablePatchError::DuplicateSlot, patch.Install(&object, duplicate, 2).Code);
    const VTablePatch::Request overflow{(std::numeric_limits<std::size_t>::max)(), Address(Replacement)};
    EXPECT_EQ(VTablePatchError::InvalidSlot, patch.Install(&object, &overflow, 1).Code);
    EXPECT_EQ(Address(Original), table[0]);
}

TEST(SdlVTablePatchTest, AnotherOwnerMustReleaseItsSlotBeforeRemovalCanComplete) {
    Pages pages; ASSERT_NE(nullptr, pages.Data);
    auto **table = static_cast<void **>(pages.Data); table[0] = Address(Original);
    ASSERT_TRUE(pages.Protect(false)); const int protection = pages.Protection();
    Instance object{table}; VTablePatch first, later;
    const VTablePatch::Request request{0, Address(Replacement)};
    const VTablePatch::Request override{0, Address(LaterReplacement)};
    ASSERT_TRUE(first.Install(&object, &request, 1));
    ASSERT_TRUE(later.Install(&object, &override, 1));
    EXPECT_EQ(VTablePatchError::OwnershipLost, first.Remove().Code);
    EXPECT_TRUE(first.IsInstalled()); EXPECT_EQ(Address(LaterReplacement), table[0]);
    EXPECT_EQ(protection, pages.Protection());
    EXPECT_TRUE(later.Remove()); EXPECT_EQ(Address(Replacement), table[0]);
    EXPECT_TRUE(first.Remove()); EXPECT_EQ(Address(Original), table[0]);
    EXPECT_EQ(protection, pages.Protection());
}

TEST(SdlVTablePatchTest, MixedPageProtectionsAreRejectedWithoutMutation) {
    Pages pages(2); ASSERT_NE(nullptr, pages.Data);
    auto **table = reinterpret_cast<void **>(static_cast<char *>(pages.Data) + pages.PageSize - sizeof(void *));
    table[0] = Address(Original); table[1] = Address(Original);
    ASSERT_TRUE(pages.Protect(false, 0, pages.PageSize));
    Instance object{table}; VTablePatch patch;
    const VTablePatch::Request requests[]{{0, Address(Replacement)}, {1, Address(LaterReplacement)}};
    EXPECT_EQ(VTablePatchError::UnsupportedProtectionLayout, patch.Install(&object, requests, 2).Code);
    EXPECT_FALSE(patch.IsInstalled());
    EXPECT_EQ(Address(Original), table[0]); EXPECT_EQ(Address(Original), table[1]);
}

TEST(SdlVTablePatchTest, RemovalRestoresTheInstallTimeProtection) {
    Pages pages; ASSERT_NE(nullptr, pages.Data);
    auto **table = static_cast<void **>(pages.Data); table[0] = Address(Original);
    ASSERT_TRUE(pages.Protect(false)); const int protection = pages.Protection();
    Instance object{table}; VTablePatch patch;
    const VTablePatch::Request request{0, Address(Replacement)};
    ASSERT_TRUE(patch.Install(&object, &request, 1));
    ASSERT_TRUE(pages.Protect(true));
    EXPECT_NE(protection, pages.Protection());
    EXPECT_TRUE(patch.Remove()); EXPECT_EQ(Address(Original), table[0]);
    EXPECT_EQ(protection, pages.Protection());
}

TEST(SdlVTablePatchTest, UnmappedInstancesAndDataSlotsAreRejected) {
    VTablePatch patch;
    const VTablePatch::Request request{0, Address(Replacement)};
    EXPECT_EQ(VTablePatchError::InvalidInstance, patch.Install(nullptr, &request, 1).Code);
    EXPECT_EQ(VTablePatchError::InvalidInstance,
              patch.Install(reinterpret_cast<void *>(0x1000), &request, 1).Code);
    int data = 0; void *table[]{&data}; Instance object{table};
    EXPECT_EQ(VTablePatchError::NonExecutableOriginal, patch.Install(&object, &request, 1).Code);
    EXPECT_EQ(&data, table[0]);
}
} // namespace
