#include "BML/IVP.h"

#include <cstddef>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

#define EXPECT_GOLDEN_OFFSET(Interface, member, golden)                       \
    EXPECT_EQ(offsetof(Interface, member), static_cast<std::size_t>(golden))  \
        << #Interface "::" #member " moved"

TEST(IvpInterfaceStructOffsets, StableX86Layout) {
    static_assert(sizeof(void *) == 4,
                  "IVP tests require the retail x86 ABI");
    EXPECT_TRUE(std::is_standard_layout_v<BML_IvpInterface>);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, ReadApiInfo, 12);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetManager, 16);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetEnvironment, 20);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetPhysicsObject, 24);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetRealObject, 28);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetCore, 32);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetMaterial, 36);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, ResolveSymbol, 40);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, ResolveRva, 44);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetSymbolCount, 48);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetSymbol, 52);
    EXPECT_EQ(sizeof(BML_IvpInterface), static_cast<std::size_t>(56));
    EXPECT_EQ(sizeof(BML_IvpApiInfo), static_cast<std::size_t>(92));
    EXPECT_EQ(sizeof(BML_IvpSymbol), static_cast<std::size_t>(12));
}

} // namespace
