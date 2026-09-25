#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "Hooks/VTablePatch.h"

#include <gtest/gtest.h>

namespace {
    void OriginalFirst() {}
    void OriginalSecond() {}
    void ReplacementFirst() {}
    void ReplacementSecond() {}
    void ThirdPartyReplacement() {}

    void *FunctionAddress(void (*function)()) {
        return reinterpret_cast<void *>(function);
    }

    class VTablePage {
    public:
        VTablePage() {
            SYSTEM_INFO systemInfo = {};
            ::GetSystemInfo(&systemInfo);
            m_Size = systemInfo.dwPageSize;
            m_Slots = static_cast<void **>(::VirtualAlloc(
                nullptr, m_Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }

        ~VTablePage() {
            if (m_Slots)
                ::VirtualFree(m_Slots, 0, MEM_RELEASE);
        }

        bool Protect(DWORD protection) {
            DWORD oldProtection = 0;
            return m_Slots && ::VirtualProtect(m_Slots, m_Size, protection, &oldProtection) != FALSE;
        }

        void **Slots() const { return m_Slots; }

    private:
        void **m_Slots = nullptr;
        std::size_t m_Size = 0;
    };

    struct FakeInstance {
        void **VTable;
    };
}

TEST(VTablePatchTest, InstallsAndRemovesWholePatch) {
    VTablePage page;
    ASSERT_NE(page.Slots(), nullptr);
    page.Slots()[0] = FunctionAddress(&OriginalFirst);
    page.Slots()[2] = FunctionAddress(&OriginalSecond);
    ASSERT_TRUE(page.Protect(PAGE_READONLY));
    FakeInstance instance = {page.Slots()};

    const VTablePatch::Request requests[] = {
        {0, FunctionAddress(&ReplacementFirst)},
        {2, FunctionAddress(&ReplacementSecond)},
    };
    VTablePatch patch;

    const VTablePatchResult installed = patch.Install(&instance, requests, 2);
    ASSERT_TRUE(installed);
    EXPECT_TRUE(patch.IsInstalled());
    EXPECT_EQ(page.Slots()[0], FunctionAddress(&ReplacementFirst));
    EXPECT_EQ(page.Slots()[2], FunctionAddress(&ReplacementSecond));
    EXPECT_EQ(patch.GetOriginal(0), FunctionAddress(&OriginalFirst));
    EXPECT_EQ(patch.GetOriginal(2), FunctionAddress(&OriginalSecond));

    const VTablePatchResult removed = patch.Remove();
    EXPECT_TRUE(removed);
    EXPECT_FALSE(patch.IsInstalled());
    EXPECT_EQ(page.Slots()[0], FunctionAddress(&OriginalFirst));
    EXPECT_EQ(page.Slots()[2], FunctionAddress(&OriginalSecond));
}

TEST(VTablePatchTest, RejectsWholeRequestBeforeChangingAnySlot) {
    VTablePage page;
    ASSERT_NE(page.Slots(), nullptr);
    page.Slots()[0] = FunctionAddress(&OriginalFirst);
    page.Slots()[1] = FunctionAddress(&OriginalSecond);
    ASSERT_TRUE(page.Protect(PAGE_READONLY));
    FakeInstance instance = {page.Slots()};

    const VTablePatch::Request requests[] = {
        {0, FunctionAddress(&ReplacementFirst)},
        {1, page.Slots()},
    };
    VTablePatch patch;

    const VTablePatchResult result = patch.Install(&instance, requests, 2);
    EXPECT_EQ(result.Code, VTablePatchError::NonExecutableReplacement);
    EXPECT_EQ(result.EntryIndex, 1u);
    EXPECT_FALSE(patch.IsInstalled());
    EXPECT_EQ(page.Slots()[0], FunctionAddress(&OriginalFirst));
    EXPECT_EQ(page.Slots()[1], FunctionAddress(&OriginalSecond));
}

TEST(VTablePatchTest, RejectsDuplicateSlots) {
    VTablePage page;
    ASSERT_NE(page.Slots(), nullptr);
    page.Slots()[0] = FunctionAddress(&OriginalFirst);
    ASSERT_TRUE(page.Protect(PAGE_READONLY));
    FakeInstance instance = {page.Slots()};

    const VTablePatch::Request requests[] = {
        {0, FunctionAddress(&ReplacementFirst)},
        {0, FunctionAddress(&ReplacementSecond)},
    };
    VTablePatch patch;

    const VTablePatchResult result = patch.Install(&instance, requests, 2);
    EXPECT_EQ(result.Code, VTablePatchError::DuplicateSlot);
    EXPECT_EQ(result.EntryIndex, 1u);
    EXPECT_EQ(page.Slots()[0], FunctionAddress(&OriginalFirst));
}

TEST(VTablePatchTest, DoesNotOverwriteANewerPatchDuringRemoval) {
    VTablePage page;
    ASSERT_NE(page.Slots(), nullptr);
    page.Slots()[0] = FunctionAddress(&OriginalFirst);
    page.Slots()[1] = FunctionAddress(&OriginalSecond);
    ASSERT_TRUE(page.Protect(PAGE_READONLY));
    FakeInstance instance = {page.Slots()};

    const VTablePatch::Request requests[] = {
        {0, FunctionAddress(&ReplacementFirst)},
        {1, FunctionAddress(&ReplacementSecond)},
    };
    VTablePatch patch;
    ASSERT_TRUE(patch.Install(&instance, requests, 2));

    ASSERT_TRUE(page.Protect(PAGE_READWRITE));
    page.Slots()[0] = FunctionAddress(&ThirdPartyReplacement);
    ASSERT_TRUE(page.Protect(PAGE_READONLY));

    const VTablePatchResult removed = patch.Remove();
    EXPECT_EQ(removed.Code, VTablePatchError::OwnershipLost);
    EXPECT_EQ(removed.EntryIndex, 0u);
    EXPECT_FALSE(patch.IsInstalled());
    EXPECT_EQ(page.Slots()[0], FunctionAddress(&ThirdPartyReplacement));
    EXPECT_EQ(page.Slots()[1], FunctionAddress(&OriginalSecond));
}

TEST(VTablePatchTest, RejectsAnInaccessibleVTable) {
    VTablePage page;
    ASSERT_NE(page.Slots(), nullptr);
    page.Slots()[0] = FunctionAddress(&OriginalFirst);
    FakeInstance instance = {page.Slots()};
    ASSERT_TRUE(page.Protect(PAGE_NOACCESS));

    const VTablePatch::Request request = {0, FunctionAddress(&ReplacementFirst)};
    VTablePatch patch;
    const VTablePatchResult result = patch.Install(&instance, &request, 1);

    EXPECT_EQ(result.Code, VTablePatchError::InvalidVTable);
    EXPECT_FALSE(patch.IsInstalled());
}

TEST(VTablePatchTest, DefersRemovalWhileTheVTableIsInaccessible) {
    VTablePage page;
    ASSERT_NE(page.Slots(), nullptr);
    page.Slots()[0] = FunctionAddress(&OriginalFirst);
    ASSERT_TRUE(page.Protect(PAGE_READONLY));
    FakeInstance instance = {page.Slots()};

    const VTablePatch::Request request = {0, FunctionAddress(&ReplacementFirst)};
    VTablePatch patch;
    ASSERT_TRUE(patch.Install(&instance, &request, 1));
    ASSERT_TRUE(page.Protect(PAGE_NOACCESS));

    const VTablePatchResult deferred = patch.Remove();
    EXPECT_EQ(deferred.Code, VTablePatchError::InvalidVTable);
    EXPECT_TRUE(patch.IsInstalled());

    ASSERT_TRUE(page.Protect(PAGE_READONLY));
    EXPECT_TRUE(patch.Remove());
    EXPECT_FALSE(patch.IsInstalled());
    EXPECT_EQ(page.Slots()[0], FunctionAddress(&OriginalFirst));
}
